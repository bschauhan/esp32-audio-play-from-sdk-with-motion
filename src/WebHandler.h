#ifndef WEB_HANDLER_H
#define WEB_HANDLER_H

#include <Arduino.h>
#include <FS.h>          // for File
#include "WebServer.h"
#include "AudioManager.h"
#include "FileScanner.h"
#include "StateMachine.h"

class WebHandler {
public:
  WebHandler();
  ~WebHandler();

  // Initialize with pointers to other subsystems
  void begin(AudioManager *audio, FileScanner *fs, StateMachine *sm);

  // Call this regularly from loop()
  void handleClient();

private:
  // Core components
  WebServer    *_server   = nullptr;
  AudioManager *_audio    = nullptr;
  FileScanner  *_fs       = nullptr;
  StateMachine *_sm       = nullptr;

  // Upload state (used by handleUploadStream/handleUploadPost)
  File   _uploadFile;   // current upload file on SD
  String _uploadPath;   // final path like "/dhun/file.mp3"
  
  // Power state
  bool _powerState = true;

  // HTTP handlers
  void handleRoot();        // serve main dashboard HTML
  void handleFiles();       // GET /api/files       → JSON list of /dhun files
  void handlePlay();        // GET /api/play        → ?path=
  void handleVolume();      // GET /api/volume      → ?level=
  void handlePower();       // GET /api/power       → ?on=1/0
  void handleStatus();      // GET /api/status      → current status JSON
  void handleEQ();          // GET/POST /api/eq      → EQ settings
  void handleCrossfade();   // GET/POST /api/crossfade → crossfade settings
  void handleUploadPost();  // POST /upload (final response after streaming)
  void handleUploadStream();// POST /upload (streaming chunks from client)
  void handleDelete();      // GET /api/delete      → ?path= (delete file)
  void handleChimeSettings(); // GET/POST /api/chime-settings
};

#endif // WEB_HANDLER_H
