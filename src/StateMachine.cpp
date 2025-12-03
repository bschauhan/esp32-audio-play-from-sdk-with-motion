#include "StateMachine.h"
#include "Config.h"
#include "Settings.h"

bool StateMachine::isDNDTime() {
  if (!_dndEnabled) {
    return false; // DND is disabled
  }
  
  DateTime now;
  if (!_rtc.now(now)) {
    return false; // If we can't get the time, don't block audio
  }
  
  int currentHour = now.hour();
  
  // Handle overnight DND (e.g., 10 PM to 6 AM)
  if (_dndStartHour > _dndEndHour) {
    return (currentHour >= _dndStartHour || currentHour < _dndEndHour);
  } 
  // Handle same-day DND (e.g., 11 PM to 1 AM)
  else {
    return (currentHour >= _dndStartHour && currentHour < _dndEndHour);
  }
}

bool StateMachine::startChime() {
  _preemptState = _state;
  if (_audio && _audio->isRunning()) {
    String cur = _audio->getCurrentPath();
    if (cur.length() > 0) {
      _preemptPath = cur;
      _hadPreempt = true;
    }
  }
  
  // Start the first bell
  bool ok = (_audio ? _audio->start(String(BELL_PATH)) : false);
  if (ok) {
    _state = GREETING; // Reuse greeting state for chime sequence
    _isPlaying = true;
    return true;
  } else {
    _inChime = false;
    _chimePhase = CH_NONE;
    
    // Try to resume preempted audio if any
    if (_hadPreempt && _audio) {
      if (_audio->start(_preemptPath)) {
        _state = _preemptState;
        _isPlaying = true;
      } else {
        _state = IDLE;
        _isPlaying = false;
      }
    } else {
      _state = IDLE;
      _isPlaying = false;
    }
    _hadPreempt = false;
    _preemptPath = String();
    return false;
  }
}

void StateMachine::begin(AudioManager *am, FileScanner *fs) {
  _audio = am;
  _fs = fs;
  _isPlaying = false;
  _state = IDLE;
  _lastTriggerAttempt = 0;
  _lastMotion = 0;
  
  // Initialize Settings
  Settings::begin();
  
  // Load DND settings if they exist
  if (Settings::isInitialized()) {
    auto settings = Settings::loadDNDSettings();
    _dndEnabled = settings.enabled;
    _dndStartHour = settings.startHour;
    _dndEndHour = settings.endHour;
    _chimeWindowSec = settings.windowSec;
    
    Serial.println("Loaded DND settings from storage:");
    Serial.printf("  DND Enabled: %s\n", _dndEnabled ? "Yes" : "No");
    Serial.printf("  DND Hours: %02d:00 - %02d:00\n", _dndStartHour, _dndEndHour);
    Serial.printf("  Chime Window: %d seconds\n", _chimeWindowSec);
  } else {
    // Save default settings
    Settings::DNDSettings defaultSettings;
    defaultSettings.enabled = _dndEnabled;
    defaultSettings.startHour = _dndStartHour;
    defaultSettings.endHour = _dndEndHour;
    defaultSettings.windowSec = _chimeWindowSec;
    Settings::saveDNDSettings(defaultSettings);
    
    Serial.println("Initialized with default DND settings");
  }
  
  // Initialize RTC with better error handling
  if (!_rtc.begin()) {
    Serial.println("ERROR: Couldn't find RTC");
    Serial.println("Please check the following:");
    Serial.println("1. Is the RTC module (DS3231) properly connected?");
    Serial.printf("2. Are the I2C pins correct? SDA=%d, SCL=%d\n", I2C_SDA, I2C_SCL);
    Serial.printf("3. Is the I2C address correct? 0x%02X (Default: 0x68)\n", RTC_I2C_ADDR);
  } else {
    // Check RTC time
    DateTime now;
    if (_rtc.now(now)) {
      Serial.printf("RTC time: %02d:%02d:%02d %02d/%02d/%04d\n", 
                   now.hour(), now.minute(), now.second(),
                   now.day(), now.month(), now.year());
    } else {
      Serial.println("WARNING: Could not read time from RTC");
      Serial.println("The RTC might have lost power and needs to be set");
    }
  }
}

void StateMachine::motionSample(bool motionHigh) {
  unsigned long now = millis();

  // update last motion time
  if (motionHigh) _lastMotion = now;

  // if already playing, just update lastMotion and return
  if (_isPlaying) {
    _lastPirState = motionHigh;
    return;
  }

  // throttle retriggers
  if (now - _lastTriggerAttempt < _minGapMs) {
    _lastPirState = motionHigh;
    return;
  }

  // either respond to rising edge or respond to any HIGH when idle
  bool risingEdge = (motionHigh && !_lastPirState);
  if (risingEdge || (motionHigh && _state == IDLE)) {
    _lastTriggerAttempt = now;

    // attempt to start greeting; only set _isPlaying if start succeeded
    startGreeting();
  }

  _lastPirState = motionHigh;
}

void StateMachine::startGreeting() {
  if (isDNDTime()) {
    _state = IDLE;
    _isPlaying = false;
    return;
  }
  
  if (_audio && _audio->start(String("/jay-swaminarayan.mp3"))) {
    _state = GREETING;
    _isPlaying = true;
  } else {
    _state = IDLE;
    _isPlaying = false;
  }
}

void StateMachine::startDhunSession() {
  if (isDNDTime()) {
    _state = IDLE;
    _isPlaying = false;
    return;
  }
  
  _state = DHUN;
  _dhunSessionStart = millis();
  // start first dhun
  bool ok = startRandomDhun();
  if (!ok) {
    // nothing to play: reset
    _state = IDLE;
    _isPlaying = false;
    _lastTriggerAttempt = 0;
  } else {
    _isPlaying = true;
  }
}

bool StateMachine::startRandomDhun() {
  if (isDNDTime() || !_fs) return false;
  
  int count = _fs->getCount("/dhun");
  if (count <= 0) return false;
  
  // pick random file
  int idx = random(count);
  String path = _fs->getPath("/dhun", idx);
  
  if (_audio && _audio->start(path)) {
    _isPlaying = true;
    return true;
  }
  return false;
}

void StateMachine::periodic() {
  unsigned long now = millis();
  if (now - _lastCheck < STATE_CHECK_INTERVAL_MS) return;
  _lastCheck = now;

  // Hourly chime scheduler (uses DS3231 via RtcClock)
  DateTime dt;
  if (_rtc.now(dt)) {
    int hr = dt.hour();
    int sec = dt.second();
    // window at top of hour
    bool inWindow = (sec >= 0 && sec <= CHIME_WINDOW_SEC);
    bool inRange = (hr >= CHIME_START_HOUR && hr <= CHIME_END_HOUR);
    Serial.printf("StateMachine: Checking chime conditions - hour=%d, sec=%d, inWindow=%d, inRange=%d, inChime=%d, lastChimeHour=%d\n",
                  hr, sec, inWindow ? 1 : 0, inRange ? 1 : 0, _inChime ? 1 : 0, _lastChimeHour);
    if (inRange && inWindow && !_inChime && (_lastChimeHour != hr)) {
      int h12 = hr % 12; if (h12 == 0) h12 = 12;
      _inChime = true;
      _chimeHourNumber = h12;
      _chimeBellRemaining = h12;
      _chimePhase = CH_BELLS;
      _lastChimeHour = hr; // prevent re-triggering within the hour

      // Capture current playback to resume later
      _hadPreempt = false;
      _preemptPath = String();
      _preemptState = _state;
      if (_audio && _audio->isRunning()) {
        String cur = _audio->getCurrentPath();
        if (cur.length() > 0) {
          _preemptPath = cur;
          _hadPreempt = true;
          Serial.printf("StateMachine: preempting '%s' (state=%d) for chime\n", _preemptPath.c_str(), (int)_preemptState);
        }
      }

      bool ok = (_audio ? _audio->start(String(BELL_PATH)) : false);
      Serial.printf("StateMachine: start bell.mp3 returned %d\n", ok ? 1 : 0);
      if (ok) {
        _state = GREETING;   // reuse GREETING state for chime sequence management
        _isPlaying = true;
      } else {
        Serial.println("StateMachine: Failed to start bell.mp3, aborting chime");
        _inChime = false;
        _chimePhase = CH_NONE;
        if (_hadPreempt && _audio) {
          Serial.println("StateMachine: chime start failed -> resuming preempted track");
          if (_audio->start(_preemptPath)) {
            _state = _preemptState;
            _isPlaying = true;
          } else {
            _state = IDLE;
            _isPlaying = false;
          }
        } else {
          _state = IDLE;
          _isPlaying = false;
        }
        _hadPreempt = false;
        _preemptPath = String();
      }
    }
  } else {
    Serial.println("StateMachine: RTC now read failed");
  }

  // Core transitions
  if (_state == GREETING) {
    if (!_audio->isRunning()) {
      Serial.println("StateMachine: GREETING finished (audio not running)");
      if (_isPlaying) {
        if (_inChime) {
          if (_chimePhase == CH_BELLS) {
            if (_chimeBellRemaining > 0) {
              _chimeBellRemaining--;
            }
            if (_chimeBellRemaining > 0) {
              // Save current volume and set to max for chime
              if (!_inChime) {
                _savedVolume = _audio->getVolume();
                _inChime = true;  // Mark that we're in chime mode
                Serial.printf("StateMachine: Saved volume %d for chime\n", _savedVolume);
              }
              _audio->setVolume(21); // Max volume
              Serial.printf("StateMachine: CHIME bell remaining=%d\n", _chimeBellRemaining);
              bool ok = (_audio ? _audio->start(String(BELL_PATH)) : false);
              Serial.printf("StateMachine: start bell.mp3 returned %d\n", ok ? 1 : 0);
              if (ok) {
                _isPlaying = true;
              } else {
                Serial.println("StateMachine: Failed to start bell.mp3, aborting chime");
                _inChime = false;
                _chimePhase = CH_NONE;
                _audio->setVolume(_savedVolume);  // Restore volume on error
                Serial.printf("StateMachine: Restored volume to %d after bell error\n", _savedVolume);
                _state = IDLE;
                _isPlaying = false;
              }
            } else {
              _chimePhase = CH_NUMBER;
              char numFile[48];
              snprintf(numFile, sizeof(numFile), "%s%d.mp3", HOURS_DIR, _chimeHourNumber);
              Serial.printf("StateMachine: CHIME number -> %s\n", numFile);
              bool ok = (_audio ? _audio->start(String(numFile)) : false);
              Serial.printf("StateMachine: start number file returned %d\n", ok ? 1 : 0);
              if (ok) {
                _isPlaying = true;
              } else {
                Serial.println("StateMachine: Failed to start number file, aborting chime");
                _inChime = false;
                _chimePhase = CH_NONE;
                _audio->setVolume(_savedVolume);  // Restore volume on error
                Serial.printf("StateMachine: Restored volume to %d after number error\n", _savedVolume);
                if (_hadPreempt && _audio) {
                  Serial.println("StateMachine: number play failed -> resuming preempted track");
                  if (_audio->start(_preemptPath)) {
                    _state = _preemptState;
                    _isPlaying = true;
                  } else {
                    _state = IDLE;
                    _isPlaying = false;
                  }
                } else {
                  _state = IDLE;
                  _isPlaying = false;
                }
                _hadPreempt = false;
                _preemptPath = String();
              }
            }
          } else if (_chimePhase == CH_NUMBER) {
            static int playCount = 0;
            playCount++;
            
            if (playCount <= 2) {  // Play the hour number twice
              char numFile[48];
              snprintf(numFile, sizeof(numFile), "%s%d.mp3", HOURS_DIR, _chimeHourNumber);
              Serial.printf("StateMachine: CHIME playing number %d/2 -> %s\n", playCount, numFile);
              if (_audio->start(String(numFile))) {
                _isPlaying = true;
                return; // Exit and wait for this to finish
              }
            }
            
            // If we get here, both plays are done or failed
            if (playCount >= 2) {
              Serial.println("StateMachine: CHIME number finished twice, ending chime");
              playCount = 0;  // Reset for next time
              _inChime = false;
              _chimePhase = CH_NONE;
              // Restore original volume
              _audio->setVolume(_savedVolume);
              Serial.printf("StateMachine: Restored volume to %d after chime\n", _savedVolume);
            }
            if (_hadPreempt && _audio) {
              Serial.println("StateMachine: chime complete -> resuming preempted track");
              if (_audio->start(_preemptPath)) {
                _state = _preemptState;
                _isPlaying = true;
              } else {
                _state = IDLE;
                _isPlaying = false;
              }
            } else {
              _state = IDLE;
              _isPlaying = false;
            }
            _hadPreempt = false;
            _preemptPath = String();
          } else {
            Serial.println("StateMachine: CHIME unknown phase, aborting");
            _inChime = false;
            _chimePhase = CH_NONE;
            _audio->setVolume(_savedVolume);  // Restore volume on unknown phase
            if (_hadPreempt && _audio) {
              Serial.println("StateMachine: chime unknown phase -> resuming preempted track");
              if (_audio->start(_preemptPath)) {
                _state = _preemptState;
                _isPlaying = true;
              } else {
                _state = IDLE;
                _isPlaying = false;
              }
            } else {
              _state = IDLE;
              _isPlaying = false;
            }
            _hadPreempt = false;
            _preemptPath = String();
          }
        } else {
          _isPlaying = false;
          startDhunSession();
        }
      } else {
        _state = IDLE;
        _lastTriggerAttempt = 0;
      }
    }
  }
  else if (_state == DHUN) {
    // stop after no-motion timeout
    if (now - _lastMotion > DHUN_SESSION_TIMEOUT_MS) {
      Serial.println("StateMachine: DHUN session timeout (no motion) -> stopping");
      if (_audio) _audio->stop();
      _state = IDLE;
      _isPlaying = false;
      _lastTriggerAttempt = 0;
      return;
    }

    // if current dhun finished, start another
    if (!_audio->isRunning()) {
      Serial.println("StateMachine: DHUN track finished");
      // try to start next dhun
      bool ok = startRandomDhun();
      if (!ok) {
        Serial.println("StateMachine: failed to start dhun (increment fail)");
        // If audio manager has many consecutive fails, reboot to recover
        if (_audio && _audio->getConsecutiveFails() >= 5) {
          Serial.println("StateMachine: consecutive start failures >=5 -> rebooting");
          delay(200);
          esp_restart();
        }
      }
    }
  }
  else if (_state == IDLE) {
    // if audio unexpectedly stopped but flags still set, clear them
    if (!_audio->isRunning() && _isPlaying) {
      Serial.println("StateMachine: idle cleanup - audio not running but _isPlaying true -> clearing flags");
      _isPlaying = false;
      _lastTriggerAttempt = 0;
    }
    // no other action: motionSample() will start playback when motion appears
  }

  // Debug print for visibility:
  // Serial.printf("DBG state=%d isPlaying=%d lastMotion=%lu lastTrigger=%lu audioRunning=%d\n",
  //               _state, _isPlaying, _lastMotion, _lastTriggerAttempt, (_audio ? _audio->isRunning() : 0));
}
