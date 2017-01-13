#include "gtxt_richtext.h"
#include "gtxt_label.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_LAYER_COUNT		16
#define MAX_FONT_COUNT		16
#define MAX_COLOR_COUNT		128

#define MIN_FONT_SIZE		4
#define MAX_FONT_SIZE		128

struct edge_style {
	float size;
	union gtxt_color color;
};

struct dynamic_value {
	float start;
	float max, min;

	float glyph_dt;
	float time_dt;
};

struct dynamic_draw_style {
	bool enable;
	struct dynamic_value alpha;
	struct dynamic_value scale;
	struct dynamic_value offset_x, offset_y;
};

struct richtext_state {
	int font[MAX_LAYER_COUNT];
	int font_layer;

	int size[MAX_LAYER_COUNT];
	int size_layer;

	union gtxt_color color[MAX_LAYER_COUNT];
	int color_layer;

	struct edge_style edge[MAX_LAYER_COUNT];
	int edge_layer;

	struct gtxt_richtext_style s;

	struct dynamic_draw_style dds;

	bool disable;
	int disable_num;
};

static char FONTS[MAX_FONT_COUNT][128];
static int FONT_SIZE = 0;

static void* (*EXT_SYM_CREATE)(const char* str);
static void (*EXT_SYM_RELEASE)(void* ext_sym);
static void (*EXT_SYM_SIZE)(void* ext_sym, int* width, int* height);
static void (*EXT_SYM_RENDER)(void* ext_sym, float x, float y, void* ud);
static bool (*EXT_SYM_QUERY)(void* ext_sym, float x, float y, float w, float h, int qx, int qy, void* ud);

struct color_map {
	char name[32];
	union gtxt_color color;
};

static struct color_map COLOR[MAX_COLOR_COUNT] = {
	{ "aqua",		.color.integer = 0x00ffffff },
	{ "black",		.color.integer = 0x000000ff },
	{ "blue",		.color.integer = 0x0000ffff },
	{ "brown",		.color.integer = 0xa52a2aff },
	{ "cyan",		.color.integer = 0x00ffffff },
	{ "darkblue",	.color.integer = 0x0000a0ff },
	{ "fuchsia",	.color.integer = 0xff00ffff },
	{ "green",		.color.integer = 0x008000ff },
	{ "grey",		.color.integer = 0x808080ff },
	{ "lightblue",	.color.integer = 0xadd8e6ff },
	{ "lime",		.color.integer = 0x00ff00ff },
	{ "magenta",	.color.integer = 0xff00ffff },
	{ "maroon",		.color.integer = 0x800000ff },
	{ "navy",		.color.integer = 0x000080ff },
	{ "olive",		.color.integer = 0x808000ff },
	{ "orange",		.color.integer = 0xffa500ff },
	{ "purple",		.color.integer = 0x800080ff },
	{ "red",		.color.integer = 0xff0000ff },
	{ "silver",		.color.integer = 0xc0c0c0ff },
	{ "teal",		.color.integer = 0x008080ff },
	{ "white",		.color.integer = 0xffffffff },
	{ "yellow",		.color.integer = 0xffff00ff }
};

#define DEFAULT_COLOR_SIZE 22

static int COLOR_SIZE = DEFAULT_COLOR_SIZE;

void 
gtxt_richtext_release() {
	FONT_SIZE = 0;
	COLOR_SIZE = DEFAULT_COLOR_SIZE;
}

void 
gtxt_richtext_add_color(const char* key, unsigned int val) {
	if (COLOR_SIZE >= MAX_COLOR_COUNT) {
		printf("gtxt_richtext_add_color COLOR_SIZE over %d !\n", MAX_COLOR_COUNT);
		return;
	}

	struct color_map* cm = &COLOR[COLOR_SIZE++];
	strcpy(cm->name, key);
	cm->color.integer = val;
}

static const char* 
_skip_delimiter_and_equal(const char* str) {
	const char* ptr = str;
	int len;
	do {
		if (*ptr == '=') {
			len = 1;
		} else {
			len = gtxt_richtext_get_delimiter(ptr);
		}
		ptr += len;
	} while (len != 0);
	return ptr;
}

static inline bool
_str_head_equal(const char* str, const char* substr) {
	return strncmp(str, substr, strlen(substr)) == 0;
}

static inline bool
_parser_color(const char* token, union gtxt_color* col, const char** end_ptr) {
	if (token[0] == '#') {
		col->integer = strtoul(&token[1], end_ptr, 16);
		return true;
	} else {
		for (int i = 0; i < COLOR_SIZE; ++i) {
			if (_str_head_equal(token, COLOR[i].name)) {
				*col = COLOR[i].color;
				if (end_ptr) {
					*end_ptr = &token[strlen(COLOR[i].name)];
				}
				return true;
			}
		}
	}
	return false;
}

void 
gtxt_richtext_add_font(const char* name) {
	if (FONT_SIZE >= MAX_FONT_COUNT) {
		printf("gtxt_richtext_add_font FONT_SIZE over %d !\n", MAX_FONT_COUNT);
		return;
	}

	strcpy(&FONTS[FONT_SIZE][0], name);
	FONTS[FONT_SIZE][strlen(name) + 1] = 0;
	++FONT_SIZE;
}

void 
gtxt_richtext_ext_sym_cb_init(void* (*create)(const char* str),
							  void (*release)(void* ext_sym),
							  void (*size)(void* ext_sym, int* width, int* height), 
							  void (*render)(void* ext_sym, float x, float y, void* ud),
							  bool (*query)(void* ext_sym, float x, float y, float w, float h, int qx, int qy, void* ud)) {
	EXT_SYM_CREATE = create;
	EXT_SYM_RELEASE = release;
	EXT_SYM_SIZE = size;
	EXT_SYM_RENDER = render;
	EXT_SYM_QUERY = query;
}

void 
gtxt_ext_sym_get_size(void* ext_sym, int* width, int* height) {
	EXT_SYM_SIZE(ext_sym, width, height);
}

void 
gtxt_ext_sym_render(void* ext_sym, float x, float y, void* ud) {
	EXT_SYM_RENDER(ext_sym, x, y, ud);
}

bool 
gtxt_ext_sym_query(void* ext_sym, float x, float y, float w, float h, int qx, int qy, void* ud) {
	return EXT_SYM_QUERY(ext_sym, x, y, w, h, qx, qy, ud);
}

static inline int
_parser_font(const char* token) {
	for (int i = 0; i < MAX_FONT_COUNT; ++i) {
		if (strcmp(FONTS[i], token) == 0) {
			return i;
		}
	}
	return -1;
}

static inline void 
_parser_edge(const char* token, struct edge_style* es) {
	const char* end;
	if (_str_head_equal(token, "size=")) {
		float sz = strtod(&token[strlen("size=")], &end);
		if (sz >= 0) {
			es->size = sz;
		}
	} else if (_str_head_equal(token, "color=")) {
		es->color.integer = 0;
		_parser_color(&token[strlen("color=")], &es->color, &end);
	}
	if (*end) {
		_parser_edge(gtxt_richtext_skip_delimiter(end), es);
	}
}

static inline void
_parser_dynamic_value(const char* token, struct dynamic_value* val) {
	const char* end = token;
	val->start = strtod(gtxt_richtext_skip_delimiter(end) + strlen("start="), &end);
	val->max = strtod(gtxt_richtext_skip_delimiter(end)+ strlen("max="), &end);
	val->min = strtod(gtxt_richtext_skip_delimiter(end) + strlen("min="), &end);
	val->glyph_dt = strtod(gtxt_richtext_skip_delimiter(end) + strlen("glyph_dt="), &end);
	val->time_dt = strtod(gtxt_richtext_skip_delimiter(end) + strlen("time_dt="), &end);
}

static inline void
_parser_dynamic(const char* token, struct dynamic_draw_style* s) {
	s->enable = true;

	s->alpha.start = s->alpha.max = s->alpha.min = 1;
	s->alpha.glyph_dt = s->alpha.time_dt = 0;

	s->scale.start = s->scale.max = s->scale.min = 1;
	s->scale.glyph_dt = s->scale.time_dt = 0;

	s->offset_x.start = s->offset_x.max = s->offset_x.min = 0;
	s->offset_x.glyph_dt = s->offset_x.time_dt = 0;

	s->offset_y.start = s->offset_y.max = s->offset_y.min = 0;
	s->offset_y.glyph_dt = s->offset_y.time_dt = 0;

	if (_str_head_equal(token, "dynamic=alpha")) {
		_parser_dynamic_value(&token[strlen("dynamic=alpha")], &s->alpha);
	} else if (_str_head_equal(token, "dynamic=scale")) {
		_parser_dynamic_value(&token[strlen("dynamic=scale")], &s->scale);
	} else if (_str_head_equal(token, "dynamic=offset_x")) {
		_parser_dynamic_value(&token[strlen("dynamic=offset_x")], &s->offset_x);
	} else if (_str_head_equal(token, "dynamic=offset_y")) {
		_parser_dynamic_value(&token[strlen("dynamic=offset_y")], &s->offset_y);
	}
}

static inline void
_parser_decoration(const char* token, struct gtxt_decoration* d) {
	const char* ptr = token;
	if (_str_head_equal(ptr, "overline")) {
		d->type = GRDT_OVERLINE;
		ptr += strlen("overline");
	} else if (_str_head_equal(ptr, "underline")) {
		d->type = GRDT_UNDERLINE;
		ptr += strlen("underline");
	} else if (_str_head_equal(ptr, "strikethrough")) {
		d->type = GRDT_STRIKETHROUGH;
		ptr += strlen("strikethrough");
	} else if (_str_head_equal(ptr, "border")) {
		d->type = GRDT_BORDER;
		ptr += strlen("border");
	} else if (_str_head_equal(ptr, "bg")) {
		d->type = GRDT_BG;
		ptr += strlen("bg");
	} else {
		return;
	}

	++ptr;
	const char* end = ptr;
	if (_str_head_equal(ptr, "color=")) {
		union gtxt_color col;
		if (_parser_color(&ptr[strlen("color=")], &col, &end)) {
			d->color = col.integer;
		} else {
			d->color = 0xffffffff;
		}
	} else {
		d->color = 0xffffffff;
	}
}

#define STATE_PUSH(buf, layer, val, ret) { \
	if ((layer) < MAX_LAYER_COUNT) { \
	(buf)[(layer)++] = (val); \
	(ret) = (val); \
	} else { \
	++(layer); \
	} \
}

#define STATE_POP(buf, layer, ret) { \
	--(layer); \
	assert((layer) >= 0); \
	if ((layer) <= MAX_LAYER_COUNT) { \
	(ret) = (buf)[(layer) - 1]; \
	} else { \
	(ret) = (buf)[MAX_LAYER_COUNT - 1]; \
	} \
}

static inline bool
_parser_token(const char* token, struct richtext_state* rs) {
	// font
	if (_str_head_equal(token, "font")) {
		int font = _parser_font(_skip_delimiter_and_equal(&token[strlen("font")]));
		if (font >= 0) {
			STATE_PUSH(rs->font, rs->font_layer, font, rs->s.gs.font);
			return true;
		} else {
			return false;
		}
	} else if (_str_head_equal(token, "/font")) {
		STATE_POP(rs->font, rs->font_layer, rs->s.gs.font);
		return true;
	}	
	// size
	else if (_str_head_equal(token, "size")) {
		int size = strtol(_skip_delimiter_and_equal(&token[strlen("size")]), (char**)NULL, 10);
		if (size >= MIN_FONT_SIZE && size <= MAX_FONT_SIZE) {
			STATE_PUSH(rs->size, rs->size_layer, size, rs->s.gs.font_size);
			return true;
		} else {
			return false;
		}
	} else if (_str_head_equal(token, "/size")) {
		STATE_POP(rs->size, rs->size_layer, rs->s.gs.font_size);
		return true;
	}
	// color
	else if (_str_head_equal(token, "color")) {
		union gtxt_color col;
		col.integer = 0xffffffff;
		bool succ = _parser_color(_skip_delimiter_and_equal(&token[strlen("color")]), &col, NULL);
		if (succ) {
			STATE_PUSH(rs->color, rs->color_layer, col, rs->s.gs.font_color);
			return true;
		} else {
			return false;
		}
	} else if (_str_head_equal(token, "/color")) {
		STATE_POP(rs->color, rs->color_layer, rs->s.gs.font_color);
		return true;
	}
	// edge
	else if (_str_head_equal(token, "edge")) {
		struct edge_style es;
		es.size = 1;
		es.color.integer = 0x000000ff;
		if (strlen(token) > strlen("edge")) {
			_parser_edge(_skip_delimiter_and_equal(&token[strlen("edge")]), &es);
		}
		if (rs->edge_layer < MAX_LAYER_COUNT) {
			rs->edge[rs->edge_layer++] = es;
			rs->s.gs.edge = true;
			rs->s.gs.edge_size = es.size;
			rs->s.gs.edge_color = es.color;
		} else {
			++rs->edge_layer;
		}
		return true;
	} else if (_str_head_equal(token, "/edge")) {
		--rs->edge_layer;
		assert(rs->edge_layer >= 0);
		if (rs->edge_layer == 0) {
			rs->s.gs.edge = false;
			rs->s.gs.edge_size = 0;
			rs->s.gs.edge_color.integer = 0;
		} else if (rs->edge_layer <= MAX_LAYER_COUNT) {
			rs->s.gs.edge = true;
			rs->s.gs.edge_size = rs->edge[rs->edge_layer-1].size;
			rs->s.gs.edge_color = rs->edge[rs->edge_layer-1].color;
		} else {
			rs->s.gs.edge = true;
			rs->s.gs.edge_size = rs->edge[MAX_LAYER_COUNT-1].size;
			rs->s.gs.edge_color = rs->edge[MAX_LAYER_COUNT-1].color;
		}
		return true;
	}
	// file
	else if (_str_head_equal(token, "file")) {
		assert(!rs->s.ext_sym_ud);
		rs->s.ext_sym_ud = EXT_SYM_CREATE(_skip_delimiter_and_equal(&token[strlen("file")]));
		return true;
	} else if (_str_head_equal(token, "/file")) {
		EXT_SYM_RELEASE(rs->s.ext_sym_ud);
		rs->s.ext_sym_ud = NULL;
		return true;
	}
	// dynamic
	else if (_str_head_equal(token, "dynamic")) {
		_parser_dynamic(token, &rs->dds);
		return true;
	} else if (_str_head_equal(token, "/dynamic")) {
		rs->dds.enable = false;
		return true;
	}
	// decoration
	else if (_str_head_equal(token, "decoration=")) {
		_parser_decoration(&token[strlen("decoration=")], &rs->s.ds.decoration);
		return true;
	} else if (_str_head_equal(token, "/decoration")) {
		rs->s.ds.decoration.type = GRDT_NULL;
		rs->s.ds.pos_type = GRPT_NULL;
		rs->s.ds.row_h = 0;
		return true;
	}
	// disable
	else if (_str_head_equal(token, "plain")) {
		rs->disable = true;
		rs->disable_num = strtol(gtxt_richtext_skip_delimiter(&token[strlen("plain")]), (char**)NULL, 10);
		return true;
	}
	return false;
}

static inline void
_init_state(struct richtext_state* rs, const struct gtxt_label_style* style) {
	rs->font[0] = style->gs.font;
	rs->font_layer = 1;
	rs->size[0] = style->gs.font_size;
	rs->size_layer = 1;
	rs->color[0] = style->gs.font_color;
	rs->color_layer = 1;

	if (style->gs.edge) {
		struct edge_style es;
		es.size = style->gs.edge_size;
		es.color = style->gs.edge_color;
		rs->edge[0] = es;
		rs->edge_layer = 1;
	} else {
		rs->edge_layer = 0;
	}

	rs->s.gs = style->gs;

	rs->s.ds.alpha = 1;
	rs->s.ds.scale = 1;
	rs->s.ds.offset_x = rs->s.ds.offset_y = 0;
	rs->s.ds.decoration.type = GRDT_NULL;
	rs->s.ds.row_y = rs->s.ds.row_h = 0;
	rs->s.ds.pos_type = GRPT_NULL;

	rs->s.ext_sym_ud = NULL;

	rs->dds.enable = false;

	rs->disable = false;
}

static int
_read_token(const char* str, int ptr, int len, struct richtext_state* rs) {
	assert(str[ptr]);
	int curr = ptr;
	while (str[curr] != '>' && curr < len) {
		++curr;
	}
	if (str[curr] == '>') {
		char token[curr - ptr];
		strncpy(token, &str[ptr + 1], curr - ptr - 1);
		token[curr - ptr - 1] = 0;
		bool succ = _parser_token(token, rs);
		if (succ) {
			curr = curr + 1;			
		} else {
			curr = ptr;
		}
	} else {
		assert(curr == len);
		curr = ptr;
	}
	return curr;
}

static inline bool
_parser_plain_end(const char* str, int len, int* ptr, int disable_num) {
	int begin = *ptr + strlen("</plain");
	int end = begin;
	while (str[end] != '>' && end < len) {
		++end;
	}
	if (str[end] == '>') {
		int num = strtol(&str[begin], (char**)NULL, 10);
		if (num == disable_num) {
			*ptr = end + 1;
			return true;
		} else {
			return false;
		}
	} else {
		return false;
	}
}

void 
gtxt_richtext_parser(const char* str, const struct gtxt_label_style* style, 
					 int (*cb)(const char* str, struct gtxt_richtext_style* style, void* ud), void* ud) {
	struct richtext_state rs;
	_init_state(&rs, style);

	int len = (str==NULL)?(0):(strlen(str));
	bool token_disable = false;
	for (int i = 0; i < len; ) {
		if (!token_disable && _str_head_equal(&str[i], "</plain")) {
			if (_parser_plain_end(str, len, &i, rs.disable_num)) {
				rs.disable = false;
			} else {
				token_disable = true;
			}
		} else if (!token_disable && str[i] == '<' && !rs.disable) {
			int ptr = _read_token(str, i, len, &rs);
			if (ptr == i) {
				token_disable = true;
			} else {
				i = ptr;
			}
		} else {
			if (token_disable) {
				token_disable = false;
			}
			if (str[i] == '\\') {
				token_disable = true;
			}
			int n = cb(&str[i], &rs.s, ud);
			if (n < 0) {
				break;
			}
			i += n;
		}
	}
}

static inline float
_cal_dynamic_val(struct dynamic_value* val, int time, int glyph) {
	float ret = val->start + val->time_dt * time + val->glyph_dt * glyph;
	return MIN(val->max, MAX(val->min, ret));
}

void 
gtxt_richtext_parser_dynamic(const char* str, const struct gtxt_label_style* style, int time,
							 int (*cb)(const char* str, struct gtxt_richtext_style* style, void* ud), void* ud) {
	struct richtext_state rs;
	_init_state(&rs, style);

	int len = strlen(str);
	int glyph = 0;
	bool token_disable = false;
	for (int i = 0; i < len; ) {
		if (!token_disable && _str_head_equal(&str[i], "</plain")) {
			if (_parser_plain_end(str, len, &i, rs.disable_num)) {
				rs.disable = false;
			} else {
				token_disable = true;
			}
		} else if (!token_disable && str[i] == '<' && !rs.disable) {
			int ptr = _read_token(str, i, len, &rs);
			if (ptr == i) {
				token_disable = true;
			} else {
				i = ptr;
			}
		} else {
			if (token_disable) {
				token_disable = false;
			}
			if (str[i] == '\\') {
				token_disable = true;
			}
			if (rs.dds.enable) {
				rs.s.ds.alpha = _cal_dynamic_val(&rs.dds.alpha, time, glyph);
				rs.s.ds.scale = _cal_dynamic_val(&rs.dds.scale, time, glyph);
				rs.s.ds.offset_x = _cal_dynamic_val(&rs.dds.offset_x, time, glyph);
				rs.s.ds.offset_y = _cal_dynamic_val(&rs.dds.offset_y, time, glyph);
				++glyph;
			} else {
				rs.s.ds.alpha = 1;
				rs.s.ds.scale = 1;
				rs.s.ds.offset_x = rs.s.ds.offset_y = 0;
				glyph = 0;
			}
			int n = cb(&str[i], &rs.s, ud);
			if (n < 0) {
				break;
			}
			i += n;
		}
	}
}

int 
gtxt_richtext_get_delimiter(const char* str) {
	if (str[0] == ' ') {
		return 1;
	} else if (strncmp(str, "\xe3\x80\x80", 3) == 0) {
		return 3;
	} else if (strncmp(str, "\xc2\xa0", 2) == 0) {
		return 2;
	} else {
		return 0;
	}
}

const char* 
gtxt_richtext_skip_delimiter(const char* str) {
	const char* ptr = str;
	int len;
	do {
		len = gtxt_richtext_get_delimiter(ptr);
		ptr += len;
	} while (len != 0);
	return ptr;
}