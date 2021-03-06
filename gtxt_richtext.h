#ifdef __cplusplus
extern "C"
{
#endif

#ifndef gametext_richtext_h
#define gametext_richtext_h

#include "gtxt_typedef.h"
#include "gtxt_glyph.h"

#include <stdint.h>
#include <stdbool.h>

struct gtxt_label_style;

enum GR_DECORATION_TYPE {
	GRDT_NULL = 0,
	GRDT_OVERLINE,
	GRDT_UNDERLINE,
	GRDT_STRIKETHROUGH,
	GRDT_BORDER,
	GRDT_BG,
};

struct gtxt_decoration {
	int type;
	uint32_t color;
};

enum GR_POS_TYPE {
	GRPT_NULL = 0,
	GRPT_BEGIN,
	GRPT_MIDDLE,
	GRPT_END,
};

struct gtxt_draw_style {
	float alpha;
	float scale;
	float offset_x, offset_y;

	struct gtxt_decoration decoration;
	float row_y, row_h;
	int pos_type;
};

struct gtxt_richtext_style {
	struct gtxt_glyph_style gs;
	struct gtxt_draw_style ds;
	void* ext_sym_ud;
};

void gtxt_richtext_release();

void gtxt_richtext_add_color(const char* key, unsigned int val);

void gtxt_richtext_add_font(const char* name);

void gtxt_richtext_ext_sym_cb_init(void* (*create)(const char* str),
								   void (*release)(void* ext_sym),
								   void (*size)(void* ext_sym, int* width, int* height),
								   void (*render)(void* ext_sym, float x, float y, void* ud),
								   bool (*query)(void* ext_sym, float x, float y, float w, float h, int qx, int qy, void* ud));
void gtxt_ext_sym_get_size(void* ext_sym, int* width, int* height);
void gtxt_ext_sym_render(void* ext_sym, float x, float y, void* ud);
bool gtxt_ext_sym_query(void* ext_sym, float x, float y, float w, float h, int qx, int qy, void* ud);

void gtxt_richtext_parser(const char* str, const struct gtxt_label_style* style,
						  int (*cb)(const char* str, float line_x, struct gtxt_richtext_style* style, void* ud), void* ud);

void gtxt_richtext_parser_dynamic(const char* str, const struct gtxt_label_style* style, int time,
								  int (*cb)(const char* str, struct gtxt_richtext_style* style, void* ud), void* ud);

int gtxt_richtext_get_delimiter(const char* str);
const char* gtxt_richtext_skip_delimiter(const char* str);

#endif // gametext_richtext_h

#ifdef __cplusplus
}
#endif