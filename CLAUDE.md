# CLAUDE.md — Engineering Standards & Rules for Protocol: Hollow (UE5)

> **Context:** This repository is for **Protocol: Hollow**, a 1–4 player co-op / solo techno-occult sci-fi extraction survival-horror game built in **Unreal Engine 5 (C++)** for **PC (Steam)**. Always read `CLAUDE_ARCHITECT_BRIEF.md` before making major architectural decisions.
>
> **Engine:** locked to **UE 5.8.x** (ADR-0008). **Accepted architecture:** `Docs/Architecture/` (Blocks 1–5) + `Docs/ADR/` — read the relevant block/ADR before touching a domain.

---

## 1. Core Architectural Philosophy

- **C++ First, Blueprints for Assembly:**
  - All core gameplay, replication, physics state machines, Gameplay Ability System (GAS), procedural generation (PCG), AI logic, inventory, and extraction economy MUST live in **C++**.
  - **Blueprints** are strictly for visual composition, asset binding, UMG/CommonUI layout, Niagara VFX, animations, and audio cues. Never put business logic, economy math, or multiplayer authority checks in Blueprints.
- **Data-Driven Design:**
  - Never hardcode item stats, weapon parameters, enemy attributes, biome rooms, or crafting recipes in C++. Use `UPrimaryDataAsset`, `UDataTable`, and `FGameplayTag` containers.
- **Solo & Co-op Parity:**
  - Single-player runs on the exact same architecture as 4-player co-op (Standalone / Local Listen Server). Never write separate gameplay code paths for solo vs. multiplayer.
- **No Unnecessary `Tick()`:**
  - Disable `PrimaryActorTick.bCanEverTick = false` by default on all new Actors and Components. Use timers, delegates, Gameplay Abilities, or event-driven updates instead of per-frame ticking.

---

## 2. UE5 C++ Coding Standards

- Follow the **Epic Games C++ Coding Standard** strictly:
  - Prefixes: `A` (Actor), `U` (UObject), `F` (Struct), `E` (Enum `enum class ... : uint8`), `I` (Interface), `b` (booleans).
  - Use `TObjectPtr<T>` for UObject member pointers in headers; use `TSoftObjectPtr<T>` / `TSoftClassPtr<T>` for heavy assets (meshes, textures, VFX, rooms) to prevent synchronous loading hitches.
  - Prefer forward declarations in `.h` files; keep `#include` minimal in headers and include full definitions in `.cpp`.
  - Use `check()`, `ensure()`, and `ensureMsgf()` defensively; never crash the editor or server on recoverable null references.
  - Use dedicated log categories, one per module (`DECLARE_LOG_CATEGORY_EXTERN(LogHollowCore, Log, All);`, `LogHollowPhysics`, `LogHollowGAS`, `LogHollowAI`, `LogHollowNet`, `LogHollowPCG`, `LogHollowEconomy`, `LogHollowGame`, `LogHollowUI`, `LogHollowEditor`) — never leave `UE_LOG(LogTemp, ...)` in committed code.
  - Never rename or remove Blueprint-exposed `UPROPERTY` / `UFUNCTION` signatures without explicit approval or a Core Redirector plan.
- **Strictly cross-platform C++ (ADR-0009):** day-to-day development is on **macOS (Apple Silicon, Xcode/Apple Clang)**; Win64 builds, packaging and live co-op/FPS tests run on Windows (**MSVC**). Every change must compile warning-free on **both** toolchains:
  - No platform headers (`<windows.h>`, `AllowWindowsPlatformTypes.h`, Cocoa) or `#if PLATFORM_*` branches in gameplay modules; platform-specific code needs its own ADR and lives behind an interface in `HollowNetwork`/`HollowCore`.
  - No compiler-specific constructs: use `FORCEINLINE`, `PRAGMA_DISABLE_DEPRECATION_WARNINGS`, `UE_ARRAY_COUNT`, etc. instead of `__forceinline`, `#pragma warning`, `_countof`, MSVC extensions or GNU attributes.
  - Fixed-size types only (`int32`, `int64`, `uint8`, `SIZE_T`) — never `long` (32-bit on Win64, 64-bit on macOS). Never assume `sizeof(TCHAR)`; always use `TEXT()`, `FString`, `FCString`.
  - `#include` paths must match the file's exact case; ASCII file names without spaces; **ASCII-only content** in `.h/.cpp/.cs/.ini/.ps1/.sh` (Windows PowerShell 5.1 reads BOM-less files as ANSI); LF line endings (see `.gitattributes`).
  - Keep code compiling with unity builds **disabled** (`Build/Scripts/build-editor-mac.sh --no-unity`) so missing includes are not hidden by unity batching.
  - **No Windows-only (or Mac-only) plugins.** Every enabled plugin must support both `Mac` and `Win64`; a future upscaler (DLSS etc.) requires a separate ADR, a `PlatformAllowList`, and zero C++ dependencies.

---

## 3. Multiplayer & Replication Rules (Steam Listen Server)

- **Strict Server Authority:**
  - Damage, loot pickups, inventory transfers, in-raid crafting, extraction results, door weld integrity, and Chaos breach triggers MUST be validated and executed on the Server (`HasAuthority()`).
  - Clients request actions via `Server` RPCs (with `WithValidation` where appropriate) and receive cosmetic feedback via `NetMulticast` or `OnRep_*` callbacks.
- **Profile Authority (ADR-0005):** each player's meta-profile (credits, stash, blueprints, class levels, ship upgrades) is **owned by that player's machine** (`UHollowProfileSubsystem`, SaveGame + Steam Cloud). The **host is authoritative for the raid**: it validates the submitted loadout (`Server_SubmitLoadout`) against the item registry and sends back `FHollowRaidResult`; the client applies the resulting `FHollowProfileDelta` locally. Hub crafting/trading are local, recipe-validated profile transactions. A raid-escrow marker is written before every raid (host disconnect → "Signal Lost", own crash/quit → death rules).
- **Bandwidth Hygiene:**
  - Replicate state, not high-frequency transforms, whenever possible.
  - Use `DOREPLIFETIME_CONDITION` (`COND_OwnerOnly`, `COND_SkipOwner`, `COND_SimulatedOnly`, `COND_InitialOnly`) to avoid broadcasting private UI or owner-only data to all clients.
  - Use `FFastArraySerializer` for replicated inventories, crafting queues, and dynamic loot lists.
- **Physics & Destruction Sync:**
  - **Door Welding / Cutting:** Replicate compact state (`FHollowDoorNetState`: state enum, quantized `WeldQ`/`StructureQ`, `TransitionSerial`, server timestamp), NOT individual sparks or debris transforms. Cutting happens only along authored seams (`CutMask`).
  - **World objects have no ASC (Q4):** doors, bulkheads and barricades keep integrity as **replicated component state**, changed **only on the server** through `IHollowToolTarget` / `IHollowStructural` with data-driven rates. GAS effects stay on the **instigator** (tool fuel, heat, Resonance).
  - **Chaos Destruction (ADR-0002):** the server replicates breach state (zone mask, impact direction, server time); every machine fractures the same **authored** breach zones, so the resulting opening is identical. Debris is cosmetic, client-side, never collides with pawns/AI and never affects navigation. Built-in Geometry Collection replication stays **off**; late-join/out-of-relevancy clients snap straight to the broken state.
  - **Heavy Core Carrying (ADR-0002, Q3):** the server owns the **carry state** (`FHollowCarryNetState`: mode + carriers). While carried, the core's transform is a pure function of the carriers and is **not replicated**. Speed penalties are predicted via **CMC saved moves** (the server applies `max(claimed, server)` with a 250 ms grace). During **two-player carry only**, both carriers use client-authoritative movement (`bServerAcceptClientAuthoritativePosition` + `bIgnoreClientMovementErrorChecksAndCorrection`), with server sanity checks (speed, separation, penetration) every 0.25 s; any violation breaks the carry and restores server authority.

---

## 4. Gameplay Ability System (GAS) Rules

- **Centralized Tag Hierarchy:** All GameplayTags must live under `Hollow.*` (e.g., `Hollow.Ability.Tool.PlasmaCutter`, `Hollow.Ability.Tool.Welder`, `Hollow.State.CarryingHeavy`, `Hollow.State.Resonance.High`, `Hollow.Damage.Kinetic`, `Hollow.Damage.Occult`).
  - **Single exception (Q5):** GameplayCue tags must live under `GameplayCue.Hollow.*` (e.g., `GameplayCue.Hollow.Tool.Beam`), because GAS only discovers cues under the `GameplayCue.` root.
  - Declare native tags with `HOLLOW_DEFINE_GAMEPLAY_TAG` (see `HollowGameplayTags.cpp`): its `static_assert` rejects any tag outside these two namespaces at compile time.
- **Attributes & Effects:**
  - Define core attributes (`Health`, `Stamina`, `Oxygen`, `Resonance`, `CarryWeight`, `Thermal`) in modular `UAttributeSet` classes with `GAMEPLAYATTRIBUTE_REPNOTIFY` in every `OnRep_*`.
  - Apply all stat modifications, buffs, debuffs, tool fuel consumption, and Resonance scaling through `UGameplayEffect` — never mutate attribute values directly from external actors.
- **Prediction:** Use GAS client prediction for responsive first-person tools (cutter beam start, melee swing, lantern toggle) while keeping authoritative hit/resource validation on the server.

---

## 5. Chaos Physics, PCG & Performance Budgets

- **Target Hardware Baseline:** Maintain **60+ FPS at 1080p/1440p on RTX 3060** (Lumen + TSR/DLSS) with 4 players active.
- **Chaos Fragment Budgeting:**
  - Enforce strict sleep thresholds, max active debris counts, and timed fade/despawn on fractured Chaos chunks so destruction never degrades frame rate.
- **Runtime NavMesh Integrity:**
  - Any breached bulkhead, collapsed cover, or welded airlock MUST update a bounded NavModifier / Dynamic NavLink (`UNavigationSystemV1::UpdateActorInNavOctree`) immediately so AI (especially Iron Pilgrim and Echo-Stalker) re-routes accurately without rebuilding the entire level's NavMesh.
- **PCG Level Generation:**
  - The sector generator must be **deterministic from an `int32` Seed**, enforced by tests. It uses integer grid math only and a room pool sorted by `PrimaryAssetId`; `FMath::Rand`, `TMap`/`TSet` iteration order and Asset Registry order are forbidden in generation.
  - **Manifest replication (ADR-0003, Q1):** only the **server** runs the generator. It replicates `Seed` + a compact `FHollowSectorManifest` (< 2 KB) + `LayoutHash` via `UHollowSectorStateComponent` on `GameState` (push model, **not** `COND_InitialOnly`). Clients assemble identical geometry **from the manifest** and confirm `LayoutHash` in `Server_ReportSectorReady`. `Seed` also derives per-room dressing seeds (`Hash(Seed, RoomIdx)`).
  - Room shells are locally streamed Level Instances (static geometry/light/audio). All gameplay actors (doors, bulkheads, loot, cores, extraction) are **spawned by the server** as replicated actors at room sockets/connectors. PCG Framework is for in-room cosmetic dressing only, mainly baked in the editor.
  - Validate graph connectivity and loop count (minimum loop factor so players are never trapped in unwinnable dead ends by the Stalker).

---

## 6. AI & Proximity Audio Integration

- Use **UE5 StateTree** + **EQS (Environment Query System)** for enemy decision-making.
- Route both physical impact noises (cutting, explosions, dropped heavy cores) and **Proximity Voice Chat loudness** into `UAISense_Hearing` (`UAISense_Hearing::ReportNoiseEvent`) with clear loudness/range tags.
- Keep Echo-Stalker perception and adaptation data-driven (heat map of player activity + Resonance multiplier) so tuning difficulty requires no C++ rebuild.

---

## 7. Automated Testing & Definition of Done

Every new core system must include lightweight **UE5 Automation / Functional Tests** (`IMPLEMENT_SIMPLE_AUTOMATION_TEST` or `IMPLEMENT_COMPLEX_AUTOMATION_TEST`) covering:
1. **PCG Sector Generator:** Deterministic seed parity, zero overlapping room bounds, guaranteed path from Spawn to Extraction.
2. **Physics / Door State Machine:** Weld integrity clamping, breach state transitions, and server-to-client replication consistency.
3. **GAS & Resonance Math:** Attribute clamping, Resonance scaling curves, and tool fuel consumption.
4. **Extraction Economy:** Inventory persistence on extraction vs. insured-only retention on death.

**Definition of Done for any task:**
- Compiles cleanly with zero new compiler warnings.
- No `LogTemp`, dead code, or hardcoded magic numbers.
- Replication verified in PIE with **2+ players (Listen Server + Client)** for any networked feature.
- Relevant automation tests added or updated and passing.

---

## 8. Working Protocol for Claude Code

- Before implementing a new module or refactoring core classes, output a concise **Architecture Plan** (files to create/modify, class signatures, replication flow) and wait for confirmation if the change spans multiple modules.
- Keep classes focused and modular (avoid monolithic `Character` or `GameMode` classes — extract logic into `UActorComponent` and `UWorldSubsystem` / `UGameInstanceSubsystem`).
- Whenever a new architectural decision or plugin dependency is finalized, update `CLAUDE_ARCHITECT_BRIEF.md` or add an ADR note under `Docs/ADR/`.
