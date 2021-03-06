#include "gtxt_glyph.h"
#include "gtxt_freetype.h"

#include <ds_hash.h>
#include <ds_freelist.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct glyph_key {
	int unicode;
	struct gtxt_glyph_style s;
	float line_x;
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

DS_FREELIST(glyph_bitmap)
DS_FREELIST(glyph)

struct glyph_cache {
	struct ds_hash* hash;

	struct ds_freelist_glyph_bitmap bmp_buf;
	struct ds_freelist_glyph gly_buf;
};

static struct glyph_cache* C;

static uint32_t* (*CHAR_GEN)(const char* str, const struct gtxt_glyph_style* style, struct gtxt_glyph_layout* layout);
static void      (*GET_UF_LAYOUT)(int unicode, int font, struct gtxt_glyph_layout* layout);

static inline 
_is_color_same(const struct gtxt_glyph_color* c0, const struct gtxt_glyph_color* c1) {
    if (c0->mode_type != c1->mode_type) {
        return false;
    }

    switch (c0->mode_type)
    {
    case 0:
        if (c0->mode.ONE.color.integer != c1->mode.ONE.color.integer) {
            return false;
        }
        break;
    case 1:
        if (c0->mode.TWO.begin_col.integer != c1->mode.TWO.begin_col.integer ||
            c0->mode.TWO.end_col.integer != c1->mode.TWO.end_col.integer ||
            c0->mode.TWO.begin_pos != c1->mode.TWO.begin_pos ||
            c0->mode.TWO.end_pos != c1->mode.TWO.end_pos ||
            c0->mode.TWO.angle != c1->mode.TWO.angle) {
            return false;
        }
        break;
    case 2:
        if (c0->mode.THREE.begin_col.integer != c1->mode.THREE.begin_col.integer ||
            c0->mode.THREE.mid_col.integer != c1->mode.THREE.mid_col.integer ||
            c0->mode.THREE.end_col.integer != c1->mode.THREE.end_col.integer ||
            c0->mode.THREE.begin_pos != c1->mode.THREE.begin_pos ||
            c0->mode.THREE.mid_pos != c1->mode.THREE.mid_pos ||
            c0->mode.THREE.end_pos != c1->mode.THREE.end_pos ||
            c0->mode.THREE.angle != c1->mode.THREE.angle) {
            return false;
        }
        break;
    default:
        assert(0);
    }

    return true;
}

static inline uint32_t
_hash_color(struct gtxt_glyph_color* col) {
	uint32_t icol = 0;
	switch (col->mode_type)
	{
	case 0:
		icol = col->mode.ONE.color.integer;
		break;
	case 1:
		icol = col->mode.TWO.begin_col.integer ^ col->mode.TWO.end_col.integer;
		break;
	case 2:
		icol = col->mode.THREE.begin_col.integer ^ col->mode.THREE.mid_col.integer ^ col->mode.THREE.end_col.integer;
		break;
	default:
		assert(0);
	}
	return icol;
}

static inline unsigned int
_hash_func(int hash_sz, void* key) {
	struct glyph_key* hk = (struct glyph_key*)key;
	uint32_t hash;
	if (hk->s.edge) {
		struct gtxt_glyph_color* ecol = &hk->s.edge_color;
		hash =
			hk->unicode ^
			(hk->s.font * 97) ^
			(hk->s.font_size * 101) ^
			_hash_color(&hk->s.font_color) ^
			(int)(hk->s.edge_size * 10000) ^
			_hash_color(&hk->s.edge_color) ^
			((int)hk->line_x * 13);
	} else {
		hash =
			hk->unicode ^
			(hk->s.font * 97) ^
			(hk->s.font_size * 101) ^
			_hash_color(&hk->s.font_color) ^
			((int)hk->line_x * 13);
	}
	return hash % hash_sz;
}

static inline bool
_equal_func(void* key0, void* key1) {
	struct glyph_key* hk0 = (struct glyph_key*)key0;
	struct glyph_key* hk1 = (struct glyph_key*)key1;
	if (hk0->unicode == hk1->unicode &&
		hk0->s.font == hk1->s.font &&
		hk0->s.font_size == hk1->s.font_size &&
        _is_color_same(&hk0->s.font_color, &hk1->s.font_color) &&
		hk0->s.edge == hk1->s.edge &&
		hk0->line_x == hk1->line_x) {
		if (hk0->s.edge) {
            return hk0->s.edge_size	== hk1->s.edge_size
			    && _is_color_same(&hk0->s.edge_color, &hk1->s.edge_color);
		} else {
            return true;
		}
	} else {
        return false;
	}
}

void
gtxt_glyph_create(int cap_bitmap, int cap_layout,
				  uint32_t* (*char_gen)(const char* str, const struct gtxt_glyph_style* style, struct gtxt_glyph_layout* layout),
				  void (*get_uf_layout)(int unicode, int font, struct gtxt_glyph_layout* layout)) {
	CHAR_GEN = char_gen;
	GET_UF_LAYOUT = get_uf_layout;

	size_t bitmap_sz = sizeof(struct glyph_bitmap) * cap_bitmap;
	size_t layout_sz = sizeof(struct glyph) * cap_layout;
	size_t sz = sizeof(struct glyph_cache) + bitmap_sz + layout_sz;
	C = (struct glyph_cache*)malloc(sz);
	if (!C) {
		return;
	}
	memset(C, 0, sz);

	C->hash = ds_hash_create(cap_layout, cap_layout * 2, 0.5f, _hash_func, _equal_func);

	DS_FREELIST_CREATE(glyph_bitmap, C->bmp_buf, cap_bitmap, C + 1);
	DS_FREELIST_CREATE(glyph, C->gly_buf, cap_layout, (intptr_t)C->bmp_buf.freelist + bitmap_sz);
}

void
gtxt_glyph_release() {
	struct glyph_bitmap* bmp = C->bmp_buf.freelist;
	while (bmp) {
		free(bmp->buf); bmp->buf = NULL;
		bmp->sz = 0;
		bmp = bmp->next;
	}
	bmp = C->bmp_buf.head;
	while (bmp != C->bmp_buf.tail) {
		free(bmp->buf); bmp->buf = NULL;
		bmp->sz = 0;
		bmp = bmp->next;
	}

	free(C); C = NULL;
}

static inline struct glyph*
_new_node() {
	if (!C) {
		return NULL;
	}

	if (!C->gly_buf.freelist) {
		struct glyph* g = C->gly_buf.head;
		assert(g);
		DS_FREELIST_PUSH_NODE_TO_FREELIST(C->gly_buf, g);
		ds_hash_remove(C->hash, &g->key);
		if (g->bitmap) {
// 			g->bitmap->valid = false;
// 			g->bitmap->next = C->bmp_buf.freelist;
// 			C->bmp_buf.freelist = g->bitmap;
			g->bitmap = NULL;
		}
	}

	struct glyph* g = NULL;
	DS_FREELIST_POP_NODE_FROM_FREELIST(C->gly_buf, g);
	if (g->bitmap) {
		g->bitmap->valid = false;
		g->bmp_version = 0;
	}

	return g;
}

struct gtxt_glyph_layout*
gtxt_glyph_get_layout(int unicode, float line_x, const struct gtxt_glyph_style* style) {
	if (!C) {
		return NULL;
	}

	struct glyph_key key;
	key.unicode = unicode;
	key.s = *style;
    key.line_x = line_x;

	struct glyph* g = (struct glyph*)ds_hash_query(C->hash, &key);
	if (g) {
		return &g->layout;
	} else {
		g = _new_node();

		int ft_count = gtxt_ft_get_font_cout();
		if (style->font < ft_count) {
			gtxt_ft_get_layout(unicode, line_x, style, &g->layout);
		} else {
			GET_UF_LAYOUT(unicode, ft_count - style->font, &g->layout);
		}

		g->key = key;
		ds_hash_insert(C->hash, &g->key, g, true);

		return &g->layout;
	}
}

uint32_t*
gtxt_glyph_get_bitmap(int unicode, float line_x, const struct gtxt_glyph_style* style, struct gtxt_glyph_layout* layout) {
	if (!C) {
		return NULL;
	}

	struct glyph_key key;
	key.unicode = unicode;
	key.s = *style;
	key.line_x = line_x;

	struct glyph* g = (struct glyph*)ds_hash_query(C->hash, &key);
	if (g) {
		DS_FREELIST_MOVE_NODE_TO_TAIL(C->gly_buf, g);
		*layout = g->layout;
	} else {
		g = _new_node();
		g->key = key;
		ds_hash_insert(C->hash, &g->key, g, true);
	}

	if (g->bitmap && g->bitmap->version != g->bmp_version) {
		++g->bitmap->version;
		DS_FREELIST_PUSH_NODE_TO_FREELIST(C->bmp_buf, g->bitmap);
		g->bitmap = NULL;
		g->bmp_version = 0;
	}

	if (!g->bitmap) {
		// move first to freelist
		if (!C->bmp_buf.freelist) {
			assert(C->bmp_buf.head);
			++C->bmp_buf.head->version;
			// shouldn't pass head directly!!
			// DECONNECT_NODE may change the params
			struct glyph_bitmap* bmp = C->bmp_buf.head;
			DS_FREELIST_PUSH_NODE_TO_FREELIST(C->bmp_buf, bmp);
		}

		g->bitmap = C->bmp_buf.freelist;
		g->bmp_version = g->bitmap->version;

		C->bmp_buf.freelist = C->bmp_buf.freelist->next;
		g->bitmap->valid = false;
	}

	if (g->bitmap->valid) {
		return g->bitmap->buf;
	}

	uint32_t* ret_buf = NULL;
	uint32_t* buf = gtxt_ft_gen_char(unicode, line_x, style, &g->layout);
	if (!buf && CHAR_GEN) {
		buf = CHAR_GEN("", style, &g->layout);
	}
	if (!buf) {
		ret_buf = NULL;
	} else {
		*layout = g->layout;
		size_t sz = (size_t)(g->layout.sizer.width * g->layout.sizer.height * sizeof(uint32_t));
		if (sz > g->bitmap->sz) {
			free(g->bitmap->buf);
			g->bitmap->buf = malloc(sz);
			g->bitmap->sz = sz;
		}

		memcpy(g->bitmap->buf, buf, sz);
		g->bitmap->valid = true;

		ret_buf = g->bitmap->buf;
	}

	DS_FREELIST_MOVE_NODE_TO_TAIL(C->bmp_buf, g->bitmap);
	return ret_buf;
}