// FileScanner.cpp - improved normalize + scanFolder
#include "FileScanner.h"
#include "Config.h"
#include "SD.h"

int FileScanner::getCount(const char *dirname) {
  // Compare using String to match how we store directory names elsewhere
  String d = String(dirname);
  if (d == String("/short")) {
    return _shortCount;
  } else if (d == String("/dhun")) {
    return _dhunCount;
  }
  return 0;
}

const String & FileScanner::getPath(const char *dirname, int index) {
  static String empty = String();
  String d = String(dirname);
  if (d == String("/short")) {
    if (index >= 0 && index < _shortCount) return _shortFiles[index];
    return empty;
  } else if (d == String("/dhun")) {
    if (index >= 0 && index < _dhunCount) return _dhunFiles[index];
    return empty;
  }
  return empty;
}

String FileScanner::normalize(const String &dir, const String &name) {
  // Ensure dir starts with single leading slash and no trailing slash
  String d = dir;
  if (!d.startsWith("/")) d = "/" + d;
  // remove any trailing slash
  if (d.endsWith("/")) d = d.substring(0, d.length() - 1);

  String n = name;
  // If name already starts with '/', remove leading '/'
  if (n.startsWith("/")) n = n.substring(1);

  // If name already contains the folder prefix (e.g. "dhun/song.mp3"), strip any leading folder part
  // so result becomes "/dhun/song.mp3"
  int slashPos = n.indexOf('/');
  if (slashPos > 0) {
    // if first token equals folder name, keep only file part
    String first = n.substring(0, slashPos);
    if (first.equalsIgnoreCase(d.substring(1))) {
      n = n.substring(slashPos + 1);
    }
  }

  return d + "/" + n; // always produce absolute path like "/dhun/song.mp3"
}

void FileScanner::scanFolder(const char *dirname) {
  if (!_fs) return;
  Serial.print("Scanning: "); Serial.println(dirname);
  File root = _fs->open(String(dirname));
  if (!root) {
    Serial.println("  open failed or not present");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("  not a directory");
    root.close();
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String raw = String(file.name()); // may be "song.mp3", "/short/song.mp3" or "short/song.mp3"
      String full = normalize(dirname, raw);
      Serial.print("  raw: "); Serial.println(raw);
      Serial.print("  normalized: "); Serial.println(full);
      if (full.endsWith(".mp3") || full.endsWith(".MP3")) {
        if (String(dirname) == String("/short")) {
          if (_shortCount < 20) _shortFiles[_shortCount++] = full;
        } else if (String(dirname) == String("/dhun")) {
          if (_dhunCount < 20) _dhunFiles[_dhunCount++] = full;
        }
        Serial.print("    🎵 Added: "); Serial.println(full);
      }
    } else {
      Serial.print("  skipping directory: "); Serial.println(file.name());
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
}

void FileScanner::rescan() {
  _shortCount = 0;
  _dhunCount = 0;
  scanFolder("/dhun");
}

