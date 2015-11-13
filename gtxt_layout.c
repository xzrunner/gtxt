#include "gtxt_layout.h"
#include "gtxt_glyph.h"
#include "gtxt_label.h"
#include "gtxt_richtext.h"

#include <dtex_array.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define INIT_ROW_CAP 4
#define INIT_GLYPH_CAP 16

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

	struct glyph *head, *tail;

	struct row* next;
};

struct layout {
	struct gtxt_label_style* style;

	struct row* head;

	struct glyph* glyph_freelist;
	size_t glyph_cap;

	struct row* row_freelist;
	size_t row_cap;

	float tot_height;

	struct row* curr_row;
};

static struct layout L;

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
gtxt_layout_begin(struct gtxt_label_style* style) {
	_prepare_freelist(INIT_ROW_CAP, INIT_GLYPH_CAP);

	L.style = style;

	L.tot_height = 0;

	L.curr_row = _new_row();
	L.head = L.curr_row;
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
	while (r && r->head) {
		if (last_tail) {
			last_tail->next = r->head;
		}
		last_tail = r->tail;

		r->width = r->height = 0;
		r->head = NULL;
		r->tail = NULL;

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
_line_feed() {
	float h = L.curr_row->height * L.style->space_v;
	L.tot_height += h;
// 	if (L.tot_height > L.style->height) {
// 		return false;
// 	}

	struct row* prev = L.curr_row;
	L.curr_row = _new_row();
	if (!L.curr_row) {
		return false;
	}
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
}

bool 
gtxt_layout_single(int unicode, struct gtxt_richtext_style* style) {
	struct gtxt_glyph_style* gs;
	if (style) {
		gs = &style->gs;
	} else {
		gs = &L.style->gs;
	}

	struct gtxt_glyph_layout* g_layout = gtxt_glyph_get_layout(unicode, gs);
	float w = MAX(g_layout->advance, g_layout->sizer.width) * L.style->space_h;
	if (unicode == '\n' || L.curr_row->width + w > L.style->width) {
		if (!_line_feed()) {
			return false;
		}
		if (unicode == '\n') {
			return false;
		}
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
	L.curr_row->width += w;

	_add_glyph(g);

	return true;
}

void 
gtxt_layout_multi(struct dtex_array* unicodes) {
	int glyph_sz = dtex_array_size(unicodes);
	_prepare_glyph_freelist(glyph_sz);

	for (int i = 0; i < glyph_sz; ++i) {
		int unicode = *(int*)dtex_array_fetch(unicodes, i);
		gtxt_layout_single(unicode, NULL);
	}
}

bool 
gtxt_layout_ext_sym(int width, int height) {
	if (L.curr_row->width + width > L.style->width) {
		if (!_line_feed()) {
			return false;
		}
	}

	if (height > L.curr_row->height) {
		L.curr_row->height = height;
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

	return true;
}

void 
gtxt_layout_traverse(void (*cb)(int unicode, float x, float y, float w, float h, void* ud), void* ud) {
	float x, y;

	float tot_height = L.tot_height + L.curr_row->height;
	switch (L.style->align_v) {
	case VA_TOP: case VA_AUTO:
		y = L.style->height * 0.5f;
		break;
	case VA_BOTTOM:
		y = -L.style->height * 0.5f + tot_height;
		break;
	case VA_CENTER:
		y = tot_height * 0.5f;
		break;
	default:
		assert(0);
	}

	struct row* r = L.head;
	while (r) {
		switch (L.style->align_h) {
		case HA_LEFT: case HA_AUTO:
			x = -L.style->width * 0.5f;
			break;
		case HA_RIGHT:
			x = L.style->width * 0.5f - r->width;
			break;
		case HA_CENTER:
			x = -r->width * 0.5f;
			break;
		}

		struct glyph* g = r->head;
		while (g) {
			cb(g->unicode, x + g->x + g->w * 0.5f, y + g->y - g->h * 0.5f - r->height, g->w, g->h, ud);
			x += g->out_width;
			g = g->next;
		}

		y -= r->height * L.style->space_v;
		r = r->next;
	}
}