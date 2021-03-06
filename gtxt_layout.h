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
	GLOS_FULL,
	GLOS_CONNECTION
};

void gtxt_layout_release();

void gtxt_layout_begin(const struct gtxt_label_style* style);

enum GLO_STATUS gtxt_layout_single(int unicode, float line_x, struct gtxt_richtext_style* style);
void gtxt_layout_multi(struct ds_array* unicodes);

int gtxt_layout_add_omit_sym(const struct gtxt_glyph_style* gs);

enum GLO_STATUS gtxt_layout_ext_sym(int width, int height);

void gtxt_layout_traverse(void (*cb)(int unicode, float x, float y, float w, float h, float row_y, float start_x, void* ud), void* ud);

void gtxt_layout_end();

void gtxt_get_layout_size(float* width, float* height);

void gtxt_layout_enable_hori_offset(bool enable);

#endif // gametext_layout_h

#ifdef __cplusplus
}
#endif