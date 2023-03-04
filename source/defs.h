#pragma once
#include <Windows.h>
#include <materialsystem/shader_vcs_version.h>
typedef void* HardwareShader_t;
FORWARD_DECLARE_HANDLE(memhandle_t);
typedef memhandle_t DataCacheHandle_t;
#define DC_INVALID_HANDLE ((DataCacheHandle_t)0)
typedef void* IDirect3DVertexShader9;
typedef void* IDirect3DPixelShader9;

template<class T> int BinarySearchCombos(uint32 nStaticComboID, int nCombos, T const* pRecords)
{
	// Use binary search - data is sorted
	int nLowerIdx = 1;
	int nUpperIdx = nCombos;
	for (;;)
	{
		if (nUpperIdx < nLowerIdx)
			return -1;

		int nMiddleIndex = (nLowerIdx + nUpperIdx) / 2;
		uint32 nProbe = pRecords[nMiddleIndex - 1].m_nStaticComboID;
		if (nStaticComboID < nProbe)
		{
			nUpperIdx = nMiddleIndex - 1;
		}
		else
		{
			if (nStaticComboID > nProbe)
				nLowerIdx = nMiddleIndex + 1;
			else
				return nMiddleIndex - 1;
		}
	}
}

inline int FindShaderStaticCombo(uint32 nStaticComboID, const ShaderHeader_t& header, StaticComboRecord_t* pRecords)
{
	if (header.m_nVersion < 5)
		return -1;

	return BinarySearchCombos(nStaticComboID, header.m_nNumStaticCombos, pRecords);
}


struct ShaderFileCache_t
{
	CUtlSymbol			m_Name;
	CUtlSymbol			m_Filename;
	ShaderHeader_t		m_Header;
	bool				m_bVertexShader;

	// valid for diff version only - contains the microcode used as the reference for diff algorithm
	CUtlBuffer			m_ReferenceCombo;

	// valid for ver5 only - contains the directory
	CUtlVector< StaticComboRecord_t >	m_StaticComboRecords;
	CUtlVector< StaticComboAliasRecord_t > m_StaticComboDupRecords;

	ShaderFileCache_t()
	{
		// invalid until version established
		m_Header.m_nVersion = 0;
	}

	bool IsValid() const
	{
		return m_Header.m_nVersion != 0;
	}

	bool IsOldVersion() const
	{
		return m_Header.m_nVersion < 5;
	}

	int IsVersion6() const
	{
		return (m_Header.m_nVersion == 6);
	}

	int FindCombo(uint32 nStaticComboID)
	{
		int nSearchAliases = BinarySearchCombos(nStaticComboID, m_StaticComboDupRecords.Count(), m_StaticComboDupRecords.Base());
		if (nSearchAliases != -1)
			nStaticComboID = m_StaticComboDupRecords[nSearchAliases].m_nSourceStaticCombo;
		return FindShaderStaticCombo(nStaticComboID, m_Header, m_StaticComboRecords.Base());
	}

	bool operator==(const ShaderFileCache_t& a) const
	{
		return m_Name == a.m_Name && m_bVertexShader == a.m_bVertexShader;
	}
};

enum LoaderError_t
{
	LOADERERROR_NONE = 0,
	LOADERERROR_FILEOPEN = -1,
	LOADERERROR_READING = -2,
};

class CShaderManager : public IShaderManager
{
public:
	CShaderManager();
	virtual ~CShaderManager();

	// Methods of IShaderManager
	virtual void				Init();
	virtual void				Shutdown();
	virtual IShaderBuffer* CompileShader(const char* pProgram, size_t nBufLen, const char* pShaderVersion);
	virtual VertexShaderHandle_t CreateVertexShader(IShaderBuffer* pShaderBuffer);
	virtual void				DestroyVertexShader(VertexShaderHandle_t hShader);
	virtual PixelShaderHandle_t CreatePixelShader(IShaderBuffer* pShaderBuffer);
	virtual void				DestroyPixelShader(PixelShaderHandle_t hShader);
	virtual VertexShader_t		CreateVertexShader(const char* pVertexShaderFile, int nStaticVshIndex = 0, char* debugLabel = NULL);
	virtual PixelShader_t		CreatePixelShader(const char* pPixelShaderFile, int nStaticPshIndex = 0, char* debugLabel = NULL);
	virtual void				SetVertexShader(VertexShader_t shader);
	virtual void				SetPixelShader(PixelShader_t shader);
	virtual void				BindVertexShader(VertexShaderHandle_t shader);
	virtual void				BindPixelShader(PixelShaderHandle_t shader);
	virtual void* GetCurrentVertexShader();
	virtual void* GetCurrentPixelShader();
	virtual void				ResetShaderState();
	void						FlushShaders();
	virtual void				ClearVertexAndPixelShaderRefCounts();
	virtual void				PurgeUnusedVertexAndPixelShaders();
	void						SpewVertexAndPixelShaders();
	const char* GetActiveVertexShaderName();
	const char* GetActivePixelShaderName();
	bool						CreateDynamicCombos_Ver4(void* pContext, uint8* pComboBuffer);
	bool						CreateDynamicCombos_Ver5(void* pContext, uint8* pComboBuffer, char* debugLabel = NULL);

#if defined( DX_TO_GL_ABSTRACTION )
	virtual void				DoStartupShaderPreloading();
#endif

	static void					QueuedLoaderCallback(void* pContext, void* pContext2, const void* pData, int nSize, LoaderError_t loaderError);

public:
	typedef CUtlFixedLinkedList< void* >::IndexType_t VertexShaderIndex_t;
	typedef CUtlFixedLinkedList< void* >::IndexType_t PixelShaderIndex_t;

	struct ShaderStaticCombos_t
	{
		int					m_nCount;

		// Can't use CUtlVector here since you CUtlLinkedList<CUtlVector<>> doesn't work.
		HardwareShader_t* m_pHardwareShaders;
		struct ShaderCreationData_t
		{
			CUtlVector<uint8> 	ByteCode;
			uint32 				iCentroidMask;
		};

		ShaderCreationData_t* m_pCreationData;
	};

	struct ShaderLookup_t
	{
		CUtlSymbol				m_Name;
		int						m_nStaticIndex;
		ShaderStaticCombos_t	m_ShaderStaticCombos;
		DWORD					m_Flags;
		int						m_nRefCount;
		unsigned int			m_hShaderFileCache;

		// for queued loading, bias an aligned optimal buffer forward to correct location
		int						m_nDataOffset;

		// diff version, valid during load only
		ShaderDictionaryEntry_t* m_pComboDictionary;

		ShaderLookup_t()
		{
			m_Flags = 0;
			m_nRefCount = 0;
			m_ShaderStaticCombos.m_nCount = 0;
			m_ShaderStaticCombos.m_pHardwareShaders = 0;
			m_ShaderStaticCombos.m_pCreationData = 0;
			m_pComboDictionary = NULL;
		}
		void IncRefCount()
		{
			m_nRefCount++;
		}
		bool operator==(const ShaderLookup_t& a) const
		{
			return m_Name == a.m_Name && m_nStaticIndex == a.m_nStaticIndex;
		}
	};

#ifdef DYNAMIC_SHADER_COMPILE
	struct Combo_t
	{
		CUtlSymbol m_ComboName;
		int m_nMin;
		int m_nMax;
	};

	struct ShaderCombos_t
	{
		CUtlVector<Combo_t> m_StaticCombos;
		CUtlVector<Combo_t> m_DynamicCombos;
		int GetNumDynamicCombos(void) const
		{
			int combos = 1;
			int i;
			for (i = 0; i < m_DynamicCombos.Count(); i++)
			{
				combos *= (m_DynamicCombos[i].m_nMax - m_DynamicCombos[i].m_nMin + 1);
			}
			return combos;
		}
		int GetNumStaticCombos(void) const
		{
			int combos = 1;
			int i;
			for (i = 0; i < m_StaticCombos.Count(); i++)
			{
				combos *= (m_StaticCombos[i].m_nMax - m_StaticCombos[i].m_nMin + 1);
			}
			return combos;
		}
	};
#endif

public:
	void					CreateStaticShaders();
	void					DestroyStaticShaders();

#if defined ( DYNAMIC_SHADER_COMPILE ) && defined( REMOTE_DYNAMIC_SHADER_COMPILE )
	void					InitRemoteShaderCompile();
	void					DeinitRemoteShaderCompile();
#endif

	// The low-level dx call to set the vertex shader state
	void					SetVertexShaderState(HardwareShader_t shader, DataCacheHandle_t hCachedShader = DC_INVALID_HANDLE);

	// The low-level dx call to set the pixel shader state
	void					SetPixelShaderState(HardwareShader_t shader, DataCacheHandle_t hCachedShader = DC_INVALID_HANDLE);

	// Destroys all shaders
	void					DestroyAllShaders();

	// Destroy a particular vertex shader
	void					DestroyVertexShader(VertexShader_t shader);
	// Destroy a particular pixel shader
	void					DestroyPixelShader(PixelShader_t shader);

	bool					LoadAndCreateShaders(ShaderLookup_t& lookup, bool bVertexShader, char* debugLabel = NULL);
	FileHandle_t			OpenFileAndLoadHeader(const char* pFileName, ShaderHeader_t* pHeader);

#ifdef DYNAMIC_SHADER_COMPILE
	bool					LoadAndCreateShaders_Dynamic(ShaderLookup_t& lookup, bool bVertexShader);
	const ShaderCombos_t* FindOrCreateShaderCombos(const char* pShaderName);
	HardwareShader_t		CompileShader(const char* pShaderName, int nStaticIndex, int nDynamicIndex, bool bVertexShader);
#endif

	void					DisassembleShader(ShaderLookup_t* pLookup, int dynamicCombo, uint8* pByteCode);
	void					WriteTranslatedFile(ShaderLookup_t* pLookup, int dynamicCombo, char* pFileContents, char* pFileExtension);

	// DX_TO_GL_ABSTRACTION only, no-op otherwise

	void					SaveShaderCache(char* cacheName);	// query GLM pair cache for all active shader pairs and write them to disk in named file
	bool					LoadShaderCache(char* cacheName);	// read named file, establish compiled shader sets for each vertex+static and pixel+static, then link pairs as listed in table
																// return true on success, false if file not found

	// old void					WarmShaderCache();

	CUtlFixedLinkedList< ShaderLookup_t > m_VertexShaderDict;
	CUtlFixedLinkedList< ShaderLookup_t > m_PixelShaderDict;

	CUtlSymbolTable m_ShaderSymbolTable;

#ifdef DYNAMIC_SHADER_COMPILE	
	typedef HRESULT(__stdcall* ShaderCompileFromFileFunc_t)(LPCSTR pSrcFile, CONST D3DXMACRO* pDefines,
		LPD3DXINCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, DWORD Flags,
		LPD3DXBUFFER* ppShader, LPD3DXBUFFER* ppErrorMsgs, LPD3DXCONSTANTTABLE* ppConstantTable);
	CUtlStringMap<ShaderCombos_t>	 m_ShaderNameToCombos;
	CSysModule* m_pShaderCompiler30;
	ShaderCompileFromFileFunc_t		m_ShaderCompileFileFunc30;
#endif

	// The current vertex and pixel shader
	HardwareShader_t	m_HardwareVertexShader;
	HardwareShader_t	m_HardwarePixelShader;

	CUtlFixedLinkedList< IDirect3DVertexShader9* > m_RawVertexShaderDict;
	CUtlFixedLinkedList< IDirect3DPixelShader9* > m_RawPixelShaderDict;

	CUtlFixedLinkedList< ShaderFileCache_t > m_ShaderFileCache;

	// false, creates during init.
	// true, creates on access, helps reduce d3d memory for tools, but causes i/o hitches.
	bool m_bCreateShadersOnDemand;

#if defined( _DEBUG )
	// for debugging (can't resolve UtlSym)
	// need some history because 360 d3d has rips related to sequencing
	char	vshDebugName[MAX_SHADER_HISTORY][64];
	int		vshDebugIndex;
	char	pshDebugName[MAX_SHADER_HISTORY][64];
	int		pshDebugIndex;
#endif

#if defined ( DYNAMIC_SHADER_COMPILE ) && defined( REMOTE_DYNAMIC_SHADER_COMPILE )
	SOCKET m_RemoteShaderCompileSocket;
#endif

};

struct ShadowState_t
{

};

struct ShadowShaderState_t
{
public:
	// The vertex + pixel shader group to use...
	VertexShader_t	m_VertexShader;
	PixelShader_t	m_PixelShader;

	// The static vertex + pixel shader indices
	int				m_nStaticVshIndex;
	int				m_nStaticPshIndex;

	// Vertex data used by this snapshot
	// Note that the vertex format actually used will be the
	// aggregate of the vertex formats used by all snapshots in a material
	VertexFormat_t	m_VertexUsage;

	// Morph data used by this snapshot
	// Note that the morph format actually used will be the
	// aggregate of the morph formats used by all snapshots in a material
	MorphFormat_t	m_MorphUsage;

	// Modulate constant color into the vertex color
	bool			m_ModulateConstantColor;

	bool			m_nReserved[3];
};

abstract_class IShaderAPI030 : public IShaderDynamicAPI
{
public:
	//
	// NOTE: These methods have been ported to DX10
	//

	// Viewport methods
	virtual void SetViewports(int nCount, const ShaderViewport_t * pViewports) = 0;
	virtual int GetViewports(ShaderViewport_t* pViewports, int nMax) const = 0;

	// Buffer clearing
	virtual void ClearBuffers(bool bClearColor, bool bClearDepth, bool bClearStencil, int renderTargetWidth, int renderTargetHeight) = 0;
	virtual void ClearColor3ub(unsigned char r, unsigned char g, unsigned char b) = 0;
	virtual void ClearColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a) = 0;

	// Methods related to binding shaders
	virtual void BindVertexShader(VertexShaderHandle_t hVertexShader) = 0;
	virtual void BindGeometryShader(GeometryShaderHandle_t hGeometryShader) = 0;
	virtual void BindPixelShader(PixelShaderHandle_t hPixelShader) = 0;

	// Methods related to state objects
	virtual void SetRasterState(const ShaderRasterState_t& state) = 0;

	//
	// NOTE: These methods have not yet been ported to DX10
	//

	// Sets the mode...
	virtual bool SetMode(void* hwnd, int nAdapter, const ShaderDeviceInfo_t& info) = 0;

	virtual void ChangeVideoMode(const ShaderDeviceInfo_t& info) = 0;

	// Returns the snapshot id for the shader state
	virtual StateSnapshot_t	 TakeSnapshot() = 0;

	virtual void TexMinFilter(ShaderTexFilterMode_t texFilterMode) = 0;
	virtual void TexMagFilter(ShaderTexFilterMode_t texFilterMode) = 0;
	virtual void TexWrap(ShaderTexCoordComponent_t coord, ShaderTexWrapMode_t wrapMode) = 0;

	virtual void CopyRenderTargetToTexture(ShaderAPITextureHandle_t textureHandle) = 0;

	// Binds a particular material to render with
	virtual void Bind(IMaterial* pMaterial) = 0;

	// Flushes any primitives that are buffered
	virtual void FlushBufferedPrimitives() = 0;

	// Gets the dynamic mesh; note that you've got to render the mesh
	// before calling this function a second time. Clients should *not*
	// call DestroyStaticMesh on the mesh returned by this call.
	virtual IMesh* GetDynamicMesh(IMaterial* pMaterial, int nHWSkinBoneCount, bool bBuffered = true,
		IMesh* pVertexOverride = 0, IMesh* pIndexOverride = 0) = 0;
	virtual IMesh* GetDynamicMeshEx(IMaterial* pMaterial, VertexFormat_t vertexFormat, int nHWSkinBoneCount,
		bool bBuffered = true, IMesh* pVertexOverride = 0, IMesh* pIndexOverride = 0) = 0;

	// Methods to ask about particular state snapshots
	virtual bool IsTranslucent(StateSnapshot_t id) const = 0;
	virtual bool IsAlphaTested(StateSnapshot_t id) const = 0;
	virtual bool UsesVertexAndPixelShaders(StateSnapshot_t id) const = 0;
	virtual bool IsDepthWriteEnabled(StateSnapshot_t id) const = 0;

	// Gets the vertex format for a set of snapshot ids
	virtual VertexFormat_t ComputeVertexFormat(int numSnapshots, StateSnapshot_t* pIds) const = 0;

	// What fields in the vertex do we actually use?
	virtual VertexFormat_t ComputeVertexUsage(int numSnapshots, StateSnapshot_t* pIds) const = 0;

	// Begins a rendering pass
	virtual void BeginPass(StateSnapshot_t snapshot) = 0;

	// Renders a single pass of a material
	virtual void RenderPass(int nPass, int nPassCount) = 0;

	// Set the number of bone weights
	virtual void SetNumBoneWeights(int numBones) = 0;

	// Sets the lights
	virtual void SetLight(int lightNum, const LightDesc_t& desc) = 0;

	// Lighting origin for the current model
	virtual void SetLightingOrigin(Vector vLightingOrigin) = 0;

	virtual void SetAmbientLight(float r, float g, float b) = 0;
	virtual void SetAmbientLightCube(Vector4D cube[6]) = 0;

	// The shade mode
	virtual void ShadeMode(ShaderShadeMode_t mode) = 0;

	// The cull mode
	virtual void CullMode(MaterialCullMode_t cullMode) = 0;

	// Force writes only when z matches. . . useful for stenciling things out
	// by rendering the desired Z values ahead of time.
	virtual void ForceDepthFuncEquals(bool bEnable) = 0;

	// Forces Z buffering to be on or off
	virtual void OverrideDepthEnable(bool bEnable, bool bDepthEnable) = 0;

	virtual void SetHeightClipZ(float z) = 0;
	virtual void SetHeightClipMode(enum MaterialHeightClipMode_t heightClipMode) = 0;

	virtual void SetClipPlane(int index, const float* pPlane) = 0;
	virtual void EnableClipPlane(int index, bool bEnable) = 0;

	// Put all the model matrices into vertex shader constants.
	virtual void SetSkinningMatrices() = 0;

	// Returns the nearest supported format
	virtual ImageFormat GetNearestSupportedFormat(ImageFormat fmt, bool bFilteringRequired = true) const = 0;
	virtual ImageFormat GetNearestRenderTargetFormat(ImageFormat fmt) const = 0;

	// When AA is enabled, render targets are not AA and require a separate
	// depth buffer.
	virtual bool DoRenderTargetsNeedSeparateDepthBuffer() const = 0;

	// Texture management methods
	// For CreateTexture also see CreateTextures below
	virtual ShaderAPITextureHandle_t CreateTexture(
		int width,
		int height,
		int depth,
		ImageFormat dstImageFormat,
		int numMipLevels,
		int numCopies,
		int flags,
		const char* pDebugName,
		const char* pTextureGroupName) = 0;

	virtual void DeleteTexture(ShaderAPITextureHandle_t textureHandle) = 0;

	virtual ShaderAPITextureHandle_t CreateDepthTexture(
		ImageFormat renderTargetFormat,
		int width,
		int height,
		const char* pDebugName,
		bool bTexture) = 0;

	virtual bool IsTexture(ShaderAPITextureHandle_t textureHandle) = 0;
	virtual bool IsTextureResident(ShaderAPITextureHandle_t textureHandle) = 0;

	// Indicates we're going to be modifying this texture
	// TexImage2D, TexSubImage2D, TexWrap, TexMinFilter, and TexMagFilter
	// all use the texture specified by this function.
	virtual void ModifyTexture(ShaderAPITextureHandle_t textureHandle) = 0;

	virtual void TexImage2D(
		int level,
		int cubeFaceID,
		ImageFormat dstFormat,
		int zOffset,
		int width,
		int height,
		ImageFormat srcFormat,
		bool bSrcIsTiled,		// NOTE: for X360 only
		void* imageData) = 0;

	virtual void TexSubImage2D(
		int level,
		int cubeFaceID,
		int xOffset,
		int yOffset,
		int zOffset,
		int width,
		int height,
		ImageFormat srcFormat,
		int srcStride,
		bool bSrcIsTiled,		// NOTE: for X360 only
		void* imageData) = 0;

	virtual void TexImageFromVTF(IVTFTexture* pVTF, int iVTFFrame) = 0;

	// An alternate (and faster) way of writing image data
	// (locks the current Modify Texture). Use the pixel writer to write the data
	// after Lock is called
	// Doesn't work for compressed textures 
	virtual bool TexLock(int level, int cubeFaceID, int xOffset, int yOffset,
		int width, int height, CPixelWriter& writer) = 0;
	virtual void TexUnlock() = 0;

	// These are bound to the texture
	virtual void TexSetPriority(int priority) = 0;

	// Sets the texture state
	virtual void BindTexture(Sampler_t sampler, ShaderAPITextureHandle_t textureHandle) = 0;

	// Set the render target to a texID.
	// Set to SHADER_RENDERTARGET_BACKBUFFER if you want to use the regular framebuffer.
	// Set to SHADER_RENDERTARGET_DEPTHBUFFER if you want to use the regular z buffer.
	virtual void SetRenderTarget(ShaderAPITextureHandle_t colorTextureHandle = SHADER_RENDERTARGET_BACKBUFFER,
		ShaderAPITextureHandle_t depthTextureHandle = SHADER_RENDERTARGET_DEPTHBUFFER) = 0;

	// stuff that isn't to be used from within a shader
	virtual void ClearBuffersObeyStencil(bool bClearColor, bool bClearDepth) = 0;
	virtual void ReadPixels(int x, int y, int width, int height, unsigned char* data, ImageFormat dstFormat) = 0;
	virtual void ReadPixels(Rect_t* pSrcRect, Rect_t* pDstRect, unsigned char* data, ImageFormat dstFormat, int nDstStride) = 0;

	virtual void FlushHardware() = 0;

	// Use this to begin and end the frame
	virtual void BeginFrame() = 0;
	virtual void EndFrame() = 0;

	// Selection mode methods
	virtual int  SelectionMode(bool selectionMode) = 0;
	virtual void SelectionBuffer(unsigned int* pBuffer, int size) = 0;
	virtual void ClearSelectionNames() = 0;
	virtual void LoadSelectionName(int name) = 0;
	virtual void PushSelectionName(int name) = 0;
	virtual void PopSelectionName() = 0;

	// Force the hardware to finish whatever it's doing
	virtual void ForceHardwareSync() = 0;

	// Used to clear the transition table when we know it's become invalid.
	virtual void ClearSnapshots() = 0;

	virtual void FogStart(float fStart) = 0;
	virtual void FogEnd(float fEnd) = 0;
	virtual void SetFogZ(float fogZ) = 0;
	// Scene fog state.
	virtual void SceneFogColor3ub(unsigned char r, unsigned char g, unsigned char b) = 0;
	virtual void GetSceneFogColor(unsigned char* rgb) = 0;
	virtual void SceneFogMode(MaterialFogMode_t fogMode) = 0;

	// Can we download textures?
	virtual bool CanDownloadTextures() const = 0;

	virtual void ResetRenderState(bool bFullReset = true) = 0;

	// We use smaller dynamic VBs during level transitions, to free up memory
	virtual int  GetCurrentDynamicVBSize(void) = 0;
	virtual void DestroyVertexBuffers(bool bExitingLevel = false) = 0;

	virtual void EvictManagedResources() = 0;

	// Level of anisotropic filtering
	virtual void SetAnisotropicLevel(int nAnisotropyLevel) = 0;

	// For debugging and building recording files. This will stuff a token into the recording file,
	// then someone doing a playback can watch for the token.
	virtual void SyncToken(const char* pToken) = 0;

	// Setup standard vertex shader constants (that don't change)
	// This needs to be called anytime that overbright changes.
	virtual void SetStandardVertexShaderConstants(float fOverbright) = 0;

	//
	// Occlusion query support
	//

	// Allocate and delete query objects.
	virtual ShaderAPIOcclusionQuery_t CreateOcclusionQueryObject(void) = 0;
	virtual void DestroyOcclusionQueryObject(ShaderAPIOcclusionQuery_t) = 0;

	// Bracket drawing with begin and end so that we can get counts next frame.
	virtual void BeginOcclusionQueryDrawing(ShaderAPIOcclusionQuery_t) = 0;
	virtual void EndOcclusionQueryDrawing(ShaderAPIOcclusionQuery_t) = 0;

	// OcclusionQuery_GetNumPixelsRendered
	//	Get the number of pixels rendered between begin and end on an earlier frame.
	//	Calling this in the same frame is a huge perf hit!
	// Returns iQueryResult:
	//	iQueryResult >= 0					-	iQueryResult is the number of pixels rendered
	//	OCCLUSION_QUERY_RESULT_PENDING		-	query results are not available yet
	//	OCCLUSION_QUERY_RESULT_ERROR		-	query failed
	// Use OCCLUSION_QUERY_FINISHED( iQueryResult ) to test if query finished.
	virtual int OcclusionQuery_GetNumPixelsRendered(ShaderAPIOcclusionQuery_t hQuery, bool bFlush = false) = 0;

	virtual void SetFlashlightState(const FlashlightState_t& state, const VMatrix& worldToTexture) = 0;

	virtual void ClearVertexAndPixelShaderRefCounts() = 0;
	virtual void PurgeUnusedVertexAndPixelShaders() = 0;

	// Called when the dx support level has changed
	virtual void DXSupportLevelChanged() = 0;

	// By default, the material system applies the VIEW and PROJECTION matrices	to the user clip
	// planes (which are specified in world space) to generate projection-space user clip planes
	// Occasionally (for the particle system in hl2, for example), we want to override that
	// behavior and explictly specify a View transform for user clip planes. The PROJECTION
	// will be mutliplied against this instead of the normal VIEW matrix.
	virtual void EnableUserClipTransformOverride(bool bEnable) = 0;
	virtual void UserClipTransform(const VMatrix& worldToView) = 0;

	// ----------------------------------------------------------------------------------
	// Everything after this point added after HL2 shipped.
	// ----------------------------------------------------------------------------------

	// What fields in the morph do we actually use?
	virtual MorphFormat_t ComputeMorphFormat(int numSnapshots, StateSnapshot_t* pIds) const = 0;

	// Set the render target to a texID.
	// Set to SHADER_RENDERTARGET_BACKBUFFER if you want to use the regular framebuffer.
	// Set to SHADER_RENDERTARGET_DEPTHBUFFER if you want to use the regular z buffer.
	virtual void SetRenderTargetEx(int nRenderTargetID,
		ShaderAPITextureHandle_t colorTextureHandle = SHADER_RENDERTARGET_BACKBUFFER,
		ShaderAPITextureHandle_t depthTextureHandle = SHADER_RENDERTARGET_DEPTHBUFFER) = 0;

	virtual void CopyRenderTargetToTextureEx(ShaderAPITextureHandle_t textureHandle, int nRenderTargetID, Rect_t* pSrcRect = NULL, Rect_t* pDstRect = NULL) = 0;
	virtual void CopyTextureToRenderTargetEx(int nRenderTargetID, ShaderAPITextureHandle_t textureHandle, Rect_t* pSrcRect = NULL, Rect_t* pDstRect = NULL) = 0;

	// For dealing with device lost in cases where SwapBuffers isn't called all the time (Hammer)
	virtual void HandleDeviceLost() = 0;

	virtual void EnableLinearColorSpaceFrameBuffer(bool bEnable) = 0;

	// Lets the shader know about the full-screen texture so it can 
	virtual void SetFullScreenTextureHandle(ShaderAPITextureHandle_t h) = 0;

	// Rendering parameters control special drawing modes withing the material system, shader
	// system, shaders, and engine. renderparm.h has their definitions.
	virtual void SetFloatRenderingParameter(int parm_number, float value) = 0;
	virtual void SetIntRenderingParameter(int parm_number, int value) = 0;
	virtual void SetVectorRenderingParameter(int parm_number, Vector const& value) = 0;

	virtual float GetFloatRenderingParameter(int parm_number) const = 0;
	virtual int GetIntRenderingParameter(int parm_number) const = 0;
	virtual Vector GetVectorRenderingParameter(int parm_number) const = 0;

	virtual void SetFastClipPlane(const float* pPlane) = 0;
	virtual void EnableFastClip(bool bEnable) = 0;

	// Returns the number of vertices + indices we can render using the dynamic mesh
	// Passing true in the second parameter will return the max # of vertices + indices
	// we can use before a flush is provoked and may return different values 
	// if called multiple times in succession. 
	// Passing false into the second parameter will return
	// the maximum possible vertices + indices that can be rendered in a single batch
	virtual void GetMaxToRender(IMesh* pMesh, bool bMaxUntilFlush, int* pMaxVerts, int* pMaxIndices) = 0;

	// Returns the max number of vertices we can render for a given material
	virtual int GetMaxVerticesToRender(IMaterial* pMaterial) = 0;
	virtual int GetMaxIndicesToRender() = 0;

	// stencil methods
	virtual void SetStencilEnable(bool onoff) = 0;
	virtual void SetStencilFailOperation(StencilOperation_t op) = 0;
	virtual void SetStencilZFailOperation(StencilOperation_t op) = 0;
	virtual void SetStencilPassOperation(StencilOperation_t op) = 0;
	virtual void SetStencilCompareFunction(StencilComparisonFunction_t cmpfn) = 0;
	virtual void SetStencilReferenceValue(int ref) = 0;
	virtual void SetStencilTestMask(uint32 msk) = 0;
	virtual void SetStencilWriteMask(uint32 msk) = 0;
	virtual void ClearStencilBufferRectangle(int xmin, int ymin, int xmax, int ymax, int value) = 0;

	// disables all local lights
	virtual void DisableAllLocalLights() = 0;
	virtual int CompareSnapshots(StateSnapshot_t snapshot0, StateSnapshot_t snapshot1) = 0;

	virtual IMesh* GetFlexMesh() = 0;

	virtual void SetFlashlightStateEx(const FlashlightState_t& state, const VMatrix& worldToTexture, ITexture* pFlashlightDepthTexture) = 0;

	virtual bool SupportsMSAAMode(int nMSAAMode) = 0;

#if defined( _X360 )
	virtual HXUIFONT OpenTrueTypeFont(const char* pFontname, int tall, int style) = 0;
	virtual void CloseTrueTypeFont(HXUIFONT hFont) = 0;
	virtual bool GetTrueTypeFontMetrics(HXUIFONT hFont, XUIFontMetrics* pFontMetrics, XUICharMetrics charMetrics[256]) = 0;
	// Render a sequence of characters and extract the data into a buffer
	// For each character, provide the width+height of the font texture subrect,
	// an offset to apply when rendering the glyph, and an offset into a buffer to receive the RGBA data
	virtual bool GetTrueTypeGlyphs(HXUIFONT hFont, int numChars, wchar_t* pWch, int* pOffsetX, int* pOffsetY, int* pWidth, int* pHeight, unsigned char* pRGBA, int* pRGBAOffset) = 0;
	virtual ShaderAPITextureHandle_t CreateRenderTargetSurface(int width, int height, ImageFormat format, const char* pDebugName, const char* pTextureGroupName) = 0;
	virtual void PersistDisplay() = 0;
	virtual bool PostQueuedTexture(const void* pData, int nSize, ShaderAPITextureHandle_t* pHandles, int nHandles, int nWidth, int nHeight, int nDepth, int nMips, int* pRefCount) = 0;
	virtual void* GetD3DDevice() = 0;

	virtual void PushVertexShaderGPRAllocation(int iVertexShaderCount = 64) = 0;
	virtual void PopVertexShaderGPRAllocation(void) = 0;

	virtual void EnableVSync_360(bool bEnable) = 0; //360 allows us to bypass vsync blocking up to 60 fps without creating a new device
#endif

	virtual bool OwnGPUResources(bool bEnable) = 0;

	//get fog distances entered with FogStart(), FogEnd(), and SetFogZ()
	virtual void GetFogDistances(float* fStart, float* fEnd, float* fFogZ) = 0;

	// Hooks for firing PIX events from outside the Material System...
	virtual void BeginPIXEvent(unsigned long color, const char* szName) = 0;
	virtual void EndPIXEvent() = 0;
	virtual void SetPIXMarker(unsigned long color, const char* szName) = 0;

	// Enables and disables for Alpha To Coverage
	virtual void EnableAlphaToCoverage() = 0;
	virtual void DisableAlphaToCoverage() = 0;

	// Computes the vertex buffer pointers 
	virtual void ComputeVertexDescription(unsigned char* pBuffer, VertexFormat_t vertexFormat, MeshDesc_t& desc) const = 0;

	virtual bool SupportsShadowDepthTextures(void) = 0;

	virtual void SetDisallowAccess(bool) = 0;
	virtual void EnableShaderShaderMutex(bool) = 0;
	virtual void ShaderLock() = 0;
	virtual void ShaderUnlock() = 0;

	virtual ImageFormat GetShadowDepthTextureFormat(void) = 0;

	virtual bool SupportsFetch4(void) = 0;
	virtual void SetShadowDepthBiasFactors(float fShadowSlopeScaleDepthBias, float fShadowDepthBias) = 0;

	// ------------ New Vertex/Index Buffer interface ----------------------------
		virtual void BindVertexBuffer(int nStreamID, IVertexBuffer* pVertexBuffer, int nOffsetInBytes, int nFirstVertex, int nVertexCount, VertexFormat_t fmt, int nRepetitions = 1) = 0;
		virtual void BindIndexBuffer(IIndexBuffer* pIndexBuffer, int nOffsetInBytes) = 0;
		virtual void Draw(MaterialPrimitiveType_t primitiveType, int nFirstIndex, int nIndexCount) = 0;
		// ------------ End ----------------------------


			// Apply stencil operations to every pixel on the screen without disturbing depth or color buffers
			virtual void PerformFullScreenStencilOperation(void) = 0;

			virtual void SetScissorRect(const int nLeft, const int nTop, const int nRight, const int nBottom, const bool bEnableScissor) = 0;

			// nVidia CSAA modes, different from SupportsMSAAMode()
			virtual bool SupportsCSAAMode(int nNumSamples, int nQualityLevel) = 0;

			//Notifies the shaderapi to invalidate the current set of delayed constants because we just finished a draw pass. Either actual or not.
			virtual void InvalidateDelayedShaderConstants(void) = 0;

			// Gamma<->Linear conversions according to the video hardware we're running on
			virtual float GammaToLinear_HardwareSpecific(float fGamma) const = 0;
			virtual float LinearToGamma_HardwareSpecific(float fLinear) const = 0;

			//Set's the linear->gamma conversion textures to use for this hardware for both srgb writes enabled and disabled(identity)
			virtual void SetLinearToGammaConversionTextures(ShaderAPITextureHandle_t hSRGBWriteEnabledTexture, ShaderAPITextureHandle_t hIdentityTexture) = 0;

			virtual ImageFormat GetNullTextureFormat(void) = 0;

			virtual void BindVertexTexture(VertexTextureSampler_t nSampler, ShaderAPITextureHandle_t textureHandle) = 0;

			// Enables hardware morphing
			virtual void EnableHWMorphing(bool bEnable) = 0;

			// Sets flexweights for rendering
			virtual void SetFlexWeights(int nFirstWeight, int nCount, const MorphWeight_t* pWeights) = 0;

			virtual void FogMaxDensity(float flMaxDensity) = 0;

			// Create a multi-frame texture (equivalent to calling "CreateTexture" multiple times, but more efficient)
			virtual void CreateTextures(
				ShaderAPITextureHandle_t* pHandles,
				int count,
				int width,
				int height,
				int depth,
				ImageFormat dstImageFormat,
				int numMipLevels,
				int numCopies,
				int flags,
				const char* pDebugName,
				const char* pTextureGroupName) = 0;

			virtual void AcquireThreadOwnership() = 0;
			virtual void ReleaseThreadOwnership() = 0;

			virtual bool SupportsNormalMapCompression() const { Assert(!"This has all been removed."); return false; }

			// Only does anything on XBox360. This is useful to eliminate stalls
			virtual void EnableBuffer2FramesAhead(bool bEnable) = 0;

			virtual void SetDepthFeatheringPixelShaderConstant(int iConstant, float fDepthBlendScale) = 0;

			// debug logging
			// only implemented in some subclasses
			virtual void PrintfVA(char* fmt, va_list vargs) = 0;
			virtual void Printf(PRINTF_FORMAT_STRING const char* fmt, ...) = 0;
			virtual float Knob(char* knobname, float* setvalue = NULL) = 0;
			// Allows us to override the alpha write setting of a material
			virtual void OverrideAlphaWriteEnable(bool bEnable, bool bAlphaWriteEnable) = 0;
			virtual void OverrideColorWriteEnable(bool bOverrideEnable, bool bColorWriteEnable) = 0;

			//extended clear buffers function with alpha independent from color
			virtual void ClearBuffersObeyStencilEx(bool bClearColor, bool bClearAlpha, bool bClearDepth) = 0;

			// Allows copying a render target to another texture by specifying them both.
			virtual void CopyRenderTargetToScratchTexture(ShaderAPITextureHandle_t srcRt, ShaderAPITextureHandle_t dstTex, Rect_t* pSrcRect = NULL, Rect_t* pDstRect = NULL) = 0;

			// Allows locking and unlocking of very specific surface types.
			virtual void LockRect(void** pOutBits, int* pOutPitch, ShaderAPITextureHandle_t texHandle, int mipmap, int x, int y, int w, int h, bool bWrite, bool bRead) = 0;
			virtual void UnlockRect(ShaderAPITextureHandle_t texHandle, int mipmap) = 0;

			// Set the finest mipmap that can be used for the texture which is currently being modified. 
			virtual void TexLodClamp(int finest) = 0;

			// Set the Lod Bias for the texture which is currently being modified. 
			virtual void TexLodBias(float bias) = 0;

			virtual void CopyTextureToTexture(ShaderAPITextureHandle_t srcTex, ShaderAPITextureHandle_t dstTex) = 0;

};

#define INVALID_SHADER	(void*)( 0xFFFFFFFF )
