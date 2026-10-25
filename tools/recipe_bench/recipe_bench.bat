@echo off
rem Animation Recipe Bench: a local web page for nwn5e's animation recipes.
rem Double-click to start it (it opens in the browser); close this window to stop it.
rem Options pass through, e.g.  recipe_bench.bat --port 8800  or  --build D:\nwn5e\build
where node >nul 2>nul
if errorlevel 1 (
  echo The Animation Recipe Bench needs Node.js 18 or later: https://nodejs.org
  pause
  exit /b 1
)
node "%~dp0server.js" %*
if errorlevel 1 pause
