#pragma once
#include "chromium_fix.h"
#include <combaseapi.h>
#include <igamesystem.h>
#include <regex>
#include "defs.h"

template< class T >
class CShaderBuffer : public IShaderBuffer
{
public:
	CShaderBuffer(T* pBlob) : m_pBlob(pBlob) {}

	virtual size_t GetSize() const
	{
		return m_pBlob ? m_pBlob->GetBufferSize() : 0;
	}

	virtual const void* GetBits() const
	{
		return m_pBlob ? m_pBlob->GetBufferPointer() : NULL;
	}

	virtual void Release()
	{
		if (m_pBlob)
		{
			m_pBlob->Release();
		}
		delete this;
	}

private:
	T* m_pBlob;
};

typedef struct _D3DXMACRO
{
	const char* Name;
	const char* Definition;
} D3DXMACRO, * LPD3DXMACRO;

D3DXMACRO D3DXMACRO_NULL{ NULL, NULL };

typedef enum _D3DXINCLUDE_TYPE
{
	D3DXINC_LOCAL,
	D3DXINC_SYSTEM,
	D3DXINC_FORCE_DWORD = 0x7fffffff,
} D3DXINCLUDE_TYPE, * LPD3DXINCLUDE_TYPE;

DECLARE_INTERFACE(ID3DXInclude)
{
	STDMETHOD(Open)(THIS_ D3DXINCLUDE_TYPE include_type, const char* filename,
		const void* parent_data, const void** data, UINT * bytes) PURE;
	STDMETHOD(Close)(THIS_ const void* data) PURE;
};

typedef void* D3DXCONSTANTTABLE_DESC ;
typedef void* D3DXCONSTANT_DESC;
typedef void* D3DXHANDLE;
typedef void* D3DXVECTOR4;
typedef void* D3DXVECTOR4;
typedef void* D3DXMATRIX;

DECLARE_INTERFACE_(ID3DXBuffer, IUnknown)
{
	/*** IUnknown methods ***/
	STDMETHOD_(HRESULT, QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;
	/*** ID3DXBuffer methods ***/
	STDMETHOD_(LPVOID, GetBufferPointer)(THIS) PURE;
	STDMETHOD_(DWORD, GetBufferSize)(THIS) PURE;
};

DECLARE_INTERFACE_(ID3DXConstantTable, ID3DXBuffer)
{
	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID iid, void** out) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;
	/*** ID3DXBuffer methods ***/
	STDMETHOD_(void*, GetBufferPointer)(THIS) PURE;
	STDMETHOD_(DWORD, GetBufferSize)(THIS) PURE;
	/*** ID3DXConstantTable methods ***/
	STDMETHOD(GetDesc)(THIS_ D3DXCONSTANTTABLE_DESC * pDesc) PURE;
	STDMETHOD(GetConstantDesc)(THIS_ D3DXHANDLE hConstant, D3DXCONSTANT_DESC * pConstantDesc, UINT * pCount) PURE;
	STDMETHOD_(UINT, GetSamplerIndex)(THIS_ D3DXHANDLE hConstant) PURE;
	STDMETHOD_(D3DXHANDLE, GetConstant)(THIS_ D3DXHANDLE hConstant, UINT Index) PURE;
	STDMETHOD_(D3DXHANDLE, GetConstantByName)(THIS_ D3DXHANDLE constant, const char* name) PURE;
	STDMETHOD_(D3DXHANDLE, GetConstantElement)(THIS_ D3DXHANDLE hConstant, UINT Index) PURE;
	STDMETHOD(SetDefaults)(THIS_ struct IDirect3DDevice9* device) PURE;
	STDMETHOD(SetValue)(THIS_ struct IDirect3DDevice9* device, D3DXHANDLE constant,
		const void* data, UINT data_size) PURE;
	STDMETHOD(SetBool)(THIS_ struct IDirect3DDevice9* device, D3DXHANDLE constant, BOOL value) PURE;
	STDMETHOD(SetBoolArray)(THIS_ struct IDirect3DDevice9* device, D3DXHANDLE constant,
		const BOOL * values, UINT value_count) PURE;
	STDMETHOD(SetInt)(THIS_ struct IDirect3DDevice9* device, D3DXHANDLE constant, INT value) PURE;
	STDMETHOD(SetIntArray)(THIS_ struct IDirect3DDevice9* device, D3DXHANDLE constant,
		const INT * values, UINT value_count) PURE;
	STDMETHOD(SetFloat)(THIS_ struct IDirect3DDevice9* device, D3DXHANDLE constant, float value) PURE;
	STDMETHOD(SetFloatArray)(THIS_ struct IDirect3DDevice9* device, D3DXHANDLE constant,
		const float* values, UINT value_count) PURE;
	STDMETHOD(SetVector)(THIS_ struct IDirect3DDevice9* device, D3DXHANDLE constant, const D3DXVECTOR4 * value) PURE;
	STDMETHOD(SetVectorArray)(THIS_ struct IDirect3DDevice9* device, D3DXHANDLE constant,
		const D3DXVECTOR4 * values, UINT value_count) PURE;
	STDMETHOD(SetMatrix)(THIS_ struct IDirect3DDevice9* device, D3DXHANDLE constant, const D3DXMATRIX * value) PURE;
	STDMETHOD(SetMatrixArray)(THIS_ struct IDirect3DDevice9* device, D3DXHANDLE constant,
		const D3DXMATRIX * values, UINT value_count) PURE;
	STDMETHOD(SetMatrixPointerArray)(THIS_ struct IDirect3DDevice9* device, D3DXHANDLE constant,
		const D3DXMATRIX * *values, UINT value_count) PURE;
	STDMETHOD(SetMatrixTranspose)(THIS_ struct IDirect3DDevice9* device, D3DXHANDLE constant,
		const D3DXMATRIX * value) PURE;
	STDMETHOD(SetMatrixTransposeArray)(THIS_ struct IDirect3DDevice9* device, D3DXHANDLE constant,
		const D3DXMATRIX * values, UINT value_count) PURE;
	STDMETHOD(SetMatrixTransposePointerArray)(THIS_ struct IDirect3DDevice9* device, D3DXHANDLE constant,
		const D3DXMATRIX * *values, UINT value_count) PURE;
};


typedef HRESULT WINAPI D3DXCompileShaderDecl(const char* src_data, UINT data_len, const D3DXMACRO* defines,
	ID3DXInclude* include, const char* function_name, const char* profile, DWORD flags,
	ID3DXBuffer** shader, ID3DXBuffer** error_messages, ID3DXConstantTable** constant_table);

extern D3DXCompileShaderDecl* D3DXCompileShader = NULL;

#define D3DXSHADER_DEBUG                          (1 << 0)
#define D3DXSHADER_SKIPVALIDATION                 (1 << 1)
#define D3DXSHADER_SKIPOPTIMIZATION               (1 << 2)
#define D3DXSHADER_PACKMATRIX_ROWMAJOR            (1 << 3)
#define D3DXSHADER_PACKMATRIX_COLUMNMAJOR         (1 << 4)
#define D3DXSHADER_PARTIALPRECISION               (1 << 5)
#define D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT        (1 << 6)
#define D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT        (1 << 7)
#define D3DXSHADER_NO_PRESHADER                   (1 << 8)
#define D3DXSHADER_AVOID_FLOW_CONTROL             (1 << 9)
#define D3DXSHADER_PREFER_FLOW_CONTROL            (1 << 10)
#define D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY (1 << 12)
#define D3DXSHADER_IEEE_STRICTNESS                (1 << 13)
#define D3DXSHADER_USE_LEGACY_D3DX9_31_DLL        (1 << 16)
#define D3DXSHADER_OPTIMIZATION_LEVEL0            (1 << 14)
#define D3DXSHADER_OPTIMIZATION_LEVEL1            0
#define D3DXSHADER_OPTIMIZATION_LEVEL2            ((1 << 14) | (1 << 15))
#define D3DXSHADER_OPTIMIZATION_LEVEL3            (1 << 15)

#include "common_hlsl_cpp_consts.h.inc"
#include "common_flashlight_fxc.h.inc"
#include "common_fxc.h.inc"
#include "shader_constant_register_map.h.inc"
#include "common_pragmas.h.inc"
#include "common_ps_fxc.h.inc"
#include "common_vertexlitgeneric_dx9.h.inc"
#include "common_vs_fxc.h.inc"

class _ID3DXInclude : public ID3DXInclude
{
	STDMETHOD(Open)(THIS_ D3DXINCLUDE_TYPE include_type, const char* filename, const void* parent_data, const void** data, UINT* bytes)
	{
		if (strcmp(filename, "common_fxc.h") == 0)
		{
			*data = common_fxc_h;
			*bytes = sizeof(common_fxc_h) - 1;
		}		
		else if (strcmp(filename, "common_vs_fxc.h") == 0)
		{
			*data = common_vs_fxc_h;
			*bytes = sizeof(common_vs_fxc_h) - 1;
		}			
		else if (strcmp(filename, "common_ps_fxc.h") == 0)
		{
			*data = common_ps_fxc_h;
			*bytes = sizeof(common_ps_fxc_h) - 1;
		}	
		else if (strcmp(filename, "common_hlsl_cpp_consts.h") == 0)
		{
			*data = common_hlsl_cpp_consts_h;
			*bytes = sizeof(common_hlsl_cpp_consts_h) - 1;
		}		
		else if (strcmp(filename, "shader_constant_register_map.h") == 0)
		{
			*data = shader_constant_register_map_h;
			*bytes = sizeof(shader_constant_register_map_h) - 1;
		}		
		else if (strcmp(filename, "common_flashlight_fxc.h") == 0)
		{
			*data = common_flashlight_fxc_h;
			*bytes = sizeof(common_flashlight_fxc_h) - 1;
		}		
		else if (strcmp(filename, "common_vertexlitgeneric_dx9.h") == 0)
		{
			*data = common_vertexlitgeneric_dx9_h;
			*bytes = sizeof(common_vertexlitgeneric_dx9_h) - 1;
		}		
		else if (strcmp(filename, "common_pragmas.h") == 0)
		{
			*data = common_pragmas_h;
			*bytes = sizeof(common_pragmas_h) - 1;
		}
		else
		{
			return !S_OK;
		}
		return S_OK;
	}
	STDMETHOD(Close)(THIS_ const void* data)
	{
		return S_OK;
	}
};

namespace std
{
	template<class BidirIt, class Traits, class CharT, class UnaryFunction>
	std::basic_string<CharT> regex_replace(BidirIt first, BidirIt last,
		const std::basic_regex<CharT, Traits>& re, UnaryFunction f)
	{
		std::basic_string<CharT> s;

		typename std::match_results<BidirIt>::difference_type
			positionOfLastMatch = 0;
		auto endOfLastMatch = first;

		auto callback = [&](const std::match_results<BidirIt>& match)
		{
			auto positionOfThisMatch = match.position(0);
			auto diff = positionOfThisMatch - positionOfLastMatch;

			auto startOfThisMatch = endOfLastMatch;
			std::advance(startOfThisMatch, diff);

			s.append(endOfLastMatch, startOfThisMatch);
			s.append(f(match));

			auto lengthOfMatch = match.length(0);

			positionOfLastMatch = positionOfThisMatch + lengthOfMatch;

			endOfLastMatch = startOfThisMatch;
			std::advance(endOfLastMatch, lengthOfMatch);
		};

		std::regex_iterator<BidirIt> begin(first, last, re), end;
		std::for_each(begin, end, callback);

		s.append(endOfLastMatch, last);

		return s;
	}

	template<class Traits, class CharT, class UnaryFunction>
	std::string regex_replace(const std::string& s,
		const std::basic_regex<CharT, Traits>& re, UnaryFunction f)
	{
		return regex_replace(s.cbegin(), s.cend(), re, f);
	}

} // namespace std

void replaceAll(std::string& str, const std::string& from, const std::string& to) {
	if (from.empty())
		return;
	size_t start_pos = 0;
	while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
		str.replace(start_pos, from.length(), to);
		start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
}
}

char* ProcessErrorMessge(const char* pProgram, const char* pErrorMessage, int lineOffset)
{
	std::string tmp(pErrorMessage);

	static char execpath[MAX_PATH] = { 0 };
	static std::regex regex = std::regex("\\\\memory\\((\\d*),(\\d*)\\): ");
	if (!*execpath)
	{
		GetModuleFileName(NULL, execpath, MAX_PATH);
		V_StripFilename(execpath);
		// x86-x64 path fix
#ifdef CHROMIUM
		int len = strlen(execpath);
		if (execpath[len - 1] == 'n' && execpath[len - 2] == 'i' && execpath[len - 3] == 'b')
		{
			execpath[len - 4] = 0;
		}
#endif
	}

	int lline = 0;
	int lcol = 0;
	replaceAll(tmp, execpath, std::string(""));

	tmp = std::regex_replace(tmp, regex,
		[lineOffset, &lcol, &lline, pProgram](const std::smatch& m)
	{
		int line = atoi(m.str(1).c_str());
		int col = atoi(m.str(2).c_str());
		if (line == lline && lcol == col) { return std::string(); }
		lline = line;
		lcol = col;
		auto program = pProgram;
		int currline = 0;
		char chr;
		std::string output;
		while (chr = *program)
		{
			if (chr == '\n')
			{
				currline++;

				if (currline == (line - 1))
				{
					program++;
					output.append(std::to_string(line + lineOffset));
					output.append(": ");
					col += 2 + m.str(1).length();
					while (chr = *program)
					{
						if (chr == '\t')
						{
							output.push_back(' ');
						}
						else
						{
							output.push_back(chr);
						}
						if (chr == '\n')
						{
							while (col > 0)
							{
								output.append(" ");
								col--;
							}
							output.append("^");
							output.push_back('\n');
							return output;
						}
						program++;
					}
				}

			}
			program++;
		}

		return output;
	});

	return strdup(tmp.c_str());
}

inline bool CompileShader(const char* pProgram, size_t nBufLen, const char* pShaderVersion, D3DXMACRO* defines, void** pOutput, int CompileFlags = 0, int LineOffset = 0)
{
	int nCompileFlags = CompileFlags;
#ifdef _DEBUG
	nCompileFlags |= D3DXSHADER_DEBUG;
#endif
	_ID3DXInclude include;
	ID3DXBuffer* pCompiledShader, *pErrorMessages;
	HRESULT hr = D3DXCompileShader(pProgram, nBufLen,
		defines, &include, "main", pShaderVersion, nCompileFlags,
		&pCompiledShader, &pErrorMessages, NULL);

	if (FAILED(hr))
	{
		if (pErrorMessages)
		{
			*pOutput = ProcessErrorMessge(pProgram, (const char*)pErrorMessages->GetBufferPointer(), LineOffset);
			pErrorMessages->Release();
		}
		return false;
	}

	CShaderBuffer< ID3DXBuffer >* pShaderBuffer = new CShaderBuffer< ID3DXBuffer >(pCompiledShader);
	if (pErrorMessages)
	{
		pErrorMessages->Release();
	}
	*pOutput = pShaderBuffer;
	return true;
}