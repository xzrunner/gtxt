#include "gtxt_adapter.h"
#include "gtxt_freetype.h"
#include "gtxt_glyph.h"
#include "gtxt_richtext.h"

#include <dtex_facade.h>

#include <lua.h>
#include <lauxlib.h>

static void*
ext_sym_create(const char* str) {
	if (strncmp(str, "path=", 5) != 0) {
		return NULL;
	}

	return NULL;
}

static void
ext_sym_release(void* ext_sym) {
	if (!ext_sym) {
		return;
	}
}

static void 
ext_sym_get_size(void* ext_sym, int* width, int* height) {
	if (!ext_sym) {
		*width= *height = 0;
		return;
	}

	*width = 0;
	*height = 0;
}

static void
ext_sym_render(void* ext_sym, float x, float y, void* ud) {
	if (!ext_sym) {
		return;
	}
}

static int
linit(lua_State* L) {
	int cap_bitmap = luaL_optinteger(L, 1, 50);
	int cap_layout = luaL_optinteger(L, 2, 500);
	
	struct dtex_cg* cg = dtexf_get_cg();
	gtxt_adapter_init(cg);

	gtxt_ft_init();

	gtxt_glyph_init(cap_bitmap, cap_layout, NULL);

	gtxt_richtext_ext_sym_cb_init(&ext_sym_create, &ext_sym_release, &ext_sym_get_size, &ext_sym_render);

	return 0;
}

static int
ladd_font(lua_State* L) {
	const char* name = luaL_checkstring(L, 1);
	const char* path = luaL_checkstring(L, 2);
	gtxt_ft_add_font(name, path);
	return 0;
}

int
luaopen_gtxt_c(lua_State* L) {
	luaL_Reg l[] = {
		{ "init", linit },
		{ "add_font", ladd_font },

		{ NULL, NULL },		
	};
	luaL_newlib(L, l);
	return 1;
}