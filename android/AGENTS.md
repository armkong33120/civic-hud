# Android working instructions

Read README.md, CHANGELOG.md, docs/AUTH_SETUP.md and AGENT_LOOP_STATE.md before changes. Continue verified code instead of starting over.

The owner requires every delivered app update to increment versionCode/versionName, document changes and validation in CHANGELOG.md, and update GitHub. Follow docs/RELEASE_PROCESS.md. Never claim a local commit was pushed or a provider flow works without evidence.

Preserve actual mileage, calibration and service history. Android uses PID010D speed integration only and a3000ms stale cutoff. Do not copy the historical ESP32 counter method. Keep debug simulation in its separate database; never fabricate real vehicle service records for a preview.

Provider registrations/account backend/cloud sync are not live. Keep pending provider methods honest and offline entry usable. UI consumes repository state only. Do not embed client secrets or publish raw phone databases, screenshots, logs, private contacts/GPS, PIN or local config. Curated aggregate verification summaries may be published.

Use relevant tests and physical-device checks for changed behavior. Do not use a power/lock key during ADB work. Keep screen awake as authorized.

Current brand is LuckRot. Keep internal application ID com.civichud.maintenance to preserve upgrades/data and prepared callback config. Source mascot/prompts:brand/README.md. Always install using adb install --user 0 -r; verify the removed user95 Dual App copy stays uninstalled.
