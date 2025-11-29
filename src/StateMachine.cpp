#include "StateMachine.h"
#include "Config.h"

void StateMachine::begin(AudioManager *am, FileScanner *fs) {
  _audio = am;
  _fs = fs;
  _isPlaying = false;
  _state = IDLE;
  _lastTriggerAttempt = 0;
  _lastMotion = 0;
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
    Serial.printf("StateMachine: motionSample -> attempt trigger (rising=%d, state=%d)\n", risingEdge ? 1 : 0, _state);
    _lastTriggerAttempt = now;

    // attempt to start greeting; only set _isPlaying if start succeeded
    startGreeting();
  }

  _lastPirState = motionHigh;
}

void StateMachine::startGreeting() {
  Serial.println("StateMachine: startGreeting()");
  _state = GREETING;
  bool ok = (_audio ? _audio->start(String("/jay-swaminarayan.mp3")) : false);
  if (ok) {
    _isPlaying = true;
    Serial.println("StateMachine: greeting started OK");
  } else {
    Serial.println("StateMachine: greeting failed - remain IDLE");
    _state = IDLE;
    _isPlaying = false;
    _lastTriggerAttempt = millis();
  }
}

void StateMachine::startDhunSession() {
  Serial.println("StateMachine: startDhunSession()");
  _state = DHUN;
  _dhunSessionStart = millis();
  // start first dhun
  bool ok = startRandomDhun();
  if (!ok) {
    Serial.println("StateMachine: startDhunSession -> no dhun started, ending session");
    // nothing to play: reset
    _state = IDLE;
    _isPlaying = false;
    _lastTriggerAttempt = 0;
  } else {
    _isPlaying = true;
  }
}

bool StateMachine::startRandomDhun() {
  if (!_fs) return false;
  int cnt = _fs->getCount("/dhun");
  if (cnt == 0) return false;
  int idx = random(cnt);
  String p = _fs->getPath("/dhun", idx);
  Serial.printf("StateMachine: startRandomDhun -> %s\n", p.c_str());
  bool ok = (_audio ? _audio->start(p) : false);
  if (!ok) {
    Serial.println("StateMachine: audio->start failed for dhun");
    return false;
  }
  _isPlaying = true;
  return true;
}

void StateMachine::periodic() {
  unsigned long now = millis();
  if (now - _lastCheck < STATE_CHECK_INTERVAL_MS) return;
  _lastCheck = now;

  // Core transitions
  if (_state == GREETING) {
    // if greeting finished, and not aborted, start dhun
    if (!_audio->isRunning()) {
      Serial.println("StateMachine: GREETING finished (audio not running)");
      // ensure we were actually playing before
      if (_isPlaying) {
        // start dhun session
        _isPlaying = false; // will be set when dhun actually starts
        startDhunSession();
      } else {
        // nothing was playing - ensure clean state
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
  Serial.printf("DBG state=%d isPlaying=%d lastMotion=%lu lastTrigger=%lu audioRunning=%d\n",
                _state, _isPlaying, _lastMotion, _lastTriggerAttempt, (_audio ? _audio->isRunning() : 0));
}
