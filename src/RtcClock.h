#ifndef RTC_CLOCK_H
#define RTC_CLOCK_H

#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include "Config.h"  // Include Config.h to access I2C pin definitions

class RtcClock {
public:
  bool begin(TwoWire &wire = Wire) {
    _wire = &wire;
    _wire->begin(I2C_SDA, I2C_SCL);
    
    // Initialize RTC with the correct I2C address
    _ok = _rtc.begin(_wire);
    if (!_ok) {
      return false;
    }
    
    // Check if RTC lost power and set default time if needed
    if (_rtc.lostPower()) {
      // Set to compile time as default
      _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    
    return _ok;
  }

  // Returns true if time could be read
  bool now(DateTime &out) {
    if (!_ok) return false;
    // If RTC lost power and time is not set, signal failure
    if (_rtc.lostPower()) return false;
    out = _rtc.now();
    return true;
  }

  // Set the RTC to the specified date/time
  void adjust(const DateTime &dt) {
    if (_ok) {
      _rtc.adjust(dt);
    }
  }

private:
  TwoWire *_wire = nullptr;
  RTC_DS3231 _rtc;
  bool _ok = false;
};

#endif // RTC_CLOCK_H
