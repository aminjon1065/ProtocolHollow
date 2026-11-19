# ADR-0007: Технологии, сознательно исключённые из v1

- **Статус:** Accepted · 2026-11-19 (Proposed → Accepted: подтверждено владельцем проекта, Q1–Q6)
- **Связано:** [Блок 1 §1.5](../Architecture/01_Module_and_Plugin_Map.md), [Блок 5](../Architecture/05_Risk_Map_and_Scope_Control.md)

| Технология | Причина отказа | Когда пересмотреть |
|---|---|---|
| Iris | generic + push model + dormancy хватает на 4 игроков | если Insights покажет CPU репликации хоста > 1.5 мс |
| ReplicationGraph | масштаб не тот (≤ 4 соединения, ~300 актёров) | никогда для этого проекта |
| Mover / Network Prediction | API нестабилен; CMC с saved moves покрывает задачи | после 1.0 |
| Physics resimulation / NetworkPhysicsComponent | CPU хоста и сложность; принцип ADR-0002 | только для изолированной фичи после 1.0 |
| Mass Entity / MassAI | рой ≤ 20 агентов | если рой понадобится ≥ 50 |
| World Partition для рейда | рейд собирается из Level Instances | никогда для рейда |
| Wwise / FMOD | MetaSounds + Modulation бесплатны и интегрированы | при найме саунд-дизайнера с требованием |
| MetaHuman | лица скрыты шлемами | кат-сцены в хабе, если понадобятся |
| Vivox / EOS Voice | только fallback (ADR-0006) | если спайк UE VOIP провалится |
| Свободная runtime-резка мешей | не реплицируется, дорого; используем швы | никогда в v1 |

Подключение любой из этих технологий — **отдельный ADR**.
