#ifdef __cplusplus
extern "C"
{
#endif

#ifndef gametext_label_h
#define gametext_label_h

#include "gtxt_typedef.h"
#include "gtxt_glyph.h"

#include <stdbool.h>

enum gtxt_hori_align {
	HA_LEFT		= 0,
	HA_RIGHT	= 1,
	HA_CENTER	= 2,
	HA_AUTO		= 3
};

enum gtxt_vert_align {
	VA_TOP		= 0,
	VA_BOTTOM	= 1,
	VA_CENTER	= 2,
	VA_AUTO		= 3
};

struct gtxt_label_style {
	int width;
	int height;

	int align_h;
	int align_v;
	
	float space_h;
	float space_v;

	struct gtxt_glyph_style gs;
};

struct gtxt_draw_style;

void gtxt_label_draw(const char* str, struct gtxt_label_style* style, void (*render)(int id, float* texcoords, float x, float y, float w, float h, struct gtxt_draw_style* ds, void* ud), void* ud);
void gtxt_label_draw_richtext(const char* str, struct gtxt_label_style* style, int time, void (*render)(int id, float* texcoords, float x, float y, float w, float h, struct gtxt_draw_style* ds, void* ud), void* ud);

void gtxt_label_reload(const char* str, struct gtxt_label_style* style);
void gtxt_label_reload_richtext(const char* str, struct gtxt_label_style* style);

void* gtxt_label_point_query(const char* str, struct gtxt_label_style* style, int x, int y, void* ud);

void gtxt_get_label_size(const char* str, struct gtxt_label_style* style, float* width, float* height);

#endif // gametext_label_h

#ifdef __cplusplus
}
#endif