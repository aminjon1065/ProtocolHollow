# ADR-0001: Слоистая модульная архитектура C++

- **Статус:** Accepted · 2026-11-19 (Proposed → Accepted: подтверждено владельцем проекта, Q1–Q6)
- **Связано:** CLAUDE.md §1, §8 · [Блок 1](../Architecture/01_Module_and_Plugin_Map.md)

## Контекст
Соло-разработка на годы и много доменов: GAS, физика, PCG, ИИ, экономика, онлайн. Нужны быстрые инкрементальные сборки, изоляция доменов и ясное место для каждого класса, в том числе для будущих сессий Claude Code.

## Решение
- 10 project-модулей в `Source/`: `HollowCore` → `HollowGAS` → {`HollowEconomy`, `HollowPhysics`, `HollowAI`}; `HollowCore` → {`HollowNetwork`, `HollowPCG`}; поверх всех — primary `HollowGame`, затем `HollowUI`; плюс Editor-модуль `HollowEditor`.
- Зависимости строго ацикличны. Связь «вверх» идёт через интерфейсы `HollowCore` (`IHollowInteractable`, `IHollowToolTarget`, `IHollowStructural`, `IHollowCarryable`), GameplayTags, делегаты и `TSoftClassPtr` в DataAssets.
- Чистая логика (генератор, математика дверей, резолвер экстракции) живёт в plain C++ и тестируется без `UWorld`.
- Контент секторов (включая Сектор I) оформлен как content-only GameFeature Plugins. Жизненный цикл плагина — в [ADR-0012](ADR-0012-sector-game-feature-lifecycle.md).

## Последствия
- (+) Домены изолированы, тесты получаются точечными, DLC-пайплайн проверяется с первого дня.
- (−) Больше Build.cs и `*_API`-экспорта. Правило: экспортируем только то, что реально нужно соседям.
