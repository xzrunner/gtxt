#include "gtxt_adapter.h"
#include "gtxt_glyph.h"

#include <dtex_cg.h>

#include <assert.h>

static struct dtex_cg* CG;

void 
gtxt_adapter_init(struct dtex_cg* cg) {
	CG = cg;
}

static inline void
_init_glyph(int unicode, struct gtxt_glyph_style* style, struct dtex_glyph* glyph) {
	glyph->unicode		= unicode;
	glyph->s.font		= style->font;
	glyph->s.font_size	= style->font_size;
	glyph->s.font_color	= style->font_color.integer;
	glyph->s.edge		= style->edge;
	glyph->s.edge_size	= style->edge_size;
	glyph->s.edge_color	= style->edge_color.integer;
}

void
gtxt_draw_glyph(int unicode, float x, float y, float w, float h, 
                struct gtxt_glyph_style* gs, struct gtxt_draw_style* ds, 
				void (*render)(int id, float* texcoords, float x, float y, float w, float h, struct gtxt_draw_style* ds, void* ud), void* ud) {
	if (unicode == ' ' || unicode == '\n') {
		return;
	}

	struct dtex_glyph g;
	_init_glyph(unicode, gs, &g);

	int uid = 0;
	float* texcoords = dtex_cg_query(CG, &g, &uid);
	if (!texcoords) {
		struct gtxt_glyph_layout layout;
		uint32_t* buf = gtxt_glyph_get_bitmap(unicode, gs, &layout);
		texcoords = dtex_cg_load(CG, buf, layout.sizer.width, layout.sizer.height, &g);
		if (!texcoords) {
			return;
		}
	}

	render(uid, texcoords, x, y, w, h, ds, ud);
}

void 
gtxt_reload_glyph(int unicode, struct gtxt_glyph_style* style) {
	if (unicode == ' ' || unicode == '\n') {
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