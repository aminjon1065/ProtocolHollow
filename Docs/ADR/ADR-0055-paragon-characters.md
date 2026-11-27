# ADR-0055: CHAR-01 — персонажи Paragon вместо заглушек Manny

- **Статус:** Proposed · 2026-11-27 (CHAR-01; план и решения 1–4 приняты владельцем «ок»; решение 3, риг рук от первого лица, в ходе работы не подтвердилось — см. п. 3)
- **Связано:**
  - [ADR-0011](ADR-0011-first-person-rendering-and-placeholders.md) — отрисовка от первого лица; заглушки Manny;
  - [ADR-0027](ADR-0027-art-prep-locks-tool-mount-montages-budget.md) — монтажи врагов, ToolMount;
  - [ADR-0043](ADR-0043-art-kit-fab-staging-cc0.md) — процедурный набор, накладки `DA_Parts_*`, склад Fab;
  - [ADR-0046](ADR-0046-game-feel.md) — покачивание, отдача и нырок оружия от первого лица;
  - [ADR-0051](ADR-0051-fab-integration-overgrown-absorption.md) — конвейер Fab.

## Контекст
- **Заглушки.** ExoFrame, Железный Пилигрим и Эхо-Хищник — это Manny из шаблона движка с процедурными накладками (`DA_Parts_*`), без своих анимаций. Руки от первого лица висят по бокам, поэтому инструмент ездит на камере (AK-4).
- **Паки.** Владелец взял на Fab три бесплатных пака Epic: Paragon Wraith, Riktor и Sevarog (Standard License; слово PARAGON нельзя использовать в названии и рекламе игры).
  - Каждый пак — 1,3–1,5 ГБ: скины, звуки, Cascade, демо-карты, Blueprint и AnimBlueprint Paragon.
  - Материалы — многослойные «Orion» с исходниками текстур 4096.
  - Окно Fab положило паки в открытый ProtocolHollow. Их перенесли на склад `mv`: пути `/Game/ParagonX` не изменились.
- **Что нашла проверка на складе:**
  - у Wraith винтовка встроена в меш (слот `M03_weapon`);
  - скелет Wraith смотрит превью-скином `Ghost_Martian_GDC`;
  - у скелета Sevarog в превью-вложениях Cascade-эффект и меш маски скина;
  - скелет Riktor ссылается на UE4 `Rig`, а этого класса в 5.8 нет;
  - `Jog_Fwd` у Sevarog аддитивный, у Sevarog нет костей `ball_l`/`ball_r`;
  - миграция движка тянет все зависимости, мягкие тоже. Перенести целиком пак с битыми DialogueWave `rename_directory` не может (Riktor).

## Решение
1. **Конвейер Fab** (`fab_manifest.json`, `prepare_fab_assets.py`).
   - **`lean`** (Riktor, Sevarog). Пак не переносится на складе. Пути `keep` пишутся как `/Game/Fab/<Pack>`, а читаются в `staged_at`. Копируется в `Content/<Pack>` игры, игра переносит его под `/Game/Fab/<Pack>` (`rename_directory`). Повторная миграция заменяет эту папку целиком.
   - **`"prepare": "kept"`** (Wraith, уже перенесённый). Готовится только нужная часть, без загрузки всех 1584 ассетов пака.
   - Обе схемы копируют то, что `keep` нужно во время игры: **только жёсткие ссылки**, файлами, как это делает Migrate. Превью-меши и превью-вложения скелетов (мягкие редакторские ссылки на скины и Cascade) остаются на складе. Склад не правится, кроме `hidden_slots`.
   - Пакет, который движок не может загрузить (UE4 `Rig`), не копируется. Ссылающийся на него скелет пересохраняется без ссылки, реестр пересканируется.
   - **`hidden_slots`.** Слоты меша на складе получают пустой маскированный `M_Hollow_Hidden` пака:
     - винтовка Wraith (`M03_weapon`) не рисуется, и её текстуры не едут в игру;
     - длинная цепь-крюк Riktor (`M06`, `M12`, `M13`) не анимирована в `Idle_Combat` и торчала из руки вверх на метры.

     После правки слотов реестр пересканирует меш и материал, иначе новый материал не попадает в копию (CH-9).
   - **Потолки текстур:** `texture_source_max_by_folder` — свои текстуры героя (Wraith 2048, Riktor и Sevarog 1024); общие текстуры слоёв — 512 (`texture_source_max`).
   - **Размер исходника.** Решение о сжатии принимает `ResizeTextureSource` (`AlreadyDone`), а не платформенный размер: группа LOD или `MaxTextureSize` прятали большие исходники.
   - `HOLLOW_FAB_ONLY=<pack,...>` ограничивает прогон указанными паками.
   - В запретные классы добавлены `AnimBlueprint` и `ParticleSystem` (Cascade).
2. **Состав в игре.** По меш, скелет, физассеты, материалы и текстуры героя, плюс анимации:
   - **ExoFrame (Wraith):** `Idle_Combat`, четыре `Jog_*_Combat`, aim offset `Idle_AO_Combat` (9 поз);
   - **Пилигрим (Riktor):** `Idle_Combat`, `Jog_Fwd`, `PrimaryAttack_C_Slow`, `Ability_Lockdown`, `Stun_Start`, `Stun_Loop`;
   - **Хищник (Sevarog):** `Idle`, `Walk_Fwd`, `Sprint_Fwd`, `Swing1_Medium`, `Stun_Start`, `Stun_Loop`, `Speedburst`, `LevelStart`.

   Root motion выключен во всех. 0 звуков, скинов, карт, Blueprint и Cascade.
3. **C++.**
   - **`UHollowCharacterAnimInstance`** (HollowCore) — общий базовый класс AnimBP. `NativeUpdateAnimation` читает пешку: `Speed`, `Direction`, `AimPitch`, `bIsFalling`. Чистая функция `FHollowLocomotion::From` проверяется тестом.
   - **ExoFrame.** Камера на сокете `head` тела (смещение до `Eyes_Position` — данные): покачивание головы при беге даёт анимация тела. `FirstPersonMesh` теперь едет на камере (только владелец, без тени), но меша у него нет.
     - **Риг рук не получился.** План был такой: тот же меш Wraith на камере со скрытой головой, инструмент в сокете `ToolMount` кисти, покачивание GP-01c двигает весь риг. Замер поз показал, что в `Idle_Combat` винтовка у щеки: кисть на 8 см **позади** глаз, плечо в 20 см от неё. Любое жёсткое смещение рига, которое выводит кисть в кадр, выводит туда же плечо, шею или левую руку.
     - Снимки трёх вариантов: глаза в камере — рук не видно; хват в точке `VIEW_OFFSET` — плечи у камеры; наклон вокруг глаз — левая рука перечёркивает кадр.
     - Код рига в `UHollowToolMountComponent` и свойство скрываемых костей удалены, ToolMount не менялся.
     - Инструмент владельца по-прежнему едет на камере (`VIEW_OFFSET`, `create_art_kit.py`). AK-4 остаётся открытым (CH-3): нужны FP-анимации рук или IK.
   - **Тело ExoFrame** держит инструмент в сокете `ToolMount` (`ThirdPersonSocket`), напарники видят его там, где у Wraith была винтовка.
   - **`UHollowCosmeticPartsComponent::TintParameters`.** Цвет класса уходит в векторные параметры собственных материалов тела (и рук, если они есть): MID на каждый слот с таким параметром, яркость `Intensity` из данных. У Wraith это `DeepEmissiveColor`; `ShallowEmissive` по умолчанию тёмный и не тинтуется.
   - **`UHollowAnimationAuthoring`** (HollowEditor, только редактор) — то, что Python не достаёт: оси, сэмплы и скорости BlendSpace, слот, сегменты и секции монтажа, сокет и слот скелета.
4. **Контент** (`create_character_assets.py`, новый; задаёт вид всех трёх персонажей):
   - **Скелеты:** превью — свой меш, слот `DefaultSlot`. У Wraith сокет `ToolMount` на `hand_r` там, где в `Idle_Combat` лежит его винтовка: X вдоль ствола, Z вверх. Хват инструментов и оружия — смещение `−GRIP` от сокета (`create_art_kit.py`).
   - **`BS_<Char>_Locomotion`.**
     - У врагов 1D по `Speed`: Пилигрим `Idle_Combat` / `Jog_Fwd` на 180 и 330, Хищник `Idle` / `Walk_Fwd` / `Sprint_Fwd` на 220 и 480.
     - У ExoFrame 2D: `Direction × Speed`, джог в четыре стороны на 400 и 640.
     - Скорость воспроизведения каждого сэмпла — скорость игры, делённая на «естественную» скорость движения, которую скрипт измеряет по стоящей ноге (0,35–1,8). Если стопа медленнее 100 см/с (Sevarog парит), скорость воспроизведения 1,0.
   - **`ABP_*`** (родитель `UHollowCharacterAnimInstance`). Граф собран headless через движковый `UBlueprintGraphEditor` (модуль BlueprintEditorLibrary): BlendSpace → [у тела ExoFrame aim offset по `AimPitch`] → `Slot DefaultSlot` → выход.
   - **`AM_*`.**
     - Melee и Breach: секции `Windup` и `Impact`. `Impact` — момент остановки бьющей кисти или оружия, скрипт меряет его по костям: Пилигрим 0,60 и 0,63 с, Хищник 0,34 с. Монтаж разгоняется так, чтобы удар пришёлся на конец замаха.
     - Stunned: `Start`, затем `Loop` по кругу.
     - Хищник: VentEnter — `Speedburst`, VentExit — отрезок 1,0–2,1 с `LevelStart`, подъём из-под пола.
   - **BP.**
     - Меш, AnimBP, смещение `−HalfHeight`, поворот −90°. Масштаб под капсулу: голова × 1,1 = высота капсулы. Wraith ×1,013, Riktor ×1,154, Sevarog ×0,795.
     - Без переопределённых материалов и без набора накладок.
     - У врагов `OnlyTickPoseWhenRendered`.
     - Капсулы, StateTree и ИИ не меняются: `create_pilgrim`/`create_stalker` больше не трогают меш.
   - **`DA_Enemy_*.Montages`** заполнены.
5. **Удалено** (`create_art_kit.py` `RETIRED`):
   - `DA_Parts_*` и 14 мешей накладок;
   - шесть `MI_Kit_*`, которые носили только они.

   `M_Kit_Visor`/`MI_Kit_Visor` и функции брони в `Build/Blender/hollow_kit.py` остались: их удаление поменяло бы хеш спецификации каждого меша набора и пересобрало бы их все (CH-6). Manny остаётся только у Шептунов.
6. **Цена на Low** (CH-10).
   - На уровне LOD1 Paragon стоил на Mac Low на 1,4 мс GPU больше Manny, в основном в тенях локальных ламп. Поэтому Low (уровень 1 движка) переопределяет `[ViewDistanceQuality@1] r.SkeletalMeshLODBias=2` в `Config/DefaultScalability.ini`. Это значение движкового уровня 0; дальность прорисовки не меняется, High (уровень 2) остаётся с 0.
   - Смоук `playthrough-mac.sh chars` идёт на фиксированном секторе (`--seed`, по умолчанию 1624260639, сектор базового замера), иначе его A/B сравнивает разные сцены.

## Проверка
- **Тесты:** `run-tests-mac.sh --strict` — 288/288. Новые тесты:
  - `Hollow.Core.Animation.Locomotion`;
  - `Hollow.Game.Characters.Look`: меши, капсулы, ноги и голова в капсуле, `DefaultSlot`, камера, сокет;
  - `Hollow.Editor.AnimationAuthoring`.

  Обновлены:
  - `Hollow.Game.Enemy.Montages`: отгружаемые монтажи в `DefaultSlot`, без root motion, удар укладывается в замах, Stunned зациклен, VentEnter прячет тело после монтажа; пустые слоты — как раньше;
  - `Hollow.Game.Fab.Content`: 450 МБ, потолки текстур по папкам, без root motion, скрытые слоты пустые и отключены;
  - `Hollow.Game.Art.*`, `Hollow.Game.ExoFrame.Composition`, `Hollow.Game.Stalker.AmbushInHottestRoom`.
- **Сборки:** редактор unity и `--no-unity`, игра Development и Shipping — без предупреждений проекта.
- **Скрипты** идемпотентны: 0 изменённых файлов из 1034. Локализация: 409 записей, 0 непереведённых.
- **FPS** (Mac Low, 1280×720, пакет Development; подробности в KnownIssues, «Прогоны CHAR-01»):
  - `chars` на секторе базы: Paragon **78,4** (GPU P95 12,81 мс), база Manny — 74,6 (14,05), Manny в той же сессии — 76,4 (13,38). **+2,6 %**;
  - пакет, один процесс: Paragon 82,5 против Manny 81,2;
  - до правки Low (решение 6) было −19 %.
- **Снимки** — `Saved/CHAR01_Shots`.

## Последствия
- Персонажи весят в `Content/Fab` около 219 МБ (Wraith 67, Riktor 72, Sevarog 80), весь `Content/Fab` — 374 МБ. Потолок теста `Hollow.Game.Fab.Content` поднят с 300 до 450 МБ.
- На складе остались неудаляемые копии: `/Game/ParagonWraith` (74) и, как раньше, `/Game/AdvancedCompanionDroneSystem` (77). Удалить вручную (CH-1, DR-1).
- У Wraith нет анимации приседа: напарники видят присевшего ExoFrame стоящим (CH-2).
- Рук от первого лица у ExoFrame нет; инструмент владельца на камере, как до CHAR-01 (AK-4, CH-3).
- Подробности и открытые вопросы — KnownIssues, раздел CHAR-01.
