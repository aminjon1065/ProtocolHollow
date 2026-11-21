# Блок 2 — Структура папок и иерархия ключевых C++ классов

> Статус: **Proposed v0.1** · 2026-11-19 · Модули описаны в [Блоке 1](01_Module_and_Plugin_Map.md)

---

## 2.1 Репозиторий

```
ProtocolHollow/
├─ ProtocolHollow.uproject
├─ Config/
│  ├─ DefaultEngine.ini · DefaultGame.ini · DefaultInput.ini
│  ├─ DefaultScalability.ini · DefaultDeviceProfiles.ini     (пресеты 3060 / Steam Deck)
│  └─ Tags/HollowContentTags.ini                            (только не-нативные теги контента)
├─ Source/
│  ├─ ProtocolHollow.Target.cs · ProtocolHollowEditor.Target.cs
│  ├─ HollowCore/     Public/{Tags, Interfaces, Data, Net, Noise, Math}  Private/{…, Tests}   (в каждом модуле: Public/<Module>Log.h)
│  ├─ HollowGAS/      Public/{AbilitySystem, Attributes, Abilities, Effects, Characters, Data}
│  ├─ HollowEconomy/  Public/{Items, Inventory, Crafting, Insurance, Extraction, Profile}
│  ├─ HollowNetwork/  Public/{Session, Voice, Entitlements, Diagnostics}
│  ├─ HollowPhysics/  Public/{Doors, Cutting, Breach, Carry, Movement, Gravity, Props, Destruction, Navigation}
│  ├─ HollowPCG/      Public/{Generator (чистый C++), Data, Assembly, Dressing}
│  ├─ HollowAI/       Public/{Characters, Controllers, StateTree, EQS, Perception, Director, Heatmap, Vents}
│  ├─ HollowGame/     Public/{Framework, Character, Tools, Interaction, Raid, Hub}
│  ├─ HollowUI/       Public/{Foundation, Visor, Hub, ViewModels}
│  └─ HollowEditor/   Private/{Validation, Visualizers, SectorPreview}
├─ Plugins/
│  ├─ GameFeatures/
│  │  ├─ HollowSector_Docks/        Сектор I — базовая игра, тот же пайплайн, что у DLC
│  │  ├─ HollowSector_Cathedral/    Сектор II (позже)
│  │  └─ HollowSector_CryoArchive/  Сектор III, база для DLC (позже)
│  └─ Lyra/  CommonLoadingScreen · ModularGameplayActors   (CommonUser не берём, ADR-0006)
├─ Content/Hollow/
│  ├─ Core/        Input/ · Data/Global/ · Materials/Master/ · Audio/{Mix, Buses}/
│  ├─ Characters/  ExoFrame/ · Enemies/{Whisperer, IronPilgrim, EchoStalker}/
│  ├─ Tools/       PlasmaCutter/ · Welder/ · Gadgets/
│  ├─ GAS/         Abilities/ · Effects/ · Cues/
│  ├─ Items/ · Recipes/ · ExoFrames/
│  ├─ World/Kit/   общий модульный кит: стены, проёмы, двери, переборки (Nanite)
│  ├─ Maps/        L_Boot · L_Hub_Skit · L_Raid_Persistent · L_Transition · Dev/ · Tests/
│  ├─ UI/
│  └─ Developers/  личные песочницы (исключены из кука)
├─ Docs/          Architecture/ · ADR/ · Sprints/ · QA/
└─ Build/         скрипты: прогон тестов, BuildGraph и упаковка (позже)
```

### Префиксы ассетов

| Префикс | Тип | Префикс | Тип |
|---|---|---|---|
| `BP_` | Blueprint-актёр | `DA_` | Primary Data Asset |
| `DT_` | DataTable | `CT_` / `CF_` | CurveTable / CurveFloat |
| `GA_` | Gameplay Ability | `GE_` | Gameplay Effect |
| `GCN_` | GameplayCue Notify | `GC_` | Geometry Collection |
| `ST_` | StateTree | `EQS_` | Environment Query |
| `LV_` | уровень-комната | `PLA_` | Packed Level Actor |
| `M_` / `MI_` | материал / инстанс | `MPC_` | Material Parameter Collection |
| `NS_` | Niagara System | `MSS_` | MetaSound Source |
| `W_` | UMG/CommonUI-виджет | `IA_` / `IMC_` | Input Action / Mapping Context |
| `NAV_` | NavArea / NavQueryFilter (BP-тюнинг) | `CCC_` | Chaos Cache Collection |

### Primary Asset Types (`UHollowAssetManager`)
`HollowItem`, `HollowToolModule`, `HollowRecipe`, `HollowExoFrame`, `HollowRoom`, `HollowBiome`, `HollowContract`, `HollowEnemy`, `HollowDoor`, `HollowBreachable`, `HollowCarryable`.
Все `*Definition` — **лёгкие DataAssets**: тяжёлые меши, VFX и уровни внутри них хранятся только как `TSoftObjectPtr`/`TSoftClassPtr`. Поэтому определения предметов можно целиком предзагрузить при старте и **реплицировать указателями** (NetGUID), а не строками (см. §2.6).

### Дерево GameplayTags (нативные, `HollowCore/Public/Tags/HollowGameplayTags.h`)

```
Hollow.Ability.Tool.{PlasmaCutter, Welder}      Hollow.Ability.Gadget.PulseMine
Hollow.Ability.{Interact, Carry.Heavy, Sprint}  Hollow.Ability.Enemy.{Pilgrim.Breach, Pilgrim.Melee, Stalker.Ambush}
Hollow.State.CarryingHeavy.{Solo, Coop}         Hollow.State.Resonance.{Low, Medium, High, Critical}
Hollow.State.{Stunned, Downed, Dead, Tool.Overheated, Invulnerable.Kinetic}
Hollow.Damage.{Kinetic, Occult, Thermal, Structural}
Hollow.Noise.{Voice, Footstep, Tool.Cutter, Tool.Welder, Impact.Heavy, Structural.Breach, Core.Drop}
Hollow.Event.{Door.Welded, Door.Breached, Bulkhead.Breached, Core.PickedUp, Raid.Extracted}   Hollow.Event.Enemy.{Breach, Melee} (S3-04: триггеры атак)
Hollow.AI.Event.Reconsider                      (S3-04: факты мозга изменились — StateTree выбирает заново)
Hollow.Input.{Move, Look, PrimaryUse, SecondaryUse, Interact, Sprint, Crouch, Gadget}
Hollow.Room.Type.{Spawn, Extraction, CoreVault, Hangar, Corridor, Junction, Storage}
Hollow.Socket.{PlayerSpawn, Loot, CoreSpawn, EnemySpawn, VentNode, Extraction}
Hollow.Item.Type.{Tool, ToolModule, Consumable, Artifact, AetherCore}
Hollow.Data.{Damage, ResonanceGain, FuelCost, ToolStat.*}       ← ключи SetByCaller
GameplayCue.Hollow.{Tool.Beam, Door.WeldSparks, Bulkhead.Breach, Stalker.Presence, Enemy.Windup}
```
🔶 **Исключение из правила `Hollow.*`:** GAS ищет GameplayCue только под корнем `GameplayCue.`, поэтому теги кьюшек живут в `GameplayCue.Hollow.*`. Это единственное исключение, его нужно внести в CLAUDE.md §4.

```cpp
// HollowCore/Public/Tags/HollowGameplayTags.h
#pragma once
#include "NativeGameplayTags.h"

namespace HollowTags
{
	HOLLOWCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tool_PlasmaCutter); // Hollow.Ability.Tool.PlasmaCutter
	HOLLOWCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Tool_Welder);
	HOLLOWCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CarryingHeavy_Solo);
	HOLLOWCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Noise_Tool_Welder);
	// …
}
```

---

## 2.2 Game Framework

```
UGameInstance
└─ UHollowGameInstance                         [Game]  тонкий: маршрутизирует NetworkFailure/TravelFailure → UHollowSessionSubsystem

AGameModeBase
└─ AHollowGameModeBase                         [Game]  спавн, выдача ExoFrame, общие правила
   ├─ AHollowHubGameMode                       [Game]  «Скит»: лобби, контракт, верстак, готовность
   └─ AHollowRaidGameMode                      [Game]  без логики внутри; вся логика в server-only компонентах:
        ├─ UHollowRaidFlowComponent            фазы Assembling → Deploying → InRaid → Extraction → Debrief; S3-03: опрос зоны,
        │                                      исходы игроков, итоги, возврат в хаб (ADR-0016; Outcome-компонент слит сюда)
        ├─ UHollowSectorAuthorityComponent     [Game]  (S2-02) Seed из URL → Generate → Manifest → GameState; после сборки на сервере:
        │                                              спавн актёров по сокетам/соединениям, NavMesh, сбор Server_ReportSectorReady
        │                                              (несовпадение хеша и таймаут → кик), затем bSectorReady и pawn'ы
        └─ UHollowRaidOutcomeComponent         [Game]  смерть/эвакуация → FHollowRaidResult каждому игроку

AGameStateBase
└─ AHollowGameState                            [Game]  тоже тонкий; состояние живёт в UGameStateComponent'ах:
   ├─ UHollowSectorStateComponent    [PCG]  (S2-02) Rep (push): FHollowSectorManifest {Biome, SectorPlugin, Seed, BuildId, LayoutHash,
   │                                         Rooms, Connections}, bool bSectorReady
   ├─ UHollowSectorLoaderComponent   [Game] (S2-02) на каждой машине: манифест → ActivateSector → Assemble → Server_ReportSectorReady
   ├─ UHollowRaidStateComponent      [Game] (S3-03) Rep (push): фаза, RaidId, конец фазы, живые / в зоне / ядра в зоне
   │                                         (одним компонентом вместо RaidPhase + ExtractionState; escrow профиля на OnRep)
   ├─ UHollowSquadResonanceComponent [GAS]  (S4-02) Rep (push): FHollowSquadResonanceNetState {uint8 Level, Tier} — максимум
   │                                         живых, 2 Гц, только при изменении; на каждой машине его читает
   │                                         UHollowResonancePresentationSubsystem [Game] (MPC_Resonance, CB_Resonance)
   (зона эвакуации — AHollowExtractionZone на сокете Extraction: объём без коллизии, явный overlap-запрос сервера)

APlayerState
└─ AHollowPlayerState : IAbilitySystemInterface   [Game]
   ├─ UHollowAbilitySystemComponent (Mixed) + AttributeSets   ← ASC на PlayerState переживает смерть и респаун Pawn
   ├─ UHollowInventoryComponent     [Economy] FFastArray, COND_OwnerOnly
   ├─ UHollowLoadoutComponent       [Economy] публичная часть: что видно другим (сборка инструмента в руках)
   └─ ExoFrameId · InsuredSlots (OwnerOnly) · FHollowRaidStats (для дебрифа)
   ⚠ SetNetUpdateFrequency(30…60): дефолт у PlayerState очень низкий, атрибуты будут «лагать»

APlayerController
└─ AHollowPlayerController                     [Game]  Client_ReceiveRaidResult(), Server_ReportSectorReady(BuildId, LayoutHash) (S2-02); UI — у AHollowHUD [UI] (S2-06)
   └─ UHollowVoiceLoudnessComponent            [Net]   громкость микрофона → сервер (§3.8)

ACharacter
└─ AHollowCharacterBase : IAbilitySystemInterface, IGameplayTagAssetInterface   [GAS]
   ├─ AHollowExoFrameCharacter                 [Game]  игрок (композиция в §2.3)
   └─ AHollowEnemyCharacter                    [AI]    свой ASC (Minimal), данные из UHollowEnemyDefinition
      ├─ AHollowWhispererCharacter · AHollowEchoStalkerCharacter   (S3-04: Пилигрим — BP_IronPilgrim, нативного наследника нет)

AAIController
└─ AHollowAIController                         [AI]    UStateTreeAIComponent, UAIPerceptionComponent, NavQueryFilter из DA; S3-04: факты
                                                        мозга и активности (ADR-0017), UHollowPathFollowingComponent (препятствия на связях)
```

Почему GameMode и GameState тонкие: по CLAUDE.md §8 монолиты запрещены. Каждая подсистема рейда — отдельный компонент, у которого есть своя спецификация и свой тест.

---

## 2.3 Персонаж ExoFrame — композиция компонентов

| Компонент | Модуль | Роль | Сеть |
|---|---|---|---|
| `UHollowCharacterMovementComponent` | Physics | ходьба, low-G и zero-G, штрафы переноски через **кастомные saved moves** | предсказание CMC |
| `UHollowInteractionComponent` | Game | локальный sweep по каналу `Interaction` по таймеру (`UHollowInteractionSettings`, 15 Гц, без Tick) из глаз pawn; `Server_Interact` с мягкой серверной проверкой (линия видимости, дальность + допуск, темп, блокирующие теги ASC). Взаимодействие — компонент, не GAS-способность (S1-06) | RPC |
| `UHollowEquipmentComponent` | Game | инструмент в руках, сокеты FP и TP, точка луча | публичный rep сборки |
| `UHollowCarrierComponent` | Physics | сторона носильщика: хват, тетер в коопе, IK-цели. *S3-02:* предсказание подъёма (заявка в CMC), `Server_Drop`, `GE_Carry_Solo` на ASC, серверный режим для CMC; цели крепления визуала — FP-камера и TP-меш ([ADR-0015](../ADR/ADR-0015-solo-carry-claim.md)). *S3-06:* предсказанное подключение (заявка `Coop`), партнёр и тетер в CMC (`HollowCarryTether`), `GE_Carry_Coop`, выход из коопа вместо сброса; санитарная проверка и поза носилок — на `UHollowCarryableComponent`, client-auth — в CMC ([ADR-0028](../ADR/ADR-0028-coop-carry-tether-client-authority.md)) | state через `UHollowCarryableComponent` |
| `UHollowExoFrameComponent` | Game | применяет `UHollowExoFrameDefinition`: AbilitySet, init-GE, таланты | на сервере |
| `UHollowVisorComponent` | UI | диегетика шлема поверх моделей визора (S2-06 начал с экранного `W_Visor`, [ADR-0013](../ADR/ADR-0013-ui-foundation-commonui.md)); только косметика и только локально. *S4-08:* стекло шлема — элемент самого `W_Visor` (`W_VisorFrame`), компонент на персонаже не понадобился ([ADR-0026](../ADR/ADR-0026-diegetic-visor-resonance-tags-helmet-lamp.md)) | — |
| `UVOIPTalker` (движковый) | Net | 3D-голос с аттенюацией и эффектами шлема и рации | движковый VOIP |

Камера от первого лица — штатный First Person Rendering 5.8 ([ADR-0011](../ADR/ADR-0011-first-person-rendering-and-placeholders.md)): FP-руки (`FirstPerson`, `OnlyOwnerSee`, копия позы TP) и TP-тело (`WorldSpaceRepresentation`: владелец видит только тень), камера на сокете `head` FP-меша. До S1-05 `AHollowExoFrameCharacter` наследуется от `ACharacter`, после — от `AHollowCharacterBase`. **Гейплейная** привязка к позе руками не делается никогда, руки — чистая косметика (важно для переноски, §3.5).

### Интерфейс (S2-06, [ADR-0013](../ADR/ADR-0013-ui-foundation-commonui.md))
```
AHollowHUD [UI, только локальный PC]
 ├─ UHollowVisorContext            pawn + ASC игрока (OnAbilitySystemChanged / OnPossessedPawnChanged / OnPawnControllerChanged)
 │   ├─ UHollowPromptModel         OnFocusChanged + глиф IA_Interact для текущего устройства (DA_InputGlyphs)
 │   └─ UHollowToolTargetModel     Hollow.State.Tool.Active -> HollowToolAim::TraceLocal -> IHollowTargetStatusSource
 └─ UHollowPrimaryLayout (W_PrimaryLayout)   стеки Hollow.UI.Layer.{Game, GameMenu, Menu, Modal}
     ├─ Game: UHollowVisorWidget (W_Visor)   Overlay: стекло UHollowVisorFrameWidget (S4-08) | Canvas: UHollowStatBarWidget x4
     │                                         (FHollowAttributeWatch; здоровье, стамина, топливо, нагрев), UHollowInteractionPromptWidget,
     │                                         UHollowTargetStatusWidget, прицел, строка рейда; S4-08: UHollowResonanceReadoutWidget,
     │                                         UHollowVoiceIndicatorWidget, UHollowLampIndicatorWidget (FHollowResonanceTierWatch,
     │                                         стиль DA_VisorStyle); UI-действие IA_SystemMenu (Esc / Start)
     └─ Menu: UHollowSystemMenuWidget (W_SystemMenu)   Resume / Quit (UHollowButton), Back, фокус на Resume
```
- Все виджеты C++ с `DisableNativeTick`; WBP — только раскладка, их собирает `create_ui_assets.py` через `UHollowWidgetAuthoring` (HollowEditor).
- Настройки: `UHollowUISettings` (классы WBP, `IMC_UI`, действия, глифы, `TargetProbeRateHz` = 10, `TargetLingerSeconds` = 0,5); CommonUI на Enhanced Input (`BP_CommonInputData`).
- Цель с прогрессом реализует `IHollowTargetStatusSource` (Core): сейчас это `AHollowDoor` (шов, отметка `SealThreshold`).

---

## 2.4 Слой GAS

```
UAbilitySystemComponent
└─ UHollowAbilitySystemComponent      привязка ввода к способностям по Hollow.Input.*, политики активации, tag relationships

UAttributeSet
└─ UHollowAttributeSetBase            ATTRIBUTE_ACCESSORS, clamp-хелперы
   ├─ UHollowHealthSet      Health, MaxHealth, Armor; meta: IncomingDamage, IncomingHealing          rep: всем (тиммейтам на визор)
   ├─ UHollowSurvivalSet    Stamina, MaxStamina, Oxygen, MaxOxygen, Thermal, MaxThermal              rep: COND_OwnerOnly   (ThermalResistance — с крио-биомом)
   ├─ UHollowResonanceSet   Resonance, MaxResonance, ResonanceGainMultiplier                         rep: всем
   ├─ UHollowCarrySet       CarryWeight, CarryCapacity                                               rep: COND_OwnerOnly   (отложен в S3-02: читать некому до веса инвентаря; скорость переноски — в saved moves, ADR-0015)
   └─ UHollowToolSet        ToolFuel, MaxToolFuel, ToolHeat, MaxToolHeat                             rep: COND_OwnerOnly
   (наборы разбиты в том числе по условию репликации: приватные данные не уходят чужим клиентам)
   Общая база UHollowAttributeSetBase: пары «текущее/максимум» зажаты в [0, Max], прочие атрибуты ≥ 0, снижение максимума подтягивает текущее
   (через ASC). Наборы — default subobjects владельца: у игрока AHollowPlayerState, у врага — сам персонаж.

UGameplayAbility
└─ UHollowGameplayAbility             ActivationPolicy {OnInputTriggered, WhileInputActive, OnSpawn}, cost и cooldown из данных
   ├─ UHollowGA_ContinuousTool        [Game] (S2-03) база луча: серверный трейс 10 Гц, прогресс по серверному времени (§3.2)
   │  └─ GA_Tool_Welder · GA_Tool_PlasmaCutter   BP-наследники без своего кода: вся разница в UHollowToolDefinition (DA),
   │                                             а смысл работы (сварка / резка) решает мишень — двери S2-04, швы S2-07
   ├─ UHollowGA_PulseMine   (взаимодействие — UHollowInteractionComponent, не способность; переноска тоже: S3-02 обходится без
   │                         UHollowGA_CarryHeavy, подъём — через Interact, эффекты — UHollowCarrySoloEffect / UHollowStaminaRecoveryEffect;
   │                         S3-06: общая база UHollowCarryEffect, UHollowCarryCoopEffect)
   └─ (враги, [AI]) UHollowGA_EnemyAttack (S3-04: ServerOnly, событие + цель, замах → удар → восстановление, DA_Attack_*)
                    ├─ UHollowGA_PilgrimBreach · UHollowGA_PilgrimMelee   (S3-04)
                    └─ UHollowGA_StalkerAmbush

UGameplayEffectExecutionCalculation
├─ UHollowDamageExecution             S3-04: SetByCaller Hollow.Data.Damage, тип из UHollowDamageEffect, броня цели K/(K+Armor),
│                                      иммунитет по тегу; правила типов в UHollowDamageSettings (Structural — только мировым объектам)
└─ (S4-02) UHollowResonanceGainCalculation — MMC, а не Execution: коэффициент × ResonanceGainMultiplier цели (не snapshot);
                                       кривая сложности контракта добавится множителем позже

UPrimaryDataAsset
├─ UHollowAbilitySet                  способности (+InputTag) и эффекты к выдаче; только сервер; handles для снятия. Наборы атрибутов НЕ выдаёт (S1-05)
├─ UHollowExoFrameDefinition          класс: AbilitySet, init-GE, ветки талантов, фирменный инструмент
└─ UHollowToolDefinition              темп сварки и резки, частота сэмплов, дальность, GE топлива и нагрева, кьюшки, тег шума (S3-05: числа шума — в DA_NoiseConfig)
```

```cpp
// HollowGAS/Public/Attributes/HollowResonanceSet.h  (шаблон для всех наборов)
UCLASS()
class HOLLOWGAS_API UHollowResonanceSet : public UHollowAttributeSetBase
{
	GENERATED_BODY()
public:
	ATTRIBUTE_ACCESSORS(UHollowResonanceSet, Resonance);
	ATTRIBUTE_ACCESSORS(UHollowResonanceSet, MaxResonance);
	ATTRIBUTE_ACCESSORS(UHollowResonanceSet, ResonanceGainMultiplier);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;         // clamp [0, Max]
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;            // clamp; тиры — у отряда (S4-02, S4-08: теги Hollow.State.Resonance.* из тира отряда)

	UFUNCTION() void OnRep_Resonance(const FGameplayAttributeData& OldValue);   // GAMEPLAYATTRIBUTE_REPNOTIFY внутри
	UFUNCTION() void OnRep_MaxResonance(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ResonanceGainMultiplier(const FGameplayAttributeData& OldValue);

private:
	UPROPERTY(ReplicatedUsing = OnRep_Resonance)               FGameplayAttributeData Resonance;
	UPROPERTY(ReplicatedUsing = OnRep_MaxResonance)            FGameplayAttributeData MaxResonance;
	UPROPERTY(ReplicatedUsing = OnRep_ResonanceGainMultiplier) FGameplayAttributeData ResonanceGainMultiplier;
};
```

**Где *не* используем атрибуты GAS** 🔶: целостность дверей, переборок и баррикад. Это **состояние мировых объектов**, а не статы актёров. Давать ASC каждой двери (а их 30–60 на сектор) дорого по памяти и трафику. Двери принимают работу через `IHollowToolTarget` и `IHollowStructural`, темпы и множители берутся из данных, расчёт идёт только на сервере. *S3-01:* `IHollowStructural` (Core) реализует `AHollowBreachableBulkhead`; прочность зон хранит `UHollowStructuralIntegrityComponent` (Physics), а не атрибуты. GE при этом остаются у **инициатора**: топливо, нагрев, Резонанс. Правило CLAUDE.md §4 «никаких прямых мутаций атрибутов» выполняется полностью, потому что у дверей атрибутов нет.

---

## 2.5 Предметы, инвентарь, крафт, экономика

```
UPrimaryDataAsset
├─ UHollowItemDefinition       DisplayName, Icon (soft), Fragments: TArray<TObjectPtr<UHollowItemFragment>> (Instanced)
│    фрагменты: _Stack · _Weight · _Equippable(→UHollowToolDefinition) · _Resonance · _Value · _Insurable · _HeavyCarry(→TSoftClassPtr<AActor>)
├─ UHollowToolModuleDefinition слот {Frame, Core, Relic, Attachment} + модификаторы статов (данные, не код)
├─ UHollowRecipeDefinition     входы, выходы, требуемый чертёж, уровень верстака
└─ UHollowEconomyConfig        цены (CurveTable), стоимость страховки, налог корпорации, кривые наград по угрозе

Чистая логика (без UWorld, покрыта тестами):
├─ HollowToolMath::ComputeStats(const FHollowToolAssembly&, const UHollowToolDefinition&) → FHollowToolStats
├─ FHollowCraftingRules::Validate/Execute(FHollowProfileTransaction&, const UHollowRecipeDefinition&)
└─ FHollowExtractionResolver::Resolve(const FHollowRaidContext&, EHollowRaidOutcome) → FHollowProfileDelta

Реализовано в S3-03 (ADR-0016): UHollowItemDefinition (тип HollowItem: имя, тип, цена, страхуемость, стек; без фрагментов),
FHollowItemStack / FHollowLoadoutSlot / FHollowItemCatalog, FHollowExtractionResolver (Extracted: всё + продажа ядер;
Dead/MIA/Abandoned: только застрахованное; SignalLost: лоадаут цел), HollowProfileRules (Apply, BeginRaid, ApplyResult,
ResolveEscrow), UHollowProfileSaveGame (версия), UHollowProfileSubsystem (escrow, отчёт для дебрифа), UHollowEconomySettings.

Реализовано в S5-01 (ADR-0030): UHollowInventoryComponent на AHollowPlayerState (FHollowInventoryList, COND_OwnerOnly, push;
записи Item/Count/Taken), HollowLoadoutRules (мягкая проверка лоадаута на хосте), UHollowItemDefinition::AbilitySet
(способности предмета на время рейда), UHollowGameplayAbility::ItemCostPerUse + IHollowItemCostSource (GAS),
FHollowRaidResult::Spent. Transfer и DropToWorld — с добычей и крафтом.

Компоненты и подсистемы:
├─ UHollowInventoryComponent   [PlayerState] серверный API: TryAdd, TryRemove, Transfer, DropToWorld
├─ UHollowLoadoutComponent     [PlayerState] публичная сборка в руках (видят все)
└─ UHollowProfileSubsystem     [GameInstanceSubsystem, ЛОКАЛЬНЫЙ] SaveGame с версией, Steam Cloud, escrow рейда 🔶 ADR-0005
```

```cpp
// HollowEconomy/Public/Inventory/HollowInventoryList.h
USTRUCT()
struct FHollowItemEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY() int32 InstanceId = INDEX_NONE;                          // уникален в рамках рейда
	UPROPERTY() TObjectPtr<const UHollowItemDefinition> Definition;     // NetGUID; все определения предзагружены
	UPROPERTY() int32 Count = 0;
	UPROPERTY() FHollowToolAssembly Assembly;                           // пусто для не-инструментов
	UPROPERTY() bool bInsured = false;

	void PostReplicatedAdd(const struct FHollowInventoryList& InArraySerializer);
	void PostReplicatedChange(const struct FHollowInventoryList& InArraySerializer);
	void PreReplicatedRemove(const struct FHollowInventoryList& InArraySerializer);
};

USTRUCT()
struct FHollowInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY() TArray<FHollowItemEntry> Items;
	UPROPERTY(NotReplicated) TObjectPtr<UActorComponent> OwnerComponent = nullptr;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FHollowItemEntry, FHollowInventoryList>(Items, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FHollowInventoryList> : public TStructOpsTypeTraitsBase2<FHollowInventoryList>
{
	enum { WithNetDeltaSerializer = true };
};

// UHollowInventoryComponent::GetLifetimeReplicatedProps:
//   FDoRepLifetimeParams P; P.bIsPushBased = true; P.Condition = COND_OwnerOnly;
//   DOREPLIFETIME_WITH_PARAMS_FAST(UHollowInventoryComponent, InventoryList, P);
```

**Экземпляры предметов — структуры, а не UObject** (в отличие от Lyra). Это дешевле по трафику и памяти, а SaveGame и тесты становятся проще. Если когда-нибудь понадобится сложное поведение экземпляра, оно будет жить во фрагменте определения, а не в инстансе.

---

## 2.6 PCG / сборка сектора

```
UPrimaryDataAsset
├─ UHollowBiomeDefinition     (S2-01, тип HollowBiome, лежит в сектор-GFP) FHollowSectorGenRules GenRules; пул комнат = все
│                             HollowRoom этого плагина (ADR-0012), без явного списка; позже: таблицы врагов, пресеты света
│                             и звука, классы дверей и переборок (TSoftClassPtr)
├─ UHollowRoomDefinition      (S1-07) TSoftObjectPtr<UWorld> RoomLevel; FIntVector Footprint (клетки, в VS Z = 1);
│                             TArray<FHollowRoomConnector> {Cell, Side: N=+X|E=+Y|S=-X|W=-Y, AllowedKinds: Door|Bulkhead|Open}
│                               (один стандартный проём 300 x 350 на весь VS, Docs/QA/LevelMetrics.md)
│                             TArray<FHollowRoomSocket>    {SocketType: Hollow.Socket.*, Transform}
│                             FGameplayTagContainer RoomTags (ровно один Hollow.Room.Type.*); IsDataValid -> HollowRoomRules
│                             позже (S2-01): Weight; MinCount/MaxCount; DressingVariants
├─ UHollowLevelMetrics        (S1-07, UDeveloperSettings) клетка, этаж, стены, проём, вент-люк, крупнейший агент — единый источник чисел
└─ UHollowContractDefinition  биом, уровень угрозы, модификаторы аномалий, цели добычи

Чистый C++ (без UObject, тесты Блока 7.1 CLAUDE.md):
├─ HollowRoomRules            (S1-07) ValidateRoom / ValidateMetrics: коннектор на внешнем ребре, дубликаты, сокеты в footprint, тип комнаты
├─ FHollowRoomTemplate        (S2-01) POD-снимок RoomDefinition: Id, EHollowRoomRole {Spawn, Extraction, CoreVault, Path, Branch},
│                             Footprint, Connectors; роль по тегу даёт HollowSectorTemplates (генератор тегов не видит)
├─ FHollowSectorGenRules      (USTRUCT в DA) роли по тегам, CriticalPathMin/Max, CoreVaultCount, OptionalBranchesMin/Max,
│                             MaxBranchDepth, MinLoops, MaxBridgeRooms, MinBulkheadConnections, MaxRooms, MaxExtentCells,
│                             MaxAttempts, MaxPlacementsPerAttempt
├─ FHollowRandom / HollowRandom::DeriveSeed   PCG32 + SplitMix64: только целые числа, бит-в-бит на Mac arm64 и Win64
├─ HollowGridMath             повороты Yaw90, стороны, PivotOffset для Level Instance (S2-02)
├─ HollowSectorGraph          BFS, цикломатическое число, глубина тупиков, Analyze(Layout)
└─ FHollowSectorGenerator::Generate(const FHollowSectorGenInput&, FHollowSectorLayout& Out) → EHollowGenResult
                              Layout: Rooms {Template, Role, Cell, Size, Yaw90, bCriticalPath}, Connections {RoomA/B,
                              ConnectorA/B, CellA, SideA, Kind}, Sealed (незанятые коннекторы), TemplateIds, LayoutHash (CRC32)

USTRUCT (реплицируется):
└─ FHollowSectorManifest      Seed, ContentVersion, LayoutHash,
                              TArray<FHollowPlacedRoom>{TObjectPtr<const UHollowRoomDefinition> Room; FIntVector Cell; uint8 Yaw90; uint8 DressingVariant}
                              TArray<FHollowPlacedConnection>{RoomA, ConnA, RoomB, ConnB, EHollowConnectionKind: Door|Bulkhead|Open|Vent}

UWorldSubsystem
└─ UHollowSectorAssemblySubsystem   (S2-02) Assemble(Manifest) на каждой машине: Reconstruct (раскладка из локальных DA, роли по биому)
                                    → LayoutHash; async-стрим комнат (ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr, имена
                                    "Sector<BuildId>_<Idx>", трансформ RoomTransform = (Cell + PivotOffset) × CellSize, yaw × 90);
                                    заглушки Sealed — локальные AStaticMeshActor (меш биома, размер проёма × толщина стены;
                                    Movable с S3-02: Static-компонент в начатом мире отказывался от меша, и проёмы были дырами)
                                    → OnAssembled(BuildId, LocalHash). Геймплейных актёров не трогает — их спавнит SectorAuthority.
Временные актёры: AHollowSectorMarker (реплицируется, dormant) по правилам биома SocketActors/ConnectionActors, PlayerSpawn → APlayerStart.
Door → BP_Door (S2-04), Bulkhead → BP_Bulkhead_Plate | BP_Bulkhead_Twin (S3-01: FHollowConnectionActorRule::Variants,
вариант = DeriveSeed(Seed, ConnectionIndex) % N, одинаков для seed). CoreSpawn → BP_AetherCore (S3-02, FHollowSocketActorRule::SpawnOffset
поднимает тело на полвысоты, чтобы оно лежало на полу сокета).
```

**Алгоритм генератора (v0 реализован в S2-01, только целочисленная сетка):**
0. Пул сортируется по `Id` внутри генератора; комнаты поворачиваются на 0/90/180/270°. Комнаты и связи — стеки, любой шаг поиска откатывается точно.
1. **Критический путь:** Spawn в (0,0) со случайным поворотом, затем `CriticalPathMin…Max` комнат Path и Extraction. Шаг роста: случайный свободный коннектор, перебор «шаблон × поворот × коннектор» (каждый шаблон получает равный шанс), проверка свободных клеток и `MaxExtentCells`; при тупике — откат.
2. **Ветки с CoreVault** (решение S2-01): по одной на ядро, от внутренней комнаты критического пути, глубина ≤ `MaxBranchDepth`. Ядро — крюк в сторону, путь к эвакуации через хранилище не проходит.
3. **Петли**, пока `E − V + 1 < MinLoops`: сначала соединяем уже смотрящие друг на друга свободные коннекторы; затем «мосты» — от свободного коннектора растёт до `MaxBridgeRooms` комнат Path, пока одна из них не встанет напротив **любого** свободного коннектора другой комнаты. Замыкающая комната находится аналитически до размещения, промежуточная ставится только если её выход ещё может дотянуться. Петля не может сократить маршрут Spawn → Extraction ниже `CriticalPathMin + 1` переходов. Петли дают второй путь от Сталкера.
4. **Опциональные ветки** (Branch, например Storage) — если осталось место (`MaxRooms`).
5. **Виды проёмов:** `MinBulkheadConnections` случайных соединений, где разрешён Bulkhead; остальные Door, иначе Open. Незанятые коннекторы уходят в `Sealed`: их глушит сборка (S2-02). *S3-01:* переборка — стена, пока её не пробьют. Поэтому кандидат, без которого граф перестал бы быть связным, пропускается, а приёмка требует `ComponentsWithoutBulkheads == 1` ([ADR-0014](../ADR/ADR-0014-breachable-bulkheads.md)).
6. **Приёмка** через `HollowSectorGraph::Analyze`: без пересечений, связно, ровно один Spawn и Extraction, число ядер, маршрут, петли, глубина тупиков, лимиты, квота переборок. Провал → повтор с `HollowRandom::DeriveSeed(Seed, Attempt)` (SplitMix64), до `MaxAttempts`.
7. **Запреты, которые ломают детерминизм:** `FMath::Rand`, `FRandomStream`, итерация `TMap`/`TSet` (только поиск), порядок Asset Registry, float в топологии.

**Замеры (S2-01, Docks, 1000 seed'ов):** при `MinLoops = 2` и мосте ≤ 3 комнат генерируются все seed'ы, в среднем 3,2 попытки и около 2 мс на сектор, 13–14 комнат. С мостом ≤ 2 без раскладки остаются 1,4 % seed'ов; `MinLoops = 3` на текущем ките удаётся лишь в половине случаев (нужны новые коридорные модули).

**Где работает PCG Framework:** только **внутри комнат**, для декора (кабели, трубы, мусор, биомеханика). Варианты декора по возможности запекаются в редакторе в `DressingVariants`. Рантайм-PCG допускается для косметики с `RoomSeed = Hash(Seed, RoomIdx)` без коллизий, влияющих на геймплей. Геймплейные объекты (укрытия, лут, двери) **всегда** спавнит сервер как реплицируемые актёры. Подробнее и про отклонение от CLAUDE.md §5 — в [ADR-0003](../ADR/ADR-0003-sector-manifest-replication.md).

---

## 2.7 ИИ

*Реализовано в S3-04* ([ADR-0017](../ADR/ADR-0017-iron-pilgrim-brain-breach-damage.md)):
- `AHollowEnemyCharacter`, `UHollowEnemyDefinition`, `AHollowAIController` (факты: цель, стимул, препятствие; активности Patrol / Chase / Investigate / Attack / Breach на таймерах и колбэках), `UHollowPathFollowingComponent`;
- узлы StateTree `FHollowSTTask_{Patrol, Chase, Investigate, Attack, Breach}` и `FHollowSTCond_{HasTarget, TargetInEngageRange, HasStimulus, HasObstacle}` без параметров и без Tick;
- `ST_IronPilgrim` строит `create_pilgrim_assets.py` через `UHollowStateTreeAuthoring` (HollowEditor);
- патруль — случайная достижимая точка Heavy-навмеша.

*Реализовано в S4-01* ([ADR-0019](../ADR/ADR-0019-echo-stalker-heat-leash-vents-ambush.md)):
- Core: `FHollowSectorTopology` + `UHollowSectorTopologySubsystem` (комнаты, порталы, проёмы вентиляции; сервер), `IHollowLightSource` / `FHollowLightCone`; Game: `HollowSectorTopologyBuilder` (после сборки сектора), лампа `HelmetLight` у ExoFrame;
- AI, серверные подсистемы мира: `UHollowActivityHeatmapSubsystem`, `UHollowVentNetworkSubsystem`, `UHollowThreatDirectorSubsystem` (Stalk → Hunt → Retreat) с данными `UHollowThreatConfig` (`DA_ThreatConfig`);
- `AHollowStalkerController` (Track / Ambush / Retreat, шахта на своём таймере), `UHollowStalkerDefinition`; `AHollowEnemyCharacter::SetInVent` (реплицируемый `bInVent`);
- EQS: `UHollowEnvQueryGenerator_FocusRoomPoints`, `UHollowEnvQueryTest_OutOfLightCone`, `UHollowEnvQueryContext_FocusRoomHotPortal`; `EQ_StalkerFocusRoom` строит `UHollowEnvQueryAuthoring` (Editor);
- узлы `FHollowSTTask_{Stunned, Track, Ambush, Retreat}`, `FHollowSTCond_{IsStunned, IsHunting, IsRetreating}`; удар `UHollowGA_EnemyMelee` → `UHollowGA_PilgrimMelee` · `UHollowGA_StalkerAmbush`;
- GAS: `UHollowStunnedEffect`, `UHollowPulseMineCooldownEffect`, `UHollowStalkerTraitsEffect`; Game: `UHollowGA_PulseMine`, `AHollowPulseMine`, `UHollowPulseMineDefinition`.

*Art-Prep* ([ADR-0027](../ADR/ADR-0027-art-prep-locks-tool-mount-montages-budget.md)):
- AI: `FHollowEnemyMontages` в `UHollowEnemyDefinition` (Melee, Breach, Stunned, VentEnter, VentExit), `UHollowEnemyAttackDefinition::Montage`, `UHollowEnemyMontageComponent` на `AHollowEnemyCharacter` — монтажи из cue замаха (атака в `SourceObject`), тега оглушения и состояния шахты.
- Game: `UHollowToolMountComponent` и меши `FirstPersonToolMesh` / `ToolMesh` у ExoFrame; `HollowToolAim::BeamStart`. GAS: `UHollowToolDefinition::{HeldMesh, HeldTransform, MuzzleSocket}`.
- PCG: `UHollowRoomDefinition::bArtLocked` (editor-only), `HollowLightBudget::CheckHeroRig`. Editor: `UHollowArtBudgetSettings`, `HollowArtBudget`, `UHollowArtBudgetValidator`.

*S4-08* ([ADR-0026](../ADR/ADR-0026-diegetic-visor-resonance-tags-helmet-lamp.md)): у лампы появился выключатель — `bHelmetLampOn` (push, `COND_SkipOwner`), `UHollowGA_HelmetLamp` + `UHollowHelmetLampCooldownEffect`; `GetLightCone` смотрит на выключатель, `UHollowEnvQueryTest_OutOfLightCone` не менялся.

```
AHollowEnemyCharacter (ASC Minimal, UHollowEnemyDefinition) → Whisperer · IronPilgrim · EchoStalker
AHollowAIController  → UStateTreeAIComponent (ST_ из DA) · UAIPerceptionComponent (Hearing + Sight) · фильтр навигации из DA
  └ AHollowStalkerController (S4-01): поводок директора, Track / Ambush / Retreat, EQ_StalkerFocusRoom

StateTree (USTRUCT, FStateTreeTaskCommonBase и т.п.):
  Tasks:      FHollowSTTask_MoveToRoom · _BreachObstacle · _InvestigateNoise · _TraverseVent · _Ambush · _Retreat · _PlayAbility
  Conditions: FHollowSTCond_HasStimulus · _SquadResonanceAbove · _PathBlockedByDoor · _IsStunned
  Global:     FHollowSTEval_DirectorBudget · _BestHeatRoom

EQS: UHollowEnvQueryGenerator_RoomSockets · UHollowEnvQueryTest_OutOfFlashlightCone · UHollowEnvQueryTest_HeatScore

UWorldSubsystem (server-only):
  UHollowThreatDirectorSubsystem    кривая напряжения, пейсинг спавнов, «поводок» агрессии Сталкера (UHollowDirectorConfig)
  UHollowActivityHeatmapSubsystem   тепло по комнатам и порталам графа сектора, затухание (данные из шума и перемещений)
  UHollowVentNetworkSubsystem       абстрактный граф вентиляции из манифеста (Connection = Vent), без навмеша
```

---

## 2.8 Подсистемы: сводка

| Подсистема | Тип | Где активна | Модуль |
|---|---|---|---|
| `UHollowSessionSubsystem` | GameInstance | везде | Network |
| `UHollowVoiceSubsystem` | GameInstance | везде (локальный микрофон) | Network |
| `UHollowEntitlementSubsystem` | GameInstance | везде | Network |
| `UHollowProfileSubsystem` | GameInstance | везде (**только свой профиль**) | Economy |
| `UHollowNoiseSubsystem` | World | сервер (S3-05: правила в `DA_NoiseConfig`, [ADR-0018](../ADR/ADR-0018-noise-to-ai.md)) | Core |
| `UHollowSectorAssemblySubsystem` | World | везде (спавн — только на сервере) | PCG |
| `UHollowDestructionBudgetSubsystem` | World | везде (косметический бюджет у каждой машины свой; S3-01: слоты live-проломов, тела, таймеры жизни осколков по `UHollowDestructionBudgetConfig`) | Physics |
| `UHollowNavUpdateSubsystem` | World | сервер (навмеш только на хосте) | Physics |
| `UHollowThreatDirectorSubsystem` | World | сервер | AI |
| `UHollowActivityHeatmapSubsystem` | World | сервер | AI |
| `UHollowVentNetworkSubsystem` | World | сервер (клиентам — только косметика звука) | AI |

---

## 2.9 Жизненный цикл: Хаб → Рейд → Дебриф

```mermaid
sequenceDiagram
  participant H as Хост (Listen Server)
  participant C as Клиент
  Note over H,C: Хаб «Скит» (listen-сессия; соло = приватная сессия на 1 игрока)
  C->>H: Server_SubmitLoadout(FHollowLoadoutSnapshot) — проверка по реестру предметов и лимитам стаков
  C->>C: ProfileSubsystem: escrow-маркер «рейд начат» и снимок лоадаута в SaveGame
  H->>C: ServerTravel L_Raid_Persistent?Contract=…&Seed=… (seamless, лоадаут переносится через CopyProperties)
  H->>H: SectorAuthority: Generate(Seed) → Manifest (+ретраи по производному seed)
  H-->>C: GameState: Seed + Manifest + LayoutHash (push, один раз)
  par сборка на каждой машине
    H->>H: стрим комнат, спавн геймплейных актёров, построение NavMesh
    C->>C: стрим комнат по манифесту, локальный декор по RoomSeed
  end
  C->>H: Server_ReportSectorReady(LayoutHash)
  H->>H: все готовы и NavMesh готов → Phase = Deploying → InRaid
  Note over H,C: рейд 15–30 минут
  H->>C: Client_ReceiveRaidResult(FHollowRaidResult)
  C->>C: Resolver → FHollowProfileDelta → Apply → SaveGame (+Steam Cloud), escrow снят
  H->>C: ServerTravel обратно в хаб
```

- *S3-03:* escrow пишется не до `ServerTravel`, а когда машина видит RaidId рейда (фаза после `Assembling`); `Server_SubmitLoadout`
  перенесён в задачу инвентаря. *S5-01 ([ADR-0030](../ADR/ADR-0030-raid-inventory-loadout.md)):* лоадаут сдаётся уже в рейде —
  хост на приходе игрока шлёт `Client_RequestLoadout(RaidId)`, машина игрока отвечает `Server_SubmitLoadout`, хост принимает
  один раз в `Assembling`/`Deploying` и заполняет инвентарь рейда; перенос через `CopyProperties` не нужен. Итог приходит в `Debrief` (10 с), неподтверждённый дебриф показывается снова в хабе. Уход хоста →
  `SignalLost` по таймауту соединения, найденный при запуске маркер → `Abandoned` ([ADR-0016](../ADR/ADR-0016-raid-flow-extraction-escrow.md)).
- **Seed** выбирает только хост, один раз, в хабе. Это единственное место с недетерминированным источником. Для «ежедневных контрактов» seed берётся как хеш даты.
- **Seamless travel:** нужна `L_Transition`, а в PIE включается `net.AllowPIESeamlessTravel 1`.
- **Сборка «Хост и клиенты»:** в PIE прогоняем каждый день, в Steam — раз в неделю (Блок 4).
