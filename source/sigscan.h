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