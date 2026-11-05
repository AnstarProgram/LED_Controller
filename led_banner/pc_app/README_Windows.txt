NYBanner PC - INSTRUKCIYA (v3.3)

POCHEMU OKNO ZAKRIVALOS I CTO ISPRAVLENO:
- start.bat i build_exe.bat teper VSEGDA zhdayut natiatiya klavishi (pause)
  i pishut polnyj zhurnal v fail ryadom: log_start.txt / log_build.txt.
- Esli programa za-pushaetsya i srazu zakryvaetsya - otkroyte
  nybanner_error.log (sozdaetsya avtomaticheski) - tam tochka sboya.
- Sborka EXE bolshe ne zavisit ot PATH: bat vyzyvaet
  "python -m PyInstaller" napryamuyu.

BYSTRYJ START (bez sborki EXE):
1. Rasarkhivayte arkhiv, papku pc_app peremestite v F:\pc_app
2. Dvojnoy klik start.bat  -> otkroetsya programa NYBanner PC.
   (Nuzhen Python 3.9-3.13 s https://python.org, galochka
    "Add python.exe to PATH" pri ustanovke.)
3. Hotite odin fayl NYBanner_PC.exe? Dvojnoy klik build_exe.bat,
   zhdajte 1-3 minuty, rezultat v F:\pc_app\NYBanner_PC.exe

YESLI PYTHOON 3.14 NA WINDOWS 7/8:
PyInstaller na 3.14 trebuyet Windows 10+. Postavte Python 3.12
(papkoy eshe odnoy versiyey on ne мешается) ili polzuites
start.bat / saytom esp32_web (vse funkcii v brauzere).

SKACHAT S VEBA (zerkalo onlyfiles.com, est reklama):
APK telefona: https://onlyfiles.com/wuwFwTon3fXs/nybanner-debug.apk
Release APK : https://onlyfiles.com/wbwjwIo93ebb/nybanner-release.apk
Arkhiv      : (obnovlyaetsya skriptom site/make_mirror_links.sh)
