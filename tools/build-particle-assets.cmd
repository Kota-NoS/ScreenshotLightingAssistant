@echo off
setlocal EnableDelayedExpansion
rem Codex can expose both PATH and Path; MSBuild rejects that duplicate pair.
set "SLA_BUILD_PATH=!PATH!"
set "Path="
set "PATH=!SLA_BUILD_PATH!"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64 >nul
if errorlevel 1 exit /b %errorlevel%

"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" ^
  -S tools\sparkle_nif_builder ^
  -B tools\sparkle_nif_builder\build-0.3.0 ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DNIFLY_ROOT=S:\Code\CodexProjects\2026-09-11\new-chat\PyNifly\NiflyDLL\external\nifly
if errorlevel 1 exit /b %errorlevel%

"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" ^
  --build tools\sparkle_nif_builder\build-0.3.0 ^
  --config Release ^
  --target particle_field_nif_builder inspect_nif
exit /b %errorlevel%
