workspace "Project"
    architecture "x64"

    configurations { 
        "Debug", 
        "Release",
        "Distribution"
    }

    startproject "OTNFileTest"

--------------------------------------------------------
-- Helper function for consistent directory structure
--------------------------------------------------------
function SetTargetAndObjDirs(projectName)
    local root = _MAIN_SCRIPT_DIR

    -- Executables and DLLs
    targetdir(root .. "/build/bin/" .. projectName .. "/%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}")

    -- Intermediate build files
    objdir(root .. "/build/intermediates/" .. projectName .. "/%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}")

    
    --------------------------------------------------------
    -- Automatically move all .lib files to centralized folder
    --------------------------------------------------------
    filter "kind:StaticLib"
        targetdir(root .. "/build/lib/" .. projectName .. "/%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}")
    filter {}
end

--------------------------------------------------------
-- Helper for common configuration settings
--------------------------------------------------------
function ApplyCommonConfigs()
    filter "system:windows"
        filter "action:vs*"
            flags { "MultiProcessorCompile" }

    filter "configurations:Debug"
        defines { "DEBUG" }
        runtime "Debug"
        symbols "On"
        buildoptions { "/MDd" }

    filter "configurations:Release"
        defines { "NDEBUG" }
        runtime "Release"
        optimize "Full"
        buildoptions { "/MD" }

    filter "configurations:Distribution"
        defines { "NDEBUG" }
        runtime "Release"
        optimize "Full"
        buildoptions { "/MD" }

    filter {}
end

include "OTNFile"
include "OTNFileTest"

--------------------------------------------------------
-- Custom clean action
--------------------------------------------------------
newaction {
    trigger = "clean",
    description = "Remove all binaries, intermediates, Visual Studio files, and Makefile artifacts",
    execute = function()
        print("Removing binaries...")
        os.rmdir("./build/bin")

        print("Removing intermediates...")
        os.rmdir("./build/intermediates")

        print("Removing libraries...")
        os.rmdir("./build/lib")
        os.rmdir("./build")

        print("Removing Visual Studio files...")
        os.rmdir("./.vs")
        os.rmdir("./.vscode")
        os.remove("**.sln")
        os.remove("**.vcxproj")
        os.remove("**.vcxproj.filters")
        os.remove("**.vcxproj.user")
        os.remove("**.slnLaunch.user")

        print("Removing Makefile artifacts...")
        os.remove("Makefile") 
        os.remove("Makefile.*")

        os.remove("**.o")
        os.remove("**.d")
        os.remove("**.a") 
        os.remove("**.so")
        os.remove("**.exe")

        print("Done.")
    end
}