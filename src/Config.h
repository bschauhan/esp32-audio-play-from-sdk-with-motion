#ifndef CONFIG_H
#define CONFIG_H


#include <Arduino.h>


// Pins
#define I2S_BCLK 27
#define I2S_LRCLK 14
#define I2S_DIN 12
#define SD_CS 13
#define PIR_PIN 26
#define SD_SCK 25
#define SD_MISO 32
#define SD_MOSI 33


// Behavior
#define DEFAULT_VOLUME 11
#define DHUN_SESSION_TIMEOUT_MS (10UL * 60UL * 1000UL)
#define STATE_CHECK_INTERVAL_MS 120


#endif // CONFIG_H