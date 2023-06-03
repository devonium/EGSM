#pragma once
#include "f_imaterialsystemhardwareconfig.h"
#include "cpp_shader_constant_register_map.h"
#include "BaseVSShader.h"
#include <shaderlib/cshader.h>
#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Lua/LuaInterface.h>
#include <Color.h>
#include <shaderapi/IShaderDevice.h>
#include <shaderapi/ishaderapi.h>
#include <shaderapi/ishaderdynamic.h>
#include "lua_utils.h"
#include <BaseVSShader.h>
#include <ishadercompiledll.h>
#include <tier1/utllinkedlist.h>
#include "IShaderSystem.h"
#include <materialsystem/imaterial.h>
#include <materialsystem/itexture.h>
#include <shaderlib/cshader.h>
#include <defs.h>
#include <utlhash.h>

namespace ShaderLib
{
	static CShaderManager* g_pShaderManager = NULL;
	int  MenuInit(GarrysMod::Lua::ILuaBase* LUA);
	void MenuDeinit(GarrysMod::Lua::ILuaBase* LUA);
	void LuaInit(GarrysMod::Lua::ILuaBase* LUA);
	void LuaPostInit(GarrysMod::Lua::ILuaBase* LUA);
	void LuaDeinit(GarrysMod::Lua::ILuaBase* LUA);

#ifdef WIN64
	static CUtlFixedLinkedList< CShaderManager::ShaderLookup_t > g_VertexShaderDict;
	static CUtlFixedLinkedList< CShaderManager::ShaderLookup_t > g_PixelShaderDict;
#endif

	inline CUtlFixedLinkedList< CShaderManager::ShaderLookup_t >& GetVertexShaderDict()
	{
#ifdef WIN64
		return g_VertexShaderDict;
#else 
		return g_pShaderManager->m_VertexShaderDict;
#endif
	}

	inline CUtlFixedLinkedList< CShaderManager::ShaderLookup_t >& GetPixelShaderDict()
	{
#ifdef WIN64
		return g_PixelShaderDict;
#else 
		return g_pShaderManager->m_PixelShaderDict;
#endif
	}

	static bool b_isEgsmShader = false;

	static float fVals[4];
	static BOOL  bVals[4];

	typedef CBaseVSShader CBaseClass;
	static int s_nFlags = -256;

	class CShaderParam
	{
		struct _ShaderParamInfo_t
		{
			const char* m_pName;
			const char* m_pHelp;
			ShaderParamType_t m_Type;
			const char* m_pDefaultValue = NULL;
			int m_nFlags;
		};

	public:
		CShaderParam(CUtlVector<CShaderParam*>& s_ShaderParams, const char* pName, ShaderParamType_t type, const char* pDefaultParam, const char* pHelp, int nFlags)
		{
			m_Info.m_pName = pName;
			m_Info.m_Type = type;
			m_Info.m_pDefaultValue = pDefaultParam;
			m_Info.m_pHelp = pHelp;
			m_Info.m_nFlags = nFlags;
			m_Index = NUM_SHADER_MATERIAL_VARS + s_ShaderParams.Count();
			s_ShaderParams.AddToTail(this);
		}	

		CShaderParam(CShaderParam** s_pShaderParamOverrides, ShaderMaterialVars_t var, ShaderParamType_t type, const char* pDefaultParam, const char* pHelp, int nFlags)
		{
			m_Info.m_pName = "override"; 
			m_Info.m_Type = type; 
			m_Info.m_pDefaultValue = pDefaultParam; 
			m_Info.m_pHelp = pHelp; 
			m_Info.m_nFlags = nFlags; 
			AssertMsg(!s_pShaderParamOverrides[var], ("Shader parameter override duplicately defined!")); 
			s_pShaderParamOverrides[var] = this; 
			m_Index = var;
		}
		operator int() { return m_Index; }
		const char* GetName() { return m_Info.m_pName; }
		ShaderParamType_t GetType() { return m_Info.m_Type; }
		const char* GetDefault() { return m_Info.m_pDefaultValue; }
		int GetFlags() const { return m_Info.m_nFlags; }
		const char* GetHelp() { return m_Info.m_pHelp; }

	public:
		_ShaderParamInfo_t m_Info;
		int m_Index;
	};

	static const char* s_HelpString = "Help for TeshShader";

	class GShader : public CBaseClass
	{
	public:
		char* s_Name = strdup("TestShader");
		CUtlVector<CShaderParam*> s_ShaderParams;
		CShaderParam* s_pShaderParamOverrides[NUM_SHADER_MATERIAL_VARS];

		void Destruct()
		{
			while (s_ShaderParams.Count() > 0)
			{
				auto param = s_ShaderParams.Head();
				delete param->m_Info.m_pDefaultValue;
				delete param->m_Info.m_pName;
				s_ShaderParams.Remove(0);
			}
			while (Binds.Count() > 0) { Binds.Remove(0); }
			while (PConstants.Count() > 0) { PConstants.Remove(0); }
			while (VConstants.Count() > 0) { VConstants.Remove(0); }
			if (PShader) { delete PShader; }
			if (VShader) { delete VShader; }
			if (s_Name) { delete s_Name; }
		}

		char const* GetName() const { return s_Name; }
		int GetFlags() const { return s_nFlags; }
		int GetNumParams() const { return CBaseClass::GetNumParams() + s_ShaderParams.Count(); }
		char const* GetParamName(int param) const {
			int nBaseClassParamCount = CBaseClass::GetNumParams();
			if (param < nBaseClassParamCount)
				return CBaseClass::GetParamName(param);
			else
				return s_ShaderParams[param - nBaseClassParamCount]->GetName();
		}
		char const* GetParamHelp(int param) const
		{
			int nBaseClassParamCount = CBaseClass::GetNumParams();
			if (param < nBaseClassParamCount)
			{
				if (!s_pShaderParamOverrides[param])
					return CBaseClass::GetParamHelp(param);
				else
					return s_pShaderParamOverrides[param]->GetHelp();
			}
			else
				return s_ShaderParams[param - nBaseClassParamCount]->GetHelp();
		}
		ShaderParamType_t GetParamType(int param) const
		{
			int nBaseClassParamCount = CBaseClass::GetNumParams();
			if (param < nBaseClassParamCount)
				return CBaseClass::GetParamType(param);
			else
				return s_ShaderParams[param - nBaseClassParamCount]->GetType();
		}
		char const* GetParamDefault(int param) const
		{
			int nBaseClassParamCount = CBaseClass::GetNumParams();
			if (param < nBaseClassParamCount)
			{
				if (!s_pShaderParamOverrides[param])
					return CBaseClass::GetParamDefault(param);
				else
					return s_pShaderParamOverrides[param]->GetDefault();
			}
			else
				return s_ShaderParams[param - nBaseClassParamCount]->GetDefault();
		}
		int GetParamFlags(int param) const
		{
			int nBaseClassParamCount = CBaseClass::GetNumParams();
			if (param < nBaseClassParamCount)
			{
				if (!s_pShaderParamOverrides[param])
					return CBaseClass::GetParamFlags(param);
				else
					return s_pShaderParamOverrides[param]->GetFlags();
			}
			else
				return s_ShaderParams[param - nBaseClassParamCount]->GetFlags();
		}

		virtual char const* GetFallbackShader(IMaterialVar** params) const
		{
			// Requires DX9 + above
			if (g_pHardwareConfig->GetDXSupportLevel() < 90)
			{
				return "Wireframe";
			}
			return 0;
		}

		virtual bool NeedsFullFrameBufferTexture(IMaterialVar** params, bool bCheckSpecificToThisFrame /* = true */) const
		{
			return true;
		}

		void OnInitShaderInstance(IMaterialVar** params, IShaderInit* pShaderInit, const char* pMaterialName)
		{
			for (int i = 0; i < GetNumParams(); i++)
			{
				if (!params[i]->IsDefined() && GetParamDefault(i))
				{
					params[i]->SetStringValue(GetParamDefault(i));
				}
			}

			for (int i = 0; i < PConstants.Count(); i++)
			{
				ShaderConstantF* constant = PConstants.Element(i);
				if (constant->Type == constant->Param)
				{
					auto c = (reinterpret_cast<ShaderConstantP*>(constant));
					c->IsValid = 1;
				}
			}

			for (int i = VConstants.Count(); --i >= 0; )
			{
				ShaderConstantF* constant = VConstants.Element(i);
				if (constant->Type == constant->Param)
				{
					auto c = (reinterpret_cast<ShaderConstantP*>(constant));
					c->IsValid = 1;
				}
			}

			for (int i = Binds.Count(); --i >= 0; )
			{
				TextureBind* bind = Binds.Element(i);
				bind->IsValid = true;
				if (bind->IsStandardTexture) { continue; }
				if (bind->ParamIndex > (GetNumParams() - 1))
				{
					static Color color(255, 0, 0, 255);
					ConColorMsg(color, "%s: %s param %s is not declared\n", GetName(), pMaterialName, GetParamName(bind->ParamIndex));
				}

				if (bind->Type == bind->Texture)
				{
					LoadTexture(bind->ParamIndex);
				}
				else if (bind->Type == bind->Cubemap)
				{
					LoadCubeMap(bind->ParamIndex);
				}
				else
				{
					LoadBumpMap(bind->ParamIndex);
				}
			}

			params[FLAGS]->SetIntValue(params[FLAGS]->GetIntValue() | Flags);
			params[FLAGS2]->SetIntValue(params[FLAGS2]->GetIntValue() | Flags2);
		}

		struct TextureBind
		{
			enum TextureType
			{
				Texture,
				Cubemap,
				Bumpmap,
			};
			int ParamIndex = 0;
			int Sampler = 0;
			TextureType Type = Texture;
			bool IsStandardTexture = false;
			bool IsValid = false;
		};

		struct ShaderConstantF
		{
			enum ShaderConstantType
			{
				Matrix = 0,
				Float = 1,
				Bool = 2,
				Param = 3,
			};
			char Index = 0;
			int Num : 4;
			ShaderConstantType Type : 4;
			float Vals[4];
			float _[13];
		};

		enum RetValueType
		{
			NONE,
			FLOAT,
			BOOL
		};

#define GENStandardConstantType(_) _(EYE_POS) _(CURTIME) _(LIGHT_INFO) _(AMBIENT_CUBE) _(FOG_PARAMS) _(SHADER_CONTROLS) _(DIFFUSE_MODULATION)
#define GENStandardConstantTypeEnum(v) v,
		struct ShaderConstantP
		{
			enum StandardConstantType
			{
				GENStandardConstantType(GENStandardConstantTypeEnum)
				__Total__,
			};
			int Index = 0;
			int RowsCount : 4;
			ShaderConstantF::ShaderConstantType Type : 4;
			int ParamType = 0;
			int Param = 0;
			int IsValid = 2;
			int IsStandard = false;
			float _[13];
		};

		struct ShaderConstantM
		{
			char Index = 0;
			ShaderConstantF::ShaderConstantType Type : 4;
			int RowsCount = 0;
			float Vals[16];
		};

		LightState_t lightState;
		VMatrix viewMatrix, projectionMatrix, viewProjectionMatrix, inverseViewProjectionMatrix;

		RetValueType StdToValue(IShaderDynamicAPI* pShaderAPI, ShaderConstantP* param, IMaterialVar** params, int vsh)
		{
			CMatRenderContextPtr pRenderContext(g_pMaterialSystem);
			switch (param->Param)
			{
			case ShaderConstantP::EYE_POS:
				pShaderAPI->GetWorldSpaceCameraPosition(fVals);
				return FLOAT;
				break;
			case ShaderConstantP::CURTIME:
				fVals[0] = pShaderAPI->CurrentTime();
				return FLOAT;
				break;
			case ShaderConstantP::LIGHT_INFO:
				pShaderAPI->CommitPixelShaderLighting(param->Index);
				return NONE;
				break;
			case ShaderConstantP::AMBIENT_CUBE:
				pShaderAPI->SetPixelShaderStateAmbientLightCube(param->Index, !lightState.m_bAmbientLight);
				SetAmbientCubeDynamicStateVertexShader();
				return NONE;
				break;
			case ShaderConstantP::FOG_PARAMS:
				pShaderAPI->SetPixelShaderFogParams(param->Index);
				return NONE;
				break;			
			case ShaderConstantP::SHADER_CONTROLS:
				fVals[0] = pShaderAPI->GetPixelFogCombo() == 1 ? 1 : 0;
				fVals[1] = pShaderAPI->ShouldWriteDepthToDestAlpha() && IsPC() ? 1 : 0;
				fVals[2] = (pShaderAPI->GetPixelFogCombo() == 1 && (pShaderAPI->GetSceneFogMode() == MATERIAL_FOG_LINEAR_BELOW_FOG_Z)) ? 1 : 0;
				fVals[3] = IS_FLAG_SET(MATERIAL_VAR_VERTEXALPHA) ? 1 : 0;
				return FLOAT;
				break;
			case ShaderConstantP::DIFFUSE_MODULATION:
				SetModulationPixelShaderDynamicState_LinearColorSpace(param->Index);
				return NONE;
				break;
			default:
				return NONE;
			}
		}

		bool UpdateFlags = false;
		int Flags = 0;
		int Flags2 = 0;
		int ActiveSamplers = 0;
		int UDataRef = NULL;
		char* PShader = NULL;
		char* VShader = NULL;

		GarrysMod::Lua::ILuaBase* LUA = NULL;
		CUtlVector<TextureBind*> Binds;
		CUtlVector<ShaderConstantF*> PConstants;
		CUtlVector<ShaderConstantF*> VConstants;

		bool SupportsFlashlight = false;

		MaterialCullMode_t CullMode = MATERIAL_CULLMODE_CCW;
		ShaderPolyMode_t PolyMode = SHADER_POLYMODE_FILL;

		bool OverrideBlending = false;
		ShaderBlendFactor_t BlendSrc = SHADER_BLEND_SRC_ALPHA;
		ShaderBlendFactor_t BlendDst = SHADER_BLEND_ONE_MINUS_SRC_ALPHA;

		bool IsStencilEnabled = false;
		StencilOperation_t StencilFailOp = StencilOperation_t::STENCILOPERATION_KEEP;
		StencilOperation_t StencilZFail = StencilOperation_t::STENCILOPERATION_KEEP;
		StencilOperation_t StencilPassOp = StencilOperation_t::STENCILOPERATION_KEEP;
		StencilComparisonFunction_t StencilCompareFunc = StencilComparisonFunction_t::STENCILCOMPARISONFUNCTION_NEVER;
		int StencilReferenceValue = 0;
		uint32 StencilTestMask = 0;
		uint32 StencilWriteMask = 0;
		
		ITexture* rts[4] = { 0, 0, 0, 0 };

		void OnDrawElements(IMaterialVar** params, IShaderShadow* pShaderShadow, IShaderDynamicAPI* pShaderAPI, VertexCompressionType_t vertexCompression, CBasePerMaterialContextData** pContextDataPtr)
		{
			if (!VShader || !PShader) { Draw(false); return; }
			bool bHasFlashlight = SupportsFlashlight && UsingFlashlight(params);
			

			bool bHasBaseTexture = params[BASETEXTURE]->IsTexture();
			bool bIsAlphaTested = IS_FLAG_SET(MATERIAL_VAR_ALPHATEST) != 0;

			BlendType_t nBlendType = EvaluateBlendRequirements(BASETEXTURE, true);
			bool bFullyOpaque = (nBlendType != BT_BLENDADD) && (nBlendType != BT_BLEND) && !bIsAlphaTested && !bHasFlashlight;

			if (IsSnapshotting())
			{
				pShaderShadow->EnableAlphaTest(bIsAlphaTested);

				if (bHasFlashlight)
				{
					if (bHasBaseTexture)
					{
						SetAdditiveBlendingShadowState(BASETEXTURE, true);
					}

					if (bIsAlphaTested)
					{
						// disable alpha test and use the zfunc zequals since alpha isn't guaranteed to 
						// be the same on both the regular pass and the flashlight pass.
						pShaderShadow->EnableAlphaTest(false);
						pShaderShadow->DepthFunc(SHADER_DEPTHFUNC_EQUAL);
					}
					pShaderShadow->EnableBlending(true);
					pShaderShadow->EnableDepthWrites(false);

					// Be sure not to write to dest alpha
					pShaderShadow->EnableAlphaWrites(false);

				}
				else // not flashlight pass
				{
					SetBlendingShadowState(nBlendType);

				}

				for (int i = 0; i <= ActiveSamplers; i++)
				{
					pShaderShadow->EnableTexture((Sampler_t)i, true);
					pShaderShadow->EnableSRGBRead((Sampler_t)i, true);
				}

				//// Always enable...will bind white if nothing specified...
				pShaderShadow->EnableTexture(SHADER_SAMPLER0, true);		// Base (albedo) map
				pShaderShadow->EnableSRGBRead(SHADER_SAMPLER0, true);		// Base (albedo) map

				if (bHasFlashlight)
				{
					pShaderShadow->EnableTexture(SHADER_SAMPLER4, true);	// Shadow depth map
					pShaderShadow->SetShadowDepthFiltering(SHADER_SAMPLER4);
					pShaderShadow->EnableSRGBRead(SHADER_SAMPLER4, false);
					pShaderShadow->EnableTexture(SHADER_SAMPLER5, true);	// Noise map
					pShaderShadow->EnableTexture(SHADER_SAMPLER6, true);	// Flashlight cookie
					pShaderShadow->EnableSRGBRead(SHADER_SAMPLER6, true);
				}

				pShaderShadow->EnableSRGBWrite(true);

				unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED;

				int nTexCoordCount = 1;
				int userDataSize = 4;
				pShaderShadow->VertexShaderVertexFormat(flags, nTexCoordCount, NULL, userDataSize);

				pShaderShadow->SetVertexShader(VShader, INT_MIN);
				pShaderShadow->SetPixelShader(PShader, INT_MIN);

				if (bHasFlashlight)
				{
					FogToBlack();
				}
				else
				{
					DefaultFog();
				}

				// HACK HACK HACK - enable alpha writes all the time so that we have them for underwater stuff
				pShaderShadow->EnableAlphaWrites(bFullyOpaque);
			}
			else // not snapshotting -- begin dynamic state
			{
				if (bHasBaseTexture)
				{
					BindTexture(SHADER_SAMPLER0, BASETEXTURE);
				}
				else
				{
					pShaderAPI->BindStandardTexture(SHADER_SAMPLER0, TEXTURE_WHITE);
				}

				LightState_t lightState = { 0, false, false };
				bool bFlashlightShadows = false;
				if (bHasFlashlight)
				{
					//BindTexture(SHADER_SAMPLER6, BASET, info.m_nFlashlightTextureFrame);
					VMatrix worldToTexture;
					ITexture* pFlashlightDepthTexture;
					FlashlightState_t state = pShaderAPI->GetFlashlightStateEx(worldToTexture, &pFlashlightDepthTexture);
					bFlashlightShadows = state.m_bEnableShadows && (pFlashlightDepthTexture != NULL);

					SetFlashLightColorFromState(state, pShaderAPI, PSREG_FLASHLIGHT_COLOR);

					if (pFlashlightDepthTexture && g_pConfig->ShadowDepthTexture() && state.m_bEnableShadows)
					{
						BindTexture(SHADER_SAMPLER4, pFlashlightDepthTexture, 0);
						pShaderAPI->BindStandardTexture(SHADER_SAMPLER5, TEXTURE_SHADOW_NOISE_2D);
					}
				}
				else // no flashlight
				{
					pShaderAPI->GetDX9LightState(&lightState);
				}

				MaterialFogMode_t fogType = pShaderAPI->GetSceneFogMode();
				//int fogIndex = (fogType == MATERIAL_FOG_LINEAR_BELOW_FOG_Z) ? 1 : 0;
				int numBones = pShaderAPI->GetCurrentNumBones();

				bool bWriteDepthToAlpha = false;
				bool bWriteWaterFogToAlpha = false;
				if (bFullyOpaque)
				{
					bWriteDepthToAlpha = pShaderAPI->ShouldWriteDepthToDestAlpha();
					bWriteWaterFogToAlpha = (fogType == MATERIAL_FOG_LINEAR_BELOW_FOG_Z);
				}

				pShaderAPI->SetVertexShaderIndex((5 * (numBones > 0) + lightState.m_nNumLights));
				pShaderAPI->SetPixelShaderIndex((5 * (bHasFlashlight)) + lightState.m_nNumLights);

				if (bHasFlashlight)
				{
					VMatrix worldToTexture;
					float atten[4], pos[4], tweaks[4];

					const FlashlightState_t& flashlightState = pShaderAPI->GetFlashlightState(worldToTexture);
					SetFlashLightColorFromState(flashlightState, pShaderAPI, PSREG_FLASHLIGHT_COLOR);

					BindTexture(SHADER_SAMPLER6, flashlightState.m_pSpotlightTexture, flashlightState.m_nSpotlightTextureFrame);

					atten[0] = flashlightState.m_fConstantAtten;		// Set the flashlight attenuation factors
					atten[1] = flashlightState.m_fLinearAtten;
					atten[2] = flashlightState.m_fQuadraticAtten;
					atten[3] = flashlightState.m_FarZ;
					pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_ATTENUATION, atten, 1);

					pos[0] = flashlightState.m_vecLightOrigin[0];		// Set the flashlight origin
					pos[1] = flashlightState.m_vecLightOrigin[1];
					pos[2] = flashlightState.m_vecLightOrigin[2];
					pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_POSITION_RIM_BOOST, pos, 1);

					pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_TO_WORLD_TEXTURE, worldToTexture.Base(), 4);

					// Tweaks associated with a given flashlight
					tweaks[0] = ShadowFilterFromState(flashlightState);
					tweaks[1] = ShadowAttenFromState(flashlightState);
					HashShadow2DJitter(flashlightState.m_flShadowJitterSeed, &tweaks[2], &tweaks[3]);
					pShaderAPI->SetPixelShaderConstant(PSREG_ENVMAP_TINT__SHADOW_TWEAKS, tweaks, 1);

					// Dimensions of screen, used for screen-space noise map sampling
					float vScreenScale[4] = { 1280.0f / 32.0f, 720.0f / 32.0f, 0, 0 };
					int nWidth, nHeight;
					pShaderAPI->GetBackBufferDimensions(nWidth, nHeight);
					vScreenScale[0] = (float)nWidth / 32.0f;
					vScreenScale[1] = (float)nHeight / 32.0f;
					pShaderAPI->SetPixelShaderConstant(PSREG_FLASHLIGHT_SCREEN_SCALE, vScreenScale, 1);
				}

				for (int i = 0; i < Binds.Count(); i++)
				{
					TextureBind* bind = Binds.Element(i);
					if (!bind->IsValid) { continue; };
					if (bind->IsStandardTexture)
					{
						pShaderAPI->BindStandardTexture((Sampler_t)bind->Sampler, (StandardTextureId_t)bind->ParamIndex);
					}
					else
					{
						BindTexture((Sampler_t)bind->Sampler, bind->ParamIndex, 0);
					}
				}

				for (int i = 0; i < PConstants.Count(); i++)
				{
					ShaderConstantF* constant = PConstants.Element(i);
					if (constant->Type == constant->Float)
					{
						pShaderAPI->SetPixelShaderConstant(constant->Index, constant->Vals);
					}
					else if (constant->Type == constant->Matrix)
					{
						pShaderAPI->SetPixelShaderConstant(constant->Index, ((ShaderConstantM*)constant)->Vals, ((ShaderConstantM*)constant)->RowsCount);
					}
					else if (constant->Type == constant->Param)
					{
						auto constantP = (reinterpret_cast<ShaderConstantP*>(constant));
						if (constantP->IsStandard)
						{
							switch (StdToValue(pShaderAPI, constantP, params, false))
							{
							case NONE:
								break;
							case FLOAT:
								pShaderAPI->SetPixelShaderConstant(constantP->Index, fVals);
								break;
							case BOOL:
								pShaderAPI->SetBooleanPixelShaderConstant(constantP->Index, bVals);
								break;
							}
							continue;
						}
						if (constantP->IsValid != 1)
						{
							if (constantP->IsValid == 2)
							{
								static Color color(255, 0, 0, 255);
								ConColorMsg(color, "%s: parameter '%s' is not valid, reload your material\n", GetName(), GetParamName(constantP->Param));
								constantP->IsValid = 0;
							}
							continue;
						}
						auto param = params[constantP->Param];

						if (!param || !param->IsDefined()) { continue; }
						auto type = param->GetType();

						if (type == MATERIAL_VAR_TYPE_FLOAT)
						{
							fVals[0] = param->GetFloatValue();
							pShaderAPI->SetPixelShaderConstant(constant->Index, fVals);
						}
						else if (type == MATERIAL_VAR_TYPE_INT)
						{
							bVals[0] = param->GetIntValue();
							pShaderAPI->SetIntegerPixelShaderConstant(constant->Index, bVals);
						}
						else if (type == MATERIAL_VAR_TYPE_VECTOR)
						{
							const float* f = param->GetVecValue();
							pShaderAPI->SetPixelShaderConstant(constant->Index, f, 1);
						}
						else if (type == MATERIAL_VAR_TYPE_MATRIX)
						{
							VMatrix f = param->GetMatrixValue();
							pShaderAPI->SetPixelShaderConstant(constant->Index, f.Base(), constantP->RowsCount);
						}
					}
				}

				for (int i = 0; i < VConstants.Count(); i++)
				{
					ShaderConstantF* constant = VConstants.Element(i);
					if (constant->Type == constant->Float)
					{
						pShaderAPI->SetVertexShaderConstant(constant->Index, constant->Vals);
					}
					else if (constant->Type == constant->Matrix)
					{
						pShaderAPI->SetVertexShaderConstant(constant->Index, ((ShaderConstantM*)constant)->Vals, ((ShaderConstantM*)constant)->RowsCount);
					}
					else if (constant->Type == constant->Param)
					{
						auto constantP = (reinterpret_cast<ShaderConstantP*>(constant));
						if (constantP->IsStandard)
						{
							switch (StdToValue(pShaderAPI, constantP, params, true))
							{
							case NONE:
								break;
							case FLOAT:
								pShaderAPI->SetVertexShaderConstant(constantP->Index, fVals);
								break;
							case BOOL:
								pShaderAPI->SetBooleanVertexShaderConstant(constantP->Index, bVals);
								break;
							}
							continue;
						}

						if (constantP->IsValid != 1)
						{
							if (constantP->IsValid == 2)
							{
								static Color color(255, 0, 0, 255);
								ConColorMsg(color, "%s: param '%s' is not valid, reload your material\n", GetName(), GetParamName(constantP->Param));
								constantP->IsValid = 0;
							}
							continue;
						}

						auto param = params[constantP->Param];

						if (!param || !param->IsDefined()) { continue; }
						auto type = param->GetType();

						if (type == MATERIAL_VAR_TYPE_FLOAT)
						{
							float v = param->GetFloatValue();
							pShaderAPI->SetVertexShaderConstant(constant->Index, &v);
						}
						else if (type == MATERIAL_VAR_TYPE_VECTOR)
						{
							pShaderAPI->SetVertexShaderConstant(constant->Index, param->GetVecValue(), 1);
						}
						else if (type == MATERIAL_VAR_TYPE_MATRIX)
						{
							VMatrix f = param->GetMatrixValue();
							pShaderAPI->SetPixelShaderConstant(constant->Index, f.Base(), constantP->RowsCount);
						}
					}
				}

			}
			CMatRenderContextPtr pRenderContext(g_pMaterialSystem);
			auto pMatRenderContext = g_pMaterialSystem->GetRenderContext();

			bool isWaterPass = false;
			auto rt = pMatRenderContext->GetRenderTarget();
			if (rt)
			{
				isWaterPass = strcmp(rt->GetName(), "_rt_waterreflection") == 0;
			}

			if (!IsSnapshotting())
			{
				pRenderContext->SetStencilEnable(IsStencilEnabled);
				pRenderContext->SetStencilFailOperation(StencilFailOp);
				pRenderContext->SetStencilZFailOperation(StencilZFail);
				pRenderContext->SetStencilPassOperation(StencilPassOp);
				pRenderContext->SetStencilCompareFunction(StencilCompareFunc);
				pRenderContext->SetStencilReferenceValue(StencilReferenceValue);
				pRenderContext->SetStencilTestMask(StencilTestMask);
				pRenderContext->SetStencilWriteMask(StencilWriteMask);
				pRenderContext->CullMode(CullMode);

				if (!isWaterPass)
				{				
					if (rts[0]) { pMatRenderContext->SetRenderTargetEx(0, rts[0]); }
					if (rts[1]) { pMatRenderContext->SetRenderTargetEx(1, rts[1]); }
					if (rts[2]) { pMatRenderContext->SetRenderTargetEx(2, rts[2]); }
					if (rts[3]) { pMatRenderContext->SetRenderTargetEx(3, rts[3]); }			
				}
			}
			b_isEgsmShader = true;
			Draw();
			b_isEgsmShader = false;
			if (!IsSnapshotting())
			{
				if (!isWaterPass)
				{
					if (rts[0]) { pMatRenderContext->SetRenderTargetEx(0, NULL); }
					if (rts[1]) { pMatRenderContext->SetRenderTargetEx(1, NULL); }
					if (rts[2]) { pMatRenderContext->SetRenderTargetEx(2, NULL); }
					if (rts[3]) { pMatRenderContext->SetRenderTargetEx(3, NULL); }
				}
				pRenderContext->CullMode(MATERIAL_CULLMODE_CCW);
				pRenderContext->SetStencilEnable(false);
				
			}
		}
	};
}

typedef ShaderLib::GShader::ShaderConstantP ShaderConstantP;
typedef ShaderLib::GShader::ShaderConstantM ShaderConstantM;
typedef ShaderLib::GShader::ShaderConstantF ShaderConstantF;
typedef ShaderLib::GShader::TextureBind TextureBind;