dependencies = {
	basePath = "./deps"
}

function dependencies.load()
	local scripts = os.matchfiles(path.join(dependencies.basePath, "premake/*.lua"))
	for _, script in pairs(scripts) do
		local module = script:gsub("%.lua$", "")
		require(module)
	end
end

newoption {
	trigger = "copy-to",
	description = "Copy the built DLL to this directory.",
	value = "PATH"
}

dependencies.load()

workspace "T7Overclocked"
startproject "T7Overclocked"
location "./build"
objdir "%{wks.location}/obj"
targetdir "%{wks.location}/bin/%{cfg.platform}/%{cfg.buildcfg}"

configurations {"Debug", "Release"}
language "C++"
cppdialect "C++20"
architecture "x86_64"
platforms "x64"
systemversion "latest"
symbols "On"
staticruntime "On"
editandcontinue "Off"
warnings "Extra"
characterset "ASCII"

if os.getenv("CI") then
	defines {"CI"}
end

flags {"NoIncrementalLink", "NoMinimalRebuild", "MultiProcessorCompile", "No64BitChecks"}

filter "platforms:x64"
	defines {"_WINDOWS", "WIN32"}
filter {}

filter "configurations:Release"
	optimize "Size"
	buildoptions {"/GL"}
	linkoptions {"/IGNORE:4702", "/LTCG"}
	defines {"NDEBUG"}
filter {}

filter "configurations:Debug"
	optimize "Debug"
	defines {"DEBUG", "_DEBUG"}
filter {}

project "T7Overclocked"
kind "SharedLib"
language "C++"
targetname "T7Overclocked"

pchheader "std_include.hpp"
pchsource "src/client/std_include.cpp"

linkoptions {"/IGNORE:4254", "/IGNORE:4244", "/DYNAMICBASE:NO", "/SAFESEH:NO", "/LARGEADDRESSAWARE", "/LAST:.main", "/PDBCompress"}

files {
	"./src/client/main.cpp",
	"./src/client/std_include.cpp",
	"./src/client/std_include.hpp",
	"./src/client/components/asset_limits.cpp",
	"./src/client/components/asset_limits.hpp",
	"./src/client/game/game.cpp",
	"./src/client/game/game.hpp",
	"./src/client/game/offsets.cpp",
	"./src/client/game/offsets.hpp",
	"./src/client/game/structs.hpp"
}

includedirs {"./src/client", "./src/common", "%{prj.location}/src"}
resincludedirs {"$(ProjectDir)src"}

minhook.import()

if _OPTIONS["copy-to"] then
	postbuildcommands {"copy /y \"$(TargetPath)\" \"" .. _OPTIONS["copy-to"] .. "\""}
end

group "Dependencies"
minhook.project()
