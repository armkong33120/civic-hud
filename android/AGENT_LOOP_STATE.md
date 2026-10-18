# Agent loop state — Smart Vehicle Maintenance Tracker

Latest directive: 18 October 2026. Android project: outputs/smart-vehicle-tracker.

## Loop status
- Loop 0 COMPLETE: handoff read, existing ESP32 preserved; JDK17, SDK36, Gradle8.13 located; S22 ADB online; stay-on and timeout requested.
- Loop 1 COMPLETE: 3-module contracts in docs/CONTRACTS.md; debug DUMP-protected receiver and isolated simulation DB.
- Loop 2 COMPLETE: domain, Room and Compose specialists running; root integrates transport/foreground service.
- Loop 3 COMPLETE: debug APK built; 39 domain +9 storage +7 protocol tests passed. GPS type issue and compatibility warning corrected.
- Loop 4 COMPLETE: A-E, actualRoom snapshots, customrules/notifications, malformedreply, freshfirstsample and nonzero coldrestart passed. GPS-on-tap gracefully timedout without erasing form; actualfix not obtained.
- Loop 5 COMPLETE: independent domain/service and storage critics signed off within physical S22 simulation scope. Actual car/adapter and outdoor GPS fix remain real-world validation tasks, documented in morning guide.

## Requirements / evidence gates
- [x] Modular Kotlin/Compose + pure domain + Room data modules.
- [x] Only PID010D distance; no PID0131 in Android production calculation.
- [x] Validated AT init, Bluetooth Classic SPP, bounded framing/timeouts/reconnect.
- [x] Monotonic speed integration, invalid/disconnect/stall >3000ms cuts gap.
- [x] Room checkpoint every0.5km/60s/transitions, sync-ready metadata.
- [x] Calibration, custom dual km/month intervals, service transaction/history/optional fields.
- [x] GPS-on-tap and graceful permission handling.
- [x] Dual warning notifications and periodic date worker.
- [x] Debug-only shell-protected simulator uses isolated DB; release merged manifest has no injection receiver.
- [x] Unit tests and debug/release compile clean:55tests,0failures,0Kotlinwarnings.
- [x] S22 A normal / B dropout / C background idle / D reconnect / E UI+restart persistence passed.
- [x] No fatal crashes in scoped test logs; review actual device evidence.
- [x] Morning guide accurately distinguishes simulation from actual car validation.

## Defects found and repaired during loops
- Freshness checked before integration; old/future frames rejected.
- Start/stop controls serialized; calibration and service save share session mutex.
- AT initialization rejects explicit error responses; calibration flushes checkpoint.
- Notification permission/channel enabled checked before dedup reservation; simulation notifications clearly labeled.
- Adapter-controlled percent text formatted separately to prevent notification crash; unsolicited serial drain bounded.
- Stale interval editor preserves latest service baseline/cycle.

## Constraints
No cloud/auth implementation. No real ECU test claims from simulation. Do not use power/lock key. Keep PIN private and out of artifacts. Never alter ESP32 data or vehicle history to simulate tests.

## Final evidence and scope
55unit tests,0failures;debug/releasebuild,0Kotlinwarnings;A-Epass on SM-S901E Android16. Crashbuffer has0ownedFATAL. Actual carECU anddeepDoze not falsely claimed; GPSfix unavailable indoors, manualform andtimeout verified. No cloud/auth or ESP32 changes.

# UX/profile/account/feedback iteration — 18 Oct 2026
- Research ACTIVE: Android motion/accessibility and provider/Play requirements reviewed; analysis in docs/UX_AUTH_UPGRADE_PLAN.md.
- Owner confirms no registered OAuth apps: prepare code/setup, no fake authentication or paid cloud resources.
- Profile + feedback storage migration PENDING; preserve existing mileage/history.
- Modern UI/motion + profile/account/feedback flows PENDING.
- Regression/build/migration/S22 verification PENDING.
- Previous HIL55-test pass is historical baseline, not proof of new changes.

### UX/profile/auth preparation — implementation gate (18 Oct 2026)
- Storage agent: Room v2/profile/feedback implemented;16 data tests pass;3 v1 device snapshot migrations preserve old values and match v2 schema.
- UI/auth agents hit usage limit; lead continued implementation. No claim those agents finished implementation.
- Lead implemented modern odometer/maintenance motion, persisted theme/reduced motion, custom vehicle form, durable feedback form+explicit sharing, AppAuth public-client PKCE integration+server-verified account contracts and setup guide.
- External provider registrations absent by owner report. Live OAuth, account linking/deletion backend and feedback delivery remain unverified/unprovisioned. Offline core remains available.
- Loop3 build/test running; Loop4 upgraded-device migration/UI checks pending. Previous55-test evidence is historical baseline only.

### UX/profile/auth preparation — verification transition Loop3→4→5
- Loop3:68 unit tests pass (domain39/data16/app13), debug+unsignedrelease build pass, no Kotlin warnings.
- Loop4:S22 APKupgrade+Room1→2 succeeds. All actual-domain table original columns unchanged. Simulation UI profile/feedback survives restart; feedback PENDING with no receipt. New driving/dropout/background/reconnect regression passes; stopped service and returned real mode.
- Loop5 lead review: no production TODO/FIXME/fake mileage; Android distance code contains no0131. Auth remains config-gated and requires backend verification. Live OAuth/backend/real car/deepDoze remain explicitly unverified, consistent with owner's prepare-only account request.
- Detailed scope:docs/UX_UPGRADE_REPORT.md; provider setup:docs/AUTH_SETUP.md. This completes preparation, not live cloud-account delivery.
- Final installed build: appearance choices confirmed across force-stop/relaunch, unconfigured account login disabled, light/dark captures visually reviewed. Appearance writes committed off the UI thread before publishing saved state. Final device appearance restored to system; reduce motion off; real tracking stopped.
- Final verification package refreshed; source/docs publish excludes raw screenshots, phone databases/backups/logs and local configuration.

### 0.3.0 launch/account entry iteration — 18 Oct 2026
- Owner requested branded light splash → login/signup choice for unauthenticated entry, plus GitHub version/changelog for every update.
- Added native Android31+ branded vector splash+short exit animation, shared launcher/welcome mark and Compose account entry. No artificial loading timer or network requirement to reach entry.
- Account welcome separates login/signup, lists Google/LINE/Facebook/email as awaiting activation; real configured broker remains available through its hosted flow. Explicit offline session path preserves usable offline core; guest is never marked signed-in.
- Notification permission deferred until Bluetooth use. VersionCode3/versionName0.3.0. Device gate/build/public changelog pending.
- 0.3.0 Loop3:68 unit tests pass; debug/unsignedrelease builds pass, no Kotlinwarnings.
- 0.3.0 Loop4:S22 installed build passes cold welcome/logo, signup switch, disabled pending providers, explicit offline entry, hot resume and cold restart gate. Actual mileage/history unchanged. Native launch video captured locally; app-owned crash records absent.
- 0.3.0 Loop5:reviewed account welcome screenshot, no fake sign-in/registration and no new cloud provisioning. No extra loading timer; Android can skip native splash on hot resume. Sign-in session remains memory-only while provider/backend setup is pending. GitHub CHANGELOG/version/tag update required for each delivered build from now on.
- Publish receipt:GitHub main and peeled tag android-v0.3.0 both point to fd2a787. Preview release published at https://github.com/armkong33120/civic-hud/releases/tag/android-v0.3.0 ; APK/sourceZIP/checksum JSON all uploaded, not draft. Installed APK SHA256 matches345d9568aa028fdf762d91d9e896815388e6b6ea20856b1d255e5f7f41bd1551. Final phone is on unauthenticated welcome; core tracking remains stopped.

### Device-only cleanup — duplicate Samsung Dual App
Owner explicitly requested removal of the confusing duplicate. Removed com.civichud.maintenance only from Samsung DUAL_APP user95 using user-scoped uninstall. Verified user95 installed=false, user0 installed=true with unchanged data directory inodes; main0.3.0 launch succeeds. No APK/source changes, so no app version increment for this device-only cleanup. All future ADB installs must use `adb install --user 0 -r ...`; never omit the target user on this Samsung device.

### LuckRot branding0.3.1 — implementation/build gate
- Owner chose LuckRot and cartoon mechanic standing with folded arms and wrench. Built-in imagegen produced full mascot + compact icon rendition, copied into project brand/; only512px alpha-preserving runtime export ships in APK.
- Updated launcher/app label, shared mascot icon/adaptive mask/native splash/welcome and dashboard title. Kept internal appID and DB/callback paths unchanged to preserve data.
- VersionCode4/versionName0.3.1; app13 relevant tests+debug/release builds pass. Main-profile-only install uses --user0 explicitly. Device verification/public release gate in progress.
- 0.3.1 device/audit gate passed:S22 launcher screenshot shows one LuckRot mechanic icon; app-info XML and welcome/dashboard show LuckRot. Signup/offline navigation and cold launch work. Actual odometer/history unchanged, user0 installed/version4, user95 remains uninstalled. No app-owned crash record. Only relevant app13 tests rerun; domain/data55 passing baseline unchanged. Runtime mascot PNG512×512RGBA; master artwork excluded from APK. Publish preview/version tag next.
