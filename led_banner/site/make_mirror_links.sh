#!/usr/bin/env bash
# Генерирует свежие ссылки-зеркала onlyfiles.com для сайта и печатает
# готовый блок MIRROR = {...} — вставьте его в index.html вместо старого.
# OnlyFiles принимает файлы до 100 МБ, хранит несколько дней, на странице
# скачивания есть реклама (это и есть «зеркало с рекламой»).
# Использование:  bash site/make_mirror_links.sh
set -e
cd "$(dirname "$0")/.."

APK="NYBanner-debug.apk"                 # приложение для телефона
REL="NYBanner-release.apk"               # подписанный релиз (RuStore)
EXE="pc_app/dist/NYBanner.exe"           # собранный .exe (если нет — пропустится)
ZIP="vsy.444.zip"       # весь проект + прошивка

up() { # $1 = файл -> печать ссылки на страницу onlyfiles.com
  local f="$1"
  [ -f "$f" ] || { echo "# (!) файл не найден: $f" >&2; return 1; }
  curl -sS --max-time 300 -X POST "https://api.onlyfiles.com/v1/upload" \
    -H "Origin: https://onlyfiles.com" -H "Referer: https://onlyfiles.com/" \
    -H "User-Agent: Mozilla/5.0" -F "file=@$f" \
  | python3 -c "import sys,json;d=json.load(sys.stdin);print(d['data']['file']['url']['full']) if d.get('status') else (print('ERR',d,file=sys.stderr) or sys.exit(1))"
}

echo "// ---- блок для вставки в index.html (onlyfiles, живут несколько дней) ----"
echo "const MIRROR = {"
echo "  apk:      \"$(up "$APK" || echo '')\","
echo "  release:  \"$(up "$REL" || echo '')\","
echo "  exe:      \"$(up "$EXE" || echo '')\","
echo "  firmware: \"$(up "$ZIP" || echo '')\""
echo "};"
