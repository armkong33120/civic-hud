## 2026-10-13T15:16:32Z
You are E2E Test Writer (teamwork_preview_test_writer).
Your working directory is: /Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/test_writer_e2e_1/

You MUST read:
/Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/ORIGINAL_REQUEST.md
/Users/arm/Documents/Arduino/civic_hud/PROJECT.md
/Users/arm/Documents/Arduino/civic_hud/TEST_INFRA.md
/Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/spec_miner_survey_1/survey_spec.md
/Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/explorer_obd_storage_survey_3/survey_obd_storage.md
/Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/explorer_codebase_survey_2/survey_codebase.md

Your mission:
Design and implement the comprehensive opaque-box E2E test suite covering Tiers 1-4 across all project features defined in TEST_INFRA.md and PROJECT.md.
Write tests under /Users/arm/Documents/Arduino/civic_hud/tests/:
1. tests/test_elm_simulator.cpp:
   - Automated ELM327 simulator feeding realistic response streams (echo, SEARCHING..., NO DATA, ?, split packets, slow responses, > prompt).
   - Asserts PID 01 31 distance parsing and exact delta km across sequence.
   - Asserts ECU counter wrap (65535 -> 0) and ECU DTC clear (counter drops) without losing or double-counting km.
   - Asserts PID 01 0D speed integration fallback when PID 01 31 returns NO DATA.
   - Asserts Bluetooth state machine: disconnect -> retries without blocking -> reconnects in <= 10s.
2. tests/test_storage_web.cpp:
   - NVS packed POD struct serialization for 14+ items.
   - LittleFS CSV service history formatting and parsing.
   - Web Settings HTML form parsing, ODO update, interval modification (10,000 -> 15,000 km), visibility toggles, add-item.
3. tests/test_thai_engine.cpp:
   - UTF-8 Thai parsing and 4-tier combining glyph layout (no floating vowels, correct stack order).
   - Urgency sorting math: min(r_km, r_days), Overdue > Due Soon > OK.
   - Usage progress bar math: (1 - ratio) * 100% pinned at 100% for overdue.
   - Pagination: 5 items per page across 14 items.
   - Touch axis swap formula.
4. tests/test_integration_scenarios.cpp:
   - Real-world application scenarios (Tier 4):
     - Highway commute (odometer advances 25 km, countdowns decrement).
     - Tunnel/signal loss & auto-reconnect in <= 10s.
     - Oil change service reset & history logging.
     - Web settings reconfiguration & persistence roundtrip.
     - ECU diagnostic reset event.
5. tests/run_all_tests.sh:
   - Bash script compiling all test suites with:
     /Library/Developer/CommandLineTools/usr/bin/clang++ -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -std=c++17
   - Runs all tests and verifies 100% pass.
6. When all tests pass, generate /Users/arm/Documents/Arduino/civic_hud/TEST_READY.md per the template in PROJECT.md / TEST_INFRA.md with exact test counts per Tier and command to execute.

Write handoff report to /Users/arm/Documents/Arduino/civic_hud/.agents/teamwork/test_writer_e2e_1/handoff.md.
Maintain progress.md in your directory.
Send message to orchestrator upon completion.
