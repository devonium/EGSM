#if !defined( VIEWRENDER_H )
#define VIEWRENDER_H
#ifdef _WIN32
#pragma once
#endif

#include "shareddefs.h"
#include "tier1/utlstack.h"
#include "iviewrender.h"
#include "view_shared.h"
#include <networkvar.h>
#include "replay/ireplayscreenshotsystem.h"
#include <playernet_vars.h>
#include <cdll_int.h>

class CBase3dView : public CRefCounted<>,
	protected CViewSetup
{
	DECLARE_CLASS_NOBASE(CBase3dView);
public:
	CBase3dView(void* pMainView);

	VPlane* GetFrustum();
	virtual int		GetDrawFlags() { return 0; }

#ifdef PORTAL
	virtual	void	EnableWorldFog() {};
#endif

protected:
	// @MULTICORE (toml 8/11/2006): need to have per-view frustum. Change when move view stack to client
	VPlane* m_Frustum;
	void* m_pMainView;
};

//-----------------------------------------------------------------------------
// Base class for 3d views
//-----------------------------------------------------------------------------
class CRendering3dView : public CBase3dView
{
	DECLARE_CLASS(CRendering3dView, CBase3dView);
public:
	CRendering3dView(void* pMainView);
	virtual ~CRendering3dView() { ReleaseLists(); }

	void Setup(const CViewSetup& setup);

	// What are we currently rendering? Returns a combination of DF_ flags.
	virtual int		GetDrawFlags();

	virtual void	Draw() {};

protected:

	// Fog setup
	void			EnableWorldFog(void);
	void			SetFogVolumeState(const VisibleFogVolumeInfo_t& fogInfo, bool bUseHeightFog);

	// Draw setup
	void			SetupRenderablesList(int viewID);

	void			UpdateRenderablesOpacity();

	// If iForceViewLeaf is not -1, then it uses the specified leaf as your starting area for setting up area portal culling.
	// This is used by water since your reflected view origin is often in solid space, but we still want to treat it as though
	// the first portal we're looking out of is a water portal, so our view effectively originates under the water.
	void			BuildWorldRenderLists(bool bDrawEntities, int iForceViewLeaf = -1, bool bUseCacheIfEnabled = true, bool bShadowDepth = false, float* pReflectionWaterHeight = NULL);

	// Purpose: Builds render lists for renderables. Called once for refraction, once for over water
	void			BuildRenderableRenderLists(int viewID);

	// More concise version of the above BuildRenderableRenderLists().  Called for shadow depth map rendering
	void			BuildShadowDepthRenderableRenderLists();

	void			DrawWorld(float waterZAdjust);

	// Draws all opaque/translucent renderables in leaves that were rendered
	void			DrawOpaqueRenderables(ERenderDepthMode DepthMode);
	void			DrawTranslucentRenderables(bool bInSkybox, bool bShadowDepth);

	// Renders all translucent entities in the render list
	void			DrawTranslucentRenderablesNoWorld(bool bInSkybox);

	// Draws translucent renderables that ignore the Z buffer
	void			DrawNoZBufferTranslucentRenderables(void);

	// Renders all translucent world surfaces in a particular set of leaves
	void			DrawTranslucentWorldInLeaves(bool bShadowDepth);

	// Renders all translucent world + detail objects in a particular set of leaves
	void			DrawTranslucentWorldAndDetailPropsInLeaves(int iCurLeaf, int iFinalLeaf, int nEngineDrawFlags, int& nDetailLeafCount, LeafIndex_t* pDetailLeafList, bool bShadowDepth);

	// Purpose: Computes the actual world list info based on the render flags
	void			PruneWorldListInfo();

#ifdef PORTAL
	virtual bool	ShouldDrawPortals() { return true; }
#endif

	void ReleaseLists();

	//-----------------------------------------------
	// Combination of DF_ flags.
	int m_DrawFlags;
	int m_ClearFlags;

	IWorldRenderList* m_pWorldRenderList;
	void* m_pRenderablesList;
	void* m_pWorldListInfo;
	void* m_pCustomVisibility;
};

class CSkyboxView : public CRendering3dView
{
	DECLARE_CLASS(CSkyboxView, CRendering3dView);
public:
	CSkyboxView(void* pMainView) :
		CRendering3dView(pMainView),
		m_pSky3dParams(NULL)
	{
	}



	bool			Setup(const CViewSetup& view, int* pClearFlags, SkyboxVisibility_t* pSkyboxVisible);
	void			Draw();



#ifdef PORTAL
	virtual bool ShouldDrawPortals() { return false; }
#endif

	virtual SkyboxVisibility_t	ComputeSkyboxVisibility();

	bool			GetSkyboxFogEnable();

	void			Enable3dSkyboxFog(void);
	void			DrawInternal(int iSkyBoxViewID, bool bInvokePreAndPostRender, ITexture* pRenderTarget, ITexture* pDepthTarget);

	sky3dparams_t* PreRender3dSkyboxWorld(SkyboxVisibility_t nSkyboxVisible);

	sky3dparams_t* m_pSky3dParams;
};

#endif