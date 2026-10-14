#include "MaintenanceEngine.h"
#include <algorithm>
#include <cmath>
#include "StorageManager.h"

MaintenanceEngine::MaintenanceEngine() {
    loadDefaults();
}

void MaintenanceEngine::loadDefaults() {
    m_items.clear();
    // Default 14 items for Civic FC 1.8 + LPG
    m_items = {
        {"oil",    "🛢️ น้ำมันเครื่อง + กรอง", 10000, 180, 15, 90000, 62,   "12 ส.ค. 26", true},
        {"cvt",    "⚙️ น้ำมันเกียร์ CVT",    25000, 540, 15, 80000, 300,  "10 ม.ค. 26", true},
        {"tire",   "🔄 สลับยาง & ถ่วงล้อ",   10000, 0,   10, 88800, 150,  "15 พ.ค. 26", true},
        {"air",    "🌬️ กรองอากาศเครื่องยนต์", 20000, 365, 10, 86740, 150,  "15 พ.ค. 26", true},
        {"cabin",  "❄️ กรองแอร์ห้องโดยสาร", 15000, 365, 10, 86640, 200,  "25 มี.ค. 26", true},
        {"brake",  "🛑 น้ำมันเบรก",       40000, 730, 10, 72000, 760,  "10 ก.ย. 24", true},
        {"bpads",  "🛑 ผ้าเบรก",          40000, 0,   10, 75000, 450,  "20 ก.ค. 25", true},
        {"plugs",  "⚡ หัวเทียน (Iridium)",60000, 0,   10, 55000, 500,  "10 พ.ค. 25", true},
        {"batt",   "🔋 แบตเตอรี่",        0,     730, 8,  0,     310,  "5 ธ.ค. 25",  true},
        {"cool",   "💧 น้ำยาหม้อน้ำ",     100000,3650,5,  0,     2900, "10 พ.ย. 18", true},
        {"prb",    "📄 ภาษี & พ.ร.บ.",    0,     365, 10, 0,     330,  "15 พ.ย. 25", true},
        {"lpg_f",  "⛽ กรองแก๊ส LPG",     20000, 365, 15, 85000, 250,  "20 ก.พ. 26", true},
        {"lpg_t",  "🔧 เช็ครั่ว/จูนแก๊ส",  10000, 365, 10, 92000, 120,  "15 มิ.ย. 26", true},
        {"lpg_v",  "⚙️ เช็คระยะวาล์ว LPG", 40000, 0,   10, 65000, 400,  "15 ก.ย. 25", true}
    };

    // Pre-populate initial sample history records
    m_history = {
        {"🛢️ น้ำมันเครื่อง", "12 ส.ค. 26", 90000, 80000, 10000, 100000},
        {"🔄 สลับยาง",       "15 พ.ค. 26", 88800, 78800, 10000, 98800},
        {"⛽ กรองแก๊ส LPG",   "20 ก.พ. 26", 85000, 65000, 20000, 105000},
        {"⚙️ น้ำมันเกียร์ CVT", "10 ม.ค. 26", 80000, 55000, 25000, 105000}
    };
}

void MaintenanceEngine::evaluate(uint32_t currentOdoKm, uint32_t currentEpochDays) {
    for (auto& item : m_items) {
        float rKm = 999.0f;
        float rDays = 999.0f;
        item.kmLeft = 0;
        item.daysLeft = 0;

        if (item.intervalKm > 0) {
            uint32_t usedKm = (currentOdoKm >= item.lastKm) ? (currentOdoKm - item.lastKm) : 0;
            int64_t left = int64_t(item.intervalKm) - int64_t(usedKm);
            item.kmLeft = int32_t(std::max<int64_t>(INT32_MIN, std::min<int64_t>(INT32_MAX, left)));
            rKm = static_cast<float>(item.kmLeft) / static_cast<float>(item.intervalKm);
        }

        if (item.intervalDays > 0) {
            uint32_t usedDays = (currentEpochDays >= item.lastEpoch) ? (currentEpochDays - item.lastEpoch) : 0;
            item.daysLeft = static_cast<int32_t>(item.intervalDays) - static_cast<int32_t>(usedDays);
            rDays = static_cast<float>(item.daysLeft) / static_cast<float>(item.intervalDays);
        }

        item.remainingRatio = std::min(rKm, rDays);
        item.isLimitedByTime = (rDays < rKm);

        float warnThreshold = static_cast<float>(item.warnPct) / 100.0f;
        if (item.remainingRatio <= 0.0f) {
            item.status = AlertStatus::OVERDUE;
            item.usedPct = 100.0f;
        } else if (item.remainingRatio <= warnThreshold) {
            item.status = AlertStatus::DUE_SOON;
            item.usedPct = (1.0f - item.remainingRatio) * 100.0f;
        } else {
            item.status = AlertStatus::OK;
            item.usedPct = (1.0f - item.remainingRatio) * 100.0f;
        }

        // Clamp usedPct
        if (item.usedPct < 0.0f) item.usedPct = 0.0f;
        if (item.usedPct > 100.0f) item.usedPct = 100.0f;
    }
}

std::vector<MaintenanceItem> MaintenanceEngine::getSortedItems() const {
    std::vector<MaintenanceItem> sorted = m_items;

    // Filter visible items only
    sorted.erase(std::remove_if(sorted.begin(), sorted.end(), [](const MaintenanceItem& it) {
        return !it.visible;
    }), sorted.end());

    // Sort by urgency:
    // 1. AlertStatus::OVERDUE (2) > DUE_SOON (1) > OK (0)
    // 2. Lowest remaining ratio first (closer to / furthest past due limit)
    std::sort(sorted.begin(), sorted.end(), [](const MaintenanceItem& a, const MaintenanceItem& b) {
        if (static_cast<uint8_t>(a.status) != static_cast<uint8_t>(b.status)) {
            return static_cast<uint8_t>(a.status) > static_cast<uint8_t>(b.status);
        }
        return a.remainingRatio < b.remainingRatio;
    });

    return sorted;
}

bool MaintenanceEngine::resetItem(const std::string& itemId,
                                  uint32_t currentOdoKm,
                                  uint32_t currentEpochDays,
                                  const std::string& currentDateStr,
                                  ServiceHistoryRecord& outRecord) {
    for (auto& item : m_items) {
        if (item.id == itemId) {
            uint32_t prevOdo = item.lastKm;
            uint32_t delta = (currentOdoKm >= prevOdo) ? (currentOdoKm - prevOdo) : 0;
            uint32_t nextDue = item.intervalKm > 0 ? (currentOdoKm + item.intervalKm) : 0;

            outRecord.itemName = item.name;
            outRecord.dateStr = currentDateStr;
            outRecord.odoKm = currentOdoKm;
            outRecord.prevOdoKm = prevOdo;
            outRecord.deltaKm = delta;
            outRecord.nextDueKm = nextDue;

            m_history.insert(m_history.begin(), outRecord);

            // Update item state
            item.lastKm = currentOdoKm;
            item.lastEpoch = currentEpochDays;
            item.lastDate = currentDateStr;

            // Re-evaluate immediately
            evaluate(currentOdoKm, currentEpochDays);
            return true;
        }
    }
    return false;
}

bool MaintenanceEngine::updateIntervalKm(const std::string& itemId, uint32_t newIntervalKm) {
    for (auto& item : m_items) {
        if (item.id == itemId) {
            item.intervalKm = newIntervalKm;
            return true;
        }
    }
    return false;
}

bool MaintenanceEngine::setItemVisibility(const std::string& itemId, bool visible) {
    for (auto& item : m_items) {
        if (item.id == itemId) {
            item.visible = visible;
            return true;
        }
    }
    return false;
}

bool MaintenanceEngine::addItem(const std::string& name, uint32_t intervalKm, uint16_t intervalDays, uint32_t currentOdoKm, const std::string& currentDateStr) {
    if (m_items.size() >= 24) return false;
    std::string normalized;
    if(name.empty() || !StorageManager::normalizeUtf8("🔧 " + name,79,normalized))return false;
    uint32_t suffix=1;
    std::string newId;
    do { newId="custom_"+std::to_string(suffix++); } while(std::any_of(m_items.begin(),m_items.end(),[&](const MaintenanceItem& x){return x.id==newId;}));
    MaintenanceItem item{};
    item.id = newId;
    item.name = normalized;
    item.intervalKm = intervalKm;
    item.intervalDays = intervalDays;
    item.warnPct = 10;
    item.lastKm = currentOdoKm;
    item.lastEpoch = 0;
    item.lastDate = currentDateStr;
    item.visible = true;
    m_items.push_back(item);
    return true;
}

bool MaintenanceEngine::removeItem(const std::string& itemId) {
    auto it = std::remove_if(m_items.begin(), m_items.end(), [&itemId](const MaintenanceItem& x) {
        return x.id == itemId;
    });
    if (it != m_items.end()) {
        m_items.erase(it, m_items.end());
        return true;
    }
    return false;
}

std::vector<MaintenanceItem>& MaintenanceEngine::getItems() {
    return m_items;
}

const std::vector<MaintenanceItem>& MaintenanceEngine::getItems() const {
    return m_items;
}

const std::vector<ServiceHistoryRecord>& MaintenanceEngine::getHistory() const {
    return m_history;
}

void MaintenanceEngine::addHistoryRecord(const ServiceHistoryRecord& record) {
    m_history.insert(m_history.begin(), record);
}

AlertStatus MaintenanceEngine::getHighestAlertStatus() const {
    AlertStatus highest = AlertStatus::OK;
    for (const auto& item : m_items) {
        if (!item.visible) continue;
        if (item.status == AlertStatus::OVERDUE) return AlertStatus::OVERDUE;
        if (item.status == AlertStatus::DUE_SOON) highest = AlertStatus::DUE_SOON;
    }
    return highest;
}

uint32_t MaintenanceEngine::getActiveAlertCount() const {
    uint32_t count = 0;
    for (const auto& item : m_items) {
        if (!item.visible) continue;
        if (item.status != AlertStatus::OK) count++;
    }
    return count;
}
