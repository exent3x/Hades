@echo off
setlocal
scons target=template_debug arch=x86_64
if errorlevel 1 (
  echo BUILD FAILED.
  exit /b 1
)
echo BUILD SUCCESSFUL.
if exist project\bin\windows\russian_roguelite.windows.template_debug.x86_64.dll echo DLL: project\bin\windows\russian_roguelite.windows.template_debug.x86_64.dll
endlocal
