@echo off
rem Pre-build step: bake NPC voice lines with Piper (Tools/bake_voices.py).
rem Skips quietly when the Piper virtualenv isn't installed so builds on a
rem machine without TTS still succeed. Override the venv with PIPER_VENV.
setlocal
if "%PIPER_VENV%"=="" set "PIPER_VENV=C:\Dev\Tools\piper-venv"
set "PY=%PIPER_VENV%\Scripts\python.exe"
if not exist "%PY%" (
    echo bake_voices: Piper venv not found at %PIPER_VENV%, skipping voice bake
    exit /b 0
)
"%PY%" "%~dp0bake_voices.py" %*
exit /b 0
