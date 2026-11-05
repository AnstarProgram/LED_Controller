# Публикация APK через GitHub Releases (бесплатная раздача)

GitHub Releases — готовый «сайт для скачивания»: стабильные прямые ссылки,
до 2 ГБ на файл, HTTPS со страницы `https://github.com/<ЛОГИН>/<РЕПО>/releases/latest`.
Именно эти ссылки использует вкладка **«Файлы»** в приложении NYBanner.

> ⚠️ В этом окружении нет вашего GitHub-аккаунта и токена, поэтому сам релиз
> публикуется вами — ниже два способа (веб-интерфейс на 2 минуты и скрипт).

---

## Шаг 0. Создать репозиторий (один раз)

1. Зарегистрируйтесь на https://github.com (если ещё нет аккаунта).
2. Кнопка **New repository** → имя, например `nybanner` → **Public** → Create.
3. Загрузите код проекта:

```bash
cd /workspace/led_banner
git init -b main 2>/dev/null || true
git add firmware_esp32 pc_app android github RUKOVODSTVO_APK.md README* 2>/dev/null
git commit -m "NYBanner v1.0.0: firmware, PC app, Android source" 2>/dev/null || true
git remote add origin https://github.com/<ЛОГИН>/nybanner.git   # <- впишите логин
git push -u origin main
```

*(APK в git не коммитится — он отдаётся как Asset релиза; это правильный способ.)*

---

## Способ 1. Через веб-интерфейс (без установки ничего)

1. Откройте `https://github.com/<ЛОГИН>/nybanner/releases/new`
2. **Choose tag** → создайте тег `v1.0.0`
3. **Release title**: `NYBanner v1.0.0` (текст описания можно скопировать из `RELEASE_TEMPLATE.md`)
4. Перетащите в окно **Attach binaries** два файла:
   - `github/NYBanner-v1.0.0.apk`
   - `github/NYBanner-v1.0.0.apk.sha256`
5. Нажмите **Publish release**.

Готовая ссылка для раздачи людям:

```
https://github.com/<ЛОГИН>/nybanner/releases/download/v1.0.0/NYBanner-v1.0.0.apk
```

Страница релиза (красивая, с описанием):

```
https://github.com/<ЛОГИН>/nybanner/releases/latest
```

---

## Способ 2. Скриптом (для регулярных обновлений)

```bash
# установка gh CLI (один раз)
sudo mkdir -p /etc/apt/keyrings
curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg \
  | sudo tee /etc/apt/keyrings/githubcli-archive-keyring.gpg >/dev/null
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" \
  | sudo tee /etc/apt/sources.list.d/github-cli.list >/dev/null
sudo apt update && sudo apt install gh -y

gh auth login            # логин через браузер, ~30 секунд

cd led_banner/github
chmod +x publish_release.sh
./publish_release.sh v1.0.0          # опубликует ../NYBanner-release.apk под тегом v1.0.0
```

Для каждого нового обновления: соберите release-APK (`gradle assembleRelease`,
скопируйте в `../NYBanner-release.apk`) и запустите `./publish_release.sh v1.0.1`.

---

## Как привязать это к приложению

Во вкладке **«Файлы»** приложения уже есть поле URL. Впишите туда адрес
своего репозитория вида `https://api.github.com/<ЛОГИН>/nybanner/releases` —
телефон покажет список версий и будет качать APK/клипы напрямую с GitHub CDN.

То же самое работает для прошивки: файлы клипов `.bin`/`index.json` можно
класть в Assets того же релиза — ESP32 скачивает их по HTTP-ссылке
(GitHub раздаёт ассеты без авторизации).

## Проверка целостности

Пользователь может сверить хэш:
```
sha256sum NYBanner-v1.0.0.apk        # на телефоне (Termux) или ПК
```
и сравнить с содержимым `NYBanner-v1.0.0.apk.sha256` из Assets.
