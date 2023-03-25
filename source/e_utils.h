#pragma once
#include <Windows.h>
#include <scanning/symbolfinder.hpp>
#include <detouring/hook.hpp>

struct DynLibInfo
{
	void* baseAddress;
	size_t memorySize;
};

bool GetLibraryInfo(const void* handle, DynLibInfo& lib);
void* ScanSign(const void* handle, const char* sig, size_t len, const void* start = NULL);

#define GetVTable(ptr) ((void***)ptr)[0]

#define Define_thiscall_Hook(rettype, name, thistype, ...) \
	Detouring::Hook name##_hook; \
	typedef rettype (__thiscall* name##_decl)(thistype _this, __VA_ARGS__); \
	inline name##_decl name##_trampoline() { return name##_hook.GetTrampoline<name##_decl>();}\
	rettype __fastcall name##_detour(thistype _this, void* edx, __VA_ARGS__) 

#define Setup_Hook(name, targ) \
{ \
	Detouring::Hook::Target target(reinterpret_cast<void*>(targ)); \
	name##_hook.Create(target, name##_detour); \
	name##_hook.Enable(); \
}

#define HOOK_SIGN(x) x;

#ifdef WIN64
#define HOOK_SIGN_x32(x)
#define HOOK_SIGN_x64(x) x;
#else
#define HOOK_SIGN_x32(x) x;
#define HOOK_SIGN_x64(x) 
#endif

#ifdef CHROMIUM
#define HOOK_SIGN_M(x)

#ifdef WIN64
#define HOOK_SIGN_CHROMIUM_X64(x) x;
#define HOOK_SIGN_CHROMIUM_x32(x)
#else
#define HOOK_SIGN_CHROMIUM_X64(x)
#define HOOK_SIGN_CHROMIUM_X32(x) x;

#endif
#define HOOK_SIGN_CHROMIUM(x) x;
#else
#define HOOK_SIGN_M(x) x;
#define HOOK_SIGN_CHROMIUM(x)
#define HOOK_SIGN_CHROMIUM_X64(x)
#define HOOK_SIGN_CHROMIUM_X32(x)
#endif