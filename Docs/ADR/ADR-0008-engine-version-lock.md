# ADR-0008: Фиксация версии движка — Unreal Engine 5.8.x

- **Статус:** Accepted · 2026-11-19 (Proposed → Accepted: подтверждено владельцем проекта, Q1–Q6)
- **Связано:** бриф §6 («5.4 / 5.5+») · [Блок 1 §1.1](../Architecture/01_Module_and_Plugin_Map.md)

## Контекст
UE 5.8 вышла в июне 2026 (есть хотфикс 5.8.1). По имеющимся данным, это последний релиз ветки UE5 перед UE6. В 5.8 MegaLights перешёл в Production Ready, появился Lumen Lite (Beta, GI «Medium», цель — слабые ПК и хэндхелды), SSGI объявлен deprecated, расширен Chaos Caching с runtime-воспроизведением. PCG в статусе Production Ready с 5.7.

## Решение
- Проект стартует и живёт на **UE 5.8.x** до релиза 1.0. Обновляемся только на хотфиксы 5.8.x, каждое обновление — отдельный коммит, прогон тестов и smoke-тест на 2 игроках.
- Переход на UE6 — отдельный ADR после 1.0 или при критичной для проекта фиче.
- Steam Deck и пресет Low строятся на Lumen Lite, так как SSGI deprecated, а процедурный уровень запечь нельзя.

## Последствия
- (+) Стабильная база без миграций в разгар производства.
- (−) Новые фичи UE6 до 1.0 недоступны.

## Источники
- https://gamefromscratch.com/unreal-engine-5-8-released/
- https://tomlooman.com/unreal-engine-5-8-performance-highlights/
- https://gamefromscratch.com/unreal-engine-5-7-released/
