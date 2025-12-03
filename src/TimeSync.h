#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <WiFi.h>
#include <time.h>
#include "RtcClock.h"
#include "Config.h"

class TimeSync {
public:
  static void begin() {
    // Configure NTP
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    _lastSyncAttempt = 0;
    _timeSynced = false;
  }

  static bool syncIfNeeded(RtcClock &rtc) {
    // Only try to sync if WiFi is connected and we haven't synced yet
    if (WiFi.status() == WL_CONNECTED && !_timeSynced) {
      unsigned long now = millis();
      
      // Only try to sync every 5 minutes max
      if (now - _lastSyncAttempt < 300000) { // 5 minutes
        return false;
      }
      
      _lastSyncAttempt = now;
      
      // Get time from NTP
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 5000)) { // 5 second timeout
        // Convert to DateTime and set RTC
        DateTime dt(
          timeinfo.tm_year + 1900,
          timeinfo.tm_mon + 1,
          timeinfo.tm_mday,
          timeinfo.tm_hour,
          timeinfo.tm_min,
          timeinfo.tm_sec
        );
        
        rtc.adjust(dt);
        _timeSynced = true;
        return true;
      }
    }
    return false;
  }

  static bool isTimeSynced() {
    return _timeSynced;
  }

private:
  static unsigned long _lastSyncAttempt;
  static bool _timeSynced;
};

// Initialize static members
unsigned long TimeSync::_lastSyncAttempt = 0;
bool TimeSync::_timeSynced = false;

#endif // TIME_SYNC_H
