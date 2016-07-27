#include "gtxt_label.h"
#include "gtxt_layout.h"
#include "gtxt_richtext.h"
#include "gtxt_adapter.h"
#include "gtxt_glyph.h"

#include <ds_array.h>

#include <string.h>
#include <assert.h>

static struct ds_array* UNICODE_BUF;

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
	const struct gtxt_label_style* style;

	void (*render)(int id, float* texcoords, float x, float y, float w, float h, struct gtxt_draw_style* ds, void* ud);
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
	gtxt_draw_glyph(unicode, x, y, w, h, &params->style->gs, NULL, params->render, params->ud);
}

struct layout_pos {
	int unicode;
	float x, y;
	float w, h;
	float row_y;
};

struct draw_richtext_params {
	struct layout_pos* result;
	int sz;
	int idx;

	void (*render)(int id, float* texcoords, float x, float y, float w, float h, struct gtxt_draw_style* ds, void* ud);
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
		assert(pos->unicode == -1);
		gtxt_ext_sym_render(style->ext_sym_ud, pos->x, pos->y, params->ud);
	} else {
		struct layout_pos* pos = &params->result[params->idx++];
		style->ds.row_y = pos->row_y;
		if (style->ds.row_h == 0) {
			style->ds.row_h = pos->h;
		} 
		if (style->ds.pos_type == GRPT_NULL) {
			style->ds.pos_type = GRPT_BEGIN;
		} else if (style->ds.pos_type == GRPT_BEGIN) {
			style->ds.pos_type = GRPT_MIDDLE;
		} else if (style->ds.pos_type == GRPT_MIDDLE && str[len] == '<') {
			style->ds.pos_type = GRPT_END;
		}
		gtxt_draw_glyph(pos->unicode, pos->x, pos->y, pos->w, pos->h, &style->gs, &style->ds, params->render, params->ud);
	}

	return len;
}

void 
gtxt_label_draw(const char* str, const struct gtxt_label_style* style,  
				void (*render)(int id, float* texcoords, float x, float y, float w, float h, struct gtxt_draw_style* ds, void* ud), void* ud) {
	if (!UNICODE_BUF) {
		UNICODE_BUF = ds_array_create(128, sizeof(int));
	}

	int str_len = strlen(str);
	for (int i = 0; i < str_len; ) {
		int len = _unicode_len(str[i]);
		int unicode = _get_unicode(str + i, len);
		ds_array_add(UNICODE_BUF, &unicode);
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

	ds_array_clear(UNICODE_BUF);
}

static inline int
_layout_richtext_glyph_cb(const char* str, struct gtxt_richtext_style* style, void* ud) {
	int len = _unicode_len(str[0]);
	int unicode = _get_unicode(str, len);

	enum GLO_STATUS status = GLOS_NORMAL;
	if (style->ext_sym_ud) {
		int w, h;
		gtxt_ext_sym_get_size(style->ext_sym_ud, &w, &h);
		gtxt_layout_ext_sym(w, h);
		// todo
//		succ = true;
	} else {
		status = gtxt_layout_single(unicode, style);
	}

	switch (status) {
	case GLOS_NORMAL:
		if (ud) {
			int* count = (int*)ud;
			++*count;
		}
		break;
	case GLOS_FULL:
		len = 0;
		int n = gtxt_layout_add_omit_sym(&style->gs);
		if (ud) {
			int* count = (int*)ud;
			*count += n;
		}
		break;
	}

	return len;
}

static inline void
_get_layout_result_cb(int unicode, float x, float y, float w, float h, float row_y, void* ud) {
	struct draw_richtext_params* params = (struct draw_richtext_params*)ud;
	assert(params->idx < params->sz);

	struct layout_pos* pos = &params->result[params->idx];
	pos->unicode = unicode;
	pos->x = x;
	pos->y = y;
	pos->w = w;
	pos->h = h;
	pos->row_y = row_y;

	++params->idx;
}

void 
gtxt_label_draw_richtext(const char* str, const struct gtxt_label_style* style, int time,
						 void (*render)(int id, float* texcoords, float x, float y, float w, float h, struct gtxt_draw_style* ds, void* ud), void* ud) {
	if (!UNICODE_BUF) {
		UNICODE_BUF = ds_array_create(128, sizeof(int));
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
	gtxt_layout_traverse2(_get_layout_result_cb, &params);

	gtxt_layout_end();

	// draw
	params.idx = 0;
	gtxt_richtext_parser_dynamic(str, style, time, _draw_richtext_glyph_cb, &params);	// layout

	ds_array_clear(UNICODE_BUF);
}

void 
gtxt_label_reload(const char* str, const struct gtxt_label_style* style) {
 	int str_len = strlen(str);
 	for (int i = 0; i < str_len; ) {
 		int len = _unicode_len(str[i]);
 		int unicode = _get_unicode(str + i, len);
		gtxt_reload_glyph(unicode, &style->gs);
 		i += len;
 	}
}

static inline int
_reload_richtext_glyph_cb(const char* str, struct gtxt_richtext_style* style, void* ud) {
	int len = _unicode_len(str[0]);
	int unicode = _get_unicode(str, len);
	if (unicode == '\n') {
		;
	} else if (style->ext_sym_ud) {
		;
	} else {
		gtxt_reload_glyph(unicode, &style->gs);
	}
	return len;
}

void 
gtxt_label_reload_richtext(const char* str, const struct gtxt_label_style* style) {
	gtxt_richtext_parser(str, style, _reload_richtext_glyph_cb, NULL);
}

struct query_richtext_params {
	struct layout_pos* result;
	int sz;
	int idx;

	int qx, qy;

	void* ud;

	void* ret_ext_sym;
};

static inline int
_query_richtext_glyph_cb(const char* str, struct gtxt_richtext_style* style, void* ud) {
	int len = _unicode_len(str[0]);

	struct query_richtext_params* params = (struct query_richtext_params*)ud;
	if (params->idx >= params->sz) {
		return len;
	}

	int unicode = _get_unicode(str, len);
	if (unicode == '\n') {
		return len;
	} else if (style->ext_sym_ud) {
		struct layout_pos* pos = &params->result[params->idx++];
		assert(pos->unicode == -1);
		bool find = gtxt_ext_sym_query(style->ext_sym_ud, pos->x, pos->y, pos->w, pos->h, params->qx, params->qy, params->ud);
		if (find) {
			params->ret_ext_sym = style->ext_sym_ud;
			return 0;
		}
	} else {
		++params->idx;
	}

	return len;
}

void* 
gtxt_label_point_query(const char* str, const struct gtxt_label_style* style, int x, int y, void* ud) {
	if (!UNICODE_BUF) {
		UNICODE_BUF = ds_array_create(128, sizeof(int));
	}

	gtxt_layout_begin(style);

	// layout
	int count = 0;
	gtxt_richtext_parser(str, style, _layout_richtext_glyph_cb, &count);	// layout

	// get layout
	struct layout_pos pos[count];
	struct query_richtext_params params;

	params.result = pos;
	params.sz = count;
	params.idx = 0;
	params.qx = x;
	params.qy = y;
	params.ud = ud;
	params.ret_ext_sym = NULL;
	gtxt_layout_traverse2(_get_layout_result_cb, &params);

	gtxt_layout_end();

	// query
	params.idx = 0;
	gtxt_richtext_parser(str, style, _query_richtext_glyph_cb, &params);	// layout

	ds_array_clear(UNICODE_BUF);

	return params.ret_ext_sym;
}

void 
gtxt_get_label_size(const char* str, const struct gtxt_label_style* style, float* width, float* height) {
	gtxt_layout_begin(style);
	gtxt_richtext_parser(str, style, _layout_richtext_glyph_cb, NULL);
	gtxt_get_layout_size(width, height);
	gtxt_layout_end();
}