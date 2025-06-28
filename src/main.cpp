#include "Arduino.h"
#include "Audio.h"
#include "SD.h"
#include "SPI.h"

#define I2S_BCLK  26   // Bit Clock
#define I2S_LRCLK 25   // Left/Right Clock (Word Select)
#define I2S_DIN   22   // Data In
#define SD_CS     5    // SD Card Chip Select Pin
#define PIR_PIN   4    // PIR Motion Sensor Output Pin

Audio audio;
bool isPlaying = false;  
unsigned long lastMotionTime = 0;
const unsigned long motionTimeout = 5000; // Pause after 5 seconds of no motion

String shortFiles[20];  // Array to store MP3 filenames from /short
int shortFileCount = 0;

String dhunFiles[20];   // Array to store MP3 filenames from /dhun
int dhunFileCount = 0;

// Function to list MP3 files in a given folder and store their full paths
void listMP3Files(fs::FS &fs, const char *dirname, String fileArray[], int &count) {
    Serial.print("📂🔍 Scanning SD card folder: ");
    Serial.println(dirname);
    
    File root = fs.open(dirname);
    if (!root || !root.isDirectory()) {
        Serial.println("❌ Failed to open folder: " + String(dirname));
        return;
    }

    while (true) {
        File file = root.openNextFile();
        if (!file) break;
        if (!file.isDirectory()) {
            String filename = file.name();
            if (filename.endsWith(".mp3") || filename.endsWith(".MP3")) {
                if (count < 20) {
                    fileArray[count] = String(dirname) + "/" + filename;  // Store full path
                    Serial.println("🎵 Found: " + fileArray[count]);
                    count++;
                } else {
                    Serial.println("⚠ Too many files! Only storing first 20.");
                    break;
                }
            }
        }
        file.close();
    }

    Serial.printf("✅ Total MP3 files found in '%s': %d\n", dirname, count);
}

// Function to play an audio file and wait for it to finish
void playAudioFile(String filePath) {
    if (!SD.exists(filePath)) {
        Serial.println("❌ File not found: " + filePath);
        return;
    }

    Serial.println("▶ Playing: " + filePath);
    audio.connecttoFS(SD, filePath.c_str());

    // Wait for audio to finish
    while (audio.isRunning()) {
        audio.loop();
    }
}

// Function to play the greeting audio
void playGreeting() {
    playAudioFile("/jay-swaminarayan.mp3");
}

// Function to play a random MP3 from a given folder
void playRandomMP3(String fileArray[], int count, String folderName) {
    if (count == 0) {
        Serial.println("⚠ No MP3 files found in '" + folderName + "' folder.");
        return;
    }

    int randomIndex = random(count);  // Generate valid random index
    Serial.printf("🔀 Randomly selected index %d from '%s'\n", randomIndex, folderName.c_str());
    
    playAudioFile(fileArray[randomIndex]);
}

void setup() {
    Serial.begin(115200);
    pinMode(PIR_PIN, INPUT);

    Serial.println("🎵🔄 Booting up...");
    Serial.println("📂🧐 Initializing SD card...");

    if (!SD.begin(SD_CS)) {
        Serial.println("❌ SD Card Initialization Failed! Please check connections.");
        return;
    }
    Serial.println("✅ SD Card Ready!");

    // Initialize random seed from an analog pin
    randomSeed(analogRead(0));

    // Scan both folders and store full paths
    listMP3Files(SD, "/short", shortFiles, shortFileCount);
    listMP3Files(SD, "/dhun", dhunFiles, dhunFileCount);

    audio.setPinout(I2S_BCLK, I2S_LRCLK, I2S_DIN);
    audio.setVolume(21);   // Max allowed in ESP32-audioI2S

    Serial.println("🚀 System Ready! Waiting for Motion... 🏃‍♂️");
}

void loop() {
    int motion = digitalRead(PIR_PIN);

    if (motion == HIGH) { 
        lastMotionTime = millis();

        if (!isPlaying) {
            Serial.println("🎵 Motion Detected! Playing Sequence... ▶");

            playGreeting();  // 🚀 Play greeting `/jay-swaminarayan.mp3`
            delay(1000);     // Small delay to avoid overlap

            playRandomMP3(shortFiles, shortFileCount, "/short");  // 🔀 Play a random MP3 from /short
            delay(1000);

            playRandomMP3(dhunFiles, dhunFileCount, "/dhun");  // 🔀 Play a random MP3 from /dhun

            isPlaying = true;
        }
    } 

    // Reset playing state when motion stops
    if (isPlaying && millis() - lastMotionTime > motionTimeout) {
        Serial.println("⏸ No motion detected, system reset.");
        isPlaying = false;
    }

    if (isPlaying) {
        audio.loop(); // Keep playing audio
    }
}
