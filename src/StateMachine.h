#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <Arduino.h>
#include <Preferences.h>
#include "AudioManager.h"
#include "FileScanner.h"
#include "RtcClock.h"
#include "Settings.h"

class StateMachine {
public:
  enum State {
    IDLE,
    GREETING,
    DHUN
  };

  StateMachine() = default; 
  void begin(AudioManager *audio, FileScanner *fs);
  void motionSample(bool motionHigh);
  void periodic();

  // Public methods
  RtcClock& getRtc() { return _rtc; }  // Moved to public section

  // DND and chime settings getters/setters
  int getDNDStartHour() const { return _dndStartHour; }
  int getDNDEndHour() const { return _dndEndHour; }
  int getChimeWindowSec() const { return _chimeWindowSec; }
  bool isDNDEnabled() const { return _dndEnabled; }
  
  void setDNDHours(int startHour, int endHour) {
    _dndStartHour = startHour;
    _dndEndHour = endHour;
    saveSettings();
  }
  
  void setChimeWindowSec(int seconds) {
    if (seconds > 0 && seconds <= 60) {
      _chimeWindowSec = seconds;
      saveSettings();
    }
  }
  
  void setDNDEnabled(bool enabled) {
    _dndEnabled = enabled;
    saveSettings();
  }

private:
  // Member variables
  AudioManager *_audio = nullptr;
  FileScanner *_fs = nullptr;
  State _state = IDLE;
  bool _isPlaying = false;
  unsigned long _lastMotion = 0;
  unsigned long _lastCheck = 0;
  unsigned long _dhunSessionStart = 0;  // Track when Dhun session started
  unsigned long _lastTriggerAttempt = 0;
  unsigned long _minGapMs = 500; // min gap between triggers
  bool _lastPirState = false;

  // RTC + chime
  RtcClock _rtc;  // Single declaration of _rtc
  int _lastChimeHour = -1;
  bool _inChime = false;
  int _chimeBellRemaining = 0;
  int _chimeHourNumber = 0;
  int _dndStartHour = 22;  // Default DND start: 10 PM
  int _dndEndHour = 6;     // Default DND end: 6 AM
  bool _dndEnabled = true; // DND enabled by default
  int _chimeWindowSec = 5; // Default chime window in seconds
  enum ChimePhase { CH_NONE, CH_BELLS, CH_NUMBER } _chimePhase = CH_NONE;

  // Preemption tracking for resume after chime
  String _preemptPath;
  State _preemptState = IDLE;
  bool _hadPreempt = false;
  int _savedVolume = 0;

  // Audio control
  bool isDNDTime();
  void startGreeting();
  void startDhunSession();
  bool startRandomDhun();
  bool startChime();
  
  // Save current settings to persistent storage
  void saveSettings() {
    Settings::DNDSettings settings;
    settings.enabled = _dndEnabled;
    settings.startHour = _dndStartHour;
    settings.endHour = _dndEndHour;
    settings.windowSec = _chimeWindowSec;
    Settings::saveDNDSettings(settings);
    
    Serial.println("Saved DND settings:");
    Serial.printf("  DND Enabled: %s\n", _dndEnabled ? "Yes" : "No");
    Serial.printf("  DND Hours: %02d:00 - %02d:00\n", _dndStartHour, _dndEndHour);
    Serial.printf("  Chime Window: %d seconds\n", _chimeWindowSec);
  }
};


#endif // STATE_MACHINE_H