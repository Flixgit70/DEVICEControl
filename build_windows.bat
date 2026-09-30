@echo off
setlocal
cd /d "%~dp0"

rem Optional: set JUCE_DIR=C:\path\to\JUCE to use a local checkout.
rem Without JUCE_DIR, the pinned JUCE release is fetched automatically.
cmake --preset windows-release
if errorlevel 1 exit /b %errorlevel%

cmake --build --preset windows-release --parallel
if errorlevel 1 exit /b %errorlevel%

ctest --preset windows-release
if errorlevel 1 exit /b %errorlevel%

endlocal
