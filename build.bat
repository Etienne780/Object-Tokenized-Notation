@echo off

set premake=vendor\premake5\premake5.exe

IF "%~1" == "" GOTO PrintHelp
IF "%~1" == "help" GOTO PrintHelp
IF "%~1" == "compile" GOTO Compile

%premake% %1
GOTO Done

:PrintHelp

echo.
echo Enter 'build.bat [action]' where action is one of the following:
echo.
echo   clean             Remove all binaries and intermediate binaries and project files.
echo   compile           Will generate make file then compile using the make file.
echo   codelite          Generate CodeLite project files
echo   gmake             Generate GNU makefiles for POSIX, MinGW, and Cygwin
echo   gmakelegacy       Generate GNU makefiles for POSIX, MinGW, and Cygwin
echo   vs2005            Generate Visual Studio 2005 project files
echo   vs2008            Generate Visual Studio 2008 project files
echo   vs2010            Generate Visual Studio 2010 project files
echo   vs2012            Generate Visual Studio 2012 project files
echo   vs2013            Generate Visual Studio 2013 project files
echo   vs2015            Generate Visual Studio 2015 project files
echo   vs2017            Generate Visual Studio 2017 project files
echo   vs2019            Generate Visual Studio 2019 project files
echo   vs2022            Generate Visual Studio 2022 project files
echo   xcode4            Generate Apple Xcode 4 project files
GOTO Done

:Compile
%premake% vs2022

if defined DevEnvDir goto Build

set "VSINSTALLER=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
set "PATH=%PATH%;%VSINSTALLER%"
set "VSPATH="
for /f "usebackq delims=" %%i in (`"%VSINSTALLER%\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"

if not defined VSPATH (
    echo Visual Studio with C++ build tools not found
    exit /b 1
)

call "%VSPATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -no_logo

:Build
set solutionFile=Project.sln
msbuild /t:Build /p:Configuration=Debug /p:Platform=x64 %solutionFile%
GOTO Done

:Done