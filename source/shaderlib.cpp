#include "shaderlib.h"
#include <GarrysMod/Lua/LuaInterface.h>
#include <mathlib/ssemath.h>
#include "f_mathlib.h"
//#include "f_imaterialsystemhardwareconfig.h"
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
#include <sigscan.h>
#include "Windows.h"

using namespace GarrysMod::Lua;
extern IShaderSystem* g_pSLShaderSystem;
extern IMaterialSystemHardwareConfig* g_pHardwareConfig = NULL;
extern const MaterialSystem_Config_t* g_pConfig = NULL;
extern IMaterialSystem* g_pMaterialSystem = NULL;

namespace ShaderLib
{
	IShaderDevice* g_pShaderDevice = NULL;
	IShaderAPI* g_pShaderApi = NULL;
	IShaderShadow* g_pShaderShadow = NULL;
	CShaderSystem* g_pCShaderSystem = NULL;
	CShaderSystem::ShaderDLLInfo_t* g_pShaderLibDLL = NULL;

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

	LUA_FUNCTION(shaderlib_compilepsh)
	{
		auto name = LUA->CheckString(1);
		int flags = LUA->CheckNumber(2);

		LUA->CheckType(3, Type::STRING);
		unsigned int programLen = 0;
		auto program = LUA->GetString(3, &programLen);

		CShaderManager::ShaderLookup_t lookup;
		lookup.m_Name = g_pShaderManager->m_ShaderSymbolTable.AddString(name);
		lookup.m_nStaticIndex = INT_MIN;

		auto index = g_pShaderManager->m_PixelShaderDict.Find(lookup);
		if (index != g_pShaderManager->m_PixelShaderDict.InvalidIndex())
		{
			auto hash = HashString(program);
			if (hash == g_pShaderManager->m_PixelShaderDict[index].m_nDataOffset)
			{
				return 0;
			}
			g_pShaderManager->m_PixelShaderDict[index].m_nDataOffset = hash;
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
		D3DXMACRO ScreenW = D3DXMACRO{ "SCR_W", w.c_str() };
		D3DXMACRO ScreenH = D3DXMACRO{ "SCR_H", h.c_str() };

		std::vector<D3DXMACRO> macroDefines;
		macroDefines.push_back(NUM_LIGHTS);
		macroDefines.push_back(FLASHLIGHT);
		macroDefines.push_back(ScreenW);
		macroDefines.push_back(ScreenH);
		macroDefines.push_back(ScreenSize);
		macroDefines.push_back(SM);
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
			shaders[i] = g_pShaderManager->m_RawPixelShaderDict[(int)g_pShaderManager->CreatePixelShader(bf)];
			bf->Release();
		}

		if (index == g_pShaderManager->m_PixelShaderDict.InvalidIndex())
		{
			lookup.m_ShaderStaticCombos.m_pHardwareShaders = shaders;
			lookup.m_ShaderStaticCombos.m_nCount = combosCount;

			g_pShaderManager->m_PixelShaderDict.AddToTail(lookup);
		}
		else
		{
			auto lp = &g_pShaderManager->m_PixelShaderDict[index];
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

	LUA_FUNCTION(shaderlib_compilevsh)
	{
		auto name = LUA->CheckString(1);
		int flags = LUA->CheckNumber(2);

		LUA->CheckType(3, Type::STRING);
		unsigned int programLen = 0;
		auto program = LUA->GetString(3, &programLen);

		CShaderManager::ShaderLookup_t lookup;
		lookup.m_Name = g_pShaderManager->m_ShaderSymbolTable.AddString(name);
		lookup.m_nStaticIndex = INT_MIN;

		auto index = g_pShaderManager->m_VertexShaderDict.Find(lookup);
		if (index != g_pShaderManager->m_VertexShaderDict.InvalidIndex())
		{
			auto hash = HashString(program);
			if (hash == g_pShaderManager->m_VertexShaderDict[index].m_nDataOffset)
			{
				return 0;
			}
			g_pShaderManager->m_VertexShaderDict[index].m_nDataOffset = hash;
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
			shaders[i] = g_pShaderManager->m_RawVertexShaderDict[(int)g_pShaderManager->CreateVertexShader(bf)];
			bf->Release();
		}

		if (index == g_pShaderManager->m_VertexShaderDict.InvalidIndex())
		{
			lookup.m_ShaderStaticCombos.m_pHardwareShaders = shaders;
			lookup.m_ShaderStaticCombos.m_nCount = combosCount;
			g_pShaderManager->m_VertexShaderDict.AddToTail(lookup);
		}
		else
		{
			auto lp = &g_pShaderManager->m_VertexShaderDict[index];
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

	LUA_FUNCTION(shaderlib_newshader)
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

		ShaderUData* u = LUA->NewUserType<ShaderUData>(244);
		LUA->CreateMetaTable("Shader");
		LUA->SetMetaTable(-2);

		u->Shader = shader;
		shader->LUA = LUA;
		shader->UDataRef = LUA->ReferenceCreate();
		LUA->ReferencePush(shader->UDataRef);
		return 1;
	}

	lua_lib shader_methods;

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
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		int index = LUA->CheckNumber(2);
		SetConstant<ShaderConstantF>(LUA, index, ShaderConstantF::Float, u->Shader->PConstants);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetVertexShaderConstant)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
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
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		int index = LUA->CheckNumber(2);
		SetConstantM<ShaderConstantM>(LUA, index, ShaderConstantF::Matrix, u->Shader->PConstants);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetVertexShaderConstantM)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
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
		lua_Debug* ar = luau_getcallinfo(LUA);
		// Using '%i' crashes the game for some fucking reason :|
		auto currline = std::to_string(ar->currentline);
		LUA->PushFormattedString("%s:%s: attempt to set a missing parameter", ar->short_src, currline.c_str());
		currline.~basic_string();
		LUA->Error();

	}

	LUA_LIB_FUNCTION(shader_methods, SetPixelShaderConstantFP)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		int index = LUA->CheckNumber(2);
		int paramindex = LUA->CheckNumber(3);
		if (paramindex > (u->Shader->GetNumParams() - 1)) { AttemptToSetError(LUA); return 0; }
		SetConstantFP(LUA, index, paramindex, u->Shader->PConstants);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetVertexShaderConstantFP)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
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
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		SetStandardConstant(LUA->CheckNumber(2), LUA->CheckNumber(3), u->Shader->PConstants);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetVertexShaderStandardConstant)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		SetStandardConstant(LUA->CheckNumber(2), LUA->CheckNumber(3), u->Shader->VConstants);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, AddParam)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		const char* name = LUA->CheckString(2);
		int type = max(min(LUA->CheckNumber(3), SHADER_PARAM_TYPE_MATRIX4X2), SHADER_PARAM_TYPE_TEXTURE);
		ShaderLib::CShaderParam* param = NULL;
		for (int i = u->Shader->s_ShaderParams.Count(); --i >= 0; )
		{
			param = u->Shader->s_ShaderParams.Element(i);
			if (strcmp(param->GetName(), name) == 0)
			{
				param->m_Info.m_Type = (ShaderParamType_t)type;
				LUA->PushNumber(param->operator int());
				return 1;
			}
		}

		//for (int i = NUM_SHADER_MATERIAL_VARS-1; --i >= 0; )
		//{
		//	param = u->Shader->s_pShaderParamOverrides[i];
		//	if (strcmp(param->GetName(), name) == 0)
		//	{
		//		//if (param->m_Info.m_pDefaultValue) { delete param->m_Info.m_pDefaultValue; }
		//		param->m_Info.m_pDefaultValue = strdup(defvalue);
		//		LUA->PushNumber(param->operator int());
		//		return 1;
		//	}
		//}

		param = new ShaderLib::CShaderParam(u->Shader->s_ShaderParams, strdup(name), (ShaderParamType_t)type, "", "", 0);
		LUA->PushNumber(param->operator int());
		return 1;
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
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		int sampler = LUA->CheckNumber(2);
		int paramindex = LUA->CheckNumber(3);
		BindTexture(u, sampler, paramindex, TextureBind::TextureType::Texture);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, BindStandardTexture)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		int sampler = LUA->CheckNumber(2);
		int paramindex = min(max(LUA->CheckNumber(3), StandardTextureId_t::TEXTURE_LIGHTMAP), StandardTextureId_t::TEXTURE_DEBUG_LUXELS);
		BindTexture(u, sampler, paramindex, TextureBind::TextureType::Texture, true);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, BindCubeMap)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		int sampler = LUA->CheckNumber(2);
		int paramindex = LUA->CheckNumber(3);
		BindTexture(u, sampler, paramindex, TextureBind::TextureType::Cubemap);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, BindBumpMap)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		int sampler = LUA->CheckNumber(2);
		int paramindex = LUA->CheckNumber(3);
		BindTexture(u, sampler, paramindex, TextureBind::TextureType::Bumpmap);
		return 0;
	}

	CShaderManager::ShaderLookup_t* FindShader(const char* name, CUtlFixedLinkedList<CShaderManager::ShaderLookup_t>& list)
	{
		int pshIndex = list.Head();
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
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		const char* name = LUA->CheckString(2);
		auto shader = FindShader(name, g_pShaderManager->m_PixelShaderDict);
		if (!shader)
		{
			// Using '%i' crashes the game for some fucking reason :|
			lua_Debug* ar = luau_getcallinfo(LUA);
			auto currline = std::to_string(ar->currentline);
			LUA->PushFormattedString("%s:%s: attempt to set a missing pixel shader(%s)", ar->short_src, currline.c_str(), name);
			currline.~basic_string();
			LUA->Error();
		}
		if (u->Shader->PShader) { delete u->Shader->PShader; }
		shader->m_Flags = 0;
		u->Shader->PShader = strdup(name);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetVertexShader)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		const char* name = LUA->CheckString(2);
		auto shader = FindShader(name, g_pShaderManager->m_VertexShaderDict);
		if (!shader)
		{
			lua_Debug* ar = luau_getcallinfo(LUA);
			// Using '%i' crashes the game for some fucking reason :|
			auto currline = std::to_string(ar->currentline);
			LUA->PushFormattedString("%s:%s: attempt to set a missing vertex shader(%s)", ar->short_src, currline.c_str(), name);
			currline.~basic_string();
			LUA->Error();
		}
		if (u->Shader->VShader) { delete u->Shader->VShader; }
		shader->m_Flags = 0;
		u->Shader->VShader = strdup(name);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetFlags)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		u->Shader->Flags = LUA->CheckNumber(2);
		u->Shader->UpdateFlags = true;
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetFlags2)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		u->Shader->Flags2 = LUA->CheckNumber(2);
		u->Shader->UpdateFlags = true;
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetCullMode)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		u->Shader->CullMode = (MaterialCullMode_t)max(min((int)LUA->CheckNumber(2), MATERIAL_CULLMODE_CW), MATERIAL_CULLMODE_CCW);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetPolyMode)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		u->Shader->PolyMode = (ShaderPolyMode_t)max(min((int)LUA->CheckNumber(2), SHADER_POLYMODEFACE_FRONT_AND_BACK), SHADER_POLYMODEFACE_FRONT);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, OverrideBlending)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		u->Shader->OverrideBlending = LUA->GetBool(2);
		u->Shader->BlendSrc = (ShaderBlendFactor_t)LUA->CheckNumber(3);
		u->Shader->BlendDst = (ShaderBlendFactor_t)LUA->CheckNumber(4);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, EnableFlashlightSupport)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		u->Shader->SupportsFlashlight = LUA->GetBool(2);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, EnableStencil)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		u->Shader->IsStencilEnabled = LUA->GetBool(2);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, GetName)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		LUA->PushString(u->Shader->s_Name);
		return 1;
	}

	LUA_LIB_FUNCTION(shader_methods, SetStencilFailOperation)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		u->Shader->StencilFailOp = (StencilOperation_t)LUA->CheckNumber(2);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetStencilZFailOperation)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		u->Shader->StencilZFail = (StencilOperation_t)LUA->CheckNumber(2);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetStencilPassOperation)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		u->Shader->StencilPassOp = (StencilOperation_t)LUA->CheckNumber(2);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetStencilCompareFunction)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		u->Shader->StencilCompareFunc = (StencilComparisonFunction_t)LUA->CheckNumber(2);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetStencilReferenceValue)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		u->Shader->StencilReferenceValue = LUA->CheckNumber(2);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetStencilTestMask)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		u->Shader->StencilTestMask = LUA->CheckNumber(2);
		return 0;
	}

	LUA_LIB_FUNCTION(shader_methods, SetStencilWriteMask)
	{
		ShaderUData* u = LUA->GetUserType<ShaderUData>(1, 244);
		u->Shader->StencilWriteMask = LUA->CheckNumber(2);
		return 0;
	}

	typedef VertexShader_t(__thiscall* CShaderManager_SetVertexShaderDecl)(CShaderManager* _this, const char* pVertexShaderFile, int nStaticVshIndex, char* debugLabel);

	VertexShader_t __fastcall CreateVertexShader_detour(CShaderManager* _this, DWORD, const char* pVertexShaderFile, int nStaticVshIndex, char* debugLabel)
	{
		g_pShaderManager = _this;
		return (int)INVALID_SHADER;// hook.GetTrampoline<CShaderManager_SetVertexShaderDecl>()(_this, pVertexShaderFile, nStaticVshIndex, debugLabel);
	}


	int MenuInit(GarrysMod::Lua::ILuaBase* LUA)
	{
		static Color msgc(100, 255, 100, 255);
		ConColorMsg(msgc, "ShaderLib\n");

		if (!Sys_LoadInterface("materialsystem", "MaterialSystemHardwareConfig012", NULL, (void**)&g_pHardwareConfig)) { ShaderLibError("MaterialSystemHardwareConfig012 == NULL"); }
		if (g_pHardwareConfig->GetDXSupportLevel() < 90) { ShaderLibError("Unsupported DirectX version(dxlevel<90)"); };
		if (!Sys_LoadInterface("materialsystem", "ShaderSystem002", NULL, (void**)&g_pSLShaderSystem)) { ShaderLibError("IShaderSystem == NULL"); }
		if (!Sys_LoadInterface("materialsystem", "VMaterialSystemConfig002", NULL, (void**)&g_pConfig)) { ShaderLibError("VMaterialSystemConfig002 == NULL"); }
		if (!Sys_LoadInterface("materialsystem", MATERIAL_SYSTEM_INTERFACE_VERSION, NULL, (void**)&g_pMaterialSystem)) { ShaderLibError("VMaterialSystem == NULL"); }

		g_pShaderShadow = (IShaderShadow*)(g_pMaterialSystem->QueryInterface(SHADERSHADOW_INTERFACE_VERSION));
		if (!g_pShaderShadow) { ShaderLibError("IShaderShadow == NULL"); }

		g_pShaderDevice = (IShaderDevice*)(g_pMaterialSystem->QueryInterface(SHADER_DEVICE_INTERFACE_VERSION));
		if (!g_pShaderDevice) { ShaderLibError("IShaderDevice == NULL"); }

		g_pShaderApi = (IShaderAPI*)(g_pMaterialSystem->QueryInterface(SHADERAPI_INTERFACE_VERSION));
		if (!g_pShaderApi) { ShaderLibError("IShaderAPI == NULL"); }

		auto shaderapidx = GetModuleHandle("shaderapidx9.dll");
		if (!shaderapidx) { ShaderLibError("shaderapidx9.dll == NULL\n"); }

		auto d3dx9_40 = GetModuleHandle("D3DX9_40.dll");
		if (!d3dx9_40) { ShaderLibError("D3DX9_40.dll == NULL\n"); }

		static const char sign[] = "55 8B EC 8B 45 08 83 EC 28 56 57 8B F9 85 C0 0F 84 ? ? ? ? 50 8D 45 0A C7 45 EC 00 00 00 00 50 8D 4F 64 C7 45 F0 00 00 00 00 C7 45 E0 00 00 00 00 C7 45 E4 00 00 00 00 C7 45 E8 00 00 00 00 C7 45 FC 00 00 00 00 E8 ? ? ? ? 8B 77 18 8B 4D 0C 89 4D DC 66 8B 00 66 89 45 D8 85 F6 74 11 66 39 06 75 05";
		CShaderManager_SetVertexShaderDecl CShaderManager_SetVertexShader = (CShaderManager_SetVertexShaderDecl)ScanSign(shaderapidx, sign, sizeof(sign) - 1);
		if (!CShaderManager_SetVertexShader) { ShaderLibError("CShaderManager::SetVertexShader == NULL\n"); return 0; }

		Detouring::Hook hook;
		Detouring::Hook::Target target(reinterpret_cast<void*>(CShaderManager_SetVertexShader));
		hook.Create(target, CreateVertexShader_detour);
		hook.Enable();
		g_pShaderShadow->SetVertexShader("HEREHEREHEREHERE", 1010);
		hook.Destroy();

		if (!g_pShaderManager) { ShaderLibError("g_pShaderManager == NULL\n"); return 0; }


		D3DXCompileShader = (D3DXCompileShaderDecl*)GetProcAddress(d3dx9_40, "D3DXCompileShader");
		if (!D3DXCompileShader) { ShaderLibError("D3DXCompileShader == NULL\n"); return 0; }

		g_pCShaderSystem = (CShaderSystem*)g_pSLShaderSystem;

		g_pShaderLibDLL = &g_pCShaderSystem->m_ShaderDLLs[g_pCShaderSystem->m_ShaderDLLs.AddToTail()];
		g_pShaderLibDLL->m_pFileName = strdup("custom_shaders.dll");
		g_pShaderLibDLL->m_bModShaderDLL = true;

		//sapi = (IShaderDynamicAPI*)((IShaderAPI030*)(shaderapi));

		ConColorMsg(msgc, "-Succ\n");
	}

	void LuaInit(GarrysMod::Lua::ILuaBase* LUA)
	{
		LUA->PushSpecial(SPECIAL_GLOB);
		LUA->CreateTable();
		LUA->PushCFunction(shaderlib_compilepsh);
		LUA->SetField(-2, "CompilePixelShader");

		LUA->PushCFunction(shaderlib_compilevsh);
		LUA->SetField(-2, "CompileVertexShader");

		LUA->PushCFunction(shaderlib_newshader);
		LUA->SetField(-2, "NewShader");

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

		LUA->CreateMetaTable("Shader");
		LUA->CreateTable();
		luau_openlib(LUA, shader_methods, 0);
		LUA->SetField(-2, "__index");
		LUA->Pop();
	}

	void LuaDeinit(GarrysMod::Lua::ILuaBase* LUA)
	{
		if (!g_pShaderLibDLL) { return; }
		for (int index = g_pShaderLibDLL->m_ShaderDict.Count(); --index >= 0; )
		{
			ShaderLib::GShader* shader = (ShaderLib::GShader*)g_pShaderLibDLL->m_ShaderDict.Element(index);
			shader->Destruct();
			g_pShaderLibDLL->m_ShaderDict.RemoveAt(index);
		}

		if (!g_pShaderManager) { return; }

		{
			int pshIndex = g_pShaderManager->m_PixelShaderDict.Head();
			int pshIndexPrev = 0;
			while (pshIndex != g_pShaderManager->m_PixelShaderDict.InvalidIndex())
			{
				CShaderManager::ShaderLookup_t& lookup = g_pShaderManager->m_PixelShaderDict[pshIndex];
				pshIndexPrev = pshIndex;
				pshIndex = g_pShaderManager->m_PixelShaderDict.Next(pshIndex);
				if (lookup.m_nStaticIndex == INT_MIN)
				{
					auto combos = &lookup.m_ShaderStaticCombos;
					for (int i = 0; i < combos->m_nCount; i++)
					{
						IDirect3DPixelShader9* pixelshader = (IDirect3DPixelShader9*)lookup.m_ShaderStaticCombos.m_pHardwareShaders[i];
						g_pShaderManager->DestroyPixelShader((PixelShaderHandle_t)g_pShaderManager->m_RawPixelShaderDict.Find(pixelshader));
					}
					delete combos->m_pHardwareShaders;
					g_pShaderManager->m_PixelShaderDict.Remove(pshIndexPrev);
				}
			}
		}

		{
			int vshIndex = g_pShaderManager->m_VertexShaderDict.Head();
			int vshIndexPrev = 0;
			while (vshIndex != g_pShaderManager->m_VertexShaderDict.InvalidIndex())
			{
				CShaderManager::ShaderLookup_t& lookup = g_pShaderManager->m_VertexShaderDict[vshIndex];
				vshIndexPrev = vshIndex;
				vshIndex = g_pShaderManager->m_VertexShaderDict.Next(vshIndex);
				if (lookup.m_nStaticIndex == INT_MIN)
				{
					auto combos = &lookup.m_ShaderStaticCombos;
					for (int i = 0; i < combos->m_nCount; i++)
					{
						IDirect3DVertexShader9* vertexshader = (IDirect3DVertexShader9*)lookup.m_ShaderStaticCombos.m_pHardwareShaders[i];
						g_pShaderManager->DestroyVertexShader((VertexShaderHandle_t)g_pShaderManager->m_RawVertexShaderDict.Find(vertexshader));
					}
					delete combos->m_pHardwareShaders;
					g_pShaderManager->m_VertexShaderDict.Remove(vshIndexPrev);
				}
			}
		}
	}

}