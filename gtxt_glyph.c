#include "gtxt_glyph.h"
#include "gtxt_freetype.h"

#include <dtex_hash.h>

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

	struct glyph_bitmap *next;
};

struct glyph {
	struct glyph_key key;

	struct glyph_bitmap* bitmap;
	int bmp_version;
	struct gtxt_glyph_layout layout; 

	struct glyph *prev, *next;
};

struct glyph_cache {
	struct dtex_hash* hash;

	struct glyph_bitmap* bitmap_freelist;
	struct glyph_bitmap *bitmap_head, *bitmap_tail;

	struct glyph* glyph_freelist;
	struct glyph *glyph_head, *glyph_tail;
};

static struct glyph_cache* C;

static uint32_t* (*CHAR_GEN)(const char* str, struct gtxt_glyph_style* style, struct gtxt_glyph_layout* layout) = NULL;

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
				  uint32_t* (*char_gen)(const char* str, struct gtxt_glyph_style* style, struct gtxt_glyph_layout* layout)) {
	CHAR_GEN = char_gen;

	size_t bitmap_sz = sizeof(struct glyph_bitmap) * cap_bitmap;
	size_t layout_sz = sizeof(struct glyph) * cap_layout;
	size_t sz = sizeof(struct glyph_cache) + bitmap_sz + layout_sz;
	C = (struct glyph_cache*)malloc(sz);
	if (!C) {
		return;
	}
	memset(C, 0, sz);

	C->hash = dtex_hash_create(cap_layout, cap_layout * 2, 0.5f, _hash_func, _equal_func);

	// bitmap
	C->bitmap_freelist = (struct glyph_bitmap*)(C + 1);
	for (int i = 0; i < cap_bitmap - 1; ++i) {
		C->bitmap_freelist[i].next = &C->bitmap_freelist[i + 1];
	}
	C->bitmap_freelist[cap_bitmap - 1].next = NULL;
	
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

		dtex_hash_remove(C->hash, &g->key);
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
gtxt_glyph_get_layout(int unicode, struct gtxt_glyph_style* style) {
	struct glyph_key key;
	key.unicode = unicode;
	key.s = *style;

	struct glyph* g = (struct glyph*)dtex_hash_query(C->hash, &key);
	if (g) {
		return &g->layout;
	} else {
		g = _new_node();

		gtxt_ft_get_layout(unicode, style, &g->layout);

		g->key = key;
		dtex_hash_insert(C->hash, &g->key, g, true);

		return &g->layout;
	}
}

uint32_t* 
gtxt_glyph_get_bitmap(int unicode, struct gtxt_glyph_style* style, struct gtxt_glyph_layout* layout) {
	struct glyph_key key;
	key.unicode = unicode;
	key.s = *style;

	struct glyph* g = (struct glyph*)dtex_hash_query(C->hash, &key);
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
		dtex_hash_insert(C->hash, &g->key, g, true);
	}

	if (g->bitmap && g->bitmap->version != g->bmp_version) {
		g->bitmap = NULL;
		g->bmp_version = 0;
	}

	if (!g->bitmap) {
		if (!C->bitmap_freelist) {
			assert(C->bitmap_head);
			struct glyph_bitmap* bmp = C->bitmap_head;
			++bmp->version;
			C->bitmap_head = bmp->next;
			bmp->next = C->bitmap_freelist;
			C->bitmap_freelist = bmp;
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
	if (!C->bitmap_tail) {
		assert(!C->bitmap_head);
		C->bitmap_head = C->bitmap_tail = g->bitmap;
	} else {
		C->bitmap_tail->next = g->bitmap;
		C->bitmap_tail = g->bitmap;
	}
	g->bitmap->next = NULL;

	return ret_buf;
}