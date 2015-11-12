#ifdef __cplusplus
extern "C"
{
#endif

#ifndef gametext_layout_h
#define gametext_layout_h

#include <stdbool.h>

void gtxt_layout_begin(struct gtxt_label_style* style);

bool gtxt_layout_single(int unicode, struct gtxt_richtext_style* style);
void gtxt_layout_multi(struct dtex_array* unicodes);

bool gtxt_layout_ext_sym(int width, int height);

void gtxt_layout_traverse(void (*cb)(int unicode, float x, float y, float w, float h, void* ud), void* ud);

void gtxt_layout_end();

#endif // gametext_layout_h

#ifdef __cplusplus
}
#endif