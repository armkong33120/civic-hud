#pragma once

// =========================================================================
// Hardware Pin Mapping for CYD (ESP32-2432S028 / HW-458)
// =========================================================================

// --- Display (TFT 2.8" 240x320 ILI9341 / ST7789) ---
#define TFT_MOSI            13
#define TFT_MISO            12
#define TFT_SCLK            14
#define TFT_CS              15
#define TFT_DC              2
#define TFT_RST             -1    // Connected to EN/RST
#define TFT_BL              21    // Backlight (PWM controllable)

// --- Resistive Touch Screen (XPT2046) ---
#define XPT2046_CLK         25
#define XPT2046_CS          33
#define XPT2046_DIN         32    // MOSI to touch controller
#define XPT2046_DO          39    // MISO from touch controller (input only)
#define XPT2046_IRQ         36    // Touch interrupt pin

// --- RGB Status LED (Active Low on CYD) ---
#define RGB_LED_R           17
#define RGB_LED_G           4
#define RGB_LED_B           16

// --- Audio / Buzzer ---
#define AUDIO_PIN           26    // DAC / PWM speaker output

// --- Light Dependent Resistor (LDR) Sensor ---
#define LDR_SENSOR_PIN      34    // ADC1 input for auto-dimming

// --- MicroSD Card (SPI) ---
#define SD_CS               5
#define SD_MOSI             23
#define SD_CLK              18
#define SD_MISO             19

// --- Additional Expansion Headers ---
#define EXP1_SDA            21
#define EXP1_SCL            22
#define EXP1_IO35           35
#define EXP2_IO27           27
