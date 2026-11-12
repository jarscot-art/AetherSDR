@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0hf5a-candidate-launch.ps1"
set "candidateExit=%ERRORLEVEL%"
if not "%candidateExit%"=="0" pause
exit /b %candidateExit%
