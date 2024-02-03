local depthWriteMat = CreateMaterial( "egsm/depthmat", "DepthWrite", {
  ["$no_fullbright"] = "1",
  ["$color_depth"] = "1",
  ["$alpha_test"] = "0",
} )

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
local RenderView = render.RenderView
local rClearDepth = render.ClearDepth

local function PreDrawEffectsHK() 
	if bDepthPass then return end
	bDepthPass = true
	
	halo.Render = dummyFn
	
	MaterialOverride( depthWriteMat )
	BrushMaterialOverride( depthWriteMat )
	WorldMaterialOverride( depthWriteMat )
	ModelMaterialOverride( depthWriteMat )
	
	BeginDepthPass()
		rClear(0,0,0,0) 
		
		rClear(0,0,0,0)
		
		rClear(0,0,0,0) 

		RenderView()
	EndDepthPass()

	MaterialOverride()
	BrushMaterialOverride()
	WorldMaterialOverride()
	ModelMaterialOverride()
	
	halo.Render = hRender
	bDepthPass = false
end

local zero_vec = Vector(0,0,0)
local min,max = zero_vec, zero_vec

function shaderlib.__INIT()
	halo = halo
	hRender = halo.Render

	hook.Add("PreDrawViewModel", "!!!EGSM_ImTooLazy", function() if bDepthPass then rClearDepth() end end)	
	hook.Add("NeedsDepthPass", "!!!EGSM_ImTooLazy", PreDrawEffectsHK)
	
	hook.Add("PostDraw2DSkyBox", "!!!EGSM_ImTooLazy", function()
		local rt = render.GetRenderTarget()
		if !rt or rt:GetName() != "egsm_skyboxrt" 
		then  
			return
		end
		cam.Start3D( zero_vec, EyeAngles() )
		min,max = game.GetWorld():GetModelBounds()
	
		render.SetMaterial( depthWriteMat )
		
		render.DrawBox( zero_vec, angle_zero, max, min, color_white )
		cam.End3D()
	end)	
end