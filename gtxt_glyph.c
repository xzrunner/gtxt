#include "gtxt_glyph.h"
#include "gtxt_freetype.h"

#include <ds_hash.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct glyph_key {
	int unicode;
	struct gtxt_glyph_style s;
};

struct glyph_bitmap {
	int version;

	bool valid;

	uint32_t* buf;
	size_t sz;

	struct glyph_bitmap *prev, *next;
};

struct glyph {
	struct glyph_key key;

	struct glyph_bitmap* bitmap;
	int bmp_version;
	struct gtxt_glyph_layout layout; 

	struct glyph *prev, *next;
};

struct glyph_cache {
	struct ds_hash* hash;

	struct glyph_bitmap* bitmap_freelist;
	struct glyph_bitmap *bitmap_head, *bitmap_tail;

	struct glyph* glyph_freelist;
	struct glyph *glyph_head, *glyph_tail;
};

static struct glyph_cache* C;

static uint32_t* (*CHAR_GEN)(const char* str, const struct gtxt_glyph_style* style, struct gtxt_glyph_layout* layout) = NULL;

static inline unsigned int 
_hash_func(int hash_sz, void* key) {
	struct glyph_key* hk = (struct glyph_key*)key;
	uint32_t hash;
	if (hk->s.edge) {
		hash = 
			hk->unicode ^ 
			(hk->s.font * 97) ^ 
			(hk->s.font_size * 101) ^
			hk->s.font_color.integer ^ 
			(int)(hk->s.edge_size * 10000) ^
			hk->s.edge_color.integer;
	} else {
		hash = 
			hk->unicode ^ 
			(hk->s.font * 97) ^ 
			(hk->s.font_size * 101) ^
			hk->s.font_color.integer;
	}
	return hash % hash_sz;
}

static inline bool 
_equal_func(void* key0, void* key1) {
	struct glyph_key* hk0 = (struct glyph_key*)key0;
	struct glyph_key* hk1 = (struct glyph_key*)key1;
	if (hk0->unicode == hk1->unicode && 
		hk0->s.font == hk1->s.font && 
		hk0->s.font_size	== hk1->s.font_size && 
		hk0->s.font_color.integer == hk1->s.font_color.integer && 
		hk0->s.edge == hk1->s.edge) {
		if (hk0->s.edge) {
			return hk0->s.edge_size	== hk1->s.edge_size
				&& hk0->s.edge_color.integer == hk1->s.edge_color.integer;
		} else {
			return true;
		}
	} else {
		return false;
	}
}

void 
gtxt_glyph_create(int cap_bitmap, int cap_layout,
				  uint32_t* (*char_gen)(const char* str, const struct gtxt_glyph_style* style, struct gtxt_glyph_layout* layout)) {
	CHAR_GEN = char_gen;

	size_t bitmap_sz = sizeof(struct glyph_bitmap) * cap_bitmap;
	size_t layout_sz = sizeof(struct glyph) * cap_layout;
	size_t sz = sizeof(struct glyph_cache) + bitmap_sz + layout_sz;
	C = (struct glyph_cache*)malloc(sz);
	if (!C) {
		return;
	}
	memset(C, 0, sz);

	C->hash = ds_hash_create(cap_layout, cap_layout * 2, 0.5f, _hash_func, _equal_func);

	// bitmap
	C->bitmap_freelist = (struct glyph_bitmap*)(C + 1);
	for (int i = 0; i < cap_bitmap - 1; ++i) {
		C->bitmap_freelist[i].next = &C->bitmap_freelist[i + 1];
	}
	C->bitmap_freelist[cap_bitmap - 1].next = NULL;
	C->bitmap_freelist[0].prev = NULL;
	for (int i = 1; i < cap_bitmap; ++i) {
		C->bitmap_freelist[i].prev = &C->bitmap_freelist[i-1];
	}

	// layout
	C->glyph_freelist = (struct glyph*)((intptr_t)C->bitmap_freelist + bitmap_sz);
	for (int i = 0; i < cap_layout - 1; ++i) {
		C->glyph_freelist[i].next = &C->glyph_freelist[i+1];
	}
	C->glyph_freelist[cap_layout - 1].next = NULL;
	C->glyph_freelist[0].prev = NULL;
	for (int i = 1; i < cap_layout; ++i) {
		C->glyph_freelist[i].prev = &C->glyph_freelist[i-1];
	}
}

void 
gtxt_glyph_release() {
	struct glyph_bitmap* bmp = C->bitmap_freelist;
	while (bmp) {
		free(bmp->buf); bmp->buf = NULL;
		bmp->sz = 0;
		bmp = bmp->next;
	}
	bmp = C->bitmap_head;
	while (bmp != C->bitmap_tail) {
		free(bmp->buf); bmp->buf = NULL;
		bmp->sz = 0;
		bmp = bmp->next;
	}

	free(C); C = NULL;
}

static inline struct glyph*
_new_node() {
	if (!C->glyph_freelist) {
		assert(C->glyph_head);
		struct glyph* g = C->glyph_head;
		C->glyph_head = g->next;
		if (!C->glyph_head) {
			C->glyph_tail = NULL;
		}

		ds_hash_remove(C->hash, &g->key);
		if (g->bitmap) {
// 			g->bitmap->valid = false;
// 			g->bitmap->next = C->bitmap_freelist;
// 			C->bitmap_freelist = g->bitmap;
			g->bitmap = NULL;
		}
		g->prev = g->next = NULL;

		C->glyph_freelist = g;
	}

	assert(C->glyph_freelist);
	struct glyph* g = C->glyph_freelist;
	C->glyph_freelist = g->next;

	if (!C->glyph_head) {
		assert(!C->glyph_tail);
		C->glyph_head = C->glyph_tail = g;
		g->prev = g->next = NULL;
	}

	if (g->bitmap) {
		g->bitmap->valid = false;
		g->bmp_version = 0;
	}

	return g;
}

struct gtxt_glyph_layout* 
gtxt_glyph_get_layout(int unicode, const struct gtxt_glyph_style* style) {
	struct glyph_key key;
	key.unicode = unicode;
	key.s = *style;

	struct glyph* g = (struct glyph*)ds_hash_query(C->hash, &key);
	if (g) {
		return &g->layout;
	} else {
		g = _new_node();

		gtxt_ft_get_layout(unicode, style, &g->layout);

		g->key = key;
		ds_hash_insert(C->hash, &g->key, g, true);

		return &g->layout;
	}
}

static inline void
_deconnect_bmp(struct glyph_bitmap* bmp) {
	if (C->bitmap_head == bmp) {
		C->bitmap_head = bmp->next;
	}
	if (C->bitmap_tail == bmp) {
		C->bitmap_tail = bmp->prev;
	}
	if (bmp->prev) {
		bmp->prev->next = bmp->next;
	}
	if (bmp->next) {
		bmp->next->prev = bmp->prev;
	}
}

static inline void
_push_bmp_to_freelist(struct glyph_bitmap* bmp) {
	++bmp->version;
	_deconnect_bmp(bmp);
	bmp->prev = NULL;
	bmp->next = C->bitmap_freelist;
	if (C->bitmap_freelist) {
		C->bitmap_freelist->prev = bmp;
	}
	C->bitmap_freelist = bmp;
}

static inline void
_move_bmp_to_tail(struct glyph_bitmap* bmp) {
	_deconnect_bmp(bmp);
	if (!C->bitmap_head) {
		C->bitmap_head = bmp;
	}
	bmp->prev = C->bitmap_tail;
	if (C->bitmap_tail) {
		C->bitmap_tail->next = bmp;
	}
	bmp->next = NULL;
	C->bitmap_tail = bmp;
}

uint32_t* 
gtxt_glyph_get_bitmap(int unicode, const struct gtxt_glyph_style* style, struct gtxt_glyph_layout* layout) {
	struct glyph_key key;
	key.unicode = unicode;
	key.s = *style;

	struct glyph* g = (struct glyph*)ds_hash_query(C->hash, &key);
	if (g) {
		if (g->prev) {
			g->prev->next = g->next;
		}
		g->next = NULL;
		assert(C->glyph_tail);
		C->glyph_tail->next = g;
		*layout = g->layout;
	} else {
		g = _new_node();
		g->key = key;
		ds_hash_insert(C->hash, &g->key, g, true);
	}

	if (g->bitmap && g->bitmap->version != g->bmp_version) {
		_push_bmp_to_freelist(g->bitmap);
		g->bitmap = NULL;
		g->bmp_version = 0;
	}

	if (!g->bitmap) {
		// move first to freelist
		if (!C->bitmap_freelist) {
			assert(C->bitmap_head);
			_push_bmp_to_freelist(C->bitmap_head);
		}

		g->bitmap = C->bitmap_freelist;
		g->bmp_version = g->bitmap->version;

		C->bitmap_freelist = C->bitmap_freelist->next;
		g->bitmap->valid = false;
	}

	if (g->bitmap->valid) {
		return g->bitmap->buf;
	}

	uint32_t* ret_buf = NULL;

	uint32_t* buf = gtxt_ft_gen_char(unicode, style, &g->layout);
	if (!buf && CHAR_GEN) {
		buf = CHAR_GEN("", style, &g->layout);
	}
	if (!buf) {
		ret_buf = NULL;
	} else {
		*layout = g->layout;
		size_t sz = g->layout.sizer.width * g->layout.sizer.height * sizeof(uint32_t);
		if (sz > g->bitmap->sz) {
			free(g->bitmap->buf);
			g->bitmap->buf = malloc(sz);
			g->bitmap->sz = sz;
		}

		memcpy(g->bitmap->buf, buf, sz);
		g->bitmap->valid = true;

		ret_buf = g->bitmap->buf;
	}

	_move_bmp_to_tail(g->bitmap);

	return ret_buf;
}