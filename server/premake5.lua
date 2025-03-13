workspace "HLSLVariantLSPServer"
	startproject "Server"
	architecture "x64"
	warnings "extra"
	flags { "MultiProcessorCompile" }
	
	cppdialect "C++20"
	systemversion "latest"
	
	-- Disable C4201 nonstandard extension used: nameless struct/union
	disablewarnings { "4201" }

	-- Link warning suppression
	-- LNK4006: Sympbol already defined in another library will pick first definition
	-- LNK4099: Debugging Database file (pdb) missing for given obj
	-- LNK4098: defaultlib 'library' conflicts with use of other libs; use /NODEFAULTLIB:library
	linkoptions { "-IGNORE:4006,4099,4098" }

	-- Should use: usestandardpreprocessor 'On', but does not work for some reason. So setting it manualy.
	buildoptions { "/Zc:preprocessor" }

	-- Platform
	platforms
	{
		"x64"
	}

	-- Configurations
	configurations
	{
		"Debug",
		"Release"
	}

	filter "configurations:Debug"
		symbols "on"
		runtime "Debug"
		defines
		{
			"MSLP_DEBUG",
		}
	filter "configurations:Release"
		symbols "off"
		runtime "Release"
		optimize "Full"
		defines
		{
			"MSLP_RELEASE",
		}
		-- Disable C100 Unused parameter
		disablewarnings { "4100" }
	filter {}

	-- Compiler option
	filter "action:vs*"
		defines
		{
			"MSLP_VISUAL_STUDIO",
			"_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING",
			"_CRT_SECURE_NO_WARNINGS",
		}
	filter { "action:vs*", "configurations:Debug" }
		defines
		{
			"_CRTDBG_MAP_ALLOC",
		}

	filter "system:windows"
		defines
		{
			"MSLP_PLATFORM_WINDOWS",
		}

	filter {}

function FixSlashes(s)
	return s:gsub("\\", "/")
end

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}-%{cfg.platform}"

wksLocation = "%{wks.location}"

includeDir = {}
includeDir["lsp"] 				= "%{wks.location}/externals/lsp-framework/build/lib/include/"
includeDir["treeSitter"] 		= "%{wks.location}/externals/tree-sitter/lib/include/"

libDir = {}
libDir["lsp"]					= "%{wks.location}/externals/lsp-framework/build/"
libDir["treeSitter"]			= "%{wks.location}/externals/tree-sitter/lib/build/"

include "premake5-helper.lua"

project "HLSLVServer"
	kind "ConsoleApp"
	language "C++"

	-- Targets
	targetdir ("%{wks.location}/Build/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/Build/obj/" .. outputdir .. "/%{prj.name}")

    -- Files to include
	files { GetFiles("Src/") }

	ExternalsIncludes()
	ExternalsLinks()

	-- Copy server release exe to the client's bin folder. 
	filter {"system:windows", "configurations:Release"}
		postbuildcommands {"xcopy /y /d %{wks.location}Build\\bin\\" .. outputdir .. "\\%{prj.name}\\%{prj.name}.exe .\\..\\hlslvariant\\bin\\"}
	filter {}

project "*"