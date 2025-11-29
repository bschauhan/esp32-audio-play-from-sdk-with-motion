#include "Config.h"
#include "FileScanner.h"
#include "AudioManager.h"
#include "StateMachine.h"
#include "WebHandler.h"


AudioManager audioManager;
FileScanner fileScanner;
StateMachine stateMachine;
WebHandler webHandler;

void setup() {
    Serial.begin(115200);
    pinMode(PIR_PIN, INPUT);

    Serial.println("\n🚀 ESP32 Audio - Multi-file build\n");

    // init SD
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
    if (!SD.begin(SD_CS)) {
        Serial.println("❌ SD begin failed - halting");
        while (true) delay(1000);
    }

    // scan files
    fileScanner.begin(SD);
    // fileScanner.scanFolder("/short");
    fileScanner.scanFolder("/dhun");

    // init audio
    audioManager.begin(I2S_BCLK, I2S_LRCLK, I2S_DIN);
    audioManager.setVolume(DEFAULT_VOLUME);

    // init web
    webHandler.begin(&audioManager, &fileScanner);

    // init state machine
    stateMachine.begin(&audioManager, &fileScanner);

    Serial.println("✅ System Ready");
}

void loop() {
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