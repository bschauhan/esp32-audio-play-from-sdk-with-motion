#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H


#include <Arduino.h>
#include "AudioManager.h"
#include "FileScanner.h"


class StateMachine {
public:
void begin(AudioManager *am, FileScanner *fs);
void motionSample(bool motionHigh);
void periodic();
private:
// inside StateMachine class (update private section)

private:
  AudioManager *_audio = nullptr;
  FileScanner *_fs = nullptr;
  bool _isPlaying = false;
  unsigned long _lastMotion = 0;
  unsigned long _dhunSessionStart = 0;
  unsigned long _lastCheck = 0;
  enum State { IDLE, GREETING, DHUN } _state = IDLE;

  // retrigger control
  unsigned long _lastTriggerAttempt = 0;
  unsigned long _minGapMs = 500; // min gap between triggers
  bool _lastPirState = false;

  void startGreeting();
  void startDhunSession();
  bool startRandomDhun();  // return true if started

};


#endif // STATE_MACHINE_H