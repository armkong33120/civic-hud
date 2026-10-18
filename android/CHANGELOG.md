# Changelog

## 0.3.0 — 2026-10-18 (versionCode 3)

### Changed
- Start with a shared vector road/check logo and Android31+ native animated splash/short exit, then route unauthenticated users to an account welcome screen. No artificial loading timer, bitmap assets or new splash dependency.
- Separate login and signup views with Google/LINE/Facebook/email availability cards. Provider-specific activation is pending registration/configuration; no simulated authentication.
- Keep a deliberate “เข้าใช้งานออฟไลน์ก่อน” path. It opens the existing dashboard without marking the user signed in or syncing data. Hot resume preserves the offline choice; unauthenticated cold restart shows welcome again.
- Move notification permission request from app launch to Bluetooth use, so initial account entry is uninterrupted.
- Preserve existing vehicle mileage/history and the existing account broker integration.

### Validation / limits
- 68 existing unit tests pass; debug and unsigned release build pass.
- Physical S22(Android16) checks: welcome/logo, distinct signup view, disabled unregistered methods, offline dashboard, hot resume, cold restart gate and unchanged actual odometer/history. Native launch recording captured locally.
- Live provider login/signup/account linking/cloud sync are not operational until the registrations/backend in `docs/AUTH_SETUP.md` are completed. A native splash may be skipped by Android on a hot resume; it is not an extra loading page.

## 0.2.0 — 2026-10-18 (versionCode 2)

- Modern Compose dashboard, live odometer/progress transitions and expandable service history.
- Persisted light/dark/system theme and reduced motion preferences.
- Editable vehicle profile with Room1→2 migration preserving existing mileage/history.
- Durable local feedback outbox and user-selected sharing; no automatic cloud delivery.
- Prepared AppAuth code+PKCE account client, backend contracts and provider setup/research documentation.68 unit tests and S22 upgrade/simulation checks pass.

## 0.1.0 — 2026-10-18 (versionCode 1)

- Offline Android MVP: real Bluetooth Classic SPP/ELM327 transport, PID010D-only distance integration,3s stale cutoff, Room storage, calibration, dual maintenance schedules, service history and local notifications.
- Shell-protected isolated debug OBD injection.55 unit tests and initial S22 A–E simulation checks pass. Actual-car verification remains separate.
