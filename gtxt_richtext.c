#include "gtxt_richtext.h"
#include "gtxt_label.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>

#define MAX_LAYER	16
#define MAX_FONT	16

struct edge_style {
	float size;
	union gtxt_color color;
};

struct richtext_state {
	int font[MAX_LAYER];
	int font_layer;

	int size[MAX_LAYER];
	int size_layer;

	union gtxt_color color[MAX_LAYER];
	int color_layer;

	struct edge_style edge[MAX_LAYER];
	int edge_layer;

	struct gtxt_richtext_style style;
};

static char FONTS[MAX_FONT][128];
static int FONT_SIZE = 0;

static void* (*EXT_SYM_CREATE)(const char* str);
static void (*EXT_SYM_RELEASE)(void* ext_sym);
static void (*EXT_SYM_SIZE)(void* ext_sym, int* width, int* height);
static void (*EXT_SYM_RENDER)(void* ext_sym, float x, float y, void* ud);

struct color_map {
	char* name;
	union gtxt_color color;
};

static const struct color_map COLOR[] = {
	{ "aqua",		0x00ffffff },
	{ "black",		0x000000ff },
	{ "blue",		0x0000ffff },
	{ "brown",		0xa52a2aff },
	{ "cyan",		0x00ffffff },
	{ "darkblue",	0x0000a0ff },
	{ "fuchsia",	0xff00ffff },
	{ "green",		0x008000ff },
	{ "grey",		0x808080ff },
	{ "lightblue",	0xadd8e6ff },
	{ "lime",		0x00ff00ff },
	{ "magenta",	0xff00ffff },
	{ "maroon",		0x800000ff },
	{ "navy",		0x000080ff },
	{ "olive",		0x808000ff },
	{ "orange",		0xffa500ff },
	{ "purple",		0x800080ff },
	{ "red",		0xff0000ff },
	{ "silver",		0xc0c0c0ff },
	{ "teal",		0x008080ff },
	{ "white",		0xffffffff },
	{ "yellow",		0xffff00ff }
};

static inline union gtxt_color
_parser_color(const char* token, char** end_ptr) {
	union gtxt_color col;
	col.integer = 0;
	if (token[0] == '#') {
		col.integer = strtoul(&token[1], end_ptr, 16);
	} else {
		int num = sizeof(COLOR) / sizeof(struct color_map);
		for (int i = 0; i < num; ++i) {
			if (strncmp(&token[0], COLOR[i].name, strlen(COLOR[i].name)) == 0) {
				col = COLOR[i].color;
				if (end_ptr) {
					*end_ptr = &token[strlen(COLOR[i].name)];
				}
				break;
			}
		}
	}
	return col;
}

void 
gtxt_richtext_add_font(const char* name) {
	strcpy(&FONTS[FONT_SIZE][0], name);
	FONTS[FONT_SIZE][strlen(name) + 1] = 0;
	++FONT_SIZE;
}

void 
gtxt_richtext_ext_sym_cb_init(void* (*create)(const char* str),
							  void (*release)(void* ext_sym),
							  void (*size)(void* ext_sym, int* width, int* height), 
							  void (*render)(void* ext_sym, float x, float y, void* ud)) {
	EXT_SYM_CREATE = create;
	EXT_SYM_RELEASE = release;
	EXT_SYM_SIZE = size;
	EXT_SYM_RENDER = render;
}

void 
gtxt_ext_sym_get_size(void* ext_sym, int* width, int* height) {
	EXT_SYM_SIZE(ext_sym, width, height);
}

void 
gtxt_ext_sym_render(void* ext_sym, float x, float y, void* ud) {
	EXT_SYM_RENDER(ext_sym, x, y, ud);
}

static inline int
_parser_font(const char* token) {
	for (int i = 0; i < MAX_FONT; ++i) {
		if (strcmp(FONTS[i], token) == 0) {
			return i;
		}
	}
	return 0;
}

static inline void 
_parser_edge(const char* token, struct edge_style* es) {
	char* end;
	if (strncmp(token, "size=", strlen("size=")) == 0) {
		es->size = strtod(&token[strlen("size=")], &end );
	} else if (strncmp(token, "color=", strlen("color=")) == 0) {
		es->color = _parser_color(&token[strlen("color=")], &end);
	}
	if (*end) {
		_parser_edge(end + 1, es);
	}
}

#define STATE_PUSH(buf, layer, val, ret) { \
	if ((layer) < MAX_LAYER) { \
	(buf)[(layer)++] = (val); \
	(ret) = (val); \
	} else { \
	++(layer); \
	} \
}

#define STATE_POP(buf, layer, ret) { \
	--(layer); \
	assert((layer) >= 0); \
	if ((layer) <= MAX_LAYER) { \
	(ret) = (buf)[(layer) - 1]; \
	} else { \
	(ret) = (buf)[MAX_LAYER - 1]; \
	} \
}

static inline void
_parser_token(const char* token, struct richtext_state* rs) {
	// font
	if (strncmp(token, "font", strlen("font")) == 0) {
		int font = _parser_font(&token[strlen("font")+1]);
		STATE_PUSH(rs->font, rs->font_layer, font, rs->style.gs.font)
	} else if (strncmp(token, "/font", strlen("/font")) == 0) {
		STATE_POP(rs->font, rs->font_layer, rs->style.gs.font);		
	}	
	// size
	else if (strncmp(token, "size", strlen("size")) == 0) {
		int size = strtol(&token[strlen("size")+1], (char**)NULL, 10);
		STATE_PUSH(rs->size, rs->size_layer, size, rs->style.gs.font_size)
	} else if (strncmp(token, "/size", strlen("/size")) == 0) {
		STATE_POP(rs->size, rs->size_layer, rs->style.gs.font_size);				
	}
	// color
	else if (strncmp(token, "color", strlen("color")) == 0) {
		union gtxt_color col = _parser_color(&token[strlen("color")+1], NULL);
		STATE_PUSH(rs->color, rs->color_layer, col, rs->style.gs.font_color)
	} else if (strncmp(token, "/color", strlen("/color")) == 0) {
		STATE_POP(rs->color, rs->color_layer, rs->style.gs.font_color);
	}
	// edge
	else if (strncmp(token, "edge", strlen("edge")) == 0) {
		struct edge_style es;
		es.size = 1;
		es.color.integer = 0x000000ff;
		if (strlen(token) > strlen("edge")) {
			_parser_edge(&token[strlen("edge")+1], &es);
		}
		if (rs->edge_layer < MAX_LAYER) {
			rs->edge[rs->edge_layer++] = es;
			rs->style.gs.edge = true;
			rs->style.gs.edge_size = es.size;
			rs->style.gs.edge_color = es.color;
		} else {
			++rs->edge_layer;
		}
	} else if (strncmp(token, "/edge", strlen("/edge")) == 0) {
		--rs->edge_layer;
		assert(rs->edge_layer >= 0);
		if (rs->edge_layer == 0) {
			rs->style.gs.edge = false;
			rs->style.gs.edge_size = 0;
			rs->style.gs.edge_color.integer = 0;
		} else if (rs->edge_layer <= MAX_LAYER) {
			rs->style.gs.edge = true;
			rs->style.gs.edge_size = rs->edge[rs->edge_layer-1].size;
			rs->style.gs.edge_color = rs->edge[rs->edge_layer-1].color;
		} else {
			rs->style.gs.edge = true;
			rs->style.gs.edge_size = rs->edge[MAX_LAYER-1].size;
			rs->style.gs.edge_color = rs->edge[MAX_LAYER-1].color;
		}
	}
	// file
	else if (strncmp(token, "file", strlen("file")) == 0) {
		assert(!rs->style.ext_sym_ud);
		rs->style.ext_sym_ud = EXT_SYM_CREATE(&token[strlen("file")+1]);
	} else if (strncmp(token, "/file", strlen("/file")) == 0) {
		EXT_SYM_RELEASE(rs->style.ext_sym_ud);
		rs->style.ext_sym_ud = NULL;
	}
}

static inline void
_init_state(struct richtext_state* rs, struct gtxt_label_style* style) {
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

	rs->style.gs = style->gs;
	rs->style.ext_sym_ud = NULL;
}

void 
gtxt_richtext_parser(const char* str, struct gtxt_label_style* style, 
					 int (*cb)(const char* str, struct gtxt_richtext_style* style, void* ud), void* ud) {
	struct richtext_state rs;
	_init_state(&rs, style);

	int len = strlen(str);
	for (int i = 0; i < len; ) {
		if (str[i] == '<') {
			int j = i;
			while (str[j] != '>' && j < len) {
				++j;
			}
			if (str[j] == '>') {
				char token[j - i];
				strncpy(token, &str[i + 1], j - i - 1);
				token[j - i - 1] = 0;
				_parser_token(token, &rs);
				i = j + 1;
			} else {
				assert(j == len);
				return;
			}
		} else {
			int n = cb(&str[i], &rs.style, ud);
			i += n;
		}
	}
}