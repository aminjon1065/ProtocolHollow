# ADR-0006: Онлайн-стек — OSS Steam + SteamSockets, соло = listen, голос через UE VOIP

- **Статус:** Accepted · 2026-11-19 (Proposed → Accepted: подтверждено владельцем проекта, Q1–Q6)
- **Связано:** CLAUDE.md §1 (Solo & Co-op Parity), §6 · [Блок 1](../Architecture/01_Module_and_Plugin_Map.md), [Блок 3 §3.8](../Architecture/03_Physics_and_Replication_Deep_Dive.md)

## Решение
1. `OnlineSubsystemSteam` (лобби, инвайты, присутствие) и `SteamSockets` net driver (Steam Datagram Relay: NAT traversal, IP хоста скрыт). Fallback — `IpNetDriver`.
2. Сессии строятся поверх `CommonUser` из Lyra в `UHollowSessionSubsystem`.
3. **Соло — приватная listen-сессия на 1 игрока.** Код один и тот же, в хаб можно позвать друзей.
4. В PIE используется Null OSS; Steam проверяется еженедельно упакованным билдом на 2 машинах.
5. **Голос:** движковый VOIP поверх игрового соединения, `UVOIPTalker` с аттенюацией, окклюзией и эффектами «шлем/рация».
6. **Голос → ИИ** не зависит от бэкенда: клиент меряет огибающую микрофона и шлёт `Server_ReportVoiceLoudness(uint8)` (unreliable, ≤5 Гц, выше порога), сервер передаёт это в `UHollowNoiseSubsystem` → `UAISense_Hearing::ReportNoiseEvent` с тегом `Hollow.Noise.Voice`.
7. Запасной вариант — EOS Voice. Переключение не затрагивает ИИ.
8. Версия сборки (BuildId) кладётся в настройки сессии и сверяется при входе.
9. Host migration не делаем (см. ADR-0005 «Signal Lost»).

## Последствия
- (+) Нет серверов и их затрат; один путь кода для соло и коопа.
- (−) Качество UE VOIP нужно проверить спайком (Sprint 4).
