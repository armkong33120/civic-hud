#include "OdometerEngine.h"
#include <cmath>

static OdometerEngine* s_odoInstance = nullptr;

OdometerEngine* OdometerEngine::getInstance() {
    return s_odoInstance;
}

void OdometerEngine::setInstance(OdometerEngine* instance) {
    s_odoInstance = instance;
}

OdometerEngine::OdometerEngine()
    : m_currentOdo(0)
    , m_lastPersistedOdo(0)
    , m_tripStartOdo(0)
    , m_lastPid31(0)
    , m_hasPid31Baseline(false)
    , m_fractionalKm(0.0f)
{
    s_odoInstance = this;
}

void OdometerEngine::calibrate(uint32_t actualOdoKm) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentOdo = actualOdoKm;
    m_lastPersistedOdo = actualOdoKm;
    m_tripStartOdo = actualOdoKm;
    m_fractionalKm = 0.0f;

}

void OdometerEngine::updateFromPid31(uint32_t currentPid31Km) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (currentPid31Km > 65535) return;
    if (!m_hasPid31Baseline) {
        m_lastPid31 = currentPid31Km;
        m_hasPid31Baseline = true;

        return;
    }

    if (currentPid31Km >= m_lastPid31) {
        // Normal progression: accumulated distance
        uint32_t delta = currentPid31Km - m_lastPid31;
        m_currentOdo += delta;
    } else {
        // Existing M1 wrap/reset policy retained; ambiguous clears require
        // physical ECU validation before a future policy change.
        if (m_lastPid31 > 60000 && currentPid31Km < 5000) {
            m_currentOdo += (65536 - m_lastPid31) + currentPid31Km;
        } else {
            m_currentOdo += currentPid31Km;
        }
    }

    m_lastPid31 = currentPid31Km;

}

void OdometerEngine::updateFromPid31(uint16_t currentPid31Km) {
    updateFromPid31(static_cast<uint32_t>(currentPid31Km));
}

void OdometerEngine::updateFromSpeed(float speedKmh, float dtSeconds) {
    if (!std::isfinite(speedKmh) || !std::isfinite(dtSeconds) || speedKmh <= 0.0f || speedKmh > 255.0f || dtSeconds <= 0.0f) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    // Distance = speed * time
    double deltaKm = (static_cast<double>(speedKmh) * static_cast<double>(dtSeconds)) / 3600.0;
    m_fractionalKm += static_cast<float>(deltaKm);

    while (m_fractionalKm >= 0.99999f) {
        m_currentOdo += 1;
        m_fractionalKm -= 1.0f;
    }
    if (m_fractionalKm < 0.0f) {
        m_fractionalKm = 0.0f;
    }

}

void OdometerEngine::updateFromSpeed(uint8_t speedKph, uint32_t deltaTimeMs) {
    updateFromSpeed(static_cast<float>(speedKph), static_cast<float>(deltaTimeMs) / 1000.0f);
}

void OdometerEngine::resetTrip() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tripStartOdo = m_currentOdo;

}

float OdometerEngine::getTripKm() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_currentOdo >= m_tripStartOdo) {
        return static_cast<float>(m_currentOdo - m_tripStartOdo) + m_fractionalKm;
    }
    return 0.0f;
}

uint32_t OdometerEngine::getCurrentOdo() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentOdo;
}

bool OdometerEngine::checkAndClearNvsDirty(uint32_t intervalKm) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_currentOdo >= m_lastPersistedOdo + intervalKm) {
        m_lastPersistedOdo = m_currentOdo;

        return true;
    }

    return false;
}

bool OdometerEngine::hasPid31Baseline() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_hasPid31Baseline;
}

uint32_t OdometerEngine::getLastPid31() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastPid31;
}

void OdometerEngine::setLastPid31(uint32_t lastPid31) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastPid31 = lastPid31;
    m_hasPid31Baseline = true;

}

OdometerSnapshot OdometerEngine::snapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return {m_currentOdo, m_lastPid31, m_hasPid31Baseline, m_fractionalKm};
}
void OdometerEngine::restore(const OdometerSnapshot& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentOdo = m_lastPersistedOdo = m_tripStartOdo = value.odometerKm;
    m_lastPid31 = value.lastPid31;
    m_hasPid31Baseline = value.hasPid31Baseline && value.lastPid31 <= 65535;
    m_fractionalKm = std::isfinite(value.fractionalKm) && value.fractionalKm >= 0 && value.fractionalKm < 1 ? value.fractionalKm : 0;
}
bool OdometerEngine::persistenceDue(uint32_t intervalKm) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentOdo >= m_lastPersistedOdo && m_currentOdo - m_lastPersistedOdo >= intervalKm;
}
void OdometerEngine::markPersisted(const OdometerSnapshot& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastPersistedOdo = value.odometerKm;
}
void OdometerEngine::resetPid31Baseline() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hasPid31Baseline = false;
}
