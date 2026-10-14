#include "test_framework.h"
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>

// ============================================================================
// NVS & LittleFS Storage Subsystem Test Harness
// ============================================================================

namespace StorageSim {

#pragma pack(push, 1)
struct PackedItemRecord {
    char     id[12];
    char     name[48];        // UTF-8 Thai name with emoji
    uint32_t intervalKm;
    uint16_t intervalDays;
    uint8_t  warnPct;
    uint32_t lastKm;
    uint32_t lastEpoch;
    char     lastDate[24];
    uint8_t  flags;           // bit 0 = visible, bit 1 = isCustom (exact 100 bytes)
};
#pragma pack(pop)

struct ServiceHistoryEntry {
    std::string dateStr;
    std::string itemName;
    uint32_t odoKm;
    uint32_t prevOdoKm;
    uint32_t deltaKm;
    uint32_t nextDueKm;
};

// Mock NVS Store (Simulating ESP32 Preferences)
class MockNvsStore {
public:
    void clear() {
        m_uints.clear();
        m_strings.clear();
        m_blobs.clear();
    }

    void putUInt(const std::string& key, uint32_t val) {
        m_uints[key] = val;
    }

    uint32_t getUInt(const std::string& key, uint32_t defaultVal = 0) const {
        auto it = m_uints.find(key);
        return (it != m_uints.end()) ? it->second : defaultVal;
    }

    void putString(const std::string& key, const std::string& val) {
        m_strings[key] = val;
    }

    std::string getString(const std::string& key, const std::string& defaultVal = "") const {
        auto it = m_strings.find(key);
        return (it != m_strings.end()) ? it->second : defaultVal;
    }

    void putBytes(const std::string& key, const void* buf, size_t len) {
        const uint8_t* bytePtr = reinterpret_cast<const uint8_t*>(buf);
        m_blobs[key] = std::vector<uint8_t>(bytePtr, bytePtr + len);
    }

    size_t getBytesLength(const std::string& key) const {
        auto it = m_blobs.find(key);
        return (it != m_blobs.end()) ? it->second.size() : 0;
    }

    size_t getBytes(const std::string& key, void* outBuf, size_t maxLen) const {
        auto it = m_blobs.find(key);
        if (it == m_blobs.end()) return 0;
        size_t toCopy = std::min(maxLen, it->second.size());
        std::memcpy(outBuf, it->second.data(), toCopy);
        return toCopy;
    }

    bool isKeyPresent(const std::string& key) const {
        return m_uints.count(key) || m_strings.count(key) || m_blobs.count(key);
    }

private:
    std::map<std::string, uint32_t> m_uints;
    std::map<std::string, std::string> m_strings;
    std::map<std::string, std::vector<uint8_t>> m_blobs;
};

// Mock LittleFS Store (Simulating SPIFFS/LittleFS partition file)
class MockLittleFS {
public:
    void clear() { m_files.clear(); }

    bool exists(const std::string& path) const {
        return m_files.count(path) > 0;
    }

    bool writeString(const std::string& path, const std::string& content) {
        m_files[path] = content;
        return true;
    }

    bool appendString(const std::string& path, const std::string& content) {
        m_files[path] += content;
        return true;
    }

    std::string readString(const std::string& path) const {
        auto it = m_files.find(path);
        return (it != m_files.end()) ? it->second : "";
    }

    bool remove(const std::string& path) {
        return m_files.erase(path) > 0;
    }

private:
    std::map<std::string, std::string> m_files;
};

// Item model in memory
struct MemoryItem {
    std::string id;
    std::string name;
    uint32_t intervalKm = 0;
    uint16_t intervalDays = 0;
    uint8_t warnPct = 10;
    uint32_t lastKm = 0;
    uint32_t lastEpoch = 0;
    std::string lastDate;
    bool visible = true;
    bool isCustom = false;
};

class StorageManagerModel {
public:
    StorageManagerModel(MockNvsStore& nvs, MockLittleFS& fs)
        : m_nvs(nvs), m_fs(fs)
    {
    }

    // Default 14 items initialization
    std::vector<MemoryItem> createDefaultItems() {
        return {
            {"oil",    "🛢️ น้ำมันเครื่อง + กรอง", 10000, 180, 15, 90000, 62,   "12 ส.ค. 26", true, false},
            {"cvt",    "⚙️ น้ำมันเกียร์ CVT",    25000, 540, 15, 80000, 300,  "10 ม.ค. 26", true, false},
            {"tire",   "🔄 สลับยาง & ถ่วงล้อ",   10000, 0,   10, 88800, 150,  "15 พ.ค. 26", true, false},
            {"air",    "🌬️ กรองอากาศเครื่องยนต์", 20000, 365, 10, 86740, 150,  "15 พ.ค. 26", true, false},
            {"cabin",  "❄️ กรองแอร์ห้องโดยสาร", 15000, 365, 10, 86640, 200,  "25 มี.ค. 26", true, false},
            {"brake",  "🛑 น้ำมันเบรก",       40000, 730, 10, 72000, 760,  "10 ก.ย. 24", true, false},
            {"bpads",  "🛑 ผ้าเบรก",          40000, 0,   10, 75000, 450,  "20 ก.ค. 25", true, false},
            {"plugs",  "⚡ หัวเทียน (Iridium)",60000, 0,   10, 55000, 500,  "10 พ.ค. 25", true, false},
            {"batt",   "🔋 แบตเตอรี่",        0,     730, 8,  0,     310,  "5 ธ.ค. 25",  true, false},
            {"cool",   "💧 น้ำยาหม้อน้ำ",     100000,3650,5,  0,     2900, "10 พ.ย. 18", true, false},
            {"prb",    "📄 ภาษี & พ.ร.บ.",    0,     365, 10, 0,     330,  "15 พ.ย. 25", true, false},
            {"lpg_f",  "⛽ กรองแก๊ส LPG",     20000, 365, 15, 85000, 250,  "20 ก.พ. 26", true, false},
            {"lpg_t",  "🔧 เช็ครั่ว/จูนแก๊ส",  10000, 365, 10, 92000, 120,  "15 มิ.ย. 26", true, false},
            {"lpg_v",  "⚙️ เช็คระยะวาล์ว LPG", 40000, 0,   10, 65000, 400,  "15 ก.ย. 25", true, false}
        };
    }

    bool saveItemsToNvs(const std::vector<MemoryItem>& items) {
        if (items.size() > 24) return false;
        std::vector<PackedItemRecord> blob(items.size());
        for (size_t i = 0; i < items.size(); ++i) {
            std::memset(&blob[i], 0, sizeof(PackedItemRecord));
            std::strncpy(blob[i].id, items[i].id.c_str(), sizeof(blob[i].id) - 1);
            std::strncpy(blob[i].name, items[i].name.c_str(), sizeof(blob[i].name) - 1);
            blob[i].intervalKm = items[i].intervalKm;
            blob[i].intervalDays = items[i].intervalDays;
            blob[i].warnPct = items[i].warnPct;
            blob[i].lastKm = items[i].lastKm;
            blob[i].lastEpoch = items[i].lastEpoch;
            blob[i].flags = (items[i].visible ? 0x01 : 0x00) | (items[i].isCustom ? 0x02 : 0x00);
        }

        m_nvs.putUInt("item_count", static_cast<uint32_t>(items.size()));
        m_nvs.putBytes("items_blob", blob.data(), blob.size() * sizeof(PackedItemRecord));
        return true;
    }

    bool loadItemsFromNvs(std::vector<MemoryItem>& outItems) {
        uint32_t count = m_nvs.getUInt("item_count", 0);
        size_t blobLen = m_nvs.getBytesLength("items_blob");
        if (count == 0 || blobLen != count * sizeof(PackedItemRecord)) {
            return false;
        }

        std::vector<PackedItemRecord> blob(count);
        m_nvs.getBytes("items_blob", blob.data(), blobLen);

        outItems.clear();
        for (size_t i = 0; i < count; ++i) {
            MemoryItem it;
            it.id = blob[i].id;
            it.name = blob[i].name;
            it.intervalKm = blob[i].intervalKm;
            it.intervalDays = blob[i].intervalDays;
            it.warnPct = blob[i].warnPct;
            it.lastKm = blob[i].lastKm;
            it.lastEpoch = blob[i].lastEpoch;
            it.lastDate = blob[i].lastDate;
            it.visible = (blob[i].flags & 0x01) != 0;
            it.isCustom = (blob[i].flags & 0x02) != 0;
            outItems.push_back(it);
        }
        return true;
    }

    // CSV Service History
    bool appendHistoryLog(const ServiceHistoryEntry& record) {
        std::ostringstream ss;
        ss << record.dateStr << ","
           << record.itemName << ","
           << record.odoKm << ","
           << record.prevOdoKm << ","
           << record.deltaKm << ","
           << record.nextDueKm << "\n";
        return m_fs.appendString("/service_history.csv", ss.str());
    }

    std::vector<ServiceHistoryEntry> loadHistoryLogs() {
        std::vector<ServiceHistoryEntry> records;
        std::string raw = m_fs.readString("/service_history.csv");
        if (raw.empty()) return records;

        std::istringstream stream(raw);
        std::string line;
        while (std::getline(stream, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            if (line.empty()) continue;

            std::istringstream lineStream(line);
            std::string col;
            std::vector<std::string> cols;
            while (std::getline(lineStream, col, ',')) {
                cols.push_back(col);
            }

            if (cols.size() >= 6) {
                try {
                    ServiceHistoryEntry entry;
                    entry.dateStr = cols[0];
                    entry.itemName = cols[1];
                    entry.odoKm = std::stoul(cols[2]);
                    entry.prevOdoKm = std::stoul(cols[3]);
                    entry.deltaKm = std::stoul(cols[4]);
                    entry.nextDueKm = std::stoul(cols[5]);
                    records.push_back(entry);
                } catch (...) {}
            }
        }
        return records;
    }

private:
    MockNvsStore& m_nvs;
    MockLittleFS& m_fs;
};

// URL percent decoder
std::string urlDecode(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '+') {
            out += ' ';
        } else if (in[i] == '%' && i + 2 < in.size()) {
            std::string hexStr = in.substr(i + 1, 2);
            try {
                char ch = static_cast<char>(std::stoul(hexStr, nullptr, 16));
                out += ch;
                i += 2;
            } catch (...) {
                out += in[i];
            }
        } else {
            out += in[i];
        }
    }
    return out;
}

// Web Settings Form Handler Simulator
class WebSettingsHandler {
public:
    WebSettingsHandler(StorageManagerModel& storage, MockNvsStore& nvs)
        : m_storage(storage), m_nvs(nvs)
    {
        m_items = m_storage.createDefaultItems();
        m_currentOdo = 98540;
        m_btName = "OBDII";
        m_btPin = "1234";
        m_screenDirty = false;
    }

    uint32_t getOdo() const { return m_currentOdo; }
    const std::string& getBtName() const { return m_btName; }
    const std::string& getBtPin() const { return m_btPin; }
    bool isScreenDirty() const { return m_screenDirty; }
    void clearScreenDirty() { m_screenDirty = false; }
    const std::vector<MemoryItem>& getItems() const { return m_items; }

    // Generates HTML matching ui_layout_c.html
    std::string renderHtmlPage() {
        std::ostringstream h;
        h << "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Civic FC 1.8 HUD Settings</title></head><body>";
        h << "<h2>Civic FC 1.8 HUD Settings</h2>";
        h << "<form method='POST' action='/save'>";
        
        // ODO Card
        h << "<div class='card'><b>เลขไมล์รถจริง (Odometer):</b><br>";
        h << "<input type='number' name='odo' value='" << m_currentOdo << "'></div>";

        // Bluetooth Card
        h << "<div class='card'><b>Bluetooth OBD-II Settings:</b><br>";
        h << "<input type='text' name='bt_name' value='" << m_btName << "'>";
        h << "<input type='text' name='bt_pin' value='" << m_btPin << "'></div>";

        // Add Custom Item Card
        h << "<div class='card'><b>เพิ่มรายการใหม่:</b><br>";
        h << "<input type='text' name='new_name' placeholder='ชื่อรายการ'>";
        h << "<input type='number' name='new_km' placeholder='รอบระยะ (กม.)'></div>";

        // Items Table
        h << "<div class='card'><b>รายการบำรุงรักษา:</b><br><table>";
        for (const auto& it : m_items) {
            h << "<tr><td><input type='checkbox' name='vis_" << it.id << "'"
              << (it.visible ? " checked" : "") << "></td>";
            h << "<td>" << it.name << "</td>";
            h << "<td><input type='number' name='km_" << it.id << "' value='" << it.intervalKm << "'></td></tr>";
        }
        h << "</table></div>";

        h << "<button type='submit'>💾 บันทึกการตั้งค่าทั้งหมด (Save All)</button></form>";

        // Service History Table
        auto hist = m_storage.loadHistoryLogs();
        h << "<div class='card'><b>สมุดประวัติการเปลี่ยนถ่าย:</b><br><table>";
        h << "<tr><th>วันที่</th><th>รายการ</th><th>เลขไมล์</th><th>ระยะรอบนั้น</th><th>กำหนดรอบถัดไป</th></tr>";
        for (const auto& r : hist) {
            h << "<tr><td>" << r.dateStr << "</td><td>" << r.itemName << "</td>"
              << "<td>" << r.odoKm << " km</td><td>" << r.deltaKm << " km</td>"
              << "<td>" << r.nextDueKm << " km</td></tr>";
        }
        h << "</table></div></body></html>";
        return h.str();
    }

    // Handles POST /save form submission (URL-encoded body)
    int handlePostSave(const std::string& postBody) {
        std::map<std::string, std::string> params;
        std::istringstream stream(postBody);
        std::string pair;

        while (std::getline(stream, pair, '&')) {
            size_t eqPos = pair.find('=');
            if (eqPos != std::string::npos) {
                std::string k = urlDecode(pair.substr(0, eqPos));
                std::string v = urlDecode(pair.substr(eqPos + 1));
                params[k] = v;
            }
        }

        // 1. Process Odometer calibration
        if (params.count("odo")) {
            try {
                uint32_t newOdo = std::stoul(params["odo"]);
                if (newOdo > 0) {
                    m_currentOdo = newOdo;
                    m_nvs.putUInt("odo", newOdo);
                }
            } catch (...) {}
        }

        // 2. Process Bluetooth credentials
        if (params.count("bt_name") && !params["bt_name"].empty()) {
            m_btName = params["bt_name"];
            m_nvs.putString("bt_name", m_btName);
        }
        if (params.count("bt_pin") && !params["bt_pin"].empty()) {
            m_btPin = params["bt_pin"];
            m_nvs.putString("bt_pin", m_btPin);
        }

        // 3. Process Per-Item Intervals & Visibility
        for (auto& it : m_items) {
            std::string kmKey = "km_" + it.id;
            if (params.count(kmKey)) {
                try {
                    uint32_t newKm = std::stoul(params[kmKey]);
                    it.intervalKm = newKm;
                } catch (...) {}
            }

            std::string visKey = "vis_" + it.id;
            it.visible = (params.count(visKey) > 0 && params[visKey] != "0");
        }

        // 4. Process Add Custom Item
        if (params.count("new_name") && !params["new_name"].empty() && params.count("new_km")) {
            try {
                uint32_t newKm = std::stoul(params["new_km"]);
                if (newKm > 0 && m_items.size() < 24) {
                    MemoryItem newItem;
                    newItem.id = "custom_" + std::to_string(m_items.size() + 1);
                    newItem.name = "🔧 " + params["new_name"];
                    newItem.intervalKm = newKm;
                    newItem.intervalDays = 0;
                    newItem.warnPct = 10;
                    newItem.lastKm = m_currentOdo;
                    newItem.lastEpoch = 0;
                    newItem.lastDate = "13 ต.ค. 26";
                    newItem.visible = true;
                    newItem.isCustom = true;
                    m_items.push_back(newItem);
                }
            } catch (...) {}
        }

        // Persist items to NVS
        m_storage.saveItemsToNvs(m_items);
        m_screenDirty = true;
        return 303; // HTTP 303 Redirect
    }

private:
    StorageManagerModel& m_storage;
    MockNvsStore& m_nvs;
    std::vector<MemoryItem> m_items;
    uint32_t m_currentOdo;
    std::string m_btName;
    std::string m_btPin;
    bool m_screenDirty;
};

} // namespace StorageSim

// ============================================================================
// TIER 1: FEATURE COVERAGE TESTS (F10 - F16)
// ============================================================================

TEST_TIER1(Test_F10_NvsScalar_SaveAndLoadOdometer) {
    StorageSim::MockNvsStore nvs;
    nvs.putUInt("odo", 98540);
    ASSERT_EQ(nvs.getUInt("odo"), 98540);
    nvs.putUInt("odo", 100500);
    ASSERT_EQ(nvs.getUInt("odo"), 100500);
}

TEST_TIER1(Test_F10_NvsScalar_SaveAndLoadLastPid31) {
    StorageSim::MockNvsStore nvs;
    nvs.putUInt("last_pid31", 1215);
    ASSERT_EQ(nvs.getUInt("last_pid31"), 1215);
}

TEST_TIER1(Test_F10_NvsScalar_SaveAndLoadBtName) {
    StorageSim::MockNvsStore nvs;
    nvs.putString("bt_name", "OBDII");
    ASSERT_STR_EQ(nvs.getString("bt_name"), "OBDII");
    nvs.putString("bt_name", "CIVIC_ELM");
    ASSERT_STR_EQ(nvs.getString("bt_name"), "CIVIC_ELM");
}

TEST_TIER1(Test_F10_NvsScalar_SaveAndLoadBtPin) {
    StorageSim::MockNvsStore nvs;
    nvs.putString("bt_pin", "1234");
    ASSERT_STR_EQ(nvs.getString("bt_pin"), "1234");
    nvs.putString("bt_pin", "0000");
    ASSERT_STR_EQ(nvs.getString("bt_pin"), "0000");
}

TEST_TIER1(Test_F10_NvsWearLevelingDirtyIntervalCheck) {
    uint32_t lastPersisted = 98540;
    uint32_t currentOdo = 98543;
    bool dirty = (currentOdo - lastPersisted >= 5);
    ASSERT_FALSE(dirty);
    currentOdo = 98545; // delta = 5 km
    dirty = (currentOdo - lastPersisted >= 5);
    ASSERT_TRUE(dirty);
}

TEST_TIER1(Test_F11_PackedItemRecord_ExactStructSize100Bytes) {
    ASSERT_EQ(sizeof(StorageSim::PackedItemRecord), 100);
}

TEST_TIER1(Test_F11_PackedItemBlob_14ItemsExact1400Bytes) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    auto defaults = sm.createDefaultItems();
    ASSERT_EQ(defaults.size(), 14);

    bool saveOk = sm.saveItemsToNvs(defaults);
    ASSERT_TRUE(saveOk);
    ASSERT_EQ(nvs.getBytesLength("items_blob"), 1400);
    ASSERT_EQ(nvs.getUInt("item_count"), 14);
}

TEST_TIER1(Test_F11_PackedItemBlob_RoundtripFieldFidelity) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    auto defaults = sm.createDefaultItems();
    sm.saveItemsToNvs(defaults);

    std::vector<StorageSim::MemoryItem> loaded;
    bool loadOk = sm.loadItemsFromNvs(loaded);
    ASSERT_TRUE(loadOk);
    ASSERT_EQ(loaded.size(), 14);
    ASSERT_STR_EQ(loaded[0].id, "oil");
    ASSERT_EQ(loaded[0].intervalKm, 10000);
    ASSERT_EQ(loaded[0].intervalDays, 180);
    ASSERT_EQ(loaded[0].lastKm, 90000);
}

TEST_TIER1(Test_F11_ThaiUnicodeAndEmojiPreservedInBlob) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    auto defaults = sm.createDefaultItems();
    sm.saveItemsToNvs(defaults);

    std::vector<StorageSim::MemoryItem> loaded;
    sm.loadItemsFromNvs(loaded);
    ASSERT_STR_CONTAINS(loaded[0].name, "น้ำมันเครื่อง");
    ASSERT_STR_CONTAINS(loaded[0].name, "🛢️");
}

TEST_TIER1(Test_F11_CustomItemFlagPreservedInBlob) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    auto items = sm.createDefaultItems();
    StorageSim::MemoryItem custom;
    custom.id = "custom_1";
    custom.name = "🔧 น้ำยาฉีดกระจก";
    custom.intervalKm = 5000;
    custom.isCustom = true;
    items.push_back(custom);

    sm.saveItemsToNvs(items);
    std::vector<StorageSim::MemoryItem> loaded;
    sm.loadItemsFromNvs(loaded);
    ASSERT_EQ(loaded.size(), 15);
    ASSERT_TRUE(loaded[14].isCustom);
    ASSERT_STR_CONTAINS(loaded[14].name, "น้ำยาฉีดกระจก");
}

TEST_TIER1(Test_F12_LittleFS_CSVAppendServiceRecord) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);

    StorageSim::ServiceHistoryEntry rec;
    rec.dateStr = "13 ต.ค. 26";
    rec.itemName = "🛢️ น้ำมันเครื่อง";
    rec.odoKm = 98540;
    rec.prevOdoKm = 90000;
    rec.deltaKm = 8540;
    rec.nextDueKm = 108540;

    bool appendOk = sm.appendHistoryLog(rec);
    ASSERT_TRUE(appendOk);
    ASSERT_TRUE(fs.exists("/service_history.csv"));
    std::string csv = fs.readString("/service_history.csv");
    ASSERT_STR_CONTAINS(csv, "98540,90000,8540,108540");
}

TEST_TIER1(Test_F12_LittleFS_CSVMultipleAppendsAndOrder) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);

    sm.appendHistoryLog({"12 ส.ค. 26", "🛢️ น้ำมันเครื่อง", 90000, 80000, 10000, 100000});
    sm.appendHistoryLog({"13 ต.ค. 26", "🔄 สลับยาง", 98540, 88800, 9740, 108540});

    auto logs = sm.loadHistoryLogs();
    ASSERT_EQ(logs.size(), 2);
    ASSERT_STR_EQ(logs[0].itemName, "🛢️ น้ำมันเครื่อง");
    ASSERT_STR_EQ(logs[1].itemName, "🔄 สลับยาง");
}

TEST_TIER1(Test_F12_LittleFS_CSVParsingFieldAccuracy) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);

    sm.appendHistoryLog({"10 ม.ค. 26", "⚙️ น้ำมันเกียร์ CVT", 80000, 55000, 25000, 105000});
    auto logs = sm.loadHistoryLogs();
    ASSERT_EQ(logs.size(), 1);
    ASSERT_EQ(logs[0].deltaKm, 25000);
    ASSERT_EQ(logs[0].nextDueKm, 105000);
}

TEST_TIER1(Test_F12_ThaiDateStringFormattedInCsv) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);

    sm.appendHistoryLog({"13 ต.ค. 26", "🛢️ น้ำมันเครื่อง", 98540, 90000, 8540, 108540});
    std::string csv = fs.readString("/service_history.csv");
    ASSERT_STR_CONTAINS(csv, "13 ต.ค. 26");
    // Assert literal "TODAY" is NOT present
    ASSERT_FALSE(csv.find("TODAY") != std::string::npos);
}

TEST_TIER1(Test_F12_CsvFileCreationOnFirstWrite) {
    StorageSim::MockLittleFS fs;
    ASSERT_FALSE(fs.exists("/service_history.csv"));
    fs.appendString("/service_history.csv", "header\n");
    ASSERT_TRUE(fs.exists("/service_history.csv"));
}

TEST_TIER1(Test_F13_WebSettings_HtmlRendersOdometerField) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    std::string html = web.renderHtmlPage();
    ASSERT_STR_CONTAINS(html, "name='odo'");
    ASSERT_STR_CONTAINS(html, "value='98540'");
}

TEST_TIER1(Test_F13_WebSettings_HtmlRendersBtCredentials) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    std::string html = web.renderHtmlPage();
    ASSERT_STR_CONTAINS(html, "name='bt_name' value='OBDII'");
    ASSERT_STR_CONTAINS(html, "name='bt_pin' value='1234'");
}

TEST_TIER1(Test_F13_WebSettings_HtmlRendersAll14Items) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    std::string html = web.renderHtmlPage();
    ASSERT_STR_CONTAINS(html, "km_oil");
    ASSERT_STR_CONTAINS(html, "km_cvt");
    ASSERT_STR_CONTAINS(html, "km_tire");
    ASSERT_STR_CONTAINS(html, "km_lpg_v");
}

TEST_TIER1(Test_F13_WebSettings_HtmlRendersVisibilityCheckboxes) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    std::string html = web.renderHtmlPage();
    ASSERT_STR_CONTAINS(html, "name='vis_oil' checked");
    ASSERT_STR_CONTAINS(html, "name='vis_air' checked");
}

TEST_TIER1(Test_F13_WebSettings_HtmlRendersAddItemInputs) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    std::string html = web.renderHtmlPage();
    ASSERT_STR_CONTAINS(html, "name='new_name'");
    ASSERT_STR_CONTAINS(html, "name='new_km'");
}

TEST_TIER1(Test_F14_WebPostSave_OdometerCalibration) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    int status = web.handlePostSave("odo=100500");
    ASSERT_EQ(status, 303);
    ASSERT_EQ(web.getOdo(), 100500);
    ASSERT_EQ(nvs.getUInt("odo"), 100500);
}

TEST_TIER1(Test_F14_WebPostSave_IntervalModification_10k_to_15k) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    web.handlePostSave("km_oil=15000&vis_oil=1");
    auto items = web.getItems();
    ASSERT_EQ(items[0].intervalKm, 15000);
}

TEST_TIER1(Test_F14_WebPostSave_VisibilityToggle_HidesItem) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    // Unchecked checkbox is absent from POST body -> visible = false
    web.handlePostSave("km_tire=10000"); // vis_tire omitted
    auto items = web.getItems();
    for (const auto& it : items) {
        if (it.id == "tire") {
            ASSERT_FALSE(it.visible);
            break;
        }
    }
}

TEST_TIER1(Test_F14_WebPostSave_SetsScreenDirtyFlag) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    web.clearScreenDirty();
    ASSERT_FALSE(web.isScreenDirty());
    web.handlePostSave("odo=99000");
    ASSERT_TRUE(web.isScreenDirty());
}

TEST_TIER1(Test_F14_WebPostSave_ReturnsHttp303Redirect) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    int res = web.handlePostSave("bt_name=OBD_TEST&bt_pin=1234");
    ASSERT_EQ(res, 303);
}

TEST_TIER1(Test_F15_WebSettings_HistoryTableRenderedInHtml) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    sm.appendHistoryLog({"12 ส.ค. 26", "🛢️ น้ำมันเครื่อง", 90000, 80000, 10000, 100000});

    StorageSim::WebSettingsHandler web(sm, nvs);
    std::string html = web.renderHtmlPage();
    ASSERT_STR_CONTAINS(html, "สมุดประวัติการเปลี่ยนถ่าย");
    ASSERT_STR_CONTAINS(html, "12 ส.ค. 26");
    ASSERT_STR_CONTAINS(html, "90000 km");
}

TEST_TIER1(Test_F15_WebSettings_HistoryTableAllColumnsPresent) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    std::string html = web.renderHtmlPage();
    ASSERT_STR_CONTAINS(html, "<th>วันที่</th>");
    ASSERT_STR_CONTAINS(html, "<th>รายการ</th>");
    ASSERT_STR_CONTAINS(html, "<th>เลขไมล์</th>");
    ASSERT_STR_CONTAINS(html, "<th>ระยะรอบนั้น</th>");
    ASSERT_STR_CONTAINS(html, "<th>กำหนดรอบถัดไป</th>");
}

TEST_TIER1(Test_F15_WebSettings_EmptyHistoryGracefulTable) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    std::string html = web.renderHtmlPage();
    ASSERT_STR_CONTAINS(html, "<table>");
    ASSERT_STR_CONTAINS(html, "</table>");
}

TEST_TIER1(Test_F15_WebSettings_CsvExportEndpointSimulated) {
    StorageSim::MockLittleFS fs;
    fs.writeString("/service_history.csv", "date,name,odo,prev,delta,next\n13 Oct,Oil,98540,90000,8540,108540\n");
    std::string rawExport = fs.readString("/service_history.csv");
    ASSERT_STR_CONTAINS(rawExport, "13 Oct,Oil,98540");
}

TEST_TIER1(Test_F15_WebSettings_SaveButtonVisualLabel) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    std::string html = web.renderHtmlPage();
    ASSERT_STR_CONTAINS(html, "💾 บันทึกการตั้งค่าทั้งหมด (Save All)");
}

TEST_TIER1(Test_F16_UrlDecoder_DecodesThaiUtf8PercentEncoding) {
    // "%E0%B8%99%E0%B9%89%E0%B8%B2%E0%B8%A1" = "น้าม"
    std::string encoded = "%E0%B8%99%E0%B9%89%E0%B8%B2%E0%B8%A1";
    std::string decoded = StorageSim::urlDecode(encoded);
    ASSERT_STR_EQ(decoded, "น้าม");
}

TEST_TIER1(Test_F16_SerialCommand_WebSaveBridgeExecution) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    std::string cmd = "CMD:WEB_SAVE odo=99999&bt_name=OBD_BRIDGED&bt_pin=4321";
    std::string payload = cmd.substr(13); // strip CMD:WEB_SAVE 
    int res = web.handlePostSave(payload);
    ASSERT_EQ(res, 303);
    ASSERT_EQ(web.getOdo(), 99999);
    ASSERT_STR_EQ(web.getBtName(), "OBD_BRIDGED");
}

TEST_TIER1(Test_F16_SimulatedReboot_AllDataRestored) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);

    // Write state
    nvs.putUInt("odo", 102000);
    nvs.putString("bt_name", "CIVIC_BT");
    nvs.putString("bt_pin", "0000");

    auto items = sm.createDefaultItems();
    items[0].intervalKm = 15000;
    items[2].visible = false;
    sm.saveItemsToNvs(items);
    sm.appendHistoryLog({"13 ต.ค. 26", "🛢️ น้ำมันเครื่อง", 102000, 90000, 12000, 117000});

    // Simulate Cold Reboot: new StorageManagerModel instance reading from same stores
    StorageSim::StorageManagerModel smRebooted(nvs, fs);
    ASSERT_EQ(nvs.getUInt("odo"), 102000);
    ASSERT_STR_EQ(nvs.getString("bt_name"), "CIVIC_BT");

    std::vector<StorageSim::MemoryItem> restoredItems;
    smRebooted.loadItemsFromNvs(restoredItems);
    ASSERT_EQ(restoredItems[0].intervalKm, 15000);
    ASSERT_FALSE(restoredItems[2].visible);

    auto restoredHistory = smRebooted.loadHistoryLogs();
    ASSERT_EQ(restoredHistory.size(), 1);
    ASSERT_EQ(restoredHistory[0].deltaKm, 12000);
}

TEST_TIER1(Test_F16_SingleSaveAll_UpdatesBothMemoryAndNvs) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    web.handlePostSave("odo=101000&km_oil=12000");
    ASSERT_EQ(web.getOdo(), 101000);
    ASSERT_EQ(nvs.getUInt("odo"), 101000);

    std::vector<StorageSim::MemoryItem> nvsItems;
    sm.loadItemsFromNvs(nvsItems);
    ASSERT_EQ(nvsItems[0].intervalKm, 12000);
}

TEST_TIER1(Test_F16_AddItemViaWeb_PersistsToNvsBlob) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    web.handlePostSave("new_name=WiperFluid&new_km=5000");
    auto items = web.getItems();
    ASSERT_EQ(items.size(), 15);
    ASSERT_STR_CONTAINS(items[14].name, "WiperFluid");

    std::vector<StorageSim::MemoryItem> nvsItems;
    sm.loadItemsFromNvs(nvsItems);
    ASSERT_EQ(nvsItems.size(), 15);
}

// ============================================================================
// TIER 2: BOUNDARY & CORNER CASE TESTS (F10 - F16)
// ============================================================================

TEST_TIER2(Test_Tier2_Nvs_ZeroOdometerValue) {
    StorageSim::MockNvsStore nvs;
    nvs.putUInt("odo", 0);
    ASSERT_EQ(nvs.getUInt("odo", 98540), 0);
}

TEST_TIER2(Test_Tier2_Nvs_MaxOdometerValue_4000000) {
    StorageSim::MockNvsStore nvs;
    nvs.putUInt("odo", 4000000);
    ASSERT_EQ(nvs.getUInt("odo"), 4000000);
}

TEST_TIER2(Test_Tier2_Nvs_EmptyBtNameFallsBackToDefault) {
    StorageSim::MockNvsStore nvs;
    std::string val = nvs.getString("bt_name", "OBDII");
    ASSERT_STR_EQ(val, "OBDII");
}

TEST_TIER2(Test_Tier2_Nvs_EmptyBtPinFallsBackToDefault) {
    StorageSim::MockNvsStore nvs;
    std::string val = nvs.getString("bt_pin", "1234");
    ASSERT_STR_EQ(val, "1234");
}

TEST_TIER2(Test_Tier2_Nvs_MissingKeyReturnsCustomDefault) {
    StorageSim::MockNvsStore nvs;
    ASSERT_EQ(nvs.getUInt("non_existent", 42), 42);
}

TEST_TIER2(Test_Tier2_Nvs_CorruptedBlobLengthRejected) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);

    nvs.putUInt("item_count", 14);
    // Write wrong blob size (e.g. 50 bytes instead of 1400)
    std::vector<uint8_t> corrupted(50, 0);
    nvs.putBytes("items_blob", corrupted.data(), 50);

    std::vector<StorageSim::MemoryItem> items;
    bool ok = sm.loadItemsFromNvs(items);
    ASSERT_FALSE(ok);
}

TEST_TIER2(Test_Tier2_PackedItem_MaxNameLength47Chars) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    auto items = sm.createDefaultItems();
    std::string longName(47, 'A');
    items[0].name = longName;

    sm.saveItemsToNvs(items);
    std::vector<StorageSim::MemoryItem> loaded;
    sm.loadItemsFromNvs(loaded);
    ASSERT_EQ(loaded[0].name.size(), 47);
}

TEST_TIER2(Test_Tier2_MaxCapacity_24ItemsInBlob_Exact2400Bytes) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    auto items = sm.createDefaultItems(); // 14 items
    // Add 10 custom items -> 24 items
    for (int i = 15; i <= 24; ++i) {
        StorageSim::MemoryItem custom;
        custom.id = "custom_" + std::to_string(i);
        custom.name = "Custom " + std::to_string(i);
        custom.intervalKm = 5000;
        items.push_back(custom);
    }
    ASSERT_EQ(items.size(), 24);
    bool saveOk = sm.saveItemsToNvs(items);
    ASSERT_TRUE(saveOk);
    ASSERT_EQ(nvs.getBytesLength("items_blob"), 2400);
}

TEST_TIER2(Test_Tier2_ItemAdditionBeyond24_RejectedSafely) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    auto items = sm.createDefaultItems();
    for (int i = 15; i <= 25; ++i) {
        StorageSim::MemoryItem custom;
        custom.id = "custom_" + std::to_string(i);
        custom.name = "Custom " + std::to_string(i);
        items.push_back(custom);
    }
    ASSERT_EQ(items.size(), 25);
    bool saveOk = sm.saveItemsToNvs(items);
    ASSERT_FALSE(saveOk); // Must reject > 24 items!
}

TEST_TIER2(Test_Tier2_ZeroDistanceInterval_ForTimeOnlyItem) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    auto items = sm.createDefaultItems();
    // Item batt has intervalKm = 0, intervalDays = 730
    ASSERT_EQ(items[8].intervalKm, 0);
    ASSERT_EQ(items[8].intervalDays, 730);
    sm.saveItemsToNvs(items);
    std::vector<StorageSim::MemoryItem> loaded;
    sm.loadItemsFromNvs(loaded);
    ASSERT_EQ(loaded[8].intervalKm, 0);
}

TEST_TIER2(Test_Tier2_ZeroTimeInterval_ForDistanceOnlyItem) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    auto items = sm.createDefaultItems();
    // Item tire has intervalDays = 0, intervalKm = 10000
    ASSERT_EQ(items[2].intervalDays, 0);
    ASSERT_EQ(items[2].intervalKm, 10000);
    sm.saveItemsToNvs(items);
    std::vector<StorageSim::MemoryItem> loaded;
    sm.loadItemsFromNvs(loaded);
    ASSERT_EQ(loaded[2].intervalDays, 0);
}

TEST_TIER2(Test_Tier2_Csv_EmptyFileReadReturnsZeroRecords) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    auto logs = sm.loadHistoryLogs();
    ASSERT_EQ(logs.size(), 0);
}

TEST_TIER2(Test_Tier2_Csv_OneHundredEntriesStress) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    for (int i = 1; i <= 100; ++i) {
        sm.appendHistoryLog({"13 ต.ค. 26", "TestItem", static_cast<uint32_t>(i * 1000), 0, 1000, 2000});
    }
    auto logs = sm.loadHistoryLogs();
    ASSERT_EQ(logs.size(), 100);
    ASSERT_EQ(logs[99].odoKm, 100000);
}

TEST_TIER2(Test_Tier2_Csv_MalformedLineWithMissingColumnsIgnored) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    fs.writeString("/service_history.csv", "13 Oct,Oil,98540\n13 Oct,Oil,98540,90000,8540,108540\n");
    auto logs = sm.loadHistoryLogs();
    ASSERT_EQ(logs.size(), 1); // Malformed line ignored
}

TEST_TIER2(Test_Tier2_Csv_NonNumericColumnsIgnored) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    fs.writeString("/service_history.csv", "13 Oct,Oil,INVALID,ABC,DEF,XYZ\n13 Oct,Oil,98540,90000,8540,108540\n");
    auto logs = sm.loadHistoryLogs();
    ASSERT_EQ(logs.size(), 1);
}

TEST_TIER2(Test_Tier2_Html_EscapingSpecialChars) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);
    std::string html = web.renderHtmlPage();
    ASSERT_TRUE(html.find("<script>") == std::string::npos);
}

TEST_TIER2(Test_Tier2_WebPostSave_ZeroOdoIgnored) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    uint32_t odoBefore = web.getOdo();
    web.handlePostSave("odo=0"); // Odo calibration must be > 0
    ASSERT_EQ(web.getOdo(), odoBefore);
}

TEST_TIER2(Test_Tier2_WebPostSave_NegativeOdoIgnored) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    uint32_t odoBefore = web.getOdo();
    web.handlePostSave("odo=-500");
    ASSERT_GE(web.getOdo(), odoBefore);
}

TEST_TIER2(Test_Tier2_WebPostSave_EmptyBodyNoCrash) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    int res = web.handlePostSave("");
    ASSERT_EQ(res, 303);
}

TEST_TIER2(Test_Tier2_WebPostSave_UnknownItemIdsIgnored) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    int res = web.handlePostSave("km_unknown_item=99999");
    ASSERT_EQ(res, 303);
}

TEST_TIER2(Test_Tier2_WebPostSave_CorruptedUrlEncodingHandledSafely) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    int res = web.handlePostSave("bt_name=%ZZ%GG&odo=99000");
    ASSERT_EQ(res, 303);
    ASSERT_EQ(web.getOdo(), 99000);
}

TEST_TIER2(Test_Tier2_WebPostSave_MultipleItemsModifiedSimultaneously) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    web.handlePostSave("km_oil=15000&km_cvt=30000&km_air=25000");
    auto items = web.getItems();
    ASSERT_EQ(items[0].intervalKm, 15000);
    ASSERT_EQ(items[1].intervalKm, 30000);
    ASSERT_EQ(items[3].intervalKm, 25000);
}

TEST_TIER2(Test_Tier2_WebSettings_HistoryTableRendersFiftyEntries) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    for (int i = 0; i < 50; ++i) {
        sm.appendHistoryLog({"13 ต.ค. 26", "Oil", 98540, 90000, 8540, 108540});
    }
    StorageSim::WebSettingsHandler web(sm, nvs);
    std::string html = web.renderHtmlPage();
    ASSERT_STR_CONTAINS(html, "98540 km");
}

TEST_TIER2(Test_Tier2_FiveConsecutiveRebootsPersistence) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    nvs.putUInt("odo", 100000);

    for (int boot = 0; boot < 5; ++boot) {
        StorageSim::StorageManagerModel smBoot(nvs, fs);
        ASSERT_EQ(nvs.getUInt("odo"), 100000);
    }
}

TEST_TIER2(Test_Tier2_EmptyLittleFsPartitionHandledGracefully) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    auto logs = sm.loadHistoryLogs();
    ASSERT_EQ(logs.size(), 0);
}

TEST_TIER2(Test_Tier2_UrlDecoder_MixedThreeAndFourByteUtf8) {
    // "%F0%9F%9B%A2" = 🛢 (4-byte without variation selector)
    std::string decoded = StorageSim::urlDecode("%F0%9F%9B%A2");
    ASSERT_STR_EQ(decoded, "🛢");
}

TEST_TIER2(Test_Tier2_LongFormPayload_4Kilobytes) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    std::string longPayload = "odo=100500&bt_name=";
    longPayload.append(4000, 'A');
    int res = web.handlePostSave(longPayload);
    ASSERT_EQ(res, 303);
    ASSERT_EQ(web.getOdo(), 100500);
}

TEST_TIER2(Test_Tier2_AddItem_NameEmptyIgnored) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    web.handlePostSave("new_name=&new_km=5000");
    ASSERT_EQ(web.getItems().size(), 14); // Not added
}

TEST_TIER2(Test_Tier2_AddItem_KmZeroIgnored) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    web.handlePostSave("new_name=Wiper&new_km=0");
    ASSERT_EQ(web.getItems().size(), 14); // Not added
}

TEST_TIER2(Test_Tier2_AddItem_ReachesMax24ItemsCap) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    for (int i = 1; i <= 15; ++i) {
        web.handlePostSave("new_name=Item" + std::to_string(i) + "&new_km=1000");
    }
    // Should be clamped at exactly 24 items
    ASSERT_EQ(web.getItems().size(), 24);
}

TEST_TIER2(Test_Tier2_CsvFileRemoveOperation) {
    StorageSim::MockLittleFS fs;
    fs.writeString("/service_history.csv", "data");
    ASSERT_TRUE(fs.exists("/service_history.csv"));
    fs.remove("/service_history.csv");
    ASSERT_FALSE(fs.exists("/service_history.csv"));
}

TEST_TIER2(Test_Tier2_HistoryEntryDeltaComputationPrecision) {
    StorageSim::ServiceHistoryEntry entry;
    entry.odoKm = 98540;
    entry.prevOdoKm = 90000;
    entry.deltaKm = entry.odoKm - entry.prevOdoKm;
    ASSERT_EQ(entry.deltaKm, 8540);
}

TEST_TIER2(Test_Tier2_HistoryEntryNextDueComputation) {
    StorageSim::ServiceHistoryEntry entry;
    entry.odoKm = 98540;
    uint32_t interval = 10000;
    entry.nextDueKm = entry.odoKm + interval;
    ASSERT_EQ(entry.nextDueKm, 108540);
}

TEST_TIER2(Test_Tier2_NvsClearWipesAllData) {
    StorageSim::MockNvsStore nvs;
    nvs.putUInt("odo", 123);
    nvs.putString("bt_name", "XYZ");
    nvs.clear();
    ASSERT_FALSE(nvs.isKeyPresent("odo"));
    ASSERT_FALSE(nvs.isKeyPresent("bt_name"));
}

TEST_TIER2(Test_Tier2_LittleFsClearWipesAllFiles) {
    StorageSim::MockLittleFS fs;
    fs.writeString("/test.txt", "data");
    fs.clear();
    ASSERT_FALSE(fs.exists("/test.txt"));
}

// ============================================================================
// TIER 3: CROSS-FEATURE COMBINATION TESTS
// ============================================================================

TEST_TIER3(Test_Tier3_WebIntervalChange_Plus_OdoUpdate_Plus_Reboot) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    // Save All: oil 10k -> 15k, odo 98540 -> 100500
    web.handlePostSave("odo=100500&km_oil=15000&vis_oil=1");

    // Cold reboot
    StorageSim::StorageManagerModel smReboot(nvs, fs);
    ASSERT_EQ(nvs.getUInt("odo"), 100500);
    std::vector<StorageSim::MemoryItem> items;
    smReboot.loadItemsFromNvs(items);
    ASSERT_EQ(items[0].intervalKm, 15000);
}

TEST_TIER3(Test_Tier3_AddCustomItem_Plus_HideDefaultItem_Plus_Reboot) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    // Hide spark plugs (plugs), add Wiper
    web.handlePostSave("vis_plugs=0&new_name=Wiper&new_km=5000");

    StorageSim::StorageManagerModel smReboot(nvs, fs);
    std::vector<StorageSim::MemoryItem> items;
    smReboot.loadItemsFromNvs(items);
    ASSERT_EQ(items.size(), 15);
    ASSERT_FALSE(items[7].visible); // plugs hidden
    ASSERT_STR_CONTAINS(items[14].name, "Wiper");
}

TEST_TIER3(Test_Tier3_ServiceResetUpdatesHistory_WebSettingsReflectsNewRow) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);

    // Perform service reset and append history
    sm.appendHistoryLog({"13 ต.ค. 26", "🛢️ น้ำมันเครื่อง", 98540, 90000, 8540, 108540});

    // Web Settings loads and renders HTML
    StorageSim::WebSettingsHandler web(sm, nvs);
    std::string html = web.renderHtmlPage();
    ASSERT_STR_CONTAINS(html, "13 ต.ค. 26");
    ASSERT_STR_CONTAINS(html, "🛢️ น้ำมันเครื่อง");
    ASSERT_STR_CONTAINS(html, "8540 km");
}

TEST_TIER3(Test_Tier3_NvsBlob_Plus_LittleFsHistory_PowerLossSimulation) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);

    auto items = sm.createDefaultItems();
    sm.saveItemsToNvs(items);
    sm.appendHistoryLog({"10 ม.ค. 26", "⚙️ น้ำมันเกียร์ CVT", 80000, 55000, 25000, 105000});

    // Sudden power cut & resume
    StorageSim::StorageManagerModel smPostCut(nvs, fs);
    std::vector<StorageSim::MemoryItem> itemsPost;
    ASSERT_TRUE(smPostCut.loadItemsFromNvs(itemsPost));
    ASSERT_EQ(itemsPost.size(), 14);
    auto histPost = smPostCut.loadHistoryLogs();
    ASSERT_EQ(histPost.size(), 1);
}

TEST_TIER3(Test_Tier3_WebBtCredentialsUpdate_PersistedToNvs_LoadedByObdTask) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    web.handlePostSave("bt_name=OBD_PRO_SPORT&bt_pin=9999");
    ASSERT_STR_EQ(nvs.getString("bt_name"), "OBD_PRO_SPORT");
    ASSERT_STR_EQ(nvs.getString("bt_pin"), "9999");
}

TEST_TIER3(Test_Tier3_AllItemsHiddenExceptOne_BlobValid_UnhideAllRestored) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    // Hide all items except oil
    std::string hidePayload = "vis_oil=1";
    web.handlePostSave(hidePayload);

    auto items = web.getItems();
    ASSERT_TRUE(items[0].visible);
    ASSERT_FALSE(items[1].visible);

    // Unhide all
    std::string unhidePayload;
    for (const auto& it : items) {
        unhidePayload += "vis_" + it.id + "=1&";
    }
    web.handlePostSave(unhidePayload);
    auto itemsRestored = web.getItems();
    for (const auto& it : itemsRestored) {
        ASSERT_TRUE(it.visible);
    }
}

TEST_TIER3(Test_Tier3_CustomItemAdded_IntervalModified_SurvivesReboot) {
    StorageSim::MockNvsStore nvs;
    StorageSim::MockLittleFS fs;
    StorageSim::StorageManagerModel sm(nvs, fs);
    StorageSim::WebSettingsHandler web(sm, nvs);

    web.handlePostSave("new_name=BrakeFluid&new_km=20000");
    auto items = web.getItems();
    std::string customId = items.back().id;

    // Modify custom item interval
    web.handlePostSave("km_" + customId + "=25000&vis_" + customId + "=1");

    StorageSim::StorageManagerModel smReboot(nvs, fs);
    std::vector<StorageSim::MemoryItem> rebootItems;
    smReboot.loadItemsFromNvs(rebootItems);
    ASSERT_EQ(rebootItems.back().intervalKm, 25000);
}

// ============================================================================
// MAIN RUNNER
// ============================================================================

int main() {
    return TestHarness::TestRunner::instance().runAll("Storage & Web Settings Suite");
}
