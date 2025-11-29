#include "AudioManager.h"
#include "SD.h"
#include "esp_system.h" // for esp_restart()
#include "Config.h"

void AudioManager::begin(int bclk, int lrclk, int din) {
  _bclk = bclk; _lrclk = lrclk; _din = din;
  _audio.setPinout(_bclk, _lrclk, _din);
  _audio.setVolume(_currentVolume);
  _consecutiveFails = 0;
}

void AudioManager::loop() {
  _audio.loop();
}

bool AudioManager::isRunning() { return _audio.isRunning(); }
void AudioManager::stop() { if (_audio.isRunning()) _audio.stopSong(); }
void AudioManager::setVolume(int v) { if (v<0) v=0; if (v>21) v=21; _currentVolume=v; _audio.setVolume(v); Serial.printf("AudioManager: setVolume %d\n", v); }
int AudioManager::getVolume() { return _currentVolume; }
int AudioManager::getConsecutiveFails() { return _consecutiveFails; }
void AudioManager::resetConsecutiveFails(){ _consecutiveFails = 0; }

bool AudioManager::start(const String &path) {
  Serial.print("AudioManager::start -> ");
  Serial.println(path);

  if (!SD.exists(path)) {
    Serial.println("  ❌ SD.exists -> false");
    _consecutiveFails++;
    return false;
  }

  // If already running, ask to stop and give the library a short time to settle.
  if (_audio.isRunning()) {
    Serial.println("  audio isRunning -> stopping current playback first");
    _audio.stopSong();
    unsigned long t0 = millis();
    while (_audio.isRunning() && millis() - t0 < 400) {
      _audio.loop();
      delay(5);
    }
    if (_audio.isRunning()) {
      Serial.println("  ⚠ audio still running after stop request");
    } else {
      Serial.println("  stopped successfully");
    }
  }

  // Try connecttoFS with retries and SD re-init between retries.
  const int maxTries = 3;
  for (int attempt = 1; attempt <= maxTries; ++attempt) {
    Serial.printf("  attempt %d/%d - freeHeap=%u\n", attempt, maxTries, (unsigned int)ESP.getFreeHeap());

    // small guard: yield and a brief delay to let other tasks run
    yield();
    delay(10);

    bool ok = _audio.connecttoFS(SD, path.c_str());
    if (ok) {
      // allow audio library to spin a little so isRunning updates
      for (int i=0;i<5;i++){ _audio.loop(); delay(5); }
      Serial.println("  ▶ connecttoFS returned OK");
      _consecutiveFails = 0;
      return true;
    } else {
      Serial.println("  ❌ connecttoFS failed");
      _consecutiveFails++;
      // attempt re-init SD before retrying
      Serial.println("  attempting SD re-init");
      SD.end();
      delay(40);
      if (!SD.begin(SD_CS)) {
        Serial.println("  ❌ SD.begin() failed during recovery");
      } else {
        Serial.println("  ✅ SD.begin() succeeded during recovery");
      }
      // short delay before next attempt
      delay(80);
    }
  }

  Serial.println("  ✖ All connect attempts failed");
  return false;
}
