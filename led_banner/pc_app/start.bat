@echo off
rem ============================================================
rem  NYBanner PC - PRYAMOJ ZAPUSK BEZ SBORKI EXE (v3.2)
rem  Rabotaet srazu, esli Python uzhe ustanovlen.
rem  Put' na vashej mashine: F:\pc_app\start.bat
rem ============================================================

setlocal EnableExtensions
set "SRCDIR=%~dp0"
cd /d "%SRCDIR%."

if not exist "%SRCDIR%nybanner_pc.py" (
  if exist "F:\pc_app\nybanner_pc.py" (
    set "SRCDIR=F:\pc_app\"
    cd /d "%SRCDIR%."
  )
)
if not exist "%SRCDIR%nybanner_pc.py" (
  echo  [!] NE NAIDEN nybanner_pc.py - polozhite ego ryadom s start.bat
  pause
  exit /b 1
)

set "PY="
py -3 --version >nul 2>&1 && set "PY=py -3"
if not defined PY (
  python --version >nul 2>&1 && set "PY=python"
)
if not defined PY (
  for %%P in (
    "%LOCALAPPDATA%\Programs\Python\Python314\python.exe"
    "%LOCALAPPDATA%\Programs\Python\Python313\python.exe"
    "%LOCALAPPDATA%\Programs\Python\Python312\python.exe"
    "C:\Program Files\Python314\python.exe"
    "C:\Program Files\Python313\python.exe"
  ) do if not defined PY if exist %%P set "PY=%%~P"
)
if not defined PY (
  echo  [!] Python ne najden. Ustanovite s https://python.org
  echo      (galochka "Add Python to PATH").
  pause
  exit /b 1
)

rem --- tkinter i PIL ne objazatelny: pilims tolko esli est' ---
"%PY%" -c "import tkinter" >nul 2>&1 || (
  echo  [!] V etom Python net tkinter. Pri ustanovke Python
  echo      vkluchite komponenten "tcl/tk and IDLE", libo
  echo      ustanovite polnuyu versiyu s python.org.
  pause
  exit /b 1
)

echo  Zapuskayu NYBanner PC...
start "" "%PY%" "%SRCDIR%nybanner_pc.py"
exit /b 0
