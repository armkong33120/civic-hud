#pragma once
#include <cstdint>
#include <mutex>

struct OdometerSnapshot {
    uint32_t odometerKm = 0;
    uint32_t lastPid31 = 0;
    bool hasPid31Baseline = false;
    float fractionalKm = 0.0f;
};

#ifdef ARDUINO
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

class OdometerEngine {
public:
    OdometerEngine();

    OdometerSnapshot snapshot() const;
    void restore(const OdometerSnapshot& value);
    bool persistenceDue(uint32_t intervalKm = 5) const;
    void markPersisted(const OdometerSnapshot& value);
    void resetPid31Baseline();

    // Global / singleton instance access
    static OdometerEngine* getInstance();
    static void setInstance(OdometerEngine* instance);

    // Set initial actual vehicle odometer reading
    void calibrate(uint32_t actualOdoKm);

    // Primary method: Process OBD-II PID 0x31 (Distance since codes cleared)
    void updateFromPid31(uint32_t currentPid31Km);
    void updateFromPid31(uint16_t currentPid31Km);
    void updateFromPid31(int currentPid31Km) { updateFromPid31(static_cast<uint32_t>(currentPid31Km)); }

    // Fallback method: Integrate speed when PID 0x31 is unavailable
    void updateFromSpeed(float speedKmh, float dtSeconds);
    void updateFromSpeed(uint8_t speedKph, uint32_t deltaTimeMs);

    // Trip odometer management
    void resetTrip();
    float getTripKm() const;

    // Get current total vehicle odometer
    uint32_t getCurrentOdo() const;
    uint32_t getOdometerKm() const { return getCurrentOdo(); }

    // Check if odometer has progressed enough to warrant an NVS write
    bool checkAndClearNvsDirty(uint32_t intervalKm = 5);

    // Check if baseline is established
    bool hasPid31Baseline() const;

    // Last known PID 31 distance for persistence
    uint32_t getLastPid31() const;
    void setLastPid31(uint32_t lastPid31);

private:
    uint32_t m_currentOdo;
    uint32_t m_lastPersistedOdo;
    uint32_t m_tripStartOdo;
    
    // PID 0x31 delta tracking
    uint32_t m_lastPid31;
    bool m_hasPid31Baseline;

    // Fallback speed integration
    float m_fractionalKm;

    mutable std::mutex m_mutex;
};
