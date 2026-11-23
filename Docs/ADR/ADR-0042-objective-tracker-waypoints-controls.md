# ADR-0042: Трекер цели, маркеры на визоре и шпаргалка клавиш

- **Статус:** Accepted · 2026-11-23 (UX-01, план и уточнения 1–6 подтверждены владельцем проекта)
- **Связано:**
  - [ADR-0013](ADR-0013-ui-foundation-commonui.md) — CommonUI, модели визора на событиях;
  - [ADR-0026](ADR-0026-diegetic-visor-resonance-tags-helmet-lamp.md) — диегетический визор, `DA_VisorStyle`;
  - [ADR-0016](ADR-0016-raid-flow-extraction-escrow.md) — фазы рейда и эвакуация;
  - [ADR-0039](ADR-0039-cathedral-altar-arcs.md) — алтарь очищения;
  - [ADR-0032](ADR-0032-hub-skiff-terminals-upgrades.md), [ADR-0041](ADR-0041-contract-picks-sector.md) — терминалы хаба, цены с бонусом ядер;
  - арт-библия (`Docs/Art/Art_Direction_Bible_v0.1.md`, §4E: Хищник без маркера).

## Контекст
- После первого домашнего плейтеста главная боль: в хабе и в рейде непонятно, что делать сейчас и куда идти.
- В процедурных отсеках игрок теряет зону эвакуации и алтарь. Клавиши инструментов, гаджета и голоса нигде не написаны.

## Решение
1. **Цель — чистая функция фактов** (`HollowObjectiveRules`, HollowUI).
   - **Факты:** рейд или хаб, хост или гость, открыт ли верстак или шкафчик, фаза рейда и остаток её времени, жив ли игрок, поднималось ли ядро, есть ли поднятое неочищенное ядро и свободный алтарь, идёт ли ритуал, цены.
   - **Шаги** (тексты EN через LOCTEXT, уточнение 1):

     | Шаг | Текст | Маркеры |
     |---|---|---|
     | HubPrepare | 1/2 Check the workbench or the locker | CONTRACTS, WORKBENCH, LOCKER |
     | HubLaunch (хост) | 2/2 Go to the contracts console in the cockpit to launch the raid | те же |
     | HubWaitHost (гость) | 2/2 Go to the contracts console: the host launches the raid | те же |
     | Deploying | Deploying... | — |
     | FindCore | Find the Aether core in the vault | VAULT, EVAC |
     | PurifyOrExtract | Purify the core at the Altar (1,250 cr) or carry it to extraction (500 cr) | ALTAR, EVAC |
     | GuardRitual | Guard the Altar until the ritual ends | ALTAR, EVAC |
     | DeliverCore | Deliver the core to the extraction zone | EVAC |
     | HoldZone | Hold the zone until departure 0:42 | EVAC |
     | Down / RaidOver | You are down: the squad can still extract / Raid over | — |

   - Цены — `FHollowExtractionResolver::ItemEarnings` с бонусом ядер профиля этого игрока, как на экране контрактов. С апгрейдом игрок видит 1,500 / 600.
2. **Модель цели** `UHollowObjectiveModel` живёт в `UHollowVisorContext` и собирает факты только из событий:
   - `UHollowRaidStateComponent::OnStateChanged`;
   - ядра: `OnCarryStateChanged`, `OnLootItemChanged`;
   - алтари: новый `AHollowRitualAltar::OnAltarStateChanged` (на каждой машине, из `ApplyState`);
   - `UHollowHubTerminalSubsystem::OnTerminalOpened/Closed` своего игрока;
   - тег `Hollow.State.Dead` на своём ASC;
   - появление актёров (`AddOnActorSpawnedHandler`) — на клиенте так приходят реплицированные ядро, зона и алтарь.
   - Отметка «ядро поднималось» — своя у каждого ядра и сбрасывается с новым `RaidId`. Подключение посреди рейда невозможно (join только в хабе), так что её видят все машины.
   - Тик не используется. Таймер раз в секунду перерисовывает обратный отсчёт, только пока идёт HoldZone.
3. **Маркеры** (уточнения 2–3).
   - `UHollowWaypointLayerWidget` — полноэкранный слой визора над стеклом и под телеметрией. В нём пул `W_VisorWaypoint`: ромб на месте цели или шеврон у края, подпись «EVAC 42 m».
   - Таймер `DA_VisorStyle.WaypointRateHz` (12 Гц, допустимо 1–30, проверяет валидатор) работает, только пока есть что показать.
   - Под открытым экраном терминала маркеры убираются. Ближе 2,5 м маркер не показывается.
   - **Математика** — чистые функции `HollowWaypointMath`:
     - `Project` — горизонтальный FOV, как в движке по умолчанию;
     - `ScreenDirection` — точка точно сзади даёт стрелку вниз и не дёргается;
     - `Place` — внутри отступа `WaypointEdgeMargin` маркер стоит на месте цели, иначе на краю со стрелкой;
     - `Metres`.
   - **Что отмечается:**
     - VAULT — ближайшее ядро, которое ещё не поднимали;
     - ALTAR — сиденье алтаря, пока он не потрачен;
     - EVAC — ближайшая зона эвакуации;
     - в хабе — по терминалу каждого вида.
   - Врагов маркеры не показывают никогда.
   - **Полосы компаса нет:** верх по центру занят строкой эвакуации.
4. **Сеть** (уточнение 4).
   - Зона эвакуации и алтарь стали `bAlwaysRelevant`, ядро было таким с S3-02. Иначе дальний клиент не получает цель маркера.
   - Все три актёра dormant (`DORM_DormantAll`) и по одному на рейд: новых байтов между изменениями нет.
5. **Шпаргалка** (уточнение 5).
   - `UHollowControlsHintWidget` (`W_ControlsHint`) стоит под Резонансом.
   - **Заголовок** — «[H] CONTROLS»; на геймпаде «[View] CONTROLS».
   - **Строки** — по одной на действие, клавиши выровнены в колонку (моноширинный шрифт визора, `HollowControlsHintRules::Lines`).
   - **Данные** `DA_ControlsHint` (`UHollowControlsHintSet`): действие, подпись, подписи по классам; пустая подпись скрывает строку. Отметка «только в рейде» нужна инструментам, гаджету и заправке.
     - Инженер видит Plasma cutter / Welder / Pulse mine.
     - Оккультист видит Aether lantern / Ritual ward, строки сварки у него нет.
   - **Клавиши** — с текущего устройства через `QueryKeysMappedToAction`. Сменилось устройство — строки перестраиваются сами.
   - **Переключатель** — `IA_ControlsHint` (H / `Gamepad_Special_Left`), UI-действие визора, как системное меню (`InputMode = Game`, `bConsumeInput = false`).
   - Состояние хранится в `UHollowGameUserSettings::bControlsHintShown`; по умолчанию шпаргалка развёрнута.
6. **Контент.** `create_visor_assets.py` создаёт:
   - `M_UI_WaypointMarker` — UI-материал без текстур, ромб или шеврон по параметру `Shape`, цвет вершин; два MI;
   - `W_VisorObjective`, `W_VisorWaypoint`, `W_VisorWaypoints`, `W_ControlsHint`, `DA_ControlsHint`, `IA_ControlsHint` и его клавиши в `IMC_UI`;
   - новые поля `DA_VisorStyle`; W_Visor (раскладка v4).
   - Новых скриптов нет, их по-прежнему 34.
7. **Инструменты:**
   - `Hollow.UI.Dump` печатает строку цели и строки трекера, маркеров и шпаргалки;
   - лог «Objective of …: …» — на каждой смене шага;
   - новый режим смоука `playthrough-mac.sh guide`.

## Последствия
- (+) Каждое событие рейда сразу меняет строку цели и маркеры на всех машинах; сеть не добавляет ни байта, кроме релевантности двух dormant-актёров.
- (+) Новый шаг — это строка в правилах и тест. Новый вид места — значение enum, вид в `DA_VisorStyle` (валидатор требует ровно один) и выбор цели в модели.
- (−) На headless-клиенте (`-nullrhi`) у слоя нет размера, и маркеры не ставятся; цель и шпаргалка работают. Расстановку проверяют рендерящий хост и автотест.
- (−) Подписи клавиш пока текстовые, иконок нет (ADR-0013).

## Проверка
- Автотесты:
  - `Hollow.UI.Objective.Rules`;
  - `Hollow.UI.Waypoint.Math`;
  - `Hollow.UI.Visor.Objective` — настоящие ядро, алтарь, зона и W_Visor: все шаги рейда, цены с бонусом, обратный отсчёт по таймеру;
  - `Hollow.UI.Visor.HubObjective` — три терминала, маркеры уходят под экран;
  - `Hollow.UI.Visor.ControlsHint`.
- **Смоук `guide --net 150`** на собранной игре — PASS:
  - хост и гость проходят все шаги хаба и рейда в Соборе;
  - маркеры на рендерящем хосте с метрами и стрелками;
  - H и View сворачивают шпаргалку, подписи меняются на глифы геймпада;
  - эвакуация 1000 → 2250.
- **Регрессия:** все 16 режимов `playthrough-mac.sh` и soak 20 мин — PASS.
