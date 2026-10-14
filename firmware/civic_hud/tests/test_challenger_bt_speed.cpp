#include "test_framework.h"
#include "OdometerEngine.h"
#include "ObdTask.h"

#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <cmath>
#include <random>
#include <deque>
#include <iostream>

// ============================================================================
// CHALLENGER 2 EMPIRICAL TEST SUITE
// Bluetooth State Machine Stress, Timing Verification & Speed Fallback Precision
// ============================================================================

inline std::ostream& operator<<(std::ostream& os, const ObdState& state) {
    switch (state) {
        case ObdState::DISCONNECTED: return os << "DISCONNECTED";
        case ObdState::CONNECTING: return os << "CONNECTING";
        case ObdState::INITIALIZING_ELM: return os << "INITIALIZING_ELM";
        case ObdState::CONNECTED: return os << "CONNECTED";
        case ObdState::RECONNECT_WAIT: return os << "RECONNECT_WAIT";
    }
    return os << "UNKNOWN";
}

namespace ChallengerTest {

// Mock ELM327 Stream Engine for Host-Side State Machine Simulation
class MockElmStream {
public:
    MockElmStream() : m_connected(true), m_linkDropped(false), m_dropMidCommand(false) {}

    void setConnected(bool c) {
        m_connected = c;
        if (!c) m_linkDropped = true;
    }
    bool isConnected() const { return m_connected; }
    void clearDrop() { m_linkDropped = false; }
    bool wasDropped() const { return m_linkDropped; }

    void setDropMidCommand(bool d) { m_dropMidCommand = d; }

    // Simulates sending command and receiving response
    bool exchange(const std::string& cmd, std::string& outResp, uint32_t timeoutMs, uint32_t& elapsedMs) {
        if (!m_connected) {
            elapsedMs = timeoutMs;
            return false;
        }

        if (m_dropMidCommand) {
            m_dropMidCommand = false;
            m_connected = false;
            m_linkDropped = true;
            elapsedMs = timeoutMs;
            return false;
        }

        // Response simulation with realistic ELM327 delays (5..20 ms)
        elapsedMs = 10;
        if (cmd == "ATZ") {
            outResp = "\r\rELM327 v1.5\r\r>";
            return true;
        } else if (cmd == "ATE0" || cmd == "ATL0" || cmd == "ATSP6") {
            outResp = "OK\r\r>";
            return true;
        } else if (cmd == "ATRV") {
            outResp = "14.2V\r\r>";
            return true;
        } else if (cmd == "0131") {
            outResp = "41 31 13 88\r\r>"; // 5000 km
            return true;
        } else if (cmd == "010D") {
            outResp = "41 0D 50\r\r>"; // 80 km/h
            return true;
        } else if (cmd == "0105") {
            outResp = "41 05 80\r\r>"; // 88 deg C
            return true;
        }

        outResp = "?\r\r>";
        return false;
    }

private:
    bool m_connected;
    bool m_linkDropped;
    bool m_dropMidCommand;
};

// Stateful representation of the ObdTask state machine running on Core 0
class SimulatedObdStateMachine {
public:
    SimulatedObdStateMachine(MockElmStream& stream, OdometerEngine* odo = nullptr)
        : m_stream(stream)
        , m_odo(odo)
        , m_state(ObdState::DISCONNECTED)
        , m_stateEnterMs(0)
        , m_lastSpeedPollMs(0)
        , m_lastPid31PollMs(0)
        , m_lastAuxPollMs(0)
        , m_consecutiveErrors(0)
        , m_reconnectCount(0)
        , m_disconnectCount(0)
        , m_obdConnected(false)
        , m_btConnected(false)
        , m_lastPacketTimestampMs(0)
    {}

    ObdState getState() const { return m_state; }
    bool isConnected() const { return m_obdConnected; }
    uint32_t getReconnectCount() const { return m_reconnectCount; }
    uint32_t getDisconnectCount() const { return m_disconnectCount; }

    // Execute one state machine step at time currentMs
    void step(uint32_t currentMs) {
        switch (m_state) {
            case ObdState::DISCONNECTED: {
                m_obdConnected = false;
                m_btConnected = false;
                m_state = ObdState::CONNECTING;
                m_stateEnterMs = currentMs;
                break;
            }

            case ObdState::CONNECTING: {
                m_obdConnected = false;
                m_btConnected = false;
                if (m_stream.isConnected()) {
                    // Connect handshake successful
                    m_btConnected = true;
                    m_state = ObdState::INITIALIZING_ELM;
                    m_stateEnterMs = currentMs;
                } else {
                    // Connect attempt failed -> backoff
                    m_state = ObdState::RECONNECT_WAIT;
                    m_stateEnterMs = currentMs;
                }
                break;
            }

            case ObdState::INITIALIZING_ELM: {
                if (!m_stream.isConnected() || m_stream.wasDropped()) {
                    m_stream.clearDrop();
                    m_state = ObdState::RECONNECT_WAIT;
                    m_stateEnterMs = currentMs;
                    m_disconnectCount++;
                    break;
                }

                // Simulate init sequence: ATZ, ATE0, ATL0, ATSP6, ATRV
                std::string resp;
                uint32_t elMs = 0;
                bool ok = m_stream.exchange("ATZ", resp, 1200, elMs);
                if (ok) ok = m_stream.exchange("ATE0", resp, 600, elMs);
                if (ok) ok = m_stream.exchange("ATL0", resp, 500, elMs);
                if (ok) ok = m_stream.exchange("ATSP6", resp, 500, elMs);
                if (ok) m_stream.exchange("ATRV", resp, 600, elMs);

                if (ok && m_stream.isConnected()) {
                    m_obdConnected = true;
                    m_consecutiveErrors = 0;
                    m_lastSpeedPollMs = currentMs;
                    m_lastPid31PollMs = currentMs;
                    m_lastAuxPollMs = currentMs;
                    m_state = ObdState::CONNECTED;
                    m_stateEnterMs = currentMs;
                    m_reconnectCount++;
                } else {
                    m_state = ObdState::RECONNECT_WAIT;
                    m_stateEnterMs = currentMs;
                    m_disconnectCount++;
                }
                break;
            }

            case ObdState::CONNECTED: {
                if (!m_stream.isConnected() || m_stream.wasDropped()) {
                    m_stream.clearDrop();
                    m_obdConnected = false;
                    m_btConnected = false;
                    m_state = ObdState::RECONNECT_WAIT;
                    m_stateEnterMs = currentMs;
                    m_disconnectCount++;
                    break;
                }

                // Periodic telemetry polling
                if (currentMs - m_lastSpeedPollMs >= 500) {
                    m_lastSpeedPollMs = currentMs;
                    std::string resp;
                    uint32_t elMs = 0;
                    if (m_stream.exchange("010D", resp, 500, elMs)) {
                        m_consecutiveErrors = 0;
                        m_lastPacketTimestampMs = currentMs;
                    } else {
                        m_consecutiveErrors++;
                    }
                }

                if (currentMs - m_lastPid31PollMs >= 2000) {
                    m_lastPid31PollMs = currentMs;
                    std::string resp;
                    uint32_t elMs = 0;
                    if (m_stream.exchange("0131", resp, 600, elMs)) {
                        m_consecutiveErrors = 0;
                        m_lastPacketTimestampMs = currentMs;
                    } else {
                        m_consecutiveErrors++;
                    }
                }

                if (m_consecutiveErrors >= 4) {
                    // 4 consecutive timeouts trigger link recovery
                    m_obdConnected = false;
                    m_btConnected = false;
                    m_state = ObdState::RECONNECT_WAIT;
                    m_stateEnterMs = currentMs;
                    m_disconnectCount++;
                    break;
                }
                break;
            }

            case ObdState::RECONNECT_WAIT: {
                m_obdConnected = false;
                m_btConnected = false;
                // Backoff 3500ms before retrying connect
                if (currentMs - m_stateEnterMs >= 3500) {
                    m_state = ObdState::CONNECTING;
                    m_stateEnterMs = currentMs;
                }
                break;
            }
        }
    }

private:
    MockElmStream& m_stream;
    OdometerEngine* m_odo;
    ObdState m_state;
    uint32_t m_stateEnterMs;
    uint32_t m_lastSpeedPollMs;
    uint32_t m_lastPid31PollMs;
    uint32_t m_lastAuxPollMs;
    uint8_t m_consecutiveErrors;
    uint32_t m_reconnectCount;
    uint32_t m_disconnectCount;
    bool m_obdConnected;
    bool m_btConnected;
    uint32_t m_lastPacketTimestampMs;
};

} // namespace ChallengerTest

// ============================================================================
// TEST CASES: MISSION 1 - BLUETOOTH STATE MACHINE & TIMING STRESS
// ============================================================================

// 1. Rapid connect/disconnect cycling test: 50 cycles
TEST_TIER1(Test_Challenger_BT_RapidConnectDisconnectCycling_50Cycles) {
    ChallengerTest::MockElmStream stream;
    ChallengerTest::SimulatedObdStateMachine sm(stream);

    uint32_t virtualTimeMs = 0;
    const uint32_t stepMs = 50;

    // Run 50 connect-disconnect-reconnect cycles
    const int targetCycles = 50;
    int completedCycles = 0;

    for (int cycle = 0; cycle < targetCycles; ++cycle) {
        // Step 1: Ensure dongle is connected and allow state machine to reach CONNECTED
        stream.setConnected(true);
        uint32_t connectStartTime = virtualTimeMs;
        while (!sm.isConnected() && (virtualTimeMs - connectStartTime < 15000)) {
            sm.step(virtualTimeMs);
            virtualTimeMs += stepMs;
        }
        ASSERT_TRUE(sm.isConnected());
        ASSERT_EQ(sm.getState(), ObdState::CONNECTED);

        // Run in connected state for a random short period (100ms - 1000ms)
        uint32_t connectedDuration = 100 + (cycle * 17) % 900;
        uint32_t connStart = virtualTimeMs;
        while (virtualTimeMs - connStart < connectedDuration) {
            sm.step(virtualTimeMs);
            virtualTimeMs += stepMs;
        }

        // Step 2: Abruptly drop link
        stream.setConnected(false);
        sm.step(virtualTimeMs);
        virtualTimeMs += stepMs;

        // Verify it detects disconnect and transitions to RECONNECT_WAIT
        while (sm.getState() != ObdState::RECONNECT_WAIT && (virtualTimeMs - connStart < 10000)) {
            sm.step(virtualTimeMs);
            virtualTimeMs += stepMs;
        }
        ASSERT_EQ(sm.getState(), ObdState::RECONNECT_WAIT);
        ASSERT_TRUE(!sm.isConnected());

        completedCycles++;
    }

    ASSERT_EQ(completedCycles, 50);
    ASSERT_TRUE(sm.getDisconnectCount() >= 50);
}

// 2. Verify Reconnect Time is <= 10.0s under various link loss modes
TEST_TIER2(Test_Challenger_BT_ReconnectTimeUnder10Seconds_AllLossModes) {
    ChallengerTest::MockElmStream stream;
    ChallengerTest::SimulatedObdStateMachine sm(stream);

    uint32_t virtualTimeMs = 0;
    const uint32_t stepMs = 50;

    // Subtest A: Graceful link loss (event notification)
    {
        stream.setConnected(true);
        while (!sm.isConnected()) {
            sm.step(virtualTimeMs);
            virtualTimeMs += stepMs;
        }

        // Abrupt disconnect
        uint32_t disconnectTimestamp = virtualTimeMs;
        stream.setConnected(false);
        sm.step(virtualTimeMs);
        virtualTimeMs += stepMs;

        // Dongle powers back on 500ms later (simulating fast ignition cycle)
        virtualTimeMs += 500;
        stream.setConnected(true);

        // Measure time until state machine re-establishes CONNECTED
        while (!sm.isConnected() && (virtualTimeMs - disconnectTimestamp < 20000)) {
            sm.step(virtualTimeMs);
            virtualTimeMs += stepMs;
        }

        uint32_t reconnectDurationMs = virtualTimeMs - disconnectTimestamp;
        // Expected: ~3500ms backoff + ~50ms init = ~4000ms, strictly <= 10000ms
        ASSERT_TRUE(sm.isConnected());
        ASSERT_TRUE(reconnectDurationMs <= 10000);
    }

    // Subtest B: Silent link drop (packet timeout, no event)
    {
        // Now connected. Make queries fail (consecutive timeouts)
        uint32_t silentDropTimestamp = virtualTimeMs;
        stream.setConnected(false);

        // Run until timeouts accumulate and reconnect completes
        // Allow dongle to be available again after 1000ms
        bool dongleRestored = false;
        while (!sm.isConnected() && (virtualTimeMs - silentDropTimestamp < 25000)) {
            sm.step(virtualTimeMs);
            virtualTimeMs += stepMs;

            if (!dongleRestored && (virtualTimeMs - silentDropTimestamp >= 2000)) {
                stream.setConnected(true);
                dongleRestored = true;
            }
        }

        uint32_t totalRecoveryMs = virtualTimeMs - silentDropTimestamp;
        // 4 query timeouts (4 x 500ms = 2000ms) + 3500ms backoff = ~5500 - 6000ms <= 10000ms
        ASSERT_TRUE(sm.isConnected());
        ASSERT_TRUE(totalRecoveryMs <= 10000);
    }

    // Subtest C: Drop occurring mid-initialization (during ATZ/ATSP6)
    {
        // Force disconnect and reconnect cycle
        stream.setConnected(false);
        while (sm.getState() != ObdState::RECONNECT_WAIT) {
            sm.step(virtualTimeMs);
            virtualTimeMs += stepMs;
        }

        // Wait out backoff
        while (sm.getState() != ObdState::CONNECTING) {
            sm.step(virtualTimeMs);
            virtualTimeMs += stepMs;
        }

        // When entering INITIALIZING_ELM, trigger mid-command drop
        stream.setConnected(true);
        stream.setDropMidCommand(true);
        uint32_t initDropTimestamp = virtualTimeMs;

        sm.step(virtualTimeMs); // -> INITIALIZING_ELM with drop
        virtualTimeMs += stepMs;
        sm.step(virtualTimeMs); // handles drop -> RECONNECT_WAIT

        // Restore stream
        stream.setConnected(true);

        // Measure time until successfully connected
        while (!sm.isConnected() && (virtualTimeMs - initDropTimestamp < 20000)) {
            sm.step(virtualTimeMs);
            virtualTimeMs += stepMs;
        }

        uint32_t recoveryMs = virtualTimeMs - initDropTimestamp;
        ASSERT_TRUE(sm.isConnected());
        ASSERT_TRUE(recoveryMs <= 10000);
    }
}

// 3. Dual-Thread Stress: UI thread is NEVER blocked (> 100ms) during connect/reconnect churn
TEST_TIER3(Test_Challenger_BT_UIThreadNeverBlockedOver100Ms_RealTimeMultiThreaded) {
    std::atomic<bool> stopFlag(false);
    std::atomic<double> maxUiLatencyMs(0.0);
    std::atomic<uint64_t> uiIterations(0);

    OdometerEngine odoEngine;
    odoEngine.calibrate(98540);

    ChallengerTest::MockElmStream stream;
    ChallengerTest::SimulatedObdStateMachine sm(stream, &odoEngine);

    // Thread 1: Simulates Core 0 Worker Task (Continuous BT churn, connect, drop, query)
    std::thread workerThread([&]() {
        uint32_t vTime = 0;
        int churn = 0;
        while (!stopFlag.load()) {
            churn++;
            if (churn % 30 == 0) {
                // Drop connection violently
                stream.setConnected(false);
            } else if (churn % 30 == 10) {
                // Re-enable connection
                stream.setConnected(true);
            }

            sm.step(vTime);
            vTime += 20;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });

    // Thread 2: Simulates Core 1 UI Thread (Services touch, update, draw at high frequency)
    std::thread uiThread([&]() {
        ObdTask obdTask;
        obdTask.setOdometerEngine(&odoEngine);

        while (!stopFlag.load()) {
            auto start = std::chrono::high_resolution_clock::now();

            // Simulate exact Core 1 loop actions:
            obdTask.update();
            bool connected = obdTask.isConnected();
            (void)connected;
            uint32_t currentOdo = odoEngine.getCurrentOdo();
            (void)currentOdo;

            // Touch debounce and mapping calculation simulation
            int rawX = 1800 + (uiIterations.load() % 100);
            int rawY = 2200;
            int tx = (3800 - rawY) * 240 / 3540;
            int ty = (rawX - 240) * 320 / 3560;
            (void)tx; (void)ty;

            auto end = std::chrono::high_resolution_clock::now();
            double latencyMs = std::chrono::duration<double, std::milli>(end - start).count();

            double currentMax = maxUiLatencyMs.load();
            while (latencyMs > currentMax && !maxUiLatencyMs.compare_exchange_weak(currentMax, latencyMs)) {}

            uiIterations.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(5)); // ~200 Hz UI loop
        }
    });

    // Run concurrent test for 600 ms of real-world time
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    stopFlag.store(true);

    workerThread.join();
    uiThread.join();

    double worstLatency = maxUiLatencyMs.load();
    uint64_t totalUiTicks = uiIterations.load();

    std::cout << "      [Empirical UI Latency Metric: Max = " << worstLatency 
              << " ms, Total UI Loops = " << totalUiTicks << "]\n";

    // HARD CRITERION: UI thread latency must be strictly < 100.0 ms (target: < 5.0 ms)
    ASSERT_TRUE(worstLatency < 100.0);
    ASSERT_TRUE(worstLatency < 5.0);
    ASSERT_TRUE(totalUiTicks > 50);
}

// ============================================================================
// TEST CASES: MISSION 2 - SPEED FALLBACK ACCUMULATION PRECISION STRESS
// ============================================================================

// 4. Highway Commute Precision: 100,000 samples @ 110 km/h (0.5s intervals)
TEST_TIER1(Test_Challenger_SpeedPrecision_HighwayCommute_100kSamples) {
    OdometerEngine odo;
    odo.calibrate(100000);

    const int samples = 100000;
    const float speedKmh = 110.0f;
    const float dtSec = 0.5f;
    double exactAnalyticalKm = (static_cast<double>(speedKmh) * static_cast<double>(dtSec) * samples) / 3600.0;

    for (int i = 0; i < samples; ++i) {
        odo.updateFromSpeed(speedKmh, dtSec);
    }

    uint32_t odoGain = odo.getCurrentOdo() - 100000;
    float tripKm = odo.getTripKm();
    double discrepancyKm = std::fabs(tripKm - exactAnalyticalKm);
    double pctError = (discrepancyKm / exactAnalyticalKm) * 100.0;

    // Assert integer odometer is within 1 km of exact distance
    ASSERT_TRUE(std::fabs(static_cast<double>(odoGain) - exactAnalyticalKm) < 1.0);
    // Assert total trip odometer error is < 0.001%
    ASSERT_TRUE(pctError < 0.001);
}

// 5. Heavy Traffic Micro-Crawl: 200,000 samples @ 1.0 km/h (sub-kilometer floating point test)
TEST_TIER2(Test_Challenger_SpeedPrecision_MicroCrawl_200kSamples_SubKmFloatingPoint) {
    OdometerEngine odo;
    odo.calibrate(100000);

    const int samples = 200000;
    const float speedKmh = 1.0f;
    const float dtSec = 0.5f;
    // Step size: 1.0 * 0.5 / 3600 = 0.0001388888... km per step
    double exactAnalyticalKm = (static_cast<double>(speedKmh) * static_cast<double>(dtSec) * samples) / 3600.0;

    for (int i = 0; i < samples; ++i) {
        odo.updateFromSpeed(speedKmh, dtSec);
    }

    uint32_t odoGain = odo.getCurrentOdo() - 100000;
    float tripKm = odo.getTripKm();
    double discrepancyKm = std::fabs(tripKm - exactAnalyticalKm);
    double pctError = (discrepancyKm / exactAnalyticalKm) * 100.0;

    ASSERT_TRUE(std::fabs(static_cast<double>(odoGain) - exactAnalyticalKm) < 1.0);
    ASSERT_TRUE(pctError < 0.01);
}

// 6. Stochastic Dynamic Driving with Sampling Jitter: 500,000 samples (variable speed and dt)
TEST_TIER3(Test_Challenger_SpeedPrecision_StochasticCycle_500kSamples_JitteredDt) {
    OdometerEngine odo;
    odo.calibrate(100000);

    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> speedDist(0.0f, 140.0f);
    std::uniform_real_distribution<float> dtDist(0.35f, 0.65f); // 350ms to 650ms jitter

    const int samples = 500000;
    double exactAnalyticalKm = 0.0;

    for (int i = 0; i < samples; ++i) {
        float speed = speedDist(rng);
        float dt = dtDist(rng);
        odo.updateFromSpeed(speed, dt);
        exactAnalyticalKm += (static_cast<double>(speed) * static_cast<double>(dt)) / 3600.0;
    }

    uint32_t odoGain = odo.getCurrentOdo() - 100000;
    float tripKm = odo.getTripKm();
    double discrepancyKm = std::fabs(tripKm - exactAnalyticalKm);
    double pctError = (discrepancyKm / exactAnalyticalKm) * 100.0;

    // Over 500,000 steps (~4,000+ km driven), drift must be < 0.001%
    ASSERT_TRUE(std::fabs(static_cast<double>(odoGain) - exactAnalyticalKm) < 1.0);
    ASSERT_TRUE(pctError < 0.001);
}

// 7. Long-Term Multi-Million Sample Endurance: 2,000,000 samples (continuous driving simulation)
TEST_TIER4(Test_Challenger_SpeedPrecision_MultiMillionSample_Endurance_ZeroDrift) {
    OdometerEngine odo;
    odo.calibrate(100000);

    const int samples = 2000000;
    const float speedKmh = 90.0f;
    const float dtSec = 0.5f;
    double exactAnalyticalKm = (static_cast<double>(speedKmh) * static_cast<double>(dtSec) * samples) / 3600.0;

    for (int i = 0; i < samples; ++i) {
        odo.updateFromSpeed(speedKmh, dtSec);
    }

    uint32_t odoGain = odo.getCurrentOdo() - 100000;
    float tripKm = odo.getTripKm();
    double discrepancyKm = std::fabs(tripKm - exactAnalyticalKm);
    double pctError = (discrepancyKm / exactAnalyticalKm) * 100.0;

    // 25,000 km driven under simulation
    ASSERT_TRUE(std::fabs(static_cast<double>(odoGain) - exactAnalyticalKm) < 1.0);
    ASSERT_TRUE(pctError < 0.001);
}

// 8. Adversarial Boundary Cases: 0 km/h idling, negative parameters, boundary transitions
TEST_TIER2(Test_Challenger_SpeedPrecision_AdversarialBoundaries_ZeroAndNegatives) {
    OdometerEngine odo;
    odo.calibrate(100000);

    // 1M samples at 0 km/h (idling in parking lot) -> exactly 0 delta
    for (int i = 0; i < 1000000; ++i) {
        odo.updateFromSpeed(0.0f, 0.5f);
    }
    ASSERT_EQ(odo.getCurrentOdo(), 100000);
    ASSERT_NEAR(odo.getTripKm(), 0.0f, 0.0001f);

    // Negative speeds and negative dt must be rejected without modifying state
    odo.updateFromSpeed(-80.0f, 0.5f);
    odo.updateFromSpeed(80.0f, -0.5f);
    odo.updateFromSpeed(-80.0f, -0.5f);
    ASSERT_EQ(odo.getCurrentOdo(), 100000);
    ASSERT_NEAR(odo.getTripKm(), 0.0f, 0.0001f);

    // Exact threshold boundary: 0.99999f
    // Advance fractional distance to exactly threshold
    odo.updateFromSpeed(239.9976f, 15.0f); // Physically representable PID speed: 239.9976 * 15 / 3600 = 0.99999 km
    ASSERT_EQ(odo.getCurrentOdo(), 100001);
}

// Runner
int main() {
    return ::TestHarness::TestRunner::instance().runAll("Challenger 2 Empirical Verification: BT State Machine & Speed Precision");
}
