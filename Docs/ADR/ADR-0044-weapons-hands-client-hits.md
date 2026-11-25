# ADR-0044: Оружие, режим рук и клиентские попадания

- **Статус:** Accepted · 2026-11-24 (GP-01a; план GP-01 и решения 1–8 приняты владельцем «ок», раскладка — по стандарту шутеров; реализация принята «ок»)
- **Связано:**
  - [ADR-0017](ADR-0017-iron-pilgrim-brain-breach-damage.md) — урон через `UHollowDamageExecution`, броня;
  - [ADR-0018](ADR-0018-noise-to-ai.md) — шум для ИИ;
  - [ADR-0030](ADR-0030-raid-inventory-loadout.md) — рейд-инвентарь, предметы дают способности;
  - [ADR-0031](ADR-0031-hub-economy-crafting-shop-insurance.md) — магазин и верстак;
  - [ADR-0035](ADR-0035-aether-lantern.md) — Expose фонаря;
  - [ADR-0037](ADR-0037-occult-relics-equip-slots.md) — слоты снаряжения;
  - [ADR-0043](ADR-0043-art-kit-fab-staging-cc0.md) — арт-кит, Niagara-авторинг.

## Контекст
- После живого плейтеста игроку не хватает боя: он только убегает и варит двери.
- Инструменты уже занимают ЛКМ/ПКМ/R (резак или фонарь, сварка, дозаправка). Оружие не должно их ломать.
- Gamepad Y был перезарядкой. Стандарт шутеров: Y — смена оружия, X — перезарядка (нажатие) и взаимодействие (удержание), R3 — удар.
- Серверный трейс с допуском, как у инструментов, хорош для луча. По быстрым целям при 150 мс без отмотки он даёт промахи «в упор».

## Решение
1. **Режим рук** — `UHollowHandsComponent` на `AHollowExoFrameCharacter`: `EHollowHandsMode {Tools, Weapon}`.
   - `Mode`: push model, `COND_SkipOwner`. Владелец предсказывает, как фонарь шлема.
   - `Weapon`: всем; на теле виден ствол.
   - Оружие берётся из рейд-инвентаря: сервер ищет у ASC способность `UHollowGA_WeaponFire` и обновляет её при каждом изменении инвентаря.
   - На каждой машине компонент держит loose-тег `Hollow.State.Hands.Weapon`. Инструменты и дозаправка им блокируются, способности оружия его требуют.
2. **Смена — `UHollowGA_SwapHands`** (LocalPredicted). Костюм выдаёт её трижды: на `Hollow.Input.Hands.{Tools, Weapon, Cycle}`; что просят — читается из input-тега спеки.
   - Почему через ASC: смена и следующий выстрел идут одним надёжным каналом ASC, порядок гарантирован.
   - Кулдаун `GE_Hands_Swap` (0,25 с) — это и есть «руки заняты» (`Hollow.State.Hands.Swapping`).
   - При смене отменяются `Hollow.Ability.Tool` и `Hollow.Ability.Weapon`.
   - Переноска ядра и смерть блокируют `Ability.Weapon`, `Ability.Melee`, `Ability.Hands.Swap`.
3. **Данные** — `UHollowWeaponDefinition` (HollowGAS, primary asset `HollowWeapon`):
   - стрельба: темп, дробины, разброс (от бедра и в прицеле), дальность, спад урона;
   - урон: пробитие брони и бонус под Expose, импульс;
   - магазин: размер, время перезарядки, `AmmoItem` (`FPrimaryAssetId`, GAS не зависит от Economy);
   - серверная проверка: допуски угла, глаз и bounds;
   - шум, меш в руке, вид от первого лица, Niagara-эффекты.
   - Магазин — атрибуты `UHollowWeaponSet` (`Magazine`, `MaxMagazine`, `COND_OwnerOnly`), как топливо инструментов. Патроны — предметы рейд-инвентаря (`Hollow.Item.Type.Ammo`), как ячейки топлива.
4. **Выстрел — `UHollowGA_WeaponFire`** (LocalPredicted):
   - **Владелец:**
     - Commit: `GE_Weapon_Cost` −1 патрон и кулдаун предсказаны.
     - Трассы по каналу `HollowWeapon` из камеры вдоль control rotation. Визуальная отдача камеры прицел не трогает.
     - Дробины детерминированы номером выстрела.
     - Попадания уходят компактной target data `FHollowWeaponShotData`, около 150 байт на дробовик.
   - **Сервер:**
     - `HollowWeaponMath::CheckHit` на каждую дробину проверяет: глаза, конус плюс допуск, дальность, bounds цели с отмоткой ≤ 250 мс, LOS по WorldStatic.
     - Сумма по цели — один GE урона с SetByCaller `Hollow.Data.{Damage, ArmorPenetration, ExposedPenetration}` и один импульс через `IHollowImpulseReceiver`.
     - Шум `Hollow.Noise.Weapon.*`, `UAISense_Damage`.
     - Огонь по своим выключен (`UHollowDamageSettings::bFriendlyFire`): дробь останавливается о союзника.
   - **Хост:** его собственные попадания применяются как есть.
   - Кулдаун GE = 0,85 интервала (дрожание пакетов). Точный темп держит владелец.
   - **Остальные:** `GameplayCue.Hollow.Weapon.Fire` (статический) — звук, вспышка, трассеры, искры из origin/aim/serial. Точки попаданий не реплицируются.
5. **Урон:** `HollowDamageMath::Mitigate(..., Penetration)`: `Armor × (1 − Pen)`; под `Hollow.State.Exposed` к пробитию добавляется бонус. Эффекты без SetByCaller бьют как прежде.
6. **Отброс:** `IHollowImpulseReceiver` (HollowCore). Враг делает `LaunchCharacter × KnockbackScale`; импульс не меньше `StaggerImpulse` даёт `GE_Status_Stagger` (короткий `UHollowStunnedEffect`: мозг стоит, атаки заблокированы). Новой логики ИИ нет.
7. **Перезарядка — `UHollowGA_WeaponReload`.** Committed: патроны входят сразу (предсказанный `GE_Weapon_Load`, сервер забирает предметы), затем оружие занято `ReloadSeconds` (`State.Weapon.Reloading`: ни выстрела, ни смены).
   - **Сухой щелчок** — `UHollowGA_WeaponDryFire` (LocalOnly, магазин пуст): щелчок и автоперезарядка.
   - **Прицел** — `UHollowGA_WeaponAim` на ПКМ/LT в режиме оружия: разброс × `AimSpreadMultiplier`.
8. **Удар — `UHollowGA_MeleeBash`** (Q / R3, оба режима): серверный сфер-свип из вида игрока, урон через броню, импульс, шум, кулдаун 0,7 с.
9. **Раскладка — стандарт шутеров:**

   | Действие | Клавиатура | Геймпад |
   |---|---|---|
   | Перезарядка / дозаправка | R | X (нажатие) |
   | Взаимодействие | E | X (удержание 0,3 с, подсказка «Hold X») |
   | Инструменты / оружие | 1 / 2 | — |
   | Переключение | колесо | Y |
   | Удар | Q | R3 |

   - `FHollowInputAction::bPressOnTrigger`: перезарядку жмёт её триггер.
   - Interact биндится на `Triggered` (E — Pressed, X — Hold).
10. **Экономика (данные):**

    | | Цена | Слот лоадаута |
    |---|---|---|
    | Гвоздомёт | 150 | в стартовом наборе обоих классов + 40 гвоздей |
    | Дробовик | 650 | рецепт: 12 лома + 4 осколка + 250 кр |
    | Гвоздь | 1 кр | рецепт: 20 штук = 2 лома + 4 кр |
    | Патрон дроби | 4 кр | рецепт: 8 штук = 3 лома + 1 осколок + 6 кр |

    - Патроны — исключение из правила ADR-0031 «расходники не продаются».
    - Слот `Hollow.Item.Slot.Weapon` с лимитом 1, `MaxLoadoutSlots` 6 → 8.
11. **Локализация:** C++-тексты — `LOCTEXT`. Новые тексты данных (имена оружия, патронов, рецептов, строки подсказки, «Wheel») лежат в string table `/Game/Hollow/Localization/ST_Gameplay`. Записи ставит `UHollowLocalizationAuthoring` (HollowEditor). Цель Game (en + ru) и перевод старых текстов — L10N-01.

## Проверка
- **Тесты:**
  - `Hollow.GAS.Weapons.{Math, Magazine}`, `Hollow.GAS.Damage.Penetration`;
  - `Hollow.Economy.Weapons.Rules`;
  - `Hollow.Game.Weapons.{Data, Hands, Fire, Shotgun, ServerCheck, Melee}`, `Hollow.Game.Art.WeaponInHand`;
  - обновлены `Hollow.GAS.ReplicationConditions`, `Hollow.Economy.{Crafting.Data, LoadoutSync.Data}`, `Hollow.UI.Visor.ControlsHint`, `Hollow.Game.Art.Kit`.
- **Смоук** `playthrough-mac.sh weapons` (кооп, 150 мс):
  - клиент стреляет из гвоздомёта, перезаряжается и бьёт;
  - хост бьёт из дробовика (стаггер);
  - хост показывает принятые и отклонённые попадания клиента.

## Последствия
- (+) Новое оружие — это данные: определение, BP-наследники fire и reload, набор, предмет, рецепт.
- (+) Попадания честные для стреляющего при 150 мс, а сервер всё ещё отсекает невозможное.
- (−) Хитбоксы — капсулы; хедшотов и скелетной отмотки нет.
- (−) Перезарядку нельзя отменить: патроны уже в магазине.
- (−) Трассер — быстрая частица, гибнет при столкновении с миром. Точного луча до цели нет.
- (?) Второй слот оружия (апгрейд «Кобура») потребует магазин на ствол.
