#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>
#include "Audio.h"

class AudioManager {
public:
  void begin(int bclk, int lrclk, int din);
  void loop();
  bool isRunning();
  void stop();
  void setVolume(int v);
  int  getVolume();
  bool start(const String &path); // improved start with retries
  int  getConsecutiveFails();
  void resetConsecutiveFails();

private:
  Audio _audio;
  int _bclk=0, _lrclk=0, _din=0;
  int _currentVolume = 21;
  int _consecutiveFails = 0;
};

#endif // AUDIO_MANAGER_H
