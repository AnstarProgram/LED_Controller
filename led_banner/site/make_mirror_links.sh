#!/usr/bin/env bash
# Генерирует свежие ссылки-зеркала tmpfiles.org для сайта и печатает
# готовый блок MIRROR = {...} — вставьте его в index.html вместо старого.
# Использование:  bash site/make_mirror_links.sh
set -e
cd "$(dirname "$0")/.."

APK="NYBanner-debug.apk"           # или github/NYBanner-v1.0.0.apk
EXE="pc_app/dist/NYBanner.exe"     # путь к собранному .exe (если нет — пропустится)
ZIP="../NYBanner_vse_v1.0.zip"     # весь проект / прошивка

up() { # $1 = файл -> печать прямой ссылки
  local f="$1"
  [ -f "$f" ] || { echo "# (!) файл не найден: $f" >&2; return 1; }
  local raw
  raw=$(curl -sS -F "file=@$f" https://tmpfiles.org/api/v1/upload)
  # отдаём страницу-обёртку (кнопка Download) — прямой /download/ удаляет файл сразу
  echo "$raw" | grep -o 'https://tmpfiles.org/[0-9]*/[^"]*' | sed 's|tmpfiles.org/|tmpfiles.org/download/|'
}

echo "// ---- блок для вставки в index.html (свежие, живут ~1 час) ----"
echo "const MIRROR = {"
echo "  apk:      \"$(up "$APK" || echo '')\","
echo "  exe:      \"$(up "$EXE" || echo '')\","
echo "  firmware: \"$(up "$ZIP" || echo '')\""
echo "};"
