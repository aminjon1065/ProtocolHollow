# ADR-0041: Контракт выбирает сектор; смена биомов в одной сессии

- **Статус:** Accepted · 2026-11-22 (S7-04, план и уточнения 1–8 подтверждены владельцем проекта)
- **Связано:**
  - [ADR-0012](ADR-0012-sector-game-feature-lifecycle.md) — сектор как Game Feature: Registered при старте, Active на время рейда;
  - [ADR-0003](ADR-0003-sector-manifest-replication.md) — манифест сектора и `LayoutHash`;
  - [ADR-0032](ADR-0032-hub-skiff-terminals-upgrades.md) — терминал контрактов, выбор хоста, апгрейд «бонус за ядра»;
  - [ADR-0016](ADR-0016-raid-flow-extraction-escrow.md) — поток рейда и возврат в хаб;
  - [ADR-0038](ADR-0038-cathedral-sector-two-storey-rooms.md) — Собор Реактора, до S7-04 подключался dev-переопределением;
  - [Блок 6](../Architecture/06_Phase2_Content_and_Meta_Plan.md) (S7-04).

## Контекст
- **Блок 6 (S7-04):**
  - контракт Собора `DA_Contract_Cathedral` рядом с `DA_Contract_Docks`;
  - выбор хоста в `W_ContractTerminal` определяет запускаемый сектор, без dev-переопределения `RaidSector`;
  - гости в хабе вживую видят выбранный сектор и награду;
  - Доки → хаб → Собор → хаб в одной сессии: чистая активация и деактивация обоих GFP, без утечек, ошибок и рассинхрона `LayoutHash`;
  - полная регрессия в коопе, FPS в обоих секторах, итог Фазы 2.
- **Что уже было:**
  - выбор контракта реплицировался (`UHollowHubStateComponent`) и уходил в URL рейда (`?Contract=`);
  - манифест несёт `SectorPlugin`, и каждая машина сама активирует плагин из манифеста (`UHollowSectorLoaderComponent`) и отпускает его на `EndPlay`;
  - но сектор сервер брал из `UHollowSectorSettings::RaidSector`, а Собор в смоуках подключался `-ini`-переопределением этого свойства.

## Решение
1. **Контракт — единственный источник сектора.**
   - `AHollowRaidGameMode::InitGame` определяет контракт через `HollowRaidLauncher::ContractFromOptions`:
     - контракт из URL;
     - без опции (карта рейда открыта напрямую, `Hollow.Raid.Start` вне хаба) — `DefaultContract`;
     - неизвестный контракт — Warning и `DefaultContract` (уточнение 5).
   - Контракт получают `UHollowRaidFlowComponent` и `UHollowSectorAuthorityComponent::SetContract`.
   - `SetContract` загружает DA контракта синхронно: это несколько текстов и id, загрузка идёт внутри загрузки карты, рывка нет. Так сектор известен до `BeginPlay`, и это проверяется автотестом без латентных команд.
   - Дальше цепочка прежняя: биом и комнаты плагина → генератор → манифест с `SectorPlugin` → каждая машина активирует плагин.
   - **`UHollowSectorSettings::RaidSector` удалён** (уточнение 2) вместе со строкой `DefaultGame.ini`. Свойство было только Config, не открыто Blueprint.
2. **Данные контракта** (`UHollowContractDefinition`):
   - новые поля `FText SectorName` и `TArray<FPrimaryAssetId> RewardItems` (`AllowedTypes = HollowItem`);
   - `IsDataValid`: `SectorFeature` — GFP с биомом и комнатами, награда названа и состоит из предметов;
   - `Load(Id)` — контракт по id или null.

   | Контракт | Сектор | `SectorName` | Награда |
   |---|---|---|---|
   | `DA_Contract_Docks` (по умолчанию) | `HollowSector_Docks` | Sector I - Orbital Docks | Эфирное ядро |
   | `DA_Contract_Cathedral` | `HollowSector_Cathedral` | Sector II - Reactor Cathedral | Эфирное ядро, очищенное ядро |

   - Оба лежат в `/Game/Hollow/Hub/Contracts` (уточнение 4): хаб видит их, пока плагины только Registered.
   - Создаёт и сверяет со спецификацией `create_hub_assets.py` (список `CONTRACTS`); новых скриптов нет — их по-прежнему 34.
3. **Награда только показывается** (уточнение 1); нового правила экономики нет.
   - Цена — функция `FHollowExtractionResolver::ItemEarnings`: стоимость предмета по каталогу, для Эфирных ядер — плюс бонус апгрейдов корабля. Её же теперь вызывает резолвер эвакуации, так что экран и эвакуация не разойдутся.
   - Каждая машина считает награду по своему профилю: игрок с апгрейдом «бонус за ядра» видит 600 / 1500 кр, без него — 500 / 1250.
4. **Экран контрактов** (`UHollowContractTerminalWidget`, правила `HollowHubScreenRules`):
   - `FHollowContractOffer` — контракт, название сектора и награда этого игрока;
   - строка каждого контракта: «Sector II - Reactor Cathedral. Reward: Aether core 500 cr, Purified Aether core 1,250 cr» и описание;
   - строка роли (`RoleText`) — сводка выбранного контракта: у хоста «Now: …», у гостя «The host chose: …»;
   - обновление вживую — через существующий `OnContractChanged`; WBP не менялся (LFS).
5. **Хаб помнит контракт отряда** (уточнение 3).
   - `ReturnToHub` уходит на `HollowRaidLauncher::MakeHubUrl(Contract)` — карта хаба с `?Contract=`.
   - `UHollowHubStateComponent::BeginPlay` берёт контракт из опций игрового режима, иначе `DefaultContract`.
   - Новая сессия начинается с Доков; после рейда в Собор хост выбирает заново, только чтобы сменить сектор.
6. **Проверка утечек.**
   - `UHollowSectorFeatureSubsystem::CountLoadedPackages` — пакеты плагина в памяти, кроме его GameFeatureData (он загружен, пока плагин Registered).
   - `DescribeSector` (`Hollow.Sector.Status`, логи активации) печатает `references` и `loaded`.
   - В собранной игре после `obj gc` в хабе у обоих секторов должно быть `references=0 loaded=0`. В редакторе этот счёт не показателен: загруженные ассеты держит флаг `RF_Standalone`.
7. **Скрипты:**
   - смоуки `cathedral` и `altar` выбирают контракт хоста `Hollow.Hub.Select DA_Contract_Cathedral` в хабе — настоящий путь вместо `-ini`;
   - новый смоук `biomes` — Доки → хаб → Собор → хаб в одной сессии, он же кооп-регрессия Фазы 2 (уточнение 6);
   - `perf-capture-mac.sh --contract NAME` вместо `--sector`; `perf-e9-win64.ps1 -Contract Docks|Cathedral` для замера на Windows (уточнение 7, на Mac не запускался).

## Последствия
- Новый сектор — это плагин и контракт; никакого конфига проекта трогать не нужно.
- Dev-путь «открыть `L_Raid_Dev` в PIE» всегда идёт в сектор `DefaultContract`. Чтобы проверить Собор в PIE, выберите контракт в хабе.
- Синхронная загрузка DA контракта в `InitGame` — осознанное исключение из правила мягких ссылок: ассет крошечный и грузится вместе с картой.
- Награда на экране — это цены предметов, а не обещание: что реально вынесено, решает эвакуация.

## Проверка
- **Автотесты:**
  - `Hollow.Game.Hub.Data` — оба контракта валидны, сектор каждого — GFP с биомом и комнатами, награды верные, `RaidSector` в конфиге нет;
  - `Hollow.Game.Hub.Terminals` — выбор Собора хостом меняет URL рейда;
  - `Hollow.Game.Raid.ContractSector` — `ContractFromOptions` (Собор, пусто, мусор), URL возврата с контрактом, `InitGame` режима рейда даёт потоку контракт и авторитету сектор;
  - `Hollow.Game.Sector.Switch` — в одном процессе Доки → (выгрузка Доков одновременно с активацией Собора) → вторая ссылка на Собор → хаб → снова Доки → хаб: состояния и ссылки каждого плагина;
  - `Hollow.UI.Hub.Rules` — строки и строка роли с сектором и наградой;
  - `Hollow.UI.Hub.Contracts` — настоящий `W_ContractTerminal`: Собор с ценой этого игрока (бонус ядер), смена контракта на сервере вживую меняет подсветку и сводку.
- **Прогоны** на собранном Mac-билде (подробно — KnownIssues, «Прогоны S7-04»):
  - **`biomes --net 150` — PASS ×3 подряд:** хеши совпали в обоих рейдах; после каждого рейда `references=0 loaded=0` у обоих секторов на обеих машинах; контракт сохраняется, экран гостя следует за хостом; кредиты сошлись до единицы (хост 2750, клиент 2670);
  - **регрессия Фазы 2:** все 16 режимов `playthrough-mac.sh` PASS, включая soak 20 мин; падения — только известные движковые V-6 на старте и L-7 при остановке;
  - **E9 на M4 (ориентир):** Low — Доки 57,3 / Собор 58,9 FPS, High — 24,8 / 27,2 FPS; звук 0,14–0,16 мс.
- **Попутно исправлено в скриптах прогонов (KnownIssues RG-1):** ожидание `listening on port` совпадало со строкой трассировки `Control listening on port 1985`. Замеры ждут `IpNetDriver listening on port`. Смоуки явно ждут главного меню хоста и сообщают, если хост так и не открыл сессию.
