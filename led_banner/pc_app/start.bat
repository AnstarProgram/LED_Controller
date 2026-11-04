@echo off
rem ============================================================
rem  NYBanner PC - pryamoj zapusk (v3.3)
rem  Dvoynoj klik okno NE zakroetsya samo: vsegda zhdet klavishu.
rem  Vsy podrobnosti pechatautsya v log_start.txt ryadom.
rem ============================================================
setlocal EnableExtensions
set "SRCDIR=%~dp0"
cd /d "%SRCDIR%."
set "LOG=%SRCDIR%log_start.txt"

>"%LOG%" echo NyBanner PC start / %date% %time% / papka: %CD%

if not exist "%SRCDIR%nybanner_pc.py" (
  >>"%LOG%" echo [!] NET nybanner_pc.py v %CD%
  echo  [!] NE NAIDEN nybanner_pc.py - polozhite ego ryadom s start.bat
  goto :finish 1
)

rem --- ishchem Python: py launcher, python, tipichnye puti ---
set "PY="
py -3 --version >>"%LOG%" 2>&1 && set "PY=py -3"
if not defined PY (
  python --version >>"%LOG%" 2>&1 && set "PY=python"
)
if not defined PY (
  for %%P in ("%LOCALAPPDATA%\Programs\Python\Python314\python.exe" "%LOCALAPPDATA%\Programs\Python\Python313\python.exe" "%LOCALAPPDATA%\Programs\Python\Python312\python.exe" "%LOCALAPPDATA%\Programs\Python\Python311\python.exe" "%LOCALAPPDATA%\Programs\Python\Python310\python.exe" "%LOCALAPPDATA%\Programs\Python\Python39\python.exe" "C:\Program Files\Python314\python.exe" "C:\Program Files\Python313\python.exe" "C:\Program Files\Python312\python.exe" "C:\Python314\python.exe" "C:\Python313\python.exe" "C:\Python312\python.exe") do (
    if exist %%P if not defined PY set "PY=%%P"
  )
)
if not defined PY (
  >>"%LOG%" echo [!] Python ne najden
  echo  [!] Python ne najden. Ustanovite s https://python.org
  echo      (pri ustanovke postavit galochku "Add python.exe to PATH").
  goto :finish 1
)
>>"%LOG%" echo [*] Najden Python: %PY%

rem --- tkinter (bez nego GUI ne podnimetsya) ---
"%PY%" -c "import tkinter" >>"%LOG%" 2>&1
if errorlevel 1 (
  >>"%LOG%" echo [!] V etom Python net tkinter
  echo  [!] V etom Python net modulya tkinter.
  echo      Pri ustanovke Python vkluchite komponent "tcl/tk and IDLE",
  echo      libo ustanovite polnuyu versiyu s python.org.
  goto :finish 1
)

echo  Zapuskayu NYBanner PC...
>>"%LOG%" echo [*] Zapuskayu GUI...
start "" "%PY%" "%SRCDIR%nybanner_pc.py"
>>"%LOG%" echo [*] Komanda start vydana. Esli okno ne pojavilos - smotrite nybanner_error.log

:finish
set "RC=%~1"
if not defined RC set "RC=0"
echo.
echo ============================================
echo  Gotovo (kod %RC%). Polnyj tekst: log_start.txt
echo  Oshibki GUI: nybanner_error.txt / nybanner_error.log
echo ============================================
pause
exit /b %RC%
