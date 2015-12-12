#ifdef __cplusplus
extern "C"
{
#endif

#ifndef gametext_adapter_h
#define gametext_adapter_h

struct dtex_cg;
struct gtxt_glyph_style;
struct gtxt_draw_style;

void gtxt_adapter_init(struct dtex_cg* cg);

void gtxt_draw_glyph(int unicode, float x, float y, float w, float h,
                     struct gtxt_glyph_style* gs, struct gtxt_draw_style* ds, 
					 void (*render)(int id, float* texcoords, float x, float y, float w, float h, struct gtxt_draw_style* ds, void* ud), void* ud);

void gtxt_reload_glyph(int unicode, struct gtxt_glyph_style* style);

#endif // gametext_adapter_h

#ifdef __cplusplus
}
#endif