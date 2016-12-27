#include "gtxt_adapter.h"
#include "gtxt_glyph.h"
#include "gtxt_freetype.h"
#include "gtxt_user_font.h"

#include <dtex_cg.h>

#include <assert.h>
#include <stddef.h>

static struct dtex_cg* CG = NULL;

void 
gtxt_adapter_create(struct dtex_cg* cg) {
	CG = cg;
}

void 
gtxt_adapter_release() {
	CG = NULL;
}

static inline void
_init_glyph(int unicode, const struct gtxt_glyph_style* style, struct dtex_glyph* glyph) {
	glyph->unicode		= unicode;
	glyph->s.font		= style->font;
	glyph->s.font_size	= style->font_size;
	glyph->s.font_color	= style->font_color.integer;
	glyph->s.edge		= style->edge;
	glyph->s.edge_size	= style->edge_size;
	glyph->s.edge_color	= style->edge_color.integer;
}

static void
_load_ft_char(int unicode, const struct gtxt_glyph_style* gs, struct dtex_glyph* glyph) {
	struct gtxt_glyph_layout layout;
	uint32_t* buf = gtxt_glyph_get_bitmap(unicode, gs, &layout);
	if (buf) {
		dtex_cg_load_bmp(CG, buf, layout.sizer.width, layout.sizer.height, glyph);
	}
}

void
gtxt_draw_glyph(int unicode, float x, float y, float w, float h, 
                const struct gtxt_glyph_style* gs, struct gtxt_draw_style* ds, 
				void (*render)(int id, float* texcoords, float x, float y, float w, float h, struct gtxt_draw_style* ds, void* ud), void* ud) {
	if (unicode == ' ' || unicode == 160 || unicode == '\n') {
		return;
	}

	struct dtex_glyph g;
	_init_glyph(unicode, gs, &g);

	int uid = 0;
	float* texcoords = dtex_cg_query(CG, &g, &uid);
	if (!texcoords) {
		int ft_count = gtxt_ft_get_font_cout();
		if (gs->font < ft_count) {
			_load_ft_char(unicode, gs, &g);
		} else {
			int uf_font = gs->font - ft_count;
			texcoords = gtxt_uf_query_and_load(uf_font, unicode, &g);			
		}
		if (!texcoords) {
			return;
		}
	}

	render(uid, texcoords, x, y, w, h, ds, ud);
}

void 
gtxt_reload_glyph(int unicode, const struct gtxt_glyph_style* style) {
	if (unicode == ' ' || unicode == 160 || unicode == '\n') {
		return;
	}

	struct dtex_glyph g;
	_init_glyph(unicode, style, &g);

	int uid = 0;
	float* texcoords = dtex_cg_query(CG, &g, &uid);
	assert(texcoords);

	struct gtxt_glyph_layout layout;
	uint32_t* buf = gtxt_glyph_get_bitmap(unicode, style, &layout);
	dtex_cg_reload(CG, buf, layout.sizer.width, layout.sizer.height, texcoords);
}