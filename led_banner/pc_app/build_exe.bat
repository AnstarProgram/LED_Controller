@echo off
rem ============================================================
rem  NYBanner PC - sborka NYBanner_PC.exe (v3.2)
rem  Rabotaet ot lyuboj papki, v tom chisle F:\pc_app
rem  Dvoynoj klik -> gotovij fail v etoj zhe papke
rem ============================================================

setlocal EnableExtensions
set "SRCDIR=%~dp0"

chcp 866 >nul 2>&1

cd /d "%SRCDIR%."

echo.
echo  ============================================
echo   NYBanner PC - sborka EXE
echo   Papka: %CD%
echo  ============================================
echo.

rem --- shagi dlya Windows-7/8: PyInstaller ne zapuskaetsya bez KB2533623 ---
ver | find "6.1." >nul 2>&1 && set "OLDWIN=1"
if not defined OLDWIN ver | find "6.2." >nul 2>&1 && set "OLDWIN=1"
if not defined OLDWIN ver | find "6.3." >nul 2>&1 && set "OLDWIN=1"

rem --- nybanner_pc.py ryadom so skriptom? esli net - tipichnye puti ---
if not exist "%SRCDIR%nybanner_pc.py" (
  if exist "F:\pc_app\nybanner_pc.py" (
    set "SRCDIR=F:\pc_app\"
    cd /d "%SRCDIR%."
  )
)
if not exist "%SRCDIR%nybanner_pc.py" (
  echo  [!] NE NAIDEN fail nybanner_pc.py
  echo      Razmestite ego RYADOM s etim bat-fajlom, primer:
  echo        F:\pc_app\build_exe.bat
  echo        F:\pc_app\nybanner_pc.py
  echo  [X] SBORKA PROVALENA: NET ISHODNIKA
  pause
  exit /b 1
)

rem --- ishchem Python: py launcher, python, python3 ---
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
    "%LOCALAPPDATA%\Programs\Python\Python311\python.exe"
    "C:\Program Files\Python314\python.exe"
    "C:\Program Files\Python313\python.exe"
    "C:\Python314\python.exe"
    "C:\Python313\python.exe"
  ) do if not defined PY if exist %%P set "PY=%%~P"
)
if not defined PY (
  echo  [!] Python ne najden. Ustanovite s https://python.org
  echo      i POSTAV'TE galochku "Add Python to PATH".
  pause
  exit /b 1
)

echo  Python:
%PY% --version
echo.

rem --- Glavnoj fix: ispol'zuem TOL'KO "%PY% -m PyInstaller", a ne komandu
rem     "pyinstaller". Pri user-install (site-packages ne writeable) fayl
rem     pyinstaller.exe kladetsya v ...\Roaming\Python\Scripts\, kotoryj
rem     ochen' chastno NOT v PATH - otSyuda byla oshibka
rem     ""pyinstaller" ne yavlyayetsya komandoj". ---
echo  Proveryayu PyInstaller...
%PY% -m PyInstaller --version >nul 2>&1
if errorlevel 1 (
  echo  PyInstaller net v etoj versii Python - ustanavlivayu...
  %PY% -m pip install --user --upgrade pyinstaller
)
%PY% -m PyInstaller --version >nul 2>&1
if errorlevel 1 (
  echo  [!] Ne udalos' ustanovit' PyInstaller. Povernut' vruchnuyu:
  echo        %PY% -m pip install --user pyinstaller
  echo  [X] SBORKA PROVALENA
  pause
  exit /b 1
)
echo  PyInstaller OK.
echo.

if defined OLDWIN (
  echo  [!] Vash Windows starshe 10. Esli sborka upadet s oshibkoj
  echo      "api-ms-win-core-path-l1-1-0.dll" - eta Windows ne podderzhivaet
  echo      novye Python/PyInstaller. Dva vyoda:
  echo        1) ustanovite Python 3.9 i povtorite sborku;
  echo        2) libo ispol'zujte PRYAMOJ ZAPUSK: start.bat
  echo           ^(programma pojdet oknom, bez sozdaniya exe^).
  echo.
)

echo  ======== Sobiraetsya NYBanner_PC.exe ========
%PY% -m PyInstaller --noconfirm --clean --windowed --onefile ^
  --name NYBanner_PC ^
  nybanner_pc.py

if exist "%SRCDIR%dist\NYBanner_PC.exe" (
  copy /y "%SRCDIR%dist\NYBanner_PC.exe" "%SRCDIR%NYBanner_PC.exe" >nul
  echo.
  echo  ============================================
  echo   GOTOVO! Fail: %SRCDIR%NYBanner_PC.exe
  echo   Zapuskat' dvojnym klikom, instaljacija ne nuzhna.
  echo  ============================================
) else (
  echo.
  echo  [!] Sborka ne poluchilas'. Smotrite tekst vyshe.
  echo      Caushche vsego: perezapustite skript posle ustanovki Python.
  echo  [X] SBORKA PROVALENA
)

pause
exit /b 0
