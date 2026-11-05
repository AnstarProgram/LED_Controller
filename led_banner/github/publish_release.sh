#!/usr/bin/env bash
# ============================================================
# Публикация APK в GitHub Releases.
# Использование:
#   ./publish_release.sh v1.0.0 [путь/к/apk]
# По умолчанию берёт ../NYBanner-release.apk
#
# Требуется: gh CLI (https://cli.github.com) ИЛИ токен GITHUB_TOKEN.
# Первый запуск: gh auth login  (логин через браузер, 30 секунд)
# ============================================================
set -euo pipefail

TAG="${1:-v1.0.0}"
APK="${2:-$(dirname "$0")/../NYBanner-release.apk}"
OWNER_REPO="${OWNER_REPO:-}"   # например "ivan-petrov/nybanner"; пусто = репозиторий, к которому привязан gh

if [ ! -f "$APK" ]; then
  echo "ERROR: APK не найден: $APK"; exit 1
fi

NAME="NYBanner-${TAG}.apk"
cp "$APK" "$NAME"
sha256sum "$NAME" | awk '{print $1}' > "${NAME}.sha256"

echo "==> Готовятся файлы: $NAME ($(du -h "$NAME" | cut -f1)) и ${NAME}.sha256"

BODY="Приложение для Android для управления новогодней LED-лентой 8x96 на ESP32.

* Редактор текста с живым превью 1:1
* Wi-Fi (автопоиск по UDP) и Bluetooth (устройство NY-BANNER)
* Буфер на 5 минут, автодозагрузка клипов за 5 сек до конца
* Вкладка Файлы со ссылками на этот релиз

SHA-256: $(cat ${NAME}.sha256)"

if command -v gh >/dev/null 2>&1; then
  REPO_ARG=()
  [ -n "$OWNER_REPO" ] && REPO_ARG=(-R "$OWNER_REPO")
  if gh release view "$TAG" "${REPO_ARG[@]}" >/dev/null 2>&1; then
    echo "==> Релиз $TAG уже существует — обновляю ассеты"
    gh release upload "$TAG" "${REPO_ARG[@]}" --clobber "$NAME" "${NAME}.sha256"
  else
    gh release create "$TAG" "${REPO_ARG[@]}" \
      --title "NYBanner ${TAG}" \
      --notes "$BODY" \
      "$NAME" "${NAME}.sha256"
  fi
  echo "==> Готово! Страница релиза:"
  gh release view "$TAG" "${REPO_ARG[@]}" --web 2>/dev/null || true
else
  echo "gh CLI не найден. Воспользуйтесь веб-интерфейсом:"
  echo "  1. Откройте https://github.com/<ваш_логин>/<ваш_репозиторий>/releases/new"
  echo "  2. Тег: $TAG, название: 'NYBanner ${TAG}'"
  echo "  3. Перетащите файлы $NAME и ${NAME}.sha256 в окно Assets"
  echo "  4. Нажмите Publish release"
fi

rm -f "$NAME" "${NAME}.sha256"
