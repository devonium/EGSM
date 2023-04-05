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
#ifdef WIN64
		if (execpath[len - 1] == '4' && execpath[len - 2] == '6' && execpath[len - 3] == 'n')
		{
			execpath[len - 10] = 0;
		}
#else
		if (execpath[len - 1] == 'n' && execpath[len - 2] == 'i' && execpath[len - 3] == 'b')
		{
			execpath[len - 4] = 0;
		}
#endif
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

DECLARE_INTERFACE_(IDirect3DDevice9, IUnknown)
{
	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;
	typedef void* IDirect3D9;
	typedef void* D3DCAPS9;
	typedef void* D3DDISPLAYMODE;
	typedef void* D3DDEVICE_CREATION_PARAMETERS;
	typedef void* IDirect3DSurface9;
	typedef void* IDirect3DSwapChain9;
	typedef void* D3DPRESENT_PARAMETERS;
	typedef void* D3DRASTER_STATUS;
	typedef void* D3DGAMMARAMP;
	typedef int D3DBACKBUFFER_TYPE;
	/*** IDirect3DDevice9 methods ***/
	STDMETHOD(TestCooperativeLevel)(THIS) PURE;
	STDMETHOD_(UINT, GetAvailableTextureMem)(THIS) PURE;
	STDMETHOD(EvictManagedResources)(THIS) PURE;
	STDMETHOD(GetDirect3D)(THIS_ IDirect3D9 * *ppD3D9) PURE;
	STDMETHOD(GetDeviceCaps)(THIS_ D3DCAPS9 * pCaps) PURE;
	STDMETHOD(GetDisplayMode)(THIS_ UINT iSwapChain, D3DDISPLAYMODE * pMode) PURE;
	STDMETHOD(GetCreationParameters)(THIS_ D3DDEVICE_CREATION_PARAMETERS * pParameters) PURE;
	STDMETHOD(SetCursorProperties)(THIS_ UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9 * pCursorBitmap) PURE;
	STDMETHOD_(void, SetCursorPosition)(THIS_ int X, int Y, DWORD Flags) PURE;
	STDMETHOD_(BOOL, ShowCursor)(THIS_ BOOL bShow) PURE;
	STDMETHOD(CreateAdditionalSwapChain)(THIS_ D3DPRESENT_PARAMETERS * pPresentationParameters, IDirect3DSwapChain9 * *pSwapChain) PURE;
	STDMETHOD(GetSwapChain)(THIS_ UINT iSwapChain, IDirect3DSwapChain9 * *pSwapChain) PURE;
	STDMETHOD_(UINT, GetNumberOfSwapChains)(THIS) PURE;
	STDMETHOD(Reset)(THIS_ D3DPRESENT_PARAMETERS * pPresentationParameters) PURE;
	STDMETHOD(Present)(THIS_ CONST RECT * pSourceRect, CONST RECT * pDestRect, HWND hDestWindowOverride, CONST RGNDATA * pDirtyRegion) PURE;
	STDMETHOD(GetBackBuffer)(THIS_ UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 * *ppBackBuffer) PURE;
	STDMETHOD(GetRasterStatus)(THIS_ UINT iSwapChain, D3DRASTER_STATUS * pRasterStatus) PURE;
	STDMETHOD(SetDialogBoxMode)(THIS_ BOOL bEnableDialogs) PURE;
	STDMETHOD_(void, SetGammaRamp)(THIS_ UINT iSwapChain, DWORD Flags, CONST D3DGAMMARAMP * pRamp) PURE;
	STDMETHOD_(void, GetGammaRamp)(THIS_ UINT iSwapChain, D3DGAMMARAMP * pRamp) PURE;

	typedef void* D3DPOOL;
	typedef void* IDirect3DTexture9;
	typedef void* IDirect3DVolumeTexture9;
	typedef void* IDirect3DCubeTexture9;
	typedef void* IDirect3DVertexBuffer9;
	typedef void* IDirect3DIndexBuffer9;
	typedef int D3DMULTISAMPLE_TYPE;
	typedef int D3DTEXTUREFILTERTYPE;
	typedef void* IDirect3DBaseTexture9;
	typedef void* D3DCOLOR;
	STDMETHOD(CreateTexture)(THIS_ UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9 * *ppTexture, HANDLE * pSharedHandle) PURE;
	STDMETHOD(CreateVolumeTexture)(THIS_ UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9 * *ppVolumeTexture, HANDLE * pSharedHandle) PURE;
	STDMETHOD(CreateCubeTexture)(THIS_ UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9 * *ppCubeTexture, HANDLE * pSharedHandle) PURE;
	STDMETHOD(CreateVertexBuffer)(THIS_ UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9 * *ppVertexBuffer, HANDLE * pSharedHandle) PURE;
	STDMETHOD(CreateIndexBuffer)(THIS_ UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9 * *ppIndexBuffer, HANDLE * pSharedHandle) PURE;
	STDMETHOD(CreateRenderTarget)(THIS_ UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9 * *ppSurface, HANDLE * pSharedHandle) PURE;
	STDMETHOD(CreateDepthStencilSurface)(THIS_ UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9 * *ppSurface, HANDLE * pSharedHandle) PURE;
	STDMETHOD(UpdateSurface)(THIS_ IDirect3DSurface9 * pSourceSurface, CONST RECT * pSourceRect, IDirect3DSurface9 * pDestinationSurface, CONST POINT * pDestPoint) PURE;
	STDMETHOD(UpdateTexture)(THIS_ IDirect3DBaseTexture9 * pSourceTexture, IDirect3DBaseTexture9 * pDestinationTexture) PURE;
	STDMETHOD(GetRenderTargetData)(THIS_ IDirect3DSurface9 * pRenderTarget, IDirect3DSurface9 * pDestSurface) PURE;
	STDMETHOD(GetFrontBufferData)(THIS_ UINT iSwapChain, IDirect3DSurface9 * pDestSurface) PURE;
	STDMETHOD(StretchRect)(THIS_ IDirect3DSurface9 * pSourceSurface, CONST RECT * pSourceRect, IDirect3DSurface9 * pDestSurface, CONST RECT * pDestRect, D3DTEXTUREFILTERTYPE Filter) PURE;
	STDMETHOD(ColorFill)(THIS_ IDirect3DSurface9 * pSurface, CONST RECT * pRect, D3DCOLOR color) PURE;
	STDMETHOD(CreateOffscreenPlainSurface)(THIS_ UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 * *ppSurface, HANDLE * pSharedHandle) PURE;
	STDMETHOD(SetRenderTarget)(THIS_ DWORD RenderTargetIndex, IDirect3DSurface9 * pRenderTarget) PURE;
	STDMETHOD(GetRenderTarget)(THIS_ DWORD RenderTargetIndex, IDirect3DSurface9 * *ppRenderTarget) PURE;
	STDMETHOD(SetDepthStencilSurface)(THIS_ IDirect3DSurface9 * pNewZStencil) PURE;
	STDMETHOD(GetDepthStencilSurface)(THIS_ IDirect3DSurface9 * *ppZStencilSurface) PURE;
	STDMETHOD(BeginScene)(THIS) PURE;
	STDMETHOD(EndScene)(THIS) PURE;

	typedef void* D3DRECT;
	typedef void* D3DMATRIX;
	typedef void* IDirect3DVertexDeclaration9;
	typedef void* D3DVIEWPORT9;
	typedef void* D3DLIGHT9;
	typedef void* D3DMATERIAL9;
	typedef int D3DTRANSFORMSTATETYPE;
	typedef int D3DRENDERSTATETYPE;
	typedef int D3DTEXTURESTAGESTATETYPE;
	typedef int D3DSTATEBLOCKTYPE;
	typedef int D3DSAMPLERSTATETYPE;
	typedef int D3DPRIMITIVETYPE;

	STDMETHOD(Clear)(THIS_ DWORD Count, CONST D3DRECT * pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) PURE;
	STDMETHOD(SetTransform)(THIS_ D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX * pMatrix) PURE;
	STDMETHOD(GetTransform)(THIS_ D3DTRANSFORMSTATETYPE State, D3DMATRIX * pMatrix) PURE;
	STDMETHOD(MultiplyTransform)(THIS_ D3DTRANSFORMSTATETYPE, CONST D3DMATRIX*) PURE;
	STDMETHOD(SetViewport)(THIS_ CONST D3DVIEWPORT9 * pViewport) PURE;
	STDMETHOD(GetViewport)(THIS_ D3DVIEWPORT9 * pViewport) PURE;
	STDMETHOD(SetMaterial)(THIS_ CONST D3DMATERIAL9 * pMaterial) PURE;
	STDMETHOD(GetMaterial)(THIS_ D3DMATERIAL9 * pMaterial) PURE;
	STDMETHOD(SetLight)(THIS_ DWORD Index, CONST D3DLIGHT9*) PURE;
	STDMETHOD(GetLight)(THIS_ DWORD Index, D3DLIGHT9*) PURE;
	STDMETHOD(LightEnable)(THIS_ DWORD Index, BOOL Enable) PURE;
	STDMETHOD(GetLightEnable)(THIS_ DWORD Index, BOOL * pEnable) PURE;
	STDMETHOD(SetClipPlane)(THIS_ DWORD Index, CONST float* pPlane) PURE;
	STDMETHOD(GetClipPlane)(THIS_ DWORD Index, float* pPlane) PURE;
	STDMETHOD(SetRenderState)(THIS_ D3DRENDERSTATETYPE State, DWORD Value) PURE;
	STDMETHOD(GetRenderState)(THIS_ D3DRENDERSTATETYPE State, DWORD * pValue) PURE;

	typedef void* IDirect3DStateBlock9;
	typedef void* D3DCLIPSTATUS9;
	typedef void* D3DVERTEXELEMENT9;

	STDMETHOD(CreateStateBlock)(THIS_ D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9 * *ppSB) PURE;
	STDMETHOD(BeginStateBlock)(THIS) PURE;
	STDMETHOD(EndStateBlock)(THIS_ IDirect3DStateBlock9 * *ppSB) PURE;
	STDMETHOD(SetClipStatus)(THIS_ CONST D3DCLIPSTATUS9 * pClipStatus) PURE;
	STDMETHOD(GetClipStatus)(THIS_ D3DCLIPSTATUS9 * pClipStatus) PURE;
	STDMETHOD(GetTexture)(THIS_ DWORD Stage, IDirect3DBaseTexture9 * *ppTexture) PURE;
	STDMETHOD(SetTexture)(THIS_ DWORD Stage, IDirect3DBaseTexture9 * pTexture) PURE;
	STDMETHOD(GetTextureStageState)(THIS_ DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD * pValue) PURE;
	STDMETHOD(SetTextureStageState)(THIS_ DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) PURE;
	STDMETHOD(GetSamplerState)(THIS_ DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD * pValue) PURE;
	STDMETHOD(SetSamplerState)(THIS_ DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) PURE;
	STDMETHOD(ValidateDevice)(THIS_ DWORD * pNumPasses) PURE;
	STDMETHOD(SetPaletteEntries)(THIS_ UINT PaletteNumber, CONST PALETTEENTRY * pEntries) PURE;
	STDMETHOD(GetPaletteEntries)(THIS_ UINT PaletteNumber, PALETTEENTRY * pEntries) PURE;
	STDMETHOD(SetCurrentTexturePalette)(THIS_ UINT PaletteNumber) PURE;
	STDMETHOD(GetCurrentTexturePalette)(THIS_ UINT * PaletteNumber) PURE;
	STDMETHOD(SetScissorRect)(THIS_ CONST RECT * pRect) PURE;
	STDMETHOD(GetScissorRect)(THIS_ RECT * pRect) PURE;
	STDMETHOD(SetSoftwareVertexProcessing)(THIS_ BOOL bSoftware) PURE;
	STDMETHOD_(BOOL, GetSoftwareVertexProcessing)(THIS) PURE;
	STDMETHOD(SetNPatchMode)(THIS_ float nSegments) PURE;
	STDMETHOD_(float, GetNPatchMode)(THIS) PURE;
	STDMETHOD(DrawPrimitive)(THIS_ D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) PURE;
	STDMETHOD(DrawIndexedPrimitive)(THIS_ D3DPRIMITIVETYPE, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) PURE;
	STDMETHOD(DrawPrimitiveUP)(THIS_ D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) PURE;
	STDMETHOD(DrawIndexedPrimitiveUP)(THIS_ D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, CONST void* pIndexData, D3DFORMAT IndexDataFormat, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) PURE;
	STDMETHOD(ProcessVertices)(THIS_ UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9 * pDestBuffer, IDirect3DVertexDeclaration9 * pVertexDecl, DWORD Flags) PURE;
	STDMETHOD(CreateVertexDeclaration)(THIS_ CONST D3DVERTEXELEMENT9 * pVertexElements, IDirect3DVertexDeclaration9 * *ppDecl) PURE;
	STDMETHOD(SetVertexDeclaration)(THIS_ IDirect3DVertexDeclaration9 * pDecl) PURE;
	STDMETHOD(GetVertexDeclaration)(THIS_ IDirect3DVertexDeclaration9 * *ppDecl) PURE;
	STDMETHOD(SetFVF)(THIS_ DWORD FVF) PURE;
	STDMETHOD(GetFVF)(THIS_ DWORD * pFVF) PURE;
	STDMETHOD(CreateVertexShader)(THIS_ CONST DWORD * pFunction, IDirect3DVertexShader9 * *ppShader) PURE;
	STDMETHOD(SetVertexShader)(THIS_ IDirect3DVertexShader9 * pShader) PURE;
	STDMETHOD(GetVertexShader)(THIS_ IDirect3DVertexShader9 * *ppShader) PURE;
	STDMETHOD(SetVertexShaderConstantF)(THIS_ UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) PURE;
	STDMETHOD(GetVertexShaderConstantF)(THIS_ UINT StartRegister, float* pConstantData, UINT Vector4fCount) PURE;
	STDMETHOD(SetVertexShaderConstantI)(THIS_ UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount) PURE;
	STDMETHOD(GetVertexShaderConstantI)(THIS_ UINT StartRegister, int* pConstantData, UINT Vector4iCount) PURE;
	STDMETHOD(SetVertexShaderConstantB)(THIS_ UINT StartRegister, CONST BOOL * pConstantData, UINT  BoolCount) PURE;
	STDMETHOD(GetVertexShaderConstantB)(THIS_ UINT StartRegister, BOOL * pConstantData, UINT BoolCount) PURE;
	STDMETHOD(SetStreamSource)(THIS_ UINT StreamNumber, IDirect3DVertexBuffer9 * pStreamData, UINT OffsetInBytes, UINT Stride) PURE;
	STDMETHOD(GetStreamSource)(THIS_ UINT StreamNumber, IDirect3DVertexBuffer9 * *ppStreamData, UINT * pOffsetInBytes, UINT * pStride) PURE;
	STDMETHOD(SetStreamSourceFreq)(THIS_ UINT StreamNumber, UINT Setting) PURE;
	STDMETHOD(GetStreamSourceFreq)(THIS_ UINT StreamNumber, UINT * pSetting) PURE;
	STDMETHOD(SetIndices)(THIS_ IDirect3DIndexBuffer9 * pIndexData) PURE;
	STDMETHOD(GetIndices)(THIS_ IDirect3DIndexBuffer9 * *ppIndexData) PURE;
	STDMETHOD(CreatePixelShader)(THIS_ CONST DWORD * pFunction, IDirect3DPixelShader9 * *ppShader) PURE;
	STDMETHOD(SetPixelShader)(THIS_ IDirect3DPixelShader9 * pShader) PURE;
};