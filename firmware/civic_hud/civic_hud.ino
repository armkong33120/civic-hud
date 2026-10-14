#include <Arduino.h>
#include <esp_heap_caps.h>
#include <WiFi.h>
#include <WebServer.h>
#include <BluetoothSerial.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include "pins.h"
#include "config.h"
#include "MaintenanceItem.h"
#include "OdometerEngine.h"
#include "MaintenanceEngine.h"
#include "StorageManager.h"
#include "ObdTask.h"
#include "WebSettings.h"
#include "DisplayEngine.h"
#include "ResourceMonitor.h"

// --- Global Hardware Drivers ---
static TFT_eSPI            g_tft = TFT_eSPI();
static HUDDisplay           g_display(g_tft);
static SPIClass            g_touchSpi(HSPI);
static XPT2046_Touchscreen g_touch(XPT2046_CS, XPT2046_IRQ);

// --- Global Core Engines ---
static bool                g_storedPidBaseline = false;
static OdometerEngine      g_odoEngine;
static MaintenanceEngine   g_maintEngine;
static StorageManager      g_storage;
static ObdTask             g_obdTask;

// --- Web Server (AP: 192.168.1.1) ---
static WebServer           g_server(WEB_SERVER_PORT);

// --- UI State Tracking ---
static int                 g_modalItemIdx = -1;  // -1 = modal closed, 0..N = item index in sorted list
static int                 g_slideProgress = 0;   // 0..100%
static uint32_t            g_lastUiRefresh = 0;
static uint32_t            g_lastTouchTime = 0;
static bool                g_screenDirty = true;
static ResourceMonitor     g_monitor;
static bool                g_monitorScreen = false;
static uint8_t             g_hotspotState = 0; // off, opening, on, closing, error
static uint32_t            g_hotspotRequestedMs = 0;
static bool                g_webRoutesReady = false;
static int                 g_scrollOffset = 0;

// Color Palette (RGB565)
#define C_BG        0x0000  // Very dark slate (#0b0f14)
#define C_CARD_OK   0x10E3  // Dark grey card (#161b22)
#define C_CARD_WARN 0x2902  // Dark amber card (#2a2210)
#define C_CARD_ERR  0x2882  // Dark red card (#2a1215)
#define C_BORDER_OK 0x3DCA  // Emerald green (#3fb950)
#define C_BORDER_WN 0xE588  // Amber (#e3b341)
#define C_BORDER_ER 0xF289  // Red (#f85149)
#define C_TEXT_MUT  0x8410  // Gray text
#define C_ACCENT    0x3CF3  // Cyan/Sky blue

// Function Prototypes
void initHardware();
void playChime(uint16_t freq, uint16_t durationMs);
void updateRgbStatus();
void updateAutoDimming();
void drawHeader();
void drawServiceList();
void drawModal();
void handleTouch();
bool setupWebServer();
void toggleHotspot();
void updateHotspot();

// =========================================================================
// Setup & Main Loop
// =========================================================================
void setup() {
    Serial.begin(115200);
    Serial.println(F("\n[INIT] Starting Civic FC 1.8 Maintenance HUD..."));

    initHardware();
    if (!g_display.begin()) Serial.println(F("[UI] Sprite allocation FAILED"));

    // Initialize Storage & Odometer
    if(!g_storage.begin()) Serial.println(F("[STORAGE] Initialization incomplete"));
    uint32_t savedOdo = DEFAULT_INITIAL_ODO, savedPid = 0;
    bool validPid = false;
    float fraction = 0;
    if (g_storage.loadOdometerCheckpoint(savedOdo, savedPid, validPid, &fraction)) {
        OdometerSnapshot checkpoint{savedOdo, savedPid, validPid, fraction};
        g_odoEngine.restore(checkpoint);
        g_storedPidBaseline=validPid;
    } else {
        g_odoEngine.calibrate(DEFAULT_INITIAL_ODO);
        Serial.println(F("[STORAGE] No checkpoint; using initial odometer."));
    }
    if (!g_storage.loadItems(g_maintEngine.getItems())) {
        g_maintEngine.loadDefaults();
        // Keep existing stored bytes available for recovery; never overwrite on a failed read.
        if (!g_storage.hasItemsData()) {
            if (!g_storage.saveItems(g_maintEngine.getItems())) Serial.println(F("[STORAGE] Initial save failed"));
        } else Serial.println(F("[STORAGE] Using defaults in RAM; stored data preserved."));
    }
    g_maintEngine.evaluate(g_odoEngine.getCurrentOdo(), 0);

    // Load Bluetooth OBD Configuration
    std::string btName = OBD_BT_DEVICE_NAME;
    std::string btPin = OBD_BT_PIN;
    g_storage.loadBtConfig(btName, btPin);

    // Start Web Server AP (http://192.168.1.1)
    WiFi.mode(WIFI_OFF);
    Serial.println(F("[WEB] Wi-Fi paused for OBD distance test"));

    // Start Bluetooth OBD Client
    g_obdTask.begin(btName, btPin, &g_odoEngine);

    Serial.printf("[HEAP] ready free=%u largest=%u min=%u\n",ESP.getFreeHeap(),heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),ESP.getMinFreeHeap());

    // Initial audio chime & screen draw
    playChime(1000, 100);
    delay(50);
    playChime(1400, 150);

    g_screenDirty = true;
    g_monitor.begin();
    Serial.println(F("[INIT] Systems ready! Rendering Layout C..."));
}

bool persistCheckpoint() {
    const auto state = g_odoEngine.snapshot();
    if (!g_storage.saveOdometerCheckpoint(state.odometerKm, state.lastPid31,
                                          state.hasPid31Baseline, state.fractionalKm)) {
        Serial.println(F("[STORAGE] Checkpoint write failed; retry remains pending."));
        return false;
    }
    g_odoEngine.markPersisted(state);
    g_storedPidBaseline=state.hasPid31Baseline;
    return true;
}

void recordService(const std::string& id) {
    MaintenanceEngine pending=g_maintEngine;
    ServiceHistoryRecord rec;
    if (!pending.resetItem(id, g_odoEngine.getCurrentOdo(), 0, "", rec)) return;
    if(!g_storage.saveItems(pending.getItems())) {
        Serial.println(F("[SERVICE] Save failed; service not confirmed."));
        playChime(400,150);return;
    }
    if(!g_storage.appendHistoryLog(rec)) {
        const bool reverted=g_storage.saveItems(g_maintEngine.getItems());
        Serial.printf("[SERVICE] History failed; rollback=%d; service not confirmed.\n",reverted);
        if(!reverted) {
            g_storage.loadItems(g_maintEngine.getItems());
            g_maintEngine.evaluate(g_odoEngine.getCurrentOdo(),0);
            g_screenDirty=true;
        }
        playChime(400,150);return;
    }
    g_maintEngine=std::move(pending);
    const bool odoOk=persistCheckpoint();
    Serial.printf("[SERVICE] id=%s history=1 items=1 checkpoint=%d\n",id.c_str(),odoOk);
    g_display.closeModal();g_screenDirty=true;playChime(1500,150);
}

void snapshotScreen() {
    // Read actual controller GRAM, not a host rendering or reconstructed framebuffer.
    g_obdTask.setDiagnosticsMuted(true);
    uint8_t row[240 * 3];
    Serial.println(F("SNAPSHOT_RGB888 240 320 230400"));
    Serial.flush();
    for (int y=0; y<320; ++y) {
        g_tft.readRectRGB(0,y,240,1,row);
        Serial.write(row,sizeof(row));
        delay(1);
    }
    Serial.println(F("\nSNAPSHOT_END"));
    Serial.flush();
    g_obdTask.setDiagnosticsMuted(false);
}

void processCommand(const std::string& cmd) {
    if(cmd=="CMD:DISPLAY_INVERT 0") {g_tft.invertDisplay(false);return;}
    if(cmd=="CMD:DISPLAY_INVERT 1") {g_tft.invertDisplay(true);return;}
    if(cmd=="CMD:THEME DAY") {g_display.setNight(false);g_screenDirty=true;return;}
    if(cmd=="CMD:THEME NIGHT") {g_display.setNight(true);g_screenDirty=true;return;}
    if(cmd=="CMD:HOTSPOT ON") {if(g_hotspotState==0||g_hotspotState==4)toggleHotspot();return;}
    if(cmd=="CMD:HOTSPOT OFF") {if(g_hotspotState==2)toggleHotspot();return;}
    if(cmd=="CMD:HOTSPOT_STATUS") {Serial.printf("[HOTSPOT] state=%u paused=%d ssid=CivicHUD ip=%s heap=%u\n",g_hotspotState,g_obdTask.configurationPaused(),WiFi.softAPIP().toString().c_str(),ESP.getFreeHeap());return;}
    if(cmd=="CMD:MONITOR") { g_monitor.print();return; }
    if(cmd=="CMD:MONITOR_SCREEN") { g_monitorScreen=!g_monitorScreen;g_screenDirty=true;return; }
    if(cmd.rfind("CMD:OBD_QUERY ",0)==0) {g_obdTask.requestQuery(cmd.substr(14));return;}
    if(cmd=="CMD:OBD_STATUS") {
        const auto t=g_obdTask.getTelemetry();const auto o=g_odoEngine.snapshot();
        Serial.printf("[OBD_STATUS] BT=%d ECU=%d SPEED=%.0f PID31=%u HAS31=%d SUPPORTED31=%d VOLTAGE=%.2f AGE_MS=%u ODO=%u FRACTION=%.6f\n",t.btConnected,t.obdConnected,t.speedKmh,t.currentPid31Km,t.hasPid31,t.pid31Supported,t.batteryVoltage,t.lastPacketTimestampMs?millis()-t.lastPacketTimestampMs:UINT32_MAX,o.odometerKm,o.fractionalKm);return;
    }
    if(cmd.rfind("CMD:ODO_SYNC ",0)==0) {
        unsigned long odo=0,pid=0;
        if(sscanf(cmd.c_str()+13,"%lu %lu",&odo,&pid)==2 && odo>0 && odo<=1000000 && pid<=65535) {
            g_odoEngine.restore(OdometerSnapshot{(uint32_t)odo,(uint32_t)pid,true,0});
            bool saved=persistCheckpoint();g_screenDirty=true;
            Serial.printf("[SYNCED] reference_odo=%lu reference_pid=%lu saved=%d\n",odo,pid,saved);
        }
        return;
    }
    if(cmd.rfind("CMD:ODO_CALIBRATE ",0)==0) {
        uint32_t value=0;
        if(WebSettings::parseUint(cmd.substr(18),value)&&value>0) {
            g_odoEngine.calibrate(value);g_odoEngine.resetPid31Baseline();
            const bool saved=persistCheckpoint();g_screenDirty=true;
            Serial.printf("[CALIBRATED] ODO=%u saved=%d\n",value,saved);
        }
        return;
    }
    if(cmd=="CMD:BT_SCAN_CONNECT") {
        if(g_hotspotState!=0&&g_hotspotState!=4){Serial.println("[BT_DIAG] Close hotspot first");return;}
        WiFi.mode(WIFI_OFF);
        Serial.printf("[BT_DIAG] Wi-Fi paused; free_heap=%u\n",ESP.getFreeHeap());
        g_obdTask.requestScanConnect();Serial.println(F("[BT_SCAN_QUEUED]"));return;
    }
    if(cmd=="CMD:READ_TEST") {
        const uint16_t colors[]={TFT_RED,TFT_GREEN,TFT_BLUE,TFT_WHITE};
        for(int i=0;i<4;++i){g_tft.fillRect(i*20,0,20,20,colors[i]);uint8_t rgb[3];g_tft.readRectRGB(i*20+10,10,1,1,rgb);
            Serial.printf("[PIXEL] expected=%04x read=%04x rgb=%u,%u,%u\n",colors[i],g_tft.readPixel(i*20+10,10),rgb[0],rgb[1],rgb[2]);}
        g_display.invalidate();g_screenDirty=true;return;
    }
    if(cmd=="CMD:WEB_PAGE") {
        g_obdTask.setDiagnosticsMuted(true);
        std::string name,pin;g_storage.loadBtConfig(name,pin);
        Serial.println(F("[WEB_PAGE_BEGIN]"));
        WebSettings::ChunkedHtmlBuffer buffer([](const char* bytes,size_t n){Serial.write((const uint8_t*)bytes,n);});
        std::ostream output(&buffer);
        WebSettings::renderHtmlTo(output,g_odoEngine.getCurrentOdo(),name,pin,g_maintEngine.getItems(),g_storage.loadHistoryLogs());
        output.flush();Serial.println(F("\n[WEB_PAGE_END]"));Serial.flush();
        g_obdTask.setDiagnosticsMuted(false);return;
    }
    if (cmd == "CMD:SNAPSHOT") { snapshotScreen(); return; }
    if (cmd == "CMD:REBOOT") { persistCheckpoint(); Serial.flush(); ESP.restart(); return; }
    if (cmd.rfind("CMD:UI_PAGE ",0)==0) { g_display.setPage(atoi(cmd.c_str()+12)); g_screenDirty=true; return; }
    if (cmd.rfind("CMD:UI_OPEN ",0)==0) { g_display.openItem(cmd.substr(12)); g_screenDirty=true; return; }
    if (cmd.rfind("CMD:UI_SLIDER ",0)==0) { g_display.setSliderForPreview(atoi(cmd.c_str()+14)); g_screenDirty=true; return; }
    if (cmd.rfind("CMD:SERVICE ",0)==0) { recordService(cmd.substr(12)); return; }
    if (cmd.rfind("CMD:TOUCH ",0)==0) {
        int x=0,y=0,down=0;
        if (sscanf(cmd.c_str()+10,"%d %d %d",&x,&y,&down)==3) {
            const auto sorted=g_maintEngine.getSortedItems();
            const auto id=g_display.touch(down!=0,x,y,sorted,0);
            if(!id.empty()) recordService(id);
            g_screenDirty=true;
        }
        return;
    }
    if (cmd == "CMD:STATE") {
        const auto state=g_odoEngine.snapshot();
        Serial.printf("[STATE] ODO=%u PID=%u VALID=%d FRACTION=%.6f PAGE=%u ITEMS=%u\n",
                      state.odometerKm,state.lastPid31,state.hasPid31Baseline,state.fractionalKm,
                      (unsigned)g_display.page(),(unsigned)g_maintEngine.getItems().size());
        for(const auto& it:g_maintEngine.getItems()) {
            Serial.printf("[ITEM] %s|%u|%u|%d|%s|%s\n",it.id.c_str(),it.intervalKm,it.lastKm,it.visible,it.lastDate.c_str(),it.name.c_str());
        }
        Serial.println(F("[STATE_END]")); return;
    }
    WebSettings::handleSerialCommand(cmd,g_odoEngine,g_maintEngine,g_storage,g_obdTask,g_screenDirty);
}

void loop() {
    if(g_hotspotState==2)g_server.handleClient();
    updateHotspot();
    static std::string command;
    while (Serial.available()) {
        const char c=static_cast<char>(Serial.read());
        if(c=='\n') { processCommand(command); command.clear(); }
        else if(c!='\r') {
            if(command.size()<2048) command.push_back(c);
            else { command.clear(); Serial.println(F("[CMD] Input too long")); }
        }
    }
    g_obdTask.update();
    handleTouch();
    g_monitor.update();
    const uint32_t now=millis();
    if(g_screenDirty || g_display.dirty() || now-g_lastUiRefresh>=800) {
        g_lastUiRefresh=now;
        g_maintEngine.evaluate(g_odoEngine.getCurrentOdo(),0);
        const auto telemetry=g_obdTask.getTelemetry();
        if(g_monitorScreen) g_display.renderMonitor(g_monitor,g_hotspotState,telemetry.speedKmh>0.5f&&millis()-telemetry.lastPacketTimestampMs<10000);
        else g_display.render(g_maintEngine.getSortedItems(),g_odoEngine.getCurrentOdo(),
                         telemetry.obdConnected,telemetry.lastPacketTimestampMs?telemetry.batteryVoltage:0,"");
        g_screenDirty=false;
    }
    if(g_odoEngine.persistenceDue(1) || (!g_storedPidBaseline && g_odoEngine.hasPid31Baseline())) persistCheckpoint();
    updateRgbStatus();
    updateAutoDimming();
    delay(10);
}

// =========================================================================
// Hardware Initialization
// =========================================================================
void initHardware() {
    // RGB LED (Active-Low)
    pinMode(RGB_LED_R, OUTPUT);
    pinMode(RGB_LED_G, OUTPUT);
    pinMode(RGB_LED_B, OUTPUT);
    digitalWrite(RGB_LED_R, HIGH);
    digitalWrite(RGB_LED_G, HIGH);
    digitalWrite(RGB_LED_B, HIGH);

    // Audio Speaker
    pinMode(AUDIO_PIN, OUTPUT);
    digitalWrite(AUDIO_PIN, LOW);

    // LDR Light Sensor
    pinMode(LDR_SENSOR_PIN, INPUT);

    // Backlight PWM (CYD TFT_BL)
    ledcAttach(TFT_BL, 5000, 8);
    ledcWrite(TFT_BL, 240); // 95% brightness

    // TFT LCD Initialization
    g_tft.init();
    g_tft.setRotation(0); // 0 = Portrait (240x320)
    // This CYD panel displays complementary colours without inversion.
    g_tft.invertDisplay(true);
    g_tft.fillScreen(C_BG);

    // Touch Controller Initialization
    g_touchSpi.begin(XPT2046_CLK, XPT2046_DO, XPT2046_DIN, XPT2046_CS);
    g_touch.begin(g_touchSpi);
    // Keep native touch axes; mapPortraitTouch applies the portrait transform once.
    g_touch.setRotation(1);
}

void playChime(uint16_t freq, uint16_t durationMs) {
    tone(AUDIO_PIN, freq, durationMs);
}

void updateRgbStatus() {
    AlertStatus st = g_maintEngine.getHighestAlertStatus();
    if (st == AlertStatus::OVERDUE) {
        digitalWrite(RGB_LED_R, LOW);
        digitalWrite(RGB_LED_G, HIGH);
        digitalWrite(RGB_LED_B, HIGH);
    } else if (st == AlertStatus::DUE_SOON) {
        digitalWrite(RGB_LED_R, LOW);
        digitalWrite(RGB_LED_G, LOW);
        digitalWrite(RGB_LED_B, HIGH);
    } else {
        digitalWrite(RGB_LED_R, HIGH);
        digitalWrite(RGB_LED_G, LOW);
        digitalWrite(RGB_LED_B, HIGH);
    }
}

void updateAutoDimming() {
    static uint32_t lastDimCheck = 0;
    if (millis() - lastDimCheck < 1000) return;
    lastDimCheck = millis();

    int raw = analogRead(LDR_SENSOR_PIN);
    int minimum=g_display.isNight()?30:130, maximum=g_display.isNight()?90:255;
    int b = map(raw, 300, 3600, minimum, maximum);
    b = constrain(b, minimum, maximum);
    ledcWrite(TFT_BL, b);
}

// UI uses the production controller shared with host gesture/pagination tests.
void handleTouch() {
    const bool down=g_touch.touched();
    static bool wasDown=false;
    if (!down && !wasDown) return;
    int x=0,y=0;
    if(down) {
        const auto point=g_touch.getPoint();
        const auto mapped=mapPortraitTouch(point.x,point.y);
        x=mapped.x; y=mapped.y;
    }
    if(down&&!wasDown&&y<32){g_display.setNight(!g_display.isNight());g_screenDirty=true;wasDown=down;return;}
    if(down&&!wasDown&&y>=292&&x>=80&&x<=160&&g_display.activeId().empty()) {
        g_monitorScreen=!g_monitorScreen;g_screenDirty=true;wasDown=down;return;
    }
    if(g_monitorScreen) {
        if(down&&!wasDown&&hudHotspotHit(x,y))toggleHotspot();
        if(down&&!wasDown&&y>=292){g_monitorScreen=false;g_screenDirty=true;}
        wasDown=down;return;
    }
    const auto telemetry=g_obdTask.getTelemetry();
    const auto id=g_display.touch(down,x,y,g_maintEngine.getSortedItems(),telemetry.speedKmh);
    wasDown=down;
    if(!id.empty()) recordService(id);
}

// =========================================================================
// Web Server (http://192.168.1.1) Configuration AP
// =========================================================================
void toggleHotspot(){
    if(g_hotspotState==2){g_server.stop();WiFi.softAPdisconnect(true);WiFi.mode(WIFI_OFF);g_obdTask.requestConfigurationPause(false);g_hotspotState=3;g_hotspotRequestedMs=millis();}
    else if(g_hotspotState==0||g_hotspotState==4){
        auto t=g_obdTask.getTelemetry();if(t.lastPacketTimestampMs&&millis()-t.lastPacketTimestampMs<10000&&t.speedKmh>0.5f){Serial.println("[HOTSPOT] Park before opening settings");return;}
        if(!persistCheckpoint()){g_hotspotState=4;g_screenDirty=true;return;}
        g_obdTask.requestConfigurationPause(true);g_hotspotState=1;g_hotspotRequestedMs=millis();
    }
    g_screenDirty=true;
}
void updateHotspot(){
    if(g_hotspotState==1){
        if(g_obdTask.configurationPaused()){
            if(setupWebServer())g_hotspotState=2;
            else{g_server.stop();WiFi.mode(WIFI_OFF);g_obdTask.requestConfigurationPause(false);g_hotspotState=4;}
            g_screenDirty=true;
        }else if(millis()-g_hotspotRequestedMs>25000){g_obdTask.requestConfigurationPause(false);g_hotspotState=4;g_screenDirty=true;}
    }else if(g_hotspotState==3&&!g_obdTask.configurationPaused()){g_hotspotState=0;g_screenDirty=true;}
}
bool setupWebServer() {
    IPAddress local_ip(192,168,1,1),gateway(192,168,1,1),subnet(255,255,255,0);
    if(!WiFi.mode(WIFI_AP)||!WiFi.softAPConfig(local_ip,gateway,subnet)||!WiFi.softAP(WIFI_AP_SSID,WIFI_AP_PASS))return false;
    if(!g_webRoutesReady){WebSettings::setupWebServer(g_server,g_odoEngine,g_maintEngine,g_storage,g_obdTask,g_screenDirty,playChime);g_webRoutesReady=true;}
    g_server.begin();
    Serial.printf("[WEB] CivicHUD active http://192.168.1.1 heap=%u\n",ESP.getFreeHeap());
    return true;
}
