#pragma once

#include <GarrysMod/Lua/LuaBase.h>
#include <lua.h>
#include <vector>
#include <string>
#include <GarrysMod/Lua/Interface.h>
#include "lua_utils.h"

void luau_openlib(GarrysMod::Lua::ILuaBase* LUA, lua_lib& lib, int isGlobal)
{
	for (unsigned int i = 0; i < lib.size(); i++)
	{
		auto d = lib[i];
		LUA->PushCFunction(d->func);
		if (isGlobal)
		{
			LUA->SetField(GarrysMod::Lua::SPECIAL_GLOB, d->name);
		}
		else
		{
			LUA->SetField(-2, d->name);
		}
	}
}

bool push_cFunction(lua_lib& lib, GarrysMod::Lua::CFunc f, const char* name)
{
	luaL_Reg* info = new luaL_Reg();
	info->func = f;
	info->name = name;
	lib.push_back(info);
	return true;
}

lua_Debug* luau_getcallinfo(GarrysMod::Lua::ILuaBase* LUA)
{
	static lua_Debug ar;
	LUA->GetStack(1, &ar);
	LUA->GetInfo("lS", &ar);
	return &ar;
}
