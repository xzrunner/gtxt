#ifdef __cplusplus
extern "C"
{
#endif

#ifndef gametext_freetype_h
#define gametext_freetype_h

#include "gtxt_typedef.h"

#include <stdbool.h>
#include <stdint.h>

struct gtxt_glyph_layout;
struct gtxt_glyph_style;

void gtxt_ft_create();
void gtxt_ft_release();

int gtxt_ft_add_font(const char* name, const char* filepath);

int gtxt_ft_get_font_cout();

void gtxt_ft_get_layout(int unicode, const struct gtxt_glyph_style*, struct gtxt_glyph_layout*);
uint32_t* gtxt_ft_gen_char(int unicode, const struct gtxt_glyph_style*, struct gtxt_glyph_layout*);

#endif // gametext_freetype_h

#ifdef __cplusplus
}
#endif