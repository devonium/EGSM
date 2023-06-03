
local depthWriteMat = CreateMaterial( "egsm/depthmat", "DepthWrite", {
  ["$no_fullbright"] = "1",
  ["$color_depth"] = "1",
  ["$alpha_test"] = "0",
} )

local FuckOffFog = GetRenderTarget( "FuckOffFog", ScrW(), ScrH() , 4,
	MATERIAL_RT_DEPTH_SEPARATE,
	4 + 8 + 1 + 65536,
	0,
	29)

local bDepthPass = false
local halo = {}
local render = render

MaterialOverride 	   =  	MaterialOverride or render.MaterialOverride
BrushMaterialOverride  = 	BrushMaterialOverride or render.BrushMaterialOverride 
WorldMaterialOverride  = 	WorldMaterialOverride or render.WorldMaterialOverride 
ModelMaterialOverride  = 	ModelMaterialOverride or render.ModelMaterialOverride 

local MaterialOverride = MaterialOverride
local BrushMaterialOverride = BrushMaterialOverride
local WorldMaterialOverride = WorldMaterialOverride
local ModelMaterialOverride = ModelMaterialOverride

function render.MaterialOverride(m)
	if bDepthPass then return end
	MaterialOverride(m)
end

function render.BrushMaterialOverride(m)
	if bDepthPass then return end
	BrushMaterialOverride(m)
end

function render.WorldMaterialOverride(m)
	if bDepthPass then return end
	WorldMaterialOverride(m)
end
function render.ModelMaterialOverride(m)
	if bDepthPass then return end
	ModelMaterialOverride(m)
end

local function dummyFn() end

local PushRenderTarget  = 	render.PushRenderTarget
local PopRenderTarget  = 	render.PopRenderTarget
local rClear  = render.Clear
local depthRT = render.GetResolvedFullFrameDepth()
local hRender = dummyFn
local BeginDepthPass = shaderlib.BeginDepthPass
local EndDepthPass = shaderlib.EndDepthPass

local function PreDrawEffectsHK() 
	if bDepthPass then return end
	bDepthPass = true
	halo.Render = dummyFn

	
	MaterialOverride( depthWriteMat )
	BrushMaterialOverride( depthWriteMat )
	WorldMaterialOverride( depthWriteMat )
	ModelMaterialOverride( depthWriteMat )
	
	PushRenderTarget(FuckOffFog)
		BeginDepthPass()
			rClear(0,0,0,0)
			PopRenderTarget()
			
			rClear(0,0,0,0)
			PopRenderTarget()
			
			render.RenderView()
		EndDepthPass()
	PopRenderTarget()

	MaterialOverride()
	BrushMaterialOverride()
	WorldMaterialOverride()
	ModelMaterialOverride()
	
	halo.Render = hRender
	bDepthPass = false
end

local rClearDepth = render.ClearDepth

function shaderlib.__INIT()
	halo = halo
	hRender = halo.Render
	hook.Add("PreDrawViewModel", "EGSM_ImTooLazy", rClearDepth)
	hook.Add( "PreDrawEffects", "EGSM_ImTooLazy", PreDrawEffectsHK)
end
