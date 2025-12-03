#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#include <Arduino.h>
#include "Audio.h"
#include "Settings.h"

class AudioManager {
public:
  void begin(int bclk, int lrclk, int din);
  void loop();
  bool isRunning();
  void stop();
  void setVolume(int v) { 
    if (v < 0) v = 0; 
    if (v > 21) v = 21; 
    _currentVolume = v; 
    _audio.setVolume(v);
    Settings::saveVolume(v);  // Save volume to persistent storage
  }
  void loadVolume() { // Added method to load volume from persistent storage
    _currentVolume = Settings::loadVolume();
    _audio.setVolume(_currentVolume);
  }
  int getVolume() { return _currentVolume; } // Added getter for volume
  
  // Equalizer functions
  void setBass(int level);
  void setMid(int level);
  void setTreble(int level);
  int getBass() { return _eqBass; }
  int getMid() { return _eqMid; }
  int getTreble() { return _eqTreble; }
  void loadEQSettings();
  void saveEQSettings();
  
  // Crossfade functions
  void setCrossfadeTime(int ms) { _crossfadeTime = ms; }
  int getCrossfadeTime() { return _crossfadeTime; }
  bool startWithCrossfade(const String &path);
  bool isCrossfading() { return _isCrossfading; }
  
  bool start(const String &path); // improved start with retries
  int  getConsecutiveFails();
  void resetConsecutiveFails();
  String getCurrentPath() const { return _currentPath; }

private:
  Audio _audio;
  int _bclk=0, _lrclk=0, _din=0;
  int _currentVolume = 21;
  int _consecutiveFails = 0;
  String _currentPath;
  
  // Equalizer variables (range: -12 to +12)
  int _eqBass = 0;
  int _eqMid = 0;
  int _eqTreble = 0;
  
  // Crossfade variables
  int _crossfadeTime = 2000; // 2 seconds default
  bool _isCrossfading = false;
  String _nextPath;
  unsigned long _crossfadeStart = 0;
  int _originalVolume = 0;
  
  // Private methods
  void applyEQ();
  void updateCrossfade();
};

#endif // AUDIO_MANAGER_H
