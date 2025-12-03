#ifndef CONFIG_H
#define CONFIG_H


#include <Arduino.h>


// Pins
// I2S Audio
#define I2S_BCLK 27
#define I2S_LRCLK 14
#define I2S_DIN 12

// SD Card
#define SD_CS 13
#define SD_SCK 25
#define SD_MISO 32
#define SD_MOSI 33

// Motion Sensor
#define PIR_PIN 26

// RTC (DS3231)
#define RTC_I2C_ADDR 0x68  // Default I2C address for DS3231
#define I2C_SDA 21         // Default SDA pin for ESP32
#define I2C_SCL 22         // Default SCL pin for ESP32


// RTC Configuration
#define RTC_UPDATE_INTERVAL_MS 1000  // How often to check RTC (ms)

// Behavior
#define DEFAULT_VOLUME 11
#define DHUN_SESSION_TIMEOUT_MS (5UL * 60UL * 1000UL)
#define STATE_CHECK_INTERVAL_MS 120

// Hourly chime config
#define CHIME_START_HOUR 6      // inclusive, 24h format
#define CHIME_END_HOUR   23     // inclusive, 24h format
#define CHIME_WINDOW_SEC 5      // trigger window at top of hour (seconds)

// Audio file paths (must exist on SD)
#define GREETING_PATH "/jay-swaminarayan.mp3"
#define BELL_PATH     "/digital_clock/bell.mp3"
#define HOURS_DIR     "/digital_clock/hours/"

// WiFi Configuration
#define WIFI_SSID       "error"      // Replace with your WiFi network name
#define WIFI_PASSWORD   "bharat@123"  // Replace with your WiFi password
#define WIFI_TIMEOUT_MS 20000                 // 20 seconds timeout for WiFi connection

#endif // CONFIG_H