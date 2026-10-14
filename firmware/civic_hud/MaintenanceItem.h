#pragma once
#include <string>
#include <cstdint>

enum class AlertStatus : uint8_t {
    OK = 0,         // 🟢 Normal (healthy)
    DUE_SOON = 1,   // 🟡 Approaching service limit (<= warnPct remaining)
    OVERDUE = 2     // 🔴 Overdue (exceeded limit)
};

struct ServiceHistoryRecord {
    std::string itemName;
    std::string dateStr;
    uint32_t odoKm;
    uint32_t prevOdoKm;
    uint32_t deltaKm;
    uint32_t nextDueKm;
};

struct MaintenanceItem {
    std::string id;
    std::string name;          // e.g. "🛢️ น้ำมันเครื่อง + กรอง"
    uint32_t intervalKm;       // 0 if distance is not used
    uint16_t intervalDays;     // 0 if time is not used
    uint8_t warnPct;           // e.g. 15 (%)
    uint32_t lastKm;           // Mileage at last service
    uint32_t lastEpoch;        // Timestamp / days offset
    std::string lastDate;      // e.g. "12 ส.ค. 26"
    bool visible;              // Toggle display

    // Runtime Evaluation
    AlertStatus status;
    float remainingRatio;      // min(kmLeft/intervalKm, daysLeft/intervalDays)
    float usedPct;             // 0% .. 100% for progress bar
    int32_t kmLeft;
    int32_t daysLeft;
    bool isLimitedByTime;
};
