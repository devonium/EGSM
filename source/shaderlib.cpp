#include "shaderlib.h"
#include <GarrysMod/Lua/LuaInterface.h>
#include <mathlib/ssemath.h>
#include "f_mathlib.h"
#include <shaderapi/IShaderDevice.h>
#include <shaderapi/ishaderapi.h>
#include <shaderapi/ishaderdynamic.h>
#include <igamesystem.h>
#include <materialsystem/imesh.h>
#include <GarrysMod/Lua/LuaBase.h>
#include <lua.h>
#include <GarrysMod/Lua/Interface.h>
#include <lua_utils.h>
#include <GarrysMod/FactoryLoader.hpp>
#include "cpp_shader_constant_register_map.h"
#include <matsys_controls/matsyscontrols.h>
#include <materialsystem/imaterialsystem.h>
#include <BaseVSShader.h>
#include <ishadercompiledll.h>
#include <tier1/utllinkedlist.h>
#include "IShaderSystem.h"
#include <shaderlib/cshader.h>
#include "mathlib/vmatrix.h"
#include <globalvars_base.h>
#include <scanning/symbolfinder.hpp>
#include <detouring/hook.hpp>
#include <defs.h>
#include "vector"
#include "ps/aftershock_vs20.inc"
#include "d3dxdefs.h"
#include <utlhash.h>
#include <texture_group_names.h>
#include <e_utils.h>
#include "Windows.h"
#include "DepthWrite.h"
#include "chromium_fix.h"
#include "fviewrender.h"

#include <istudiorender.h>

using namespace GarrysMod::Lua;
extern IShaderSystem* g_pSLShaderSystem;
extern IMaterialSystemHardwareConfig* g_pHardwareConfig = NULL;
extern const MaterialSystem_Config_t* g_pConfig = NULL;

extern lua_State* g_pClientLua;


#ifndef  CHROMIUM
extern IMaterialSystem* g_pMaterialSystem = NULL;
extern IStudioRender* g_pStudioRender = NULL;
#endif

extern DepthWrite::CShader* g_DepthWriteShader;
extern ITexture* g_NormalsTex = NULL;
extern ITexture* g_DepthTex = NULL;
extern Vector skybox_origin = Vector();
extern int iSkyBoxScale = 1;

namespace ShaderLib
{
	int ShaderUData_typeID;
	IShaderDevice* g_pShaderDevice = NULL;
	IShaderAPI* g_pShaderApi = NULL;
	IShaderShadow* g_pShaderShadow = NULL;
	CShaderSystem* g_pCShaderSystem = NULL;
	CShaderSystem::ShaderDLLInfo_t* g_pShaderLibDLL = NULL;
	IDirect3DDevice9* m_pD3DDevice = NULL;

	int g_pShaderLibDLLIndex = 0;

	const char* g_sDigits[6] = { "0", "1", "2", "3", "4", "5" };

	void ShaderCompilationError(GarrysMod::Lua::ILuaBase* LUA, int comboInd, char* error, const char* name, IShaderBuffer** shaders)
	{
		if (shaders)
		{
			comboInd--;
			while (comboInd)
			{
				shaders[comboInd]->Release();
				comboInd--;
			}
		}
		delete shaders;

		lua_Debug* ar = luau_getcallinfo(LUA);
		// Using '%i' crashes the game for some fucking reason :|
		auto currline = std::to_string(ar->currentline);
		LUA->PushFormattedString("%s:%s: shader compilation has failed %s\n%s", ar->short_src, currline.c_str(), name, error);
		currline.~basic_string();

		delete error;
		LUA->Error();
	}

	lua_lib shaderlib;

	const char* param2str[] =
	{
			"SHADER_PARAM_TYPE_TEXTURE",
			"SHADER_PARAM_TYPE_FLOAT",
			"SHADER_PARAM_TYPE_COLOR",
			"SHADER_PARAM_TYPE_VEC2",
			"SHADER_PARAM_TYPE_VEC3",
			"SHADER_PARAM_TYPE_VEC4",
			"SHADER_PARAM_TYPE_ENVMAP (obsolete)",
			"SHADER_PARAM_TYPE_FLOAT",
			"SHADER_PARAM_TYPE_FLOAT",
			"SHADER_PARAM_TYPE_FOURCC",
			"SHADER_PARAM_TYPE_MATRIX",
			"SHADER_PARAM_TYPE_MATERIAL",
			"SHADER_PARAM_TYPE_STRING",
			"SHADER_PARAM_TYPE_MATRIX4X",
	};

	void SpewShader(IShader* shader)
	{
		static Color c1(41, 171, 207, 255);
		ConColorMsg(c1, "---%s---\n\n", shader->GetName());

		int longestName = 0;
		for (int parami = NUM_SHADER_MATERIAL_VARS; parami < shader->GetNumParams(); parami++)
		{
			int len = strlen(shader->GetParamName(parami));
			if (len > longestName)
			{
				longestName = len;
			}
		}

		for (int parami = NUM_SHADER_MATERIAL_VARS; parami < shader->GetNumParams(); parami++)
		{
			static Color c1(140, 236, 97, 255);
			ConColorMsg(c1, "%s ", shader->GetParamName(parami));

			for (int space = 0; space < (longestName - strlen(shader->GetParamName(parami))); space++)
			{
				Msg(" ");
			}
			static Color c2(97, 104, 236, 255);
			ConColorMsg(c2, " %s\n", param2str[shader->GetParamType(parami)]);
		}
		ConColorMsg(c1, "\n--------\n\n");
	}

	LUA_LIB_FUNCTION(shaderlib, SpewShaders)
	{
		for (int i = 0; i < g_pCShaderSystem->m_ShaderDLLs.Count(); i++)
		{
			auto dllinfo = &g_pCShaderSystem->m_ShaderDLLs[i];
			static Color c1(255, 50, 50, 255);
			ConColorMsg(c1, "%s\n", dllinfo->m_pFileName);

			for (int index = dllinfo->m_ShaderDict.Count(); --index >= 0; )
			{
				auto shader = dllinfo->m_ShaderDict.Element(index);
				SpewShader(shader);
			}
		}
		return 0;
	}

	LUA_LIB_FUNCTION(shaderlib, SpewShader)
	{
		auto name = LUA->CheckString();

		auto shader = g_pCShaderSystem->FindShader(name);
		if (!shader)
		{
			luau_error(LUA, "couldn't find '%s'", name);
		}
		SpewShader(shader);
		return 0;
	}

	LUA_LIB_FUNCTION(shaderlib, CompilePixelShader)
	{
		auto name = LUA->CheckString(1);
		int flags = LUA->CheckNumber(2);

		LUA->CheckType(3, Type::STRING);
		unsigned int programLen = 0;
		auto program = LUA->GetString(3, &programLen);

		CShaderManager::ShaderLookup_t lookup;
		lookup.m_Name = g_pShaderManager->m_ShaderSymbolTable.AddString(name);
		lookup.m_nStaticIndex = INT_MIN;

		auto index = GetPixelShaderDict().Find(lookup);
		if (index != GetPixelShaderDict().InvalidIndex())
		{
			auto hash = HashString(program);
			if (hash == GetPixelShaderDict()[index].m_nDataOffset)
			{
				return 0;
			}
			GetPixelShaderDict()[index].m_nDataOffset = hash;
		}

		HardwareShader_t* shaders = NULL;
		int combosCount = 10;
		int comboInd = 0;

		int _w, _h = 0;
		g_pMaterialSystem->GetBackBufferDimensions(_w, _h);
		std::string w = std::to_string(_w);
		std::string h = std::to_string(_h);

		D3DXMACRO ScreenSize = D3DXMACRO{ "SCR_S", "float2(SCR_W, SCR_H)" };
		D3DXMACRO NUM_LIGHTS = D3DXMACRO{ "NUM_LIGHTS", g_sDigits[0] };
		D3DXMACRO FLASHLIGHT = D3DXMACRO{ "FLASHLIGHT", g_sDigits[0] };
		D3DXMACRO SM = D3DXMACRO{ "SHADER_MODEL_PS_3_0", g_sDigits[1] };
#ifdef WIN64
		D3DXMACRO X64 = D3DXMACRO{ "X64", g_sDigits[1] };
#else
		D3DXMACRO X64 = D3DXMACRO{ "X64", g_sDigits[0] };
#endif
		D3DXMACRO ScreenW = D3DXMACRO{ "SCR_W", w.c_str() };
		D3DXMACRO ScreenH = D3DXMACRO{ "SCR_H", h.c_str() };

		std::vector<D3DXMACRO> macroDefines;
		macroDefines.push_back(NUM_LIGHTS);
		macroDefines.push_back(FLASHLIGHT);
		macroDefines.push_back(ScreenW);
		macroDefines.push_back(ScreenH);
		macroDefines.push_back(ScreenSize);
		macroDefines.push_back(SM);
		macroDefines.push_back(X64);
		macroDefines.push_back(D3DXMACRO_NULL);

		int lineOffset = 0;
		if (!LUA->IsType(4, Type::Bool) || LUA->GetBool(4))
		{
			lua_Debug* ar = luau_getcallinfo(LUA);
			lineOffset = *program == '\n' ? ar->currentline - 1 : ar->currentline;
		}

		for (int iFLASHLIGHT = 0; iFLASHLIGHT < 2; iFLASHLIGHT++)
		{
			for (int iNUM_LIGHTS = 0; iNUM_LIGHTS < 5; iNUM_LIGHTS++)
			{
				macroDefines[0].Definition = g_sDigits[iNUM_LIGHTS];
				macroDefines[1].Definition = g_sDigits[iFLASHLIGHT];

				IShaderBuffer* cs_output = NULL;
				if (!CompileShader(program, programLen, "ps_3_0", &macroDefines.front(), (void**)&cs_output, flags, lineOffset))
				{
					if (index != GetPixelShaderDict().InvalidIndex()) { GetPixelShaderDict()[index].m_nDataOffset = 0; }
					w.~basic_string();
					h.~basic_string();
					ShaderCompilationError(LUA, comboInd, (char*)cs_output, name, (IShaderBuffer**)shaders);
				}
				if (!shaders) { shaders = new HardwareShader_t[combosCount]; }
				shaders[comboInd] = cs_output;
				comboInd++;
			}
		}

		for (int i = 0; i < combosCount; i++)
		{
			IShaderBuffer* bf = (IShaderBuffer*)shaders[i];
			auto ind = (intp)g_pShaderManager->CreatePixelShader(bf);
			shaders[i] = g_pShaderManager->m_RawPixelShaderDict[ind];
			bf->Release();
		}

		if (index == GetPixelShaderDict().InvalidIndex())
		{
			lookup.m_ShaderStaticCombos.m_pHardwareShaders = shaders;
			lookup.m_ShaderStaticCombos.m_nCount = combosCount;
			GetPixelShaderDict()[GetPixelShaderDict().AddToTail(lookup)].m_nDataOffset = HashString(program);
		}
		else
		{
			auto lp = &GetPixelShaderDict()[index];
			auto combos = &lp->m_ShaderStaticCombos;
			for (int i = 0; i < combos->m_nCount; i++)
			{
				IDirect3DPixelShader9* pixelshader = (IDirect3DPixelShader9*)lp->m_ShaderStaticCombos.m_pHardwareShaders[i];
				g_pShaderManager->DestroyPixelShader((PixelShaderHandle_t)g_pShaderManager->m_RawPixelShaderDict.Find(pixelshader));
			}
			delete combos->m_pHardwareShaders;
			combos->m_pHardwareShaders = shaders;
			combos->m_nCount = combosCount;
		}

		return 0;
	}

	LUA_LIB_FUNCTION(shaderlib, CompileVertexShader)
	{
		auto name = LUA->CheckString(1);
		int flags = LUA->CheckNumber(2);

		LUA->CheckType(3, Type::STRING);
		unsigned int programLen = 0;
		auto program = LUA->GetString(3, &programLen);

		CShaderManager::ShaderLookup_t lookup;
		lookup.m_Name = g_pShaderManager->m_ShaderSymbolTable.AddString(name);
		lookup.m_nStaticIndex = INT_MIN;

		auto index = GetVertexShaderDict().Find(lookup);
		if (index != GetVertexShaderDict().InvalidIndex())
		{
			auto hash = HashString(program);
			if (hash == GetVertexShaderDict()[index].m_nDataOffset)
			{
				return 0;
			}
			GetVertexShaderDict()[index].m_nDataOffset = hash;
		}

		HardwareShader_t* shaders = NULL;

		int combosCount = 10;
		int comboInd = 0;

		D3DXMACRO NUM_LIGHTS = D3DXMACRO{ "NUM_LIGHTS", g_sDigits[0] };
		D3DXMACRO SKINNING = D3DXMACRO{ "SKINNING", g_sDigits[0] };
		D3DXMACRO SM = D3DXMACRO{ "SHADER_MODEL_VS_3_0", g_sDigits[1] };
		D3DXMACRO CV = D3DXMACRO{ "COMPRESSED_VERTS", g_sDigits[1] };

		std::vector<D3DXMACRO> macroDefines;
		macroDefines.push_back(NUM_LIGHTS);
		macroDefines.push_back(SKINNING);
		macroDefines.push_back(SM);
		macroDefines.push_back(CV);
		macroDefines.push_back(D3DXMACRO_NULL);

		int lineOffset = 0;
		if (!LUA->IsType(4, Type::Bool) || LUA->GetBool(4))
		{
			lua_Debug* ar = luau_getcallinfo(LUA);
			lineOffset = *program == '\n' ? ar->currentline - 1 : ar->currentline;
		}

		for (int iSKINNING = 0; iSKINNING < 2; iSKINNING++)
		{
			for (int iNUM_LIGHTS = 0; iNUM_LIGHTS < 5; iNUM_LIGHTS++)
			{
				macroDefines[0].Definition = g_sDigits[iNUM_LIGHTS];
				macroDefines[1].Definition = g_sDigits[iSKINNING];

				IShaderBuffer* cs_output = NULL;
				if (!CompileShader(program, programLen, "vs_3_0", &macroDefines.front(), (void**)&cs_output, flags, lineOffset))
				{
					if (index != GetVertexShaderDict().InvalidIndex()) { GetVertexShaderDict()[index].m_nDataOffset = 0; }
					GetVertexShaderDict()[index].m_nDataOffset = 0;
					ShaderCompilationError(LUA, comboInd, (char*)cs_output, name, (IShaderBuffer**)shaders);
				}
				if (!shaders) { shaders = new HardwareShader_t[combosCount]; }
				shaders[comboInd] = cs_output;
				comboInd++;
			}
		}

		for (int i = 0; i < combosCount; i++)
		{
			IShaderBuffer* bf = (IShaderBuffer*)shaders[i];
			shaders[i] = g_pShaderManager->m_RawVertexShaderDict[(intp)g_pShaderManager->CreateVertexShader(bf)];
			bf->Release();
		}

		if (index == GetVertexShaderDict().InvalidIndex())
		{
			lookup.m_ShaderStaticCombos.m_pHardwareShaders = shaders;
			lookup.m_ShaderStaticCombos.m_nCount = combosCount;
			GetVertexShaderDict()[GetVertexShaderDict().AddToTail(lookup)].m_nDataOffset = HashString(program);
		}
		else
		{
			auto lp = &GetVertexShaderDict()[index];
			auto combos = &lp->m_ShaderStaticCombos;
			for (int i = 0; i < combos->m_nCount; i++)
			{
				IDirect3DVertexShader9* pixelshader = (IDirect3DVertexShader9*)lp->m_ShaderStaticCombos.m_pHardwareShaders[i];
				g_pShaderManager->DestroyPixelShader((PixelShaderHandle_t)g_pShaderManager->m_RawVertexShaderDict.Find(pixelshader));
			}
			delete combos->m_pHardwareShaders;
			combos->m_pHardwareShaders = shaders;
			combos->m_nCount = combosCount;
		}

		return 0;
	}

	struct ShaderUData
	{
		ShaderLib::GShader* Shader = NULL;
	};

	LUA_LIB_FUNCTION(shaderlib, NewShader)
	{
		ShaderLib::GShader* shader = NULL;
		const char* shadername = LUA->CheckString(1);

		int ind = g_pShaderLibDLL->m_ShaderDict.Find(shadername);
		if (ind != g_pShaderLibDLL->m_ShaderDict.InvalidIndex())
		{
			shader = (ShaderLib::GShader*)g_pShaderLibDLL->m_ShaderDict.Element(ind);
		}
		else
		{
			shader = new ShaderLib::GShader();
			shader->s_Name = strdup(shadername);
			g_pShaderLibDLL->m_ShaderDict.Insert(shader->s_Name, shader);
		}

		if (shader->LUA)
		{
			LUA->ReferencePush(shader->UDataRef);
			return 1;
		}

		ShaderUData* u = LUA->NewUserType<ShaderUData>(ShaderUData_typeID);
		LUA->CreateMetaTable("Shader");
		LUA->SetMetaTable(-2);

		u->Shader = shader;
		shader->LUA = LUA;
		shader->UDataRef = LUA->ReferenceCreate();
		LUA->ReferencePush(shader->UDataRef);
		return 1;
	}

	lua_lib shader_methods;

	ShaderUData* GetUShader(GarrysMod::Lua::ILuaBase* LUA)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, ShaderUData_typeID + 0);
		if (!u)
		{
			LUA->TypeError(1, "Shader");
		}
		return u;
	}

	template <typename T>
	void SetConstant(ILuaBase* LUA, int index, ShaderConstantF::ShaderConstantType type, CUtlVector<ShaderConstantF*>& constants)
	{
		T* constant = NULL;
		for (int i = constants.Count(); --i >= 0; )
		{
			constant = (reinterpret_cast<T*>(constants.Element(i)));
			if (constant->Index == index)
			{
				goto setup;
			}
		}
		constant = new T();
		constants.AddToTail((ShaderConstantF*)constant);
	setup:
		constant->Type = type;
		constant->Index = index;
		for (int i = 0; i <= 3; i++)
		{
			constant->Vals[i] = LUA->IsType(i + 3, Type::BOOL) ? (LUA->GetBool(3 + i)) : LUA->GetNumber(3 + i);
		}
	}

	LUA_LIB_FUNCTION(shader_methods, SetPixelShaderConstant)
	{
		ShaderUData* u = GetUShader(LUA);
		int index = LUA->CheckNumber(2);
		SetConstant<ShaderConstantF>(LUA, index, ShaderConstantF::Float, u->Shader->PConstants);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetVertexShaderConstant)
	{
		ShaderUData* u = GetUShader(LUA);
		int index = LUA->CheckNumber(2);
		SetConstant<ShaderConstantF>(LUA, index, ShaderConstantF::Float, u->Shader->VConstants);
		return 0;
	}

	template <typename T>
	void SetConstantM(ILuaBase* LUA, int index, ShaderConstantF::ShaderConstantType type, CUtlVector<ShaderConstantF*>& constants)
	{
		LUA->CheckType(3, Type::MATRIX);
		float* matrix = (float*)((ILuaBase::UserData*)LUA->GetUserdata(3))->data;
		int rowsCount = 4;
		if (LUA->IsType(4, Type::NUMBER))
		{
			rowsCount = clamp<int>(LUA->CheckNumber(4), 1, 4);
		}

		T* constant = NULL;
		for (int i = constants.Count(); --i >= 0; )
		{
			constant = (reinterpret_cast<T*>(constants.Element(i)));
			if (constant->Index == index)
			{
				goto setup;
			}
		}
		constant = new T();
		constants.AddToTail((ShaderConstantF*)constant);
	setup:
		constant->Type = type;
		constant->Index = index;
		constant->RowsCount = rowsCount;
		for (int i = 0; i < (4 * rowsCount); i++)
		{
			constant->Vals[i] = matrix[i];
		}
	}

	LUA_LIB_FUNCTION(shader_methods, SetPixelShaderConstantM)
	{
		ShaderUData* u = GetUShader(LUA);
		int index = LUA->CheckNumber(2);
		SetConstantM<ShaderConstantM>(LUA, index, ShaderConstantF::Matrix, u->Shader->PConstants);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetVertexShaderConstantM)
	{
		ShaderUData* u = GetUShader(LUA);
		int index = LUA->CheckNumber(2);
		SetConstantM<ShaderConstantM>(LUA, index, ShaderConstantF::Matrix, u->Shader->VConstants);
		return 0;
	}

	void SetConstantFP(ILuaBase* LUA, int index, int paramindex, CUtlVector<ShaderConstantF*>& constants)
	{
		int rowsCount = 4;
		if (LUA->IsType(4, Type::NUMBER))
		{
			rowsCount = clamp<int>(LUA->CheckNumber(4), 1, 4);
		}
		ShaderConstantP* constant = NULL;
		for (int i = constants.Count(); --i >= 0; )
		{
			constant = (reinterpret_cast<ShaderConstantP*>(constants.Element(i)));
			if (constant->Index == index)
			{
				goto setup;
			}
		}
		constant = new ShaderConstantP();
		constants.AddToTail((ShaderConstantF*)constant);
		constant->IsValid = 2;
		constant->IsStandard = false;
	setup:
		constant->Type = ShaderConstantF::Param;
		constant->Index = index;
		if (constant->Param != paramindex) { constant->IsValid = 2; }
		constant->Param = paramindex;
	}

	void AttemptToSetError(ILuaBase* LUA)
	{
		luau_error(LUA, "attempt to set a missing parameter", 0)
	}

	LUA_LIB_FUNCTION(shader_methods, SetPixelShaderConstantFP)
	{
		ShaderUData* u = GetUShader(LUA);
		int index = LUA->CheckNumber(2);
		int paramindex = LUA->CheckNumber(3);
		if (paramindex > (u->Shader->GetNumParams() - 1)) { AttemptToSetError(LUA); return 0; }
		SetConstantFP(LUA, index, paramindex, u->Shader->PConstants);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetVertexShaderConstantFP)
	{
		ShaderUData* u = GetUShader(LUA);
		int index = LUA->CheckNumber(2);
		int paramindex = LUA->CheckNumber(3);
		if (paramindex > (u->Shader->GetNumParams() - 1)) { AttemptToSetError(LUA); return 0; }
		SetConstantFP(LUA, index, paramindex, u->Shader->VConstants);
		return 0;
	}

	void SetStandardConstant(int index, int constanttype, CUtlVector<ShaderConstantF*>& constants)
	{
		ShaderConstantP* constant = NULL;
		for (int i = constants.Count(); --i >= 0; )
		{
			constant = (reinterpret_cast<ShaderConstantP*>(constants.Element(i)));
			if (constant->Index == index)
			{
				goto setup;
			}
		}
		constant = new ShaderConstantP();
		constants.AddToTail((ShaderConstantF*)constant);
		constant->IsValid = 1;
		constant->IsStandard = true;
	setup:
		constant->Type = ShaderConstantF::Param;
		constant->Index = index;
		constant->Param = min(max(constanttype, ShaderConstantP::EYE_POS), ShaderConstantP::__Total__);
	}

	LUA_LIB_FUNCTION(shader_methods, SetPixelShaderStandardConstant)
	{
		ShaderUData* u = GetUShader(LUA);
		SetStandardConstant(LUA->CheckNumber(2), LUA->CheckNumber(3), u->Shader->PConstants);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetVertexShaderStandardConstant)
	{
		ShaderUData* u = GetUShader(LUA);
		SetStandardConstant(LUA->CheckNumber(2), LUA->CheckNumber(3), u->Shader->VConstants);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, AddParam)
	{
		ShaderUData* u = GetUShader(LUA);
		const char* name = LUA->CheckString(2);
		int type = max(min(LUA->CheckNumber(3), SHADER_PARAM_TYPE_MATRIX4X2), SHADER_PARAM_TYPE_TEXTURE);
		const char* defvalue = LUA->GetString(4);
		if (defvalue)
		{
			defvalue = strdup(defvalue);
		}

		ShaderLib::CShaderParam* param = NULL;
		for (int i = u->Shader->s_ShaderParams.Count(); --i >= 0; )
		{
			param = u->Shader->s_ShaderParams.Element(i);
			if (strcmp(param->GetName(), name) == 0)
			{
				param->m_Info.m_Type = (ShaderParamType_t)type;
				if (param->m_Info.m_pDefaultValue)
				{
					delete param->m_Info.m_pDefaultValue;
				}
				param->m_Info.m_pDefaultValue = defvalue;
				LUA->PushNumber(param->operator int());
				return 1;
			}
		}

		param = new ShaderLib::CShaderParam(u->Shader->s_ShaderParams, strdup(name), (ShaderParamType_t)type, defvalue, "", 0);
		LUA->PushNumber(param->operator int());
		return 1;
	}

	LUA_LIB_FUNCTION(shader_methods, SetParamDefValue)
	{
		ShaderUData* u = GetUShader(LUA);
		int paramindex = LUA->CheckNumber(2);
		if (paramindex > (u->Shader->GetNumParams() - 1)) { luau_error(LUA, "attempt to use a missing parameter", 0) return 0; }
		const char* defvalue = LUA->CheckString(3);
		if (defvalue)
		{
			defvalue = strdup(defvalue);
		}

		if (paramindex > NUM_SHADER_MATERIAL_VARS)
		{
			delete u->Shader->s_ShaderParams[paramindex]->m_Info.m_pDefaultValue;
			u->Shader->s_ShaderParams[paramindex]->m_Info.m_pDefaultValue = defvalue;
			return 0;
		}

		auto param = u->Shader->s_pShaderParamOverrides[paramindex];
		if (!param)
		{
			u->Shader->s_pShaderParamOverrides[paramindex];
			param = new ShaderLib::CShaderParam(u->Shader->s_pShaderParamOverrides, (ShaderMaterialVars_t)paramindex, (ShaderParamType_t)0, defvalue, 0, 0);
			return 0;
		}

		delete param->m_Info.m_pDefaultValue;
		param->m_Info.m_pDefaultValue = defvalue;

		return 0;
	}

	void BindTexture(ShaderUData* u, int sampler, int paramindex, TextureBind::TextureType type, bool isStandardTexture = false)
	{
		if (sampler > u->Shader->ActiveSamplers) { u->Shader->ActiveSamplers = sampler; }

		for (int i = u->Shader->Binds.Count(); --i >= 0; )
		{
			TextureBind* bind = u->Shader->Binds.Element(i);
			if (bind->Sampler == sampler)
			{
				bind->ParamIndex = paramindex;
				bind->Type = type;
				bind->IsStandardTexture = isStandardTexture;
				return;
			}
		}
		TextureBind* bind = new TextureBind();
		bind->ParamIndex = paramindex;
		bind->Type = type;
		bind->Sampler = sampler;
		bind->IsStandardTexture = isStandardTexture;
		u->Shader->Binds.AddToTail(bind);
	}

	LUA_LIB_FUNCTION(shader_methods, BindTexture)
	{
		ShaderUData* u = GetUShader(LUA);
		int sampler = LUA->CheckNumber(2);
		int paramindex = LUA->CheckNumber(3);
		BindTexture(u, sampler, paramindex, TextureBind::TextureType::Texture);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, BindStandardTexture)
	{
		ShaderUData* u = GetUShader(LUA);
		int sampler = LUA->CheckNumber(2);
		int paramindex = min(max(LUA->CheckNumber(3), StandardTextureId_t::TEXTURE_LIGHTMAP), StandardTextureId_t::TEXTURE_DEBUG_LUXELS);
		BindTexture(u, sampler, paramindex, TextureBind::TextureType::Texture, true);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, BindCubeMap)
	{
		ShaderUData* u = GetUShader(LUA);
		int sampler = LUA->CheckNumber(2);
		int paramindex = LUA->CheckNumber(3);
		BindTexture(u, sampler, paramindex, TextureBind::TextureType::Cubemap);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, BindBumpMap)
	{
		ShaderUData* u = GetUShader(LUA);
		int sampler = LUA->CheckNumber(2);
		int paramindex = LUA->CheckNumber(3);
		BindTexture(u, sampler, paramindex, TextureBind::TextureType::Bumpmap);
		return 0;
	}

	CShaderManager::ShaderLookup_t* FindShader(const char* name, CUtlFixedLinkedList<CShaderManager::ShaderLookup_t>& list)
	{
		intp pshIndex = list.Head();
		while (pshIndex != list.InvalidIndex())
		{
			CShaderManager::ShaderLookup_t* lookup = &list[pshIndex];
			pshIndex = list.Next(pshIndex);
			if (lookup->m_nStaticIndex == INT_MIN && strcmp(g_pShaderManager->m_ShaderSymbolTable.String(lookup->m_Name), name) == 0)
			{
				return lookup;
			}
		}
		return NULL;
	}

	LUA_LIB_FUNCTION(shader_methods, SetPixelShader)
	{
		ShaderUData* u = GetUShader(LUA);
		const char* name = LUA->CheckString(2);
		auto shader = FindShader(name, GetPixelShaderDict());
		if (!shader)
		{
			luau_error(LUA, "attempt to set a missing pixel shader(%s)", name);
		}
		if (u->Shader->PShader) { delete u->Shader->PShader; }
		shader->m_Flags = 0;
		u->Shader->PShader = strdup(name);

		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetVertexShader)
	{
		ShaderUData* u = GetUShader(LUA);
		const char* name = LUA->CheckString(2);
		auto shader = FindShader(name, GetVertexShaderDict());
		if (!shader)
		{
			luau_error(LUA, "attempt to set a missing vertex shader(%s)", name);
		}
		if (u->Shader->VShader) { delete u->Shader->VShader; }
		shader->m_Flags = 0;
		u->Shader->VShader = strdup(name);

		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetFlags)
	{
		ShaderUData* u = GetUShader(LUA);
		u->Shader->Flags = LUA->CheckNumber(2);
		u->Shader->UpdateFlags = true;
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetFlags2)
	{
		ShaderUData* u = GetUShader(LUA);
		u->Shader->Flags2 = LUA->CheckNumber(2);
		u->Shader->UpdateFlags = true;
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetCullMode)
	{
		ShaderUData* u = GetUShader(LUA);
		u->Shader->CullMode = (MaterialCullMode_t)max(min((int)LUA->CheckNumber(2), MATERIAL_CULLMODE_CW), MATERIAL_CULLMODE_CCW);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetPolyMode)
	{
		ShaderUData* u = GetUShader(LUA);
		u->Shader->PolyMode = (ShaderPolyMode_t)max(min((int)LUA->CheckNumber(2), SHADER_POLYMODEFACE_FRONT_AND_BACK), SHADER_POLYMODEFACE_FRONT);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, OverrideBlending)
	{
		ShaderUData* u = GetUShader(LUA);
		u->Shader->OverrideBlending = LUA->GetBool(2);
		u->Shader->BlendSrc = (ShaderBlendFactor_t)LUA->CheckNumber(3);
		u->Shader->BlendDst = (ShaderBlendFactor_t)LUA->CheckNumber(4);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, EnableFlashlightSupport)
	{
		ShaderUData* u = GetUShader(LUA);
		u->Shader->SupportsFlashlight = LUA->GetBool(2);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, EnableStencil)
	{
		ShaderUData* u = GetUShader(LUA);
		u->Shader->IsStencilEnabled = LUA->GetBool(2);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, GetName)
	{
		ShaderUData* u = GetUShader(LUA);
		LUA->PushString(u->Shader->s_Name);
		return 1;
	}

	LUA_LIB_FUNCTION(shader_methods, SetStencilFailOperation)
	{
		ShaderUData* u = GetUShader(LUA);
		u->Shader->StencilFailOp = (StencilOperation_t)LUA->CheckNumber(2);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetStencilZFailOperation)
	{
		ShaderUData* u = GetUShader(LUA);
		u->Shader->StencilZFail = (StencilOperation_t)LUA->CheckNumber(2);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetStencilPassOperation)
	{
		ShaderUData* u = GetUShader(LUA);
		u->Shader->StencilPassOp = (StencilOperation_t)LUA->CheckNumber(2);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetStencilCompareFunction)
	{
		ShaderUData* u = GetUShader(LUA);
		u->Shader->StencilCompareFunc = (StencilComparisonFunction_t)LUA->CheckNumber(2);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetStencilReferenceValue)
	{
		ShaderUData* u = GetUShader(LUA);
		u->Shader->StencilReferenceValue = LUA->CheckNumber(2);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetStencilTestMask)
	{
		ShaderUData* u = GetUShader(LUA);
		u->Shader->StencilTestMask = LUA->CheckNumber(2);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetStencilWriteMask)
	{
		ShaderUData* u = GetUShader(LUA);
		u->Shader->StencilWriteMask = LUA->CheckNumber(2);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetRenderTarget)
	{
		ShaderUData* u = GetUShader(LUA);
		int ind = LUA->CheckNumber(2);
		if (ind < 0 || ind > 3)
		{
			LUA->ThrowError("rt index out of valid range [0-3]");
		}

		const char* name = LUA->GetString(3);
		auto tex = g_pMaterialSystem->FindTexture(name, TEXTURE_GROUP_RENDER_TARGET);

		if (!tex || tex->IsError()) { LUA->ThrowError("provided texture doens't exist"); }
		if (!tex->IsRenderTarget()) { LUA->ThrowError("provided texture is not a render target"); }

		u->Shader->rts[ind] = tex;
		return 0;
	}

	bool bDepthPass = false;

#define MK_CV(name) ConVar* name = NULL; bool name##_OV = 0;
#define RES_CV(name) name->SetValue(name##_OV);
#define SET_CV(name) name##_OV = name->GetBool(); name->SetValue(false);
	MK_CV(mat_drawwater)
		MK_CV(r_drawtranslucentworld)


		LUA_LIB_FUNCTION(shaderlib, BeginDepthPass)
	{
		auto pMatRenderContext = g_pMaterialSystem->GetRenderContext();
		pMatRenderContext->PushRenderTargetAndViewport(g_NormalsTex);
		pMatRenderContext->ClearBuffers(true, true);

		pMatRenderContext->PushRenderTargetAndViewport(g_DepthTex);
		pMatRenderContext->ClearBuffers(true, true);

		g_pStudioRender->ForcedMaterialOverride(NULL, OVERRIDE_SSAO_DEPTH_WRITE);

		SET_CV(mat_drawwater);
		SET_CV(r_drawtranslucentworld);

		bDepthPass = true;
		return 0;
	}

	LUA_LIB_FUNCTION(shaderlib, EndDepthPass)
	{
		g_pStudioRender->ForcedMaterialOverride(0);

		RES_CV(mat_drawwater);
		RES_CV(r_drawtranslucentworld);

		bDepthPass = false;
		return 0;
	}

	Define_method_Hook(VertexShader_t, CShaderManager_CreateVertexShader, CShaderManager*, const char* pVertexShaderFile, int nStaticVshIndex, char* debugLabel)
	{
		if (!g_pShaderManager)
		{
			g_pShaderManager = _this;
#ifndef WIN64
			CShaderManager_CreateVertexShader_hook.Destroy();
#endif
			return  (int)INVALID_SHADER;
		}

		CShaderManager::ShaderLookup_t lookup;
		lookup.m_Name = g_pShaderManager->m_ShaderSymbolTable.AddString(pVertexShaderFile);
		lookup.m_nStaticIndex = nStaticVshIndex;
		auto l = GetVertexShaderDict().Find(lookup);
		if (l != GetVertexShaderDict().InvalidIndex())
		{
			return l;
		}

		return CShaderManager_CreateVertexShader_trampoline()(_this, pVertexShaderFile, nStaticVshIndex, debugLabel);
	}

#ifdef WIN64

	Define_method_Hook(VertexShader_t, CShaderManager_CreatePixelShader, CShaderManager*, const char* pVertexShaderFile, int nStaticVshIndex, char* debugLabel)
	{
		CShaderManager::ShaderLookup_t lookup;
		lookup.m_Name = g_pShaderManager->m_ShaderSymbolTable.AddString(pVertexShaderFile);
		lookup.m_nStaticIndex = nStaticVshIndex;
		auto l = GetPixelShaderDict().Find(lookup);
		//Msg("%s %i %i\n", pVertexShaderFile, nStaticVshIndex, l == GetPixelShaderDict().InvalidIndex());
		if (l != GetPixelShaderDict().InvalidIndex())
		{
			return l;
		}

		return CShaderManager_CreatePixelShader_trampoline()(_this, pVertexShaderFile, nStaticVshIndex, debugLabel);

	}

	Define_method_Hook(void, CShaderManager_SetVertexShader, CShaderManager*, VertexShader_t shader)
	{
		if (b_isEgsmShader)
		{
			CShaderManager::ShaderLookup_t& lookup = GetVertexShaderDict()[shader];
			HardwareShader_t& dxshader = lookup.m_ShaderStaticCombos.m_pHardwareShaders[_this->m_nVertexShaderIndex];
			m_pD3DDevice->SetVertexShader((IDirect3DPixelShader9*)dxshader);
			_this->m_HardwareVertexShader = dxshader;
			return;
		}
		CShaderManager_SetVertexShader_trampoline()(_this, shader);
		return;
	}

	Define_method_Hook(void, CShaderManager_SetPixelShader, CShaderManager*, PixelShader_t shader)
	{
		if (b_isEgsmShader)
		{
			CShaderManager::ShaderLookup_t& lookup = GetPixelShaderDict()[shader];
			HardwareShader_t& dxshader = lookup.m_ShaderStaticCombos.m_pHardwareShaders[_this->m_nVertexShaderIndex];
			m_pD3DDevice->SetPixelShader((IDirect3DVertexShader9*)dxshader);
			_this->m_HardwarePixelShader = dxshader;
			return;

		}

		CShaderManager_SetPixelShader_trampoline()(_this, shader);
		return;
	}

#endif

	Define_method_Hook(void, CShaderManager_PurgeUnusedVertexAndPixelShaders, CShaderManager*)
	{
		if (!g_pShaderManager) { return; }
		return;
	}

	Define_method_Hook(void, RT_ClearColor4ub, void*, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
	{
		if (bDepthPass)
		{
			return;
		}

		RT_ClearColor4ub_trampoline()(_this, r, g, b, a);
	}



	IMaterial* transpMat = NULL;
	IMaterial* depthMat = NULL;

	Define_method_Hook(void, CMatRenderContextBase_Bind, void*, IMaterial* mat, void* data)
	{
		if (bDepthPass && strcmp(mat->GetShaderName(), "DepthWrite") != 0)
		{
			CMatRenderContextBase_Bind_trampoline()(_this, transpMat, NULL);
			return;
		}
		CMatRenderContextBase_Bind_trampoline()(_this, mat, data);
	}

	Define_method_Hook(void, CShaderSystem_InitShaderParameters, CShaderSystem*, IShader* pShader, IMaterialVar** params, const char* pMaterialName)
	{
		//Msg("%s %i\n", pShader->GetName(), pShader->GetFlags());
		if (pShader->GetFlags() == ShaderLib::s_nFlags)
		{
			for (int i = 0; i < pShader->GetNumParams(); i++)
			{
				if (!params[i]->IsDefined() && pShader->GetParamDefault(i))
				{
					params[i]->SetValueAutodetectType(pShader->GetParamDefault(i));
					//Msg("\t%s %s %s\n", pShader->GetParamName(i), pShader->GetParamDefault(i), params[i]->GetStringValue());
				}
			}
		}
		CShaderSystem_InitShaderParameters_trampoline()(_this, pShader, params, pMaterialName);
		return;
	}

	Define_method_Hook(IMaterial*, R_StudioSetupSkinAndLighting, void*, IMatRenderContext* pRenderContext, int index, IMaterial** ppMaterials, int materialFlags,
		void /*IClientRenderable*/* pClientRenderable, void* pColorMeshes, void* lighting)
	{
		if (bDepthPass)
		{
			g_pStudioRender->ForcedMaterialOverride(NULL, OVERRIDE_SSAO_DEPTH_WRITE);

		}
		IMaterial* ret = R_StudioSetupSkinAndLighting_trampoline()(_this, pRenderContext, index, ppMaterials, materialFlags, pClientRenderable, pColorMeshes, lighting);
		return ret;
	}

	Define_method_Hook(void, CBaseWorldView_SSAO_DepthPass, void*)
	{
		return;
	}

	Define_method_Hook(void, CSkyboxView_Draw, CSkyboxView*)
	{
		if (bDepthPass)
		{
			skybox_origin = _this->m_pSky3dParams->origin;
			if (_this->m_pSky3dParams->scale > 0)
			{
				iSkyBoxScale = _this->m_pSky3dParams->scale;
			}
			else
			{
				iSkyBoxScale = 1;
			}
		}

		CSkyboxView_Draw_trampoline()(_this);

		return;
	}

	class FUCKOOPConVar : public ConVar
	{
	public:
		void SETFUCKINGMAX()
		{
			this->m_bHasMax = true;
			this->m_fMaxVal = 0;			
			
			this->m_bHasMin = true;
			this->m_fMinVal = 0;
		}
	};

	int MenuInit(GarrysMod::Lua::ILuaBase* LUA)
	{
		static Color msgc(100, 255, 100, 255);
		ConColorMsg(msgc, "ShaderLib\n");

		if (!Sys_LoadInterface("materialsystem", "MaterialSystemHardwareConfig012", NULL, (void**)&g_pHardwareConfig)) { ShaderLibError("MaterialSystemHardwareConfig012 == NULL"); }
		if (g_pHardwareConfig->GetDXSupportLevel() < 90) { ShaderLibError("Unsupported DirectX version(dxlevel<90)"); };
		if (!Sys_LoadInterface("materialsystem", "ShaderSystem002", NULL, (void**)&g_pSLShaderSystem)) { ShaderLibError("IShaderSystem == NULL"); }
		if (!Sys_LoadInterface("materialsystem", "VMaterialSystemConfig002", NULL, (void**)&g_pConfig)) { ShaderLibError("VMaterialSystemConfig002 == NULL"); }
		if (!Sys_LoadInterface("materialsystem", MATERIAL_SYSTEM_INTERFACE_VERSION, NULL, (void**)&g_pMaterialSystem)) { ShaderLibError("VMaterialSystem == NULL"); }
		if (!Sys_LoadInterface("studiorender", STUDIO_RENDER_INTERFACE_VERSION, NULL, (void**)&g_pStudioRender)) { ShaderLibError("IStudioRender == NULL"); }

		g_pShaderShadow = (IShaderShadow*)(g_pMaterialSystem->QueryInterface(SHADERSHADOW_INTERFACE_VERSION));
		g_pShaderApi = (IShaderAPI*)(g_pMaterialSystem->QueryInterface(SHADERAPI_INTERFACE_VERSION));
		g_pShaderDevice = (IShaderDevice*)(g_pMaterialSystem->QueryInterface(SHADER_DEVICE_INTERFACE_VERSION));
		static SourceSDK::FactoryLoader icvar_loader("vstdlib", true, IS_SERVERSIDE, "bin/");
		g_pCVar = icvar_loader.GetInterface<ICvar>(CVAR_INTERFACE_VERSION);

		if (!g_pShaderShadow) { ShaderLibError("IShaderShadow == NULL"); }
		if (!g_pShaderDevice) { ShaderLibError("IShaderDevice == NULL"); }
		if (!g_pShaderApi) { ShaderLibError("IShaderAPI == NULL"); }
		if (!g_pCVar) { ShaderLibError("g_pCVar == NULL"); }

		auto shaderapidx = GetModuleHandle("shaderapidx9.dll");
		if (!shaderapidx) { ShaderLibError("shaderapidx9.dll == NULL\n"); }

		auto materialsystemdll = GetModuleHandle("materialsystem.dll");
		if (!materialsystemdll) { ShaderLibError("materialsystem.dll == NULL\n"); }

		auto studiorenderdll = GetModuleHandle("studiorender.dll");
		if (!studiorenderdll) { ShaderLibError("studiorender.dll == NULL\n"); }

		auto d3dx9_40 = GetModuleHandle("D3DX9_40.dll");
		if (!d3dx9_40)
		{
			d3dx9_40 = GetModuleHandle("D3DX9_42.dll");
			if (!d3dx9_40)
			{
				ShaderLibError("D3DX9_40.dll == NULL\nD3DX9_42.dll == NULL\n");
			}
		}

		{
			static const char sign[] =
				HOOK_SIGN_CHROMIUM_x32("55 8B EC 83 EC 1C 53 8B 5D 0C C6 45 FE 00 56 8B F1 89 75 F8 57 8B 46 04 8A 88 50 02 00 00 8B B8 4C 02 00 00 88 4D FF 83 FB 1F 77 17 8B 94 98 54 02 00 00 85 D2 74 0C 8A 8C 18 D4 02 00 00 8B FA 88 4D FF 80 78 24 00 7D 23 85 FF 75 1F F6 40 25 04 8A 7D FE 74 0B")
				HOOK_SIGN_CHROMIUM_x64("48 89 5C 24 20 48 89 54 24 10 48 89 4C 24 08 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 50 48 8B 41 08 45 32 F6 49 63 F0 4D 8B E1 48 8B EA 4C 8B F9 48 8B B8 50 02 00 00 44 0F B6 A8 58 02 00 00 83 FE 1F 77 19 4C 8B 84 F0 60 02 00 00 4D 85 C0 74 0C 44 0F B6 AC ")
				HOOK_SIGN_x32("55 8B EC 83 EC 1C 53 8B 5D 0C C6 45 FE 00 56 8B F1 89 75 F8 57 8B 46 04 8A 88 50 02 00 00 8B B8 4C 02 00 00 88 4D FF 83 FB 1F 77 17 8B 94 98 54 02 00 00 85 D2 74 0C 8A 8C 18 D4 02 00 00 8B FA 88 4D FF 80 78 24 00 7D 23 85 FF 75 1F F6 40 25 04 8A 7D FE 74 0B 8B BE C0 07 00 00 E9 ? ? ? ? 8B BE BC 07")

				R_StudioSetupSkinAndLighting_decl R_StudioSetupSkinAndLighting = (R_StudioSetupSkinAndLighting_decl)ScanSign(studiorenderdll, sign, sizeof(sign) - 1);

			if (!R_StudioSetupSkinAndLighting) { ShaderLibError("R_StudioSetupSkinAndLighting == NULL\n"); return 0; }

			Setup_Hook(R_StudioSetupSkinAndLighting, R_StudioSetupSkinAndLighting)
		}

		{
			static const char sign[] =
				HOOK_SIGN_CHROMIUM_x64("48 8B C4 48 89 58 18 55 56 41 54 41 56 41 57 48 83 EC 60 4D 8B F9 41 8B F0 4C 8B F1 48 85 D2 0F 84 ? ? ? ? 0F 57 C0 4C 8B C2 45 33 E4 66 0F 7F 40 A8 48 8D 50 10 44 89 60 A0 48 81 C1 B0 00 00 00 E8 ? ? ? ? 49 8B 5E 20 0F B7 28 48 85 DB 74 17 66 39 2B 75 09 39 73 04 0F 84 ? ? ? ? 48 8B 5B 48 48")
				HOOK_SIGN_x32("55 8B EC 8B 45 08 83 EC 28 56 57 8B F9 85 C0 0F 84 ? ? ? ? 50 8D 45 0A C7 45 EC 00 00 00 00 50 8D 4F 64 C7 45 F0 00 00 00 00 C7 45 E0 00 00 00 00 C7 45 E4 00 00 00 00 C7 45 E8 00 00 00 00 C7 45 FC 00 00 00 00 E8 ? ? ? ? 8B 77 18 8B 4D 0C 89 4D DC 66 8B 00 66 89 45 D8 85 F6 74 11 66 39 06 75 05")

				CShaderManager_CreateVertexShader_decl CShaderManager_CreateVertexShader = (CShaderManager_CreateVertexShader_decl)ScanSign(shaderapidx, sign, sizeof(sign) - 1);
			if (!CShaderManager_CreateVertexShader) { ShaderLibError("CShaderManager::SetVertexShader == NULL\n"); return 0; }

			Setup_Hook(CShaderManager_CreateVertexShader, CShaderManager_CreateVertexShader)
				g_pShaderShadow->SetVertexShader("HEREHEREHEREHERE", 1010);
			//CShaderManager_CreateVertexShader_hook.Disable();
			if (!g_pShaderManager) { ShaderLibError("g_pShaderManager == NULL\n"); return 0; }

#ifdef WIN64
			{
				Setup_Hook(CShaderManager_CreatePixelShader, GetVTable(g_pShaderManager)[6]);
				Setup_Hook(CShaderManager_SetVertexShader, GetVTable(g_pShaderManager)[9]);
				Setup_Hook(CShaderManager_SetPixelShader, GetVTable(g_pShaderManager)[10]);

				static const char sign[] = "BA E1 0D 74 5E 48 89 1D ?? ?? ?? ??";
				auto ptr = ScanSign(shaderapidx, sign, sizeof(sign) - 1);
				if (!ptr) { ShaderLibError("g_pD3D9Device sig == NULL\n"); }

				auto offset = ((uint32_t*)ptr)[2];
				m_pD3DDevice = *(IDirect3DDevice9**)((char*)ptr + offset + 12);
				if (!m_pD3DDevice) { ShaderLibError("m_pD3DDevice == NULL\n"); }
			}
#endif

		}

		D3DXCompileShader = (D3DXCompileShaderDecl*)GetProcAddress(d3dx9_40, "D3DXCompileShader");
		if (!D3DXCompileShader) { ShaderLibError("D3DXCompileShader == NULL\n"); return 0; }

		g_pCShaderSystem = (CShaderSystem*)g_pSLShaderSystem;

		auto clientdll = GetModuleHandle("client.dll");
		if (!clientdll) { ShaderLibError("client.dll == NULL\n"); }

		{
			static const char sign[] =
				HOOK_SIGN_x32("55 8B EC 83 EC 08 53 8B D9 8B ? ? ? ? ? 8B 01 8B 40 2C FF D0 84 C0 0F 84 ? ? ? ? 8B ? ? ? ? ? 8B 81 0C 10 00 00 89 45 F8 85 C0 74 16 6A 04 6A 00 68 ? ? ? ? 6A 00 68 ? ? ? ? FF 15 ? ? ? ? 8B")
				HOOK_SIGN_x64("40 56 48 83 EC 30 48 8B F1 48 8B ? ? ? ? ? 48 8B 01 FF 50 58 84 C0 0F 84 ? ? ? ? 48 8B ? ? ? ? ? 48 89 5C 24 40 48 89 6C 24 48 48 89 7C 24 50 8B A9 0C 10 00 00 4C 89 74 24 58 85 ED 74 24 C7")

				void* DepthPass = ScanSign(clientdll, sign, sizeof(sign) - 1);
			if (!DepthPass) { ShaderLibError("CBaseWorldView::SSAO_DepthPass == NULL\n"); return 0; }
			Setup_Hook(CBaseWorldView_SSAO_DepthPass, DepthPass)
		}

		{
			static const char sign[] =
				HOOK_SIGN_M("56 57 8B F9 8B ? ? ? ? ? 8B B1 0C 10 00 00 85 F6 74 16 6A 04 6A 00 68 ? ? ? ? 6A 00 68 ? ? ? ? FF 15 ? ? ? ? 6A 00 6A 01 6A 01 8B CF E8")
				HOOK_SIGN_CHROMIUM_x32("56 57 8B F9 8B ? ? ? ? ? 8B B1 0C 10 00 00 85 F6 74 16 6A 04 6A 00 68 ? ? ? ? 6A 00 68 ? ? ? ? FF 15 ? ? ? ? 6A 00 6A 01 6A 01")
				HOOK_SIGN_CHROMIUM_x64("48 89 5C 24 08 57 48 83 EC 30 48 8B F9 48 8B ? ? ? ? ? 8B 99 0C 10 00 00 85 DB 74 24 C7 44 24 28 04 00 00 00 4C 8D ? ? ? ? ? 45 33 C0 C6 44 24 20 00 48 8D ? ? ? ? ? FF 15 ? ? ? ? 45 33 C9 41 B0 01 48 8B CF 41 8D 51 01 E8 ? ? ? ? 85 DB 74 0D")


				void* CSkyboxView_Draw = ScanSign(clientdll, sign, sizeof(sign) - 1);
			if (!CSkyboxView_Draw) { ShaderLibError("CSkyboxView_Draw == NULL\n"); return 0; }
			Setup_Hook(CSkyboxView_Draw, CSkyboxView_Draw)
		}

		{
			static const char sign[] =
				HOOK_SIGN_x32("55 8B EC 8B ? ? ? ? ? 8B 01 5D FF A0 98 01 00 00")
				HOOK_SIGN_x64("55 8B EC 8B ? ? ? ? ? 8B 01 5D FF A0 98 01 00 00");

			//RT_ClearColor4ub_decl RT_ClearColor4ub = (RT_ClearColor4ub_decl)ScanSign(materialsystemdll, sign, sizeof(sign) - 1);
			//if (!RT_ClearColor4ub) { ShaderLibError("RT_ClearColor4ub == NULL\n"); return 0; }
			//
			//Setup_Hook(RT_ClearColor4ub, RT_ClearColor4ub)
		}

		{
			static const char sign[] =
				HOOK_SIGN_CHROMIUM_x32("55 8B EC 56 57 8B F9 8B 4D 08 85 C9 75 14 68 ? ? ? ? FF 15 ? ? ? ? 8B ? ? ? ? ? 83 C4 04 8B 01 FF 90 78 01 00 00 8B 17 8B CF 8B F0 FF 92 2C 03 00 00 3B C6 74 2F 8B 06 8B CE 8B 80 E0 00 00 00 FF D0 84 C0 75 14 8B 06 8B CE FF 90 5C 01 00 00")
				HOOK_SIGN_x32("55 8B EC 56 57 8B F9 8B 4D 08 85 C9 75 22 39 ? ? ? ? ? 0F 84 ? ? ? ? 68 ? ? ? ? FF 15 ? ? ? ? 8B ? ? ? ? ? 83 C4 04 EB 0D 8B 01 FF 75 0C FF 90 D4 00 00 00 8B F0 8B 06 8B CE 53 FF 90 78 01 00 00 8B D8 8B CB 8B 13 FF 92 E8 00 00 00 85 C0 0F 8F ? ? ? ? 8B")
				HOOK_SIGN_x64("48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 49 8B F0 48 8B F9 48 85 D2 75 14 48 8D ? ? ? ? ? FF 15 ? ? ? ? 48 8B ? ? ? ? ? 48 8B 02 48 8B CA FF 90 F0 02 00 00 48 8B 17 48 8B CF 48 8B D8 FF 92 58 06 00 00 48 3B C3 74 37 48 8B 13 48 8B CB FF 92 C0 01 00")

				void* Bind = ScanSign(materialsystemdll, sign, sizeof(sign) - 1);
			if (!Bind) { ShaderLibError("CMatRenderContextBase::Bind == NULL\n"); return 0; }
			Setup_Hook(CMatRenderContextBase_Bind, Bind)
		}

		Setup_Hook(CShaderManager_PurgeUnusedVertexAndPixelShaders, GetVTable(g_pShaderManager)[15])
			Setup_Hook(CShaderSystem_InitShaderParameters, GetVTable(g_pCShaderSystem)[14])

		g_pShaderLibDLLIndex = g_pCShaderSystem->m_ShaderDLLs.AddToTail();
		g_pShaderLibDLL = &g_pCShaderSystem->m_ShaderDLLs[g_pShaderLibDLLIndex];
		g_pShaderLibDLL->m_pFileName = strdup("egsm_shaders.dll");
		g_pShaderLibDLL->m_bModShaderDLL = true;
		//IShaderDynamicAPI* sapi = (IShaderDynamicAPI*)((IShaderAPI030*)(g_pShaderApi));

		for (int i = 0; i < g_pCShaderSystem->m_ShaderDLLs.Count(); i++)
		{
			auto shaderdll = &g_pCShaderSystem->m_ShaderDLLs[i];
			for (int index = shaderdll->m_ShaderDict.Count(); --index >= 0; )
			{
				auto shader = shaderdll->m_ShaderDict.Element(index);
				if (strcmp(shader->GetName(), "DepthWrite") == 0)
				{
					shaderdll->m_ShaderDict[index] = g_DepthWriteShader;
				}
			}
		}

		{
			auto pVMTKeyValues = new KeyValues("UnlitGeneric");
			pVMTKeyValues->SetString("$basetexture", "color/white");
			pVMTKeyValues->SetFloat("$alpha", 0);
			transpMat = g_pMaterialSystem->CreateMaterial("egsm/transpmat", pVMTKeyValues);
			transpMat->IncrementReferenceCount();
		}
		{
			auto pVMTKeyValues = new KeyValues("DepthWrite");
			pVMTKeyValues->SetString("$basetexture", "color/white");
			pVMTKeyValues->SetInt("$no_fullbright", 1);
			pVMTKeyValues->SetInt("$$color_depth", 1);
			pVMTKeyValues->SetInt("$alpha_test", 0);
			depthMat = g_pMaterialSystem->CreateMaterial("egsm/depthpmat", pVMTKeyValues);
			depthMat->IncrementReferenceCount();
		}
		mat_drawwater = g_pCVar->FindVar("mat_drawwater");
		r_drawtranslucentworld = g_pCVar->FindVar("r_drawtranslucentworld");
		auto mat_antialias = g_pCVar->FindVar("mat_antialias");
		mat_antialias->SetValue(0);
		((FUCKOOPConVar*)(mat_antialias))->SETFUCKINGMAX();

		ConColorMsg(msgc, "-Succ\n");
	}

	void LuaPostInit(GarrysMod::Lua::ILuaBase* LUA)
	{

	}

	void LuaInit(GarrysMod::Lua::ILuaBase* LUA)
	{
		g_pMaterialSystem->BeginRenderTargetAllocation();
		g_DepthTex = g_pMaterialSystem->CreateNamedRenderTargetTextureEx2("_rt_ResolvedFullFrameDepth", 1, 1,
			RT_SIZE_FULL_FRAME_BUFFER, IMAGE_FORMAT_RGBA32323232F, MATERIAL_RT_DEPTH_SEPARATE,
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_POINTSAMPLE,
			CREATERENDERTARGETFLAGS_NOEDRAM);
		g_DepthTex->IncrementReferenceCount();

		g_NormalsTex = g_pMaterialSystem->CreateNamedRenderTargetTextureEx2("_rt_Normals", 1, 1,
			RT_SIZE_FULL_FRAME_BUFFER, IMAGE_FORMAT_RGBA32323232F, MATERIAL_RT_DEPTH_SEPARATE,
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_POINTSAMPLE,
			CREATERENDERTARGETFLAGS_NOEDRAM);
		g_NormalsTex->IncrementReferenceCount();
		g_pMaterialSystem->EndRenderTargetAllocation();


		LUA->PushSpecial(SPECIAL_GLOB);
		LUA->CreateTable();
		luau_openlib(LUA, shaderlib, 0);
		LUA->SetField(-2, "shaderlib");
		LUA->Pop();

		LUA->PushSpecial(SPECIAL_GLOB);
		LUAUPushConst("", , PSREG_SELFILLUMTINT)
			LUAUPushConst("", , PSREG_DIFFUSE_MODULATION)
			LUAUPushConst("", , PSREG_ENVMAP_TINT__SHADOW_TWEAKS)
			LUAUPushConst("", , PSREG_SELFILLUM_SCALE_BIAS_EXP)
			LUAUPushConst("", , PSREG_AMBIENT_CUBE)
			LUAUPushConst("", , PSREG_ENVMAP_FRESNEL__SELFILLUMMASK)
			LUAUPushConst("", , PSREG_EYEPOS_SPEC_EXPONENT)
			LUAUPushConst("", , PSREG_FOG_PARAMS)
			LUAUPushConst("", , PSREG_FLASHLIGHT_ATTENUATION)
			LUAUPushConst("", , PSREG_FLASHLIGHT_POSITION_RIM_BOOST)
			LUAUPushConst("", , PSREG_FLASHLIGHT_TO_WORLD_TEXTURE)
			LUAUPushConst("", , PSREG_FRESNEL_SPEC_PARAMS)
			LUAUPushConst("", , PSREG_LIGHT_INFO_ARRAY)
			LUAUPushConst("", , PSREG_SPEC_RIM_PARAMS)
			LUAUPushConst("", , PSREG_FLASHLIGHT_COLOR)
			LUAUPushConst("", , PSREG_LINEAR_FOG_COLOR)
			LUAUPushConst("", , PSREG_LIGHT_SCALE)
			LUAUPushConst("", , PSREG_FLASHLIGHT_SCREEN_SCALE)
			LUAUPushConst("", , PSREG_SHADER_CONTROLS)

			LUAUPushConst("", , SHADER_POLYMODE_FILL);
		LUAUPushConst("", , SHADER_POLYMODE_LINE);
		LUAUPushConst("", , SHADER_POLYMODE_POINT);
		LUAUPushConstMan("SHADER_CULLMODE_CW", MATERIAL_CULLMODE_CW);
		LUAUPushConstMan("SHADER_CULLMODE_CCW", MATERIAL_CULLMODE_CCW);

		LUAUPushConst("PARAM_", , FLAGS)
			LUAUPushConst("PARAM_", , FLAGS_DEFINED)
			LUAUPushConst("PARAM_", , FLAGS2)
			LUAUPushConst("PARAM_", , FLAGS_DEFINED2)
			LUAUPushConst("PARAM_", , COLOR)
			LUAUPushConst("PARAM_", , ALPHA)
			LUAUPushConst("PARAM_", , BASETEXTURE)
			LUAUPushConst("PARAM_", , FRAME)
			LUAUPushConst("PARAM_", , BASETEXTURETRANSFORM)
			LUAUPushConst("PARAM_", , FLASHLIGHTTEXTURE)
			LUAUPushConst("PARAM_", , FLASHLIGHTTEXTUREFRAME)
			LUAUPushConst("PARAM_", , COLOR2)
			LUAUPushConst("PARAM_", , SRGBTINT)

			LUAUPushConst("STD", , TEXTURE_LIGHTMAP)
			LUAUPushConst("STD", , TEXTURE_LIGHTMAP_FULLBRIGHT)
			LUAUPushConst("STD", , TEXTURE_LIGHTMAP_BUMPED)
			LUAUPushConst("STD", , TEXTURE_LIGHTMAP_BUMPED_FULLBRIGHT)
			LUAUPushConst("STD", , TEXTURE_WHITE)
			LUAUPushConst("STD", , TEXTURE_BLACK)
			LUAUPushConst("STD", , TEXTURE_GREY)
			LUAUPushConst("STD", , TEXTURE_GREY_ALPHA_ZERO)
			LUAUPushConst("STD", , TEXTURE_NORMALMAP_FLAT)
			LUAUPushConst("STD", , TEXTURE_NORMALIZATION_CUBEMAP)
			LUAUPushConst("STD", , TEXTURE_NORMALIZATION_CUBEMAP_SIGNED)
			LUAUPushConst("STD", , TEXTURE_FRAME_BUFFER_FULL_TEXTURE_0)
			LUAUPushConst("STD", , TEXTURE_FRAME_BUFFER_FULL_TEXTURE_1)
			LUAUPushConst("STD", , TEXTURE_COLOR_CORRECTION_VOLUME_0)
			LUAUPushConst("STD", , TEXTURE_COLOR_CORRECTION_VOLUME_1)
			LUAUPushConst("STD", , TEXTURE_COLOR_CORRECTION_VOLUME_2)
			LUAUPushConst("STD", , TEXTURE_COLOR_CORRECTION_VOLUME_3)
			LUAUPushConst("STD", , TEXTURE_FRAME_BUFFER_ALIAS)
			LUAUPushConst("STD", , TEXTURE_SHADOW_NOISE_2D)
			LUAUPushConst("STD", , TEXTURE_MORPH_ACCUMULATOR)
			LUAUPushConst("STD", , TEXTURE_MORPH_WEIGHTS)
			LUAUPushConst("STD", , TEXTURE_FRAME_BUFFER_FULL_DEPTH)
			LUAUPushConst("STD", , TEXTURE_IDENTITY_LIGHTWARP)
			LUAUPushConst("STD", , TEXTURE_DEBUG_LUXELS)

			LUAUPushConst("", , MATERIAL_VAR_DEBUG)
			LUAUPushConst("", , MATERIAL_VAR_NO_DEBUG_OVERRIDE)
			LUAUPushConst("", , MATERIAL_VAR_NO_DRAW)
			LUAUPushConst("", , MATERIAL_VAR_USE_IN_FILLRATE_MODE)
			LUAUPushConst("", , MATERIAL_VAR_VERTEXCOLOR)
			LUAUPushConst("", , MATERIAL_VAR_VERTEXALPHA)
			LUAUPushConst("", , MATERIAL_VAR_SELFILLUM)
			LUAUPushConst("", , MATERIAL_VAR_ADDITIVE)
			LUAUPushConst("", , MATERIAL_VAR_ALPHATEST)
			LUAUPushConst("", , MATERIAL_VAR_MULTIPASS)
			LUAUPushConst("", , MATERIAL_VAR_ZNEARER)
			LUAUPushConst("", , MATERIAL_VAR_MODEL)
			LUAUPushConst("", , MATERIAL_VAR_FLAT)
			LUAUPushConst("", , MATERIAL_VAR_NOCULL)
			LUAUPushConst("", , MATERIAL_VAR_NOFOG)
			LUAUPushConst("", , MATERIAL_VAR_IGNOREZ)
			LUAUPushConst("", , MATERIAL_VAR_DECAL)
			LUAUPushConst("", , MATERIAL_VAR_ENVMAPSPHERE)
			LUAUPushConst("", , MATERIAL_VAR_NOALPHAMOD)
			LUAUPushConst("", , MATERIAL_VAR_ENVMAPCAMERASPACE)
			LUAUPushConst("", , MATERIAL_VAR_BASEALPHAENVMAPMASK)
			LUAUPushConst("", , MATERIAL_VAR_TRANSLUCENT)
			LUAUPushConst("", , MATERIAL_VAR_NORMALMAPALPHAENVMAPMASK)
			LUAUPushConst("", , MATERIAL_VAR_NEEDS_SOFTWARE_SKINNING)
			LUAUPushConst("", , MATERIAL_VAR_OPAQUETEXTURE)
			LUAUPushConst("", , MATERIAL_VAR_ENVMAPMODE)
			LUAUPushConst("", , MATERIAL_VAR_SUPPRESS_DECALS)
			LUAUPushConst("", , MATERIAL_VAR_HALFLAMBERT)
			LUAUPushConst("", , MATERIAL_VAR_WIREFRAME)
			LUAUPushConst("", , MATERIAL_VAR_ALLOWALPHATOCOVERAGE)
			LUAUPushConst("", , MATERIAL_VAR_IGNORE_ALPHA_MODULATION)

			LUAUPushConst("", , MATERIAL_VAR2_LIGHTING_UNLIT)
			LUAUPushConst("", , MATERIAL_VAR2_LIGHTING_VERTEX_LIT)
			LUAUPushConst("", , MATERIAL_VAR2_LIGHTING_LIGHTMAP)
			LUAUPushConst("", , MATERIAL_VAR2_LIGHTING_BUMPED_LIGHTMAP)
			LUAUPushConst("", , MATERIAL_VAR2_DIFFUSE_BUMPMAPPED_MODEL)
			LUAUPushConst("", , MATERIAL_VAR2_USES_ENV_CUBEMAP)
			LUAUPushConst("", , MATERIAL_VAR2_NEEDS_TANGENT_SPACES)
			LUAUPushConst("", , MATERIAL_VAR2_NEEDS_SOFTWARE_LIGHTING)
			LUAUPushConst("", , MATERIAL_VAR2_BLEND_WITH_LIGHTMAP_ALPHA)
			LUAUPushConst("", , MATERIAL_VAR2_NEEDS_BAKED_LIGHTING_SNAPSHOTS)
			LUAUPushConst("", , MATERIAL_VAR2_USE_FLASHLIGHT)
			LUAUPushConst("", , MATERIAL_VAR2_USE_FIXED_FUNCTION_BAKED_LIGHTING)
			LUAUPushConst("", , MATERIAL_VAR2_NEEDS_FIXED_FUNCTION_FLASHLIGHT)
			LUAUPushConst("", , MATERIAL_VAR2_USE_EDITOR)
			LUAUPushConst("", , MATERIAL_VAR2_NEEDS_POWER_OF_TWO_FRAME_BUFFER_TEXTURE)
			LUAUPushConst("", , MATERIAL_VAR2_NEEDS_FULL_FRAME_BUFFER_TEXTURE)
			LUAUPushConst("", , MATERIAL_VAR2_IS_SPRITECARD)
			LUAUPushConst("", , MATERIAL_VAR2_USES_VERTEXID)
			LUAUPushConst("", , MATERIAL_VAR2_SUPPORTS_HW_SKINNING)
			LUAUPushConst("", , MATERIAL_VAR2_SUPPORTS_FLASHLIGHT)

			LUAUPushConst("", , SHADER_PARAM_TYPE_TEXTURE)
			LUAUPushConst("", , SHADER_PARAM_TYPE_COLOR)
			LUAUPushConst("", , SHADER_PARAM_TYPE_VEC2)
			LUAUPushConst("", , SHADER_PARAM_TYPE_VEC3)
			LUAUPushConst("", , SHADER_PARAM_TYPE_VEC4)
			LUAUPushConst("", , SHADER_PARAM_TYPE_FLOAT)
			LUAUPushConst("", , SHADER_PARAM_TYPE_MATRIX)
			LUAUPushConst("", , SHADER_PARAM_TYPE_STRING)
			LUAUPushConst("", , SHADER_PARAM_TYPE_MATRIX4X2)

			LUAUPushConst("", , D3DXSHADER_DEBUG)
			LUAUPushConst("", , D3DXSHADER_SKIPVALIDATION)
			LUAUPushConst("", , D3DXSHADER_SKIPOPTIMIZATION)
			LUAUPushConst("", , D3DXSHADER_PACKMATRIX_ROWMAJOR)
			LUAUPushConst("", , D3DXSHADER_PACKMATRIX_COLUMNMAJOR)
			LUAUPushConst("", , D3DXSHADER_PARTIALPRECISION)
			LUAUPushConst("", , D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT)
			LUAUPushConst("", , D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT)
			LUAUPushConst("", , D3DXSHADER_NO_PRESHADER)
			LUAUPushConst("", , D3DXSHADER_AVOID_FLOW_CONTROL)
			LUAUPushConst("", , D3DXSHADER_PREFER_FLOW_CONTROL)
			LUAUPushConst("", , D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY)
			LUAUPushConst("", , D3DXSHADER_IEEE_STRICTNESS)
			LUAUPushConst("", , D3DXSHADER_USE_LEGACY_D3DX9_31_DLL)
			LUAUPushConst("", , D3DXSHADER_OPTIMIZATION_LEVEL0)
			LUAUPushConst("", , D3DXSHADER_OPTIMIZATION_LEVEL1)
			LUAUPushConst("", , D3DXSHADER_OPTIMIZATION_LEVEL2)
			LUAUPushConst("", , D3DXSHADER_OPTIMIZATION_LEVEL3)

#define GENStandardConstantTypeLua(v) LUAUPushConst("STDCONST_", ShaderConstantP::StandardConstantType::, v);
			GENStandardConstantType(GENStandardConstantTypeLua);
		LUA->Pop();

		ShaderUData_typeID = LUA->CreateMetaTable("Shader");
		LUA->CreateTable();
		luau_openlib(LUA, shader_methods, 0);
		LUA->SetField(-2, "__index");
		LUA->Pop();
	}

	void MenuDeinit(GarrysMod::Lua::ILuaBase* LUA)
	{
		if (!g_pShaderLibDLL) { return; }
		g_pCShaderSystem->m_ShaderDLLs.RemoveAll();
	}

	void LuaDeinit(GarrysMod::Lua::ILuaBase* LUA)
	{
		if (bDepthPass)
		{
			RES_CV(mat_drawwater);
			RES_CV(r_drawtranslucentworld);
		}
		if (!g_pShaderLibDLL) { return; }
		for (int index = g_pShaderLibDLL->m_ShaderDict.Count(); --index >= 0; )
		{
			ShaderLib::GShader* shader = (ShaderLib::GShader*)g_pShaderLibDLL->m_ShaderDict.Element(index);
			shader->LUA = NULL;
			shader->Destruct();
			g_pShaderLibDLL->m_ShaderDict.RemoveAt(index);

		}

		if (!g_pShaderManager) { return; }

		{
			intp pshIndex = GetPixelShaderDict().Head();
			intp pshIndexPrev = 0;
			while (pshIndex != GetPixelShaderDict().InvalidIndex())
			{
				CShaderManager::ShaderLookup_t& lookup = GetPixelShaderDict()[pshIndex];
				pshIndexPrev = pshIndex;
				pshIndex = GetPixelShaderDict().Next(pshIndex);
				if (lookup.m_nStaticIndex == INT_MIN)
				{
					auto combos = &lookup.m_ShaderStaticCombos;
					for (int i = 0; i < combos->m_nCount; i++)
					{
						IDirect3DPixelShader9* pixelshader = (IDirect3DPixelShader9*)lookup.m_ShaderStaticCombos.m_pHardwareShaders[i];
						g_pShaderManager->DestroyPixelShader((PixelShaderHandle_t)g_pShaderManager->m_RawPixelShaderDict.Find(pixelshader));
					}
					delete combos->m_pHardwareShaders;
					GetPixelShaderDict().Remove(pshIndexPrev);
				}
			}
		}

		{
			intp vshIndex = GetVertexShaderDict().Head();
			intp vshIndexPrev = 0;
			while (vshIndex != GetVertexShaderDict().InvalidIndex())
			{
				CShaderManager::ShaderLookup_t& lookup = GetVertexShaderDict()[vshIndex];
				vshIndexPrev = vshIndex;
				vshIndex = GetVertexShaderDict().Next(vshIndex);
				if (lookup.m_nStaticIndex == INT_MIN)
				{
					auto combos = &lookup.m_ShaderStaticCombos;
					for (int i = 0; i < combos->m_nCount; i++)
					{
						IDirect3DVertexShader9* vertexshader = (IDirect3DVertexShader9*)lookup.m_ShaderStaticCombos.m_pHardwareShaders[i];
						g_pShaderManager->DestroyVertexShader((VertexShaderHandle_t)g_pShaderManager->m_RawVertexShaderDict.Find(vertexshader));
					}
					delete combos->m_pHardwareShaders;
					GetVertexShaderDict().Remove(vshIndexPrev);
				}
			}
		}
	}

}