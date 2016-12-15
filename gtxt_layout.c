#include "gtxt_layout.h"
#include "gtxt_glyph.h"
#include "gtxt_label.h"
#include "gtxt_richtext.h"

#include <ds_array.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define INIT_ROW_CAP 4
#define INIT_GLYPH_CAP 16

#define OMIT_UNICODE 46
#define OMIT_COUNT 3

#define CONNECT_UNICODE 45

#define MAX_ROW_CONDENSE 0.10f

struct glyph {
	int unicode;

	float x, y;
	float w, h;

	float out_width;

	struct glyph* next;
};

struct row {
	float width;
	float height;
	float ymax, ymin;

	struct glyph *head, *tail;
	int glyph_count;

	float offset;

	struct row* next;
};

enum CONNECTED_GLYPH_TYPE
{
	CGT_NULL = 0,
	CGT_NUMBER,
	CGT_LETTER,
};

struct layout {
	const struct gtxt_label_style* style;

	struct row* head;
	int row_count;

	struct glyph* glyph_freelist;
	size_t glyph_cap;

	struct row* row_freelist;
	size_t row_cap;

	float prev_tot_h;

	struct glyph* prev_single_glyph;
	enum CONNECTED_GLYPH_TYPE connected_glyph_type;

	struct glyph *prev_glyph, *prev_prev_glyph;

	struct row* curr_row;
};

static struct layout L;

void 
gtxt_layout_release() {
	free(L.glyph_freelist);
	free(L.row_freelist);
	memset(&L, 0, sizeof(L));
}

static inline void
_prepare_glyph_freelist(int cap) {
	if (cap <= L.glyph_cap || cap <= 0) {
		return;
	}

	int new_num = cap - L.glyph_cap;
	int sz = sizeof(struct glyph) * new_num;
	struct glyph* new_list = (struct glyph*)malloc(sz);
	if (!new_list) {
		printf("malloc fail, gtxt_layout _prepare_glyph_freelist \n");
		exit(1);
	}
	memset(new_list, 0, sz);
	for (int i = 0; i < new_num - 1; ++i) {
		new_list[i].next = &new_list[i+1];
	}
	new_list[new_num - 1].next = NULL;

	if (L.glyph_freelist) {
		struct glyph* end = L.glyph_freelist;
		while (end->next) { 
			end = end->next;
		}
		end->next = new_list;
	} else {
		L.glyph_freelist = new_list;				
	}

	L.glyph_cap = cap;
}

static inline void
_prepare_row_freelist(int cap) {
	if (cap <= L.row_cap || cap <= 0) {
		return;
	}

	int new_num = cap - L.row_cap;
	int sz = sizeof(struct row) * new_num;
	struct row* new_list = (struct row*)malloc(sz);
	if (!new_list) {
		printf("malloc fail, gtxt_layout _prepare_row_freelist \n");
		exit(1);
	}
	memset(new_list, 0, sz);
	for (int i = 0; i < new_num - 1; ++i) {
		new_list[i].next = &new_list[i+1];
	}
	new_list[new_num - 1].next = NULL;

	if (L.row_freelist) {
		struct row* end = L.row_freelist;
		while (end->next) { 
			end = end->next;
		}
		end->next = new_list;
	} else {
		L.row_freelist = new_list;
	}

	L.row_cap = cap;
}

// #define PREPARE_FREELIST(type, cap) do { \
// 	if ((cap) <= L.##type##_cap || (cap) <= 0) { \
// 		break; \
// 	} \
// 	\
// 	int new_num = (cap) - L.##type##_cap; \
// 	int sz = sizeof(struct type) * new_num; \
// 	struct type * new_list = (struct type *)malloc(sz); \
// 	if (!new_list) { \
// 	printf("malloc fail, gtxt_layout _prepare_##type##_freelist \n"); \
// 		exit(1); \
// 	} \
// 	memset(new_list, 0, sz); \
// 	for (int i = 0; i < new_num - 1; ++i) { \
// 		new_list[i].next = &new_list[i+1]; \
// 	} \
// 	new_list[new_num - 1].next = NULL; \
// 	\
// 	if (L.##type##_freelist) { \
// 		struct type * end = L.##type##_freelist; \
// 		while (end->next) { \
// 			end = end->next; \
// 		} \
// 		end->next = new_list; \
// 	} else { \
// 		L.##type##_freelist = new_list; \
// 	} \
// 	\
// 	L.##type##_cap = (cap); \
// } while (0)

static inline void 
_prepare_freelist(int row_cap, int glyph_cap) {
	_prepare_row_freelist(row_cap);
	_prepare_glyph_freelist(glyph_cap);
}

static inline struct row*
_new_row() {
	if (!L.row_freelist) {
		assert(L.row_cap > 0);
		_prepare_row_freelist(L.row_cap * 2);
		return _new_row();
	}

	struct row* r = L.row_freelist;
	assert(r);
	L.row_freelist = r->next;
	r->next = NULL;
	return r;
}

void 
gtxt_layout_begin(const struct gtxt_label_style* style) {
	_prepare_freelist(INIT_ROW_CAP, INIT_GLYPH_CAP);

	L.style = style;

	L.prev_tot_h = 0;

	L.prev_single_glyph = NULL;
	L.connected_glyph_type = CGT_NULL;

	L.prev_glyph = NULL;
	L.prev_prev_glyph = NULL;

	L.curr_row = _new_row();
	L.head = L.curr_row;
	L.row_count = 1;
}

void 
gtxt_layout_end() {
	struct row* last_row = L.head;
	while (last_row->next) {
		last_row = last_row->next;
	}
	last_row->next = L.row_freelist;
	L.row_freelist = L.head;

	struct glyph* freelist = L.glyph_freelist;
	L.glyph_freelist = L.head->head;

	struct glyph* last_tail = NULL;
	struct row* r = L.head;
	while (r) {
		if (last_tail && r->head) {
			last_tail->next = r->head;
		}
		if (r->tail) {
			last_tail = r->tail;
		}

		r->width = r->height = 0;
		r->ymax = r->ymin = 0;
		r->head = NULL;
		r->tail = NULL;
		r->glyph_count = 0;
		r->offset = 0;

		r = r->next;
	}
	if (last_tail) {
		last_tail->next = freelist;
	}
	if (!L.glyph_freelist) {
		L.glyph_freelist = freelist;
	}
}

static inline struct glyph*
_new_glyph() {
	if (!L.glyph_freelist) {
		assert(L.glyph_cap > 0);
		_prepare_glyph_freelist(L.glyph_cap * 2);
		return _new_glyph();
	}

	struct glyph* g = L.glyph_freelist;
	assert(g);
	L.glyph_freelist = g->next;
	g->next = NULL;
	return g;
}

static inline bool
_new_line() {
	L.prev_single_glyph = NULL;
	L.connected_glyph_type = CGT_NULL;
	L.prev_glyph = NULL;
	L.prev_prev_glyph = NULL;

	float h = L.curr_row->height * L.style->space_v;
	L.prev_tot_h += h;
	// over label height
	if (!L.style->overflow) {
		float tot_h = L.prev_tot_h + L.curr_row->height;
		if (tot_h > L.style->height) {
			L.prev_tot_h -= h;
			return false;
		}
	}

	struct row* prev = L.curr_row;
	L.curr_row = _new_row();
	// no free row
	if (!L.curr_row) {
		return false;
	}
	++L.row_count;
	prev->next = L.curr_row;
	L.curr_row->next = NULL;
	return true;
}

static inline void
_add_glyph(struct glyph* g) {
	if (!L.curr_row->head) {
		assert(!L.curr_row->tail);
		L.curr_row->head = L.curr_row->tail = g;
	} else {
		L.curr_row->tail->next = g;
		L.curr_row->tail = g;
	}
	++L.curr_row->glyph_count;
}

static inline bool
_is_letter(int unicode) {
	return 
		(unicode >= 97 && unicode <= 122)	||		// a-z
		(unicode >= 65 && unicode <= 90);			// A-Z
}

static inline bool
_is_number(int unicode) {
	return 
		(unicode >= 48 && unicode <= 57) ||		// 0-9
		unicode == 46		||		// .
		unicode == 37		||		// %
		unicode == 45		||		// -
		unicode == 43;				// +
}

static inline enum CONNECTED_GLYPH_TYPE
_get_connected_glyph_type(int unicode) {
	if (_is_number(unicode)) {
		return CGT_NUMBER;
	} else if (_is_letter(unicode)) {
		return CGT_LETTER;
	} else {
		return CGT_NULL;
	}
}

static inline bool
_is_punctuation(int unicode) {
	return 
		unicode == 65292	||		// £¬
		unicode == 12290	||		// ¡£
		unicode == 65311	||		// £¿
		unicode == 8220		||		// ¡°
		unicode == 8221		||		// ¡±
		unicode == 65281	||		// £¡
		unicode == 65307	||		// ;
		unicode == 65306	||		// :
		unicode == 8216		||		//¡®
		unicode == 8217		||		// ¡¯
		unicode == 12289	||		// ¡¢
		unicode == 65289	||		// £©
		unicode == 12305	||		// ¡¿
		unicode == 12303	||		// ¡»
		unicode == 12299	||		// ¡·

		unicode == 44		||		// ,
		unicode == 46		||		// .
		unicode == 63		||		// ?
		unicode == 39		||		// '
		unicode == 34		||		// "
		unicode == 33		||		// !
		unicode == 59		||		// ;
		unicode == 58		||		// :
		unicode == 39		||		// '
		unicode == 41		||		// )
		unicode == 93		||		// ]
		unicode == 125		;		// }

}

static int
_remove_glyph_to_end(struct glyph* g) {
	int count = 0;
	struct glyph* curr = g;
	while (curr) {
		++count;
		struct glyph* next = curr->next;
		curr->next = L.glyph_freelist;
		L.glyph_freelist = curr;
		curr = next;
	}
	return count;
}

static enum GLO_STATUS
_handle_new_line_force(struct gtxt_glyph_layout* g_layout) {
	if (L.curr_row->height == 0) {
		L.curr_row->height = g_layout->metrics_height;
	}
	if (!_new_line()) {
		return GLOS_FULL;
	} else {
		return GLOS_NEWLINE;
	}
}

static enum GLO_STATUS
_new_line_for_common(float w, float max_condense) {
	float offset = L.style->width - L.curr_row->width - w;
	float room = L.style->width - L.curr_row->width;
	if (room > w * 0.5f && -offset / L.style->width < max_condense) {
		L.curr_row->offset = offset;
	} else {
		L.curr_row->offset = L.style->width - L.curr_row->width;
		if (!_new_line()) {
			return GLOS_FULL;
		}
	}
	return GLOS_NORMAL;
}

static enum GLO_STATUS
_new_line_directly(float w, float max_condense) {
	if (!_new_line()) {
		return GLOS_FULL;
	}
	return GLOS_NORMAL;
}

static inline struct glyph*
_row_backtracking(struct row* row, float remain) {
	float w = 0;
	struct glyph* curr = row->head;
	struct glyph* prev = NULL;
	while (prev != row->tail) {
		w += curr->out_width;
		if (w > remain) {
			return prev;
		}
		prev = curr;
		curr = curr->next;
	}
	return prev;
}

static enum GLO_STATUS
_add_connected_sym(struct gtxt_richtext_style* style, const struct gtxt_glyph_style* gs) {
	struct gtxt_glyph_layout* layout = gtxt_glyph_get_layout(CONNECT_UNICODE, gs);
	float conn_w = layout->advance * L.style->space_h;
	if (conn_w > L.style->width) {
		return GLOS_FULL;
	}
	struct glyph* prev_glyph = _row_backtracking(L.curr_row, L.style->width - conn_w);
	if (!prev_glyph) {
		return GLOS_FULL;
	}
	struct glyph* next_glyph = prev_glyph->next;
	struct row* prev_row = L.curr_row;

	struct glyph* g = prev_glyph->next;
	while (g) {
		prev_row->width -= g->out_width;
		--prev_row->glyph_count;
		g = g->next;
	}
	prev_row->offset = 0;
	prev_row->tail = prev_glyph;
	prev_glyph->next = NULL;
	gtxt_layout_single(CONNECT_UNICODE, NULL);
	prev_row->offset = L.style->width - prev_row->width;

	if (!_new_line()) {
		_remove_glyph_to_end(next_glyph);
		return GLOS_FULL;
	} else {
		struct glyph* g = next_glyph;
		while (g) {
			gtxt_layout_single(g->unicode, style);
			g = g->next;
		}
		_remove_glyph_to_end(next_glyph);
		return GLOS_CONNECTION;
	}
}

static enum GLO_STATUS
_move_chars2nextline(struct gtxt_richtext_style* style, struct glyph* prev_glyph) {
	struct row* prev_row = L.curr_row;

	struct glyph* g = prev_glyph->next;
	while (g) {
		prev_row->width -= g->out_width;
		--prev_row->glyph_count;
		g = g->next;
	}
	prev_row->offset = L.style->width - prev_row->width;
	prev_row->tail = prev_glyph;

	if (!_new_line()) {
		_remove_glyph_to_end(prev_glyph->next);
		prev_glyph->next = NULL;
		return GLOS_FULL;
	} else {
		struct glyph* g = prev_glyph->next;
		while (g) {
			gtxt_layout_single(g->unicode, style);
			g = g->next;
		}
		_remove_glyph_to_end(prev_glyph->next);
		prev_glyph->next = NULL;
		return GLOS_NORMAL;
	}
}

static enum GLO_STATUS
_new_line_for_connected(struct gtxt_richtext_style* style, const struct gtxt_glyph_style* gs, float w) {
	L.curr_row->offset = L.style->width - L.curr_row->width - w;
	if (-L.curr_row->offset / L.style->width <= MAX_ROW_CONDENSE) {
		return GLOS_NORMAL;	
	}
	if (!L.prev_single_glyph) {
		return _add_connected_sym(style, gs);
	} else {
		return _move_chars2nextline(style, L.prev_single_glyph);
	}
}

static enum GLO_STATUS
_new_line_for_punctuation(struct gtxt_richtext_style* style, const struct gtxt_glyph_style* gs, float w) {
	L.curr_row->offset = L.style->width - L.curr_row->width - w;
	if (-L.curr_row->offset / L.style->width <= MAX_ROW_CONDENSE) {
		return GLOS_NORMAL;	
	}
	if (!L.prev_prev_glyph) {
		L.curr_row->offset = L.style->width - L.curr_row->width;
		if (!_new_line()) {
			return GLOS_FULL;
		} else {
			return GLOS_NORMAL;
		}
	} else {
		return _move_chars2nextline(style, L.prev_prev_glyph);
	}
}

static enum GLO_STATUS
_handle_new_line_too_long(int unicode, struct gtxt_richtext_style* style, const struct gtxt_glyph_style* gs, struct gtxt_glyph_layout* g_layout, float w) {
	if (L.curr_row->height == 0) {
		L.curr_row->height = g_layout->metrics_height;
	}
	if (_is_punctuation(unicode)) {
		return _new_line_for_punctuation(style, gs, w);
	} else if (_get_connected_glyph_type(unicode) == L.connected_glyph_type &&
		       L.connected_glyph_type != CGT_NULL) {
		return _new_line_for_connected(style, gs, w);
	} else {
		return _new_line_for_common(w, MAX_ROW_CONDENSE);
	}
	return GLOS_NORMAL;
}

static enum GLO_STATUS
_handle_new_line(int unicode, struct gtxt_richtext_style* style, const struct gtxt_glyph_style* gs, struct gtxt_glyph_layout* g_layout, float w) {
 	if (unicode == '\n') {
		return _handle_new_line_force(g_layout);
 	} else if (L.curr_row->width + w + L.curr_row->offset > L.style->width) {
		return _handle_new_line_too_long(unicode, style, gs, g_layout, w);
	} else {
		return GLOS_NORMAL;
	}
}

enum GLO_STATUS 
gtxt_layout_single(int unicode, struct gtxt_richtext_style* style) {
	const struct gtxt_glyph_style* gs;
	if (style) {
		gs = &style->gs;
	} else {
		gs = &L.style->gs;
	}
	struct gtxt_glyph_layout* g_layout = gtxt_glyph_get_layout(unicode, gs);
	if (!g_layout) {
		return GLOS_NORMAL;
	}
	float w = g_layout->advance * L.style->space_h;
	enum GLO_STATUS status = _handle_new_line(unicode, style, gs, g_layout, w);
	if (status == GLOS_NEWLINE || status == GLOS_FULL) {
		return status;
	}

	struct glyph* g = _new_glyph();
	assert(g);
	g->unicode = unicode;

	g->x = g_layout->bearing_x;
	g->y = g_layout->bearing_y;
	g->w = g_layout->sizer.width;
	g->h = g_layout->sizer.height;

	g->out_width = w;

	if (g_layout->metrics_height > L.curr_row->height) {
		L.curr_row->height = g_layout->metrics_height;
	}
	if (g_layout->bearing_y > L.curr_row->ymax) {
		L.curr_row->ymax = g_layout->bearing_y;
	}
	if (g_layout->bearing_y - g_layout->sizer.height < L.curr_row->ymin) {
		L.curr_row->ymin = g_layout->bearing_y - g_layout->sizer.height;
	}
	L.curr_row->width += w;

	_add_glyph(g);

	L.connected_glyph_type = _get_connected_glyph_type(unicode);
	if (L.connected_glyph_type == CGT_NULL) {
		L.prev_single_glyph = g;
	}

	L.prev_prev_glyph = L.prev_glyph;
	L.prev_glyph = g;

	return status;
}

void 
gtxt_layout_multi(struct ds_array* unicodes) {
	int glyph_sz = ds_array_size(unicodes);
	_prepare_glyph_freelist(glyph_sz * 2);

	for (int i = 0; i < glyph_sz; ++i) {
		int unicode = *(int*)ds_array_fetch(unicodes, i);
		enum GLO_STATUS status = gtxt_layout_single(unicode, NULL);
		if (status == GLOS_FULL) {
			gtxt_layout_add_omit_sym(&L.style->gs);
			break;
		}
	}
}

static inline int
_row_make_room(struct row* row, float remain) {
	int count = 0;
	float w = 0;
	struct glyph* curr = row->head;
	struct glyph* prev = NULL;
	while (prev != row->tail) {
		w += curr->out_width;
		if (w > remain) {
			row->width = w - curr->out_width;
			if (prev) {
				count = _remove_glyph_to_end(curr);
				prev->next = NULL;
				row->tail = prev;
			}
			break;
		}
		prev = curr;
		curr = curr->next;
	}
	return count;
}

int
gtxt_layout_add_omit_sym(const struct gtxt_glyph_style* gs) {
	struct gtxt_glyph_layout* layout = gtxt_glyph_get_layout(OMIT_UNICODE, gs);
	float omit_w = layout->advance * L.style->space_h * OMIT_COUNT;
	if (omit_w > L.style->width) {
		return 0;
	}

	float max_w = L.style->width - omit_w;
	struct row* row = L.curr_row;
	int rm_count = _row_make_room(row, max_w);

	for (int i = 0; i < OMIT_COUNT; ++i) {
		gtxt_layout_single(OMIT_UNICODE, NULL);
	}
	row->width += omit_w;
	row->glyph_count += OMIT_COUNT - rm_count;

	return OMIT_COUNT - rm_count;
}

enum GLO_STATUS 
gtxt_layout_ext_sym(int width, int height) {
	if (L.curr_row->width + width > L.style->width) {
		if (!_new_line()) {
			return GLOS_FULL;
		}
	}

	if (height > L.curr_row->height) {
		L.curr_row->height = height;
	}
	if (height > L.curr_row->ymax) {
		L.curr_row->ymax = height;
	}
	L.curr_row->width += width;

	struct glyph* g = _new_glyph();
	assert(g);
	g->unicode = -1;
	g->x = 0;
	g->y = height;
	g->w = width;
	g->h = height;
	g->out_width = width;
	_add_glyph(g);

	L.connected_glyph_type = CGT_NULL;
	L.prev_single_glyph = g;
	L.prev_prev_glyph = L.prev_glyph;
	L.prev_glyph = g;

	return GLOS_NORMAL;
}

static float
_get_start_x(float line_width) {
	float x = 0;
	switch (L.style->align_h) {
	case HA_LEFT: case HA_AUTO: case HA_TILE:
		x = -L.style->width * 0.5f;
		break;
	case HA_RIGHT:
		x = L.style->width * 0.5f - line_width;
		break;
	case HA_CENTER:
		x = -line_width * 0.5f;
		break;
	}
	return x;
}

static float
_get_tot_height() {
	float h = 0;
	if (L.head->next) {
		h = L.prev_tot_h + L.curr_row->height;
	} else {
		assert(L.curr_row == L.head);
		h = L.head->ymax - L.head->ymin;
	}
	return h;
}

static float
_get_start_y() {
	float y;
	float tot_h = _get_tot_height();
	if (L.head->next) {
		switch (L.style->align_v) {
		case VA_TOP: case VA_AUTO:
			y = L.style->height * 0.5f - L.head->ymax;
			break;
		case VA_BOTTOM:
			y = -L.style->height * 0.5f + tot_h - L.head->ymax;
			break;
		case VA_CENTER:
			y = tot_h * 0.5f - L.head->ymax;
			break;
		case VA_TILE:
			y = L.style->height * 0.5f;
			break;
		default:
			assert(0);
		}		
	} else {
		struct row* r = L.head;
		switch (L.style->align_v) {
		case VA_TOP: case VA_AUTO: case VA_TILE:
			y = L.style->height * 0.5f - r->ymax;
			break;
		case VA_BOTTOM:
			y = -L.style->height * 0.5f - r->ymin;
			break;
		case VA_CENTER:
			y = -tot_h * 0.5f - r->ymin;
			break;
		default:
			assert(0);
		}
	}
	return y;
}

static void
_layout_traverse_hori(struct row* r, float y, void (*cb)(int unicode, float x, float y, float w, float h, float row_y, void* ud), void* ud) {
	if (r->glyph_count == 0) {
		assert(!r->head && !r->tail);
		return;
	}
	float row_offset = r->offset;
	// only one char
	if (r->head && !r->head->next) {
		row_offset = 0;
	}
	if (L.style->align_h == HA_TILE) {
		float grid = (float)L.style->width / r->glyph_count;
		float x = _get_start_x(r->width + row_offset) + grid * 0.5f;
		float dx = row_offset / r->glyph_count;
		struct glyph* g = r->head;
		while (g) {
			if (L.style->align_v == VA_TILE) {
				cb(g->unicode, x, y, g->w, g->h, y, ud);
			} else {
				cb(g->unicode, x, y + g->y - g->h * 0.5f, g->w, g->h, y, ud);
			}
			x += grid + dx;
			g = g->next;
		}
	} else {
		float x = _get_start_x(r->width + row_offset);
		float dx = row_offset / r->glyph_count;
		struct glyph* g = r->head;
		while (g) {
			if (L.style->align_v == VA_TILE) {
				cb(g->unicode, x + g->x + g->w * 0.5f, y, g->w, g->h, y, ud);
			} else {
				cb(g->unicode, x + g->x + g->w * 0.5f, y + g->y - g->h * 0.5f, g->w, g->h, y, ud);
			}
			x += g->out_width + dx;
			g = g->next;
		}
	}
}

void 
gtxt_layout_traverse(void (*cb)(int unicode, float x, float y, float w, float h, float row_y, void* ud), void* ud) {
	if (L.row_count == 0) {
		assert(!L.head);
		return;
	}
	if (L.style->align_v == VA_TILE) {
		float grid = (float)L.style->height / L.row_count;
		float y = _get_start_y() -  grid * 0.5f;
		struct row* r = L.head;
		while (r) {
			_layout_traverse_hori(r, y, cb, ud);
			y -= grid;
			r = r->next;
		}
	} else {
		float y = _get_start_y();
		struct row* r = L.head;
		while (r) {
			_layout_traverse_hori(r, y, cb, ud);
			y -= r->height * L.style->space_v;
			r = r->next;
		}
	}
}

void 
gtxt_get_layout_size(float* width, float* height) {
	*width = 0;
	struct row* r = L.head;
	while (r) {
		*width = MAX(*width, r->width + r->offset);
		r = r->next;
	}

	*height = _get_tot_height();
}