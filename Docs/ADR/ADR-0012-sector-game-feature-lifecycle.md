# ADR-0012: Жизненный цикл сектор-GFP (Registered при старте, Active на время рейда)

- **Статус:** Accepted · 2026-11-19 (S1-08, подтверждено владельцем проекта)
- **Связано:** ADR-0001 (сектора как content-only GFP), ADR-0003 (манифест сектора), ADR-0010 · [Docs/QA/LevelMetrics.md](../QA/LevelMetrics.md)

## Контекст
- По ADR-0001 контент каждого сектора, включая Сектор I, оформлен как content-only GameFeature-плагин, чтобы DLC-пайплайн работал с первого дня.
- Генератору (S2-01) нужен пул комнат сектора на сервере. Сборке (S2-02) нужны те же комнаты на каждом клиенте: геометрия строится локально из манифеста.
- Состояние GameFeature-плагина локально для процесса и не реплицируется. В PIE несколько GameInstance живут в одном процессе.

## Решение
1. **Плагин** `Plugins/GameFeatures/HollowSector_<Name>/`:
   - content-only, без модулей, поэтому собирается на Mac и Win64 без оговорок (ADR-0009);
   - в дескрипторе `ExplicitlyLoaded: true`, `EnabledByDefault: false`, `BuiltInInitialFeatureState: "Registered"`; в `.uproject` плагин включён явно;
   - `UGameFeatureData` лежит в `/<Plugin>/<Plugin>` и добавляет каталог `/<Plugin>/Data` к типу `HollowRoom`.
2. **Тип `HollowRoom`** объявлен в `DefaultGame.ini` проекта: базовый класс и правило `AlwaysCook`, без каталогов в `/Game`. Каталоги приносит каждый сектор при регистрации. Правила кука берутся из проекта (так работает `AddGameFeatureToAssetManager`).
3. **Состояния:**
   - **Registered** при старте на каждой машине: контент смонтирован, id комнат известны AssetManager, ничего не загружено;
   - **Active** на время рейда: хост выбирает сектор, клиенты узнают его с манифестом (S2-02) и активируют у себя;
   - после рейда снова **Registered** (`UnloadGameFeaturePlugin(..., bKeepRegistered=true)`).
4. **`UHollowSectorFeatureSubsystem`** (HollowGame, `UEngineSubsystem`): `ActivateSector` / `DeactivateSector` со счётчиком ссылок, чтобы GameInstance в одном процессе (PIE-кооп) не деактивировали сектор друг другу. `GetSectorRooms` возвращает комнаты сектора, отсортированные по `PrimaryAssetId` (детерминизм, CLAUDE.md §5). Dev-команды: `Hollow.Sector.Activate|Deactivate|Status`.
5. **Политика GameFeatures** пока движковая (`UDefaultGameFeaturesProjectPolicies`). Своя (`UHollowGameFeaturePolicy`, проверка владения Steam DLC в `IsPluginAllowed`) появится вместе с первым DLC.
6. **Привязка «контракт → сектор»** появится вместе с контрактами. До тех пор рейд-флоу (S2-02) берёт сектор из одного значения в настройках.

## Последствия
- (+) Сектор I проходит тот же путь, что будущие DLC: регистрация, активация, деактивация. Сектор можно отключить, убрав плагин.
- (+) Комнаты доступны генератору сразу после старта, без загрузки.
- (−) Новый сектор нельзя создать скриптом «с нуля» в одну операцию. Пока нет `GameFeatureData`, плагин падает в `ErrorRegistering` и не монтирует контент. Для нового сектора используем мастер редактора (New Plugin → Game Feature (Content Only)) или временно ставим в дескрипторе `ExplicitlyLoaded: false` на первый прогон скрипта. Так создан Сектор II «Собор Реактора» (S7-01, [ADR-0038](ADR-0038-cathedral-sector-two-storey-rooms.md)).
- (−) Переименование уровней скриптом в unattended-режиме блокирует диалог AssetRenameManager о ссылках из CDO. Перенос уровней между точками монтирования делаем через `duplicate_asset` + **явное сохранение** + удаление оригинала (S1-08).
- (?) Что cooked-сборка содержит контент плагина, на Mac не проверено: это проверяет `package-win64.cmd` (`Docs/QA/SteamLiveTest.md`).
