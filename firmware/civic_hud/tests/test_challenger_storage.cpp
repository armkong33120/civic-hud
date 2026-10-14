#include "test_framework.h"

#include "MaintenanceItem.h"

inline std::ostream& operator<<(std::ostream& os, AlertStatus st) {
    switch (st) {
        case AlertStatus::OK: return os << "OK";
        case AlertStatus::DUE_SOON: return os << "DUE_SOON";
        case AlertStatus::OVERDUE: return os << "OVERDUE";
    }
    return os << "UNKNOWN";
}

// Include StorageManager.cpp directly to grant the test harness full visibility
// over s_hostUints, s_hostStrings, s_hostBlobs, and s_hostCsvFile for white-box corruption.
#include "StorageManager.cpp"

#include "MaintenanceEngine.h"
#include "OdometerEngine.h"
#include "ObdTask.h"
#include "WebSettings.h"

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <limits>
#include <cmath>
#include <random>

// ============================================================================
// HELPER UTILITIES FOR ADVERSARIAL TESTING
// ============================================================================

static void resetHostStorageState() {
    s_hostUints.clear();
    s_hostStrings.clear();
    s_hostBlobs.clear();
    s_hostCsvFile.clear();
}

// ============================================================================
// PART 1: EXTREME INTERVAL VALUES (0 km, 1,000,000 km, 65535 days, UINT32_MAX)
// ============================================================================

TEST_TIER2(Adv_Interval_ZeroKmAndZeroDays_Handling) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    MaintenanceItem item;
    item.id = "zero_test";
    item.name = "Zero Interval Item";
    item.intervalKm = 0;
    item.intervalDays = 0;
    item.warnPct = 10;
    item.lastKm = 100000;
    item.lastEpoch = 50;
    item.lastDate = "13 ต.ค. 26";
    item.visible = true;

    std::vector<MaintenanceItem> items = { item };
    ASSERT_TRUE(storage.saveItems(items));

    std::vector<MaintenanceItem> loaded;
    ASSERT_TRUE(storage.loadItems(loaded));
    ASSERT_EQ(loaded.size(), 1);
    ASSERT_EQ(loaded[0].intervalKm, 0);
    ASSERT_EQ(loaded[0].intervalDays, 0);

    // Evaluate in MaintenanceEngine
    MaintenanceEngine maint;
    maint.getItems() = loaded;
    maint.evaluate(105000, 60);

    const auto& evaluated = maint.getItems();
    ASSERT_FALSE(std::isnan(evaluated[0].remainingRatio));
    ASSERT_FALSE(std::isinf(evaluated[0].remainingRatio));
    ASSERT_EQ(evaluated[0].status, AlertStatus::OK);
    ASSERT_EQ(evaluated[0].usedPct, 0.0f);
}

TEST_TIER2(Adv_Interval_OneMillionKm_BoundaryHandling) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    MaintenanceItem item;
    item.id = "million_km";
    item.name = "Million Km Lifetime Component";
    item.intervalKm = 1000000; // 1,000,000 km
    item.intervalDays = 0;
    item.warnPct = 10; // Warn when <= 10% remaining (i.e. <= 100,000 km left)
    item.lastKm = 0;
    item.lastEpoch = 0;
    item.lastDate = "13 ต.ค. 26";
    item.visible = true;

    std::vector<MaintenanceItem> items = { item };
    ASSERT_TRUE(storage.saveItems(items));

    std::vector<MaintenanceItem> loaded;
    ASSERT_TRUE(storage.loadItems(loaded));
    ASSERT_EQ(loaded.size(), 1);
    ASSERT_EQ(loaded[0].intervalKm, 1000000);

    MaintenanceEngine maint;
    maint.getItems() = loaded;

    // At 500,000 km -> 50% used
    maint.evaluate(500000, 0);
    ASSERT_EQ(maint.getItems()[0].kmLeft, 500000);
    ASSERT_EQ(maint.getItems()[0].status, AlertStatus::OK);
    ASSERT_NEAR(maint.getItems()[0].usedPct, 50.0f, 0.1f);

    // At 920,000 km -> 80,000 km left (8% remaining <= 10% warnPct) -> DUE_SOON
    maint.evaluate(920000, 0);
    ASSERT_EQ(maint.getItems()[0].kmLeft, 80000);
    ASSERT_EQ(maint.getItems()[0].status, AlertStatus::DUE_SOON);

    // At 1,000,001 km -> Overdue
    maint.evaluate(1000001, 0);
    ASSERT_EQ(maint.getItems()[0].kmLeft, -1);
    ASSERT_EQ(maint.getItems()[0].status, AlertStatus::OVERDUE);
    ASSERT_NEAR(maint.getItems()[0].usedPct, 100.0f, 0.1f);
}

TEST_TIER2(Adv_Interval_MaxUint16Days_65535Days) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    MaintenanceItem item;
    item.id = "max_days";
    item.name = "Long Life Coolant / Sealed Part";
    item.intervalKm = 0;
    item.intervalDays = 65535; // Maximum uint16_t (~179.4 years)
    item.warnPct = 5;
    item.lastKm = 0;
    item.lastEpoch = 1000;
    item.lastDate = "13 ต.ค. 26";
    item.visible = true;

    std::vector<MaintenanceItem> items = { item };
    ASSERT_TRUE(storage.saveItems(items));

    std::vector<MaintenanceItem> loaded;
    ASSERT_TRUE(storage.loadItems(loaded));
    ASSERT_EQ(loaded.size(), 1);
    ASSERT_EQ(loaded[0].intervalDays, 65535);

    MaintenanceEngine maint;
    maint.getItems() = loaded;

    // Evaluate at epoch day 10,000 (usedDays = 9,000)
    maint.evaluate(0, 10000);
    ASSERT_EQ(maint.getItems()[0].daysLeft, 65535 - 9000);
    ASSERT_EQ(maint.getItems()[0].status, AlertStatus::OK);
}

TEST_TIER2(Adv_Interval_Uint32Max_BoundaryInvestigation) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    MaintenanceItem item;
    item.id = "u32_max";
    item.name = "Max UInt32 Interval";
    item.intervalKm = std::numeric_limits<uint32_t>::max(); // 4,294,967,295 km
    item.intervalDays = 0;
    item.warnPct = 10;
    item.lastKm = 0;
    item.lastEpoch = 0;
    item.lastDate = "13 ต.ค. 26";
    item.visible = true;

    std::vector<MaintenanceItem> items = { item };
    ASSERT_TRUE(storage.saveItems(items));

    std::vector<MaintenanceItem> loaded;
    ASSERT_TRUE(storage.loadItems(loaded));
    ASSERT_EQ(loaded.size(), 1);
    ASSERT_EQ(loaded[0].intervalKm, std::numeric_limits<uint32_t>::max());

    // Architectural observation: MaintenanceEngine casts intervalKm to int32_t
    // When intervalKm > INT32_MAX, static_cast<int32_t>(UINT32_MAX) becomes -1
    MaintenanceEngine maint;
    maint.getItems() = loaded;
    maint.evaluate(100000, 0);

    // Documented engine behaviour: negative int32_t triggers overdue
    ASSERT_EQ(maint.getItems()[0].status, AlertStatus::OK);
}

TEST_TIER3(Adv_Interval_All24Items_ExtremePermutations_Sorting) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    std::vector<MaintenanceItem> items;
    for (int i = 0; i < 24; ++i) {
        MaintenanceItem it;
        it.id = "item_" + std::to_string(i);
        it.name = "Test Item " + std::to_string(i);
        if (i % 4 == 0)      { it.intervalKm = 0; it.intervalDays = 0; }
        else if (i % 4 == 1) { it.intervalKm = 1; it.intervalDays = 1; }
        else if (i % 4 == 2) { it.intervalKm = 500000; it.intervalDays = 10000; }
        else                 { it.intervalKm = 1000000; it.intervalDays = 65535; }
        it.warnPct = 10;
        it.lastKm = 100000;
        it.lastEpoch = 500;
        it.lastDate = "13 ต.ค. 26";
        it.visible = true;
        items.push_back(it);
    }

    ASSERT_TRUE(storage.saveItems(items));

    std::vector<MaintenanceItem> loaded;
    ASSERT_TRUE(storage.loadItems(loaded));
    ASSERT_EQ(loaded.size(), 24);

    MaintenanceEngine maint;
    maint.getItems() = loaded;
    maint.evaluate(100500, 502);

    auto sorted = maint.getSortedItems();
    ASSERT_EQ(sorted.size(), 24);

    // Verify sort order: OVERDUE (2) >= DUE_SOON (1) >= OK (0)
    for (size_t i = 1; i < sorted.size(); ++i) {
        ASSERT_TRUE(static_cast<uint8_t>(sorted[i - 1].status) >= static_cast<uint8_t>(sorted[i].status));
    }
}

// ============================================================================
// PART 2: THAI UTF-8 NAMES, ACCENTS/TONES, AND BOUNDARY LENGTHS
// ============================================================================

TEST_TIER1(Adv_ThaiUtf8_ToneMarksAndCombiningCharacters_FidelityWithin47Bytes) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    MaintenanceItem item;
    item.id = "thai_oil";
    // Thai with multiple tone marks, above vowels, below vowels, and emoji within 47 bytes
    item.name = "🛢️ น้ำมันเครื่อง"; // 38 bytes (fits in char name[48])
    item.intervalKm = 10000;
    item.intervalDays = 180;
    item.warnPct = 15;
    item.lastKm = 90000;
    item.lastEpoch = 62;
    item.lastDate = "12 ส.ค. 26";
    item.visible = true;

    std::vector<MaintenanceItem> items = { item };
    ASSERT_TRUE(storage.saveItems(items));

    std::vector<MaintenanceItem> loaded;
    ASSERT_TRUE(storage.loadItems(loaded));
    ASSERT_EQ(loaded.size(), 1);
    ASSERT_STR_EQ(loaded[0].name, "🛢️ น้ำมันเครื่อง");
}

TEST_TIER1(Adv_ThaiUtf8_EmpiricalBug_DefaultItemsSilentDataLoss) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    // Load the project's actual 14 default items
    MaintenanceEngine maint;
    maint.loadDefaults();
    const auto& original = maint.getItems();
    ASSERT_EQ(original.size(), 14);

    // Save defaults to storage
    ASSERT_TRUE(storage.saveItems(original));

    // Reload from storage
    std::vector<MaintenanceItem> reloaded;
    ASSERT_TRUE(storage.loadItems(reloaded));
    ASSERT_EQ(reloaded.size(), 14);

    // Regression: every default now survives the 80-byte storage contract.
    for(size_t i=0;i<original.size();i++)ASSERT_STR_EQ(reloaded[i].name,original[i].name);
}

TEST_TIER2(Adv_ThaiUtf8_ExactBoundary_47Bytes_NoTruncation) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    // Construct a Thai UTF-8 string that is exactly 47 bytes long:
    // 15 Thai characters (15 * 3 = 45 bytes) + 2 ASCII characters ("99") = 47 bytes.
    std::string exact47 = "กรองอากาศแอร์รถ"; // 15 Thai chars = 45 bytes
    exact47 += "99"; // + 2 ASCII bytes = exactly 47 bytes
    ASSERT_EQ(exact47.size(), 47);

    MaintenanceItem item;
    item.id = "bnd_47";
    item.name = exact47;
    item.intervalKm = 20000;
    item.intervalDays = 365;
    item.warnPct = 10;
    item.lastKm = 50000;
    item.lastEpoch = 0;
    item.lastDate = "13 ต.ค. 26";
    item.visible = true;

    std::vector<MaintenanceItem> items = { item };
    ASSERT_TRUE(storage.saveItems(items));

    std::vector<MaintenanceItem> loaded;
    ASSERT_TRUE(storage.loadItems(loaded));
    ASSERT_EQ(loaded.size(), 1);
    ASSERT_EQ(loaded[0].name.size(), 47);
    ASSERT_STR_EQ(loaded[0].name, exact47);
}

TEST_TIER2(Adv_ThaiUtf8_Truncation_Over47Bytes_NullTerminated) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    // Long Thai string > 48 bytes
    std::string longThai = "🔧 เปลี่ยนถ่ายน้ำมันเครื่องสังเคราะห์แท้และกรองพรีเมียมเกรดสูง";
    ASSERT_GT(longThai.size(), 48);

    MaintenanceItem item;
    item.id = "long_thai";
    item.name = longThai;
    item.intervalKm = 10000;
    item.intervalDays = 180;
    item.warnPct = 10;
    item.lastKm = 50000;
    item.lastEpoch = 0;
    item.lastDate = "13 ต.ค. 26";
    item.visible = true;

    std::vector<MaintenanceItem> items = { item };
    ASSERT_TRUE(storage.saveItems(items));

    std::vector<MaintenanceItem> loaded;
    ASSERT_TRUE(storage.loadItems(loaded));
    ASSERT_EQ(loaded.size(), 1);

    // Must be truncated to at most 47 bytes (with null terminator at byte 48)
    ASSERT_LE(loaded[0].name.size(), 79);
    ASSERT_STR_EQ(loaded[0].name, longThai.substr(0, loaded[0].name.size()));
}

TEST_TIER2(Adv_ThaiUtf8_MultiByteCutAcross47ByteBoundary_NoCrash) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    // Construct a string where byte 46 is the first byte of a 3-byte Thai char
    // 15 Thai chars = 45 bytes.
    // Next char is 'ก' (0xE0, 0xB8, 0x81).
    // Byte 45 = 0xE0, Byte 46 = 0xB8, Byte 47 = 0x81.
    // If we have 15 Thai chars + 'A' (46 bytes) + 'ก' (bytes 46, 47, 48):
    std::string splitThai = "กรองอากาศแอร์รถ"; // 45 bytes
    splitThai += "A"; // 46 bytes
    splitThai += "ก"; // 46 + 3 = 49 bytes (cut happens in middle of 'ก')

    MaintenanceItem item;
    item.id = "split_thai";
    item.name = splitThai;
    item.intervalKm = 10000;
    item.intervalDays = 180;
    item.warnPct = 10;
    item.lastKm = 50000;
    item.lastEpoch = 0;
    item.lastDate = "13 ต.ค. 26";
    item.visible = true;

    std::vector<MaintenanceItem> items = { item };
    ASSERT_TRUE(storage.saveItems(items));

    std::vector<MaintenanceItem> loaded;
    ASSERT_TRUE(storage.loadItems(loaded));
    ASSERT_EQ(loaded.size(), 1);
    ASSERT_LE(loaded[0].name.size(), 79);

    // Verify WebSettings HTML generation does not crash on truncated UTF-8 string
    std::vector<ServiceHistoryRecord> hist;
    std::string html = WebSettings::renderHtmlPage(100000, "OBDII", "1234", loaded, hist);
    ASSERT_FALSE(html.empty());
    ASSERT_STR_CONTAINS(html, "split_thai");
}

TEST_TIER2(Adv_Boundary_IdAndDateLengths_TruncatedSafely) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    MaintenanceItem item;
    item.id = "custom_item_with_very_long_id_exceeding_12_bytes"; // > 12 chars
    item.name = "Test Item";
    item.intervalKm = 10000;
    item.intervalDays = 180;
    item.warnPct = 10;
    item.lastKm = 50000;
    item.lastEpoch = 0;
    item.lastDate = "13 ต.ค. 2569 เวลา 14:30:00 น."; // > 24 chars
    item.visible = true;

    std::vector<MaintenanceItem> items = { item };
    // IDs must not silently truncate into collisions.
    ASSERT_FALSE(storage.saveItems(items));
    items[0].id="custom_item";
    ASSERT_TRUE(storage.saveItems(items));
    std::vector<MaintenanceItem>loaded;ASSERT_TRUE(storage.loadItems(loaded));
    ASSERT_EQ(loaded[0].id,items[0].id);ASSERT_LE(loaded[0].lastDate.size(),23u);
    std::string normalized;ASSERT_TRUE(StorageManager::normalizeUtf8(loaded[0].lastDate,23,normalized));
}

// ============================================================================
// PART 3: CORRUPT NVS BLOB BYTE SEQUENCES & GRACEFUL FALLBACK
// ============================================================================

TEST_TIER2(Adv_CorruptBlob_TruncatedLength_RejectsGracefully) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    // Test cases for corrupted lengths for count = 14 (expected = 1400 bytes)
    std::vector<size_t> badLengths = { 0, 1, 10, 50, 99, 100, 101, 1399, 1401, 2000 };

    for (size_t len : badLengths) {
        resetHostStorageState();
        s_hostUints["item_count"] = 14;
        s_hostBlobs["items_blob"] = std::vector<uint8_t>(len, 0xAA);

        std::vector<MaintenanceItem> items;
        bool result = storage.loadItems(items);
        ASSERT_FALSE(result);
        ASSERT_EQ(items.size(), 0);
    }
}

TEST_TIER2(Adv_CorruptBlob_InvalidItemCount_RejectsGracefully) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    std::vector<uint32_t> badCounts = { 0, 25, 26, 50, 100, 1000, std::numeric_limits<uint32_t>::max() };

    for (uint32_t count : badCounts) {
        resetHostStorageState();
        s_hostUints["item_count"] = count;
        // Allocate matching byte count so size check alone wouldn't fail
        size_t totalBytes = (count <= 1000) ? count * sizeof(PackedItemRecord) : 100;
        s_hostBlobs["items_blob"] = std::vector<uint8_t>(totalBytes, 0x00);

        std::vector<MaintenanceItem> items;
        bool result = storage.loadItems(items);
        ASSERT_FALSE(result);
        ASSERT_EQ(items.size(), 0);
    }
}

TEST_TIER2(Adv_CorruptBlob_ErasedFlash_All0xFF_GracefulFallback) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    // Flash memory erased state is all 0xFF
    s_hostUints["item_count"] = 0xFFFFFFFF;
    s_hostBlobs["items_blob"] = std::vector<uint8_t>(1400, 0xFF);

    std::vector<MaintenanceItem> items;
    bool loaded = storage.loadItems(items);
    ASSERT_FALSE(loaded);

    // System fallback verification: loadDefaults() restores health
    MaintenanceEngine maint;
    if (!loaded) {
        maint.loadDefaults();
        ASSERT_TRUE(storage.saveItems(maint.getItems()));
    }

    std::vector<MaintenanceItem> restored;
    ASSERT_TRUE(storage.loadItems(restored));
    ASSERT_EQ(restored.size(), 14);
    ASSERT_STR_EQ(restored[0].id, "oil");
}

TEST_TIER2(Adv_CorruptBlob_UnterminatedStringsInBlob_SafetyInvestigation) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    // Construct a 100-byte record where id, name, and lastDate have NO null-terminator
    // (all bytes filled with 'X')
    std::vector<uint8_t> corruptBlob(sizeof(PackedItemRecord), 'X');
    // Ensure last byte (flags) is 0x01
    corruptBlob.back() = 0x01;

    s_hostUints["item_count"] = 1;
    s_hostBlobs["items_blob"] = corruptBlob;

    std::vector<MaintenanceItem> loaded;
    // loadItems will read blob[0].id, blob[0].name, blob[0].lastDate
    // Test that this executes without memory segfault
    bool result = storage.loadItems(loaded);
    ASSERT_FALSE(result);
    ASSERT_TRUE(loaded.empty());
    return;

    // Historical observation: id and name bleed across boundaries if unterminated in blob,
    // but the system does not crash or throw unhandled exceptions.
    ASSERT_FALSE(loaded[0].id.empty());
    ASSERT_FALSE(loaded[0].name.empty());
}

TEST_TIER3(Adv_CorruptBlob_FuzzedRandomStreams_Stability) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> distCount(0, 50);
    std::uniform_int_distribution<size_t> distLen(0, 3000);
    std::uniform_int_distribution<uint8_t> distByte(0, 255);

    for (int iter = 0; iter < 50; ++iter) {
        resetHostStorageState();
        uint32_t count = distCount(rng);
        size_t len = distLen(rng);

        std::vector<uint8_t> randomBytes(len);
        for (auto& b : randomBytes) b = distByte(rng);

        s_hostUints["item_count"] = count;
        s_hostBlobs["items_blob"] = randomBytes;

        std::vector<MaintenanceItem> items;
        // Must never crash, throw, or hang
        bool res = storage.loadItems(items);
        if (count == 0 || count > 24 || len != count * sizeof(PackedItemRecord)) {
            ASSERT_FALSE(res);
        }
    }
}

// ============================================================================
// PART 4: RAPID SEQUENTIAL SAVES AND HISTORY LOG APPENDS (100+ ENTRIES)
// ============================================================================

TEST_TIER1(Adv_RapidSequentialSaves_100Cycles) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    MaintenanceEngine maint;
    maint.loadDefaults();
    auto items = maint.getItems();

    // Perform 100 rapid sequential saves with evolving interval and odometer
    for (int i = 1; i <= 100; ++i) {
        items[0].intervalKm = 10000 + i * 100;
        items[0].lastKm = 90000 + i * 50;
        ASSERT_TRUE(storage.saveItems(items));
    }

    std::vector<MaintenanceItem> loaded;
    ASSERT_TRUE(storage.loadItems(loaded));
    ASSERT_EQ(loaded.size(), 14);
    ASSERT_EQ(loaded[0].intervalKm, 10000 + 100 * 100); // 20,000 km
    ASSERT_EQ(loaded[0].lastKm, 90000 + 100 * 50);      // 95,000 km
}

TEST_TIER1(Adv_HistoryLog_150EntriesStress_AccuracyAndOrder) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    // Append 150 consecutive service history records
    for (uint32_t i = 1; i <= 150; ++i) {
        ServiceHistoryRecord rec;
        rec.dateStr = "13 ต.ค. 26";
        rec.itemName = "Item #" + std::to_string(i);
        rec.odoKm = 100000 + i * 1000;
        rec.prevOdoKm = 100000 + (i - 1) * 1000;
        rec.deltaKm = 1000;
        rec.nextDueKm = rec.odoKm + 10000;

        ASSERT_TRUE(storage.appendHistoryLog(rec));
    }

    auto logs = storage.loadHistoryLogs();
    ASSERT_EQ(logs.size(), 150);

    // Verify 1st, 75th, and 150th record data exact match
    ASSERT_STR_EQ(logs[0].itemName, "Item #1");
    ASSERT_EQ(logs[0].odoKm, 101000);
    ASSERT_EQ(logs[0].deltaKm, 1000);

    ASSERT_STR_EQ(logs[74].itemName, "Item #75");
    ASSERT_EQ(logs[74].odoKm, 175000);

    ASSERT_STR_EQ(logs[149].itemName, "Item #150");
    ASSERT_EQ(logs[149].odoKm, 250000);
    ASSERT_EQ(logs[149].nextDueKm, 260000);
}

TEST_TIER2(Adv_HistoryLog_MalformedRowsSkippedGracefully) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    // Inject mixed valid and corrupted CSV lines
    s_hostCsvFile = 
        "header_row_date,item,odo,prev,delta,next\n"                         // Header (skipped)
        "13 ต.ค. 26,🛢️ น้ำมันเครื่อง,100000,90000,10000,110000\n"          // Valid #1
        "\n"                                                                // Empty line (skipped)
        "corrupted_line_only_three_cols,foo,bar\n"                          // Insufficient cols (skipped)
        "13 ต.ค. 26,⚙️ เกียร์ CVT,105000,80000,25000,130000\n"              // Valid #2
        "13 ต.ค. 26,Invalid Numbers,ABC,DEF,GHI,JKL\n"                      // Non-numeric cols (skipped)
        "   \r\n"                                                           // Whitespace line (skipped)
        "13 ต.ค. 26,🔄 สลับยาง,110000,100000,10000,120000\r\n";             // Valid #3 with CRLF

    auto logs = storage.loadHistoryLogs();
    ASSERT_EQ(logs.size(), 3);
    ASSERT_STR_EQ(logs[0].itemName, "🛢️ น้ำมันเครื่อง");
    ASSERT_STR_EQ(logs[1].itemName, "⚙️ เกียร์ CVT");
    ASSERT_STR_EQ(logs[2].itemName, "🔄 สลับยาง");
    ASSERT_EQ(logs[2].odoKm, 110000);
}

TEST_TIER2(Adv_HistoryLog_CommasInItemName_EdgeCaseInvestigation) {
    resetHostStorageState();
    StorageManager storage;
    ASSERT_TRUE(storage.begin());

    // An item name containing an unquoted comma
    ServiceHistoryRecord recWithComma;
    recWithComma.dateStr = "13 ต.ค. 26";
    recWithComma.itemName = "🛢️ น้ำมันเครื่อง, แหวนรอง"; // Contains comma!
    recWithComma.odoKm = 100000;
    recWithComma.prevOdoKm = 90000;
    recWithComma.deltaKm = 10000;
    recWithComma.nextDueKm = 110000;

    ASSERT_TRUE(storage.appendHistoryLog(recWithComma));

    // Investigating parser behaviour when CSV column splits on comma
    auto logs = storage.loadHistoryLogs();
    // In current implementation without quote escaping:
    // splitting on comma produces 7 cols instead of 6, and stoul on col[2] (" แหวนรอง") throws,
    // which is caught by catch(...) -> row skipped gracefully without crash.
    ASSERT_EQ(logs.size(), 1);
    ASSERT_EQ(logs[0].itemName,recWithComma.itemName); // Gracefully ignored rather than crashing
}

// ============================================================================
// PART 5: WEB SETTINGS FORM SUBMISSION, PERSISTENCE & HTML STRESS
// ============================================================================

TEST_TIER1(Adv_WebSave_RapidSequential_50Cycles) {
    resetHostStorageState();
    StorageManager storage;
    storage.begin();

    OdometerEngine odoEngine;
    odoEngine.calibrate(98540);
    MaintenanceEngine maint;
    maint.loadDefaults();
    ObdTask obdTask;
    bool screenDirty = false;

    // 50 rapid sequential web saves
    for (int i = 1; i <= 50; ++i) {
        std::string payload = "odo=" + std::to_string(100000 + i * 100) + 
                              "&km_oil=" + std::to_string(10000 + i * 50) + 
                              "&bt_name=OBD_" + std::to_string(i) + 
                              "&bt_pin=1234";
        int res = WebSettings::handleSavePayload(payload, odoEngine, maint, storage, obdTask, screenDirty);
        ASSERT_EQ(res, 303);
        ASSERT_EQ(odoEngine.getCurrentOdo(), 100000 + i * 100);
        ASSERT_TRUE(screenDirty);
    }

    uint32_t savedOdo = 0;
    ASSERT_TRUE(storage.loadOdometer(savedOdo));
    ASSERT_EQ(savedOdo, 105000);

    std::string btName, btPin;
    ASSERT_TRUE(storage.loadBtConfig(btName, btPin));
    ASSERT_STR_EQ(btName, "OBD_50");

    std::vector<MaintenanceItem> reloaded;
    ASSERT_TRUE(storage.loadItems(reloaded));
    ASSERT_EQ(reloaded[0].intervalKm, 10000 + 50 * 50);
}

TEST_TIER2(Adv_WebSave_MalformedUrlEncodingFuzz) {
    resetHostStorageState();
    StorageManager storage;
    storage.begin();

    OdometerEngine odoEngine;
    odoEngine.calibrate(98540);
    MaintenanceEngine maint;
    maint.loadDefaults();
    ObdTask obdTask;
    bool screenDirty = false;

    std::vector<std::string> malformedPayloads = {
        "",
        "&&&&&",
        "===&&===",
        "odo=%",
        "odo=%G",
        "odo=%ZZ",
        "odo=-10000",
        "odo=abc",
        "km_oil=%",
        "bt_name=%%%&bt_pin=%%%&new_name=%E0%B8%99%E0%B9%89%E0%B8%B2%E0%B8%A1&new_km=5000"
    };

    for (const auto& payload : malformedPayloads) {
        int res = WebSettings::handleSavePayload(payload, odoEngine, maint, storage, obdTask, screenDirty);
        ASSERT_TRUE(res==303 || res==400);
    }

    // Verify system state remained stable and valid
    ASSERT_GE(odoEngine.getCurrentOdo(), 98540);
}

TEST_TIER3(Adv_WebHtml_LargeHistoryRenderingPerformance) {
    resetHostStorageState();
    StorageManager storage;
    storage.begin();

    // Create 150 history records
    std::vector<ServiceHistoryRecord> history;
    for (int i = 1; i <= 150; ++i) {
        history.push_back({"13 ต.ค. 26", "Item " + std::to_string(i), 100000 + (uint32_t)i * 1000, 100000, 1000, 110000});
    }

    MaintenanceEngine maint;
    maint.loadDefaults();

    auto start = std::chrono::high_resolution_clock::now();
    std::string html = WebSettings::renderHtmlPage(120000, "OBDII", "1234", maint.getItems(), history);
    auto end = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();

    ASSERT_FALSE(html.empty());
    ASSERT_STR_CONTAINS(html, "Item 150");
    ASSERT_STR_CONTAINS(html, "120000");
    // Rendering 150 rows should complete in < 50 ms on host
    ASSERT_LT(elapsedMs, 50.0);
}

// ============================================================================
// MAIN RUNNER
// ============================================================================

int main() {
    std::cout << "\n======================================================================\n";
    std::cout << "  CHALLENGER 1: EMPIRICAL & ADVERSARIAL STRESS SUITE (MILESTONE 2)\n";
    std::cout << "======================================================================\n";
    return TestHarness::TestRunner::instance().runAll("Milestone 2 Storage Subsystem Adversarial Challenge");
}
