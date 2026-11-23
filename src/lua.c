/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "core.h"
#include "lua.h"

#define STARTUP "radium.startup" /* registry field with the functions given to on_startup() */

static lua_State *state;

/* Adds a traceback to the error message. */
static int
msghandler(lua_State *L)
{
	luaL_traceback(L, L, luaL_tolstring(L, 1, NULL), 1);
	return 1;
}

/* Calls the function on top of the stack and pops it, printing its error if it fails. */
static void
call(lua_State *L)
{
	int base = lua_gettop(L);

	lua_pushcfunction(L, msghandler);
	lua_insert(L, base);
	if (lua_pcall(L, 0, 0, base) != LUA_OK)
		fprintf(stderr, "radium: %s\n", lua_tostring(L, -1));
	lua_settop(L, base - 1);
}

static int
radium_onstartup(lua_State *L)
{
	int i, n = lua_gettop(L);

	for (i = 1; i <= n; i++)
		luaL_checktype(L, i, LUA_TFUNCTION);
	lua_getfield(L, LUA_REGISTRYINDEX, STARTUP);
	for (i = 1; i <= n; i++) {
		lua_pushvalue(L, i);
		lua_rawseti(L, -2, lua_rawlen(L, -2) + 1);
	}
	return 0;
}

static int
luaopen_radium(lua_State *L)
{
	static const luaL_Reg funcs[] = {
		{ "on_startup", radium_onstartup },
		{ NULL, NULL }
	};

	luaL_newlib(L, funcs);
	return 1;
}

static int
utils_runsh(lua_State *L)
{
	const char *cmd = lua_tostring(L, lua_upvalueindex(1));

	spawn(&(Arg){ .v = (const char *[]){ "/bin/sh", "-c", cmd, NULL } });
	return 0;
}

static int
utils_sh(lua_State *L)
{
	luaL_checkstring(L, 1);
	lua_settop(L, 1);
	lua_pushcclosure(L, utils_runsh, 1);
	return 1;
}

static int
luaopen_radium_utils(lua_State *L)
{
	static const luaL_Reg funcs[] = {
		{ "sh", utils_sh },
		{ NULL, NULL }
	};

	luaL_newlib(L, funcs);
	return 1;
}

void
rlua_init(const char *config)
{
	const char *dir;

	if (!(state = luaL_newstate()))
		die("radium: cannot create the Lua state");
	luaL_openlibs(state);
	lua_getglobal(state, "package");
	lua_getfield(state, -1, "path");
	lua_pushfstring(state, "%s;" LUALIBDIR "/?.lua;" LUALIBDIR "/?/init.lua", lua_tostring(state, -1));
	lua_setfield(state, -3, "path");
	luaL_requiref(state, "radium", luaopen_radium, 0);
	luaL_requiref(state, "radium.utils", luaopen_radium_utils, 0);
	lua_newtable(state);
	lua_setfield(state, LUA_REGISTRYINDEX, STARTUP);
	lua_settop(state, 0);

	if (config)
		lua_pushstring(state, config);
	else if ((dir = getenv("XDG_CONFIG_HOME")) && dir[0] == '/')
		lua_pushfstring(state, "%s/radium/init.lua", dir);
	else if ((dir = getenv("HOME")))
		lua_pushfstring(state, "%s/.config/radium/init.lua", dir);
	else
		return;
	if (luaL_loadfile(state, lua_tostring(state, -1)) == LUA_OK)
		call(state);
	else
		fprintf(stderr, "radium: %s\n", lua_tostring(state, -1));
	lua_settop(state, 0);
}

void
rlua_startup(void)
{
	lua_Integer i;

	lua_getfield(state, LUA_REGISTRYINDEX, STARTUP);
	for (i = 1; i <= (lua_Integer)lua_rawlen(state, -1); i++) {
		lua_rawgeti(state, -1, i);
		call(state);
	}
	lua_pop(state, 1);
}

void
rlua_cleanup(void)
{
	lua_close(state);
	state = NULL;
}
