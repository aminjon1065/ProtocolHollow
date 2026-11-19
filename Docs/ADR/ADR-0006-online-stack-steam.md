# ADR-0006: Онлайн-стек — OSS Steam + SteamSockets, соло = listen, голос через UE VOIP

- **Статус:** Accepted · 2026-11-19 (Proposed → Accepted: подтверждено владельцем проекта, Q1–Q6)
- **Связано:** CLAUDE.md §1 (Solo & Co-op Parity), §6 · [Блок 1](../Architecture/01_Module_and_Plugin_Map.md), [Блок 3 §3.8](../Architecture/03_Physics_and_Replication_Deep_Dive.md)

## Решение
1. `OnlineSubsystemSteam` (лобби, инвайты, присутствие) и `SteamSockets` net driver (Steam Datagram Relay: NAT traversal, IP хоста скрыт). Fallback — `IpNetDriver`.
2. Сессии — **свой тонкий `UHollowSessionSubsystem` поверх OSSv1** (решение 2026-11-19, вместо CommonUser из Lyra: у нас только Steam на PC, мультиплатформенный логин, привилегии и OSSv2 не нужны). Правила сессий — чистые функции `HollowSessionRules` с автотестами.
3. **Соло — та же listen-сессия с приватностью `Solo`**: не рекламируется, без инвайтов, `PreLogin` отклоняет входящих. Чтобы позвать друзей, хост поднимает сессию `InviteOnly` или `FriendsOnly`. Путь кода один и тот же.
4. В PIE используется Null OSS **автоматически**: в editor-сборках Steam OSS включается только с `-game` (проверено по исходникам 5.8), SteamSockets в редакторе тоже не активируется. Steam проверяется через `-game` на Mac и упакованный Win64-билд на двух аккаунтах.
5. **Голос:** движковый VOIP поверх игрового соединения, `UVOIPTalker` с аттенюацией, окклюзией и эффектами «шлем/рация».
6. **Голос → ИИ** не зависит от бэкенда: клиент меряет огибающую микрофона и шлёт `Server_ReportVoiceLoudness(uint8)` (unreliable, ≤5 Гц, выше порога), сервер передаёт это в `UHollowNoiseSubsystem` → `UAISense_Hearing::ReportNoiseEvent` с тегом `Hollow.Noise.Voice`.
7. Запасной вариант — EOS Voice. Переключение не затрагивает ИИ.
8. **Совместимость сборок — средствами движка:** сетевое рукопожатие (`FNetworkVersion`, учитывает `ProjectVersion` из DefaultGame.ini) отвергает несовместимого клиента, а `FOnlineSessionSettings::BuildUniqueId` (changelist движка) позволяет отсеять приглашение ещё на клиенте. Своего BuildId не делаем. **Правило:** при каждой несовместимой по сети сборке поднимаем `ProjectVersion`.
9. Host migration не делаем (см. ADR-0005 «Signal Lost»).
10. **В вертикальном срезе вход только в хаб** (решение 2026-11-19): `AHollowGameModeBase::PreLogin` отклоняет вход, если фаза не Hub, сессия соло или нет места. Архитектура (state-based репликация) не мешает разрешить вход посреди рейда позже.

## Последствия
- (+) Нет серверов и их затрат; один путь кода для соло и коопа.
- (−) Качество UE VOIP нужно проверить спайком (Sprint 4).
