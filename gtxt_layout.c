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

#define MAX_ROW_INDENT 0.25f

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

struct layout {
	const struct gtxt_label_style* style;

	struct row* head;
	int row_count;

	struct glyph* glyph_freelist;
	size_t glyph_cap;

	struct row* row_freelist;
	size_t row_cap;

	float prev_tot_h;

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
	float h = L.curr_row->height * L.style->space_v;
	L.prev_tot_h += h;
	// over label height
	if (!L.style->overflow) {
		float tot_h = L.prev_tot_h + L.curr_row->height;
		if (tot_h > L.style->height) {
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
		
		unicode == 44		||		// ,
		unicode == 46		||		// .
		unicode == 63		||		// ?
		unicode == 39		||		// '
		unicode == 34		||		// "
		unicode == 33		||		// !
		unicode == 59		||		// ;
		unicode == 58		||		// :
		unicode == 39;				// '
}

static enum GLO_STATUS
_handle_new_line(int unicode, struct gtxt_glyph_layout* g_layout, float w) {
 	if (unicode == '\n') {
 		if (L.curr_row->height == 0) {
 			L.curr_row->height = g_layout->metrics_height;
 		}
 		if (!_new_line()) {
 			return GLOS_FULL;
 		} else {
 			return GLOS_NEWLINE;
 		}
 	} else if (L.curr_row->width + w + L.curr_row->offset > L.style->width) {
 		if (L.curr_row->height == 0) {
 			L.curr_row->height = g_layout->metrics_height;
 		}
 		if (_is_punctuation(unicode)) {
 			L.curr_row->offset = L.style->width - L.curr_row->width - w;
			if (-L.curr_row->offset / L.style->width > MAX_ROW_INDENT) {
				L.curr_row->offset = L.style->width - L.curr_row->width;
				if (!_new_line()) {
					return GLOS_FULL;
				}
			}
 		} else {
 			if (!_new_line()) {
 				return GLOS_FULL;
 			}
 		}
 	}
	return GLOS_NORMAL;
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
	float w = g_layout->advance * L.style->space_h;
	enum GLO_STATUS status = _handle_new_line(unicode, g_layout, w);
	if (status != GLOS_NORMAL) {
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

	return GLOS_NORMAL;
}

void 
gtxt_layout_multi(struct ds_array* unicodes) {
	int glyph_sz = ds_array_size(unicodes);
	_prepare_glyph_freelist(glyph_sz);

	for (int i = 0; i < glyph_sz; ++i) {
		int unicode = *(int*)ds_array_fetch(unicodes, i);
		enum GLO_STATUS status = gtxt_layout_single(unicode, NULL);
		if (status == GLOS_FULL) {
			gtxt_layout_add_omit_sym(&L.style->gs);
			break;
		}
	}
}

static inline float
_get_omit_sym_width(const struct gtxt_glyph_style* gs) {
	struct gtxt_glyph_layout* layout = gtxt_glyph_get_layout(OMIT_UNICODE, gs);
	float w = layout->advance * L.style->space_h;
	return w;
}

int
gtxt_layout_add_omit_sym(const struct gtxt_glyph_style* gs) {
	float omit_w = _get_omit_sym_width(gs) * OMIT_COUNT;
	if (omit_w > L.style->width) {
		return 0;
	}
	int count = 0;
	float max_w = L.style->width - omit_w;
	struct row* row = L.curr_row;
	float w = 0;
	struct glyph* curr = row->head;
	struct glyph* prev = NULL;
	while (prev != row->tail) {
		w += curr->out_width;
		if (w > max_w) {
			row->width = w - curr->out_width;
			if (prev) {
				struct glyph* del_curr = curr;
				while (del_curr) {
					--count;
					struct glyph* del_next = del_curr->next;
					del_curr->next = L.glyph_freelist;
					L.glyph_freelist = del_curr;
					del_curr = del_next;
				}
				prev->next = NULL;
				row->tail = prev;
			}
			break;
		}
		prev = curr;
		curr = curr->next;
	}

	for (int i = 0; i < OMIT_COUNT; ++i) {
		++count;
		gtxt_layout_single(OMIT_UNICODE, NULL);
	}
	row->width += omit_w;

	row->glyph_count += count;

	return count;
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
	if (L.style->align_h == HA_TILE) {
		float grid = (float)L.style->width / r->glyph_count;
		float x = _get_start_x(r->width + r->offset) + grid * 0.5f;
		float dx = r->offset / r->glyph_count;
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
		float x = _get_start_x(r->width + r->offset);
		float dx = r->offset / r->glyph_count;
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
		*width = MAX(*width, r->width);
		r = r->next;
	}

	*height = _get_tot_height();
}