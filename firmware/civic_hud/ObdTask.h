#pragma once
#include <cstdint>
#include <string>
#include <mutex>
#include <atomic>

// Connection state machine states
enum class ObdState {
    DISCONNECTED = 0,
    CONNECTING = 1,
    INITIALIZING_ELM = 2,
    CONNECTED = 3,
    RECONNECT_WAIT = 4
};

// Response parsing result
enum class ElmParseResult {
    OK = 0,
    NO_DATA = 1,
    ERROR_RESP = 2,
    TIMEOUT = 3,
    SEARCHING = 4
};

// Telemetry state structure
struct VehicleTelemetry {
    float speedKmh;
    bool speedValid;
    uint8_t currentSpeedKph;
    uint16_t pid31Km;
    uint32_t currentPid31Km;
    int16_t coolantTempC;
    float batteryVoltage;
    uint16_t rpm;
    bool obdConnected;
    bool btConnected;
    bool hasPid31;
    bool pid31Supported;
    uint32_t lastPacketTimestampMs;
};
typedef VehicleTelemetry ObdTelemetry;

class OdometerEngine; // Forward declaration

class ObdTask {
public:
    ObdTask();
    ~ObdTask();

    // Start background worker on Core 0
    bool begin(const std::string& btDeviceName, const std::string& pin = "1234", OdometerEngine* odo = nullptr);

    // Return only once earlier OBD diagnostics have finished writing.
    void setDiagnosticsMuted(bool muted);

    // Link odometer engine
    void setOdometerEngine(OdometerEngine* engine);

    // Lightweight update called from Core 1 loop (non-blocking)
    void update();

    // Accessors
    VehicleTelemetry getTelemetry() const;
    bool isConnected() const;
    ObdState getState() const;

    // Helper functions for parsing ELM responses (used by driver and test harnesses)
    static ElmParseResult parsePid0131(const char* raw, uint16_t& outDistanceKm);
    static ElmParseResult parsePid010D(const char* raw, uint8_t& outSpeedKph);
    static ElmParseResult parsePid0105(const char* raw, int16_t& outCoolantTempC);
    static ElmParseResult parseAtRv(const char* raw, float& outVoltage);

    bool requestQuery(const std::string& command);

    void requestConfigurationPause(bool paused) { m_configurationRequested.store(paused); }
    bool configurationPaused() const { return m_configurationPaused.load(); }

    void requestScanConnect() { m_scanRequested.store(true); }

    // Core 0 worker task loop
    void workerLoop();

private:
    std::mutex m_logMutex;
    std::atomic<bool> m_diagnosticsMuted{false};
    mutable std::mutex m_telemetryMutex;
    std::mutex m_configMutex;
    std::string m_pendingName;
    std::string m_pendingPin;
    bool m_configPending = false;
    std::atomic<bool> m_scanRequested{false};
    std::atomic<bool> m_configurationRequested{false};
    std::atomic<bool> m_configurationPaused{false};
    std::string m_pendingQuery;
    bool exchangeCommand(const char* command, char* reply, size_t capacity, uint32_t timeoutMs);
    bool initializeElm(VehicleTelemetry& telemetry);
    VehicleTelemetry m_telemetry;
    std::atomic<ObdState> m_state;
    std::string m_btDeviceName;
    std::string m_btPin;
    OdometerEngine* m_odoEngine;

    uint32_t m_stateEnterMs;
    uint32_t m_lastSpeedPollMs;
    uint32_t m_lastPid31PollMs;
    uint32_t m_lastAuxPollMs;
    uint32_t m_lastSpeedValidMs = 0;
    uint32_t m_lastPid31ValidMs = 0;
    uint8_t m_consecutiveErrors;
    std::atomic<bool> m_taskRunning;
    bool m_btStarted;

#ifdef ARDUINO
    void* m_taskHandle; // FreeRTOS TaskHandle_t
#endif
};
