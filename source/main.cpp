#include "chromium_fix.h"
#include <mathlib/ssemath.h>
#include "f_mathlib.h"
#include "shaderlib.h"
//#include "bass.h"
#include <GarrysMod/Lua/LuaBase.h>
#include <lua.h>
#include <GarrysMod/Lua/Interface.h>
#include <lua_utils.h>
#include <GarrysMod/FactoryLoader.hpp>
#include <scanning/symbolfinder.hpp>
#include <detouring/hook.hpp>
#include "f_igamesystem.h"
#include <e_utils.h>
#include <Color.h>
#include "version.h"
#include "Windows.h"

using namespace GarrysMod::Lua;

void ExposeVersion(ILuaBase* LUA)
{
	LUA->PushSpecial(SPECIAL_GLOB);
		LUA->CreateTable();
		LUA->PushNumber(EGSM_VERSION);
		LUA->SetField(-2, "Version");
		LUA->SetField(-2, "EGSM");
	LUA->Pop();
}

typedef int			(*luaL__loadbufferex)(lua_State* L, const char* buff, size_t sz, const char* name, const char* mode);
#include "version_check.lua.inc"
#include "depthpass.lua.inc"

void Menu_Init(ILuaBase* LUA)
{
	ExposeVersion(LUA);
	ShaderLib::MenuInit(LUA);
	//Bass::MenuInit(LUA);

	auto lua_shared = GetModuleHandle("lua_shared.dll");
	if (!lua_shared) { ShaderLibError("lua_shared.dll == NULL\n"); }

	luaL__loadbufferex luaL__loadbufferfex = (luaL__loadbufferex)GetProcAddress(lua_shared, "luaL_loadbufferx");
	if (luaL__loadbufferfex(LUA->GetState(), version_check_lua, sizeof(version_check_lua) - 1, "", NULL))
	{
		Msg("%s\n", LUA->GetString());
		LUA->Pop();
		return;
	}
	if (LUA->PCall(0, 0, 0))
	{
		Msg("%s\n", LUA->GetString());
		LUA->Pop();
	}
}

void CL_Init(ILuaBase* LUA)
{
	ExposeVersion(LUA);
	ShaderLib::LuaInit(LUA);

	auto lua_shared = GetModuleHandle("lua_shared.dll");
	if (!lua_shared) { ShaderLibError("lua_shared.dll == NULL\n"); }
		
	luaL__loadbufferex luaL__loadbufferfex = (luaL__loadbufferex)GetProcAddress(lua_shared, "luaL_loadbufferx");
	if (luaL__loadbufferfex(LUA->GetState(), depthpass_lua, sizeof(depthpass_lua) - 1, "", NULL))
	{
		Msg("%s\n", LUA->GetString());
		LUA->Pop();
		return;
	}
	if (LUA->PCall(0, 0, 0))
	{
		Msg("%s\n", LUA->GetString());
		LUA->Pop();
	}

	//Bass::LuaInit(LUA);
}

void CL_PostInit(ILuaBase* LUA)
{
	ShaderLib::LuaPostInit(LUA);

	LUA->PushSpecial(SPECIAL_GLOB);
	LUA->GetField(-1, "shaderlib");
	LUA->GetField(-1, "__INIT");
	if (LUA->PCall(0, 0, 0))
	{
		LUA->Pop();
	}
	LUA->Pop(2);
	//Bass::LuaInit(LUA);
}

void CL_Deinit(ILuaBase* LUA)
{
	ShaderLib::LuaDeinit(LUA);
}

typedef int32_t		(*lua_initcl)();
typedef lua_State*  (*luaL_newstate)();
typedef void		(*luaL_closestate)(lua_State*);

Detouring::Hook lua_initcl_hk;
Detouring::Hook luaL_newstate_hk;
Detouring::Hook lua_loadbufferex_hk;
Detouring::Hook luaL_closestate_hk;

extern lua_State* g_pClientLua = NULL;
extern lua_State* g_pMenuLua = NULL;

int32_t lua_initcl_detour()
{
	luaL_newstate_hk.Enable();
	int32_t ret = lua_initcl_hk.GetTrampoline<lua_initcl>()();
	luaL_newstate_hk.Disable();
	lua_loadbufferex_hk.Disable();

	GarrysMod::Lua::ILuaBase* LUA = g_pClientLua->luabase;
	LUA->SetState(g_pClientLua);
	CL_PostInit(LUA);

	return ret;
}

lua_State* luaL_newstate_detour()
{
	g_pClientLua = luaL_newstate_hk.GetTrampoline<luaL_newstate>()();
	luaL_newstate_hk.Disable();
	lua_loadbufferex_hk.Enable();
	return g_pClientLua;
}

int luaL_loadbufferex_detour(lua_State* L, const char* buff, size_t sz, const char* name, const char* mode)
{
	if (L == g_pClientLua)
	{
		GarrysMod::Lua::ILuaBase* LUA = L->luabase;
		LUA->SetState(L);
		lua_loadbufferex_hk.Disable();
		CL_Init(LUA);
	}
	int ret = lua_loadbufferex_hk.GetTrampoline<luaL__loadbufferex>()(L, buff, sz, name, mode);

	return ret;
}

void luaL_closestate_detour(lua_State* L)
{
	if (L == g_pClientLua)
	{
		GarrysMod::Lua::ILuaBase* LUA = g_pClientLua->luabase;
		LUA->SetState(g_pClientLua);
		CL_Deinit(LUA);
		g_pClientLua = NULL;
	}
	luaL_closestate_hk.GetTrampoline<luaL_closestate>()(L);
}


GMOD_MODULE_OPEN()
{
	g_pMenuLua = LUA->GetState();
	static Color msgc(100, 255, 100, 255);
	ConColorMsg(msgc, "-----EGSM Loading-----\n");
	Menu_Init(LUA);

	auto lua_shared = GetModuleHandle("lua_shared.dll");
	if (!lua_shared) { ShaderLibError("lua_shared.dll == NULL\n"); }

	auto client = GetModuleHandle("client.dll");
	if (!client) { ShaderLibError("client.dll == NULL\n");}

	void* luaL_newstatef = GetProcAddress(lua_shared, "luaL_newstate");
	{
		Detouring::Hook::Target target(luaL_newstatef);
		luaL_newstate_hk.Create(target, luaL_newstate_detour);
	}

	void* luaL__loadbufferfex = GetProcAddress(lua_shared, "luaL_loadbufferx");
	{
		Detouring::Hook::Target target(luaL__loadbufferfex);
		lua_loadbufferex_hk.Create(target, luaL_loadbufferex_detour);
	}

	void* luaL_closestatef = GetProcAddress(lua_shared, "lua_close");
	{
		Detouring::Hook::Target target(luaL_closestatef);
		luaL_closestate_hk.Create(target, luaL_closestate_detour);
		luaL_closestate_hk.Enable();
	}
	const char lua_initclf_sign[] =
		HOOK_SIGN_CHROMIUM_x32("55 8B EC 81 EC 18 02 00 00 53 68 ? ? ? ? 8B D9 FF 15 ? ? ? ? 83 C4 04 E8 ? ? ? ? 68 ? ? ? ? 51 8B C8 C7 04 24 33 33 73 3F 8B 10 FF 52 6C 83 3D ? ? ? ? ? 74 0E 68 ? ? ? ? FF 15 ? ? ? ? 83 C4 04 68 04 02 08 00 E8 ? ? ? ? 83 C4 04 85 C0 74 09 8B C8 E8 ? ? ? ? EB 02 33 C0 56 57 A3 ? ? ? ? E8 ? ? ? ? 6A 00 6A 00 8B C8 8B 10 FF 52 10 A3 ? ? ? ? FF 15 ? ? ? ? 8B ? ? ? ? ? 68")
		HOOK_SIGN_CHROMIUM_x64("48 89 5C 24 08 48 89 74 24 10 57 48 81 EC 60 02 00 00 48 8B ? ? ? ? ? 48 33 C4 48 89 84 24 50 02 00 00 48 8B F1 48 8D ? ? ? ? ? FF 15 ? ? ? ? E8 ? ? ? ? F3 0F 10 ? ? ? ? ? 4C 8D ? ? ? ? ? 48 8B C8 48 8B 10 FF 92 D8 00 00 00 48 83 3D ? ? ? ? ? 74 0D 48 8D ? ? ? ? ? FF 15 ? ? ? ? B9 08 02 0C 00 E8 ? ? ? ? 48 85 C0 74 08 48 8B C8 E8 ? ? ? ? 48 89 ? ? ? ? ? E8 ? ? ? ? 45 33")
		HOOK_SIGN_M("55 8B EC 81 EC 18 02 00 00 53 68 ? ? ? ? 8B D9 FF 15 ? ? ? ? 83 C4 04 E8 ? ? ? ? D9 05 ? ? ? ? 68 ? ? ? ? 51 8B 10 8B C8 D9 1C 24 FF 52 6C 83 3D ? ? ? ? ? 74 0B 68 ? ? ? ? FF 15 ? ? ? ? 68 04 02 08 00 E8 ? ? ? ? 83 C4 04 85 C0 74 09 8B C8 E8 ? ? ? ? EB 02 33 C0 56 57 A3 ? ? ? ? E8 ? ? ? ? 6A 00 6A 00 8B C8 8B 10 FF 52 10 A3 ? ? ? ? FF 15 ? ? ? ? 8B")

	auto lua_initclf = ScanSign(client, lua_initclf_sign, sizeof(lua_initclf_sign) - 1);

	if (!lua_initclf)
	{
		ShaderLibError("lua_initclf == NULL\n");
	}

	{
		Detouring::Hook::Target target(lua_initclf);
		lua_initcl_hk.Create(target, lua_initcl_detour);
		lua_initcl_hk.Enable();
	}

	ConColorMsg(msgc, "----------------------\n");


	return 0;
}

GMOD_MODULE_CLOSE( )
{
	ShaderLib::MenuDeinit(LUA);
	return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		HMODULE phModule;
		char l[MAX_PATH];
		GetModuleFileName(hinstDLL, l, MAX_PATH);
		GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN, l, &phModule);
		break;
	}
	return TRUE;
}

//#include <cdll_client_int.h>
//#include "vgui/ISurface.h"
//
//#include "iclientmode.h"
//IVEngineClient* cengine = NULL;
//vgui::ISurface* g_pVGuiSurface = NULL;
//
/*class CLockCursor : CAutoGameSystemPerFrame
{
public:
	CLockCursor()
	{

	}
	virtual bool Init()
	{
		return true;
	}
	int IsLocked = true;

	void Lock()
	{
		if (!IsLocked)
		{
			IsLocked = true;
			auto mWindow = GetActiveWindow();

			RECT rect;
			GetClientRect(mWindow, &rect);

			POINT ul;
			ul.x = rect.left;
			ul.y = rect.top;

			POINT lr;
			lr.x = rect.right;
			lr.y = rect.bottom;

			MapWindowPoints(mWindow, nullptr, &ul, 1);
			MapWindowPoints(mWindow, nullptr, &lr, 1);

			rect.left = ul.x;
			rect.top = ul.y;

			rect.right = lr.x;
			rect.bottom = lr.y;

			ClipCursor(&rect);
		}
	}

	void Unlock()
	{
		if (IsLocked)
		{
			IsLocked = false;
			ClipCursor(NULL);
		}
	}

	virtual void Update(float frametime)
	{
		Msg("HasFocus() %i IsCursorVisible() %i IsInGame() %i\n", g_pVGuiSurface->HasFocus(), g_pVGuiSurface->IsCursorVisible(), cengine->IsInGame());

		if (!g_pVGuiSurface->HasFocus() || g_pVGuiSurface->IsCursorVisible()) { Unlock(); return; }
		if (cengine->IsInGame())
		{
			Lock();
		}
		else
		{
			Unlock();
		}
	}
};
*/
//static CLockCursor test;

//auto clientdll = GetModuleHandle("client.dll");
//if (!clientdll) { Msg("client.dll == NULL\n"); return 0; }

//typedef void (IGameSystem_AddDecl)(IGameSystem* pSys);
//
//if (!Sys_LoadInterface("engine", VENGINE_CLIENT_INTERFACE_VERSION, NULL, (void**)&cengine))
//{
//	ShaderLibError("IVEngineClient == NULL"); return 0;
//}
//
//if (!Sys_LoadInterface("vgui2", VGUI_SURFACE_INTERFACE_VERSION, NULL, (void**)&g_pVGuiSurface))
//{
//	ShaderLibError("ISurface == NULL"); return 0;
//}

//const char sign2[] = "55 8B EC 8B ? ? ? ? ? A1 ? ? ? ? 53 56 57 8B FA BB 08 00 00 00 8D 4F 01 3B C8 0F 8E ? ? ? ? 8B ? ? ? ? ? 85 F6 78 7C 74 0D 8B C7 99 F7 FE 8B C7 2B C2 03 C6 EB 0F 85 C0 0F 44 C3 3B C1 7D 26 03 C0 3B C1 7C FA 3B C1 7D 1C";
//IGameSystem_AddDecl* IGameSystem_Add = (IGameSystem_AddDecl*)ScanSign(clientdll, sign2, sizeof(sign2) - 1);
//if (!IGameSystem_Add) { Msg("IGameSystem::Add == NULL\n"); return 0; }
//IGameSystem_Add((IGameSystem*)&test);
