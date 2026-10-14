## 2026-10-13T16:25:57Z
You are Firmware Worker for Milestone 2 Remediation - Iteration 2 (teamwork_preview_worker).
Your working directory is: /Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/worker_m2_storage_2/

You MUST read:
/Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/ORIGINAL_REQUEST.md
/Users/arm/Documents/Arduino/civic_hud/PROJECT.md
/Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/challenger_m2_1/handoff.md
/Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/reviewer_m2_1/handoff.md

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_preview_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

Exclusive file write ownership:
- /Users/arm/Documents/Arduino/civic_hud/StorageManager.h
- /Users/arm/Documents/Arduino/civic_hud/StorageManager.cpp
- /Users/arm/Documents/Arduino/civic_hud/tests/test_storage_web.cpp
- /Users/arm/Documents/Arduino/civic_hud/tests/test_challenger_storage.cpp

Your mission:
Remediate the findings from Milestone 2 Iteration 1:
1. Fix BUG-M2-01 (CRITICAL - Silent Data Loss & Mutilated Thai Glyphs):
   - In StorageManager.h, expand PackedItemRecord::name from char name[48] to char name[80].
   - Default Thai items (oil: 62B, tire: 50B, air: 68B, cabin: 61B, lpg_t: 51B, lpg_v: 50B) must fit completely without truncation!
   - Maintain #pragma pack(push, 1) and calculate the new struct size.
2. Fix BUG-M2-02 (Buffer Over-Read Risk):
   - In StorageManager::loadItems(), use bounded string extraction via strnlen(blob[i].name, sizeof(blob[i].name)) so missing null terminators in flash do not cause out-of-bounds reads.
3. Fix BUG-M2-03 (CSV Column Misalignment):
   - Handle commas in item names in CSV logs safely (quote or sanitize).
4. Update Test Suites:
   - In tests/test_storage_web.cpp and tests/test_challenger_storage.cpp, update PackedItemRecord struct definition to char name[80] and adjust struct size assertions.
5. Verification:
   - Run: ./tests/run_all_tests.sh (all suites must pass 100%).
   - Compile firmware:
     /opt/homebrew/bin/arduino-cli compile --fqbn esp32:esp32:jczn_2432s028r:PartitionScheme=huge_app,UploadSpeed=460800 /Users/arm/Documents/Arduino/civic_hud
   - Must build with 0 errors and 0 warnings.

Write handoff report to /Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/worker_m2_storage_2/handoff.md.
Maintain progress.md in your directory.
Send message to orchestrator upon completion.
