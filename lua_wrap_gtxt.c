#include "gtxt_adapter.h"
#include "gtxt_freetype.h"
#include "gtxt_richtext.h"

#include <dtex_facade.h>

#include <lua.h>
#include <lauxlib.h>

static int
linit(lua_State* L) {
	int cap_bitmap = luaL_optinteger(L, 1, 50);
	int cap_layout = luaL_optinteger(L, 2, 500);
	
	struct dtex_cg* cg = dtexf_get_cg();
	gtxt_adapter_init(cg);

	gtxt_ft_init();

	gtxt_glyph_init(cap_bitmap, cap_layout, NULL);

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
	unsigned int val = lua_tointeger(L, 2);
	gtxt_richtext_add_color(key, val);
	return 0;
}

int
luaopen_gtxt_c(lua_State* L) {
	luaL_Reg l[] = {
		{ "init", linit },
		{ "add_font", ladd_font },
		{ "add_color", ladd_color },

		{ NULL, NULL },		
	};
	luaL_newlib(L, l);
	return 1;
}