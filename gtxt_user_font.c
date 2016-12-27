#include "gtxt_user_font.h"
#include "gtxt_glyph.h"
#include "gtxt_util.h"

#include <ds_hash.h>
#include <logger.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define MAX_FONTS	8
#define MAX_CHARS	128

struct character {
	int unicode;
	int w, h;
	void* ud;
};

struct font {
	// todo name

	struct ds_hash* hash;
};

struct user_fonts {
	struct font fonts[MAX_FONTS];
	int fonts_count;

	struct character chars[MAX_CHARS];
	int chars_count;
};

static struct user_fonts* UF = NULL;

float* (*LOAD_AND_QUERY)(void* ud, struct dtex_glyph* glyph);

void 
gtxt_uf_cb_init(float* (*load_and_query)(void* ud, struct dtex_glyph* glyph)) {
//	LOAD_AND_QUERY = load_and_query;
}

void 
gtxt_uf_create() {
	UF = (struct user_fonts*)malloc(sizeof(*UF));
	if (!UF) {
		return;
	}
	memset(UF, 0, sizeof(*UF));
}

void 
gtxt_uf_release() {
	for (int i = 0; i < UF->fonts_count; ++i) {
		struct font* f = &UF->fonts[i];
		ds_hash_release(f->hash);
	}

	free(UF); UF = NULL;
}

static inline unsigned int
_hash_func(int hash_sz, void* key) {
	int unicode = *(int*)(key);
	return unicode % hash_sz;
}

static inline bool
_equal_func(void* key0, void* key1) {
	int u0 = *(int*)(key0),
		u1 = *(int*)(key1);
	return u0 == u1;
}

int 
gtxt_uf_add_font(const char* name, int cap) {
	if (UF->fonts_count >= MAX_FONTS) {
		LOGW("%s", "gtxt_uf_add_font font full.");
		return -1;
	}

	struct font* f = &UF->fonts[UF->fonts_count++];

	f->hash = ds_hash_create(cap, cap * 2, 0.5f, _hash_func, _equal_func);

	return UF->fonts_count - 1;
}

void 
gtxt_uf_add_char(int font, const char* str, int w, int h, void* ud) {
	if (UF->chars_count >= MAX_CHARS) {
		LOGW("%s", "gtxt_uf_add_char char full.");
		return;
	}

	assert(font < UF->fonts_count);

	int len = gtxt_unicode_len(str[0]);
	int unicode = gtxt_get_unicode(str, len);

	struct character* c = &UF->chars[UF->chars_count++];
	c->unicode = unicode;
	c->w = w;
	c->h = h;
	c->ud = ud;
	ds_hash_insert(UF->fonts[font].hash, &c->unicode, c, true);
}

float* 
gtxt_uf_query_and_load(int font, int unicode, struct dtex_glyph* glyph) {
// 	assert(font < UF->fonts_count);
// 	struct character* c = ds_hash_query(UF->fonts[font].hash, &unicode);
// 	if (!c) {
// 		return NULL;
// 	} else {
// 		struct dtex_cg* cg = dtexf_get_cg();
// 		return dtex_cg_load_user(cg, glyph, LOAD_AND_QUERY, c->ud);
// 	}

	return NULL;
}

void 
gtxt_uf_get_layout(int unicode, int font, struct gtxt_glyph_layout* layout) {
	assert(font < UF->fonts_count);
	struct character* c = ds_hash_query(UF->fonts[font].hash, &unicode);
	if (!c) {
		return;
	}

	layout->sizer.width = c->w;
	layout->sizer.height = c->h;
	layout->bearing_x = 0;
	layout->bearing_y = 0;
	layout->advance = c->w;
	layout->metrics_height = c->h;
}