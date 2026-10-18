Owner requires every delivered Android update to bump versionCode/versionName, update android/CHANGELOG.md and publish verified source/docs plus a version tag to GitHub. Follow android/docs/RELEASE_PROCESS.md.

# Working instructions for the next AI

Read `AI_HANDOFF.md`, `docs/CURRENT_STATE.md`, `docs/DECISIONS.md`, then the relevant source files before editing.

- This repo contains implemented ESP32 firmware and an implemented offline Android MVP under android/. Do not report cloud/auth as built or simulation as real-car validation.
- Continue existing work; do not reimplement tested subsystems without evidence of a problem.
- Source for the Arduino sketch is `firmware/civic_hud/civic_hud.ino`. The board is ESP32-2432S028R, not ESP32-C6.
- Preserve vehicle NVS state, mileage checkpoint and service history. Never seed fabricated service records or reset mileage for a demonstration.
- ESP32 only: PID 01 31 is distance since DTC clearing; preserve its checkpoint logic. Android immutable rule: PID010D speed integration only,3second cutoff,no ECU-distance counter.
- The user stopped the engine and asked not to run extra OBD scanning/connection tests. Get fresh context before any device action. Publishing this repository did not authorize flashing or changing hardware.
- Inspect available serial ports. Do not hardcode the old port. Opening serial can cause a board reset; never keep serial open during flashing.
- Azure CLI inspection is complete (Students enabled); no cloud resources/live auth/sync implemented. Current owner authorized configurable auth-client preparation and setup documentation only; live provider registration/backend/cloud sync remain absent. Do not spend credits while documenting/testing offline features.
- Keep UI simple and Thai-friendly. No decorative service icons. Follow approved bar progression and day/night behavior.
- Test relevant changed behavior, not every historical suite indiscriminately. Do not claim old simulated tests demonstrate live Bluetooth or hardware correctness.
- Never commit account tokens, environment secrets, firmware flash/NVS backups, private contact records or location logs.
- Communicate in plain Thai to the owner. Record actual validation and limitations when handing work on.
