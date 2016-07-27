#ifdef __cplusplus
extern "C"
{
#endif

#ifndef gametext_layout_h
#define gametext_layout_h

#include <stdbool.h>

struct gtxt_label_style;
struct gtxt_richtext_style;
struct gtxt_glyph_style;
struct ds_array;

enum GLO_STATUS
{
	GLOS_NORMAL = 0,
	GLOS_NEWLINE,
	GLOS_FULL
};

void gtxt_layout_release();

void gtxt_layout_begin(const struct gtxt_label_style* style);

enum GLO_STATUS gtxt_layout_single(int unicode, struct gtxt_richtext_style* style);
void gtxt_layout_multi(struct ds_array* unicodes);

int gtxt_layout_add_omit_sym(const struct gtxt_glyph_style* gs);

bool gtxt_layout_ext_sym(int width, int height);

void gtxt_layout_traverse(void (*cb)(int unicode, float x, float y, float w, float h, void* ud), void* ud);
void gtxt_layout_traverse2(void (*cb)(int unicode, float x, float y, float w, float h, float row_y, void* ud), void* ud);

void gtxt_layout_end();

void gtxt_get_layout_size(float* width, float* height);

#endif // gametext_layout_h

#ifdef __cplusplus
}
#endif