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

struct gtxt_glyph_style {
	int font;
	int font_size;
	union gtxt_color font_color;

	bool edge;
	float edge_size;
	union gtxt_color edge_color;
};

void gtxt_glyph_create(int cap_bitmap, int cap_layout, uint32_t* (*char_gen)(const char* str, struct gtxt_glyph_style* style, struct gtxt_glyph_layout* layout));
void gtxt_glyph_release();

struct gtxt_glyph_layout* gtxt_glyph_get_layout(int unicode, struct gtxt_glyph_style*);

uint32_t* gtxt_glyph_get_bitmap(int unicode, struct gtxt_glyph_style*, struct gtxt_glyph_layout* layout);

#endif // gametext_glyph_h

#ifdef __cplusplus
}
#endif