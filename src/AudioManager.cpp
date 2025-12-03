#include "AudioManager.h"
#include "SD.h"
#include "esp_system.h" // for esp_restart()
#include "Config.h"
#include "Settings.h"

void AudioManager::begin(int bclk, int lrclk, int din) {
  _bclk = bclk; _lrclk = lrclk; _din = din;
  _audio.setPinout(_bclk, _lrclk, _din);
  
  // Initialize settings and load saved volume
  Settings::begin();
  loadVolume();  // This will load the saved volume or use default
  
  _consecutiveFails = 0;
  
  // Debug output
  Serial.printf("AudioManager: Initialized with volume %d\n", _currentVolume);
}

void AudioManager::loop() {
  _audio.loop();
}

bool AudioManager::isRunning() { return _audio.isRunning(); }
void AudioManager::stop() { if (_audio.isRunning()) { _audio.stopSong(); } _currentPath = String(); }
// setVolume and getVolume are defined in the header file
int AudioManager::getConsecutiveFails() { return _consecutiveFails; }
void AudioManager::resetConsecutiveFails(){ _consecutiveFails = 0; }

bool AudioManager::start(const String &path) {
  if (!SD.exists(path)) {
    _consecutiveFails++;
    return false;
  }

  // Save current volume before stopping
  int currentVolume = _currentVolume;

  // If already running, ask to stop and give the library a short time to settle.
  if (_audio.isRunning()) {
    _audio.stopSong();
    unsigned long t0 = millis();
    while (_audio.isRunning() && millis() - t0 < 400) {
      _audio.loop();
      delay(5);
    }
  }

  // Restore volume before starting new track
  _audio.setVolume(currentVolume);

  // Try connecttoFS with retries and SD re-init between retries.
  const int maxTries = 3;
  for (int attempt = 1; attempt <= maxTries; ++attempt) {
    // small guard: yield and a brief delay to let other tasks run
    yield();
    delay(10);

    bool ok = _audio.connecttoFS(SD, path.c_str());
    if (ok) {
      // allow audio library to spin a little so isRunning updates
      for (int i=0;i<5;i++){ _audio.loop(); delay(5); }
      _consecutiveFails = 0;
      _currentPath = path;
      return true;
    } else {
      _consecutiveFails++;
      // attempt re-init SD before retrying
      SD.end();
      delay(40);
      SD.begin(SD_CS);
      // short delay before next attempt
      delay(80);
    }
  }

  _currentPath = String();
  return false;
}
