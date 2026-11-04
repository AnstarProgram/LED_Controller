@echo off
rem ============================================================
rem  NYBanner PC - sborka NYBanner_PC.exe (v3.3)
rem  Dvoynoj klik okno NE zakroetsya samo: vsegda zhdet klavishu.
rem  Vsy podrobnosti: log_build.txt ryadom s batnikom.
rem ============================================================
setlocal EnableExtensions
set "SRCDIR=%~dp0"
cd /d "%SRCDIR%."
set "LOG=%SRCDIR%log_build.txt"

>"%LOG%" echo NyBanner PC build / %date% %time% / papka: %CD%

if not exist "%SRCDIR%nybanner_pc.py" (
  if exist "F:\pc_app\nybanner_pc.py" (
    set "SRCDIR=F:\pc_app\"
    cd /d "%SRCDIR%."
  )
)
if not exist "%SRCDIR%nybanner_pc.py" (
  >>"%LOG%" echo [!] NET nybanner_pc.py
  echo  [!] NE NAIDEN fail nybanner_pc.py. Razmestite ego RYADOM s batnikom:
  echo        F:\pc_app\build_exe.bat   +   F:\pc_app\nybanner_pc.py
  goto :finish 1
)

rem --- ishchem Python ---
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
  echo      (galochka "Add python.exe to PATH").
  goto :finish 1
)
>>"%LOG%" echo [*] Python: %PY%

rem --- PyInstaller: vzyaem EGO ot TUDA ZHE, gde Python (path ne vazhen) ---
"%PY%" -m PyInstaller --version >>"%LOG%" 2>&1
if errorlevel 1 (
  echo  [*] Stavyu PyInstaller...
  "%PY%" -m pip install --user pyinstaller pillow >>"%LOG%" 2>&1
)
"%PY%" -m PyInstaller --version >>"%LOG%" 2>&1
if errorlevel 1 (
  >>"%LOG%" echo [!] PyInstaller ne ustanovlen
  echo  [!] Ne udalos postavit PyInstaller. Smotrite log_build.txt
  goto :finish 1
)

echo  [*] Sobirayu EXE (1-3 minute)...
"%PY%" -m PyInstaller --noconfirm --clean --onefile --windowed ^
  --name NYBanner_PC --hidden-import=PIL._tkinter_finder ^
  "%SRCDIR%nybanner_pc.py" >>"%LOG%" 2>&1
set "RC=%errorlevel%"
if not "%RC%"=="0" (
  >>"%LOG%" echo [!] PyInstaller verdnul kod %RC%
  echo  [!] Sborka upala (kod %RC%). Otkroyte log_build.txt - tam tekst oshibki.
  goto :finish %RC%
)

if exist "%SRCDIR%dist\NYBanner_PC.exe" copy /y "%SRCDIR%dist\NYBanner_PC.exe" "%SRCDIR%NYBanner_PC.exe" >>"%LOG%" 2>&1
if not exist "%SRCDIR%NYBanner_PC.exe" (
  >>"%LOG%" echo [!] EXE ne najden posle sborki
  echo  [!] Gotovyj fail ne sozdan. Smotrite log_build.txt
  goto :finish 1
)
>>"%LOG%" echo [+] OK: %SRCDIR%NYBanner_PC.exe
echo.
echo  [+] GOTOVO! Fajl: %SRCDIR%NYBanner_PC.exe  - zapuskajte dvoynym klikom.

:finish
set "RC=%~1"
if not defined RC set "RC=0"
echo.
echo ============================================
echo  Konets raboty (kod %RC%). Podrobnosti: log_build.txt
echo ============================================
pause
exit /b %RC%
