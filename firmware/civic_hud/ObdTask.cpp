#include "ObdTask.h"
#include "OdometerEngine.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>

#ifdef ARDUINO
#include <Arduino.h>
#include <BluetoothSerial.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Keep each OBD message and mute changes serialized. The raw screen capture
// owns the serial stream while diagnostics are muted.
#define OBD_LOG(call) do { std::lock_guard<std::mutex> logLock(m_logMutex); if (!m_diagnosticsMuted.load()) { call; } } while (0)

static BluetoothSerial SerialBT;
static volatile bool s_btCloseEvent = false;
static bool s_responsePending = false;

static void sppCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    if (event == ESP_SPP_CLOSE_EVT) s_btCloseEvent = true;
    else if (event == ESP_SPP_OPEN_EVT) s_btCloseEvent = false;
}

static void obdTaskEntry(void* param) {
    ObdTask* task = static_cast<ObdTask*>(param);
    if (task) {
        task->workerLoop();
    }
    vTaskDelete(nullptr);
}

// One outstanding command at a time. After a timeout, recover its prompt
// before transmitting anything else, so a delayed ATI reply cannot become 010D.
static bool sendCommandAndGetResponse(Stream& stream, const char* cmd, char* outBuf, size_t maxLen, uint32_t timeoutMs) {
    if (!outBuf || maxLen == 0) return false;
    outBuf[0] = 0;
    uint32_t drainStart = millis(), quietSince = drainStart;
    while (millis() - drainStart < (s_responsePending ? 8000u : 1800u)) {
        bool received = false;
        while (stream.available()) {
            int c = stream.read(); received = true;
            if (c == '>') s_responsePending = false;
        }
        if (received) quietSince = millis();
        if (!s_responsePending && millis() - quietSince >= 180) break;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    if (s_responsePending) return false;
    char packet[96];
    const int packetLength=snprintf(packet,sizeof(packet),"%s\r",cmd);
    if(packetLength<=0 || packetLength>=(int)sizeof(packet) || stream.write((const uint8_t*)packet,packetLength)!=(size_t)packetLength)return false;
    s_responsePending = true;
    size_t idx = 0;
    const uint32_t startMs = millis();
    while (millis() - startMs < timeoutMs) {
        while (stream.available()) {
            int c = stream.read();
            if (c < 0) break;
            if (c == '>') { outBuf[idx] = 0; s_responsePending = false; return true; }
            if (idx + 1 < maxLen) outBuf[idx++] = static_cast<char>(c);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    outBuf[idx] = 0;
    return false;
}

#endif

// Helper: Normalize raw response string by stripping whitespace and uppercase conversion
static bool isHexByte(const std::string& value) {
    if (value.size() != 2) return false;
    for (char c : value) if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return false;
    return true;
}

static std::string normalizeString(const char* raw) {
    std::string norm;
    if (!raw) return norm;
    for (const char* p = raw; *p != '\0'; ++p) {
        char c = *p;
        if (c == ' ' || c == '\r' || c == '\n' || c == '\t' || c == '>') {
            continue;
        }
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
        norm.push_back(c);
    }
    return norm;
}

ObdTask::ObdTask()
    : m_state(ObdState::DISCONNECTED)
    , m_btDeviceName("OBDII")
    , m_btPin("1234")
    , m_odoEngine(nullptr)
    , m_stateEnterMs(0)
    , m_lastSpeedPollMs(0)
    , m_lastPid31PollMs(0)
    , m_lastAuxPollMs(0)
    , m_consecutiveErrors(0)
    , m_taskRunning(false)
    , m_btStarted(false)
#ifdef ARDUINO
    , m_taskHandle(nullptr)
#endif
{
    m_telemetry.speedKmh = 0.0f;
    m_telemetry.speedValid = false;
    m_telemetry.currentSpeedKph = 0;
    m_telemetry.pid31Km = 0;
    m_telemetry.currentPid31Km = 0;
    m_telemetry.coolantTempC = 0;
    m_telemetry.batteryVoltage = 0.0f;
    m_telemetry.rpm = 0;
    m_telemetry.obdConnected = false;
    m_telemetry.btConnected = false;
    m_telemetry.hasPid31 = false;
    m_telemetry.pid31Supported = true;
    m_telemetry.lastPacketTimestampMs = 0;
}

ObdTask::~ObdTask() {
    m_taskRunning = false;
}

bool ObdTask::begin(const std::string& btDeviceName, const std::string& pin, OdometerEngine* odo) {
    if (btDeviceName.empty() || btDeviceName.size() > 248 || pin.empty() || pin.size() > 16) return false;
#ifdef ARDUINO
    if (m_taskHandle != nullptr) {
        // Worker owns Bluetooth and active strings. Publish replacement config
        // without waiting for an in-progress discovery/connection attempt.
        std::lock_guard<std::mutex> lock(m_configMutex);
        m_pendingName = btDeviceName;
        m_pendingPin = pin;
        m_configPending = true;
        return true;
    }
#endif
    m_btDeviceName = btDeviceName;
    m_btPin = pin;
    if (odo != nullptr) {
        m_odoEngine = odo;
    } else if (m_odoEngine == nullptr) {
        m_odoEngine = OdometerEngine::getInstance();
    }

#ifdef ARDUINO
    if (!m_btStarted) {
        // Initialize Classic SPP master before configuring PIN. BLE is unused;
        // disabling it releases its controller RAM for Wi-Fi and display buffers.
        if (!SerialBT.begin("CivicHUD_BT", true, true)) {
            OBD_LOG(Serial.println(F("[OBD] Failed to initialize BluetoothSerial master mode!")));
            return false;
        }

        // Set pairing PIN after stack initialization
        SerialBT.setPin(m_btPin.c_str(), m_btPin.length());

        // Auto-accept Simple Secure Pairing (SSP) requests
        SerialBT.onConfirmRequest([](uint32_t) {
            SerialBT.confirmReply(true);
        });

        SerialBT.onAuthComplete([this](bool success) {
            OBD_LOG(Serial.printf("[BT_AUTH] success=%d\n",success));
        });

        // Register link closure event listener
        SerialBT.register_callback(sppCallback);
        m_btStarted = true;
    } else {
        SerialBT.setPin(m_btPin.c_str(), m_btPin.length());
    }

    // Requirement 2: Start background worker task on Core 0
    if (m_taskHandle == nullptr) {
        m_taskRunning = true;
        BaseType_t ret = xTaskCreatePinnedToCore(
            obdTaskEntry,
            "ObdWorker",
            6144,
            this,
            1,
            (TaskHandle_t*)&m_taskHandle,
            0 // Core 0
        );
        if (ret != pdPASS) {
            OBD_LOG(Serial.println(F("[OBD] Failed to create ObdWorker FreeRTOS task on Core 0!")));
            return false;
        }
        OBD_LOG(Serial.println(F("[OBD] Non-blocking FreeRTOS ObdWorker running on Core 0")));
    }
#else
    m_telemetry.obdConnected = true;
    m_telemetry.btConnected = true;
#endif

    return true;
}

bool ObdTask::requestQuery(const std::string& command) {
    static const char* allowed[] = {"ATI", "ATDP", "ATDPN", "0100", "010D", "0131", "ATRV", "010C", "0105", "0120", "0140", "ATH1", "ATSP6", "ATSH7DF", "ATCAF1", "ATCRA", "ATSH7E0", "ATSP7", "ATSH18DB33F1", "ATSH18DA10F1"};
    bool valid = false;
    for (const char* item : allowed) if (command == item) valid = true;
    if (!valid) {
#ifdef ARDUINO
        OBD_LOG(Serial.println("[OBD_QUERY_ERROR] COMMAND_NOT_ALLOWED"));
#endif
        return false;
    }
    std::lock_guard<std::mutex> lock(m_configMutex);
    if (!m_pendingQuery.empty()) {
#ifdef ARDUINO
        OBD_LOG(Serial.println("[OBD_QUERY_ERROR] QUEUE_BUSY"));
#endif
        return false;
    }
    m_pendingQuery = command;
    return true;
}

bool ObdTask::exchangeCommand(const char* command, char* reply, size_t capacity, uint32_t timeoutMs) {
#ifdef ARDUINO
    const bool received = sendCommandAndGetResponse(SerialBT, command, reply, capacity, timeoutMs);
    char printable[256]; size_t i = 0;
    for (; reply[i] && i + 1 < sizeof(printable); ++i) printable[i] = (reply[i] == '\r' || reply[i] == '\n') ? '|' : reply[i];
    printable[i] = 0;
    OBD_LOG(Serial.printf("[OBD_REPLY] cmd=%s prompt=%d raw=%s\n", command, received, printable));
    return received;
#else
    (void)command; (void)timeoutMs; if (capacity) reply[0] = 0; return false;
#endif
}

bool ObdTask::initializeElm(VehicleTelemetry& telemetry) {
#ifdef ARDUINO
    char reply[256];
    if (!exchangeCommand("ATZ",reply,sizeof(reply),5000) || normalizeString(reply).find("ELM") == std::string::npos) return false;
    for (const char* command : {"ATE0","ATL0","ATS1","ATH0","ATSP0"}) {
        if (!exchangeCommand(command,reply,sizeof(reply),2500) || normalizeString(reply).find("OK") == std::string::npos) return false;
    }
    // AT setup is cached for this live SPP link. Unsupported ECU PIDs do not
    // invalidate a healthy adapter or restart it, allowing diagnostic queries.
    telemetry.speedValid = false;
    telemetry.hasPid31 = false;
    telemetry.obdConnected = false;
    const bool maskPrompt=exchangeCommand("0100",reply,sizeof(reply),12000);
    const std::string supported = normalizeString(reply);
    const size_t pos = supported.find("4100");
    bool maskValid = maskPrompt && pos != std::string::npos && supported.size() >= pos + 12;
    if(maskValid)for(size_t i=pos+4;i<pos+12;i+=2)if(!isHexByte(supported.substr(i,2)))maskValid=false;
    OBD_LOG(Serial.printf("[OBD_SUPPORTED] pid00_valid=%d adapter_ready=1\n",maskValid));
    uint8_t speed=0;
    if (exchangeCommand("010D",reply,sizeof(reply),5000) && parsePid010D(reply,speed) == ElmParseResult::OK) {
        telemetry.speedKmh=speed; telemetry.currentSpeedKph=speed;
        telemetry.speedValid=true; telemetry.obdConnected=true;
        telemetry.lastPacketTimestampMs=m_lastSpeedValidMs=millis();
        OBD_LOG(Serial.printf("[OBD_PARSED] speed_kmh=%u ready=1\n",speed));
    } else {
        telemetry.speedKmh=0; telemetry.currentSpeedKph=0;
        OBD_LOG(Serial.println("[OBD] Adapter ready; speed unavailable. Keeping BT for PID31 and diagnostics."));
    }
    return true;
#else
    (void)telemetry; return false;
#endif
}

void ObdTask::setDiagnosticsMuted(bool muted) {
    std::lock_guard<std::mutex> lock(m_logMutex);
    m_diagnosticsMuted.store(muted);
#ifdef ARDUINO
    if (muted) Serial.flush();
#endif
}

void ObdTask::setOdometerEngine(OdometerEngine* engine) {
    m_odoEngine = engine;
}

void ObdTask::update() {
    // Non-blocking update called from Core 1 Arduino loop
    if (m_odoEngine == nullptr) {
        m_odoEngine = OdometerEngine::getInstance();
    }
}

VehicleTelemetry ObdTask::getTelemetry() const {
    std::lock_guard<std::mutex> lock(m_telemetryMutex);
    return m_telemetry;
}

bool ObdTask::isConnected() const {
    return getTelemetry().obdConnected;
}

ObdState ObdTask::getState() const {
    return m_state;
}

// -----------------------------------------------------------------------------
// Parsing Logic (Static & Platform Independent)
// -----------------------------------------------------------------------------

ElmParseResult ObdTask::parsePid0131(const char* raw, uint16_t& outDistanceKm) {
    if (!raw) return ElmParseResult::ERROR_RESP;
    std::string norm = normalizeString(raw);
    if (norm.empty()) return ElmParseResult::TIMEOUT;

    if (norm.find("NODATA") != std::string::npos) {
        return ElmParseResult::NO_DATA;
    }
    if (norm.find('?') != std::string::npos || 
        norm.find("UNABLETOCONNECT") != std::string::npos || 
        norm.find("ERROR") != std::string::npos) {
        return ElmParseResult::ERROR_RESP;
    }

    size_t pos = norm.find("4131");
    if (pos == std::string::npos) {
        if (norm.find("SEARCHING") != std::string::npos) {
            return ElmParseResult::SEARCHING;
        }
        return ElmParseResult::ERROR_RESP;
    }

    // Require 4 hex characters (2 bytes) after "4131"
    if (pos + 4 + 4 > norm.length()) {
        return ElmParseResult::ERROR_RESP;
    }

    std::string hexA = norm.substr(pos + 4, 2);
    std::string hexB = norm.substr(pos + 6, 2);
    if (!isHexByte(hexA) || !isHexByte(hexB)) return ElmParseResult::ERROR_RESP;
    char* endA = nullptr;
    char* endB = nullptr;
    unsigned long valA = strtoul(hexA.c_str(), &endA, 16);
    unsigned long valB = strtoul(hexB.c_str(), &endB, 16);
    if (*endA != '\0' || *endB != '\0') {
        return ElmParseResult::ERROR_RESP;
    }

    outDistanceKm = static_cast<uint16_t>((valA << 8) | valB);
    return ElmParseResult::OK;
}

ElmParseResult ObdTask::parsePid010D(const char* raw, uint8_t& outSpeedKph) {
    if (!raw) return ElmParseResult::ERROR_RESP;
    std::string norm = normalizeString(raw);
    if (norm.empty()) return ElmParseResult::TIMEOUT;

    if (norm.find("NODATA") != std::string::npos) {
        return ElmParseResult::NO_DATA;
    }
    if (norm.find('?') != std::string::npos || 
        norm.find("UNABLETOCONNECT") != std::string::npos || 
        norm.find("ERROR") != std::string::npos) {
        return ElmParseResult::ERROR_RESP;
    }

    size_t pos = norm.find("410D");
    if (pos == std::string::npos) {
        return ElmParseResult::ERROR_RESP;
    }

    // Require 2 hex characters (1 byte) after "410D"
    if (pos + 4 + 2 > norm.length()) {
        return ElmParseResult::ERROR_RESP;
    }

    std::string hexSpd = norm.substr(pos + 4, 2);
    if (!isHexByte(hexSpd)) return ElmParseResult::ERROR_RESP;
    char* end = nullptr;
    unsigned long val = strtoul(hexSpd.c_str(), &end, 16);
    if (*end != '\0') {
        return ElmParseResult::ERROR_RESP;
    }

    outSpeedKph = static_cast<uint8_t>(val);
    return ElmParseResult::OK;
}

ElmParseResult ObdTask::parsePid0105(const char* raw, int16_t& outCoolantTempC) {
    if (!raw) return ElmParseResult::ERROR_RESP;
    std::string norm = normalizeString(raw);
    if (norm.empty()) return ElmParseResult::TIMEOUT;

    if (norm.find("NODATA") != std::string::npos) {
        return ElmParseResult::NO_DATA;
    }
    if (norm.find('?') != std::string::npos || 
        norm.find("UNABLETOCONNECT") != std::string::npos || 
        norm.find("ERROR") != std::string::npos) {
        return ElmParseResult::ERROR_RESP;
    }

    size_t pos = norm.find("4105");
    if (pos == std::string::npos) {
        return ElmParseResult::ERROR_RESP;
    }

    if (pos + 4 + 2 > norm.length()) {
        return ElmParseResult::ERROR_RESP;
    }

    std::string hexTemp = norm.substr(pos + 4, 2);
    if (!isHexByte(hexTemp)) return ElmParseResult::ERROR_RESP;
    char* end = nullptr;
    unsigned long val = strtoul(hexTemp.c_str(), &end, 16);
    if (*end != '\0') {
        return ElmParseResult::ERROR_RESP;
    }

    // Formula: tempC = val - 40
    outCoolantTempC = static_cast<int16_t>(val) - 40;
    return ElmParseResult::OK;
}

ElmParseResult ObdTask::parseAtRv(const char* raw, float& outVoltage) {
    if (!raw) return ElmParseResult::ERROR_RESP;
    std::string norm = normalizeString(raw);
    if (norm.empty()) return ElmParseResult::TIMEOUT;

    // Skip echo of "ATRV" if present in normalized stream
    size_t startPos = 0;
    size_t atrvPos = norm.rfind("ATRV");
    if (atrvPos != std::string::npos) {
        startPos = atrvPos + 4;
    }

    size_t vPos = norm.find('V', startPos);
    if (vPos == std::string::npos) {
        vPos = norm.rfind('V');
    }
    if (vPos == std::string::npos) {
        return ElmParseResult::ERROR_RESP;
    }

    std::string num;
    for (int i = static_cast<int>(vPos) - 1; i >= static_cast<int>(startPos); --i) {
        char c = norm[i];
        if (isdigit(c) || c == '.') {
            num.insert(num.begin(), c);
        } else if (!num.empty()) {
            break;
        }
    }

    if (num.empty()) {
        return ElmParseResult::ERROR_RESP;
    }

    char* end = nullptr;
    float v = strtof(num.c_str(), &end);
    if (end == num.c_str()) {
        return ElmParseResult::ERROR_RESP;
    }

    outVoltage = v;
    return ElmParseResult::OK;
}

// -----------------------------------------------------------------------------
// Core 0 Worker Task Loop
// -----------------------------------------------------------------------------

void ObdTask::workerLoop() {
#ifdef ARDUINO
    // Brief startup window for an explicit USB diagnostic command.
    vTaskDelay(pdMS_TO_TICKS(5000));
    while (m_taskRunning) {
        if(m_configurationRequested.load()) {
            if(!m_configurationPaused.load()) {
                SerialBT.end();m_btStarted=false;s_responsePending=false;
                {std::lock_guard<std::mutex> lock(m_telemetryMutex);m_telemetry.btConnected=false;m_telemetry.obdConnected=false;m_telemetry.speedKmh=0;m_telemetry.currentSpeedKph=0;m_telemetry.speedValid=false;m_telemetry.hasPid31=false;m_telemetry.batteryVoltage=0;m_telemetry.lastPacketTimestampMs=0;}
                m_state=ObdState::DISCONNECTED;
                m_scanRequested.store(false);
                m_configurationPaused.store(true);
                OBD_LOG(Serial.println("[OBD] Paused for settings hotspot"));
            }
            vTaskDelay(pdMS_TO_TICKS(50));continue;
        }
        if(m_configurationPaused.load()) {
            if(!SerialBT.begin("CivicHUD_BT",true,true)){vTaskDelay(pdMS_TO_TICKS(1000));continue;}
            SerialBT.setPin(m_btPin.c_str(),m_btPin.length());
            SerialBT.onConfirmRequest([](uint32_t){SerialBT.confirmReply(true);});
            SerialBT.register_callback(sppCallback);
            m_btStarted=true;s_btCloseEvent=false;s_responsePending=false;
            m_state=ObdState::DISCONNECTED;m_stateEnterMs=millis();
            m_lastSpeedValidMs=m_lastPid31ValidMs=0;
            m_configurationPaused.store(false);
            OBD_LOG(Serial.println("[OBD] Resuming after settings hotspot"));
        }
        VehicleTelemetry telemetry = getTelemetry();
        bool configChanged = false;
        {
            std::lock_guard<std::mutex> lock(m_configMutex);
            if (m_configPending) {
                m_btDeviceName = m_pendingName;
                m_btPin = m_pendingPin;
                m_configPending = false;
                configChanged = true;
            }
        }
        if (configChanged) {
            SerialBT.disconnect();
            SerialBT.setPin(m_btPin.c_str(), m_btPin.length());
            telemetry.obdConnected = telemetry.btConnected = false;
            m_state = ObdState::DISCONNECTED;
        }
        if (m_scanRequested.exchange(false)) {
            telemetry.obdConnected=telemetry.btConnected=false;
            SerialBT.disconnect();
            OBD_LOG(Serial.printf("[BT_SCAN_BEGIN] target=%s free_heap=%u\n",m_btDeviceName.c_str(),ESP.getFreeHeap()));
            BTScanResults* results=SerialBT.discover(12800);
            const int count=results?results->getCount():0;
            uint8_t targetAddress[6]={0}; bool found=false;
            for(int i=0;i<count;++i) {
                BTAdvertisedDevice* device=results->getDevice(i);
                if(!device)continue;
                const auto name=device->getName();
                const auto address=device->getAddress();
                OBD_LOG(Serial.printf("[BT_DEVICE] name=%s address=%s rssi=%d\n",name.c_str(),address.toString().c_str(),device->getRSSI()));
                if(name==m_btDeviceName) {
                    memcpy(targetAddress,*address.getNative(),6);found=true;
                }
            }
            OBD_LOG(Serial.printf("[BT_SCAN_END] count=%d target_found=%d\n",count,found));
            bool linked=false;
            if(found) {
                OBD_LOG(Serial.printf("[BT_CONNECT_MAC] %02X:%02X:%02X:%02X:%02X:%02X\n",targetAddress[0],targetAddress[1],targetAddress[2],targetAddress[3],targetAddress[4],targetAddress[5]));
                linked=SerialBT.connected(2000)||SerialBT.connect(targetAddress);
            }
            OBD_LOG(Serial.printf("[BT_CONNECT_RESULT] linked=%d\n",linked));
            if(linked) {
                s_responsePending=false;
                char reply[128];
                for(const char* query:{"ATI","010D","0131"}) {
                    bool received=exchangeCommand(query,reply,sizeof(reply),1800);
                    OBD_LOG(Serial.printf("[BT_REPLY] command=%s received=%d response=%s\n",query,received,reply));
                }
                telemetry.btConnected=true;
                m_state=ObdState::INITIALIZING_ELM;
            } else m_state=ObdState::RECONNECT_WAIT;
            m_stateEnterMs=millis();
            OBD_LOG(Serial.println("[BT_DIAGNOSTIC_DONE]"));
        }
        std::string requestedQuery;
        {
            std::lock_guard<std::mutex> lock(m_configMutex);
            requestedQuery.swap(m_pendingQuery);
        }
        if (!requestedQuery.empty()) {
            char reply[256];
            if (SerialBT.connected(0)) exchangeCommand(requestedQuery.c_str(),reply,sizeof(reply),12000);
            else OBD_LOG(Serial.printf("[OBD_QUERY] cmd=%s error=BT_NOT_CONNECTED\n",requestedQuery.c_str()));
            OBD_LOG(Serial.println("[OBD_QUERY_DONE]"));
        }
        uint32_t now = millis();

        switch (m_state) {
            case ObdState::DISCONNECTED: {
                telemetry.obdConnected = false;
                telemetry.btConnected = false;
                m_state = ObdState::CONNECTING;
                m_stateEnterMs = now;
                break;
            }

            case ObdState::CONNECTING: {
                telemetry.obdConnected = false;
                telemetry.btConnected = false;
                s_btCloseEvent = false;

                OBD_LOG(Serial.printf("[OBD] Connecting to '%s' (PIN: %s)...\n", m_btDeviceName.c_str(), m_btPin.c_str()));
                uint8_t knownAddress[6] = {0xED,0x5F,0x28,0xB2,0x7E,0x1D};
                const bool linked = m_btDeviceName == "OBDII" ? SerialBT.connect(knownAddress) : SerialBT.connect(m_btDeviceName.c_str());
                if (linked) {
                    s_responsePending = false;
                    OBD_LOG(Serial.println(F("[OBD] BT SPP link established! Initializing ELM327...")));
                    telemetry.btConnected = true;
                    m_state = ObdState::INITIALIZING_ELM;
                    m_stateEnterMs = millis();
                } else {
                    OBD_LOG(Serial.println(F("[OBD] BT connect attempt failed. Waiting to retry...")));
                    m_state = ObdState::RECONNECT_WAIT;
                    m_stateEnterMs = millis();
                }
                break;
            }

            case ObdState::INITIALIZING_ELM: {
                if (s_btCloseEvent || !SerialBT.connected(0)) {
                    s_btCloseEvent = false;
                    OBD_LOG(Serial.println(F("[OBD] Link dropped during initialization.")));
                    m_state = ObdState::RECONNECT_WAIT;
                    m_stateEnterMs = millis();
                    break;
                }

                if (!initializeElm(telemetry)) {
                    OBD_LOG(Serial.println("[OBD] ELM/ECU readiness failed; retrying actual initialization."));
                    telemetry.obdConnected=false;
                    SerialBT.disconnect();
                    telemetry.btConnected=false;
                    m_state=ObdState::RECONNECT_WAIT; m_stateEnterMs=millis();
                    break;
                }
                // Preserve the known ECU counter baseline across transient data loss/reconnect.
                telemetry.pid31Supported=true;
                telemetry.hasPid31=false;

                // Query initial battery voltage
                char buf[64];
                if (exchangeCommand( "ATRV", buf, sizeof(buf), 2000)) {
                    float v = 0.0f;
                    if (parseAtRv(buf, v) == ElmParseResult::OK) {
                        telemetry.batteryVoltage = v;
                    }
                }

                OBD_LOG(Serial.printf("[OBD] Adapter initialized; ECU data_ready=%d. Keeping live BT link.\n",telemetry.obdConnected));
                m_consecutiveErrors = 0;
                m_lastSpeedPollMs = millis();
                m_lastPid31PollMs = 0;
                m_lastAuxPollMs = 0;
                m_state = ObdState::CONNECTED;
                m_stateEnterMs = millis();
                break;
            }

            case ObdState::CONNECTED: {
                // Link drop detection: event callback or connected(0) check
                if (s_btCloseEvent || !SerialBT.connected(0)) {
                    s_btCloseEvent = false;
                    OBD_LOG(Serial.println(F("[OBD] BT link lost! Resetting connection...")));
                    SerialBT.disconnect();
                    telemetry.obdConnected = false;
                    telemetry.btConnected = false;
                    m_state = ObdState::RECONNECT_WAIT;
                    m_stateEnterMs = millis();
                    break;
                }

                uint32_t currentMs = millis();

                // 1. Poll Speed (PID 01 0D) at 2 Hz (every 500 ms)
                if (currentMs - m_lastSpeedPollMs >= 500) {
                    uint32_t dtMs = currentMs - m_lastSpeedPollMs;
                    m_lastSpeedPollMs = currentMs;

                    char rxBuf[64];
                    if (exchangeCommand( "010D", rxBuf, sizeof(rxBuf), 2500)) {
                        uint8_t spd = 0;
                        if (parsePid010D(rxBuf, spd) == ElmParseResult::OK) {
                            m_consecutiveErrors = 0;
                            telemetry.speedValid = true;
                            m_lastSpeedValidMs=millis();
                            telemetry.speedKmh = static_cast<float>(spd);
                            telemetry.currentSpeedKph = spd;
                            OBD_LOG(Serial.printf("[OBD_PARSED] speed_kmh=%u odo_km=%u\n",spd,m_odoEngine?m_odoEngine->getCurrentOdo():0));
                            telemetry.lastPacketTimestampMs = currentMs;

                            // Fallback: If PID 01 31 is unsupported / NO DATA, advance odometer via speed integration
                            if (!telemetry.pid31Supported && m_odoEngine != nullptr) {
                                float dtSec = static_cast<float>(dtMs) / 1000.0f;
                                m_odoEngine->updateFromSpeed(telemetry.speedKmh, dtSec);
                            }
                        } else {
                            // NO DATA/unsupported is a valid ELM prompt, not a lost BT link.
                            m_consecutiveErrors=0;
                            telemetry.speedValid=false;
                            telemetry.speedKmh=0; telemetry.currentSpeedKph=0;
                        }
                    } else {
                        m_consecutiveErrors++;
                    }
                }

                // 2. Poll PID 01 31 (Distance since codes cleared) every 2000 ms
                if (currentMs - m_lastPid31PollMs >= (telemetry.pid31Supported ? 2000u : 30000u)) {
                    m_lastPid31PollMs = currentMs;

                    char rxBuf[64];
                    if (exchangeCommand( "0131", rxBuf, sizeof(rxBuf), 3000)) {
                        uint16_t distKm = 0;
                        ElmParseResult res = parsePid0131(rxBuf, distKm);
                        if (res == ElmParseResult::OK) {
                            m_consecutiveErrors = 0;
                            telemetry.pid31Km = distKm;
                            telemetry.currentPid31Km = distKm;
                            OBD_LOG(Serial.printf("[OBD_PARSED] pid31_km=%u\n",distKm));
                            // Fallback already accounted for travel during missing PID31.
                            // First recovered reading establishes a new baseline.
                            // Preserve the known ECU counter baseline across transient data loss/reconnect.
                            m_lastPid31ValidMs=millis();
                            telemetry.hasPid31 = true;
                            telemetry.pid31Supported = true;
                            telemetry.lastPacketTimestampMs = currentMs;

                            if (m_odoEngine != nullptr) {
                                m_odoEngine->updateFromPid31(static_cast<uint32_t>(distKm));
                            }
                        } else if (res == ElmParseResult::NO_DATA) {
                            OBD_LOG(Serial.println(F("[OBD] PID 01 31 returned NO DATA! Engaging speed integration fallback.")));
                            m_consecutiveErrors=0;
                            telemetry.hasPid31 = false;
                            telemetry.pid31Supported = m_odoEngine && m_odoEngine->hasPid31Baseline();
                            if(telemetry.pid31Supported) OBD_LOG(Serial.println("[OBD] Temporary PID31 gap; retaining counter baseline."));
                        } else {
                            m_consecutiveErrors=0;
                            telemetry.hasPid31=false;
                        }
                    } else {
                        m_consecutiveErrors++;
                    }
                }

                // 3. Poll Aux PIDs (Coolant 01 05 & Battery ATRV) every 3000 ms
                if (currentMs - m_lastAuxPollMs >= 3000) {
                    m_lastAuxPollMs = currentMs;

                    char rxBuf[64];
                    if (exchangeCommand( "0105", rxBuf, sizeof(rxBuf), 2000)) {
                        int16_t coolant = 0;
                        if (parsePid0105(rxBuf, coolant) == ElmParseResult::OK) {
                            telemetry.coolantTempC = coolant;
                        }
                    }

                    if (exchangeCommand( "ATRV", rxBuf, sizeof(rxBuf), 2000)) {
                        float volt = 0.0f;
                        if (parseAtRv(rxBuf, volt) == ElmParseResult::OK) {
                            telemetry.batteryVoltage = volt;
                        }
                    }
                }

                // Display validity reflects fresh payloads, separate from BT/adapter readiness.
                const uint32_t afterPoll=millis();
                if(telemetry.speedValid && afterPoll-m_lastSpeedValidMs>10000) {
                    telemetry.speedValid=false; telemetry.speedKmh=0; telemetry.currentSpeedKph=0;
                }
                if(telemetry.hasPid31 && afterPoll-m_lastPid31ValidMs>10000) telemetry.hasPid31=false;
                telemetry.obdConnected=telemetry.speedValid || telemetry.hasPid31;

                // Detect consecutive communication failures
                if (m_consecutiveErrors >= 4) {
                    OBD_LOG(Serial.println(F("[OBD] Multiple consecutive query timeouts. Triggering link recovery...")));
                    SerialBT.disconnect();
                    telemetry.obdConnected = false;
                    telemetry.btConnected = false;
                    m_state = ObdState::RECONNECT_WAIT;
                    m_stateEnterMs = millis();
                    break;
                }

                vTaskDelay(pdMS_TO_TICKS(50));
                break;
            }

            case ObdState::RECONNECT_WAIT: {
                telemetry.obdConnected = false;
                telemetry.btConnected = false;
                s_btCloseEvent = false;

                // Backoff 3.0 to 3.5 seconds before attempting reconnect (satisfies <= 10s auto-reconnect)
                if (millis() - m_stateEnterMs >= 3500) {
                    m_state = ObdState::CONNECTING;
                    m_stateEnterMs = millis();
                } else {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                break;
            }
        }
        {
            std::lock_guard<std::mutex> lock(m_telemetryMutex);
            m_telemetry = telemetry;
        }
    }
#endif
}
