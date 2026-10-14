#!/usr/bin/env bash
# ==============================================================================
# Civic HUD E2E Test Suite Runner
# Opaque-Box E2E Host Tests (Tiers 1–4: F1–F27)
# ==============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

CLANG_BIN="/Library/Developer/CommandLineTools/usr/bin/clang++"
SYSROOT="/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk"
CXX_FLAGS="-isysroot ${SYSROOT} -std=c++17 -O2 -I${PROJECT_ROOT} -I${PROJECT_ROOT}/tests"

BUILD_DIR="/tmp/civic_hud_tests"
mkdir -p "${BUILD_DIR}"

echo "======================================================================"
echo "    HONDA CIVIC FC 1.8 MAINTENANCE HUD - MASTER E2E TEST RUNNER"
echo "======================================================================"
echo "Project Root : ${PROJECT_ROOT}"
echo "Compiler     : ${CLANG_BIN}"
echo "Build Dir    : ${BUILD_DIR}"
echo "Timestamp    : $(date -u +"%Y-%m-%dT%H:%M:%SZ")"
echo "======================================================================"

FAILED=0

run_suite() {
    local suite_name="$1"
    local bin_name="$2"
    shift 2
    local src_files=("$@")

    echo ""
    echo "----------------------------------------------------------------------"
    echo ">>> Compiling and Running: ${suite_name}"
    echo "----------------------------------------------------------------------"

    local bin_path="${BUILD_DIR}/${bin_name}"

    if ! "${CLANG_BIN}" ${CXX_FLAGS} "${src_files[@]}" -o "${bin_path}"; then
        echo "❌ COMPILATION FAILED: ${suite_name}"
        FAILED=1
        return 1
    fi

    if ! "${bin_path}"; then
        echo "❌ TEST EXECUTION FAILED: ${suite_name}"
        FAILED=1
        return 1
    fi

    echo "✅ SUITE PASSED: ${suite_name}"
}

# 1. ELM327 Simulator & OBD Protocol Engine Suite (F1–F9)
run_suite "ELM327 Protocol & Simulation Suite (F1-F9)" \
          "test_elm_sim" \
          "${PROJECT_ROOT}/tests/test_elm_simulator.cpp" \
          "${PROJECT_ROOT}/OdometerEngine.cpp"

# 2. Storage NVS & Web Settings Server Suite (F10–F16)
run_suite "Storage & Web Settings Suite (F10-F16)" \
          "test_storage_web" \
          "${PROJECT_ROOT}/tests/test_storage_web.cpp"

# 3. Thai Typography & Layout C Engine Suite (F17–F27)
run_suite "Thai Typography & Layout C Engine Suite (F17-F27)" \
          "test_thai_engine" \
          "${PROJECT_ROOT}/tests/test_thai_engine.cpp"

# 4. Integrated Real-World Scenarios Suite (Tier 4)
run_suite "Integration & Real-World Scenarios Suite (Tier 4)" \
          "test_integration" \
          "${PROJECT_ROOT}/tests/test_integration_scenarios.cpp" \
          "${PROJECT_ROOT}/OdometerEngine.cpp" \
          "${PROJECT_ROOT}/MaintenanceEngine.cpp"

# 5. Challenger 2 Empirical BT State Machine & Speed Precision Suite
run_suite "Challenger 2 Empirical BT State Machine & Speed Precision Suite" \
          "test_challenger_bt_speed" \
          "${PROJECT_ROOT}/tests/test_challenger_bt_speed.cpp" \
          "${PROJECT_ROOT}/OdometerEngine.cpp" \
          "${PROJECT_ROOT}/ObdTask.cpp"

# 6. Challenger 2 Empirical Web Settings Server & Serial Bridge Suite
run_suite "Challenger 2 Empirical Web Settings & Serial Bridge Suite" \
          "test_challenger_web" \
          "${PROJECT_ROOT}/tests/test_challenger_m2_web.cpp" \
          "${PROJECT_ROOT}/StorageManager.cpp" \
          "${PROJECT_ROOT}/MaintenanceEngine.cpp" \
          "${PROJECT_ROOT}/OdometerEngine.cpp" \
          "${PROJECT_ROOT}/ObdTask.cpp"

# 7. Challenger 1 Empirical Storage Subsystem & Corruption Adversarial Suite
run_suite "Challenger 1 Empirical Storage Subsystem & Corruption Suite" \
          "test_challenger_storage" \
          "${PROJECT_ROOT}/tests/test_challenger_storage.cpp" \
          "${PROJECT_ROOT}/MaintenanceEngine.cpp" \
          "${PROJECT_ROOT}/OdometerEngine.cpp" \
          "${PROJECT_ROOT}/ObdTask.cpp"

# M2 regressions use production StorageManager (included for fault injection).
run_suite "M2 Production Regressions" \
          "test_m2_regressions" \
          "${PROJECT_ROOT}/tests/test_m2_regressions.cpp" \
          "${PROJECT_ROOT}/MaintenanceEngine.cpp" \
          "${PROJECT_ROOT}/OdometerEngine.cpp" \
          "${PROJECT_ROOT}/ObdTask.cpp"

# Actual production modules added during continuation (not independent UI models).
run_suite "Production UI Controller & Thai Font" "test_production_ui" "${PROJECT_ROOT}/tests/test_production_ui.cpp"
run_suite "Production Odometer Checkpoint & Strict PID Parser" "test_obd_checkpoint" "${PROJECT_ROOT}/tests/test_obd_checkpoint.cpp" "${PROJECT_ROOT}/OdometerEngine.cpp" "${PROJECT_ROOT}/ObdTask.cpp"
run_suite "Adversarial M1 Production Parser & Odometer" "test_adversarial_m1" "${PROJECT_ROOT}/tests/test_adversarial_m1.cpp" "${PROJECT_ROOT}/OdometerEngine.cpp" "${PROJECT_ROOT}/ObdTask.cpp"

echo ""
echo "======================================================================"
if [ ${FAILED} -eq 0 ]; then
    echo "🎉 ALL E2E TEST SUITES PASSED SUCCESSFULLY (100% PASS RATE)!"
    echo "   Measured counts appear in each suite summary above."
    echo "======================================================================"
    exit 0
else
    echo "❌ ONE OR MORE TEST SUITES FAILED!"
    echo "======================================================================"
    exit 1
fi

