@echo off
rem ================= NYBanner PC -> EXE =================
rem Требуется Python 3.10+ с https://python.org (галочка "Add to PATH")
python --version || (echo Установите Python и повторьте & pause & exit /b 1)
pip install pyinstaller
cd /d "%~dp0"
pyinstaller --noconfirm --clean --windowed --onefile --name NYBanner_PC nybanner_pc.py
copy /y dist\NYBanner_PC.exe ..\site_files\ 2>nul
echo.
echo ГОТОВО: pc_app\dist\NYBanner_PC.exe  - запускать двойным кликом
pause
