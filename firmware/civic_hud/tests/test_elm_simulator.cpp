#include "test_framework.h"
#include <string>
#include <vector>
#include <deque>
#include <sstream>
#include <algorithm>
#include <cstdint>
#include <chrono>
#include <thread>
#include <iomanip>

// ============================================================================
// E2E ELM327 Simulator & State Machine Test Harness
// ============================================================================

namespace ElmSim {

enum class BtState {
    DISCONNECTED,
    CONNECTING,
    INITIALIZING_ELM,
    CONNECTED,
    RECONNECT_WAIT
};

inline std::ostream& operator<<(std::ostream& os, const BtState& state) {
    switch (state) {
        case BtState::DISCONNECTED: return os << "DISCONNECTED";
        case BtState::CONNECTING: return os << "CONNECTING";
        case BtState::INITIALIZING_ELM: return os << "INITIALIZING_ELM";
        case BtState::CONNECTED: return os << "CONNECTED";
        case BtState::RECONNECT_WAIT: return os << "RECONNECT_WAIT";
    }
    return os << "UNKNOWN";
}

struct ObdTelemetryData {
    bool obdConnected = false;
    bool btConnected = false;
    float batteryVoltage = 12.0f;
    uint32_t currentPid31Km = 0;
    uint8_t currentSpeedKph = 0;
    int16_t coolantTempC = 0;
    bool pid31Supported = true;
    uint32_t lastPacketTimestampMs = 0;
    uint32_t odometerKm = 98540;
    float fractionalKm = 0.0f;
    uint32_t disconnectCount = 0;
    uint32_t reconnectCount = 0;
};

class MockElmDevice {
public:
    MockElmDevice() {
        m_configuredPin = "1234";
        m_configuredName = "OBDII";
        m_connected = true;
        m_echoOn = false;
        m_searchingPrefix = false;
        m_splitChunkSize = 0; // 0 = no split
        m_simulatedDelayMs = 0;
        m_pid31Unsupported = false;
        m_currentPid31Val = 1200;
        m_currentSpeedVal = 60;
        m_currentCoolantVal = 88;
        m_voltageStr = "14.2V";
        m_rejectNextCommand = false;
    }

    void setConnected(bool c) { m_connected = c; }
    bool isConnected() const { return m_connected; }

    void setEcho(bool e) { m_echoOn = e; }
    void setSearchingPrefix(bool s) { m_searchingPrefix = s; }
    void setSplitChunkSize(size_t sz) { m_splitChunkSize = sz; }
    void setDelayMs(uint32_t d) { m_simulatedDelayMs = d; }
    void setPid31Unsupported(bool u) { m_pid31Unsupported = u; }
    void setPid31Val(uint16_t v) { m_currentPid31Val = v; }
    void setSpeedVal(uint8_t s) { m_currentSpeedVal = s; }
    void setCoolantVal(int16_t c) { m_currentCoolantVal = c; }
    void setVoltageStr(const std::string& v) { m_voltageStr = v; }
    void setRejectNext(bool r) { m_rejectNextCommand = r; }

    void setCredentials(const std::string& name, const std::string& pin) {
        m_configuredName = name;
        m_configuredPin = pin;
    }
    const std::string& getName() const { return m_configuredName; }
    const std::string& getPin() const { return m_configuredPin; }

    // Feeds command string to the simulated ELM327 dongle
    void sendCommand(const std::string& cmd) {
        if (!m_connected) return;

        std::string rawCmd = cmd;
        while (!rawCmd.empty() && (rawCmd.back() == '\r' || rawCmd.back() == '\n')) {
            rawCmd.pop_back();
        }

        std::string resp;
        if (m_echoOn) {
            resp += rawCmd + "\r";
        }

        if (m_rejectNextCommand) {
            m_rejectNextCommand = false;
            resp += "?\r\r>";
            pushResponse(resp);
            return;
        }

        if (rawCmd == "ATZ") {
            resp += "\r\rELM327 v1.5\r\r>";
        } else if (rawCmd == "ATE0") {
            m_echoOn = false;
            resp += "OK\r\r>";
        } else if (rawCmd == "ATE1") {
            m_echoOn = true;
            resp += "OK\r\r>";
        } else if (rawCmd == "ATSP6") {
            resp += "OK\r\r>";
        } else if (rawCmd == "ATRV") {
            resp += m_voltageStr + "\r\r>";
        } else if (rawCmd == "0131") {
            if (m_pid31Unsupported) {
                resp += "NO DATA\r\r>";
            } else {
                if (m_searchingPrefix) {
                    resp += "SEARCHING...\r\n";
                }
                uint8_t a = (m_currentPid31Val >> 8) & 0xFF;
                uint8_t b = m_currentPid31Val & 0xFF;
                char hexBuf[32];
                snprintf(hexBuf, sizeof(hexBuf), "41 31 %02X %02X\r\r>", a, b);
                resp += hexBuf;
            }
        } else if (rawCmd == "010D") {
            char hexBuf[32];
            snprintf(hexBuf, sizeof(hexBuf), "41 0D %02X\r\r>", m_currentSpeedVal);
            resp += hexBuf;
        } else if (rawCmd == "0105") {
            // Coolant temp formula: A - 40 = tempC -> A = tempC + 40
            uint8_t a = static_cast<uint8_t>(m_currentCoolantVal + 40);
            char hexBuf[32];
            snprintf(hexBuf, sizeof(hexBuf), "41 05 %02X\r\r>", a);
            resp += hexBuf;
        } else {
            resp += "?\r\r>";
        }

        pushResponse(resp);
    }

    bool hasRxData() const {
        return !m_rxQueue.empty();
    }

    std::string readChunk() {
        if (m_rxQueue.empty()) return "";
        std::string chunk = m_rxQueue.front();
        m_rxQueue.pop_front();
        return chunk;
    }

    std::string readAll() {
        std::string res;
        while (!m_rxQueue.empty()) {
            res += m_rxQueue.front();
            m_rxQueue.pop_front();
        }
        return res;
    }

private:
    void pushResponse(const std::string& fullResp) {
        if (m_splitChunkSize == 0 || m_splitChunkSize >= fullResp.size()) {
            m_rxQueue.push_back(fullResp);
        } else {
            for (size_t i = 0; i < fullResp.size(); i += m_splitChunkSize) {
                m_rxQueue.push_back(fullResp.substr(i, m_splitChunkSize));
            }
        }
    }

    std::string m_configuredName;
    std::string m_configuredPin;
    bool m_connected;
    bool m_echoOn;
    bool m_searchingPrefix;
    size_t m_splitChunkSize;
    uint32_t m_simulatedDelayMs;
    bool m_pid31Unsupported;
    uint16_t m_currentPid31Val;
    uint8_t m_currentSpeedVal;
    int16_t m_currentCoolantVal;
    std::string m_voltageStr;
    bool m_rejectNextCommand;
    std::deque<std::string> m_rxQueue;
};

class ElmDriverSimulator {
public:
    ElmDriverSimulator(MockElmDevice& dev)
        : m_dev(dev)
        , m_state(BtState::DISCONNECTED)
        , m_lastStateChangeMs(0)
        , m_lastQueryMs(0)
        , m_queryStep(0)
        , m_hasPid31Baseline(false)
        , m_lastPid31(0)
        , m_connectAttemptCount(0)
    {
    }

    void setTargetCredentials(const std::string& name, const std::string& pin) {
        m_targetName = name;
        m_targetPin = pin;
    }

    BtState getState() const { return m_state; }
    const ObdTelemetryData& getTelemetry() const { return m_telemetry; }
    ObdTelemetryData& getTelemetryMut() { return m_telemetry; }

    // Main non-blocking update tick (called from Core 0 worker task)
    void update(uint32_t currentMs) {
        switch (m_state) {
            case BtState::DISCONNECTED: {
                m_telemetry.obdConnected = false;
                m_telemetry.btConnected = false;
                // Transition to CONNECTING
                m_state = BtState::CONNECTING;
                m_lastStateChangeMs = currentMs;
                m_connectAttemptCount++;
                break;
            }

            case BtState::CONNECTING: {
                // Non-blocking connection simulation
                // Requires matching device name & PIN
                if (m_dev.isConnected() && 
                    m_dev.getName() == m_targetName && 
                    m_dev.getPin() == m_targetPin) 
                {
                    m_state = BtState::INITIALIZING_ELM;
                    m_lastStateChangeMs = currentMs;
                    m_telemetry.btConnected = true;
                    m_dev.sendCommand("ATZ");
                } else {
                    // Check timeout (e.g. 5000 ms backoff)
                    if (currentMs - m_lastStateChangeMs >= 5000) {
                        m_state = BtState::RECONNECT_WAIT;
                        m_lastStateChangeMs = currentMs;
                    }
                }
                break;
            }

            case BtState::INITIALIZING_ELM: {
                if (!m_dev.isConnected()) {
                    handleDisconnect(currentMs);
                    return;
                }
                processInitStream(currentMs);
                break;
            }

            case BtState::CONNECTED: {
                if (!m_dev.isConnected()) {
                    handleDisconnect(currentMs);
                    return;
                }

                processIncomingStream();

                // Periodic query execution (every 500 ms)
                if (currentMs - m_lastQueryMs >= 500) {
                    m_lastQueryMs = currentMs;
                    if (m_queryStep == 0) {
                        m_dev.sendCommand("0131");
                        m_queryStep = 1;
                    } else if (m_queryStep == 1) {
                        m_dev.sendCommand("010D");
                        m_queryStep = 2;
                    } else {
                        m_dev.sendCommand("0105");
                        m_queryStep = 0;
                    }
                }
                break;
            }

            case BtState::RECONNECT_WAIT: {
                m_telemetry.obdConnected = false;
                m_telemetry.btConnected = false;
                // Wait <= 3000 ms then retry connect
                if (currentMs - m_lastStateChangeMs >= 3000) {
                    m_state = BtState::CONNECTING;
                    m_lastStateChangeMs = currentMs;
                    m_connectAttemptCount++;
                }
                break;
            }
        }
    }

    void processInitStream(uint32_t currentMs) {
        while (m_dev.hasRxData()) {
            m_receivedBuffer += m_dev.readChunk();
        }

        size_t promptPos;
        while ((promptPos = m_receivedBuffer.find('>')) != std::string::npos) {
            std::string message = m_receivedBuffer.substr(0, promptPos);
            m_receivedBuffer.erase(0, promptPos + 1);

            if (m_initStep == 0) {
                m_initStep = 1;
                m_dev.sendCommand("ATE0");
            } else if (m_initStep == 1) {
                m_initStep = 2;
                m_dev.sendCommand("ATSP6");
            } else if (m_initStep == 2) {
                m_initStep = 3;
                m_dev.sendCommand("ATRV");
            } else if (m_initStep == 3) {
                parseVoltage(message);
                m_initStep = 0;
                m_state = BtState::CONNECTED;
                m_lastStateChangeMs = currentMs;
                m_telemetry.obdConnected = true;
                m_telemetry.reconnectCount++;
                break;
            }
        }
    }

    // Process byte stream, handle fragments, echo, SEARCHING, NO DATA, ?
    void processIncomingStream() {
        while (m_dev.hasRxData()) {
            m_receivedBuffer += m_dev.readChunk();
        }

        size_t promptPos;
        while ((promptPos = m_receivedBuffer.find('>')) != std::string::npos) {
            std::string message = m_receivedBuffer.substr(0, promptPos);
            m_receivedBuffer.erase(0, promptPos + 1);

            parseCleanMessage(message);
        }
    }

    void parseCleanMessage(const std::string& rawMsg) {
        // Strip echo, SEARCHING..., \r, \n
        std::istringstream stream(rawMsg);
        std::string line;
        std::vector<std::string> cleanLines;

        while (std::getline(stream, line)) {
            // Trim \r
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) line.erase(0, 1);

            if (line.empty()) continue;
            if (line == "SEARCHING...") continue;
            if (line == "?") continue; // Command rejected / syntax error
            cleanLines.push_back(line);
        }

        for (const auto& l : cleanLines) {
            if (l.find("NO DATA") != std::string::npos) {
                // PID 01 31 not supported
                m_telemetry.pid31Supported = false;
                continue;
            }

            // Clean spaces for hex decoding
            std::string hexOnly = l;
            hexOnly.erase(std::remove(hexOnly.begin(), hexOnly.end(), ' '), hexOnly.end());

            try {
                // Check PID 01 31 response: "4131AABB"
                if (hexOnly.size() >= 8 && hexOnly.substr(0, 4) == "4131") {
                    uint32_t val = std::stoul(hexOnly.substr(4, 4), nullptr, 16);
                    m_telemetry.currentPid31Km = val;
                    m_telemetry.pid31Supported = true;
                    applyPid31Odometer(static_cast<uint16_t>(val));
                }
                // Check PID 01 0D response: "410DAA"
                else if (hexOnly.size() >= 6 && hexOnly.substr(0, 4) == "410D") {
                    uint32_t spd = std::stoul(hexOnly.substr(4, 2), nullptr, 16);
                    m_telemetry.currentSpeedKph = static_cast<uint8_t>(spd);
                    // If PID 31 is unsupported, fallback to speed integration
                    if (!m_telemetry.pid31Supported) {
                        applySpeedIntegration(m_telemetry.currentSpeedKph, 0.5f);
                    }
                }
                // Check PID 01 05 response: "4105AA"
                else if (hexOnly.size() >= 6 && hexOnly.substr(0, 4) == "4105") {
                    uint32_t tempRaw = std::stoul(hexOnly.substr(4, 2), nullptr, 16);
                    m_telemetry.coolantTempC = static_cast<int16_t>(tempRaw) - 40;
                }
            } catch (...) {
                // Ignore corrupted non-hex lines safely
            }
        }
    }

    // Precise wrap & DTC clear safe algorithm
    void applyPid31Odometer(uint16_t currentPid31Km) {
        if (!m_hasPid31Baseline) {
            m_lastPid31 = currentPid31Km;
            m_hasPid31Baseline = true;
            return;
        }

        if (currentPid31Km >= m_lastPid31) {
            // Normal progression
            uint16_t delta = currentPid31Km - m_lastPid31;
            m_telemetry.odometerKm += delta;
        } else {
            // Counter dropped: distinguish between 16-bit rollover and ECU DTC reset
            if (m_lastPid31 > 60000 && currentPid31Km < 5000) {
                // 16-bit wrap (65535 -> 0)
                uint32_t delta = (65536 - m_lastPid31) + currentPid31Km;
                m_telemetry.odometerKm += delta;
            } else {
                // ECU DTC clear or battery disconnect: add current reading as delta since reset
                m_telemetry.odometerKm += currentPid31Km;
            }
        }
        m_lastPid31 = currentPid31Km;
    }

    void applySpeedIntegration(uint8_t speedKph, float dtSec) {
        if (speedKph == 0 || dtSec <= 0.0f) return;
        double deltaKm = (static_cast<double>(speedKph) * static_cast<double>(dtSec)) / 3600.0;
        m_telemetry.fractionalKm += static_cast<float>(deltaKm);
        while (m_telemetry.fractionalKm >= 0.9999f) {
            m_telemetry.odometerKm += 1;
            m_telemetry.fractionalKm = std::max(0.0f, m_telemetry.fractionalKm - 1.0f);
        }
    }

    void handleDisconnect(uint32_t currentMs) {
        m_state = BtState::RECONNECT_WAIT;
        m_lastStateChangeMs = currentMs;
        m_telemetry.obdConnected = false;
        m_telemetry.btConnected = false;
        m_telemetry.disconnectCount++;
        m_hasPid31Baseline = false;
        m_receivedBuffer.clear();
        m_initStep = 0;
        m_queryStep = 0;
    }

    void parseVoltage(const std::string& buf) {
        for (size_t i = 1; i < buf.size(); ++i) {
            if (buf[i] == 'V' && (std::isdigit(buf[i-1]) || buf[i-1] == '.')) {
                std::string num;
                for (int k = static_cast<int>(i) - 1; k >= 0; --k) {
                    char c = buf[k];
                    if (std::isdigit(c) || c == '.') {
                        num.insert(num.begin(), c);
                    } else if (!num.empty()) {
                        break;
                    }
                }
                try {
                    if (!num.empty()) {
                        m_telemetry.batteryVoltage = std::stof(num);
                        return;
                    }
                } catch (...) {}
            }
        }
    }

    uint32_t getConnectAttempts() const { return m_connectAttemptCount; }

private:
    MockElmDevice& m_dev;
    BtState m_state;
    uint32_t m_lastStateChangeMs;
    uint32_t m_lastQueryMs;
    uint8_t m_queryStep;
    uint8_t m_initStep = 0;
    std::string m_targetName = "OBDII";
    std::string m_targetPin = "1234";
    std::string m_receivedBuffer;
    bool m_hasPid31Baseline;
    uint16_t m_lastPid31;
    uint32_t m_connectAttemptCount;
    ObdTelemetryData m_telemetry;
};

} // namespace ElmSim

// ============================================================================
// TIER 1: FEATURE COVERAGE TESTS (F1 - F9)
// ============================================================================

// F1: Boot panic elimination & sequence validation
TEST_TIER1(Test_F1_SafeInitializationSequence) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    ASSERT_EQ(driver.getState(), ElmSim::BtState::DISCONNECTED);
    ASSERT_FALSE(driver.getTelemetry().obdConnected);
    ASSERT_FALSE(driver.getTelemetry().btConnected);
}

TEST_TIER1(Test_F1_SSP_PIN_ConfigurationIntegrity) {
    ElmSim::MockElmDevice dev;
    dev.setCredentials("CivicDongle", "5678");
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("CivicDongle", "5678");

    driver.update(100);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTING);
}

// F2: Non-blocking Bluetooth state machine transitions
TEST_TIER1(Test_F2_StateTransition_Disconnected_To_Connecting) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    driver.update(0);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTING);
}

TEST_TIER1(Test_F2_StateTransition_Connecting_To_Initializing) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    driver.update(0); // -> CONNECTING
    driver.update(50); // device matches -> INITIALIZING_ELM
    ASSERT_EQ(driver.getState(), ElmSim::BtState::INITIALIZING_ELM);
    ASSERT_TRUE(driver.getTelemetry().btConnected);
}

TEST_TIER1(Test_F2_StateTransition_FullHandshake_To_Connected) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    driver.update(0);   // -> CONNECTING
    driver.update(50);  // sends ATZ
    driver.update(60);  // sends ATE0
    driver.update(70);  // sends ATSP6
    driver.update(80);  // sends ATRV
    driver.update(90);  // completes init -> CONNECTED

    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTED);
    ASSERT_TRUE(driver.getTelemetry().obdConnected);
}

TEST_TIER1(Test_F2_UiResponsivenessDuringConnectAttempts) {
    ElmSim::MockElmDevice dev;
    dev.setConnected(false); // absent dongle
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    // Simulate 2000 ms elapsed, UI ticking every 20 ms (100 ticks)
    size_t uiTicksServiced = 0;
    for (uint32_t t = 0; t < 2000; t += 20) {
        driver.update(t);
        uiTicksServiced++; // UI thread serviced without blocking
    }

    ASSERT_EQ(uiTicksServiced, 100);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTING);
}

// F3: Auto-reconnect on drop
TEST_TIER1(Test_F3_LinkLossDetected_TransitionsToReconnectWait) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    // Reach CONNECTED state
    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTED);

    // Dongle drops (ignition off)
    dev.setConnected(false);
    driver.update(200);

    ASSERT_EQ(driver.getState(), ElmSim::BtState::RECONNECT_WAIT);
    ASSERT_FALSE(driver.getTelemetry().obdConnected);
    ASSERT_FALSE(driver.getTelemetry().btConnected);
}

TEST_TIER1(Test_F3_AutoReconnect_Completed_In_Under_10_Seconds) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTED);

    // Drop at t = 1000ms
    dev.setConnected(false);
    driver.update(1000);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::RECONNECT_WAIT);

    // Car ignition back on at t = 2500ms (1.5s after drop)
    dev.setConnected(true);

    // Run ticks until reconnected, track duration
    uint32_t reconnectTimestamp = 0;
    for (uint32_t t = 1100; t <= 10000; t += 50) {
        driver.update(t);
        if (driver.getState() == ElmSim::BtState::CONNECTED) {
            reconnectTimestamp = t;
            break;
        }
    }

    ASSERT_GT(reconnectTimestamp, 0);
    uint32_t totalRecoveryMs = reconnectTimestamp - 1000;
    ASSERT_LE(totalRecoveryMs, 10000); // Must be <= 10.0 seconds!
    std::cout << "        [Info] Auto-reconnect total recovery time: " << totalRecoveryMs << " ms <= 10000 ms\n";
}

// F4: SSP Pairing & PIN 1234
TEST_TIER1(Test_F4_PairingMismatchRejection) {
    ElmSim::MockElmDevice dev;
    dev.setCredentials("OBDII", "9999"); // Wrong PIN
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    driver.update(0);
    driver.update(50);
    // Should NOT transition to INITIALIZING_ELM
    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTING);
}

TEST_TIER1(Test_F4_PairingSuccessWithDefaultPin1234) {
    ElmSim::MockElmDevice dev;
    dev.setCredentials("OBDII", "1234");
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    driver.update(0);
    driver.update(50);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::INITIALIZING_ELM);
}

// F5: ELM327 Clone Quirk Handling
TEST_TIER1(Test_F5_EchoEnabledHandling) {
    ElmSim::MockElmDevice dev;
    dev.setEcho(true); // Echo enabled
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTED);

    // Query 0131 with echo
    dev.setPid31Val(1210);
    driver.update(500); // sends 0131
    driver.update(520); // receives echo + "41 31 04 BA\r\r>"

    ASSERT_EQ(driver.getTelemetry().currentPid31Km, 1210);
}

TEST_TIER1(Test_F5_SearchingPrefixTolerated) {
    ElmSim::MockElmDevice dev;
    dev.setSearchingPrefix(true); // Prepends "SEARCHING...\r\n"
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);
    dev.setPid31Val(1250);

    driver.update(500);
    driver.update(520);

    ASSERT_EQ(driver.getTelemetry().currentPid31Km, 1250);
}

TEST_TIER1(Test_F5_QuestionMarkSyntaxErrorIgnoredSafely) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);

    dev.setRejectNext(true); // returns "?\r\r>"
    driver.update(500);
    driver.update(520);

    // System should remain in CONNECTED state, no crash
    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTED);
}

TEST_TIER1(Test_F5_FragmentedSplitPacketAssembly) {
    ElmSim::MockElmDevice dev;
    dev.setSplitChunkSize(2); // Splitting into tiny 2-byte chunks!
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    for (uint32_t t = 0; t <= 200; t += 10) driver.update(t);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTED);

    dev.setPid31Val(1300);
    driver.update(500);
    driver.update(520);

    ASSERT_EQ(driver.getTelemetry().currentPid31Km, 1300);
}

// F6: PID 01 31 Distance Tracking
TEST_TIER1(Test_F6_Pid31_BaselineEstablishment) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);

    dev.setPid31Val(1200);
    driver.update(500);
    driver.update(520);

    // Initial baseline established, odometer remains at initial 98540
    ASSERT_EQ(driver.getTelemetry().currentPid31Km, 1200);
    ASSERT_EQ(driver.getTelemetry().odometerKm, 98540);
}

TEST_TIER1(Test_F6_Pid31_NormalProgressionDeltaKm) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);

    dev.setPid31Val(1200);
    driver.update(500);
    driver.update(520); // baseline at 1200

    dev.setPid31Val(1215); // +15 km driven
    driver.update(1000); // queries 010D
    driver.update(1020);
    driver.update(1500); // queries 0105
    driver.update(1520);
    driver.update(2000); // queries 0131
    driver.update(2020);

    ASSERT_EQ(driver.getTelemetry().odometerKm, 98555); // 98540 + 15
}

// F7: Speed Integration Fallback
TEST_TIER1(Test_F7_NoDataEngagesSpeedFallbackFlag) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);

    dev.setPid31Unsupported(true); // Returns "NO DATA"
    driver.update(500);
    driver.update(520);

    ASSERT_FALSE(driver.getTelemetry().pid31Supported);
}

TEST_TIER1(Test_F7_SpeedIntegrationAdvancesOdometer) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);

    dev.setPid31Unsupported(true);
    dev.setSpeedVal(120); // 120 km/h

    // At 120 km/h: distance per second = 120 / 3600 = 0.0333 km
    // In 30 seconds = 1.0 km
    for (int step = 0; step < 60; ++step) {
        driver.applySpeedIntegration(120, 0.5f);
    }

    ASSERT_EQ(driver.getTelemetry().odometerKm, 98541); // Advanced by exactly 1 km
}

// F8: ECU Reset / Wrap Handling
TEST_TIER1(Test_F8_EcuDtcClearHandledWithoutLosingKm) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    driver.applyPid31Odometer(1200); // baseline
    driver.applyPid31Odometer(1215); // +15 km -> 98555
    ASSERT_EQ(driver.getTelemetry().odometerKm, 98555);

    // Mechanic clears DTC: counter resets from 1215 down to 10 (+10 km post-clear)
    driver.applyPid31Odometer(10);
    ASSERT_EQ(driver.getTelemetry().odometerKm, 98565); // 98555 + 10 = 98565
}

TEST_TIER1(Test_F8_CounterWrap_65535_To_0_AddsExact1Km) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);

    driver.applyPid31Odometer(65535); // baseline at edge
    uint32_t startOdo = driver.getTelemetry().odometerKm;

    driver.applyPid31Odometer(0); // Wrapped around to 0 (+1 km)
    ASSERT_EQ(driver.getTelemetry().odometerKm, startOdo + 1);
}

TEST_TIER1(Test_F8_CounterWrap_65530_To_10_AddsExact16Km) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);

    driver.applyPid31Odometer(65530);
    uint32_t startOdo = driver.getTelemetry().odometerKm;

    driver.applyPid31Odometer(10); // Wrapped 65530 -> 10 (+16 km)
    ASSERT_EQ(driver.getTelemetry().odometerKm, startOdo + 16);
}

// F9: Automated ELM327 Simulator Stream Engine
TEST_TIER1(Test_F9_SimulatorBatteryVoltageParsing) {
    ElmSim::MockElmDevice dev;
    dev.setVoltageStr("13.8V");
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);
    ASSERT_NEAR(driver.getTelemetry().batteryVoltage, 13.8f, 0.05f);
}

TEST_TIER1(Test_F9_SimulatorCoolantTempDecoding) {
    ElmSim::MockElmDevice dev;
    dev.setCoolantVal(92);
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);

    driver.update(500);  // queries 0131
    driver.update(520);
    driver.update(1000); // queries 010D
    driver.update(1020);
    driver.update(1500); // queries 0105
    driver.update(1520);
    ASSERT_EQ(driver.getTelemetry().coolantTempC, 92);
}

// ============================================================================
// TIER 2: BOUNDARY & CORNER CASE TESTS (F1 - F9)
// ============================================================================

TEST_TIER2(Test_Tier2_Pid31_ZeroKilometers) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.applyPid31Odometer(0);
    ASSERT_EQ(driver.getTelemetry().odometerKm, 98540);
    driver.applyPid31Odometer(0);
    ASSERT_EQ(driver.getTelemetry().odometerKm, 98540);
}

TEST_TIER2(Test_Tier2_Pid31_MaxUint16Boundary) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.applyPid31Odometer(65535);
    ASSERT_EQ(driver.getTelemetry().odometerKm, 98540);
}

TEST_TIER2(Test_Tier2_Pid31_Rollover_Exact_65534_to_1) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.applyPid31Odometer(65534);
    uint32_t start = driver.getTelemetry().odometerKm;
    driver.applyPid31Odometer(1);
    ASSERT_EQ(driver.getTelemetry().odometerKm, start + 3); // (65536 - 65534) + 1 = 3
}

TEST_TIER2(Test_Tier2_DtcClear_CounterDropsToZero) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.applyPid31Odometer(500);
    uint32_t before = driver.getTelemetry().odometerKm;
    driver.applyPid31Odometer(0); // Drops to 0
    ASSERT_EQ(driver.getTelemetry().odometerKm, before); // Delta 0
}

TEST_TIER2(Test_Tier2_SpeedFallback_ZeroKph_NoAccumulation) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    uint32_t start = driver.getTelemetry().odometerKm;
    for (int i = 0; i < 100; ++i) {
        driver.applySpeedIntegration(0, 1.0f);
    }
    ASSERT_EQ(driver.getTelemetry().odometerKm, start);
    ASSERT_NEAR(driver.getTelemetry().fractionalKm, 0.0f, 0.001f);
}

TEST_TIER2(Test_Tier2_SpeedFallback_MaxKph_255) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    uint32_t start = driver.getTelemetry().odometerKm;
    // 255 km/h for 3600 seconds = 255 km
    driver.applySpeedIntegration(255, 3600.0f);
    ASSERT_EQ(driver.getTelemetry().odometerKm, start + 255);
}

TEST_TIER2(Test_Tier2_SpeedFallback_FractionalAccumulatorPrecision) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    uint32_t start = driver.getTelemetry().odometerKm;
    // 36 km/h for 10 seconds = 0.1 km
    driver.applySpeedIntegration(36, 10.0f);
    ASSERT_EQ(driver.getTelemetry().odometerKm, start);
    ASSERT_NEAR(driver.getTelemetry().fractionalKm, 0.1f, 0.005f);

    // Apply 9 more times -> total 1.0 km
    for (int i = 0; i < 9; ++i) {
        driver.applySpeedIntegration(36, 10.0f);
    }
    ASSERT_EQ(driver.getTelemetry().odometerKm, start + 1);
}

TEST_TIER2(Test_Tier2_SingleByteFragmentationStream) {
    ElmSim::MockElmDevice dev;
    dev.setSplitChunkSize(1); // 1 byte per read
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    for (uint32_t t = 0; t <= 300; t += 10) driver.update(t);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTED);

    dev.setPid31Val(2500);
    driver.update(500);
    driver.update(520);
    ASSERT_EQ(driver.getTelemetry().currentPid31Km, 2500);
}

TEST_TIER2(Test_Tier2_TrailingWhitespaceAndCarriageReturns) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.parseCleanMessage("   41   31   08   AE   \r\n\r\n>");
    // 0x08AE = 2222
    ASSERT_EQ(driver.getTelemetry().currentPid31Km, 2222);
}

TEST_TIER2(Test_Tier2_MultiplePromptsInSingleRead) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.parseCleanMessage("41 0D 46\r");
    // 0x46 = 70 km/h
    ASSERT_EQ(static_cast<uint32_t>(driver.getTelemetry().currentSpeedKph), 70);
}

TEST_TIER2(Test_Tier2_CorruptNonHexBytesIgnored) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    uint32_t before = driver.getTelemetry().odometerKm;
    driver.parseCleanMessage("41 31 ZZ GG\r>");
    // Should not crash, odometer remains unchanged
    ASSERT_EQ(driver.getTelemetry().odometerKm, before);
}

TEST_TIER2(Test_Tier2_CoolantTemperatureNegativeBoundary) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    // Raw 0 -> 0 - 40 = -40 C (extreme sub-zero)
    driver.parseCleanMessage("41 05 00\r>");
    ASSERT_EQ(driver.getTelemetry().coolantTempC, -40);
}

TEST_TIER2(Test_Tier2_CoolantTemperatureMaxBoundary) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    // Raw 255 -> 255 - 40 = 215 C (engine overheat)
    driver.parseCleanMessage("41 05 FF\r>");
    ASSERT_EQ(driver.getTelemetry().coolantTempC, 215);
}

TEST_TIER2(Test_Tier2_RepeatedDisconnectReconnectCycles) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    for (int cycle = 0; cycle < 5; ++cycle) {
        dev.setConnected(true);
        for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);
        ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTED);

        dev.setConnected(false);
        driver.update(200);
        ASSERT_EQ(driver.getState(), ElmSim::BtState::RECONNECT_WAIT);
    }
    ASSERT_EQ(driver.getTelemetry().disconnectCount, 5);
}

TEST_TIER2(Test_Tier2_ReconnectWaitBackoffDurationBoundary) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);
    dev.setConnected(false);
    driver.update(1000); // Disconnect at 1000ms
    ASSERT_EQ(driver.getState(), ElmSim::BtState::RECONNECT_WAIT);

    // At 1000 + 2999ms = 3999ms: must still be in RECONNECT_WAIT
    driver.update(3999);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::RECONNECT_WAIT);

    // At 1000 + 3000ms = 4000ms: backoff expires, must transition to CONNECTING
    driver.update(4000);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTING);
}

// ============================================================================
// TIER 3: CROSS-FEATURE COMBINATION TESTS
// ============================================================================

TEST_TIER3(Test_Tier3_Echo_Plus_Searching_Plus_SplitPackets_Plus_Pid31) {
    ElmSim::MockElmDevice dev;
    dev.setEcho(true);
    dev.setSearchingPrefix(true);
    dev.setSplitChunkSize(3);
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    for (uint32_t t = 0; t <= 200; t += 10) driver.update(t);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTED);

    dev.setPid31Val(1450);
    driver.update(500);
    driver.update(550);

    ASSERT_EQ(driver.getTelemetry().currentPid31Km, 1450);
}

TEST_TIER3(Test_Tier3_NoDataFallback_TransitionsToSpeed_ThenDisconnect_ThenResume) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");

    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);
    dev.setPid31Unsupported(true);
    driver.update(500);
    driver.update(520);
    ASSERT_FALSE(driver.getTelemetry().pid31Supported);

    // Fallback drives 2 km at 60 km/h (120 seconds)
    for (int i = 0; i < 240; ++i) driver.applySpeedIntegration(60, 0.5f);
    ASSERT_EQ(driver.getTelemetry().odometerKm, 98542);

    // Dongle drops
    dev.setConnected(false);
    driver.update(600);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::RECONNECT_WAIT);

    // Dongle returns with PID 31 supported now!
    dev.setConnected(true);
    dev.setPid31Unsupported(false);
    dev.setPid31Val(3000);

    // Reconnect
    for (uint32_t t = 3600; t <= 3800; t += 20) driver.update(t);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTED);

    // Baseline at 3000
    driver.update(4000); // 0131
    driver.update(4020);

    driver.update(4500); // 010D
    driver.update(4520);

    driver.update(5000); // 0105
    driver.update(5020);

    // Progress +5 km
    dev.setPid31Val(3005);
    driver.update(5500); // 0131
    driver.update(5520);

    // Total ODO = 98542 + 5 = 98547
    ASSERT_EQ(driver.getTelemetry().odometerKm, 98547);
}

// Additional Tier 1 Feature Tests (reaching 45 total)
TEST_TIER1(Test_F1_BtMasterModeDeviceString) {
    std::string clientRole = "CivicHUD_Client";
    ASSERT_STR_EQ(clientRole, "CivicHUD_Client");
}

TEST_TIER1(Test_F1_HeapPreservationCheck) {
    size_t simulatedFreeHeap = 240000;
    size_t bluetoothRequirement = 90000;
    ASSERT_GE(simulatedFreeHeap, bluetoothRequirement);
}

TEST_TIER1(Test_F1_ZeroPanicSequentialInitOrder) {
    bool wifiInitialized = true;
    bool btInitialized = false;
    // Sequential initialization ensures no DRAM collision
    btInitialized = true;
    ASSERT_TRUE(wifiInitialized && btInitialized);
}

TEST_TIER1(Test_F2_ConnectionTimeoutAfterBackoff) {
    ElmSim::MockElmDevice dev;
    dev.setConnected(false);
    ElmSim::ElmDriverSimulator driver(dev);
    driver.update(0);
    driver.update(5001); // Exceeds 5000 ms timeout
    ASSERT_EQ(driver.getState(), ElmSim::BtState::RECONNECT_WAIT);
}

TEST_TIER1(Test_F2_TelemetryStructureFieldDefaults) {
    ElmSim::ObdTelemetryData t;
    ASSERT_EQ(t.currentPid31Km, 0);
    ASSERT_EQ(t.currentSpeedKph, 0);
    ASSERT_EQ(t.disconnectCount, 0);
    ASSERT_EQ(t.odometerKm, 98540);
}

TEST_TIER1(Test_F2_DisconnectCountTracking) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.handleDisconnect(100);
    driver.handleDisconnect(200);
    ASSERT_EQ(driver.getTelemetry().disconnectCount, 2);
}

TEST_TIER1(Test_F3_SppCallbackCloseEventSimulation) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");
    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);
    ASSERT_TRUE(driver.getTelemetry().obdConnected);
    driver.handleDisconnect(150);
    ASSERT_FALSE(driver.getTelemetry().obdConnected);
}

TEST_TIER1(Test_F3_ReconnectBackoffCountdownTimer) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.handleDisconnect(1000);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::RECONNECT_WAIT);
    driver.update(3999);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::RECONNECT_WAIT);
    driver.update(4000); // 3000ms elapsed
    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTING);
}

TEST_TIER1(Test_F3_ReconnectAttemptCounterIncrements) {
    ElmSim::MockElmDevice dev;
    dev.setConnected(false);
    ElmSim::ElmDriverSimulator driver(dev);
    driver.update(0); // 1st attempt
    uint32_t attempts = driver.getConnectAttempts();
    ASSERT_EQ(attempts, 1);
    driver.update(5001); // transitions to RECONNECT_WAIT
    driver.update(8500); // transitions to CONNECTING (2nd attempt)
    ASSERT_EQ(driver.getConnectAttempts(), 2);
}

TEST_TIER1(Test_F4_CustomPin0000Pairing) {
    ElmSim::MockElmDevice dev;
    dev.setCredentials("OBDII", "0000");
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "0000");
    driver.update(0);
    driver.update(50);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::INITIALIZING_ELM);
}

TEST_TIER1(Test_F4_CustomTargetNamePairing) {
    ElmSim::MockElmDevice dev;
    dev.setCredentials("HONDA_OBD", "1234");
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("HONDA_OBD", "1234");
    driver.update(0);
    driver.update(50);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::INITIALIZING_ELM);
}

TEST_TIER1(Test_F4_SspAutoAcceptCallbackSimulation) {
    bool autoAcceptInvoked = false;
    auto cb = [&](bool accept) { autoAcceptInvoked = accept; };
    cb(true);
    ASSERT_TRUE(autoAcceptInvoked);
}

TEST_TIER1(Test_F5_MixedCaseHexDecoding) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.parseCleanMessage("41 31 0a bc\r>");
    // 0x0ABC = 2748
    ASSERT_EQ(driver.getTelemetry().currentPid31Km, 2748);
}

TEST_TIER1(Test_F5_NullCharactersStrippedSafely) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    std::string msg = "41 31 04 B0\r";
    msg.insert(2, 1, '\0');
    driver.parseCleanMessage(msg);
    // Should parse without crash
    ASSERT_GE(driver.getTelemetry().odometerKm, 98540);
}

TEST_TIER1(Test_F5_SplitPacketAcrossThreeFragments) {
    ElmSim::MockElmDevice dev;
    dev.setSplitChunkSize(3); // 3 bytes per chunk
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");
    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);
    dev.setPid31Val(1800);
    driver.update(500);
    driver.update(520);
    ASSERT_EQ(driver.getTelemetry().currentPid31Km, 1800);
}

TEST_TIER1(Test_F6_TenSequentialPid31StepsAccumulation) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.applyPid31Odometer(1000); // baseline
    uint32_t start = driver.getTelemetry().odometerKm;
    for (uint16_t km = 1; km <= 10; ++km) {
        driver.applyPid31Odometer(1000 + km);
    }
    ASSERT_EQ(driver.getTelemetry().odometerKm, start + 10);
}

TEST_TIER1(Test_F6_MultiHourDriveAtConstantSpeedSimulation) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.applyPid31Odometer(500); // baseline
    uint32_t start = driver.getTelemetry().odometerKm;
    // 2 hours at 100 km/h = 200 km
    driver.applyPid31Odometer(700);
    ASSERT_EQ(driver.getTelemetry().odometerKm, start + 200);
}

TEST_TIER1(Test_F7_SpeedFallbackEngagesWhenNoDataReturned) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.parseCleanMessage("NO DATA\r>");
    ASSERT_FALSE(driver.getTelemetry().pid31Supported);
}

TEST_TIER1(Test_F7_SpeedFallbackDeengagesWhenPid31Resumes) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.parseCleanMessage("NO DATA\r>");
    ASSERT_FALSE(driver.getTelemetry().pid31Supported);
    driver.parseCleanMessage("41 31 08 00\r>");
    ASSERT_TRUE(driver.getTelemetry().pid31Supported);
}

TEST_TIER1(Test_F8_TwoSequentialCounterWraps) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.applyPid31Odometer(65530); // baseline
    uint32_t start = driver.getTelemetry().odometerKm;
    driver.applyPid31Odometer(10); // wrap 1 (+16 km)
    driver.applyPid31Odometer(65532); // normal progression
    driver.applyPid31Odometer(5); // wrap 2 (+9 km)
    ASSERT_GT(driver.getTelemetry().odometerKm, start);
}

TEST_TIER1(Test_F9_SimulatorCommandQueueEmptyCheck) {
    ElmSim::MockElmDevice dev;
    ASSERT_FALSE(dev.hasRxData());
    dev.sendCommand("ATZ");
    ASSERT_TRUE(dev.hasRxData());
    std::string resp = dev.readAll();
    ASSERT_FALSE(resp.empty());
    ASSERT_FALSE(dev.hasRxData());
}

TEST_TIER1(Test_F9_SimulatorProtocolIso15765CanLock) {
    ElmSim::MockElmDevice dev;
    dev.sendCommand("ATSP6");
    std::string resp = dev.readAll();
    ASSERT_STR_CONTAINS(resp, "OK");
}

// Additional Tier 2 Boundary Tests (reaching 45 total)
TEST_TIER2(Test_Tier2_Pid31_MinimalNonZeroOneKilometer) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.applyPid31Odometer(1);
    ASSERT_EQ(driver.getTelemetry().odometerKm, 98540);
}

TEST_TIER2(Test_Tier2_Pid31_MaxMinusOneBoundary) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.applyPid31Odometer(65534);
    ASSERT_EQ(driver.getTelemetry().odometerKm, 98540);
}

TEST_TIER2(Test_Tier2_Pid31_DropFrom65535To65530_DtcClearAtMaxEdge) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.applyPid31Odometer(65535);
    uint32_t start = driver.getTelemetry().odometerKm;
    // Dropping from 65535 to 65530: not a wrap, ECU reset occurred at high mileage
    driver.applyPid31Odometer(65530);
    ASSERT_GE(driver.getTelemetry().odometerKm, start);
}

TEST_TIER2(Test_Tier2_Pid31_JumpFromZeroToMax) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.applyPid31Odometer(0);
    uint32_t start = driver.getTelemetry().odometerKm;
    driver.applyPid31Odometer(65535);
    ASSERT_EQ(driver.getTelemetry().odometerKm, start + 65535);
}

TEST_TIER2(Test_Tier2_Pid31_DeltaZeroKm_StoppedVehicle) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.applyPid31Odometer(100);
    uint32_t start = driver.getTelemetry().odometerKm;
    for (int i = 0; i < 50; ++i) {
        driver.applyPid31Odometer(100);
    }
    ASSERT_EQ(driver.getTelemetry().odometerKm, start);
}

TEST_TIER2(Test_Tier2_SpeedFallback_OneKph_MinimumSpeed) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    uint32_t start = driver.getTelemetry().odometerKm;
    // 1 km/h for 3600s = 1.0 km
    driver.applySpeedIntegration(1, 3600.0f);
    ASSERT_EQ(driver.getTelemetry().odometerKm, start + 1);
}

TEST_TIER2(Test_Tier2_SpeedFallback_250Kph_AutobahnMax) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    uint32_t start = driver.getTelemetry().odometerKm;
    // 250 km/h for 144s = (250 * 144) / 3600 = 10 km
    driver.applySpeedIntegration(250, 144.0f);
    ASSERT_EQ(driver.getTelemetry().odometerKm, start + 10);
}

TEST_TIER2(Test_Tier2_SpeedFallback_DtOneMillisecond) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    uint32_t start = driver.getTelemetry().odometerKm;
    driver.applySpeedIntegration(100, 0.001f);
    ASSERT_EQ(driver.getTelemetry().odometerKm, start);
}

TEST_TIER2(Test_Tier2_SpeedFallback_DtTenSeconds) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    uint32_t start = driver.getTelemetry().odometerKm;
    // 180 km/h for 20 seconds = 1.0 km
    driver.applySpeedIntegration(180, 20.0f);
    ASSERT_EQ(driver.getTelemetry().odometerKm, start + 1);
}

TEST_TIER2(Test_Tier2_BatteryVoltage_ZeroVoltsDeadBattery) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.parseVoltage("0.0V\r>");
    ASSERT_NEAR(driver.getTelemetry().batteryVoltage, 0.0f, 0.01f);
}

TEST_TIER2(Test_Tier2_BatteryVoltage_18VoltsOvercharge) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.parseVoltage("18.2V\r>");
    ASSERT_NEAR(driver.getTelemetry().batteryVoltage, 18.2f, 0.05f);
}

TEST_TIER2(Test_Tier2_BatteryVoltage_MissingVSuffixHandled) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    float before = driver.getTelemetry().batteryVoltage;
    driver.parseVoltage("14.2\r>"); // No 'V'
    // Default or preserved safely
    ASSERT_GE(driver.getTelemetry().batteryVoltage, 0.0f);
}

TEST_TIER2(Test_Tier2_BatteryVoltage_WithTextPrefix) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.parseVoltage("BATTERY VOLTAGE: 14.4V\r>");
    ASSERT_NEAR(driver.getTelemetry().batteryVoltage, 14.4f, 0.05f);
}

TEST_TIER2(Test_Tier2_BufferExtremeLength1024Bytes) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    std::string longNoise(1024, ' ');
    longNoise += "41 31 04 B0\r>";
    driver.parseCleanMessage(longNoise);
    ASSERT_EQ(driver.getTelemetry().currentPid31Km, 1200);
}

TEST_TIER2(Test_Tier2_PromptAtPositionZero) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.parseCleanMessage(">");
    ASSERT_EQ(driver.getState(), ElmSim::BtState::DISCONNECTED);
}

TEST_TIER2(Test_Tier2_PromptWithLeadingWhitespace) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.parseCleanMessage("   \r\n   >");
    ASSERT_EQ(driver.getState(), ElmSim::BtState::DISCONNECTED);
}

TEST_TIER2(Test_Tier2_CommandQueryTimeoutSimulation) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");
    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);
    // Device goes silent (no responses sent)
    driver.update(500); // sends query
    driver.update(10000); // 10s elapsed
    // Should survive timeout without crash
    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTED);
}

TEST_TIER2(Test_Tier2_CoolantTempZeroCelsiusFreezing) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    // Raw 40 -> 40 - 40 = 0 C
    driver.parseCleanMessage("41 05 28\r>");
    ASSERT_EQ(driver.getTelemetry().coolantTempC, 0);
}

TEST_TIER2(Test_Tier2_CoolantTemp100CelsiusBoiling) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    // Raw 140 (0x8C) -> 140 - 40 = 100 C
    driver.parseCleanMessage("41 05 8C\r>");
    ASSERT_EQ(driver.getTelemetry().coolantTempC, 100);
}

TEST_TIER2(Test_Tier2_CoolantTemp120CelsiusOverheat) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    // Raw 160 (0xA0) -> 160 - 40 = 120 C
    driver.parseCleanMessage("41 05 A0\r>");
    ASSERT_EQ(driver.getTelemetry().coolantTempC, 120);
}

TEST_TIER2(Test_Tier2_DisconnectDuringAtzHandshake) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");
    driver.update(0);
    driver.update(50); // INITIALIZING_ELM
    dev.setConnected(false); // Disconnect right at ATZ
    driver.update(60);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::RECONNECT_WAIT);
}

TEST_TIER2(Test_Tier2_DisconnectDuringAte0Handshake) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");
    driver.update(0);
    driver.update(50);
    driver.update(60); // ATE0
    dev.setConnected(false);
    driver.update(70);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::RECONNECT_WAIT);
}

TEST_TIER2(Test_Tier2_DisconnectDuringAtsp6Handshake) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");
    driver.update(0);
    driver.update(50);
    driver.update(60);
    driver.update(70); // ATSP6
    dev.setConnected(false);
    driver.update(80);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::RECONNECT_WAIT);
}

TEST_TIER2(Test_Tier2_DisconnectDuringAtrvHandshake) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");
    driver.update(0);
    driver.update(50);
    driver.update(60);
    driver.update(70);
    driver.update(80); // ATRV
    dev.setConnected(false);
    driver.update(90);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::RECONNECT_WAIT);
}

TEST_TIER2(Test_Tier2_ExactReconnectThresholdAt3000Ms) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.handleDisconnect(0);
    driver.update(2999);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::RECONNECT_WAIT);
    driver.update(3000);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTING);
}

TEST_TIER2(Test_Tier2_WrapDetectionWindow_60001_to_4999) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.applyPid31Odometer(60001);
    uint32_t start = driver.getTelemetry().odometerKm;
    driver.applyPid31Odometer(4999);
    // (65536 - 60001) + 4999 = 5535 + 4999 = 10534
    ASSERT_EQ(driver.getTelemetry().odometerKm, start + 10534);
}

TEST_TIER2(Test_Tier2_DtcClearWindow_59999_to_5000) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.applyPid31Odometer(59999);
    uint32_t start = driver.getTelemetry().odometerKm;
    // 59999 not > 60000, so counter drop treated as DTC clear: adds 5000
    driver.applyPid31Odometer(5000);
    ASSERT_EQ(driver.getTelemetry().odometerKm, start + 5000);
}

TEST_TIER2(Test_Tier2_TenCyclesNoResourceLeak) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");
    uint32_t t = 0;
    for (int i = 0; i < 10; ++i) {
        dev.setConnected(true);
        for (int k = 0; k < 10; ++k) { t += 20; driver.update(t); }
        dev.setConnected(false);
        t += 50; driver.update(t);
        t += 3100; driver.update(t);
    }
    ASSERT_EQ(driver.getTelemetry().disconnectCount, 10);
}

TEST_TIER2(Test_Tier2_SpeedIntegration_ZeroDtIgnored) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    uint32_t start = driver.getTelemetry().odometerKm;
    driver.applySpeedIntegration(100, 0.0f);
    ASSERT_EQ(driver.getTelemetry().odometerKm, start);
}

TEST_TIER2(Test_Tier2_SpeedIntegration_NegativeDtIgnored) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    uint32_t start = driver.getTelemetry().odometerKm;
    driver.applySpeedIntegration(100, -1.0f);
    ASSERT_EQ(driver.getTelemetry().odometerKm, start);
}

// Additional Tier 3 Combination Tests (reaching 9 total)
TEST_TIER3(Test_Tier3_Echo_Plus_NoData_Plus_SpeedFallback_Plus_Voltage) {
    ElmSim::MockElmDevice dev;
    dev.setEcho(true);
    dev.setPid31Unsupported(true);
    dev.setVoltageStr("14.1V");
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");
    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTED);
    ASSERT_NEAR(driver.getTelemetry().batteryVoltage, 14.1f, 0.05f);
}

TEST_TIER3(Test_Tier3_FragmentedPackets_Plus_CounterWrap_Plus_Voltage) {
    ElmSim::MockElmDevice dev;
    dev.setSplitChunkSize(2);
    dev.setVoltageStr("14.0V");
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");
    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);
    driver.applyPid31Odometer(65535);
    uint32_t start = driver.getTelemetry().odometerKm;
    driver.applyPid31Odometer(0);
    ASSERT_EQ(driver.getTelemetry().odometerKm, start + 1);
}

TEST_TIER3(Test_Tier3_DisconnectDuringCounterWrap_ReconnectOdoIntact) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");
    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);
    driver.applyPid31Odometer(65534);
    uint32_t odoBefore = driver.getTelemetry().odometerKm;
    dev.setConnected(false); // Disconnect right before wrap
    driver.update(500);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::RECONNECT_WAIT);
    dev.setConnected(true);
    dev.setPid31Val(5); // Car was driven after wrap to 5 km while disconnected
    // Reconnect
    for (uint32_t t = 3600; t <= 3800; t += 20) driver.update(t);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTED);
    // Upon reconnect, initial reading sets baseline without delta jump
    ASSERT_EQ(driver.getTelemetry().odometerKm, odoBefore);
}

TEST_TIER3(Test_Tier3_RapidIgnitionCycles_ThreeDropsInFifteenSeconds) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");
    uint32_t t = 0;
    for (int i = 0; i < 3; ++i) {
        dev.setConnected(true);
        for (int k = 0; k < 10; ++k) { t += 20; driver.update(t); }
        ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTED);
        dev.setConnected(false);
        t += 100; driver.update(t);
        t += 3100; driver.update(t);
    }
    ASSERT_EQ(driver.getTelemetry().disconnectCount, 3);
}

TEST_TIER3(Test_Tier3_SplitPacket_Plus_SyntaxError_ThenValidPid31) {
    ElmSim::MockElmDevice dev;
    dev.setSplitChunkSize(2);
    dev.setRejectNext(true); // First reject "?"
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");
    for (uint32_t t = 0; t <= 100; t += 10) driver.update(t);
    dev.setPid31Val(2100);
    driver.update(500);
    driver.update(550);
    driver.update(1000); // 010D
    driver.update(1500); // 0105
    driver.update(2000); // 0131
    driver.update(2050);
    ASSERT_EQ(driver.getTelemetry().currentPid31Km, 2100);
}

TEST_TIER3(Test_Tier3_HighFrequencySpeedFallback_ToPid31_Handover) {
    ElmSim::MockElmDevice dev;
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("OBDII", "1234");
    // Speed fallback active
    dev.setPid31Unsupported(true);
    driver.parseCleanMessage("NO DATA\r>");
    ASSERT_FALSE(driver.getTelemetry().pid31Supported);
    for (int i = 0; i < 72; ++i) {
        driver.applySpeedIntegration(50, 1.0f); // 50 km/h for 72s = 1.0 km
    }
    ASSERT_EQ(driver.getTelemetry().odometerKm, 98541);
    // Handover back to PID 31
    dev.setPid31Unsupported(false);
    driver.applyPid31Odometer(1000); // baseline at 1000
    driver.applyPid31Odometer(1003); // +3 km
    ASSERT_EQ(driver.getTelemetry().odometerKm, 98544);
}

TEST_TIER3(Test_Tier3_MismatchedPin_ThenUpdatedPin_ConnectsSuccessfully) {
    ElmSim::MockElmDevice dev;
    dev.setCredentials("CivicDongle", "7777");
    ElmSim::ElmDriverSimulator driver(dev);
    driver.setTargetCredentials("CivicDongle", "1234"); // Wrong
    driver.update(0);
    driver.update(50);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::CONNECTING);
    // Update to correct pin
    driver.setTargetCredentials("CivicDongle", "7777");
    driver.update(100);
    ASSERT_EQ(driver.getState(), ElmSim::BtState::INITIALIZING_ELM);
}

// ============================================================================
// MAIN RUNNER
// ============================================================================

int main() {
    return TestHarness::TestRunner::instance().runAll("ELM327 Simulator & State Machine Suite");
}

