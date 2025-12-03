#include "FileScanner.h"
#include "SD.h"

// Ensure valid normalized absolute path
String FileScanner::normalize(const String &dir, const String &name) {
  String d = dir;
  if (!d.startsWith("/")) d = "/" + d;
  if (d.endsWith("/")) d.remove(d.length() - 1, 1);

  String n = name;
  if (n.startsWith("/")) n = n.substring(1);

  // If name starts with "dhun/" then remove prefix, so result becomes "/dhun/file.mp3"
  int slashPos = n.indexOf('/');
  if (slashPos > 0) {
    String first = n.substring(0, slashPos);
    if (first.equalsIgnoreCase("dhun")) {
      n = n.substring(slashPos + 1);
    }
  }

  return d + "/" + n;
}

void FileScanner::scanFolder(const char *dirname) {
  if (!_fs) return;

  String d = String(dirname);
  Serial.print("Scanning folder: ");
  Serial.println(d);

  File root = _fs->open(d);
  if (!root || !root.isDirectory()) {
    Serial.println("  ❌ Cannot open directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String raw  = String(file.name());
      String full = normalize(d, raw);

      Serial.print("  file: ");
      Serial.println(full);

      if (full.endsWith(".mp3") || full.endsWith(".MP3")) {
        if (_dhunCount < MAX_DHUN) {
          _dhunFiles[_dhunCount++] = full;
          Serial.print("    🎵 Added: ");
          Serial.println(full);
        } else {
          Serial.println("    ⚠ dhun list full, skipping");
        }
      }
    }

    file.close();
    file = root.openNextFile();
  }

  Serial.print("Scan finished. dhunCount = ");
  Serial.println(_dhunCount);

  root.close();
}

void FileScanner::rescan() {
  _dhunCount = 0;

  Serial.println("Rescanning SD...");

  // Ensure folder exists
  if (_fs && !_fs->exists("/dhun")) {
    Serial.println("  /dhun missing → creating...");
    _fs->mkdir("/dhun");
  }

  scanFolder("/dhun");

  Serial.print("Rescan complete. Total dhun files = ");
  Serial.println(_dhunCount);
}

int FileScanner::getCount(const char *dirname) {
  if (String(dirname) == "/dhun") {
    return _dhunCount;
  }
  return 0;
}

const String &FileScanner::getPath(const char *dirname, int index) {
  static String empty = String();

  if (String(dirname) == "/dhun") {
    if (index >= 0 && index < _dhunCount)
      return _dhunFiles[index];
    return empty;
  }

  return empty;
}
