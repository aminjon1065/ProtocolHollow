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
Hollow.Event.{Door.Welded, Door.Breached, Bulkhead.Breached, Core.PickedUp, Raid.Extracted}
Hollow.Input.{Move, Look, PrimaryUse, SecondaryUse, Interact, Sprint, Crouch, Gadget}
Hollow.Room.Type.{Spawn, Extraction, CoreVault, Hangar, Corridor, Junction, Storage}
Hollow.Socket.{PlayerSpawn, Loot, CoreSpawn, EnemySpawn, VentNode, Extraction}
Hollow.Item.Type.{Tool, ToolModule, Consumable, Artifact, AetherCore}
Hollow.Data.{Damage, ResonanceGain, FuelCost, ToolStat.*}       ← ключи SetByCaller
GameplayCue.Hollow.{Tool.Beam, Door.WeldSparks, Bulkhead.Breach, Stalker.Presence}
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
        ├─ UHollowRaidFlowComponent            фазы Assembling → Deploying → InRaid → Extraction → Debrief
        ├─ UHollowSectorAuthorityComponent     [Game]  (S2-02) Seed из URL → Generate → Manifest → GameState; после сборки на сервере:
        │                                              спавн актёров по сокетам/соединениям, NavMesh, сбор Server_ReportSectorReady
        │                                              (несовпадение хеша и таймаут → кик), затем bSectorReady и pawn'ы
        └─ UHollowRaidOutcomeComponent         [Game]  смерть/эвакуация → FHollowRaidResult каждому игроку

AGameStateBase
└─ AHollowGameState                            [Game]  тоже тонкий; состояние живёт в UGameStateComponent'ах:
   ├─ UHollowSectorStateComponent    [PCG]  (S2-02) Rep (push): FHollowSectorManifest {Biome, SectorPlugin, Seed, BuildId, LayoutHash,
   │                                         Rooms, Connections}, bool bSectorReady
   ├─ UHollowSectorLoaderComponent   [Game] (S2-02) на каждой машине: манифест → ActivateSector → Assemble → Server_ReportSectorReady
   ├─ UHollowRaidPhaseComponent      [Game] Rep: EHollowRaidPhase Phase, float PhaseEndServerTime
   ├─ UHollowSquadResonanceComponent [GAS]  Rep: uint8 SquadResonanceQ, EHollowResonanceTier Tier
   └─ UHollowExtractionStateComponent[Game] Rep: активная точка эвакуации, время отлёта (server time)

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
      ├─ AHollowWhispererCharacter · AHollowIronPilgrimCharacter · AHollowEchoStalkerCharacter

AAIController
└─ AHollowAIController                         [AI]    UStateTreeAIComponent, UAIPerceptionComponent, TeamId, NavQueryFilter из DA
```

Почему GameMode и GameState тонкие: по CLAUDE.md §8 монолиты запрещены. Каждая подсистема рейда — отдельный компонент, у которого есть своя спецификация и свой тест.

---

## 2.3 Персонаж ExoFrame — композиция компонентов

| Компонент | Модуль | Роль | Сеть |
|---|---|---|---|
| `UHollowCharacterMovementComponent` | Physics | ходьба, low-G и zero-G, штрафы переноски через **кастомные saved moves** | предсказание CMC |
| `UHollowInteractionComponent` | Game | локальный sweep по каналу `Interaction` по таймеру (`UHollowInteractionSettings`, 15 Гц, без Tick) из глаз pawn; `Server_Interact` с мягкой серверной проверкой (линия видимости, дальность + допуск, темп, блокирующие теги ASC). Взаимодействие — компонент, не GAS-способность (S1-06) | RPC |
| `UHollowEquipmentComponent` | Game | инструмент в руках, сокеты FP и TP, точка луча | публичный rep сборки |
| `UHollowCarrierComponent` | Physics | сторона носильщика: хват, тетер в коопе, IK-цели | state через `UHollowCarryableComponent` |
| `UHollowExoFrameComponent` | Game | применяет `UHollowExoFrameDefinition`: AbilitySet, init-GE, таланты | на сервере |
| `UHollowVisorComponent` | UI | диегетика шлема поверх моделей визора (S2-06 начал с экранного `W_Visor`, [ADR-0013](../ADR/ADR-0013-ui-foundation-commonui.md)); только косметика и только локально | — |
| `UVOIPTalker` (движковый) | Net | 3D-голос с аттенюацией и эффектами шлема и рации | движковый VOIP |

Камера от первого лица — штатный First Person Rendering 5.8 ([ADR-0011](../ADR/ADR-0011-first-person-rendering-and-placeholders.md)): FP-руки (`FirstPerson`, `OnlyOwnerSee`, копия позы TP) и TP-тело (`WorldSpaceRepresentation`: владелец видит только тень), камера на сокете `head` FP-меша. До S1-05 `AHollowExoFrameCharacter` наследуется от `ACharacter`, после — от `AHollowCharacterBase`. **Гейплейная** привязка к позе руками не делается никогда, руки — чистая косметика (важно для переноски, §3.5).

### Интерфейс (S2-06, [ADR-0013](../ADR/ADR-0013-ui-foundation-commonui.md))
```
AHollowHUD [UI, только локальный PC]
 ├─ UHollowVisorContext            pawn + ASC игрока (OnAbilitySystemChanged / OnPossessedPawnChanged / OnPawnControllerChanged)
 │   ├─ UHollowPromptModel         OnFocusChanged + глиф IA_Interact для текущего устройства (DA_InputGlyphs)
 │   └─ UHollowToolTargetModel     Hollow.State.Tool.Active -> HollowToolAim::TraceLocal -> IHollowTargetStatusSource
 └─ UHollowPrimaryLayout (W_PrimaryLayout)   стеки Hollow.UI.Layer.{Game, GameMenu, Menu, Modal}
     ├─ Game: UHollowVisorWidget (W_Visor)   UHollowStatBarWidget x3 (FHollowAttributeWatch), UHollowInteractionPromptWidget,
     │                                         UHollowTargetStatusWidget, прицел; UI-действие IA_SystemMenu (Esc / Start)
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
   ├─ UHollowCarrySet       CarryWeight, CarryCapacity                                               rep: COND_OwnerOnly   (S3-02, вместе с переноской)
   └─ UHollowToolSet        ToolFuel, MaxToolFuel, ToolHeat, MaxToolHeat                             rep: COND_OwnerOnly
   (наборы разбиты в том числе по условию репликации: приватные данные не уходят чужим клиентам)
   Общая база UHollowAttributeSetBase: пары «текущее/максимум» зажаты в [0, Max], прочие атрибуты ≥ 0, снижение максимума подтягивает текущее
   (через ASC). Наборы — default subobjects владельца: у игрока AHollowPlayerState, у врага — сам персонаж.

UGameplayAbility
└─ UHollowGameplayAbility             ActivationPolicy {OnInputTriggered, WhileInputActive, OnSpawn}, cost и cooldown из данных
   ├─ UHollowGA_ContinuousTool        [Game] (S2-03) база луча: серверный трейс 10 Гц, прогресс по серверному времени (§3.2)
   │  └─ GA_Tool_Welder · GA_Tool_PlasmaCutter   BP-наследники без своего кода: вся разница в UHollowToolDefinition (DA),
   │                                             а смысл работы (сварка / резка) решает мишень — двери S2-04, швы S2-07
   ├─ UHollowGA_CarryHeavy · UHollowGA_PulseMine   (взаимодействие — UHollowInteractionComponent, не способность)
   └─ (враги, [AI]) UHollowGA_PilgrimBreach · UHollowGA_PilgrimMelee · UHollowGA_StalkerAmbush

UGameplayEffectExecutionCalculation
├─ UHollowDamageExecution             Kinetic, Occult, Structural; броня; иммунитеты по тегам (Echo-Stalker)
└─ UHollowResonanceGainExecution      прирост × ResonanceGainMultiplier × кривая сложности контракта

UPrimaryDataAsset
├─ UHollowAbilitySet                  способности (+InputTag) и эффекты к выдаче; только сервер; handles для снятия. Наборы атрибутов НЕ выдаёт (S1-05)
├─ UHollowExoFrameDefinition          класс: AbilitySet, init-GE, ветки талантов, фирменный инструмент
└─ UHollowToolDefinition              темп сварки и резки, частота сэмплов, дальность, GE топлива и нагрева, кьюшки, шумы
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
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;            // пороги тиров → Hollow.State.Resonance.*

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
                                    заглушки Sealed — локальные AStaticMeshActor (меш биома, размер проёма × толщина стены)
                                    → OnAssembled(BuildId, LocalHash). Геймплейных актёров не трогает — их спавнит SectorAuthority.
Временные актёры: AHollowSectorMarker (реплицируется, dormant) по правилам биома SocketActors/ConnectionActors, PlayerSpawn → APlayerStart.
Door → BP_Door (S2-04), Bulkhead → BP_Bulkhead_Plate | BP_Bulkhead_Twin (S3-01: FHollowConnectionActorRule::Variants,
вариант = DeriveSeed(Seed, ConnectionIndex) % N, одинаков для seed).
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

```
AHollowEnemyCharacter (ASC Minimal, UHollowEnemyDefinition) → Whisperer · IronPilgrim · EchoStalker
AHollowAIController  → UStateTreeAIComponent (ST_ из DA) · UAIPerceptionComponent (Hearing + Sight) · фильтр навигации из DA

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
| `UHollowNoiseSubsystem` | World | сервер | Core |
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

- **Seed** выбирает только хост, один раз, в хабе. Это единственное место с недетерминированным источником. Для «ежедневных контрактов» seed берётся как хеш даты.
- **Seamless travel:** нужна `L_Transition`, а в PIE включается `net.AllowPIESeamlessTravel 1`.
- **Сборка «Хост и клиенты»:** в PIE прогоняем каждый день, в Steam — раз в неделю (Блок 4).
