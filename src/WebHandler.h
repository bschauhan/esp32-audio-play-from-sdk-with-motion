#ifndef WEB_HANDLER_H
#define WEB_HANDLER_H

#include <Arduino.h>
#include "WebServer.h"
#include "AudioManager.h"
#include "FileScanner.h"

class WebHandler {
public:
  WebHandler();
  ~WebHandler();
  // begin accepts both audio manager and file scanner
  void begin(AudioManager *am, FileScanner *fs);
  void handleClient();

private:
  WebServer * _server = nullptr;
  AudioManager * _audio = nullptr;
  FileScanner * _fs = nullptr;

  // handlers
  void handleRoot();
  void handleFiles();   // returns file lists as JSON
  void handlePlay();    // play a provided path
  void handleVolume();
  void handlePower();
  void handleStatus();
  void handleUploadPost();   // final handler after upload
  void handleUploadStream(); // stream upload handler (called repeatedly by WebServer)
  void handleDelete();       // delete a file passed as ?path=
};

#endif // WEB_HANDLER_H
