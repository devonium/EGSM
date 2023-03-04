#pragma once

#include <GarrysMod/Lua/LuaBase.h>
#include <lua.h>
#include <vector>
#include <string>
#include <GarrysMod/Lua/Interface.h>

extern lua_State* g_pClientLua;
extern lua_State* g_pMenuLua;

typedef struct luaL_Reg {
	const char* name;
	GarrysMod::Lua::CFunc func;
} luaL_Reg;

typedef std::vector<luaL_Reg*> lua_lib;

#define LUA_LIB_LUA_FUNCTION( FUNC )                \
int FUNC##__Imp( GarrysMod::Lua::ILuaBase* LUA );   \
int FUNC( lua_State* L )                            \
{                                                   \
    GarrysMod::Lua::ILuaBase* LUA = L->luabase;     \
    LUA->SetState(L);                               \
    return FUNC##__Imp( LUA );                      \
}                                                   

#define LUA_LIB_FUNCTION(lib, name) LUA_LIB_LUA_FUNCTION(lib##_##name); \
bool dummyVar_##lib##name = push_cFunction(lib,lib##_##name, #name); \
int lib##_##name##__Imp( [[maybe_unused]] GarrysMod::Lua::ILuaBase* LUA )

void luau_openlib(GarrysMod::Lua::ILuaBase* LUA, lua_lib& lib, int isGlobal);
bool push_cFunction(lua_lib& lib, GarrysMod::Lua::CFunc f, const char* name);
lua_Debug* luau_getcallinfo(GarrysMod::Lua::ILuaBase* LUA);

#define LUAUPushConst(p, ns, n) LUA->PushNumber(ns##n); LUA->SetField(-2, p###n);
#define LUAUPushConstMan(n, v) LUA->PushNumber(v); LUA->SetField(-2, n);
#define ShaderLibError(e) LUA->PushString("ShaderLib: "##e);LUA->Error();