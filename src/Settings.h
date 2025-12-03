#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <Preferences.h>

class Settings {
public:

    struct DNDSettings {
        bool enabled;
        int startHour;
        int endHour;
        int windowSec;
    };

    // Must be called once in setup()
    static void begin() {
        prefs.begin("audio-settings", false);   // RW mode
    }

    static void end() {
        prefs.end();
    }

    // ---------------------------
    // VOLUME SETTINGS
    // ---------------------------
    static void saveVolume(int volume) {
        if (volume < 0) volume = 0;
        if (volume > 21) volume = 21;

        prefs.putInt("volume", volume);
        prefs.putBool("vol_init", true);
    }

    static int loadVolume(int defaultVolume = 11) {
        return prefs.getInt("volume", defaultVolume);
    }

    static bool isVolumeSet() {
        return prefs.getBool("vol_init", false);
    }

    // ---------------------------
    // DND SETTINGS
    // ---------------------------
    static void saveDNDSettings(const DNDSettings& settings) {
        prefs.putBool("dnd_enabled", settings.enabled);
        prefs.putInt("dnd_start_hour", settings.startHour);
        prefs.putInt("dnd_end_hour", settings.endHour);
        prefs.putInt("chime_window_sec", settings.windowSec);

        prefs.putBool("dnd_init", true);
    }

    static DNDSettings loadDNDSettings() {
        DNDSettings s;
        s.enabled    = prefs.getBool("dnd_enabled", true);  // default enabled
        s.startHour  = prefs.getInt("dnd_start_hour", 22);  // default 10pm
        s.endHour    = prefs.getInt("dnd_end_hour", 6);     // default 6am
        s.windowSec  = prefs.getInt("chime_window_sec", 5); // default 5 sec window
        return s;
    }

    static bool isDNDInitialized() {
        return prefs.getBool("dnd_init", false);
    }

    // ---------------------------
    // GENERAL INTEGER SETTINGS
    // ---------------------------
    static void saveInt(const char* key, int value) {
        prefs.putInt(key, value);
    }

    static int loadInt(const char* key, int defaultValue = 0) {
        return prefs.getInt(key, defaultValue);
    }

    // ---------------------------
    // GENERAL SETTINGS FLAG
    // ---------------------------
    static bool isInitialized() {
        return prefs.getBool("settings_initialized", false);
    }

    static void markInitialized() {
        prefs.putBool("settings_initialized", true);
    }

private:
    static Preferences prefs;
};

#endif // SETTINGS_H
