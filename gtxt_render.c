#include "gtxt_render.h"
#include "gtxt_glyph.h"

#include "gtxt_ejoy2d.h"

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

//#ifndef USED_IN_EDITOR
//	struct ej_vertex_pack vb[4];
//	for (int i = 0; i < 4; ++i) {
//		vb[i].tx = texcoords[i*2] * 0xffff;
//		vb[i].ty = texcoords[i*2+1] * 0xffff;
//	}
//	ej_shader_texture(uid, 0);
//	ej_shader_draw(vb, 0xffffffff, 0, 0xff0000ff, 0x00ff00ff, 0x0000ffff);
//#else
//	dtex_ej_glyph_draw(uid, texcoords, x, y, 50, 50);
//#endif // USED_IN_EDITOR

	render(uid, texcoords, x, y, w, h, ud);
}
