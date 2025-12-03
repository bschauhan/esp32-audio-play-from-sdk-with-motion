#ifndef FILE_SCANNER_H
#define FILE_SCANNER_H

#include <Arduino.h>
#include "FS.h"

class FileScanner {
public:
  static const int MAX_DHUN = 200;   // change if needed

  void begin(fs::FS &fs) { _fs = &fs; }

  void scanFolder(const char *dirname);
  void rescan();

  int getCount(const char *dirname);
  const String &getPath(const char *dirname, int index);

private:
  fs::FS *_fs = nullptr;

  String _dhunFiles[MAX_DHUN];
  int    _dhunCount = 0;

  String normalize(const String &dir, const String &name);
};

#endif // FILE_SCANNER_H
