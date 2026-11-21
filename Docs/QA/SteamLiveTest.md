# Живой Steam-тест: Mac ↔ Windows на двух аккаунтах (S1-03 и дальше — еженедельно)

> Цель: инвайт через Steam → вход в хаб → оба игрока видят друг друга. Сессии — `UHollowSessionSubsystem` (ADR-0006), команды — `Hollow.Session.*` (только Development; консоль открывается клавишей `~`, на русской раскладке это `ё`).

## Подготовка (один раз)
1. **Два Steam-аккаунта**, добавленные друг другу в друзья: A — на Mac, B — на Windows. Один аккаунт не может зайти сам к себе.
2. Steam запущен и залогинен на обеих машинах. В Steam → Settings → In Game включён оверлей.
3. **Windows-ноутбук:** Visual Studio 2022 с MSVC 14.44.35211+ (или 14.50.35723+), Windows SDK 10.0.22621, UE **5.8.4** (тот же хотфикс, что на Mac), Git + Git LFS. Клон по короткому пути: `git clone https://github.com/aminjon1065/ProtocolHollow.git C:\Dev\ProtocolHollow`, затем `git lfs pull`.
   - Подходит и Visual Studio 2026. В Visual Studio Installer → «Отдельные компоненты» нужны: **.NET Framework 4.8 SDK** (без него UBT падает с `RulesError`: «Could not find NetFxSDK install dir»), **MSVC v14.50 (VS 2026 18.0)** (UBT выберет его сам, более новые 14.52+ Epic не проверял), **Windows 11 SDK 10.0.22621**.
   - **Smart App Control выключен** (Безопасность Windows → Управление приложениями/браузером). Иначе Windows блокирует каждый собранный неподписанный бинарник, начиная с `ProtocolHollowModuleRules.dll` (`0x800711C7`).
   - Файл подкачки не меньше объёма RAM (лучше 16–32 ГБ): при 16 ГБ RAM кук и линковка упираются в лимит выделенной памяти.

## Сборка
- **Windows:** `Build\Scripts\run-tests-win64.cmd`. Проверяет MSVC-сборку и тесты, ожидаем `OK`, код 0.
- **Windows:** `Build\Scripts\package-win64.cmd`. Первая упаковка долгая из-за компиляции шейдеров. Результат: `Packages\Win64\Development\ProtocolHollow.exe`.
- **Mac:** `./Build/Scripts/import-placeholders.sh` (один раз после клона: плейсхолдер-персонажи из шаблонов движка, ADR-0011), затем `./Build/Scripts/build-editor-mac.sh`.
- **Windows:** плейсхолдеры `package-win64.cmd` импортирует сам. Для запуска редактора на ноутбуке выполни `Build\Scripts\import-placeholders.cmd` вручную.

## Проверка кука сектор-GFP (с S1-08)
После `package-win64.cmd` запусти собранную игру с `-log` и в консоли `~` выполни `Hollow.Sector.Status HollowSector_Docks`.
Ожидаем `state=Registered references=0 rooms=8`. Если `rooms=0` или `state=Unknown`, контент плагина не попал в кук: пришли лог `Saved/Logs`.

## Тест 1 — хост Windows, клиент Mac (рекомендуемый первый)
> С S4-06 есть меню (главное: «Host co-op», «Play solo», «Quit»; Esc в хабе: «Invite friends», «Start raid», «Leave session»), а Mac запускает собранную игру `Build/Scripts/package-mac.sh`. Полный прогон E1/E2 — [NetworkChecklist.md](NetworkChecklist.md); консольные команды ниже по-прежнему работают.

1. **Windows:** запусти `ProtocolHollow.exe`. Брандмауэр Windows: «Разрешить» (частные сети). SmartScreen: «Подробнее → Выполнить в любом случае». Откроется главное меню (`L_Boot`).
2. **Mac:** запусти собранную игру:
   ```bash
   ./Build/Scripts/package-mac.sh   # один раз после pull
   open Packages/Mac/Development/ProtocolHollow.app
   ```
   Лог: `~/Library/Logs/ProtocolHollow/ProtocolHollow.log`.
3. **Windows:** «Host co-op» в меню (или в консоли `~`):
   - `Hollow.Session.Host InviteOnly` → загрузится `L_Hub_Dev`.
   - `Hollow.Session.Status` → ожидаем `State=InSession … Role=Host OnlineSubsystem=STEAM`.
4. **Windows:** `Hollow.Session.Invite` (оверлей Steam) → пригласи аккаунт A. Если оверлей не открылся, пригласи из списка друзей Steam: ПКМ по другу → «Пригласить в игру».
5. **Mac:** в чате Steam прими приглашение. Игра на Mac должна быть уже запущена (под AppID 480 Steam иначе стартует Spacewar). Ожидаем загрузку `L_Hub_Dev`.
6. **Проверка:** оба видят второго игрока (ExoFrame с манекеном Manny) и его движение: ходьба, бег на `Shift`, присед на `Ctrl`, прыжок на `Space`. В логах `controlled by … (local|remote)` показывает, кто чем управляет. В логах ожидаем `Join succeeded` у хоста и `Welcomed by server` у клиента.

## Тест 2 — негативные сценарии
- **Хост уходит:** на Windows `Hollow.Session.Leave` → Mac возвращается в `L_Boot`, в логе `Session error NetworkFailure`.
- **Соло:** на Windows `Hollow.Session.Host Solo` → `Hollow.Session.Invite` пишет «Invite UI unavailable». Попытка войти отклоняется сообщением «The host is playing solo.».
- **Смена ролей:** хост — Mac (`Hollow.Session.Host InviteOnly` в консоли `~`), инвайт из списка друзей Steam, клиент — Windows.

## Если что-то не так — пришли логи
- **Mac:** `~/Library/Logs/ProtocolHollow/ProtocolHollow.log` (собранная игра)
- **Windows:** `Packages\Win64\Development\ProtocolHollow\Saved\Logs\ProtocolHollow.log`

| Симптом | Вероятная причина |
|---|---|
| `OnlineSubsystem=NULL` в `Hollow.Session.Status` | Steam не запущен или не залогинен до старта игры |
| Ошибка про несовместимую версию сети | Разные хотфиксы UE или `ProjectVersion`. Обе машины должны быть на 5.8.4, код синхронизирован через `git pull` |
| Инвайт приходит, но вход не начинается | Игра на принимающей стороне не была запущена до принятия инвайта |
| Таймаут подключения | Брандмауэр Windows заблокировал приложение: разреши его в «Брандмауэр → Разрешение взаимодействия с приложениями» |
