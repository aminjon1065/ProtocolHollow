# ADR-0024: Упакованный Steam-билд — идентичность, подпись Mac без песочницы, AppID, меню, bug bash

- **Статус:** Accepted · 2026-11-21 (S4-06, план и уточнения 1–8 подтверждены владельцем проекта)
- **Связано:** [ADR-0005](ADR-0005-profile-authority.md) (профиль на машине игрока) · [ADR-0006](ADR-0006-online-stack-steam.md) (Steam, вход только в хаб) · [ADR-0009](ADR-0009-cross-platform-toolchain.md) (Mac и Win64) · [ADR-0013](ADR-0013-ui-foundation-commonui.md) (CommonUI) · [ADR-0016](ADR-0016-raid-flow-extraction-escrow.md) (рейд и escrow) · [ADR-0023](ADR-0023-scalability-presets-perf-capture.md) (первый пакетный билд) · [NetworkChecklist.md](../QA/NetworkChecklist.md) · [KnownIssues.md](../QA/KnownIssues.md)

## Контекст
E1 и E2 требуют упакованного билда на двух машинах (Mac ↔ Windows) через Steam-инвайт. Перед S4-06:
- **Песочница.** Движок по умолчанию подписывал Mac-сборки с App Sandbox: Development — `Sandbox.Server`, Shipping — `Sandbox.NoNet`, без сети. Shipping-билд под Mac не смог бы играть по сети. Сборка в песочнице не пишет лог вне контейнера и мешает связи со Steam.
- **Идентичность.** Идентификатор пакета был `com.YourCompany.ProtocolHollow`, имени компании и копирайта не было.
- **Steam в Shipping.** Shipping берёт AppID из define `UE_PROJECT_STEAMSHIPPINGID`, но плагин Steam в движке из лаунчера уже скомпилирован, и define до него не доходит. Глобальные define с установленным движком несовместимы.
- **Меню не было:** хост, инвайт и старт рейда работали только из консоли.
- **Скрипта упаковки Mac не было.** Архив без `-package` содержит один бинарник; артефакты упаковки (`Build/Mac/FileOpenOrder`, `PackageVersionCounter`) лезли в git.

## Решение
1. **Идентичность** (уточнение 1, значения по умолчанию):
   - `CodeSigningPrefix=com.protocolhollow` → идентификатор пакета `com.protocolhollow.ProtocolHollow`;
   - `GeneralProjectSettings`: `CompanyName` и `CopyrightNotice` «Protocol: Hollow», `Description`, версия 0.2.0;
   - `Info.Template.plist`: категория `public.app-category.action-games` и копирайт.
2. **Mac без App Sandbox** (уточнение 5). Для Development и Shipping подпись идёт одним файлом `Build/Mac/Resources/Hollow.entitlements` с пустым набором прав.
   - Сгенерированные движком `Sandbox.*.entitlements` игнорируются git.
   - Hardened runtime и нотаризация (нужен Developer ID) — когда будем раздавать билд на чужие Mac.
3. **Steam и AppID 480** (уточнение 2):
   - в разработке `bRelaunchInSteam=False` — игру запускаем сами;
   - тестовый Shipping под Windows получает `Build/Steam/steam_appid.txt` (480) рядом с exe через `RuntimeDependencies` в `HollowNetwork.Build.cs`;
   - своему AppID понадобятся одна строка в конфиге и удаление этого файла; тест `Hollow.Game.Build.Settings` сверяет AppID конфига и файла.
4. **Минимальное меню** (уточнение 4) — те же пути кода, что у консоли, так что соло и кооп идут одним путём:
   - **`W_MainMenu`** в `L_Boot`: «Host co-op», «Play solo», «Quit» → `UHollowSessionSubsystem::HostSession`.
     - `L_Boot` получил `AHollowFrontEndGameMode` (`BP_GameMode_FrontEnd`): без пешки, в нём только меню. HUD на фронтенд-карте показывает главное меню вместо визора, дебриф (например, SIGNAL LOST) — поверх него.
   - **`W_SystemMenu`** получил «Invite friends», «Start raid» и «Leave session». Какие кнопки видны, решает чистая функция `HollowMenuRules::SystemMenuOptions`:
     - «Invite friends» — у хоста онлайн-сессии;
     - «Start raid» — у хоста в хабе;
     - «Leave session» — в любой сессии.
   - **Старт рейда** вынесен в `HollowRaidLauncher` (Game), его вызывают и кнопка, и `Hollow.Raid.Start`.
   - **Отступление от S2-06:** кнопки добавляются в существующий WBP новым хелпером `UHollowWidgetAuthoring::InsertWidget`; в C++ привязки `BindWidgetOptional`.
5. **Упаковка Mac** (уточнение 7) — `Build/Scripts/package-mac.sh`:
   - BuildCookRun с `-package`; итог — самодостаточный `Packages/Mac/Development/ProtocolHollow.app` (953 МБ) или `Packages/Mac/Shipping/ProtocolHollow-Mac-Shipping.app` (791 МБ);
   - лог собранной Development-игры — `~/Library/Logs/ProtocolHollow/ProtocolHollow.log`, сохранения — `~/Library/Application Support/Epic/ProtocolHollow/Saved/SaveGames`;
   - шлюз «ноль предупреждений проекта», как у Windows-скрипта; компиляцию Shipping без упаковки проверяет `build-editor-mac.sh --shipping`;
   - сам импортирует плейсхолдеры.
6. **Bug bash** (уточнение 6) — `Build/Scripts/playthrough-mac.sh` на упакованном билде. Меню управляются клавишами, как у игрока: Esc, крестовина, A.
   - **solo** (E2): главное меню → соло → хаб → рейд → ядро → эвакуация → дебриф → хаб → «Leave session» → главное меню.
   - **coop:** хост и 3 клиента, два рейда подряд. В первом эвакуируются все, во втором клиенты гибнут, а хост эвакуируется. Сверяются хеши сектора, итоги и профили на каждой машине.
   - **soak:** 20 минут, 4 игрока, живой ИИ, действие каждую минуту; CSV на весь рейд.
     - Игроки неуязвимы (`Hollow.Dev.Invulnerable All`): иначе ИИ убивает неподвижных ботов за минуты.
     - Точный тег `Hollow.State.Invulnerable` отменяет урон любого типа в `UHollowDamageExecution`. Дочерний `.Kinetic` у Хищника по-прежнему действует только через своё правило.
   - Найденное в S4-06 исправлено (см. «Последствия»), остальное — в [KnownIssues.md](../QA/KnownIssues.md).
7. **Живой прогон** Mac ↔ Windows по [NetworkChecklist.md](../QA/NetworkChecklist.md) закрывает E1; для E2 то же делает соло-путь. На обеих машинах Development (уточнение 3), Shipping — одна проверка под Windows.

## Последствия
- (+) **Найдено и исправлено в S4-06:**
  - сетевой Shipping под Mac был невозможен из-за песочницы;
  - собранная игра на Mac не писала логов;
  - дебриф не закрывался клавишей Back (Esc / B) — только кнопкой, хотя системное меню Back закрывает;
  - soak без неуязвимости обрывался гибелью ботов;
  - **на Mac в оконном полном экране (режим по умолчанию) меню не принимало ни одного клика.** Нашёл владелец при первом ручном запуске. Ошибка движка: без выбранного монитора окно ставится по `FMacWindow::GetFullScreenInfo`, который даёт Y ниже экрана. `UHollowGameUserSettings` теперь запоминает основной монитор и переставляет окно по его `DisplayRect`; кода под платформу нет;
  - **игровая цель в Shipping не компилировалась** (на Mac и Win64): отладочный вызов StateTree существует только с gameplay debugger. Теперь он под `#if WITH_GAMEPLAY_DEBUGGER` (флаг движка, не платформы), а `build-editor-mac.sh --shipping` проверяет Shipping тем же шлюзом предупреждений.

  Вместе с ADR-0023 это 7 проблем, которые видны только в собранной игре.
- (+) **E2 подтверждён на собранном Mac-билде** через меню, без ошибок в логах. Кооп-цикл двух рейдов с 4 игроками — тоже без ошибок, хеши секторов совпали.
- (+) **Mac Shipping** собран, упакован без песочницы, 60 секунд работает без падений. Steam в нём не инициализируется: нет `steam_appid.txt`, сетевой Shipping — только под Windows, KnownIssues № 22.
- (+) **20-минутный soak с 4 игроками прошёл:** 92.9 FPS, 0 рывков, память стабильна, 0 ошибок ([KnownIssues.md](../QA/KnownIssues.md)).
- (+) `.gitignore` скрывает артефакты упаковки; коммитятся только `Hollow.entitlements` и `Info.Template.plist`.
- (−) **Кооп-переноска** (S3-06) вырезана, поэтому E8 покрыт только соло-переноской. Второй игрок на уже несомом ядре получает отказ, без вреда.
- (−) **Автопрогоны идут в окне и жмут геймпадом**, поэтому ввод мышью в полноэкранном режиме они не видят. Ручной пункт «мышь в полноэкранном режиме» вынесен в NetworkChecklist.
- (+) **Клавиатура и мышь в меню** проверены настоящими событиями macOS на собранном билде: стрелки и Enter, клик в полноэкранном и оконном режимах. Синтетический `Hollow.UI.PressKey Enter` кнопки CommonUI не нажимает, поэтому автоматика жмёт геймпадом.
- (?) **E1** закрывается только живым прогоном на двух машинах.
