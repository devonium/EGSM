PROJECT_GENERATOR_VERSION = 3

newoption({
	trigger = "gmcommon",
	description = "Sets the path to the garrysmod_common (https://github.com/danielga/garrysmod_common) directory",
	value = "path to garrysmod_common directory"
})

newoption({
	trigger = "chrommium",
	description = "Sets the path to the garrysmod_common (https://github.com/danielga/garrysmod_common) directory",
	value = "path to garrysmod_common directory"
})

local gmcommon = assert(_OPTIONS.gmcommon or os.getenv("GARRYSMOD_COMMON"),
	"you didn't provide a path to your garrysmod_common (https://github.com/danielga/garrysmod_common) directory")


include("prepare_files_for_include.lua")


include(gmcommon)


function CreateProject(config)
	assert(type(config) == "table", "supplied argument is not a table!")

	local is_server = config.serverside
	assert(type(is_server) == "boolean", "'serverside' option is not a boolean!")

	local is_vs = string.find(_ACTION, "^vs20%d%d$") ~= nil

	local sourcepath = config.source_path or _OPTIONS["source"] or SOURCE_DIRECTORY
	assert(type(sourcepath) == "string", "source code path is not a string!")

	local manual_files = config.manual_files
	if manual_files == nil then
		manual_files = false
	else
		assert(type(manual_files) == "boolean", "'manual_files' is not a boolean!")
	end

	local _workspace = workspace()

	local abi_compatible = _workspace.abi_compatible
	if not abi_compatible then
		if config.abi_compatible ~= nil then
			abi_compatible = config.abi_compatible
			assert(type(abi_compatible) == "boolean", "'abi_compatible' is not a boolean!")
		else
			abi_compatible = false
		end
	end

	local pch_enabled = false
	if config.pch_header ~= nil or config.pch_source ~= nil then
		assert(config.pch_header ~= nil, "'phc_header' must be supplied when 'pch_source' is supplied!")
		assert(type(config.pch_header) == "string", "'pch_header' is not a string!")

		if is_vs then
			assert(config.pch_source ~= nil, "'pch_source' must be supplied when 'phc_header' is supplied under Visual Studio!")
			assert(type(config.pch_source) == "string", "'pch_source' is not a string!")

			config.pch_source = sourcepath .. "/" .. config.pch_source
			assert(os.isfile(config.pch_source), "'pch_source' file " .. config.pch_source .. " could not be found!")
		end

		pch_enabled = true
	end

	local name = (is_server and "gmsv_" or "gmsv_") .. (config.name or _workspace.name)

	local windows_actions = {
		vs2015 = true,
		vs2017 = true,
		vs2019 = true,
		vs2022 = true,
		install = true,
		clean = true,
		lint = true,
		format = true,
		["export-compile-commands"] = true
	}
	if abi_compatible and os.istarget("windows") and not windows_actions[_ACTION] then
		error("The only supported compilation platforms for this project (" .. name .. ") on Windows are Visual Studio 2015, 2017, 2019 and 2022.")
	end

	if not startproject_defined then
		startproject(name)
		startproject_defined = true
	end

	local _project = project(name)

	assert(_project.directory == nil, "a project with the name '" .. name .. "' already exists!")

	_project.directory = sourcepath
	_project.serverside = is_server

		if abi_compatible then
			removeconfigurations("Debug")
			configurations({"ReleaseWithSymbols", "Release"})
		else
			configurations({"ReleaseWithSymbols", "Release", "Debug"})
		end

		if pch_enabled then
			pchheader(config.pch_header)
			if is_vs then
				pchsource(config.pch_source)
			end
		else
			flags("NoPCH")
		end

		kind("SharedLib")
		language("C++")
		defines({
			"GMMODULE",
			string.upper(string.gsub(_workspace.name, "%.", "_")) .. (_project.serverside and "_SERVER" or "_CLIENT"),
			"IS_SERVERSIDE=" .. tostring(is_server),
			"GMOD_ALLOW_OLD_TYPES",
			"GMOD_ALLOW_LIGHTUSERDATA",
			"GMOD_MODULE_NAME=\"" .. _workspace.name .. "\""
		})
		externalincludedirs(_GARRYSMOD_COMMON_DIRECTORY .. "/include")
		includedirs(_project.directory)

		if not manual_files then
			files({
				_project.directory .. "/*.h",
				_project.directory .. "/*.hpp",
				_project.directory .. "/*.hxx",
				_project.directory .. "/*.c",
				_project.directory .. "/*.cpp",
				_project.directory .. "/*.cxx"
			})
		end

		vpaths({
			["Header files/*"] = {
				_project.directory .. "/**.h",
				_project.directory .. "/**.hpp",
				_project.directory .. "/**.hxx"
			},
			["Source files/*"] = {
				_project.directory .. "/**.c",
				_project.directory .. "/**.cpp",
				_project.directory .. "/**.cxx"
			}
		})

		if abi_compatible then
			local filepath = _GARRYSMOD_COMMON_DIRECTORY .. "/source/ABICompatibility.cpp"
			files(filepath)
			vpaths({["garrysmod_common"] = filepath})
		end

		targetprefix("")
		targetextension(".dll")

		filter({"system:windows", "platforms:x86"})
			targetsuffix("_win32")

			filter({"system:windows", "platforms:x86", "configurations:ReleaseWithSymbols or Debug"})
				linkoptions("/SAFESEH:NO")

		filter({"system:windows", "platforms:x86_64"})
			targetsuffix("_win64")

		filter({"system:linux", "platforms:x86"})
			targetsuffix("_linux")

		filter({"system:linux", "platforms:x86_64"})
			targetsuffix("_linux64")

		filter({"system:macosx", "platforms:x86"})
			targetsuffix("_osx")

		filter({"system:macosx", "platforms:x86_64"})
			targetsuffix("_osx64")

		local autoinstall = _OPTIONS["autoinstall"]
		if autoinstall ~= nil then
			local binDir = #autoinstall ~= 0 and autoinstall or os.getenv("GARRYSMOD_LUA_BIN") or FindGarrysModLuaBinDirectory() or GARRYSMOD_LUA_BIN_DIRECTORY
			assert(type(binDir) == "string", "The path to garrysmod/lua/bin is not a string!")

			filter({})
				postbuildcommands({"{COPY} \"%{cfg.buildtarget.abspath}\" \"" .. binDir .. "\""})
		end

		IncludeHelpers()

		filter({})
end

CreateWorkspace({name = "egsm", abi_compatible = true})
	CreateProject({serverside = false})
    IncludeLuaShared()
    IncludeDetouring()
    IncludeScanning()
    IncludeSDKCommon()
    IncludeSDKTier0()
    IncludeSDKTier1()
    --IncludeSDKTier2()
    --IncludeSDKTier3()
	files
	{
		"source/**.cpp",
		"source/**.h",
		"source/ps/**.inc",
		"source/vs/**.inc",
		"include_files/**.inc",
		"include_files/shader_constant_register_map.h"
	}
	vpaths 
	{ 
		["ps/*"] = {"source/ps/**.inc"},
		["vs/*"] = {"source/vs/**.inc"},
	}
    --IncludeSDKMathlib()
    --IncludeSDKRaytrace()
    --IncludeSteamAPI()
    --IncludeHelpersExtended()