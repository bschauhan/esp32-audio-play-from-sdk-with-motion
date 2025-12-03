#include "AudioManager.h"
#include "SD.h"
#include "esp_system.h" // for esp_restart()
#include "Config.h"
#include "Settings.h"

void AudioManager::begin(int bclk, int lrclk, int din) {
  _bclk = bclk; _lrclk = lrclk; _din = din;
  _audio.setPinout(_bclk, _lrclk, _din);
  
  // Initialize settings and load saved volume and EQ
  Settings::begin();
  loadVolume();  // This will load the saved volume or use default
  loadEQSettings(); // Load equalizer settings
  
  _consecutiveFails = 0;
  
  // Debug output
  Serial.printf("AudioManager: Initialized with volume %d, EQ (B:%d M:%d T:%d)\n", 
                _currentVolume, _eqBass, _eqMid, _eqTreble);
}

void AudioManager::loop() {
  _audio.loop();
  
  // Handle crossfade updates
  if (_isCrossfading) {
    updateCrossfade();
  }
}

// Equalizer functions
void AudioManager::setBass(int level) {
  if (level < -12) level = -12;
  if (level > 12) level = 12;
  _eqBass = level;
  applyEQ();
  saveEQSettings();
}

void AudioManager::setMid(int level) {
  if (level < -12) level = -12;
  if (level > 12) level = 12;
  _eqMid = level;
  applyEQ();
  saveEQSettings();
}

void AudioManager::setTreble(int level) {
  if (level < -12) level = -12;
  if (level > 12) level = 12;
  _eqTreble = level;
  applyEQ();
  saveEQSettings();
}

void AudioManager::applyEQ() {
  // Apply 3-band equalizer using the ESP32-audioI2S library
  // The library supports setTone() with bass, mid, and treble parameters
  
  // Convert -12 to +12 range to library's int8_t range
  int8_t bassLib = (int8_t)constrain(_eqBass, -12, 12);
  int8_t midLib = (int8_t)constrain(_eqMid, -12, 12);
  int8_t trebleLib = (int8_t)constrain(_eqTreble, -12, 12);
  
  // Set 3-band equalizer (low-pass = bass, band-pass = mid, high-pass = treble)
  _audio.setTone(bassLib, midLib, trebleLib);
}

void AudioManager::loadEQSettings() {
  _eqBass = Settings::loadInt("eq_bass", 0);
  _eqMid = Settings::loadInt("eq_mid", 0);
  _eqTreble = Settings::loadInt("eq_treble", 0);
  
  // Clamp values to valid range
  if (_eqBass < -12) _eqBass = 0;
  if (_eqBass > 12) _eqBass = 0;
  if (_eqMid < -12) _eqMid = 0;
  if (_eqMid > 12) _eqMid = 0;
  if (_eqTreble < -12) _eqTreble = 0;
  if (_eqTreble > 12) _eqTreble = 0;
  
  applyEQ();
}

void AudioManager::saveEQSettings() {
  Settings::saveInt("eq_bass", _eqBass);
  Settings::saveInt("eq_mid", _eqMid);
  Settings::saveInt("eq_treble", _eqTreble);
}

// Crossfade functions
bool AudioManager::startWithCrossfade(const String &path) {
  if (!SD.exists(path)) {
    return false;
  }
  
  // If nothing is playing, just start normally
  if (!_audio.isRunning()) {
    return start(path);
  }
  
  // Set up crossfade
  _nextPath = path;
  _isCrossfading = true;
  _crossfadeStart = millis();
  _originalVolume = _currentVolume;
  
  Serial.printf("AudioManager: Starting crossfade to %s\n", path.c_str());
  return true;
}

void AudioManager::updateCrossfade() {
  if (!_isCrossfading) return;
  
  unsigned long elapsed = millis() - _crossfadeStart;
  float progress = (float)elapsed / (float)_crossfadeTime;
  
  if (progress >= 1.0) {
    // Crossfade complete - switch to new track
    _isCrossfading = false;
    _audio.stopSong();
    
    // Small delay to ensure stop is complete
    delay(50);
    
    // Start the new track
    bool success = start(_nextPath);
    if (success) {
      // Restore original volume
      _audio.setVolume(_originalVolume);
    }
    
    Serial.printf("AudioManager: Crossfade complete, new track started: %s\n", success ? "OK" : "FAIL");
    return;
  }
  
  // Fade out current track
  int fadeOutVolume = _originalVolume * (1.0 - progress);
  _audio.setVolume(fadeOutVolume);
  
  // When we're halfway through the crossfade, start the new track
  if (progress >= 0.5 && _nextPath.length() > 0) {
    // Save current state
    String tempPath = _nextPath;
    _nextPath = ""; // Prevent re-triggering
    
    // Start new track at low volume
    if (_audio.connecttoFS(SD, tempPath.c_str())) {
      // Fade in new track
      int fadeInVolume = _originalVolume * ((progress - 0.5) * 2.0);
      _audio.setVolume(fadeInVolume);
    }
  }
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
