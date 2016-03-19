#ifdef __cplusplus
extern "C"
{
#endif

#ifndef gametext_layout_h
#define gametext_layout_h

#include <stdbool.h>

struct gtxt_label_style;
struct gtxt_richtext_style;
struct ds_array;

void gtxt_layout_release();

void gtxt_layout_begin(const struct gtxt_label_style* style);

bool gtxt_layout_single(int unicode, struct gtxt_richtext_style* style);
void gtxt_layout_multi(struct ds_array* unicodes);

bool gtxt_layout_ext_sym(int width, int height);

void gtxt_layout_traverse(void (*cb)(int unicode, float x, float y, float w, float h, void* ud), void* ud);
void gtxt_layout_traverse2(void (*cb)(int unicode, float x, float y, float w, float h, float row_y, void* ud), void* ud);

void gtxt_layout_end();

void gtxt_get_layout_size(float* width, float* height);

#endif // gametext_layout_h

#ifdef __cplusplus
}
#endif