#ifdef __cplusplus
extern "C"
{
#endif

#ifndef gametext_rendrer_h
#define gametext_rendrer_h

#include "gtxt_typedef.h"

#include <stdbool.h>
#include <stdint.h>

void gtxt_render_init(struct dtex_cg* cg);

void gtxt_draw_glyph(int unicode, struct gtxt_glyph_style* style, float x, float y, float w, float h,
					 void (*render)(int id, float* texcoords, float x, float y, float w, float h, void* ud), void* ud);

#endif // gametext_rendrer_h

#ifdef __cplusplus
}
#endif