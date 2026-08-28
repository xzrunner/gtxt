#include "gtxt_label.h"
#include "gtxt_layout.h"
#include "gtxt_richtext.h"
#include "gtxt_glyph.h"
#include "gtxt_util.h"

#include <ds_array.h>

#include <string.h>
#include <stddef.h>
#include <assert.h>

static struct ds_array* UNICODE_BUF;

static gtxt_draw_glyph_fn DRAW_GLYPH;

void
gtxt_label_cb_init(gtxt_draw_glyph_fn draw_glyph) {
	DRAW_GLYPH = draw_glyph;
}

gtxt_draw_glyph_fn
gtxt_label_cb_get(void) {
	return DRAW_GLYPH;
}

struct draw_params {
	const struct gtxt_label_style* style;
	void* ud;
	gtxt_draw_glyph_fn draw_glyph;
};

static inline void
_draw_glyph_cb(int unicode, float x, float y, float w, float h, float row_y, float start_x, void* ud) {
	struct draw_params* params = (struct draw_params*)ud;
	gtxt_draw_glyph_fn fn = params->draw_glyph ? params->draw_glyph : DRAW_GLYPH;
	if (fn) {
		fn(unicode, x, y, w, h, start_x, &params->style->gs, NULL, params->ud);
	}
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

	void* ud;
};

static inline void
_draw_unicode(const char* str, int unicode_len, struct gtxt_richtext_style* style, const struct layout_pos* pos, struct draw_richtext_params* params) {
	style->ds.row_y = pos->row_y;
	if (style->ds.row_h == 0) {
		style->ds.row_h = pos->h;
	}
	if (style->ds.pos_type == GRPT_NULL) {
		style->ds.pos_type = GRPT_BEGIN;
	} else if (style->ds.pos_type == GRPT_BEGIN) {
		style->ds.pos_type = GRPT_MIDDLE;
	} else if (style->ds.pos_type == GRPT_MIDDLE && str[unicode_len] == '<') {
		style->ds.pos_type = GRPT_END;
	}
	if (DRAW_GLYPH) {
		DRAW_GLYPH(pos->unicode, pos->x, pos->y, pos->w, pos->h, 0, &style->gs, &style->ds, params->ud);
	}
}

static inline int
_draw_richtext_glyph_cb(const char* str, struct gtxt_richtext_style* style, void* ud) {
	int len = gtxt_unicode_len(str[0]);

	struct draw_richtext_params* params = (struct draw_richtext_params*)ud;
	if (params->idx >= params->sz) {
		return len;
	}

	int unicode = gtxt_get_unicode(str, len);
	if (unicode == '\n') {
		return len;
	} else if (style->ext_sym_ud) {
		struct layout_pos* pos = &params->result[params->idx++];
		if (pos->unicode == -1) {
			gtxt_ext_sym_render(style->ext_sym_ud, pos->x, pos->y, params->ud);
		} else {
			_draw_unicode(str, len, style, pos, params);
		}
	} else {
		struct layout_pos* pos = &params->result[params->idx++];
		if (unicode != pos->unicode) {
			len = 0;
		}
		_draw_unicode(str, len, style, pos, params);
	}

	return len;
}

static bool
_ensure_unicode_buf(void) {
	if (!UNICODE_BUF) {
		UNICODE_BUF = ds_array_create(128, sizeof(int));
		if (!UNICODE_BUF) {
			return false;
		}
		if (!ds_array_data(UNICODE_BUF)) {
			ds_array_release(UNICODE_BUF);
			UNICODE_BUF = NULL;
			return false;
		}
	}
	return true;
}

static bool
_fill_unicodes_n(const char* str, size_t nbytes) {
	if (!_ensure_unicode_buf()) {
		return false;
	}
	ds_array_clear(UNICODE_BUF);
	if (!str || nbytes == 0) {
		return true;
	}
	size_t i = 0;
	while (i < nbytes) {
		int len = gtxt_unicode_len(str[i]);
		if (len < 1) {
			len = 1;
		}
		if (i + (size_t)len > nbytes) {
			break;
		}
		int unicode = gtxt_get_unicode(str + i, len);
		ds_array_add(UNICODE_BUF, &unicode);
		i += (size_t)len;
	}
	return true;
}

static void
_draw_filled(const struct gtxt_label_style* style, gtxt_draw_glyph_fn draw_glyph, void* ud) {
	if (!style || !UNICODE_BUF) {
		return;
	}
	struct draw_params params;
	params.style = style;
	params.ud = ud;
	params.draw_glyph = draw_glyph;

	gtxt_layout_begin(style);
	gtxt_layout_multi(UNICODE_BUF);
	gtxt_layout_traverse(_draw_glyph_cb, &params);
	gtxt_layout_end();

	ds_array_clear(UNICODE_BUF);
}

void
gtxt_label_draw(const char* str, const struct gtxt_label_style* style, void* ud) {
	if (!str || !style) {
		return;
	}
	if (!_fill_unicodes_n(str, strlen(str))) {
		return;
	}
	_draw_filled(style, NULL, ud);
}

void
gtxt_label_draw_n(const char* str, size_t nbytes, const struct gtxt_label_style* style,
                  gtxt_draw_glyph_fn draw_glyph, void* ud) {
	if (!style || !_fill_unicodes_n(str, nbytes)) {
		return;
	}
	_draw_filled(style, draw_glyph, ud);
}

static inline int
_layout_richtext_glyph_cb(const char* str, float line_x, struct gtxt_richtext_style* style, void* ud) {
	int ret = gtxt_unicode_len(str[0]);
	int unicode = gtxt_get_unicode(str, ret);

	enum GLO_STATUS status = GLOS_NORMAL;
	if (style->ext_sym_ud) {
		int w, h;
		gtxt_ext_sym_get_size(style->ext_sym_ud, &w, &h);
		status = gtxt_layout_ext_sym(w, h);
	} else {
		status = gtxt_layout_single(unicode, line_x, style);
	}

	int n = 0;
	switch (status) {
	case GLOS_NORMAL:
		if (ud) {
			int* count = (int*)ud;
			++*count;
		}
		break;
	case GLOS_FULL:
		ret = -1;
		n = gtxt_layout_add_omit_sym(&style->gs);
		if (ud) {
			int* count = (int*)ud;
			*count += n;
		}
		break;
	case GLOS_CONNECTION:
		if (ud) {
			int* count = (int*)ud;
			*count += 2;
		}
		break;
	}

	return ret;
}

static inline void
_get_layout_result_cb(int unicode, float x, float y, float w, float h, float row_y, float start_x, void* ud) {
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
gtxt_label_draw_richtext(const char* str, const struct gtxt_label_style* style, int time, void* ud) {
	if (!str || !style || !_ensure_unicode_buf()) {
		return;
	}

	gtxt_layout_begin(style);

	// layout
	int count = 0;
	gtxt_richtext_parser(str, style, _layout_richtext_glyph_cb, &count);	// layout

	// get layout
	ARRAY(struct layout_pos, pos, count);
	struct draw_richtext_params params;

	params.result = pos;
	params.sz = count;
	params.idx = 0;
	params.ud = ud;
	gtxt_layout_traverse(_get_layout_result_cb, &params);

	gtxt_layout_end();

	// draw
	params.idx = 0;
	gtxt_richtext_parser_dynamic(str, style, time, _draw_richtext_glyph_cb, &params);	// layout

	ds_array_clear(UNICODE_BUF);
}

// void
// gtxt_label_reload(const char* str, const struct gtxt_label_style* style) {
//  	int str_len = strlen(str);
//  	for (int i = 0; i < str_len; ) {
//  		int len = gtxt_unicode_len(str[i]);
//  		int unicode = gtxt_get_unicode(str + i, len);
// 		gtxt_reload_glyph(unicode, &style->gs);
//  		i += len;
//  	}
// }

// static inline int
// _reload_richtext_glyph_cb(const char* str, struct gtxt_richtext_style* style, void* ud) {
// 	int len = gtxt_unicode_len(str[0]);
// 	int unicode = gtxt_get_unicode(str, len);
// 	if (unicode == '\n') {
// 		;
// 	} else if (style->ext_sym_ud) {
// 		;
// 	} else {
// 		gtxt_reload_glyph(unicode, &style->gs);
// 	}
// 	return len;
// }

// void
// gtxt_label_reload_richtext(const char* str, const struct gtxt_label_style* style) {
// 	gtxt_richtext_parser(str, style, _reload_richtext_glyph_cb, NULL);
// }

struct query_richtext_params {
	struct layout_pos* result;
	int sz;
	int idx;

	int qx, qy;

	void* ud;

	void* ret_ext_sym;
};

static inline int
_query_richtext_glyph_cb(const char* str, float line_x, struct gtxt_richtext_style* style, void* ud) {
	int len = gtxt_unicode_len(str[0]);

	struct query_richtext_params* params = (struct query_richtext_params*)ud;
	if (params->idx >= params->sz) {
		return len;
	}

	int unicode = gtxt_get_unicode(str, len);
	if (unicode == '\n') {
		return len;
	} else if (style->ext_sym_ud) {
		struct layout_pos* pos = &params->result[params->idx++];
		assert(pos->unicode == -1);
		bool find = gtxt_ext_sym_query(style->ext_sym_ud, pos->x, pos->y, pos->w, pos->h, params->qx, params->qy, params->ud);
		if (find) {
			params->ret_ext_sym = style->ext_sym_ud;
			style->ext_sym_ud = NULL;
			return -1;
		}
	} else {
		++params->idx;
	}

	return len;
}

void*
gtxt_label_point_query(const char* str, const struct gtxt_label_style* style, int x, int y, void* ud) {
	if (!str || !style || !_ensure_unicode_buf()) {
		return NULL;
	}

	gtxt_layout_begin(style);

	// layout
	int count = 0;
	gtxt_richtext_parser(str, style, _layout_richtext_glyph_cb, &count);	// layout

	// get layout
	ARRAY(struct layout_pos, pos, count);
	struct query_richtext_params params;

	params.result = pos;
	params.sz = count;
	params.idx = 0;
	params.qx = x;
	params.qy = y;
	params.ud = ud;
	params.ret_ext_sym = NULL;
	gtxt_layout_traverse(_get_layout_result_cb, &params);

	gtxt_layout_end();

	// query
	params.idx = 0;
	gtxt_richtext_parser(str, style, _query_richtext_glyph_cb, &params);	// layout

	ds_array_clear(UNICODE_BUF);

	return params.ret_ext_sym;
}

void
gtxt_get_label_size(const char* str, const struct gtxt_label_style* style, float* width, float* height) {
	if (width) {
		*width = 0;
	}
	if (height) {
		*height = 0;
	}
	if (!str || !style || !width || !height) {
		return;
	}
	gtxt_layout_begin(style);
	gtxt_richtext_parser(str, style, _layout_richtext_glyph_cb, NULL);
	gtxt_get_layout_size(width, height);
	gtxt_layout_end();
}
