#ifdef __cplusplus
extern "C"
{
#endif

#ifndef gametext_glyph_h
#define gametext_glyph_h

#include "gtxt_typedef.h"

#include <stdbool.h>

struct gtxt_glyph_sizer {
	float width;
	float height;
};

struct gtxt_glyph_layout {
	struct gtxt_glyph_sizer sizer;
	float bearing_x, bearing_y;
	float advance;
	float metrics_height;
};

struct gtxt_glyph_color {
	union {
		struct {
			union gtxt_color color;
		} ONE;

		struct {
			union gtxt_color begin_col, end_col;
			float begin_pos, end_pos;
			float angle;
		} TWO;

		struct {
			union gtxt_color begin_col, mid_col, end_col;
			float begin_pos, mid_pos, end_pos;
			float angle;
		} THREE;
	} mode;

	int mode_type;
};

struct gtxt_glyph_style {
	int font;
	int font_size;
	struct gtxt_glyph_color font_color;

	bool edge;
	float edge_size;
	struct gtxt_glyph_color edge_color;
};

void gtxt_glyph_create(int cap_bitmap, int cap_layout,
					   uint32_t* (*char_gen)(const char* str, const struct gtxt_glyph_style* style, struct gtxt_glyph_layout* layout),
					   void (*get_uf_layout)(int unicode, int font, struct gtxt_glyph_layout* layout));
void gtxt_glyph_release();

struct gtxt_glyph_layout* gtxt_glyph_get_layout(int unicode, float line_x, const struct gtxt_glyph_style*);

uint32_t* gtxt_glyph_get_bitmap(int unicode, float line_x, const struct gtxt_glyph_style*, struct gtxt_glyph_layout* layout);

#endif // gametext_glyph_h

#ifdef __cplusplus
}
#endif