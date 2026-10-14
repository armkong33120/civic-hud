#include "test_framework.h"
#include "MaintenanceItem.h"
#include "MaintenanceEngine.h"
#include "OdometerEngine.h"
#include "StorageManager.h"
#include "ObdTask.h"
#include "WebSettings.h"

#include <iostream>
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cstdint>

// ============================================================================
// CHALLENGER 2 EMPIRICAL TEST SUITE: MILESTONE 2
// Web Settings Server, POST Handling, URL Decoding & Serial Bridge Robustness
// ============================================================================

// ----------------------------------------------------------------------------
// Tier 1: Feature - Web Form Parsing & Parameter Handling
// ----------------------------------------------------------------------------

TEST_TIER1(Test_WebSettings_NormalFormSubmission_AllFieldsProcessed) {
    OdometerEngine odo;
    odo.calibrate(98540);
    MaintenanceEngine maint;
    maint.loadDefaults();
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    // Normal POST body containing ODO, BT credentials, intervals and visibility
    std::string body = "odo=100000&bt_name=CIVIC_DONGLE&bt_pin=9876&km_oil=15000&vis_oil=on&km_cvt=30000&vis_cvt=on";
    int status = WebSettings::handleSavePayload(body, odo, maint, storage, obd, screenDirty);

    ASSERT_EQ(status, 303);
    ASSERT_EQ(odo.getCurrentOdo(), 100000u);
    ASSERT_TRUE(screenDirty);

    uint32_t savedOdo = 0;
    ASSERT_TRUE(storage.loadOdometer(savedOdo));
    ASSERT_EQ(savedOdo, 100000u);

    std::string btName, btPin;
    ASSERT_TRUE(storage.loadBtConfig(btName, btPin));
    ASSERT_EQ(btName, "CIVIC_DONGLE");
    ASSERT_EQ(btPin, "9876");

    // Check oil interval updated
    bool foundOil = false;
    for (const auto& it : maint.getItems()) {
        if (it.id == "oil") {
            ASSERT_EQ(it.intervalKm, 15000u);
            ASSERT_TRUE(it.visible);
            foundOil = true;
        }
    }
    ASSERT_TRUE(foundOil);
}

TEST_TIER1(Test_WebSettings_ObdCredentialsUpdate_PropagatedToObdTask) {
    OdometerEngine odo;
    MaintenanceEngine maint;
    maint.loadDefaults();
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    std::string body = "bt_name=SUPER_ELM327&bt_pin=4321";
    int status = WebSettings::handleSavePayload(body, odo, maint, storage, obd, screenDirty);

    ASSERT_EQ(status, 303);
    std::string name, pin;
    storage.loadBtConfig(name, pin);
    ASSERT_EQ(name, "SUPER_ELM327");
    ASSERT_EQ(pin, "4321");
    ASSERT_TRUE(screenDirty);
}

TEST_TIER1(Test_WebSettings_ScreenDirtyFlagTriggeredOnSave) {
    OdometerEngine odo;
    MaintenanceEngine maint;
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    std::string body = "odo=99000";
    int status = WebSettings::handleSavePayload(body, odo, maint, storage, obd, screenDirty);

    ASSERT_EQ(status, 303);
    ASSERT_TRUE(screenDirty);
}

TEST_TIER1(Test_WebSettings_SerialCommandBridge_ExecutesIdenticalSave) {
    OdometerEngine odo;
    odo.calibrate(90000);
    MaintenanceEngine maint;
    maint.loadDefaults();
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    std::string cmd = "CMD:WEB_SAVE odo=112000&km_oil=12000";
    bool handled = WebSettings::handleSerialCommand(cmd, odo, maint, storage, obd, screenDirty);

    ASSERT_TRUE(handled);
    ASSERT_EQ(odo.getCurrentOdo(), 112000u);
    ASSERT_TRUE(screenDirty);

    uint32_t savedOdo = 0;
    storage.loadOdometer(savedOdo);
    ASSERT_EQ(savedOdo, 112000u);
}

TEST_TIER1(Test_WebSettings_HtmlRenderCompleteness_RubricElementsPresent) {
    MaintenanceEngine maint;
    maint.loadDefaults();
    std::vector<ServiceHistoryRecord> history = {
        {"🛢️ น้ำมันเครื่อง", "13 ต.ค. 26", 98540, 90000, 8540, 108540}
    };

    std::string html = WebSettings::renderHtmlPage(98540, "OBDII", "1234", maint.getItems(), history);

    ASSERT_TRUE(html.find("<!DOCTYPE html>") != std::string::npos);
    ASSERT_TRUE(html.find("192.168.1.1") != std::string::npos);
    ASSERT_TRUE(html.find("name='odo'") != std::string::npos);
    ASSERT_TRUE(html.find("value='98540'") != std::string::npos);
    ASSERT_TRUE(html.find("name='bt_name'") != std::string::npos);
    ASSERT_TRUE(html.find("name='bt_pin'") != std::string::npos);
    ASSERT_TRUE(html.find("name='new_name'") != std::string::npos);
    ASSERT_TRUE(html.find("name='new_km'") != std::string::npos);
    ASSERT_TRUE(html.find("km_oil") != std::string::npos);
    ASSERT_TRUE(html.find("vis_oil") != std::string::npos);
    ASSERT_TRUE(html.find("Save All") != std::string::npos);
    ASSERT_TRUE(html.find("history.csv") != std::string::npos);
    ASSERT_TRUE(html.find("108540 km") != std::string::npos);
}

// ----------------------------------------------------------------------------
// Tier 2: Boundary - Fuzzed ODO Inputs & Numerical Extremes
// ----------------------------------------------------------------------------

TEST_TIER2(Test_WebSettings_Odo_NegativeValuesIgnored) {
    OdometerEngine odo;
    odo.calibrate(98540);
    MaintenanceEngine maint;
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    // Fuzzed negative ODO inputs
    std::vector<std::string> negCases = {
        "odo=-1", "odo=-50000", "odo=-2147483648", "odo=-999999", "odo=%2D5000"
    };

    for (const auto& payload : negCases) {
        screenDirty = false;
        int status = WebSettings::handleSavePayload(payload, odo, maint, storage, obd, screenDirty);
        ASSERT_EQ(status, 400);
        // ODO must NOT be set to negative or wrapped uint32
        ASSERT_EQ(odo.getCurrentOdo(), 98540u);
    }
}

TEST_TIER2(Test_WebSettings_Odo_ZeroValueIgnored) {
    OdometerEngine odo;
    odo.calibrate(98540);
    MaintenanceEngine maint;
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    int status = WebSettings::handleSavePayload("odo=0", odo, maint, storage, obd, screenDirty);
    ASSERT_EQ(status, 400);
    // 0 is ignored (prevents accidental zeroing out of car odometer)
    ASSERT_EQ(odo.getCurrentOdo(), 98540u);
}

TEST_TIER2(Test_WebSettings_Odo_FloatingPointNumbersTruncatedSafely) {
    OdometerEngine odo;
    odo.calibrate(98540);
    MaintenanceEngine maint;
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    // Floating point number string
    int status = WebSettings::handleSavePayload("odo=102345.67", odo, maint, storage, obd, screenDirty);
    ASSERT_EQ(status, 400);
    // std::stol parses leading digits up to decimal point
    ASSERT_EQ(odo.getCurrentOdo(), 98540u);
}

TEST_TIER2(Test_WebSettings_Odo_NonDigitsAndAlphaStringsIgnored) {
    OdometerEngine odo;
    odo.calibrate(98540);
    MaintenanceEngine maint;
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    std::vector<std::string> alphaCases = {
        "odo=abc", "odo=---", "odo=NaN", "odo=Inf", "odo=null", "odo=undefined",
        "odo=km100000", "odo=!@#$%", "odo=%20%20"
    };

    for (const auto& payload : alphaCases) {
        screenDirty = false;
        int status = WebSettings::handleSavePayload(payload, odo, maint, storage, obd, screenDirty);
        ASSERT_EQ(status, 400);
        ASSERT_EQ(odo.getCurrentOdo(), 98540u);
    }
}

TEST_TIER2(Test_WebSettings_Odo_ExtremelyLargeOverflowHandledSafely) {
    OdometerEngine odo;
    odo.calibrate(98540);
    MaintenanceEngine maint;
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    // String that exceeds 64-bit int
    std::string hugePayload = "odo=999999999999999999999999999999999999999999999";
    int status = WebSettings::handleSavePayload(hugePayload, odo, maint, storage, obd, screenDirty);
    ASSERT_EQ(status, 400);
    // Must catch std::out_of_range and keep previous odo
    ASSERT_EQ(odo.getCurrentOdo(), 98540u);
}

TEST_TIER2(Test_WebSettings_Odo_EmptyOrWhitespaceHandledSafely) {
    OdometerEngine odo;
    odo.calibrate(98540);
    MaintenanceEngine maint;
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    int status = WebSettings::handleSavePayload("odo=", odo, maint, storage, obd, screenDirty);
    ASSERT_EQ(status, 400);
    ASSERT_EQ(odo.getCurrentOdo(), 98540u);
}

TEST_TIER2(Test_WebSettings_IntervalKm_BoundaryZeroAndLargeValues) {
    MaintenanceEngine maint;
    maint.loadDefaults();
    OdometerEngine odo;
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    // Set interval to 0 (e.g. disable distance check) and 500,000 km
    std::string payload = "km_oil=0&km_cvt=500000";
    int status = WebSettings::handleSavePayload(payload, odo, maint, storage, obd, screenDirty);
    ASSERT_EQ(status, 303);

    for (const auto& it : maint.getItems()) {
        if (it.id == "oil") ASSERT_EQ(it.intervalKm, 0u);
        if (it.id == "cvt") ASSERT_EQ(it.intervalKm, 500000u);
    }

    // Ensure evaluation does not divide by zero
    maint.evaluate(100000, 0);
    for (const auto& it : maint.getItems()) {
        if (it.id == "oil") {
            // Distance is 0, so should not crash or trigger NaN
            ASSERT_FALSE(std::isnan(it.remainingRatio));
        }
    }
}

TEST_TIER2(Test_WebSettings_IntervalKm_NonDigitsIgnoredSafely) {
    MaintenanceEngine maint;
    maint.loadDefaults();
    OdometerEngine odo;
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    // Previous oil interval was 10,000
    std::string payload = "km_oil=non_numeric&km_cvt=";
    int status = WebSettings::handleSavePayload(payload, odo, maint, storage, obd, screenDirty);
    ASSERT_EQ(status, 400);

    for (const auto& it : maint.getItems()) {
        if (it.id == "oil") ASSERT_EQ(it.intervalKm, 10000u);
        if (it.id == "cvt") ASSERT_EQ(it.intervalKm, 25000u);
    }
}

// ----------------------------------------------------------------------------
// Tier 2: Boundary - Adversarial Malformed URL-Encoded Bodies
// ----------------------------------------------------------------------------

TEST_TIER2(Test_WebSettings_Malformed_EmptyBody) {
    OdometerEngine odo;
    odo.calibrate(98540);
    MaintenanceEngine maint;
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    int status = WebSettings::handleSavePayload("", odo, maint, storage, obd, screenDirty);
    ASSERT_EQ(status, 303);
    ASSERT_EQ(odo.getCurrentOdo(), 98540u);
    ASSERT_TRUE(screenDirty);
}

TEST_TIER2(Test_WebSettings_Malformed_LoneAmpersandsAndEquals) {
    OdometerEngine odo;
    odo.calibrate(98540);
    MaintenanceEngine maint;
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    std::vector<std::string> malformed = {
        "&&&&&", "=====", "&=&==&", "&odo=100000&&", "=100000&=test&", "&&&&odo=105000&&&&"
    };

    for (const auto& body : malformed) {
        screenDirty = false;
        int status = WebSettings::handleSavePayload(body, odo, maint, storage, obd, screenDirty);
        ASSERT_EQ(status, 303);
    }
    // Should safely reach 105000
    ASSERT_EQ(odo.getCurrentOdo(), 105000u);
}

TEST_TIER2(Test_WebSettings_Malformed_IncompletePercentEncodings) {
    // Test urlDecode directly against broken escape sequences
    ASSERT_EQ(WebSettings::urlDecode("Hello+World"), "Hello World");
    ASSERT_EQ(WebSettings::urlDecode("test%"), "test%");
    ASSERT_EQ(WebSettings::urlDecode("test%1"), "test%1");
    ASSERT_EQ(WebSettings::urlDecode("test%ZZ"), "test%ZZ");
    ASSERT_EQ(WebSettings::urlDecode("test%%20"), "test% ");
    ASSERT_EQ(WebSettings::urlDecode("test%G%H%I"), "test%G%H%I");
    ASSERT_EQ(WebSettings::urlDecode("%E0%B8%99%E0%B9%89%E0%B8%B3"), "น้ำ"); // Valid Thai UTF-8
}

TEST_TIER2(Test_WebSettings_Malformed_NullByteAndBinaryFuzzing) {
    OdometerEngine odo;
    MaintenanceEngine maint;
    maint.loadDefaults();
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    // Null byte injection in URL encoding (%00)
    std::string payload = "bt_name=Safe%00Hidden&new_name=Item%00Evil&new_km=5000";
    int status = WebSettings::handleSavePayload(payload, odo, maint, storage, obd, screenDirty);
    ASSERT_EQ(status, 400);

    // Verify system did not crash and custom item was processed safely
    std::string btName, btPin;
    storage.loadBtConfig(btName, btPin);
    ASSERT_FALSE(btName.empty());
}

TEST_TIER2(Test_WebSettings_Malformed_MassivePayloadStress) {
    OdometerEngine odo;
    MaintenanceEngine maint;
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    // Construct a 50 KB body with 1000 dummy parameters
    std::string bigBody;
    for (int i = 0; i < 1000; ++i) {
        if (i > 0) bigBody += "&";
        bigBody += "param_" + std::to_string(i) + "=value_with_some_long_text_and_data_" + std::to_string(i * 123);
    }
    bigBody += "&odo=108000";

    auto start = std::chrono::high_resolution_clock::now();
    int status = WebSettings::handleSavePayload(bigBody, odo, maint, storage, obd, screenDirty);
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    ASSERT_EQ(status, 303);
    ASSERT_EQ(odo.getCurrentOdo(), 108000u);
    ASSERT_LT(ms, 100.0); // Must parse 1000 params in under 100ms on host
}

// ----------------------------------------------------------------------------
// Tier 3: Combination - Field Ordering, Missing Keys & Visibility Toggling
// ----------------------------------------------------------------------------

TEST_TIER3(Test_WebSettings_OutOfOrderAndMissingKeys) {
    OdometerEngine odo;
    odo.calibrate(98540);
    MaintenanceEngine maint;
    maint.loadDefaults();
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    // Keys scattered in arbitrary order with most fields missing
    std::string payload = "vis_cabin=1&new_km=6000&bt_pin=5555&vis_oil=0&odo=101234&new_name=TestFilter&km_air=18000";
    int status = WebSettings::handleSavePayload(payload, odo, maint, storage, obd, screenDirty);

    ASSERT_EQ(status, 303);
    ASSERT_EQ(odo.getCurrentOdo(), 101234u);

    std::string btName, btPin;
    storage.loadBtConfig(btName, btPin);
    ASSERT_EQ(btPin, "5555");

    bool foundAir = false;
    for (const auto& it : maint.getItems()) {
        if (it.id == "air") {
            ASSERT_EQ(it.intervalKm, 18000u);
            foundAir = true;
        }
    }
    ASSERT_TRUE(foundAir);
}

TEST_TIER3(Test_WebSettings_DuplicateKeys_LastValueWins) {
    OdometerEngine odo;
    odo.calibrate(98540);
    MaintenanceEngine maint;
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    std::string payload = "odo=100000&odo=120000&odo=115000&bt_name=First&bt_name=Second&bt_name=FinalELM";
    int status = WebSettings::handleSavePayload(payload, odo, maint, storage, obd, screenDirty);

    ASSERT_EQ(status, 303);
    ASSERT_EQ(odo.getCurrentOdo(), 115000u);

    std::string name, pin;
    storage.loadBtConfig(name, pin);
    ASSERT_EQ(name, "FinalELM");
}

TEST_TIER3(Test_WebSettings_VisibilityToggle_AllItemsHidden) {
    OdometerEngine odo;
    odo.calibrate(98540);
    MaintenanceEngine maint;
    maint.loadDefaults();
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    // In HTML form, when all checkboxes are unchecked, the browser sends NO vis_* keys at all!
    std::string payload = "full_form=1&odo=98540&bt_name=OBDII&bt_pin=1234";
    int status = WebSettings::handleSavePayload(payload, odo, maint, storage, obd, screenDirty);

    ASSERT_EQ(status, 303);

    // 1. Every item must have visible = false
    const auto& items = maint.getItems();
    for (const auto& it : items) {
        ASSERT_FALSE(it.visible);
    }

    // 2. Sorted items list must be empty
    auto sorted = maint.getSortedItems();
    ASSERT_EQ(sorted.size(), 0u);

    // 3. Storage must persist all-hidden state
    std::vector<MaintenanceItem> reloaded;
    ASSERT_TRUE(storage.loadItems(reloaded));
    ASSERT_EQ(reloaded.size(), items.size());
    for (const auto& it : reloaded) {
        ASSERT_FALSE(it.visible);
    }

    // 4. HTML generation must still render table rows with unchecked boxes
    std::string html = WebSettings::renderHtmlPage(98540, "OBDII", "1234", items, {});
    ASSERT_TRUE(html.find("type='checkbox'") != std::string::npos);
    ASSERT_TRUE(html.find("checked") == std::string::npos); // No boxes checked
}

TEST_TIER3(Test_WebSettings_VisibilityToggle_AllItemsVisible) {
    OdometerEngine odo;
    odo.calibrate(98540);
    MaintenanceEngine maint;
    maint.loadDefaults();
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    // Send payload with all items checked: vis_<id>=1
    std::string payload = "odo=98540";
    for (const auto& it : maint.getItems()) {
        payload += "&vis_" + it.id + "=on";
    }

    int status = WebSettings::handleSavePayload(payload, odo, maint, storage, obd, screenDirty);
    ASSERT_EQ(status, 303);

    // All items must be visible
    const auto& items = maint.getItems();
    for (const auto& it : items) {
        ASSERT_TRUE(it.visible);
    }

    // Sorted items list must contain all items
    auto sorted = maint.getSortedItems();
    ASSERT_EQ(sorted.size(), items.size());

    // NVS roundtrip
    std::vector<MaintenanceItem> reloaded;
    ASSERT_TRUE(storage.loadItems(reloaded));
    for (const auto& it : reloaded) {
        ASSERT_TRUE(it.visible);
    }
}

TEST_TIER3(Test_WebSettings_VisibilityToggle_PartialToggling) {
    MaintenanceEngine maint;
    maint.loadDefaults();
    OdometerEngine odo;
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    // Only oil, plugs, and batt checked; others unchecked
    std::string payload = "full_form=1&vis_oil=on&vis_plugs=1&vis_batt=yes";
    int status = WebSettings::handleSavePayload(payload, odo, maint, storage, obd, screenDirty);
    ASSERT_EQ(status, 303);

    for (const auto& it : maint.getItems()) {
        if (it.id == "oil" || it.id == "plugs" || it.id == "batt") {
            ASSERT_TRUE(it.visible);
        } else {
            ASSERT_FALSE(it.visible);
        }
    }

    auto sorted = maint.getSortedItems();
    ASSERT_EQ(sorted.size(), 3u);
}

// ----------------------------------------------------------------------------
// Tier 3: Combination - Custom Items Adversarial Stress
// ----------------------------------------------------------------------------

TEST_TIER3(Test_WebSettings_CustomItem_SpecialCharactersAndThaiAccents) {
    OdometerEngine odo;
    odo.calibrate(98540);
    MaintenanceEngine maint;
    maint.loadDefaults();
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    // Thai name with accents, quotes, and punctuation url-encoded
    std::string thaiName = "ล้างแอร์+เติมน้ำยา (สูตรเย็นพิเศษ)";
    std::string payload = "new_name=" + thaiName + "&new_km=15000";

    int status = WebSettings::handleSavePayload(payload, odo, maint, storage, obd, screenDirty);
    ASSERT_EQ(status, 303);
    ASSERT_EQ(maint.getItems().size(), 15u);

    // Verify Thai characters survived in live engine
    const auto& lastItem = maint.getItems().back();
    ASSERT_TRUE(lastItem.name.find("ล้างแอร์") != std::string::npos);
    ASSERT_LE(lastItem.name.size(),79u);
    std::string normalized;ASSERT_TRUE(StorageManager::normalizeUtf8(lastItem.name,79,normalized));

    // Verify NVS binary blob round-trip preserves Thai characters
    std::vector<MaintenanceItem> reloaded;
    ASSERT_TRUE(storage.loadItems(reloaded));
    ASSERT_EQ(reloaded.size(), 15u);
    ASSERT_TRUE(reloaded.back().name.find("ล้างแอร์") != std::string::npos);
}

TEST_TIER3(Test_WebSettings_CustomItem_HtmlAndScriptInjectionSafe) {
    OdometerEngine odo;
    MaintenanceEngine maint;
    maint.loadDefaults();
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    std::string xssName = "%3Cscript%3Ealert('xss')%3C%2Fscript%3E";
    std::string payload = "new_name=" + xssName + "&new_km=8000";

    int status = WebSettings::handleSavePayload(payload, odo, maint, storage, obd, screenDirty);
    ASSERT_EQ(status, 303);

    // HTML render must contain the tag without causing crash or buffer overflow
    std::string html = WebSettings::renderHtmlPage(98540, "OBDII", "1234", maint.getItems(), {});
    ASSERT_TRUE(html.find("<script>") == std::string::npos);
    ASSERT_TRUE(html.find("&lt;script&gt;") != std::string::npos);
}

TEST_TIER3(Test_WebSettings_CustomItem_ExcessiveLengthTruncatedSafely) {
    OdometerEngine odo;
    MaintenanceEngine maint;
    maint.loadDefaults();
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    // Extremely long name (150 chars)
    std::string longName(150, 'A');
    std::string payload = "new_name=" + longName + "&new_km=5000";

    int status = WebSettings::handleSavePayload(payload, odo, maint, storage, obd, screenDirty);
    ASSERT_EQ(status, 303);

    // Save and load through NVS POD struct (name array is 48 chars)
    std::vector<MaintenanceItem> reloaded;
    ASSERT_TRUE(storage.loadItems(reloaded));
    ASSERT_EQ(reloaded.size(), 15u);

    // Must be bounded within 48 bytes without memory overrun
    ASSERT_LE(reloaded.back().name.length(), 79u);
    ASSERT_EQ(reloaded.back().name,maint.getItems().back().name);
}

TEST_TIER3(Test_WebSettings_CustomItem_MaximumCapacityClamping24) {
    OdometerEngine odo;
    MaintenanceEngine maint;
    maint.loadDefaults(); // 14 items
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    ASSERT_EQ(maint.getItems().size(), 14u);

    // Add 10 items to reach the maximum 24 items limit
    for (int i = 1; i <= 10; ++i) {
        std::string payload = "new_name=Custom_" + std::to_string(i) + "&new_km=" + std::to_string(i * 1000);
        int status = WebSettings::handleSavePayload(payload, odo, maint, storage, obd, screenDirty);
        ASSERT_EQ(status, 303);
        ASSERT_EQ(maint.getItems().size(), 14u + i);
    }
    ASSERT_EQ(maint.getItems().size(), 24u);

    // Attempt to add 25th item - must be rejected
    std::string overflowPayload = "new_name=Overflow_25&new_km=9999";
    int status = WebSettings::handleSavePayload(overflowPayload, odo, maint, storage, obd, screenDirty);
    ASSERT_EQ(status, 400);
    // Size must remain strictly clamped to 24
    ASSERT_EQ(maint.getItems().size(), 24u);

    // Verify all 24 items survive NVS persistence
    std::vector<MaintenanceItem> reloaded;
    ASSERT_TRUE(storage.loadItems(reloaded));
    ASSERT_EQ(reloaded.size(), 24u);
}

TEST_TIER3(Test_WebSettings_CustomItem_EmptyNameOrZeroKmRejected) {
    OdometerEngine odo;
    MaintenanceEngine maint;
    maint.loadDefaults();
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    size_t countBefore = maint.getItems().size();

    // 1. Empty name
    WebSettings::handleSavePayload("new_name=&new_km=5000", odo, maint, storage, obd, screenDirty);
    ASSERT_EQ(maint.getItems().size(), countBefore);

    // 2. Zero km
    WebSettings::handleSavePayload("new_name=ValidName&new_km=0", odo, maint, storage, obd, screenDirty);
    ASSERT_EQ(maint.getItems().size(), countBefore);

    // 3. Alpha km
    WebSettings::handleSavePayload("new_name=ValidName&new_km=invalid", odo, maint, storage, obd, screenDirty);
    ASSERT_EQ(maint.getItems().size(), countBefore);
}

// ----------------------------------------------------------------------------
// Tier 4: Real-World Scenarios - Stress, Serial Bridge & Endurance
// ----------------------------------------------------------------------------

TEST_TIER4(Test_WebSettings_SerialBridge_FuzzedCommands) {
    OdometerEngine odo;
    odo.calibrate(98540);
    MaintenanceEngine maint;
    maint.loadDefaults();
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    // Test serial bridge commands
    ASSERT_TRUE(WebSettings::handleSerialCommand("CMD:WEB_SAVE odo=105000&bt_name=SERIAL_ELM", odo, maint, storage, obd, screenDirty));
    ASSERT_EQ(odo.getCurrentOdo(), 105000u);
    ASSERT_TRUE(screenDirty);

    screenDirty = false;
    ASSERT_TRUE(WebSettings::handleSerialCommand("CMD:WEB_SAVE ", odo, maint, storage, obd, screenDirty));
    ASSERT_TRUE(screenDirty);

    ASSERT_TRUE(WebSettings::handleSerialCommand("CMD:DUMP_STORAGE", odo, maint, storage, obd, screenDirty));

    // Unknown command must return false
    ASSERT_FALSE(WebSettings::handleSerialCommand("CMD:UNKNOWN_ACTION", odo, maint, storage, obd, screenDirty));
    ASSERT_FALSE(WebSettings::handleSerialCommand("RANDOM_TEXT_STREAM", odo, maint, storage, obd, screenDirty));
    ASSERT_FALSE(WebSettings::handleSerialCommand("", odo, maint, storage, obd, screenDirty));
}

TEST_TIER4(Test_WebSettings_EnduranceFuzzing_10kRequests_ZeroLeaks) {
    OdometerEngine odo;
    odo.calibrate(98540);
    MaintenanceEngine maint;
    maint.loadDefaults();
    StorageManager storage;
    storage.begin();
    ObdTask obd;
    bool screenDirty = false;

    std::mt19937 rng(42);
    std::vector<std::string> odoVariants = {
        "-500", "0", "98540", "100200", "12345.67", "abc", "999999999999999999", ""
    };
    std::vector<std::string> btVariants = {
        "OBDII", "ELM_%20327", "SUPER+DONGLE", "", "A%00B", "%ZZ%FF"
    };

    auto start = std::chrono::high_resolution_clock::now();

    // 10,000 rapid randomized POST requests
    for (int i = 0; i < 10000; ++i) {
        std::string odoVal = odoVariants[rng() % odoVariants.size()];
        std::string btVal = btVariants[rng() % btVariants.size()];
        std::string payload = "odo=" + odoVal + "&bt_name=" + btVal + "&bt_pin=1234&km_oil=10000";

        if (rng() % 2 == 0) {
            payload += "&vis_oil=on";
        }
        if (i % 500 == 0) {
            payload += "&&&&=====&bogus_key";
        }

        int res = WebSettings::handleSavePayload(payload, odo, maint, storage, obd, screenDirty);
        ASSERT_TRUE(res==303 || res==400);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "      [10,000 Fuzzed Web Requests Endurance Time: " 
              << std::fixed << std::setprecision(2) << totalMs << " ms (" 
              << (totalMs / 10000.0) << " ms/req)]\n";

    ASSERT_LT(totalMs, 3000.0); // 10,000 requests must finish in under 3.0 seconds
}

// ============================================================================
// Main Suite Runner Entry Point
// ============================================================================

int main() {
    return ::TestHarness::TestRunner::instance().runAll("Challenger 2 Empirical Verification: Web Settings Server & Serial Bridge");
}
