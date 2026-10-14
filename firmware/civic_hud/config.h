#pragma once
#include <stdint.h>

// =========================================================================
// Project Configuration for Honda Civic FC 1.8 Maintenance HUD
// =========================================================================

#define APP_NAME            "CivicHUD"
#define APP_VERSION         "1.0.0"

// --- Wi-Fi AP & Web Settings ---
#define WIFI_AP_SSID        "CivicHUD"
#define WIFI_AP_PASS        ""                // Open AP for easy connection, or set password
#define WIFI_AP_IP          "192.168.1.1"     // Local gateway IP
#define WEB_SERVER_PORT     80

// --- OBD-II Bluetooth Classic Connection ---
#define OBD_BT_DEVICE_NAME  "OBDII"           // Default ELM327 Bluetooth device name
#define OBD_BT_PIN          "1234"            // Default pairing PIN for ELM327
#define OBD_RETRY_INTERVAL_MS 15000

// --- Odometer Calibration ---
#define DEFAULT_INITIAL_ODO 98540             // Default initial km if not calibrated
#define NVS_SAVE_INTERVAL_KM 5                // Save odometer to NVS every 5 km
#define ODO_SPEED_MIN_KMH    3.0f             // Speeds below this considered stopped

// --- Limits & Capacities ---
#define MAX_MAINT_ITEMS     24
#define MAX_HISTORY_LOGS    50
#define CSV_HISTORY_PATH    "/service_history.csv"
