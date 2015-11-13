#include "gtxt_label.h"
#include "gtxt_layout.h"
#include "gtxt_richtext.h"
#include "gtxt_render.h"

#include <dtex_array.h>

#include <string.h>
#include <assert.h>

static struct dtex_array* UNICODE_BUF;

static inline int
_unicode_len(const char chr) {
	uint8_t c = (uint8_t)chr;
	if ((c&0x80) == 0) {
		return 1;
	} else if ((c&0xe0) == 0xc0) {
		return 2;
	} else if ((c&0xf0) == 0xe0) {
		return 3;
	} else if ((c&0xf8) == 0xf0) {
		return 4;
	} else if ((c&0xfc) == 0xf8) {
		return 5;
	} else {
		return 6;
	}
}

//static inline int
//_get_unicode_and_char(const char* str, int n, char* utf8) {
//	int i;
//	utf8[0] = str[0];
//	int unicode = utf8[0] & ((1 << (8 - n)) - 1);
//	for (i = 1; i < n; ++i) {
//		utf8[i] = str[i];
//		unicode = unicode << 6 | ((uint8_t)utf8[i] & 0x3f);
//	}
//	utf8[i] = 0;
//	return unicode;
//}

struct draw_params {
	struct gtxt_label_style* style;

	void (*render)(int id, float* texcoords, float x, float y, float w, float h, void* ud);
	void* ud;
};

static inline int
_get_unicode(const char* str, int n) {
	int unicode = str[0] & ((1 << (8 - n)) - 1);
	for (int i = 1; i < n; ++i) {
		unicode = unicode << 6 | ((uint8_t)str[i] & 0x3f);
	}
	return unicode;
}

static inline void
_draw_glyph_cb(int unicode, float x, float y, float w, float h, void* ud) {
	struct draw_params* params = (struct draw_params*)ud;
	gtxt_draw_glyph(unicode, &params->style->gs, x, y, w, h, params->render, params->ud);
}

struct layout_pos {
	int unicode;
	float x, y;
	float w, h;
};

struct draw_richtext_params {
	struct layout_pos* result;
	int sz;
	int idx;

	void (*render)(int id, float* texcoords, float x, float y, float w, float h, void* ud);
	void* ud;
};

static inline int
_draw_richtext_glyph_cb(const char* str, struct gtxt_richtext_style* style, void* ud) {
	int len = _unicode_len(str[0]);

	struct draw_richtext_params* params = (struct draw_richtext_params*)ud;
	if (params->idx >= params->sz) {
		return len;
	}

	int unicode = _get_unicode(str, len);
	if (unicode == '\n') {
		return len;
	} else if (style->ext_sym_ud) {
		struct layout_pos* pos = &params->result[params->idx++];
		assert(unicode == ' ' && pos->unicode == -1);	
		gtxt_ext_sym_render(style->ext_sym_ud, pos->x, pos->y, params->ud);
	} else {
		struct layout_pos* pos = &params->result[params->idx++];
		assert(pos->unicode == unicode);
		gtxt_draw_glyph(unicode, &style->gs, pos->x, pos->y, pos->w, pos->h, params->render, params->ud);
	}

	return len;
}

void 
gtxt_label_draw(const char* str, struct gtxt_label_style* style,  
				void (*render)(int id, float* texcoords, float x, float y, float w, float h, void* ud), void* ud) {
	if (!UNICODE_BUF) {
		UNICODE_BUF = dtex_array_create(128, sizeof(int));
	}

	int str_len = strlen(str);
	for (int i = 0; i < str_len; ) {
		int len = _unicode_len(str[i]);
		int unicode = _get_unicode(str + i, len);
		dtex_array_add(UNICODE_BUF, &unicode);
		i += len;
	}

	struct draw_params params;
	params.style = style;
	params.render = render;
	params.ud = ud;

	gtxt_layout_begin(style);
	gtxt_layout_multi(UNICODE_BUF);					// layout
	gtxt_layout_traverse(_draw_glyph_cb, &params);	// draw
	gtxt_layout_end();

	dtex_array_clear(UNICODE_BUF);
}

static inline int
_layout_richtext_glyph_cb(const char* str, struct gtxt_richtext_style* style, void* ud) {
	int len = _unicode_len(str[0]);
	int unicode = _get_unicode(str, len);

	bool succ = false;
	if (style->ext_sym_ud) {
		assert(unicode == ' ');		
		int w, h;
		gtxt_ext_sym_get_size(style->ext_sym_ud, &w, &h);
		gtxt_layout_ext_sym(w, h);
		succ = true;
	} else {
		succ = gtxt_layout_single(unicode, style);
	}
	if (succ) {
		int* count = (int*)ud;
		++*count;
	}

	return len;
}

static inline void
_get_layout_result_cb(int unicode, float x, float y, float w, float h, void* ud) {
	struct draw_richtext_params* params = (struct draw_richtext_params*)ud;
	params->result[params->idx].unicode = unicode;
	params->result[params->idx].x = x;
	params->result[params->idx].y = y;
	params->result[params->idx].w = w;
	params->result[params->idx].h = h;
	++params->idx;
}

void 
gtxt_label_draw_richtext(const char* str, struct gtxt_label_style* style, 
						 void (*render)(int id, float* texcoords, float x, float y, float w, float h, void* ud), void* ud) {
	if (!UNICODE_BUF) {
		UNICODE_BUF = dtex_array_create(128, sizeof(int));
	}

	gtxt_layout_begin(style);

	// layout
	int count = 0;
	gtxt_richtext_parser(str, style, _layout_richtext_glyph_cb, &count);	// layout

	// get layout
	struct layout_pos pos[count];
	struct draw_richtext_params params;

	params.result = pos;
	params.sz = count;
	params.idx = 0;
	params.render = render;
	params.ud = ud;
	gtxt_layout_traverse(_get_layout_result_cb, &params);

	gtxt_layout_end();

	// draw
	params.idx = 0;
	gtxt_richtext_parser(str, style, _draw_richtext_glyph_cb, &params);	// layout

	dtex_array_clear(UNICODE_BUF);
}