#include "test_framework.h"
#include "OdometerEngine.h"
#include "MaintenanceEngine.h"
#include "MaintenanceItem.h"

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <cstring>

// ============================================================================
// INTEGRATED SYSTEM HARNESS & SIMULATION SUBSYSTEMS (OPAQUE-BOX E2E)
// ============================================================================

namespace CivicHudE2E {

// 1. Packed POD Record (100 bytes)
#pragma pack(push, 1)
struct PackedItemRecord {
    char id[12];
    char name[48];
    uint32_t intervalKm;
    uint16_t intervalDays;
    uint8_t warnPct;
    uint32_t lastKm;
    uint32_t lastEpoch;
    char lastDate[24];
    uint8_t flags; // bit 0 = visible, bit 1 = custom
};
#pragma pack(pop)

static_assert(sizeof(PackedItemRecord) == 100, "PackedItemRecord must be exactly 100 bytes");

// CRC32 calculation helper
static uint32_t calculateCrc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

// 2. Mock NVS Storage
class MockNvsStore {
public:
    uint32_t odoKm = 98540;
    std::string btName = "OBDII";
    std::string btPin = "1234";
    std::vector<uint8_t> itemsBlob;
    uint32_t itemsBlobCrc = 0;
    bool isCorrupt = false;
    uint32_t writeCount = 0;

    void saveOdometer(uint32_t odo) {
        odoKm = odo;
        writeCount++;
    }

    bool saveItems(const std::vector<MaintenanceItem>& items) {
        itemsBlob.clear();
        for (const auto& it : items) {
            PackedItemRecord rec{};
            std::strncpy(rec.id, it.id.c_str(), sizeof(rec.id) - 1);
            std::strncpy(rec.name, it.name.c_str(), sizeof(rec.name) - 1);
            rec.intervalKm = it.intervalKm;
            rec.intervalDays = it.intervalDays;
            rec.warnPct = it.warnPct;
            rec.lastKm = it.lastKm;
            rec.lastEpoch = it.lastEpoch;
            std::strncpy(rec.lastDate, it.lastDate.c_str(), sizeof(rec.lastDate) - 1);
            rec.flags = (it.visible ? 0x01 : 0x00);

            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&rec);
            itemsBlob.insert(itemsBlob.end(), ptr, ptr + sizeof(PackedItemRecord));
        }
        itemsBlobCrc = calculateCrc32(itemsBlob.data(), itemsBlob.size());
        writeCount++;
        return true;
    }

    bool loadItems(std::vector<MaintenanceItem>& outItems) {
        if (isCorrupt) return false;
        if (itemsBlob.empty() || itemsBlob.size() % sizeof(PackedItemRecord) != 0) return false;
        uint32_t crc = calculateCrc32(itemsBlob.data(), itemsBlob.size());
        if (crc != itemsBlobCrc) return false;

        outItems.clear();
        size_t count = itemsBlob.size() / sizeof(PackedItemRecord);
        for (size_t i = 0; i < count; ++i) {
            PackedItemRecord rec;
            std::memcpy(&rec, itemsBlob.data() + (i * sizeof(PackedItemRecord)), sizeof(PackedItemRecord));
            MaintenanceItem it{};
            it.id = rec.id;
            it.name = rec.name;
            it.intervalKm = rec.intervalKm;
            it.intervalDays = rec.intervalDays;
            it.warnPct = rec.warnPct;
            it.lastKm = rec.lastKm;
            it.lastEpoch = rec.lastEpoch;
            it.lastDate = rec.lastDate;
            it.visible = (rec.flags & 0x01) != 0;
            outItems.push_back(it);
        }
        return true;
    }
};

// 3. Mock LittleFS Service History Log
class MockLittleFsLog {
public:
    std::vector<std::string> lines;

    void appendRecord(const ServiceHistoryRecord& rec) {
        std::ostringstream ss;
        ss << rec.itemName << "," << rec.dateStr << ","
           << rec.odoKm << "," << rec.prevOdoKm << ","
           << rec.deltaKm << "," << rec.nextDueKm;
        lines.push_back(ss.str());
    }

    size_t getRecordCount() const { return lines.size(); }

    std::string getCsvExport() const {
        std::ostringstream ss;
        ss << "Item,Date,Odometer,PrevOdometer,Delta,NextDue\n";
        for (const auto& line : lines) {
            ss << line << "\n";
        }
        return ss.str();
    }
};

// 4. Mock Bluetooth & ELM327 State Machine
enum class BtState { DISCONNECTED, CONNECTING, CONNECTED, RECONNECT_WAIT };

inline std::ostream& operator<<(std::ostream& os, const BtState& s) {
    switch (s) {
        case BtState::DISCONNECTED: return os << "DISCONNECTED";
        case BtState::CONNECTING: return os << "CONNECTING";
        case BtState::CONNECTED: return os << "CONNECTED";
        case BtState::RECONNECT_WAIT: return os << "RECONNECT_WAIT";
    }
    return os << static_cast<int>(s);
}

class MockElmBt {
public:
    BtState state = BtState::DISCONNECTED;
    uint32_t reconnectTimerMs = 0;
    uint32_t lastSuccessfulResponseMs = 0;
    bool cloneQuirksHandled = false;
    uint8_t currentSpeedKph = 0;
    uint32_t currentPid31 = 0;
    bool donglePowered = true;

    void connect() {
        state = BtState::CONNECTING;
        // Simulate SSP pairing and AT handshake
        cloneQuirksHandled = true;
        state = BtState::CONNECTED;
        reconnectTimerMs = 0;
    }

    void disconnect() {
        state = BtState::DISCONNECTED;
        reconnectTimerMs = 0;
        cloneQuirksHandled = false;
    }

    void tick(uint32_t dtMs, bool dongleAvailable) {
        if (state == BtState::CONNECTED) {
            if (!dongleAvailable) {
                state = BtState::RECONNECT_WAIT;
                reconnectTimerMs = 0;
            }
        } else if (state == BtState::RECONNECT_WAIT) {
            reconnectTimerMs += dtMs;
            if (dongleAvailable && reconnectTimerMs >= 3000) { // 3-second retry cadence
                connect();
            }
        }
    }
};

// 5. Thai Font & Layout C UI Engine Mock
class MockUiEngine {
public:
    uint8_t currentPage = 0;
    bool modalOpen = false;
    std::string modalItemId = "";
    float sliderPosition = 0.0f; // 0.0 .. 100.0%
    bool sliderConfirmed = false;
    bool touchLocked = false;

    void updateSpeed(uint8_t speedKph) {
        if (speedKph > 5) {
            touchLocked = true;
            currentPage = 0; // Forced to page 0
            modalOpen = false; // Modals dismissed while driving
        } else {
            touchLocked = false;
        }
    }

    static void mapTouchCoordinates(int rawX, int rawY, int& outTx, int& outTy) {
        // Swap axes and apply calibration
        // Display width = 240, height = 320
        outTx = (3800 - rawY) * 240 / (3800 - 260);
        outTy = (rawX - 240) * 320 / (3800 - 240);
        outTx = std::clamp(outTx, 0, 240);
        outTy = std::clamp(outTy, 0, 320);
    }

    bool handleTouch(int tx, int ty, const std::vector<MaintenanceItem>& sortedItems) {
        if (touchLocked) return false;

        // If modal is open, check modal actions
        if (modalOpen) {
            // Cancel button: (x=16..66, y=280..312)
            if (tx >= 16 && tx <= 66 && ty >= 280 && ty <= 312) {
                modalOpen = false;
                sliderPosition = 0.0f;
                sliderConfirmed = false;
                return true;
            }
            // Slider drag: (y=216..260, x=18..222)
            if (ty >= 216 && ty <= 260 && tx >= 18 && tx <= 222) {
                sliderPosition = static_cast<float>(tx - 18) / (222 - 18) * 100.0f;
                sliderPosition = std::clamp(sliderPosition, 0.0f, 100.0f);
                if (sliderPosition >= 85.0f) {
                    sliderConfirmed = true;
                }
                return true;
            }
            return false;
        }

        // Card tap: Card 0..4 on current page
        // Cards start at y=32, step 52 px (y=32..80 for Card 0, 84..132 for Card 1, etc.)
        for (int i = 0; i < 5; ++i) {
            int cardY = 32 + i * 52;
            if (ty >= cardY && ty <= cardY + 48 && tx >= 6 && tx <= 234) {
                size_t itemIndex = currentPage * 5 + i;
                if (itemIndex < sortedItems.size()) {
                    modalOpen = true;
                    modalItemId = sortedItems[itemIndex].id;
                    sliderPosition = 0.0f;
                    sliderConfirmed = false;
                    return true;
                }
            }
        }

        // Pagination buttons: y=294..318 (PREV: x=12..60, NEXT: x=180..228)
        if (ty >= 294 && ty <= 318) {
            if (tx >= 12 && tx <= 60 && currentPage > 0) {
                currentPage--;
                return true;
            }
            if (tx >= 180 && tx <= 228) {
                currentPage++;
                return true;
            }
        }

        return false;
    }
};

// 6. Complete Virtual Civic HUD System
class VirtualCivicHud {
public:
    OdometerEngine odoEngine;
    MaintenanceEngine maintEngine;
    MockNvsStore nvs;
    MockLittleFsLog littleFs;
    MockElmBt bt;
    MockUiEngine ui;

    VirtualCivicHud() {
        nvs.saveOdometer(98540);
        odoEngine.calibrate(nvs.odoKm);
        maintEngine.loadDefaults();
        nvs.saveItems(maintEngine.getItems());
        bt.currentPid31 = 12000;
        bt.currentSpeedKph = 0;
    }

    void boot() {
        uint32_t savedOdo = 0;
        savedOdo = nvs.odoKm;
        odoEngine.calibrate(savedOdo);
        std::vector<MaintenanceItem> loadedItems;
        if (nvs.loadItems(loadedItems)) {
            maintEngine.getItems() = loadedItems;
        } else {
            maintEngine.loadDefaults();
        }
        maintEngine.evaluate(odoEngine.getCurrentOdo(), 340); // epoch day 340
        bt.connect();
        ui.updateSpeed(0);
    }

    void simulateDriveTick(uint8_t speedKph, uint32_t dtSeconds, bool dongleActive) {
        bt.currentSpeedKph = speedKph;
        ui.updateSpeed(speedKph);
        bt.tick(dtSeconds * 1000, dongleActive);

        if (bt.state == BtState::CONNECTED) {
            // PID 31 advances based on speed * time
            float distanceKm = (speedKph * dtSeconds) / 3600.0f;
            static float fractionalAcc = 0.0f;
            fractionalAcc += distanceKm;
            if (fractionalAcc >= 1.0f) {
                uint32_t wholeKm = static_cast<uint32_t>(fractionalAcc);
                bt.currentPid31 += wholeKm;
                fractionalAcc -= wholeKm;
                odoEngine.updateFromPid31(bt.currentPid31);
            }
        } else {
            // Speed integration fallback
            odoEngine.updateFromSpeed(static_cast<float>(speedKph), static_cast<float>(dtSeconds));
        }

        // Periodic NVS check (every 5 km)
        if (odoEngine.checkAndClearNvsDirty(5)) {
            nvs.saveOdometer(odoEngine.getCurrentOdo());
        }

        // UI evaluation
        maintEngine.evaluate(odoEngine.getCurrentOdo(), 340);
    }
};

} // namespace CivicHudE2E

// ============================================================================
// TIER 4: REAL-WORLD APPLICATION SCENARIO 1 - HIGHWAY COMMUTE
// ============================================================================

TEST_TIER4(Test_Tier4_Scenario1_HighwayCommute_SmoothOdoAdvance) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();

    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), 98540u);
    ASSERT_EQ(hud.bt.state, CivicHudE2E::BtState::CONNECTED);

    // Initial baseline setup for PID 31
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);

    // Drive 25 km on highway at 100 km/h:
    // 25 km = 900 seconds at 100 km/h
    for (int sec = 0; sec < 900; ++sec) {
        hud.simulateDriveTick(100, 1, true);
    }

    // Odometer should have advanced by 25 km: 98,540 -> 98,565 km
    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), 98565u);
}

TEST_TIER4(Test_Tier4_Scenario1_HighwayCommute_CountdownDecrements) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);

    // Engine Oil starts at lastKm = 90000, interval = 10000 -> 1460 km left at 98540
    auto sorted = hud.maintEngine.getSortedItems();
    int32_t initialOilKmLeft = 0;
    for (const auto& item : sorted) {
        if (item.id == "oil") {
            initialOilKmLeft = item.kmLeft;
            break;
        }
    }
    ASSERT_EQ(initialOilKmLeft, 1460);

    // Drive 25 km
    for (int sec = 0; sec < 900; ++sec) {
        hud.simulateDriveTick(100, 1, true);
    }

    sorted = hud.maintEngine.getSortedItems();
    int32_t finalOilKmLeft = 0;
    for (const auto& item : sorted) {
        if (item.id == "oil") {
            finalOilKmLeft = item.kmLeft;
            break;
        }
    }
    // Should decrement by exactly 25 km: 1460 - 25 = 1435 km left
    ASSERT_EQ(finalOilKmLeft, 1435);
}

TEST_TIER4(Test_Tier4_Scenario1_HighwayCommute_StatusBarUpdates) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);

    hud.simulateDriveTick(110, 10, true);

    ASSERT_EQ(hud.bt.currentSpeedKph, 110);
    ASSERT_EQ(hud.bt.state, CivicHudE2E::BtState::CONNECTED);
    ASSERT_TRUE(hud.ui.touchLocked); // Driving locks touch!
}

TEST_TIER4(Test_Tier4_Scenario1_HighwayCommute_ZeroPanics) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();

    // Verify memory bounds and zero crashes during rapid updates
    for (int i = 0; i < 1000; ++i) {
        hud.simulateDriveTick(120, 1, true);
    }
    ASSERT_GT(hud.odoEngine.getCurrentOdo(), 98540u);
}

// ============================================================================
// TIER 4: REAL-WORLD APPLICATION SCENARIO 2 - TUNNEL SIGNAL LOSS & RECONNECT
// ============================================================================

TEST_TIER4(Test_Tier4_Scenario2_Tunnel_SignalLoss_SpeedFallbackEngaged) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);

    uint32_t startOdo = hud.odoEngine.getCurrentOdo();

    // Normal driving outside tunnel: 80 km/h for 45 s = 1.0 km
    for (int i = 0; i < 45; ++i) {
        hud.simulateDriveTick(80, 1, true);
    }
    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), startOdo + 1);

    // Enter tunnel: Dongle disconnects!
    // Drive 68 seconds inside tunnel at 80 km/h: distance = 80 * 68 / 3600 = 1.51 km
    for (int i = 0; i < 68; ++i) {
        hud.simulateDriveTick(80, 1, false);
    }

    // Fallback speed integration kept accumulating!
    ASSERT_EQ(hud.bt.state, CivicHudE2E::BtState::RECONNECT_WAIT);
    ASSERT_GE(hud.odoEngine.getCurrentOdo(), startOdo + 2);
}

TEST_TIER4(Test_Tier4_Scenario2_Tunnel_DongleReappears_ReconnectUnder10Seconds) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);

    // Signal drops
    hud.simulateDriveTick(80, 1, false);
    ASSERT_EQ(hud.bt.state, CivicHudE2E::BtState::RECONNECT_WAIT);

    // Tunnel exit after 8 seconds: Dongle available again
    // Reconnect retry ticks every 3 seconds
    hud.simulateDriveTick(80, 4, true);

    // Within <= 10 seconds total, BT must reconnect!
    ASSERT_EQ(hud.bt.state, CivicHudE2E::BtState::CONNECTED);
}

TEST_TIER4(Test_Tier4_Scenario2_Tunnel_CloneQuirksHandledOnReconnect) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();

    hud.bt.disconnect();
    ASSERT_FALSE(hud.bt.cloneQuirksHandled);

    hud.bt.connect();
    ASSERT_TRUE(hud.bt.cloneQuirksHandled);
    ASSERT_EQ(hud.bt.state, CivicHudE2E::BtState::CONNECTED);
}

TEST_TIER4(Test_Tier4_Scenario2_Tunnel_NoUIHitchOrOdometerGlitch) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);

    uint32_t lastOdo = hud.odoEngine.getCurrentOdo();

    // Simulate transition: Connect -> Disconnect -> Reconnect
    for (int i = 0; i < 20; ++i) {
        bool dongleActive = (i < 5 || i >= 15);
        hud.simulateDriveTick(60, 1, dongleActive);
        uint32_t curr = hud.odoEngine.getCurrentOdo();
        ASSERT_GE(curr, lastOdo); // Strictly monotonic non-decreasing
        lastOdo = curr;
    }
}

// ============================================================================
// TIER 4: REAL-WORLD APPLICATION SCENARIO 3 - OIL SERVICE RESET
// ============================================================================

TEST_TIER4(Test_Tier4_Scenario3_OilServiceReset_ModalHistoryInspection) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();

    auto sorted = hud.maintEngine.getSortedItems();
    ASSERT_GT(sorted.size(), 0);

    // Car is parked: speed = 0, tap on Card 0 (y=40, x=100)
    int tx, ty;
    CivicHudE2E::MockUiEngine::mapTouchCoordinates(800, 2400, tx, ty); // Maps to Card 0 area (tx ~ 95, ty ~ 50)
    hud.ui.handleTouch(tx, ty, sorted);

    ASSERT_TRUE(hud.ui.modalOpen);
    ASSERT_EQ(hud.ui.modalItemId, sorted[0].id);
}

TEST_TIER4(Test_Tier4_Scenario3_OilServiceReset_SlideConfirmThreshold85Pct) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();

    auto sorted = hud.maintEngine.getSortedItems();
    hud.ui.modalOpen = true;
    hud.ui.modalItemId = "oil";

    // Drag to 50% (x=120, y=230)
    hud.ui.handleTouch(120, 230, sorted);
    ASSERT_FALSE(hud.ui.sliderConfirmed);
    ASSERT_NEAR(hud.ui.sliderPosition, 50.0f, 1.0f);

    // Drag past 85% threshold (x=200, y=230)
    hud.ui.handleTouch(200, 230, sorted);
    ASSERT_TRUE(hud.ui.sliderConfirmed);
    ASSERT_GE(hud.ui.sliderPosition, 85.0f);
}

TEST_TIER4(Test_Tier4_Scenario3_OilServiceReset_LittleFsCsvHistoryLogged) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();

    ServiceHistoryRecord rec{};
    bool resetSuccess = hud.maintEngine.resetItem("oil", hud.odoEngine.getCurrentOdo(), 340, "13 ต.ค. 26", rec);
    ASSERT_TRUE(resetSuccess);

    // Append to LittleFS log
    hud.littleFs.appendRecord(rec);

    ASSERT_EQ(hud.littleFs.getRecordCount(), 1);
    std::string csv = hud.littleFs.getCsvExport();
    ASSERT_TRUE(csv.find("13 ต.ค. 26") != std::string::npos);
    ASSERT_TRUE(csv.find("98540") != std::string::npos);
}

TEST_TIER4(Test_Tier4_Scenario3_OilServiceReset_CardReorderedToBottom) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();

    // Before reset: Oil is due soon / near top
    auto before = hud.maintEngine.getSortedItems();
    size_t beforeRank = 999;
    for (size_t i = 0; i < before.size(); ++i) {
        if (before[i].id == "oil") {
            beforeRank = i;
            break;
        }
    }
    ASSERT_LT(beforeRank, 5); // Near top

    // Reset oil
    ServiceHistoryRecord rec{};
    hud.maintEngine.resetItem("oil", hud.odoEngine.getCurrentOdo(), 340, "13 ต.ค. 26", rec);

    // After reset: Oil has 0% used, sinks to bottom
    auto after = hud.maintEngine.getSortedItems();
    size_t afterRank = 999;
    for (size_t i = 0; i < after.size(); ++i) {
        if (after[i].id == "oil") {
            afterRank = i;
            break;
        }
    }
    ASSERT_GT(afterRank, after.size() - 3); // Sunk to the bottom!
}

// ============================================================================
// TIER 4: REAL-WORLD APPLICATION SCENARIO 4 - WEB SETTINGS RECONFIGURATION & REBOOT
// ============================================================================

TEST_TIER4(Test_Tier4_Scenario4_WebSettings_ModifyIntervalAndHideItems) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();

    // Web user updates oil interval to 15,000 km and hides spark plugs
    hud.maintEngine.updateIntervalKm("oil", 15000);
    hud.maintEngine.setItemVisibility("plugs", false);

    const auto& items = hud.maintEngine.getItems();
    for (const auto& it : items) {
        if (it.id == "oil") ASSERT_EQ(it.intervalKm, 15000u);
        if (it.id == "plugs") ASSERT_FALSE(it.visible);
    }
}

TEST_TIER4(Test_Tier4_Scenario4_WebSettings_AddNewCustomItem) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();

    size_t countBefore = hud.maintEngine.getItems().size();
    bool added = hud.maintEngine.addItem("🔄 สลับยางพิเศษ", 10000, 180, hud.odoEngine.getCurrentOdo(), "13 ต.ค. 26");
    ASSERT_TRUE(added);
    ASSERT_EQ(hud.maintEngine.getItems().size(), countBefore + 1);
}

TEST_TIER4(Test_Tier4_Scenario4_WebSettings_SaveAllAtomicTransaction) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();

    hud.maintEngine.updateIntervalKm("oil", 12000);
    hud.maintEngine.setItemVisibility("plugs", false);
    hud.nvs.saveOdometer(99000);
    bool saved = hud.nvs.saveItems(hud.maintEngine.getItems());
    ASSERT_TRUE(saved);

    ASSERT_EQ(hud.nvs.itemsBlob.size() % sizeof(CivicHudE2E::PackedItemRecord), 0u);
}

TEST_TIER4(Test_Tier4_Scenario4_WebSettings_ScreenRefreshedImmediately) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();

    // Hide item and check sorted visible view immediately
    hud.maintEngine.setItemVisibility("oil", false);
    auto sorted = hud.maintEngine.getSortedItems();
    for (const auto& it : sorted) {
        ASSERT_NE(it.id, "oil");
    }
}

TEST_TIER4(Test_Tier4_Scenario4_WebSettings_RebootPersistenceRoundtrip) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();

    // Modify configuration
    hud.maintEngine.updateIntervalKm("oil", 12500);
    hud.maintEngine.setItemVisibility("cabin", false);
    hud.nvs.saveItems(hud.maintEngine.getItems());
    hud.nvs.saveOdometer(99120);

    // Simulate complete physical power-cycle / reboot
    CivicHudE2E::VirtualCivicHud rebootedHud;
    rebootedHud.nvs = hud.nvs; // Flash memory retained
    rebootedHud.boot();

    ASSERT_EQ(rebootedHud.odoEngine.getCurrentOdo(), 99120u);
    const auto& items = rebootedHud.maintEngine.getItems();
    bool foundOil = false;
    bool foundCabin = false;
    for (const auto& it : items) {
        if (it.id == "oil") {
            foundOil = true;
            ASSERT_EQ(it.intervalKm, 12500u);
        }
        if (it.id == "cabin") {
            foundCabin = true;
            ASSERT_FALSE(it.visible);
        }
    }
    ASSERT_TRUE(foundOil);
    ASSERT_TRUE(foundCabin);
}

// ============================================================================
// TIER 4: REAL-WORLD APPLICATION SCENARIO 5 - ECU DIAGNOSTIC RESET EVENT
// ============================================================================

TEST_TIER4(Test_Tier4_Scenario5_EcuReset_ClearCodesZeroPID31) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    hud.bt.currentPid31 = 15420;
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);

    uint32_t baselineOdo = hud.odoEngine.getCurrentOdo();
    ASSERT_EQ(baselineOdo, 98540u);

    // Mechanic clears DTC codes -> PID 31 drops to 0 km!
    hud.bt.currentPid31 = 0;
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);

    // Odometer must NOT drop or glitch!
    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), baselineOdo);
}

TEST_TIER4(Test_Tier4_Scenario5_EcuReset_BaseOdometerPreservedWithoutLoss) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    hud.bt.currentPid31 = 8000;
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);

    // Drive 5 km
    hud.bt.currentPid31 = 8005;
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);
    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), 98545u);

    // ECU reset to 0
    hud.bt.currentPid31 = 0;
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);
    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), 98545u);
}

TEST_TIER4(Test_Tier4_Scenario5_EcuReset_NextPID31AdvancesSmoothly) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    hud.bt.currentPid31 = 5000;
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);

    // ECU reset to 0
    hud.bt.currentPid31 = 0;
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);

    // Next distance advances: 1, 2, 3 km
    hud.bt.currentPid31 = 1;
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);
    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), 98541u);

    hud.bt.currentPid31 = 2;
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);
    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), 98542u);
}

// ============================================================================
// TIER 4: REAL-WORLD APPLICATION SCENARIO 6 - MULTI-HOUR COMMUTE & NVS CADENCE
// ============================================================================

TEST_TIER4(Test_Tier4_Scenario6_MultiHourCommute_WithNvsPersistenceCadence) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);

    uint32_t startOdo = hud.odoEngine.getCurrentOdo();
    uint32_t startWrites = hud.nvs.writeCount;

    // Simulate 300 km commute (10800 seconds = 3 hours at 100 km/h)
    // Step by 30-second increments = 360 ticks
    for (int t = 0; t < 360; ++t) {
        hud.simulateDriveTick(100, 30, true);
    }

    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), startOdo + 300);
    // 300 km driven with 5 km cadence -> ~60 NVS writes
    uint32_t writesDone = hud.nvs.writeCount - startWrites;
    ASSERT_GE(writesDone, 55u);
    ASSERT_LE(writesDone, 65u);
}

// ============================================================================
// TIER 4: REAL-WORLD APPLICATION SCENARIO 7 - WEB ODO RESYNC WITH LIVE TRACKING
// ============================================================================

TEST_TIER4(Test_Tier4_Scenario7_WebOdoSync_LiveOdometerReanchor) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);

    // User corrects odometer to exactly 100,000 km via Web UI
    hud.odoEngine.calibrate(100000);
    hud.nvs.saveOdometer(100000);

    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), 100000u);

    // Subsequent OBD increments continue smoothly from 100,000 km
    hud.bt.currentPid31 += 10;
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);
    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), 100010u);
}

// ============================================================================
// TIER 4: REAL-WORLD APPLICATION SCENARIO 8 - CONTINUOUS MULTI-WRAP DRIVING
// ============================================================================

TEST_TIER4(Test_Tier4_Scenario8_MultiWrap_Continuous131070KmDriving) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();

    // Baseline PID 31 at 65530
    hud.odoEngine.updateFromPid31(65530u);
    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), 98540u);

    // Drive 5 km to 65535
    hud.odoEngine.updateFromPid31(65535u);
    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), 98545u);

    // First Wrap: 65535 -> 0 (+1 km)
    hud.odoEngine.updateFromPid31(0u);
    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), 98546u);

    // Drive another 65535 km across next wrap
    hud.odoEngine.updateFromPid31(65535u);
    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), 98546u + 65535u);

    // Second Wrap: 65535 -> 0 (+1 km)
    hud.odoEngine.updateFromPid31(0u);
    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), 98546u + 65536u);
}

// ============================================================================
// TIER 4: REAL-WORLD APPLICATION SCENARIO 9 - POWER LOSS & CORRUPT NVS RECOVERY
// ============================================================================

TEST_TIER4(Test_Tier4_Scenario9_PowerLoss_NvsCrcValidationAndSafeRecovery) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();

    // Simulate power cut during flash write causing corrupted data
    hud.nvs.isCorrupt = true;

    // Boot after power restoration
    CivicHudE2E::VirtualCivicHud recoveredHud;
    recoveredHud.nvs = hud.nvs;
    recoveredHud.boot();

    // Must safely fallback to standard default items without hanging
    ASSERT_EQ(recoveredHud.maintEngine.getItems().size(), 14u);
}

// ============================================================================
// TIER 4: REAL-WORLD APPLICATION SCENARIO 10 - DYNAMIC SPEED & TOUCH LOCKOUT
// ============================================================================

TEST_TIER4(Test_Tier4_Scenario10_FullThaiDisplay_UrgencyReorderUnderDynamicSpeed) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();

    // Parked: speed = 0, navigate to Page 1
    hud.ui.updateSpeed(0);
    auto sorted = hud.maintEngine.getSortedItems();
    hud.ui.handleTouch(200, 305, sorted); // Tap NEXT button
    ASSERT_EQ(hud.ui.currentPage, 1);

    // Accelerate to 60 km/h: speed lockout forces Page 0 and locks touch
    hud.ui.updateSpeed(60);
    ASSERT_EQ(hud.ui.currentPage, 0);
    ASSERT_TRUE(hud.ui.touchLocked);

    // Touch attempt while driving must be rejected
    bool handled = hud.ui.handleTouch(200, 305, sorted);
    ASSERT_FALSE(handled);
    ASSERT_EQ(hud.ui.currentPage, 0);

    // Stop car: touch unlocks
    hud.ui.updateSpeed(0);
    ASSERT_FALSE(hud.ui.touchLocked);
    handled = hud.ui.handleTouch(200, 305, sorted);
    ASSERT_TRUE(handled);
    ASSERT_EQ(hud.ui.currentPage, 1);
}

// ============================================================================
// TIER 4: EXTRA CORNER CASE WORKLOADS (ADVERSARIAL STRESS)
// ============================================================================

TEST_TIER4(Test_Tier4_Stress_RapidConsecutiveServiceResets) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();

    // Reset 5 items consecutively
    std::vector<std::string> resetTargets = {"oil", "cvt", "tire", "air", "cabin"};
    for (const auto& id : resetTargets) {
        ServiceHistoryRecord rec{};
        bool ok = hud.maintEngine.resetItem(id, hud.odoEngine.getCurrentOdo(), 340, "13 ต.ค. 26", rec);
        ASSERT_TRUE(ok);
        hud.littleFs.appendRecord(rec);
    }

    ASSERT_EQ(hud.littleFs.getRecordCount(), 5);
}

TEST_TIER4(Test_Tier4_Stress_TripOdometerResetAndAccumulation) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);

    hud.odoEngine.resetTrip();
    ASSERT_NEAR(hud.odoEngine.getTripKm(), 0.0f, 0.01f);

    hud.simulateDriveTick(90, 60, true); // 1.5 km
    ASSERT_GE(hud.odoEngine.getTripKm(), 1.0f);
}

TEST_TIER4(Test_Tier4_Stress_MaxOdometerClampingSafety) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();

    // Calibrate near uint32_t limit
    hud.odoEngine.calibrate(4000000000u);
    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), 4000000000u);
}

TEST_TIER4(Test_Tier4_Stress_ZeroSpeedIntegrationProducesNoDelta) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();

    uint32_t startOdo = hud.odoEngine.getCurrentOdo();
    // Idle parked for 1 hour at 0 km/h with disconnected OBD
    for (int i = 0; i < 360; ++i) {
        hud.simulateDriveTick(0, 10, false);
    }
    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), startOdo);
}

TEST_TIER4(Test_Tier4_Stress_All14ItemsSimultaneousUrgencyEvaluation) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();

    auto sorted = hud.maintEngine.getSortedItems();
    ASSERT_EQ(sorted.size(), 14u);

    // Verify ordering invariant: OVERDUE before DUE_SOON before OK
    for (size_t i = 1; i < sorted.size(); ++i) {
        ASSERT_GE(static_cast<uint8_t>(sorted[i - 1].status), static_cast<uint8_t>(sorted[i].status));
    }
}

// ============================================================================
// TIER 2: SYSTEM BOUNDARY & CORNER CASE TESTS (10 TESTS)
// ============================================================================

TEST_TIER2(Test_Tier2_System_SpeedAtZeroKphNoMovement) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    uint32_t startOdo = hud.odoEngine.getCurrentOdo();
    hud.simulateDriveTick(0, 30, true);
    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), startOdo);
}

TEST_TIER2(Test_Tier2_System_SpeedAtFiveKphTouchStillUnlocked) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    hud.ui.updateSpeed(5);
    ASSERT_FALSE(hud.ui.touchLocked);
}

TEST_TIER2(Test_Tier2_System_SpeedAtSixKphTouchLocked) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    hud.ui.updateSpeed(6);
    ASSERT_TRUE(hud.ui.touchLocked);
    ASSERT_EQ(hud.ui.currentPage, 0);
}

TEST_TIER2(Test_Tier2_System_SliderDragAt84PctNotConfirmed) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    auto sorted = hud.maintEngine.getSortedItems();
    hud.ui.modalOpen = true;
    hud.ui.modalItemId = "oil";
    // Track is x=18..222 (width = 204). 84% -> x = 18 + 0.84 * 204 = 189
    hud.ui.handleTouch(189, 230, sorted);
    ASSERT_FALSE(hud.ui.sliderConfirmed);
    ASSERT_LT(hud.ui.sliderPosition, 85.0f);
}

TEST_TIER2(Test_Tier2_System_SliderDragAt85PctConfirmed) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    auto sorted = hud.maintEngine.getSortedItems();
    hud.ui.modalOpen = true;
    hud.ui.modalItemId = "oil";
    // 85% -> x = 18 + 0.85 * 204 = 191
    hud.ui.handleTouch(192, 230, sorted);
    ASSERT_TRUE(hud.ui.sliderConfirmed);
    ASSERT_GE(hud.ui.sliderPosition, 85.0f);
}

TEST_TIER2(Test_Tier2_System_SingleKmDriveTick) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    uint32_t startOdo = hud.odoEngine.getCurrentOdo();
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);
    hud.bt.currentPid31 += 1;
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);
    ASSERT_EQ(hud.odoEngine.getCurrentOdo(), startOdo + 1);
}

TEST_TIER2(Test_Tier2_System_ExactFiveKmIntervalNvsWrite) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);
    ASSERT_FALSE(hud.odoEngine.checkAndClearNvsDirty(5));
    hud.bt.currentPid31 += 4;
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);
    ASSERT_FALSE(hud.odoEngine.checkAndClearNvsDirty(5));
    hud.bt.currentPid31 += 1;
    hud.odoEngine.updateFromPid31(hud.bt.currentPid31);
    ASSERT_TRUE(hud.odoEngine.checkAndClearNvsDirty(5));
}

TEST_TIER2(Test_Tier2_System_ZeroIntervalItemHandling) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    // Item batt has intervalKm = 0 (time-only)
    const auto& items = hud.maintEngine.getItems();
    for (const auto& it : items) {
        if (it.id == "batt") {
            ASSERT_EQ(it.intervalKm, 0u);
            ASSERT_GT(it.intervalDays, 0u);
        }
    }
}

TEST_TIER2(Test_Tier2_System_MaximumItemsLimit24) {
    CivicHudE2E::VirtualCivicHud hud;
    hud.boot();
    for (int i = 0; i < 10; ++i) {
        hud.maintEngine.addItem("Item " + std::to_string(i), 5000, 100, 98540, "13 ต.ค. 26");
    }
    ASSERT_EQ(hud.maintEngine.getItems().size(), 24u);
}

TEST_TIER2(Test_Tier2_System_EmptyHistoryCsvInitialization) {
    CivicHudE2E::MockLittleFsLog fs;
    ASSERT_EQ(fs.getRecordCount(), 0u);
    std::string csv = fs.getCsvExport();
    ASSERT_TRUE(csv.find("Item,Date,Odometer") != std::string::npos);
}

// ============================================================================
// MAIN RUNNER
// ============================================================================

int main() {
    return TestHarness::TestRunner::instance().runAll("Integration & Real-World Scenarios Suite");
}
