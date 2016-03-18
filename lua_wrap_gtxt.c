#include "gtxt_adapter.h"
#include "gtxt_freetype.h"
#include "gtxt_richtext.h"
#include "gtxt_layout.h"

#include <dtex_facade.h>

#include <lua.h>
#include <lauxlib.h>

static int
lcreate(lua_State* L) {
	int cap_bitmap = (int)(luaL_optinteger(L, 1, 50));
	int cap_layout = (int)(luaL_optinteger(L, 2, 500));
	
	struct dtex_cg* cg = dtexf_get_cg();
	gtxt_adapter_create(cg);

	gtxt_ft_create();

	gtxt_glyph_create(cap_bitmap, cap_layout, NULL);

	return 0;
}

static int
lrelease(lua_State* L) {
	gtxt_adapter_release();
	gtxt_ft_release();
	gtxt_glyph_release();
	gtxt_layout_release();
	gtxt_richtext_release();

	return 0;
}

static int
ladd_font(lua_State* L) {
	const char* name = luaL_checkstring(L, 1);
	const char* path = luaL_checkstring(L, 2);
	gtxt_ft_add_font(name, path);
	return 0;
}

static int
ladd_color(lua_State* L) {
	const char* key = luaL_checkstring(L, 1);
	unsigned int val = (unsigned int)(lua_tointeger(L, 2));
	gtxt_richtext_add_color(key, val);
	return 0;
}

int
luaopen_gtxt_c(lua_State* L) {
	luaL_Reg l[] = {
		{ "create", lcreate },
		{ "release", lrelease },

		{ "add_font", ladd_font },
		{ "add_color", ladd_color },

		{ NULL, NULL },		
	};
	luaL_newlib(L, l);
	return 1;
}