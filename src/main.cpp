#include "Arduino.h"
#include "Audio.h"
#include "SD.h"
#include "SPI.h"
#include "WiFi.h"
#include "WebServer.h"

#define I2S_BCLK  27   // Bit Clock
#define I2S_LRCLK 14   // Left/Right Clock (Word Select)
#define I2S_DIN   12   // Data In
#define SD_CS     13   // SD Card Chip Select Pin
#define PIR_PIN   26   // PIR Motion Sensor Output Pin

#define SD_SCK    25
#define SD_MISO   32
#define SD_MOSI   33

Audio audio;
bool isPlaying = false;  
unsigned long lastMotionTime = 0;
const unsigned long motionTimeout = 5000; // Pause after 5 seconds of no motion
unsigned long motionHighStart = 0;       
const unsigned long continuousMotionLimit = 600000; 
bool pirPrevState = false;                 
bool playbackAborted = false;
bool audioEnabled = true;
int currentVolume = 21;
WebServer server(80);

void handleRoot();
void handleVolume();
void handlePower();
void handleStatus();

String shortFiles[20];  
int shortFileCount = 0;

String dhunFiles[20];   
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

    if (!audioEnabled) {
        return;
    }

    Serial.println("▶ Playing: " + filePath);
    audio.connecttoFS(SD, filePath.c_str());

    // Wait for audio to finish
    while (audio.isRunning()) {
        audio.loop();

        // Keep web server responsive during playback
        server.handleClient();

        if (!audioEnabled) {
            audio.stopSong();
            isPlaying = false;
            playbackAborted = true;
            return;
        }

        int motion = digitalRead(PIR_PIN);
        unsigned long now = millis();

        if (motion == HIGH) {
            lastMotionTime = now;
            if (motionHighStart == 0) motionHighStart = now;

            if (now - motionHighStart >= continuousMotionLimit) {
                Serial.println("⏹ Continuous motion for 5 minutes, stopping audio.");
                audio.stopSong();
                isPlaying = false;
                playbackAborted = true;
                return;
            }
        } else {
            motionHighStart = 0;

            if (now - lastMotionTime > motionTimeout) {
                Serial.println("⏸ No motion during playback, stopping audio.");
                audio.stopSong();
                isPlaying = false;
                playbackAborted = true;
                return;
            }
        }
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

    SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
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
    audio.setVolume(currentVolume);

    WiFi.mode(WIFI_AP);
    IPAddress local_IP(192, 168, 10, 1);
    IPAddress gateway(192, 168, 10, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    WiFi.softAP("bharat", "bharat@123");
    IPAddress ip = WiFi.softAPIP();
    Serial.print("🌐 AP IP: ");
    Serial.println(ip);

    server.on("/", handleRoot);
    server.on("/api/volume", handleVolume);
    server.on("/api/power", handlePower);
    server.on("/api/status", handleStatus);
    server.begin();

    Serial.println("🚀 System Ready! Waiting for Motion... 🏃‍♂️");
}

void loop() {
    int motion = digitalRead(PIR_PIN);
    unsigned long now = millis();

    server.handleClient();

    if (motion == HIGH) {
        lastMotionTime = now;
        if (motionHighStart == 0) motionHighStart = now;
    } else {
        motionHighStart = 0;
    }

    if (!audioEnabled && isPlaying) {
        audio.stopSong();
        isPlaying = false;
        playbackAborted = true;
    }

    bool risingEdge = (motion == HIGH && pirPrevState == LOW);
    if (risingEdge && !isPlaying && audioEnabled) {
        Serial.println("🎵 Motion Detected! Playing Sequence... ▶");

        playGreeting();  // 🚀 Play greeting `/jay-swaminarayan.mp3`
        delay(1000);     // Small delay to avoid overlap
        if (playbackAborted) { playbackAborted = false; return; }

        playRandomMP3(shortFiles, shortFileCount, "/short");  // 🔀 Play a random MP3 from /short
        delay(1000);
        if (playbackAborted) { playbackAborted = false; return; }

        playRandomMP3(dhunFiles, dhunFileCount, "/dhun");  // 🔀 Play a random MP3 from /dhun
        if (playbackAborted) { playbackAborted = false; return; }

        isPlaying = true;
    }

    if (isPlaying && now - lastMotionTime > motionTimeout) {
        Serial.println("⏸ No motion detected, system reset.");
        isPlaying = false;
    }

    if (isPlaying) {
        audio.loop();
    }

    pirPrevState = (motion == HIGH);
}

void handleRoot() {
    String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>ESP32 Audio</title><style>body{font-family:sans-serif;margin:20px}label{display:block;margin:12px 0 6px}input[type=range]{width:100%}button, input[type=checkbox]{transform:scale(1.2)}.row{margin:14px 0}</style></head><body><h2>ESP32 Audio Control</h2><div class=\"row\"><label>Power</label><input id=\"power\" type=\"checkbox\"></div><div class=\"row\"><label>Volume</label><input id=\"vol\" type=\"range\" min=\"0\" max=\"21\" step=\"1\"></div><div class=\"row\"><pre id=\"status\"></pre></div><script>async function fetchStatus(){const r=await fetch('/api/status');const j=await r.json();document.getElementById('vol').value=j.volume;document.getElementById('power').checked=j.power;document.getElementById('status').textContent=JSON.stringify(j,null,2);}async function setVol(v){await fetch('/api/volume?level='+v);fetchStatus();}async function setPower(on){await fetch('/api/power?on='+(on?1:0));fetchStatus();}document.getElementById('vol').addEventListener('change',e=>setVol(e.target.value));document.getElementById('power').addEventListener('change',e=>setPower(e.target.checked));fetchStatus();</script></body></html>";
    server.send(200, "text/html", html);
}

void handleVolume() {
    if (!server.hasArg("level")) { server.send(400, "application/json", "{\"error\":\"missing level\"}"); return; }
    int v = server.arg("level").toInt();
    if (v < 0) v = 0; if (v > 21) v = 21;
    currentVolume = v;
    audio.setVolume(currentVolume);
    server.send(200, "application/json", String("{\"ok\":true,\"volume\":") + currentVolume + "}");
}

void handlePower() {
    if (!server.hasArg("on")) { server.send(400, "application/json", "{\"error\":\"missing on\"}"); return; }
    int on = server.arg("on").toInt();
    audioEnabled = (on != 0);
    if (!audioEnabled && isPlaying) { audio.stopSong(); isPlaying = false; playbackAborted = true; }
    server.send(200, "application/json", String("{\"ok\":true,\"power\":") + (audioEnabled?"true":"false") + "}");
}

void handleStatus() {
    String json = String("{\"volume\":") + currentVolume + ",\"power\":" + (audioEnabled?"true":"false") + ",\"isPlaying\":" + (isPlaying?"true":"false") + "}";
    server.send(200, "application/json", json);
}
