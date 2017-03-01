#ifdef __cplusplus
extern "C"
{
#endif

#ifndef gametext_user_font_h
#define gametext_user_font_h

#include <stdint.h>

struct gtxt_glyph_layout;
struct dtex_glyph;

void gtxt_uf_cb_init(float* (*load_and_query)(void* ud, struct dtex_glyph* glyph));

void gtxt_uf_create();
void gtxt_uf_release();

int gtxt_uf_add_font(const char* name, int cap);
void gtxt_uf_add_char(int font, const char* str, int w, int h, void* ud);

float* gtxt_uf_query_and_load(int font, int unicode, struct dtex_glyph* glyph);

void gtxt_uf_get_layout(int unicode, int font, struct gtxt_glyph_layout*);

#endif // gametext_user_font_h

#ifdef __cplusplus
}
#endif