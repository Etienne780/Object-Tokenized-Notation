project "OTNFileTest"
    language "C++"
    cppdialect "C++17"

    SetTargetAndObjDirs("%{prj.name}")

    files {
        "src/**.cpp",
        "src/**.c",
        "include/**.h",
        "include/**.hpp",
        "main.cpp"
    }

    includedirs {
        "include",
        "include/%{prj.name}",
        "../OTNFile/include",
    }

    links {
        "OTNFile",
    }

    ApplyCommonConfigs()

    filter "configurations:Debug"
        kind "ConsoleApp"

    filter "configurations:Release"
        kind "ConsoleApp"

    filter "configurations:Distribution"
        kind "ConsoleApp"

    filter {}