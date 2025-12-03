#include "Config.h"
#include "FileScanner.h"
#include "AudioManager.h"
#include "StateMachine.h"
#include "WebHandler.h"
#include "TimeSync.h"
#include <WiFi.h>


AudioManager audioManager;
FileScanner fileScanner;
StateMachine stateMachine;
WebHandler webHandler;

void setup() {
    Serial.begin(115200);
    delay(1000);
    pinMode(PIR_PIN, INPUT);

    // Initialize WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // Don't wait for WiFi to connect - we'll check it in the loop
    Serial.println("Connecting to WiFi...");

    // init SD
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
    if (!SD.begin(SD_CS)) {
        while (true) delay(1000);
    }

    // scan files
    fileScanner.begin(SD);
    fileScanner.scanFolder("/dhun");

    // init audio
    audioManager.begin(I2S_BCLK, I2S_LRCLK, I2S_DIN);
    // audioManager.setVolume(DEFAULT_VOLUME);

    // init state machine first
    stateMachine.begin(&audioManager, &fileScanner);
    
    // init web (pass state machine for chime testing)
    webHandler.begin(&audioManager, &fileScanner, &stateMachine);

}

void loop() {
    // Try to sync time if WiFi is connected
    static bool timeSynced = false;
    if (WiFi.status() == WL_CONNECTED) {
        if (!timeSynced) {
            if (TimeSync::syncIfNeeded(stateMachine.getRtc())) {
                timeSynced = true;
                Serial.println("Time synchronized with NTP server");
            }
        }
    }

    // Keep audio.loop() as fast as possible
    audioManager.loop();

    // lightweight server handling
    webHandler.handleClient();

    // lightweight motion sampling
    int motion = digitalRead(PIR_PIN);
    stateMachine.motionSample(motion == HIGH);

    // run heavier state checks at interval
    stateMachine.periodic();
}