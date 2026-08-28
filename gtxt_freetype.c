#include "gtxt_freetype.h"
#include "gtxt_glyph.h"
#include "gtxt_richtext.h"

#include <fs_file.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_IMAGE_H
#include FT_STROKER_H

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct font {
	FT_Library library;
	FT_Face face;
	unsigned char* buf;
	char* filepath;
};

#define MAX_FONTS 8

//#define PREMULTIPLY_APLHA

struct freetype {
	struct font	fonts[MAX_FONTS];
	int count;
};

static struct freetype* FT;

struct span {
	int x, y;
	int width;
	int coverage;
};

#define MAX_SPAN 2048

struct spans {
	struct span items[MAX_SPAN];
	int sz;
	int overflow;
};

static struct spans* IN_SPANS = NULL;
static struct spans* OUT_SPANS = NULL;

static union gtxt_color* BUF;
static size_t BUF_SZ;
static int BUF_VALID;

static int SOLID_WHITE = 0;

static void
_font_cleanup(FT_Library library, FT_Face face, unsigned char* buf, struct fs_file* file) {
	if (file) {
		fs_close(file);
	}
	if (face) {
		FT_Done_Face(face);
	}
	if (library) {
		FT_Done_FreeType(library);
	}
	free(buf);
}

static void
_font_dispose(struct font* f) {
	if (!f) {
		return;
	}
	if (f->face) {
		FT_Done_Face(f->face);
	}
	if (f->library) {
		FT_Done_FreeType(f->library);
	}
	free(f->buf);
	free(f->filepath);
	memset(f, 0, sizeof(*f));
}

int
gtxt_ft_ready(void) {
	return FT && IN_SPANS && OUT_SPANS ? 1 : 0;
}

void
gtxt_ft_create(void) {
	if (gtxt_ft_ready()) {
		return;
	}
	if (FT || IN_SPANS || OUT_SPANS) {
		gtxt_ft_release();
	}

	struct freetype* next_ft = (struct freetype*)calloc(1, sizeof(*next_ft));
	struct spans* next_in = (struct spans*)calloc(1, sizeof(*next_in));
	struct spans* next_out = (struct spans*)calloc(1, sizeof(*next_out));
	if (!next_ft || !next_in || !next_out) {
		free(next_ft);
		free(next_in);
		free(next_out);
		return;
	}

	FT = next_ft;
	IN_SPANS = next_in;
	OUT_SPANS = next_out;
}

void
gtxt_ft_release(void) {
	if (FT) {
		for (int i = 0; i < MAX_FONTS; ++i) {
			_font_dispose(&FT->fonts[i]);
		}
		free(FT);
		FT = NULL;
	}
	free(IN_SPANS); IN_SPANS = NULL;
	free(OUT_SPANS); OUT_SPANS = NULL;
	free(BUF); BUF = NULL;
	BUF_SZ = 0;
	BUF_VALID = 0;
	SOLID_WHITE = 0;
	gtxt_richtext_reset_fonts();
}

static int
_ft_bind_file(struct font* f, const char* filepath) {
	FT_Library library = NULL;
	FT_Face face = NULL;
	unsigned char* buf = NULL;
	char* filepath_copy = NULL;
	struct fs_file* file = NULL;
	if (!f || !filepath || filepath[0] == '\0') {
		return 0;
	}

	if (FT_Init_FreeType(&library)) {
		return 0;
	}

	file = fs_open(filepath, "rb");
	if (!file) {
		_font_cleanup(library, face, buf, file);
		return 0;
	}

	size_t sz = fs_size(file);
	if (sz == 0 || sz > (size_t)LONG_MAX) {
		_font_cleanup(library, face, buf, file);
		return 0;
	}
	buf = (unsigned char*)malloc(sz);
	if (!buf) {
		_font_cleanup(library, face, buf, file);
		return 0;
	}

	if (fs_read(file, buf, sz) != sz) {
		_font_cleanup(library, face, buf, file);
		return 0;
	}
	fs_close(file);
	file = NULL;

	if (FT_New_Memory_Face(library, (const FT_Byte*)buf, (FT_Long)sz, 0, &face)) {
		_font_cleanup(library, face, buf, file);
		return 0;
	}
	const size_t filepath_len = strlen(filepath);
	filepath_copy = (char*)malloc(filepath_len + 1);
	if (!filepath_copy) {
		_font_cleanup(library, face, buf, file);
		return 0;
	}
	memcpy(filepath_copy, filepath, filepath_len + 1);

	f->library = library;
	f->face = face;
	f->buf = buf;
	f->filepath = filepath_copy;
	return 1;
}

int
gtxt_ft_add_font(const char* name, const char* filepath) {
	if (!gtxt_ft_ready() || !name || name[0] == '\0' || strlen(name) > 127 ||
		!filepath || filepath[0] == '\0') {
		return -1;
	}

	const int existing = gtxt_richtext_find_font(name);
	if (existing >= 0) {
		if (existing >= FT->count) {
			return -1;
		}
		struct font* current = &FT->fonts[existing];
		if (!current->face) {
			if (existing != 0 || strcmp(name, "default") != 0) {
				return -1;
			}
			struct font loaded;
			memset(&loaded, 0, sizeof(loaded));
			if (!_ft_bind_file(&loaded, filepath)) {
				return -1;
			}
			*current = loaded;
			return existing;
		}
		if (!current->filepath || strcmp(current->filepath, filepath) != 0) {
			return -1;
		}
		return existing;
	}

	if (gtxt_richtext_get_font_slot_count() != FT->count) {
		return -1;
	}
	if (FT->count > 0 && strcmp(name, "default") == 0) {
		return -1;
	}

	const int reserve_default = FT->count == 0 && strcmp(name, "default") != 0;
	const int slots_needed = reserve_default ? 2 : 1;
	if (FT->count > MAX_FONTS - slots_needed) {
		return -1;
	}

	struct font loaded;
	memset(&loaded, 0, sizeof(loaded));
	if (!_ft_bind_file(&loaded, filepath)) {
		return -1;
	}

	const char* names[2];
	int name_count = 1;
	if (reserve_default) {
		names[0] = "default";
		names[1] = name;
		name_count = 2;
	} else {
		names[0] = name;
	}
	if (gtxt_richtext_add_fonts(names, name_count) != 0) {
		_font_dispose(&loaded);
		return -1;
	}

	const int index = FT->count + slots_needed - 1;
	FT->fonts[index] = loaded;
	FT->count += slots_needed;
	return index;
}

int
gtxt_ft_has_glyph(int font, int unicode) {
	if (!FT || font < 0 || font >= FT->count) {
		return 0;
	}
	struct font* sfont = &FT->fonts[font];
	if (!sfont->face) {
		return 0;
	}
	return FT_Get_Char_Index(sfont->face, (FT_ULong)unicode) != 0 ? 1 : 0;
}

void
gtxt_ft_set_solid_white(int enable) {
	SOLID_WHITE = enable ? 1 : 0;
}

int
gtxt_ft_get_font_cout(void) {
	return FT ? FT->count : 0;
}

static bool
_draw_default(struct font* font, FT_UInt gindex, float line_x, const struct gtxt_glyph_color* color, struct gtxt_glyph_layout* layout,
			  bool (*cb)(FT_Bitmap* bitmap, float line_x, const struct gtxt_glyph_color* color)) {
	FT_Face ft_face = font->face;

	if (FT_Load_Glyph(ft_face, gindex, FT_LOAD_DEFAULT)) {
		return false;
	}

	FT_Glyph glyph;
	if (FT_Get_Glyph(ft_face->glyph, &glyph)) {
		return false;
	}

	FT_Glyph_Metrics gm = ft_face->glyph->metrics;
	layout->bearing_x = (float)(gm.horiBearingX >> 6);
	layout->bearing_y = (float)(gm.horiBearingY >> 6);
	layout->sizer.height = (float)(gm.height >> 6);
	layout->sizer.width = (float)(gm.width >> 6);
	layout->advance = (float)(gm.horiAdvance >> 6);

	if (cb) {
		if (FT_Glyph_To_Bitmap(&glyph, FT_RENDER_MODE_NORMAL, 0, 1)) {
			FT_Done_Glyph(glyph);
			return false;
		}
		FT_BitmapGlyph bitmap_glyph = (FT_BitmapGlyph)glyph;
		FT_Bitmap* bitmap = &bitmap_glyph->bitmap;

//		assert(bitmap->rows == layout->sizer.height && bitmap->width == layout->sizer.width);
		layout->sizer.height = (float)bitmap->rows;
		layout->sizer.width = (float)bitmap->width;

		if (!cb(bitmap, line_x, color)) {
			FT_Done_Glyph(glyph);
			return false;
		}
	}

	FT_Done_Glyph(glyph);

	return true;
}

static int span_max = 0;

static inline void
_raster_cb(const int y, const int count, const FT_Span * const spans, void * const user) {
	struct spans* sptr = (struct spans*)user;
	if (!sptr || sptr->overflow) {
		return;
	}
	for (int i = 0; i < count; ++i) {
		if (sptr->sz >= MAX_SPAN) {
			sptr->overflow = 1;
			return;
		}

		if (sptr->sz > span_max) {
			span_max = sptr->sz;
		}

		struct span* s = &sptr->items[sptr->sz];
		s->x = spans[i].x;
		s->y = y;
		s->width = spans[i].len;
		s->coverage = spans[i].coverage;
		++sptr->sz;
	}
}

static inline bool
_draw_spans(FT_Library library, FT_Outline* outline, struct spans* spans) {
	if (!library || !outline || !spans) {
		return false;
	}
	FT_Raster_Params params;
	memset(&params, 0, sizeof(params));
	params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT;
	params.gray_spans = _raster_cb;
	params.user = spans;

	return FT_Outline_Render(library, outline, &params) == 0 && !spans->overflow;
}

struct point {
	float x, y;
};

struct rect {
	float xmin, xmax, ymin, ymax;
};

static inline void
_rect_merge_point(struct rect* r, float x, float y) {
	r->xmin = MIN(r->xmin, x);
	r->ymin = MIN(r->ymin, y);
	r->xmax = MAX(r->xmax, x);
	r->ymax = MAX(r->ymax, y);
}

static inline float
_rect_width(struct rect* r) {
	return r->xmax - r->xmin + 1;
}

static inline float
_rect_height(struct rect* r) {
	return r->ymax - r->ymin + 1;
}

static bool
_draw_with_edge(struct font* font, FT_UInt gindex, float line_x, const struct gtxt_glyph_color* font_color,
				float edge_size, const struct gtxt_glyph_color* edge_color, struct gtxt_glyph_layout* layout,
				bool (*cb)(int img_x, int img_y, int img_w, int img_h, float line_x, const struct gtxt_glyph_color* font_color, const struct gtxt_glyph_color* edge_color)) {
	FT_Face ft_face = font->face;
	FT_Library ft_library = font->library;

	if (FT_Load_Glyph(ft_face, gindex, FT_LOAD_NO_BITMAP)) {
		return false;
	}

	if (ft_face->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
		return false;
	}
	if (!isfinite(edge_size) || edge_size <= 0.0f || edge_size > (float)INT_MAX / 64.0f) {
		return false;
	}

	// Render the basic glyph to a span list.
	memset(IN_SPANS, 0, sizeof(*IN_SPANS));
	if (!_draw_spans(ft_library, &ft_face->glyph->outline, IN_SPANS)) {
		return false;
	}

	// Next we need the spans for the outline.
	memset(OUT_SPANS, 0, sizeof(*OUT_SPANS));

	// Set up a stroker.
	FT_Stroker stroker = NULL;
	if (FT_Stroker_New(ft_library, &stroker)) {
		return false;
	}
	FT_Stroker_Set(stroker,
		(int)(edge_size * 64),
		FT_STROKER_LINECAP_ROUND,
		FT_STROKER_LINEJOIN_ROUND,
		0);

	FT_Glyph glyph = NULL;
	if (FT_Get_Glyph(ft_face->glyph, &glyph)) {
		FT_Stroker_Done(stroker);
		return false;
	}

	layout->bearing_x = (float)(ft_face->glyph->metrics.horiBearingX >> 6);
	layout->bearing_y = (float)(ft_face->glyph->metrics.horiBearingY >> 6);
	layout->advance = (float)(ft_face->glyph->metrics.horiAdvance >> 6);

	if (FT_Glyph_StrokeBorder(&glyph, stroker, 0, 1)) {
		FT_Stroker_Done(stroker);
		FT_Done_Glyph(glyph);
		return false;
	}
	// Again, this needs to be an outline to work.
	if (glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
		FT_Stroker_Done(stroker);
		FT_Done_Glyph(glyph);
		return false;
	}
	FT_Outline *o = &((FT_OutlineGlyph)glyph)->outline;
	const bool outline_ok = _draw_spans(ft_library, o, OUT_SPANS);

	// Clean up afterwards.
	FT_Stroker_Done(stroker);
	FT_Done_Glyph(glyph);
	if (!outline_ok) {
		return false;
	}

	if (IN_SPANS->sz == 0) {
		layout->sizer.width = layout->sizer.height = 0;
		return false;
	}

	struct rect rect;
	rect.xmin = rect.xmax = (float)IN_SPANS->items[0].x;
	rect.ymin = rect.ymax = (float)IN_SPANS->items[0].y;
	for (int i = 0; i < IN_SPANS->sz; ++i) {
		struct span* s = &IN_SPANS->items[i];
		_rect_merge_point(&rect, (float)s->x, (float)s->y);
		_rect_merge_point(&rect, (float)(s->x + s->width - 1), (float)s->y);
	}
	for (int i = 0; i < OUT_SPANS->sz; ++i) {
		struct span* s = &OUT_SPANS->items[i];
		_rect_merge_point(&rect, (float)s->x, (float)s->y);
		_rect_merge_point(&rect, (float)(s->x + s->width - 1), (float)s->y);
	}

	int img_w = (int)_rect_width(&rect),
		img_h = (int)_rect_height(&rect);
	layout->sizer.width = (float)img_w;
	layout->sizer.height = (float)img_h;

	// fix for edge
	int in_img_h = ft_face->glyph->metrics.height >> 6;
	int in_img_w = ft_face->glyph->metrics.width >> 6;
	layout->bearing_x -= (img_w - in_img_w) * 0.5f;
	layout->bearing_y += (img_h - in_img_h) * 0.5f;
	layout->advance += img_w - in_img_w;
	layout->metrics_height += img_h - in_img_h;

	if (cb) {
		if (!cb((int)rect.xmin, (int)rect.ymin, img_w, img_h, line_x, font_color, edge_color)) {
			return false;
		}
	}

	return true;
}

static bool
_load_glyph_to_bitmap(int unicode, float line_x, const struct gtxt_glyph_style* style, struct gtxt_glyph_layout* layout,
					  bool (*default_cb)(FT_Bitmap* bitmap, float line_x, const struct gtxt_glyph_color* color),
					  bool (*edge_cb)(int img_x, int img_y, int img_w, int img_h, float line_x, const struct gtxt_glyph_color* font_color, const struct gtxt_glyph_color* edge_color)) {
	if (!layout) {
		return false;
	}
	memset(layout, 0, sizeof(*layout));
	if (!gtxt_ft_ready() || !style || style->font < 0 || style->font >= FT->count) {
		return false;
	}

	struct font* sfont = &FT->fonts[style->font];
	FT_Face ft_face = sfont->face;
	if (!ft_face) {
		return false;
	}

	if (FT_Set_Pixel_Sizes(ft_face, style->font_size, style->font_size)) {
		return false;
	}
	FT_Size_Metrics s = ft_face->size->metrics;
	layout->metrics_height = (float)(s.height >> 6);

	FT_UInt gindex = FT_Get_Char_Index(ft_face, unicode);
	if (gindex == 0) {
		const int DEFAULT_UNICODE = 9633;
		unicode = DEFAULT_UNICODE;
		gindex = FT_Get_Char_Index(ft_face, unicode);
	}

	if (unicode == ' ' || unicode == 160 || unicode == '\n') {
		edge_cb = NULL;
		default_cb = NULL;
	}
	if (style->edge) {
		return _draw_with_edge(sfont, gindex, line_x, &style->font_color,
			style->edge_size, &style->edge_color, layout, edge_cb);
	} else {
		return _draw_default(sfont, gindex, line_x, &style->font_color, layout, default_cb);
	}
}

static inline bool
_prepare_buf(size_t sz) {
	if (sz == 0) {
		return false;
	}
	if (BUF_SZ < sz) {
		union gtxt_color* next = (union gtxt_color*)malloc(sz);
		if (!next) {
			return false;
		}
		free(BUF);
		BUF = next;
		BUF_SZ = sz;
	}
	memset(BUF, 0, sz);
	return true;
}

static inline union gtxt_color
_lerp_color2(union gtxt_color begin, union gtxt_color end, float bp, float ep, float p) {
	union gtxt_color ret;
	p = (p - bp) / (ep - bp);
	ret.r = (uint8_t)(begin.r + (end.r - begin.r) * p);
	ret.g = (uint8_t)(begin.g + (end.g - begin.g) * p);
	ret.b = (uint8_t)(begin.b + (end.b - begin.b) * p);
	ret.a = (uint8_t)(begin.a + (end.a - begin.a) * p);
	return ret;
}

static inline union gtxt_color
_lerp_color(const struct gtxt_glyph_color* col, float line_x, int w, int h, int x, int y) {
	union gtxt_color ret;
	switch (col->mode_type)
	{
	case 0:
		ret = col->mode.ONE.color;
		break;
	case 1:
		{
			float rot_y = y + (line_x + x - w * 0.5f) * tanf(col->mode.TWO.angle);
			rot_y = MIN(h - 1, MAX(rot_y, 0));
			float p = rot_y / (h - 1);
			if (p <= col->mode.TWO.begin_pos) {
				ret = col->mode.TWO.begin_col;
			} else if (p >= col->mode.TWO.end_pos) {
				ret = col->mode.TWO.end_col;
			} else {
				ret = _lerp_color2(col->mode.TWO.begin_col, col->mode.TWO.end_col,
					col->mode.TWO.begin_pos, col->mode.TWO.end_pos, p);
			}
		}
		break;
	case 2:
		{
			float rot_y = y + (line_x + x - w * 0.5f) * tanf(col->mode.THREE.angle);
			rot_y = MIN(h - 1, MAX(rot_y, 0));
			float p = rot_y / (h - 1);
			if (p <= col->mode.THREE.begin_pos) {
				ret = col->mode.THREE.begin_col;
			} else if (p >= col->mode.THREE.end_pos) {
				ret = col->mode.THREE.end_col;
			} else {
				if (p < col->mode.THREE.mid_pos) {
					ret = _lerp_color2(col->mode.THREE.begin_col, col->mode.THREE.mid_col,
						col->mode.THREE.begin_pos, col->mode.THREE.mid_pos, p);
				} else {
					ret = _lerp_color2(col->mode.THREE.mid_col, col->mode.THREE.end_col,
						col->mode.THREE.mid_pos, col->mode.THREE.end_pos, p);
				}
			}
		}
		break;
	default:
		assert(0);
	}
	return ret;
}

static inline bool
_copy_glyph_default(FT_Bitmap* bitmap, float line_x, const struct gtxt_glyph_color* color) {
	if (!bitmap || bitmap->rows == 0 || bitmap->width == 0 ||
		(size_t)bitmap->rows > SIZE_MAX / sizeof(*BUF) / (size_t)bitmap->width) {
		return false;
	}
	const size_t sz = sizeof(*BUF) * (size_t)bitmap->rows * (size_t)bitmap->width;
	if (!_prepare_buf(sz)) {
		return false;
	}

	int ptr = 0;
	for (size_t i = 0; i < bitmap->rows; ++i) {
		for (size_t j = 0; j < bitmap->width; ++j) {
			int x = j;
			int y = bitmap->rows - 1 - i;
			union gtxt_color src = _lerp_color(color, line_x, bitmap->width, bitmap->rows, x, y);
			int dst_ptr = y * bitmap->width + x;
			union gtxt_color* dst = &BUF[dst_ptr];
			uint8_t a = bitmap->buffer[ptr];
			if (SOLID_WHITE) {
				dst->r = 255;
				dst->g = 255;
				dst->b = 255;
				dst->a = a;
			} else {
#ifdef PREMULTIPLY_APLHA
			dst->r = (src.r * a) >> 8;
			dst->g = (src.g * a) >> 8;
			dst->b = (src.b * a) >> 8;
			dst->a = 255;
#else
			dst->r = src.r;
			dst->g = src.g;
			dst->b = src.b;
			dst->a = a;
#endif // PREMULTIPLY_APLHA
			}
			++ptr;
		}
	}
	BUF_VALID = 1;
	return true;
}

static inline bool
_copy_glyph_with_edge(int img_x, int img_y, int img_w, int img_h, float line_x,
                      const struct gtxt_glyph_color* font_color, const struct gtxt_glyph_color* edge_color) {
	if (img_w <= 0 || img_h <= 0 || (size_t)img_h > SIZE_MAX / sizeof(*BUF) / (size_t)img_w) {
		return false;
	}
	const size_t sz = sizeof(*BUF) * (size_t)img_w * (size_t)img_h;
	if (!_prepare_buf(sz)) {
		return false;
	}

	// Loop over the outline spans and just draw them into the
	// image.
	for (int i = 0; i < OUT_SPANS->sz; ++i) {
		struct span* out_span = &OUT_SPANS->items[i];
		for (int w = 0; w < out_span->width; ++w) {
			int x = out_span->x - img_x + w;
			int y = out_span->y - img_y;
			union gtxt_color src = _lerp_color(edge_color, line_x, img_w, img_h, x, y);
			int index = (int)(y * img_w + x);
			union gtxt_color* dst = &BUF[index];
			uint8_t a = out_span->coverage;
			if (SOLID_WHITE) {
				dst->r = 255;
				dst->g = 255;
				dst->b = 255;
				dst->a = a;
			} else {
#ifdef PREMULTIPLY_APLHA
			dst->r = (src.r * a) >> 8;
			dst->g = (src.g * a) >> 8;
			dst->b = (src.b * a) >> 8;
			dst->a = 255;
#else
			dst->r = src.r;
			dst->g = src.g;
			dst->b = src.b;
			dst->a = a;
#endif // PREMULTIPLY_APLHA
			}
		}
	}

	// Then loop over the regular glyph spans and blend them into
	// the image.
	for (int i = 0; i < IN_SPANS->sz; ++i) {
		struct span* s = &IN_SPANS->items[i];
		for (int w = 0; w < s->width; ++w) {
			int x = s->x - img_x + w;
			int y = s->y - img_y;
			union gtxt_color src = _lerp_color(font_color, line_x, img_w, img_h, x, y);
			int index = y * img_w + x;
			union gtxt_color* dst = &BUF[index];
			uint8_t a = s->coverage;
			if (SOLID_WHITE) {
				dst->r = 255;
				dst->g = 255;
				dst->b = 255;
				dst->a = a;
			} else {
#ifdef PREMULTIPLY_APLHA
			dst->r = (int)(dst->r + ((src.r - dst->r) * a) / 255.0f);
			dst->g = (int)(dst->g + ((src.g - dst->g) * a) / 255.0f);
			dst->b = (int)(dst->b + ((src.b - dst->b) * a) / 255.0f);
			dst->a = 255;
#else
			dst->r = src.r;
			dst->g = src.g;
			dst->b = src.b;
			dst->a = a;
#endif // PREMULTIPLY_APLHA
			}
		}
	}
	BUF_VALID = 1;
	return true;
}

void
gtxt_ft_get_layout(int unicode, float line_x, const struct gtxt_glyph_style* style, struct gtxt_glyph_layout* layout) {
	if (!_load_glyph_to_bitmap(unicode, line_x, style, layout, NULL, NULL) && layout) {
		memset(layout, 0, sizeof(*layout));
	}
}

uint32_t*
gtxt_ft_gen_char(int unicode, float line_x, const struct gtxt_glyph_style* style, struct gtxt_glyph_layout* layout) {
	if (layout) {
		memset(layout, 0, sizeof(*layout));
	}
	BUF_VALID = 0;
	if (!gtxt_ft_ready() || FT->count == 0) {
		return NULL;
	}
	bool succ = _load_glyph_to_bitmap(unicode, line_x, style, layout, _copy_glyph_default, _copy_glyph_with_edge);
	if (!succ && layout) {
		memset(layout, 0, sizeof(*layout));
	}
	return succ && BUF_VALID ? (uint32_t*)BUF : NULL;
}
