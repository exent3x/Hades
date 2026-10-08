@echo off
setlocal
scons target=template_release arch=x86_64
if errorlevel 1 (
  echo BUILD FAILED.
  exit /b 1
)
echo BUILD SUCCESSFUL.
endlocal
