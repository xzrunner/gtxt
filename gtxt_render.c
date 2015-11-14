#include "gtxt_render.h"
#include "gtxt_glyph.h"

#include <dtex_cg.h>
#include <dtex_ej_glyph.h>

static struct dtex_cg* CG;

void 
gtxt_render_init(struct dtex_cg* cg) {
	CG = cg;
}

void
gtxt_draw_glyph(int unicode, struct gtxt_glyph_style* style, float x, float y, float w, float h,
				void (*render)(int id, float* texcoords, float x, float y, float w, float h, void* ud), void* ud) {
	struct dtex_glyph g;
	g.unicode		= unicode;
	g.s.font		= style->font;
	g.s.font_size	= style->font_size;
	g.s.font_color	= style->font_color.integer;
	g.s.edge		= style->edge;
	g.s.edge_size	= style->edge_size;
	g.s.edge_color	= style->edge_color.integer;

	int uid = 0;
	float* texcoords = dtex_cg_query(CG, &g, &uid);
	if (!texcoords) {
		struct gtxt_glyph_layout layout;
		uint32_t* buf = gtxt_glyph_get_bitmap(unicode, style, &layout);
		dtex_cg_load(CG, buf, layout.sizer.width, layout.sizer.height, &g);
		return;
	}

	render(uid, texcoords, x, y, w, h, ud);
}
