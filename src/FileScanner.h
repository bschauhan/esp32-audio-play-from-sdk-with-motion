#ifndef FILE_SCANNER_H
#define FILE_SCANNER_H


#include <Arduino.h>
#include "FS.h"


class FileScanner {
public:
void begin(fs::FS &fs) { _fs = &fs; }
void scanFolder(const char *dirname);
int getCount(const char *dirname);
void rescan();
const String &getPath(const char *dirname, int index);
private:
fs::FS *_fs = nullptr;
String _shortFiles[20];
int _shortCount = 0;
String _dhunFiles[20];
int _dhunCount = 0;
String normalize(const String &dir, const String &name);
};


#endif // FILE_SCANNER_H