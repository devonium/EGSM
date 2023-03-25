#include "e_utils.h"
#include <Windows.h>
#include <scanning/symbolfinder.hpp>
#include <detouring/hook.hpp>

bool GetLibraryInfo(const void* handle, DynLibInfo& lib)
{
	if (handle == nullptr)
		return false;

#if defined ARCHITECTURE_X86

	const WORD IMAGE_FILE_MACHINE = IMAGE_FILE_MACHINE_I386;

#elif defined ARCHITECTURE_X86_64

	const WORD IMAGE_FILE_MACHINE = IMAGE_FILE_MACHINE_AMD64;

#endif

	MEMORY_BASIC_INFORMATION info;
	if (VirtualQuery(handle, &info, sizeof(info)) == FALSE)
		return false;

	uintptr_t baseAddr = reinterpret_cast<uintptr_t>(info.AllocationBase);

	IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(baseAddr);
	IMAGE_NT_HEADERS* pe = reinterpret_cast<IMAGE_NT_HEADERS*>(baseAddr + dos->e_lfanew);
	IMAGE_FILE_HEADER* file = &pe->FileHeader;
	IMAGE_OPTIONAL_HEADER* opt = &pe->OptionalHeader;

	if (dos->e_magic != IMAGE_DOS_SIGNATURE || pe->Signature != IMAGE_NT_SIGNATURE || opt->Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC)
		return false;

	if (file->Machine != IMAGE_FILE_MACHINE)
		return false;

	if ((file->Characteristics & IMAGE_FILE_DLL) == 0)
		return false;

	lib.memorySize = opt->SizeOfImage;
	lib.baseAddress = reinterpret_cast<void*>(baseAddr);
	return true;
}

void* ScanSign(const void* handle, const char* sig, size_t len, const void* start)
{
	DynLibInfo lib;
	memset(&lib, 0, sizeof(DynLibInfo));
	if (!GetLibraryInfo(handle, lib))
		return nullptr;

	uint8_t* ptr = reinterpret_cast<uint8_t*>(start > lib.baseAddress ? const_cast<void*>(start) : lib.baseAddress);
	uint8_t* end = reinterpret_cast<uint8_t*>(lib.baseAddress) + lib.memorySize - len;
	bool found = true;
	while (ptr < end)
	{
		uint8_t* tmp = ptr;
		for (size_t i = 0; i < len; ++i)
		{
			if (sig[i] == ' ') { continue; }
			if (sig[i] == '?') { tmp++; continue; }

			if (tmp[0] != strtoul(&sig[i], NULL, 16))
			{
				found = false;
				break;
			}
			i++;
			tmp++;
		}

		if (found)
			return ptr;

		++ptr;
		found = true;
	}

	return nullptr;
}