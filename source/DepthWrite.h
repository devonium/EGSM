//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Header: $
// $NoKeywords: $
//=============================================================================//

#pragma once

#include "BaseVSShader.h"

#include "vs/fdepthwrite_ps20.inc"
#include "vs/fdepthwrite_ps20b.inc"
#include "vs/fdepthwrite_vs20.inc"

#if !defined( _X360 )
#include "vs/fdepthwrite_ps30.inc"
#include "vs/fdepthwrite_vs30.inc"
#endif

extern ITexture* g_DepthTex;
extern Vector skybox_origin;
extern ITexture* g_NormalsTex;
extern int iSkyBoxScale;

BEGIN_VS_SHADER_FLAGS(DepthWrite, "Help for Depth Write", SHADER_NOT_EDITABLE)

BEGIN_SHADER_PARAMS
SHADER_PARAM(ALPHATESTREFERENCE, SHADER_PARAM_TYPE_FLOAT, "", "Alpha reference value")
SHADER_PARAM(COLOR_DEPTH, SHADER_PARAM_TYPE_BOOL, "0", "Write depth as color")
END_SHADER_PARAMS

SHADER_INIT_PARAMS()
{
	SET_FLAGS2(MATERIAL_VAR2_SUPPORTS_HW_SKINNING);
}

SHADER_FALLBACK
{
	if (g_pHardwareConfig->GetDXSupportLevel() < 90)
	{
		return "Wireframe";
	}
	return 0;
}

SHADER_INIT
{
}

SHADER_DRAW
{
	bool bAlphaClip = IS_FLAG_SET(MATERIAL_VAR_ALPHATEST);
	const int nColorDepth = GetIntParam(COLOR_DEPTH, params, 0);
	SHADOW_STATE
	{
		// Set stream format (note that this shader supports compression)
		unsigned int flags = VERTEX_POSITION | VERTEX_NORMAL | VERTEX_FORMAT_COMPRESSED;
		int nTexCoordCount = 1;
		int userDataSize = 0;
		pShaderShadow->VertexShaderVertexFormat(flags, nTexCoordCount, NULL, userDataSize);

		if (nColorDepth == 0)
		{
			// Bias primitives when rendering into shadow map so we get slope-scaled depth bias
			// rather than having to apply a constant bias in the filtering shader later
			pShaderShadow->EnablePolyOffset(SHADER_POLYOFFSET_SHADOW_BIAS);
		}

		// Turn off writes to color buffer since we always sample shadows from the DEPTH texture later
		// This gives us double-speed fill when rendering INTO the shadow map
		pShaderShadow->EnableColorWrites((nColorDepth == 1));
		pShaderShadow->EnableAlphaWrites(true);

		// Don't backface cull unless alpha clipping, since this can cause artifacts when the
		// geometry is clipped by the flashlight near plane
		// If a material was already marked nocull, don't cull it
		pShaderShadow->EnableCulling(IS_FLAG_SET(MATERIAL_VAR_ALPHATEST) && !IS_FLAG_SET(MATERIAL_VAR_NOCULL));

#ifndef _X360
			if (!g_pHardwareConfig->HasFastVertexTextures())
#endif
			{
				DECLARE_STATIC_VERTEX_SHADER(fdepthwrite_vs20);
				SET_STATIC_VERTEX_SHADER_COMBO(ONLY_PROJECT_POSITION, !bAlphaClip && IsX360() && !nColorDepth); //360 needs to know if it *shouldn't* output texture coordinates to avoid shader patches
				SET_STATIC_VERTEX_SHADER_COMBO(COLOR_DEPTH, nColorDepth);
				SET_STATIC_VERTEX_SHADER(fdepthwrite_vs20);
				pShaderShadow->SetVertexShader("fdepthwrite_vs20", _vshIndex.GetIndex());
				if (bAlphaClip || g_pHardwareConfig->PlatformRequiresNonNullPixelShaders() || nColorDepth)
				{
					if (bAlphaClip)
					{
						pShaderShadow->EnableTexture(SHADER_SAMPLER0, true);
						pShaderShadow->EnableSRGBRead(SHADER_SAMPLER0, true);
					}

					if (g_pHardwareConfig->SupportsPixelShaders_2_b())
					{
						DECLARE_STATIC_PIXEL_SHADER(fdepthwrite_ps20b);
						SET_STATIC_PIXEL_SHADER_COMBO(COLOR_DEPTH, nColorDepth);
						SET_STATIC_PIXEL_SHADER(fdepthwrite_ps20b);
						pShaderShadow->SetPixelShader("fdepthwrite_ps20b", _pshIndex.GetIndex());
					}
					else
					{
						DECLARE_STATIC_PIXEL_SHADER(fdepthwrite_ps20);
						SET_STATIC_PIXEL_SHADER_COMBO(COLOR_DEPTH, nColorDepth);
						SET_STATIC_PIXEL_SHADER(fdepthwrite_ps20);
						pShaderShadow->SetPixelShader("fdepthwrite_ps20", _pshIndex.GetIndex());
					}
				}
			}
#ifndef _X360
			else
			{


				DECLARE_STATIC_VERTEX_SHADER(fdepthwrite_vs30);
				SET_STATIC_VERTEX_SHADER_COMBO(ONLY_PROJECT_POSITION, 0); //360 only combo, and this is a PC path
				SET_STATIC_VERTEX_SHADER_COMBO(COLOR_DEPTH, nColorDepth);
				SET_STATIC_VERTEX_SHADER(fdepthwrite_vs30);
				pShaderShadow->SetVertexShader("fdepthwrite_vs30", _vshIndex.GetIndex());
				pShaderShadow->EnableTexture(SHADER_SAMPLER0, true);
				pShaderShadow->EnableSRGBRead(SHADER_SAMPLER0, true);

				DECLARE_STATIC_PIXEL_SHADER(fdepthwrite_ps30);
				SET_STATIC_PIXEL_SHADER_COMBO(COLOR_DEPTH, nColorDepth);
				SET_STATIC_PIXEL_SHADER(fdepthwrite_ps30);
				pShaderShadow->SetPixelShader("fdepthwrite_ps30", _pshIndex.GetIndex());
			}
#endif
		}
		DYNAMIC_STATE
		{

#ifndef _X360
			if (!g_pHardwareConfig->HasFastVertexTextures())
#endif
			{
				fdepthwrite_vs20_Dynamic_Index vshIndex;
				vshIndex.SetSKINNING(pShaderAPI->GetCurrentNumBones() > 0);
				vshIndex.SetCOMPRESSED_VERTS((int)vertexCompression);
				vshIndex.SetMODEL(IS_FLAG_SET(MATERIAL_VAR_MODEL));
				pShaderAPI->SetVertexShaderIndex(vshIndex.GetIndex());

				if (bAlphaClip)
				{
					BindTexture(SHADER_SAMPLER0, BASETEXTURE, FRAME);

					float vAlphaThreshold[4] = {0.7f, 0.7f, 0.7f, 0.7f};
					if (ALPHATESTREFERENCE != -1 && (params[ALPHATESTREFERENCE]->GetFloatValue() > 0.0f))
					{
						vAlphaThreshold[0] = vAlphaThreshold[1] = vAlphaThreshold[2] = vAlphaThreshold[3] = params[ALPHATESTREFERENCE]->GetFloatValue();
					}

					pShaderAPI->SetPixelShaderConstant(0, vAlphaThreshold, 1);
				}

				if (g_pHardwareConfig->SupportsPixelShaders_2_b())
				{
					DECLARE_DYNAMIC_PIXEL_SHADER(fdepthwrite_ps20b);
					SET_DYNAMIC_PIXEL_SHADER_COMBO(ALPHACLIP, bAlphaClip);
					SET_DYNAMIC_PIXEL_SHADER(fdepthwrite_ps20b);
				}
				else
				{
					DECLARE_DYNAMIC_PIXEL_SHADER(fdepthwrite_ps20);
					SET_DYNAMIC_PIXEL_SHADER_COMBO(ALPHACLIP, bAlphaClip);
					SET_DYNAMIC_PIXEL_SHADER(fdepthwrite_ps20);
				}
			}
#ifndef _X360
			else // 3.0 shader case (PC only)
			{
				SetHWMorphVertexShaderState(VERTEX_SHADER_SHADER_SPECIFIC_CONST_6, VERTEX_SHADER_SHADER_SPECIFIC_CONST_7, SHADER_VERTEXTEXTURE_SAMPLER0);

				fdepthwrite_vs30_Dynamic_Index vshIndex;
				vshIndex.SetSKINNING(pShaderAPI->GetCurrentNumBones() > 0);
				vshIndex.SetMORPHING(pShaderAPI->IsHWMorphingEnabled());
				vshIndex.SetCOMPRESSED_VERTS((int)vertexCompression);
				vshIndex.SetMODEL(IS_FLAG_SET(MATERIAL_VAR_MODEL));
				pShaderAPI->SetVertexShaderIndex(vshIndex.GetIndex());

				if (bAlphaClip)
				{
					BindTexture(SHADER_SAMPLER0, BASETEXTURE, FRAME);

					float vAlphaThreshold[4] = {0.7f, 0.7f, 0.7f, 0.7f};
					if (ALPHATESTREFERENCE != -1 && (params[ALPHATESTREFERENCE]->GetFloatValue() > 0.0f))
					{
						vAlphaThreshold[0] = vAlphaThreshold[1] = vAlphaThreshold[2] = vAlphaThreshold[3] = params[ALPHATESTREFERENCE]->GetFloatValue();
					}

					pShaderAPI->SetPixelShaderConstant(0, vAlphaThreshold, 1);
				}

				DECLARE_DYNAMIC_PIXEL_SHADER(fdepthwrite_ps30);
				SET_DYNAMIC_PIXEL_SHADER_COMBO(ALPHACLIP, bAlphaClip);
				SET_DYNAMIC_PIXEL_SHADER(fdepthwrite_ps30);
			}
#endif

			Vector4D vParms;

			// set up arbitrary far planes, as the real ones are too far ( 30,000 )
//			pShaderAPI->SetPSNearAndFarZ( 1 );
			vParms.x = skybox_origin.x;		// arbitrary near
			vParms.y = skybox_origin.y;		// arbitrary far 
			vParms.z = skybox_origin.z;
			vParms.w = iSkyBoxScale;
			pShaderAPI->SetVertexShaderConstant(VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, vParms.Base(), 1);



			//pShaderAPI->SetVertexShaderConstant(VERTEX_SHADER_SHADER_SPECIFIC_CONST_0, skybox_origin.Base());
		}	// DYNAMIC_STATE

		CMatRenderContextPtr pRenderContext(g_pMaterialSystem);
		//auto pMatRenderContext = pRenderContext->GetRenderContext();
		
		auto b = pRenderContext->GetRenderTarget();
		

		if (!IsSnapshotting())
		{
			if (b && strcmp(b->GetName(), "fuckofffog") !=0)
			{

			}
			else
			{
				pRenderContext->SetRenderTargetEx(1, g_DepthTex);
				pRenderContext->SetRenderTargetEx(2, g_NormalsTex);
			}
		}

		Draw();

		if (!IsSnapshotting())
		{
			pRenderContext->SetRenderTargetEx(1, NULL);
			pRenderContext->SetRenderTargetEx(2, NULL);
		}

}
END_SHADER

extern DepthWrite::CShader* g_DepthWriteShader = &DepthWrite::s_ShaderInstance;