@echo off
chcp 65001 >nul
title NYBanner PC - sborka EXE
rem ============================================================
rem  SKRIPT RABOTAET OT PUTI F:\pc_app (i iz lyuboy drugoy papki):
rem    F:\pc_app\build_exe.bat   <- etot fail
rem    F:\pc_app\nybanner_pc.py  <- programma
rem  Dvoynoj klik - i poluchite F:\pc_app\NYBanner_PC.exe
rem ============================================================

set "SRCDIR=%~dp0"

rem --- esli pybanner_pc.py ne ryadom, ishchem po tipichnym putyam ---
if not exist "%SRCDIR%nybanner_pc.py" (
  if exist "F:\pc_app\nybanner_pc.py" set "SRCDIR=F:\pc_app\"
)
if not exist "%SRCDIR%nybanner_pc.py" (
  if exist "%USERPROFILE%\Desktop\pc_app\nybanner_pc.py" set "SRCDIR=%USERPROFILE%\Desktop\pc_app\"
)
if not exist "%SRCDIR%nybanner_pc.py" (
  echo.
  echo  [!] NE NAIDEN FAIL nybanner_pc.py
  echo      Skript razmeshen: %~f0
  echo      Ryadom so skriptom dolzen lezhat nybanner_pc.py
  echo      Pravil'noe razmeshchenie: F:\pc_app\build_exe.bat + F:\pc_app\nybanner_pc.py
  pause
  exit /b 1
)

cd /d "%SRCDIR%."

echo.
echo  Rabochaya papka: %CD%
echo.

rem --- Python: snachala py launcher, potom python iz PATH ---
set "PY="
py -3 --version >nul 2>&1 && set "PY=py -3"
if not defined PY (
  python --version >nul 2>&1 && set "PY=python"
)
if not defined PY (
  echo  [!] Python ne naden. Ustanovite s https://python.org
  echo      i POSTAV'TE galochku "Add Python to PATH" pri ustanovke.
  pause
  exit /b 1
)
echo  Found Python:
%PY% --version
echo.

rem --- zavisimosti: PyInstaller + Pillow (tkinter uzhe v sostave Python) ---
echo  Proveryayu zavysimosti...
%PY% -m PyInstaller --version >nul 2>&1 || %PY% -m pip install --upgrade pyinstaller pillow
if errorlevel 1 (
  echo  [!] Ne udalos' ustanovit' PyInstaller. Povernut' komanda:
  echo      %PY% -m pip install pyinstaller pillow
  pause
  exit /b 1
)

echo.
echo  ======== Sobiraetsya NYBanner_PC.exe ========
%PY% -m PyInstaller --noconfirm --clean --windowed --onefile ^
  --name NYBanner_PC ^
  --hidden-import=PIL._tkinter_finder ^
  nybanner_pc.py

if exist dist\NYBanner_PC.exe (
  copy /y dist\NYBanner_PC.exe "%SRCDIR%NYBanner_PC.exe" >nul
  echo.
  echo  ============================================
  echo   GOTOVO! Fail: %SRCDIR%NYBanner_PC.exe
  echo   Zapuskat' dvoynym klikom, instaljacija ne nuzhna.
  echo  ============================================
) else (
  echo.
  echo  [!] Sborka ne poluchilas'. Smotrite tekst vyshe.
  echo     Chashche vsego: perezapustite skript posle ustanovki Python.
)
pause
exit /b 0
