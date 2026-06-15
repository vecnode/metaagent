@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM MetaAgent desktop app — configure, build, and run from repository root.
pushd "%~dp0.."

set "CLEAN_BUILD=0"
if /I "%~1"=="--clean" set "CLEAN_BUILD=1"

set "VCVARS="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "%VSWHERE%" (
  for /f "delims=" %%I in ('"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find VC\Auxiliary\Build\vcvars64.bat') do (
    if not defined VCVARS set "VCVARS=%%I"
  )
)

if defined VCVARS (
  set "INCLUDE="
  set "LIB="
  set "LIBPATH="
  call "%VCVARS%" >nul 2>&1
)

set "CMAKE_EXE="
if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "CMAKE_EXE=%ProgramFiles%\CMake\bin\cmake.exe"
if not defined CMAKE_EXE for /f "delims=" %%I in ('where cmake 2^>nul') do if not defined CMAKE_EXE set "CMAKE_EXE=%%I"

if not defined CMAKE_EXE (
  echo [error] cmake.exe not found.
  goto :error
)

if "%CLEAN_BUILD%"=="1" (
  if exist "build\CMakeCache.txt" del /f /q "build\CMakeCache.txt"
)

echo [1/3] Configuring CMake with METAAGENT_BUILD_APP=ON (Visual Studio 2022 x64)
"%CMAKE_EXE%" -B build-msvc -G "Visual Studio 17 2022" -A x64 -DMETAAGENT_BUILD_APP=ON
if errorlevel 1 goto :error

echo [2/3] Building metaagent-app (Debug)
"%CMAKE_EXE%" --build build-msvc --target metaagent-app -j --config Debug
if errorlevel 1 goto :error

echo [3/3] Running metaagent-app
if not exist ".\build-msvc\app\Debug\metaagent-app.exe" (
  echo ERROR: .\build-msvc\app\Debug\metaagent-app.exe not found.
  goto :error
)

".\build-msvc\app\Debug\metaagent-app.exe"
goto :end

:error
echo Build or run failed.
exit /b 1

:end
popd
