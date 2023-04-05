#ifndef ISHADERSYSTEM_HA
#define ISHADERSYSTEM_HA
#include "tier1/utldict.h"
#include "tier1/KeyValues.h"
#include "tier1/strtools.h"
#include "tier1/utlstack.h"
#include "tier1/utlbuffer.h"
#include <materialsystem/itexture.h>
#include <materialsystem/imaterialvar.h>
#include <materialsystem/imaterial.h>
#include <shaderapi/ishadershadow.h>
#include <shaderapi/ishaderdynamic.h>
#include "shaderapi/IShaderDevice.h"
#include "shaderapi/ishaderapi.h"

typedef int ShaderAPITextureHandle_t;
typedef int ShaderDLL_t;
typedef short StateSnapshot_t;

struct ShaderRenderState_t
{
	// These are the same, regardless of whether alpha or color mod is used
	int				m_Flags;	// Can't shrink this to a short
	VertexFormat_t	m_VertexFormat;
	VertexFormat_t	m_VertexUsage;
	MorphFormat_t	m_MorphFormat;

	// List of all snapshots
	void* m_pSnapshots; // RenderPassList_t *m_pSnapshots;

};

enum
{
	SHADER_USING_COLOR_MODULATION = 0x1,
	SHADER_USING_ALPHA_MODULATION = 0x2,
	SHADER_USING_FLASHLIGHT = 0x4,
	SHADER_USING_FIXED_FUNCTION_BAKED_LIGHTING = 0x8,
	SHADER_USING_EDITOR = 0x10,
};

abstract_class IShaderSystem
{
public:
	virtual ShaderAPITextureHandle_t GetShaderAPITextureBindHandle(ITexture * pTexture, int nFrameVar, int nTextureChannel = 0) = 0;

	// Binds a texture
	virtual void BindTexture(Sampler_t sampler1, ITexture* pTexture, int nFrameVar = 0) = 0;
	virtual void BindTexture(Sampler_t sampler1, Sampler_t sampler2, ITexture* pTexture, int nFrameVar = 0) = 0;

	// Takes a snapshot
	virtual void TakeSnapshot() = 0;

	// Draws a snapshot
	virtual void DrawSnapshot(bool bMakeActualDrawCall = true) = 0;

	// Are we using graphics?
	virtual bool IsUsingGraphics() const = 0;

	// Are we using graphics?
	virtual bool CanUseEditorMaterials() const = 0;
};

abstract_class IShaderSystemInternal : public IShaderInit, public IShaderSystem
{
public:
	// Initialization, shutdown
	virtual void		Init() = 0;
	virtual void		Shutdown() = 0;
	virtual void		ModInit() = 0;
	virtual void		ModShutdown() = 0;

	// Methods related to reading in shader DLLs
	virtual bool		LoadShaderDLL(const char* pFullPath) = 0;
	virtual void		UnloadShaderDLL(const char* pFullPath) = 0;

	// Find me a shader!
	virtual IShader* FindShader(char const* pShaderName) = 0;

	// returns strings associated with the shader state flags...
	virtual char const* ShaderStateString(int i) const = 0;
	virtual int ShaderStateCount() const = 0;

	// Rendering related methods

	// Create debugging materials
	virtual void CreateDebugMaterials() = 0;

	// Cleans up the debugging materials
	virtual void CleanUpDebugMaterials() = 0;

	// Call the SHADER_PARAM_INIT block of the shaders
	virtual void InitShaderParameters(IShader* pShader, IMaterialVar** params, const char* pMaterialName) = 0;

	// Call the SHADER_INIT block of the shaders
	virtual void InitShaderInstance(IShader* pShader, IMaterialVar** params, const char* pMaterialName, const char* pTextureGroupName) = 0;

	// go through each param and make sure it is the right type, load textures, 
	// compute state snapshots and vertex types, etc.
	virtual bool InitRenderState(IShader* pShader, int numParams, IMaterialVar** params, ShaderRenderState_t* pRenderState, char const* pMaterialName) = 0;

	// When you're done with the shader, be sure to call this to clean up
	virtual void CleanupRenderState(ShaderRenderState_t* pRenderState) = 0;

	// Draws the shader
	virtual void DrawElements(IShader* pShader, IMaterialVar** params, ShaderRenderState_t* pShaderState, VertexCompressionType_t vertexCompression,
							   uint32 nMaterialVarTimeStamp) = 0;

	// Used to iterate over all shaders for editing purposes
	virtual int	 ShaderCount() const = 0;
	virtual int  GetShaders(int nFirstShader, int nCount, IShader** ppShaderList) const = 0;
};

abstract_class IMaterialInternal : public IMaterial
{
public:
	// class factory methods
	static IMaterialInternal * CreateMaterial(char const* pMaterialName, const char* pTextureGroupName, KeyValues * pKeyValues = NULL);
	static void DestroyMaterial(IMaterialInternal* pMaterial);

	// If supplied, pKeyValues and pPatchKeyValues should come from LoadVMTFile()
	static IMaterialInternal* CreateMaterialSubRect(char const* pMaterialName, const char* pTextureGroupName,
													KeyValues* pKeyValues = NULL, KeyValues* pPatchKeyValues = NULL, bool bAssumeCreateFromFile = false);
	static void DestroyMaterialSubRect(IMaterialInternal* pMaterial);

	// refcount
	virtual int		GetReferenceCount() const = 0;

	// enumeration id
	virtual void	SetEnumerationID(int id) = 0;

	// White lightmap methods
	virtual void	SetNeedsWhiteLightmap(bool val) = 0;
	virtual bool	GetNeedsWhiteLightmap() const = 0;

	// load/unload 
	virtual void	Uncache(bool bPreserveVars = false) = 0;
	virtual void	Precache() = 0;
	// If supplied, pKeyValues and pPatchKeyValues should come from LoadVMTFile()
	virtual bool	PrecacheVars(KeyValues* pKeyValues = NULL, KeyValues* pPatchKeyValues = NULL, CUtlVector<FileNameHandle_t>* pIncludes = NULL, int nFindContext = MATERIAL_FINDCONTEXT_NONE) = 0;

	// reload all textures used by this materals
	virtual void	ReloadTextures() = 0;

	// lightmap pages associated with this material
	virtual void	SetMinLightmapPageID(int pageID) = 0;
	virtual void	SetMaxLightmapPageID(int pageID) = 0;;
	virtual int		GetMinLightmapPageID() const = 0;
	virtual int		GetMaxLightmapPageID() const = 0;

	virtual IShader* GetShader() const = 0;

	// Can we use it?
	virtual bool	IsPrecached() const = 0;
	virtual bool	IsPrecachedVars() const = 0;

	// main draw method
	virtual void	DrawMesh(VertexCompressionType_t vertexCompression) = 0;

	// Gets the vertex format
	virtual VertexFormat_t GetVertexFormat() const = 0;
	virtual VertexFormat_t GetVertexUsage() const = 0;

	// Performs a debug trace on this material
	virtual bool PerformDebugTrace() const = 0;

	// Can we override this material in debug?
	virtual bool NoDebugOverride() const = 0;

	// Should we draw?
	virtual void ToggleSuppression() = 0;

	// Are we suppressed?
	virtual bool IsSuppressed() const = 0;

	// Should we debug?
	virtual void ToggleDebugTrace() = 0;

	// Do we use fog?
	virtual bool UseFog() const = 0;

	// Adds a material variable to the material
	virtual void AddMaterialVar(IMaterialVar* pMaterialVar) = 0;

	// Gets the renderstate
	virtual ShaderRenderState_t* GetRenderState() = 0;

	// Was this manually created (not read from a file?)
	virtual bool IsManuallyCreated() const = 0;

	virtual bool NeedsFixedFunctionFlashlight() const = 0;

	virtual bool IsUsingVertexID() const = 0;

	// Identifies a material mounted through the preload path
	virtual void MarkAsPreloaded(bool bSet) = 0;
	virtual bool IsPreloaded() const = 0;

	// Conditonally increments the refcount
	virtual void ArtificialAddRef(void) = 0;
	virtual void ArtificialRelease(void) = 0;

	virtual void			ReportVarChanged(IMaterialVar* pVar) = 0;
	virtual uint32			GetChangeID() const = 0;

	virtual bool			IsTranslucentInternal(float fAlphaModulation) const = 0;

	//Is this the queue friendly or realtime version of the material?
	virtual bool IsRealTimeVersion(void) const = 0;

	virtual void ClearContextData(void)
	{
	}

	//easy swapping between the queue friendly and realtime versions of the material
	virtual IMaterialInternal* GetRealTimeVersion(void) = 0;
	virtual IMaterialInternal* GetQueueFriendlyVersion(void) = 0;

	virtual void PrecacheMappingDimensions(void) = 0;
	virtual void FindRepresentativeTexture(void) = 0;

	// These are used when a new whitelist is passed in. First materials to be reloaded are flagged, then they are reloaded.
	virtual void DecideShouldReloadFromWhitelist(IFileList* pFileList) = 0;
	virtual void ReloadFromWhitelistIfMarked() = 0;
};

class CShaderSystem : public IShaderSystemInternal
{
public:
	CShaderSystem();

	// Methods of IShaderSystem
	virtual ShaderAPITextureHandle_t GetShaderAPITextureBindHandle(ITexture* pTexture, int nFrameVar, int nTextureChannel = 0);

	virtual void		BindTexture(Sampler_t sampler1, ITexture* pTexture, int nFrame = 0);
	virtual void		BindTexture(Sampler_t sampler1, Sampler_t sampler2, ITexture* pTexture, int nFrame = 0);

	virtual void		TakeSnapshot();
	virtual void		DrawSnapshot(bool bMakeActualDrawCall = true);
	virtual bool		IsUsingGraphics() const;
	virtual bool		CanUseEditorMaterials() const;

	// Methods of IShaderSystemInternal
	virtual void		Init();
	virtual void		Shutdown();
	virtual void		ModInit();
	virtual void		ModShutdown();

	virtual bool		LoadShaderDLL(const char* pFullPath);
	virtual bool		LoadShaderDLL(const char* pFullPath, const char* pPathID, bool bModShaderDLL);
	virtual void		UnloadShaderDLL(const char* pFullPath);

	virtual IShader* FindShader(char const* pShaderName);
	virtual void		CreateDebugMaterials();
	virtual void		CleanUpDebugMaterials();
	virtual char const* ShaderStateString(int i) const;
	virtual int			ShaderStateCount() const;

	virtual void		InitShaderParameters(IShader* pShader, IMaterialVar** params, const char* pMaterialName);
	virtual void		InitShaderInstance(IShader* pShader, IMaterialVar** params, const char* pMaterialName, const char* pTextureGroupName);
	virtual bool		InitRenderState(IShader* pShader, int numParams, IMaterialVar** params, ShaderRenderState_t* pRenderState, char const* pMaterialName);
	virtual void		CleanupRenderState(ShaderRenderState_t* pRenderState);
	virtual void		DrawElements(IShader* pShader, IMaterialVar** params, ShaderRenderState_t* pShaderState, VertexCompressionType_t vertexCompression, uint32 nVarChangeID);

	// Used to iterate over all shaders for editing purposes
	virtual int			ShaderCount() const;
	virtual int			GetShaders(int nFirstShader, int nMaxCount, IShader** ppShaderList) const;

	// Methods of IShaderInit
	virtual void		LoadTexture(IMaterialVar* pTextureVar, const char* pTextureGroupName, int nAdditionalCreationFlags = 0);
	virtual void		LoadBumpMap(IMaterialVar* pTextureVar, const char* pTextureGroupName);
	virtual void		LoadCubeMap(IMaterialVar** ppParams, IMaterialVar* pTextureVar, int nAdditionalCreationFlags = 0);

	// Used to prevent re-entrant rendering from warning messages
	typedef int SpewType_t;
	void				BufferSpew(SpewType_t spewType, const Color& c, const char* pMsg);

public:
	struct ShaderDLLInfo_t
	{
		char* m_pFileName;
		CSysModule* m_hInstance;
		void* m_pShaderDLL;
		ShaderDLL_t m_hShaderDLL;

		// True if this is a mod's shader DLL, in which case it's not allowed to 
		// override any existing shader names.
		bool m_bModShaderDLL;
		CUtlDict< IShader*, unsigned short >	m_ShaderDict;
	};

public:
	// hackhack: remove this when VAC2 is online.
	void VerifyBaseShaderDLL(CSysModule* pModule);

	// Load up the shader DLLs...
	void LoadAllShaderDLLs();

	// Load the "mshader_" DLLs.
	void LoadModShaderDLLs(int dxSupportLevel);

	// Unload all the shader DLLs...
	void UnloadAllShaderDLLs();

	// Sets up the shader dictionary.
	void SetupShaderDictionary(int nShaderDLLIndex);

	// Cleans up the shader dictionary.
	void CleanupShaderDictionary(int nShaderDLLIndex);

	// Finds an already loaded shader DLL
	int FindShaderDLL(const char* pFullPath);

	// Unloads a particular shader DLL
	void UnloadShaderDLL(int nShaderDLLIndex);

	// Sets up the current ShaderState_t for rendering
	void PrepForShaderDraw(IShader* pShader, IMaterialVar** ppParams,
		ShaderRenderState_t* pRenderState, int modulation);
	void DoneWithShaderDraw();

	// Initializes state snapshots
	void InitStateSnapshots(IShader* pShader, IMaterialVar** params, ShaderRenderState_t* pRenderState);

	// Compute snapshots for all combinations of alpha + color modulation
	void InitRenderStateFlags(ShaderRenderState_t* pRenderState, int numParams, IMaterialVar** params);

	// Computes flags from a particular snapshot
	void ComputeRenderStateFlagsFromSnapshot(ShaderRenderState_t* pRenderState);

	// Computes vertex format + usage from a particular snapshot
	bool ComputeVertexFormatFromSnapshot(IMaterialVar** params, ShaderRenderState_t* pRenderState);

	// Used to prevent re-entrant rendering from warning messages
	void PrintBufferedSpew(void);

	// Gets at the current snapshot
	StateSnapshot_t CurrentStateSnapshot();

	// Draws using a particular material..
	void DrawUsingMaterial(IMaterialInternal* pMaterial, VertexCompressionType_t vertexCompression);

	// Copies material vars
	void CopyMaterialVarToDebugShader(IMaterialInternal* pDebugMaterial, IShader* pShader, IMaterialVar** ppParams, const char* pSrcVarName, const char* pDstVarName = NULL);

	// Debugging draw methods...
	void DrawMeasureFillRate(ShaderRenderState_t* pRenderState, int mod, VertexCompressionType_t vertexCompression);
	void DrawNormalMap(IShader* pShader, IMaterialVar** ppParams, VertexCompressionType_t vertexCompression);
	bool DrawEnvmapMask(IShader* pShader, IMaterialVar** ppParams, ShaderRenderState_t* pRenderState, VertexCompressionType_t vertexCompression);

	int GetModulationSnapshotCount(IMaterialVar** params);

public:
	// List of all DLLs containing shaders
	CUtlVector< ShaderDLLInfo_t > m_ShaderDLLs;

	// Used to prevent re-entrant rendering from warning messages
	typedef void* SpewOutputFunc_t;
	SpewOutputFunc_t m_SaveSpewOutput;

	CUtlBuffer m_StoredSpew;

	// Render state we're drawing with
	ShaderRenderState_t* m_pRenderState;
	unsigned short m_hShaderDLL;
	unsigned char m_nModulation;
	unsigned char m_nRenderPass;

	// Debugging materials
	// If you add to this, add to the list of debug shader names (s_pDebugShaderName) below
	enum
	{
		MATERIAL_FILL_RATE = 0,
		MATERIAL_DEBUG_NORMALMAP,
		MATERIAL_DEBUG_ENVMAPMASK,
		MATERIAL_DEBUG_DEPTH,
		MATERIAL_DEBUG_DEPTH_DECAL,
		MATERIAL_DEBUG_WIREFRAME,

		MATERIAL_DEBUG_COUNT,
	};

	IMaterialInternal* m_pDebugMaterials[MATERIAL_DEBUG_COUNT];
	static const char* s_pDebugShaderName[MATERIAL_DEBUG_COUNT];

	bool			m_bForceUsingGraphicsReturnTrue;
};

typedef intp VertexShader_t;
typedef intp PixelShader_t;

abstract_class IShaderManager
{
public:

	// The current vertex and pixel shader index
	int m_nVertexShaderIndex;
	int m_nPixelShaderIndex;

public:
	// Initialize, shutdown
	virtual void Init() = 0;
	virtual void Shutdown() = 0;

	// Compiles vertex shaders
	virtual IShaderBuffer* CompileShader(const char* pProgram, size_t nBufLen, const char* pShaderVersion) = 0;

	// New version of these methods	[dx10 port]
	virtual VertexShaderHandle_t CreateVertexShader(IShaderBuffer* pShaderBuffer) = 0;
	virtual void DestroyVertexShader(VertexShaderHandle_t hShader) = 0;
	virtual PixelShaderHandle_t CreatePixelShader(IShaderBuffer* pShaderBuffer) = 0;
	virtual void DestroyPixelShader(PixelShaderHandle_t hShader) = 0;

	// Creates vertex, pixel shaders
	virtual VertexShader_t CreateVertexShader(const char* pVertexShaderFile, int nStaticVshIndex = 0, char* debugLabel = NULL) = 0;
	virtual PixelShader_t CreatePixelShader(const char* pPixelShaderFile, int nStaticPshIndex = 0, char* debugLabel = NULL) = 0;

	// Sets which dynamic version of the vertex + pixel shader to use
	FORCEINLINE void SetVertexShaderIndex(int vshIndex);
	FORCEINLINE void SetPixelShaderIndex(int pshIndex);

	// Sets the vertex + pixel shader render state
	virtual void SetVertexShader(VertexShader_t shader) = 0;
	virtual void SetPixelShader(PixelShader_t shader) = 0;

	// Resets the vertex + pixel shader state
	virtual void ResetShaderState() = 0;

	// Returns the current vertex + pixel shaders
	virtual void* GetCurrentVertexShader() = 0;
	virtual void* GetCurrentPixelShader() = 0;

	virtual void ClearVertexAndPixelShaderRefCounts() = 0;
	virtual void PurgeUnusedVertexAndPixelShaders() = 0;

	// The low-level dx call to set the vertex shader state
	virtual void BindVertexShader(VertexShaderHandle_t shader) = 0;
	virtual void BindPixelShader(PixelShaderHandle_t shader) = 0;

#if defined( _X360 )
	virtual const char* GetActiveVertexShaderName() = 0;
	virtual const char* GetActivePixelShaderName() = 0;
#endif

#if defined( DX_TO_GL_ABSTRACTION )
	virtual void DoStartupShaderPreloading() = 0;
#endif
};



#endif