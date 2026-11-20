# ADR-0013: UI-база на CommonUI — layout на AHUD, C++-модели визора, WBP собираются скриптом

- **Статус:** Accepted · 2026-11-19 (S2-06, подтверждено владельцем проекта, пункты 1–7 плана)
- **Связано:** CLAUDE.md §1 (C++ для логики, Blueprint для сборки), ADR-0001 (слои модулей), ADR-0010 (бинарный контент скриптами) · [Блок 2](../Architecture/02_Folder_Structure_and_Class_Hierarchy.md) · [Docs/QA/UIInputCheck.md](../QA/UIInputCheck.md)

## Контекст
Нужна основа интерфейса для соло и кооперативного режима: визор (здоровье, топливо и нагрев инструмента, подсказка взаимодействия, шкала сварки или резки) и меню. Она должна работать с клавиатурой и геймпадом, не использовать Tick и не держать логику в Blueprint. У каждого игрока свой визор, данные приходят из GAS и реплицированного состояния мира.

## Решение
1. **Владелец UI — `AHollowHUD`.** Он существует только на машине своего локального игрока и пересоздаётся вместе с HUD на каждой карте. HUD создаёт `UHollowVisorContext`, корневой `UHollowPrimaryLayout` (`W_PrimaryLayout`), кладёт визор на слой Game и подключает `IMC_UI`. Режимы игры получают HUD как данные (`HUDClass` в `BP_GameMode_Hub/Raid`). Если меню хаба должны будут переживать смену карты, layout переедет в подсистему LocalPlayer, как в Lyra.
2. **Слои** `Hollow.UI.Layer.{Game, GameMenu, Menu, Modal}` — это `UCommonActivatableWidgetStack` с тегами. Режим ввода каждый экран объявляет данными (`UHollowActivatableWidget::InputMode`: Game, Menu или GameAndMenu). Переходы стеков пока мгновенные (`TransitionDuration = 0` в WBP). Плавное появление включается числом в WBP, без кода.
3. **Модели визора — обычный C++ без плагина MVVM:**
   - `UHollowVisorContext` следит за pawn'ом и его ASC. На клиенте ASC приходит с PlayerState, после possess: `AHollowCharacterBase::OnAbilitySystemChanged`, `OnPossessedPawnChanged`, `UGameInstance::OnPawnControllerChanged`.
   - `FHollowAttributeWatch` следит за атрибутами через делегаты GAS.
   - `UHollowPromptModel` следит за `OnFocusChanged` и глифом клавиши.
   - `UHollowToolTargetModel`: тег `Hollow.State.Tool.Active`, трассировка `HollowToolAim::TraceLocal` (общая с лучом), чтение `IHollowTargetStatusSource`.

   Виджеты подписываются на нативные делегаты, у всех стоит `DisableNativeTick`.
4. **Статус цели — интерфейс Core** `IHollowTargetStatusSource`. Его реализует цель (дверь: шов из реплицированного `WeldQ`, отметка `SealThreshold`), визор знает только интерфейс. Шкала видна, пока свой инструмент работает и попадает в цель, и ещё `TargetLingerSeconds` после. В эту задержку модель продолжает читать ту же цель, поэтому финальное значение, пришедшее после отпускания, тоже появляется на шкале.
5. **Ввод — CommonUI на Enhanced Input** (`bEnableEnhancedInputSupport`, `BP_CommonInputData`: confirm и back из `IMC_UI`):
   - системное меню открывается UI-действием визора `IA_SystemMenu` (Esc / Start). Привязке нужен `InputMode = Game`: по умолчанию UI-привязки работают только в режиме Menu;
   - Back (Esc / B) закрывает меню;
   - у UI-действий `bConsumeInput = false`, поэтому они не отнимают у игры клавиши, назначенные в обоих контекстах.
6. **Глифы клавиш пока текстовые** (`DA_InputGlyphs`, раскладка Xbox): клавиша действия для текущего устройства находится через `QueryKeysMappedToAction`. Иконки и `CommonActionWidget` придут вместе с артом.
7. **WBP — только раскладка**, и её собирает `create_ui_assets.py` через `UHollowWidgetAuthoring` (HollowEditor). Дерево виджетов WBP из Python напрямую недоступно. Виджеты C++ требуют дочерние элементы по имени (`BindWidget`), поэтому WBP без них не компилируется.
8. **Паузы нет** ни в соло, ни в кооперативе (паритет, CLAUDE.md §1). Системное меню только переключает ввод.

## Последствия
- (+) UI не тикает, данные идут событиями; клиент видит только реплицированное, и двухпроцессный тест это подтверждает.
- (+) Новая цель с прогрессом (переборка, шов, ядро) — это реализация интерфейса Core, визор не меняется.
- (−) Маршрутизация CommonUI (активный корень, режим ввода, фокус, Back) упорядочивает корни по слою отрисовки. Под `-nullrhi` она не работает, и переходы стеков без тика Slate не завершаются. Поэтому сквозная проверка ввода идёт в игре с `-RenderOffScreen` и подачей клавиш через Slate (`Hollow.UI.PressKey`). Автотесты в редакторе проверяют модели и WBP без маршрутизации.
- (−) Окно `-RenderOffScreen` тоже принимает ввод мыши ОС. Сценарии наводятся на цель заново перед действием.
- (?) Живой геймпад на Mac и Windows проверяет владелец по `Docs/QA/UIInputCheck.md`.
