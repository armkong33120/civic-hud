#pragma once
#include "MaintenanceItem.h"
#include <vector>
#include <functional>

class MaintenanceEngine {
public:
    MaintenanceEngine();

    // Initialize with standard Civic FC 1.8 + LPG default items
    void loadDefaults();

    // Run evaluation against current vehicle odometer and days elapsed
    void evaluate(uint32_t currentOdoKm, uint32_t currentEpochDays = 0);

    // Get sorted view according to urgency rules (OVERDUE > DUE_SOON > OK, then by ratio)
    std::vector<MaintenanceItem> getSortedItems() const;

    // Reset item upon Slide-to-Confirm action
    bool resetItem(const std::string& itemId,
                   uint32_t currentOdoKm,
                   uint32_t currentEpochDays,
                   const std::string& currentDateStr,
                   ServiceHistoryRecord& outRecord);

    // Configuration methods (for Web Config 192.168.1.1)
    bool updateIntervalKm(const std::string& itemId, uint32_t newIntervalKm);
    bool setItemVisibility(const std::string& itemId, bool visible);
    bool addItem(const std::string& name, uint32_t intervalKm, uint16_t intervalDays, uint32_t currentOdoKm, const std::string& currentDateStr);
    bool removeItem(const std::string& itemId);

    // Get all items (raw list)
    std::vector<MaintenanceItem>& getItems();
    const std::vector<MaintenanceItem>& getItems() const;

    // History logs
    const std::vector<ServiceHistoryRecord>& getHistory() const;
    void addHistoryRecord(const ServiceHistoryRecord& record);

    // Overall system alert level (for RGB LED: 0=OK/Green, 1=DueSoon/Yellow, 2=Overdue/Red)
    AlertStatus getHighestAlertStatus() const;
    uint32_t getActiveAlertCount() const;

private:
    std::vector<MaintenanceItem> m_items;
    std::vector<ServiceHistoryRecord> m_history;
};
