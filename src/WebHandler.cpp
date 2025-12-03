#include "WebHandler.h"
#include <ArduinoJson.h>
#include "WiFi.h"
#include "Config.h"
#include "SD.h"
#include "WebServer.h"

WebHandler::WebHandler() : _server(nullptr), _audio(nullptr), _fs(nullptr), _sm(nullptr) {}
WebHandler::~WebHandler() {
  if (_server) { delete _server; _server = nullptr; }
}

void WebHandler::begin(AudioManager *audio, FileScanner *fs, StateMachine *sm) {
  _audio = audio;
  _fs = fs;
  _sm = sm;

  if (!_server) _server = new WebServer(80);

  WiFi.mode(WIFI_AP);
  IPAddress local_IP(192,168,10,1);
  IPAddress gateway(192,168,10,1);
  IPAddress subnet(255,255,255,0);
  WiFi.softAPConfig(local_IP,gateway,subnet);
  WiFi.softAP("bharat","bharat@123");

  // --- Static File Handlers for Bootstrap ---
  _server->on("/bootstrap.min.css", [this]() {
    if (SD.exists("/system/bootstrap.min.css")) {
      File f = SD.open("/system/bootstrap.min.css", "r");
      _server->streamFile(f, "text/css");
      f.close();
    } else {
      _server->send(404, "text/plain", "Bootstrap CSS not found");
    }
  });

  _server->on("/bootstrap.min.js", [this]() {
    if (SD.exists("/system/bootstrap.min.js")) {
      File f = SD.open("/system/bootstrap.min.js", "r");
      _server->streamFile(f, "application/javascript");
      f.close();
    } else {
      _server->send(404, "text/plain", "Bootstrap JS not found");
    }
  });

  // --- API Handlers ---
  _server->on("/", [this]() { this->handleRoot(); });
  _server->on("/api/files", [this]() { this->handleFiles(); });
  _server->on("/api/play",  [this]() { this->handlePlay(); });
  _server->on("/api/volume",[this]() { this->handleVolume(); });
  _server->on("/api/power", [this]() { this->handlePower(); });
  _server->on("/api/status",[this]() { this->handleStatus(); });
  _server->on("/api/chime-settings", HTTP_GET, [this]() { this->handleChimeSettings(); });
  _server->on("/api/chime-settings", HTTP_POST, [this]() { this->handleChimeSettings(); });
  _server->on("/api/delete", [this]() { this->handleDelete(); });

  _server->on("/upload", HTTP_POST,
               [this]() { this->handleUploadPost(); },
               [this]() { this->handleUploadStream(); }
  );

  _server->begin();
}

void WebHandler::handleClient() {
  if (_server) _server->handleClient();
}

void WebHandler::handleRoot() {
  if (!_server) return;
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en" data-bs-theme="light">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Bharat Audio System</title>
  <link href="/bootstrap.min.css" rel="stylesheet">
  <style>
    body { background-color: #f0f2f5; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; }
    .card { border: none; border-radius: 12px; box-shadow: 0 2px 4px rgba(0,0,0,0.05); }
    .card-header { background-color: #fff; border-bottom: 1px solid #eee; padding: 15px 20px; font-weight: 600; color: #444; border-radius: 12px 12px 0 0 !important; }
    
    /* Now Playing Card Specifics */
    .now-playing-card { background: linear-gradient(135deg, #629cf3ff 0%, #5694f0ff 100%); color: white; }
    .now-playing-card .card-header { background: rgba(255,255,255,0.1); border-bottom: 1px solid rgba(255,255,255,0.2); color: white; }
    .volume-track { height: 6px; border-radius: 3px; }
    
    /* Scrollbar for playlist */
    .playlist-container { max-height: 400px; overflow-y: auto; }
    ::-webkit-scrollbar { width: 6px; }
    ::-webkit-scrollbar-track { background: #f1f1f1; }
    ::-webkit-scrollbar-thumb { background: #cbd5e0; border-radius: 3px; }
  </style>
</head>
<body>

<nav class="navbar navbar-expand-lg navbar-dark bg-dark shadow-sm">
  <div class="container">
    <a class="navbar-brand fw-bold" href="#">
      🔊 Bharat Audio
    </a>
    <span class="navbar-text text-white-50 small" id="clockDisplay">System Ready</span>
  </div>
</nav>

<div class="container py-4">
  
  <div class="row g-4">
    
    <div class="col-lg-6">
      <div class="card now-playing-card h-100 shadow-sm">
        <div class="card-header d-flex justify-content-between align-items-center border-0">
          <span>NOW PLAYING</span>
          <span id="statusBadge" class="badge bg-white text-primary">Stopped</span>
        </div>
        <div class="card-body p-4 d-flex flex-column justify-content-between">
          <div class="mb-4">
            <div class="d-flex justify-content-between mb-2">
              <label class="form-label mb-0"><i class="bi bi-volume-up"></i> Volume</label>
              <span id="volLabel" class="fw-bold">10</span>
            </div>
            <input type="range" class="form-range" id="vol" min="0" max="21" step="1">
          </div>

          <div class="bg-white bg-opacity-10 rounded p-3 d-flex justify-content-between align-items-center">
            <span class="fw-bold"><i class="bi bi-power"></i> System Power</span>
            <div class="form-check form-switch m-0">
              <input class="form-check-input" type="checkbox" id="power" style="width: 3em; height: 1.5em; cursor: pointer;">
            </div>
          </div>
        </div>
      </div>
    </div>

    <div class="col-lg-6">
      <div class="card h-100 shadow-sm">
        <div class="card-header d-flex justify-content-between align-items-center">
          <span>🌙 DND & Automation</span>
          <div class="form-check form-switch m-0">
             <input class="form-check-input" type="checkbox" id="dndEnabled">
          </div>
        </div>
        <div class="card-body p-4">
          <div id="dndOverlay">
            
            <div class="row g-3 mb-4">
              <div class="col-6">
                <label class="form-label text-muted small fw-bold">Active Start (Hr)</label>
                <input type="number" id="activeStart" class="form-control" min="0" max="23" placeholder="0-23">
              </div>
              <div class="col-6">
                <label class="form-label text-muted small fw-bold">Active End (Hr)</label>
                <input type="number" id="activeEnd" class="form-control" min="0" max="23" placeholder="0-23">
              </div>
            </div>
            
            <div class="mb-4">
              <label class="form-label text-muted small fw-bold">Chime Duration</label>
              <div class="input-group">
                <input type="number" id="chimeWindow" class="form-control" min="1" max="60" value="5">
                <span class="input-group-text bg-light text-muted">seconds</span>
              </div>
            </div>

            <div class="alert alert-light border d-flex align-items-center small text-muted">
              ℹ️ &nbsp; Audio will only play between start and end hours.
            </div>
          </div>
          
          <div class="d-grid mt-auto">
            <button id="saveChimeSettings" class="btn btn-primary">
              Save Configuration
            </button>
          </div>
          <div id="saveToast" class="text-center mt-2 small fw-bold" style="min-height:20px"></div>
        </div>
      </div>
    </div>
  </div>

  <div class="row g-4 mt-1">
    
    <div class="col-lg-8">
      <div class="card shadow-sm h-100">
        <div class="card-header d-flex justify-content-between align-items-center bg-white">
          <span>📂 Music Library <small class="text-muted ms-2">(/dhun)</small></span>
          <button id="refreshFiles" class="btn btn-sm btn-outline-secondary">↻ Refresh</button>
        </div>
        
        <div class="list-group list-group-flush playlist-container" id="dhunList">
          <div class="text-center p-5 text-muted">Loading files...</div>
        </div>
        
        <div class="card-footer bg-white text-center border-top-0 p-2">
             <button id="loadMoreBtn" class="btn btn-sm btn-link text-decoration-none" style="display:none">Load More...</button>
        </div>
      </div>
    </div>

    <div class="col-lg-4">
      <div class="card shadow-sm h-100">
        <div class="card-header bg-success text-white">
          <span>☁️ Upload MP3</span>
        </div>
        <div class="card-body p-4">
          <div class="mb-3">
            <label class="form-label text-muted small">Select File</label>
            <input class="form-control" type="file" id="fileInput" accept=".mp3">
          </div>
          
          <div class="d-grid gap-2">
            <button id="uploadBtn" class="btn btn-success">
              Upload to SD Card
            </button>
          </div>
          
          <div id="uploadResult" class="mt-3 text-center small fw-bold"></div>
          
          <hr class="my-4 text-muted">
          <div class="text-muted small">
            <strong>Note:</strong> Files are saved to <code>/dhun/</code>. Please use short filenames without special characters.
          </div>
        </div>
      </div>
    </div>
  </div>

</div>

<script src="/bootstrap.min.js"></script>
<script>
// --- API Utilities ---
async function apiGet(path){ const r = await fetch(path); if (!r.ok) return null; return r.json(); }
async function apiAction(path){ await fetch(path); }

// --- State Management ---
async function refreshStatus(){
  const s = await apiGet('/api/status');
  if (!s) return;
  
  // Volume
  const volSlider = document.getElementById('vol');
  // Avoid jumpiness if user is dragging
  if (document.activeElement !== volSlider) {
    volSlider.value = s.volume;
  }
  document.getElementById('volLabel').textContent = s.volume;
  
  // Power
  document.getElementById('power').checked = s.power;
  
  // Now Playing
  const npTitle = document.getElementById('nowPlaying');
  if(s.nowPlaying && s.nowPlaying.length > 0) {
     npTitle.textContent = s.nowPlaying.replace('/dhun/', '');
  } else {
     npTitle.textContent = "Ready to Play";
  }

  // Badge
  const badge = document.getElementById('statusBadge');
  if(s.isPlaying) {
    badge.className = "badge bg-warning text-dark animate-pulse";
    badge.textContent = "▶ Playing";
  } else {
    badge.className = "badge bg-white text-primary";
    badge.textContent = "⏹ Stopped";
  }
}

// --- File Manager ---
let dhunStart = 0;
const dhunPageSize = 40;

async function fetchDhunPage(start) {
  const r = await fetch('/api/files?start=' + start + '&count=' + dhunPageSize);
  if (!r.ok) return null;
  return await r.json();
}

async function refreshFiles(reset=true) {
  const list = document.getElementById('dhunList');
  if (reset) {
    dhunStart = 0;
    list.innerHTML = '';
  }
  
  const page = await fetchDhunPage(dhunStart);
  if (!page) {
    list.innerHTML = '<div class="p-3 text-danger text-center">Error loading files</div>';
    return;
  }
  
  if(page.dhun.length === 0 && reset) {
     list.innerHTML = '<div class="p-5 text-muted text-center">No MP3 files found.<br>Use the Upload panel to add music.</div>';
     return;
  }

  (page.dhun || []).forEach(p => {
    const name = p.replace(/^\/dhun\//,'');
    
    const item = document.createElement('div');
    item.className = 'list-group-item list-group-item-action d-flex justify-content-between align-items-center py-3';
    
    const nameSpan = document.createElement('span');
    nameSpan.className = 'text-truncate fw-medium';
    nameSpan.style.maxWidth = '65%';
    nameSpan.textContent = name;
    
    const btnGroup = document.createElement('div');
    
    const playBtn = document.createElement('button');
    playBtn.className = 'btn btn-sm btn-primary rounded-circle me-2';
    playBtn.style.width = '32px';
    playBtn.style.height = '32px';
    playBtn.innerHTML = '▶';
    playBtn.onclick = () => playPath(p);
    
    const delBtn = document.createElement('button');
    delBtn.className = 'btn btn-sm btn-outline-danger rounded-circle';
    delBtn.style.width = '32px';
    delBtn.style.height = '32px';
    delBtn.innerHTML = '🗑';
    delBtn.onclick = async (e) => {
      e.stopPropagation(); // prevent triggering item click if we add one later
      if (!confirm('Delete ' + name + '?')) return;
      const r = await fetch('/api/delete?path='+encodeURIComponent(p));
      if (r.ok) { refreshFiles(true); refreshStatus(); } else { alert('Delete failed'); }
    };

    btnGroup.appendChild(playBtn);
    btnGroup.appendChild(delBtn);
    
    item.appendChild(nameSpan);
    item.appendChild(btnGroup);
    list.appendChild(item);
  });

  const total = page.total || 0;
  dhunStart += (page.count || 0);
  
  const moreBtn = document.getElementById('loadMoreBtn');
  moreBtn.style.display = (dhunStart >= total) ? 'none' : 'inline-block';
  moreBtn.onclick = () => refreshFiles(false);
}

async function playPath(path){
  await apiAction('/api/play?path='+encodeURIComponent(path));
  setTimeout(refreshStatus, 300);
}

// --- Volume Debounce ---
function debounce(fn, wait){
  let t = null;
  return function(...args){
    if (t) clearTimeout(t);
    t = setTimeout(()=>{ fn.apply(this, args); t = null; }, wait);
  }
}
const debouncedSend = debounce(async (v) => {
    try { await fetch('/api/volume?level=' + encodeURIComponent(v)); } 
    catch(e){}
}, 150);

document.getElementById('vol').addEventListener('input', (e)=>{
  document.getElementById('volLabel').textContent = e.target.value;
  debouncedSend(e.target.value);
});

// --- Settings Logic ---
function toggleDNDVisuals(enable) {
  const overlay = document.getElementById('dndOverlay');
  const inputs = overlay.querySelectorAll('input');
  
  if (enable) {
    overlay.style.opacity = '1';
    overlay.style.pointerEvents = 'auto';
    inputs.forEach(i => i.disabled = false);
  } else {
    overlay.style.opacity = '0.4';
    overlay.style.pointerEvents = 'none';
    inputs.forEach(i => i.disabled = true);
  }
}

document.getElementById('dndEnabled').addEventListener('change', function() {
  toggleDNDVisuals(this.checked);
});

document.getElementById('power').addEventListener('change', async (e)=>{
  await apiAction('/api/power?on='+(e.target.checked?1:0));
  refreshStatus();
});

document.getElementById('refreshFiles').addEventListener('click', () => refreshFiles(true));

// Load Initial Settings
function loadSettings() {
  fetch('/api/volume').then(r=>r.json()).then(d=>{
     if(d.volume !== undefined) {
        document.getElementById('vol').value = d.volume;
        document.getElementById('volLabel').textContent = d.volume;
     }
  });

  fetch('/api/chime-settings').then(r=>r.json()).then(s => {
      const dndOn = s.enabled !== false;
      document.getElementById('dndEnabled').checked = dndOn;
      toggleDNDVisuals(dndOn);
      if (s.startHour !== undefined) document.getElementById('activeStart').value = s.startHour;
      if (s.endHour !== undefined) document.getElementById('activeEnd').value = s.endHour;
      if (s.windowSec !== undefined) document.getElementById('chimeWindow').value = s.windowSec;
  });
}

document.getElementById('saveChimeSettings').addEventListener('click', () => {
  const btn = document.getElementById('saveChimeSettings');
  const toast = document.getElementById('saveToast');
  const dndEnabled = document.getElementById('dndEnabled').checked;
  
  const settings = {
    enabled: dndEnabled,
    startHour: parseInt(document.getElementById('activeStart').value),
    endHour: parseInt(document.getElementById('activeEnd').value),
    windowSec: parseInt(document.getElementById('chimeWindow').value)
  };

  btn.disabled = true;
  const originalText = btn.textContent;
  btn.textContent = "Saving...";

  fetch('/api/chime-settings', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(settings)
  })
  .then(r => r.json())
  .then(data => {
    if (data.ok) {
      toast.textContent = "Settings Saved Successfully!";
      toast.className = "text-center mt-2 small text-success fw-bold";
    } else {
      toast.textContent = "Error: " + (data.message || "Unknown");
      toast.className = "text-center mt-2 small text-danger fw-bold";
    }
  })
  .catch(e => {
     toast.textContent = "Network Error";
     toast.className = "text-center mt-2 small text-danger fw-bold";
  })
  .finally(() => {
    btn.disabled = false;
    btn.textContent = originalText;
    setTimeout(() => { toast.textContent = ""; }, 3000);
  });
});

// --- Upload Logic ---
document.getElementById('uploadBtn').addEventListener('click', async () => {
  const fi = document.getElementById('fileInput');
  const res = document.getElementById('uploadResult');
  
  if (!fi.files.length) { 
     res.textContent = "Please select a file first."; 
     res.className = "mt-3 text-center small fw-bold text-danger";
     return; 
  }
  
  const file = fi.files[0];
  if (!file.name.toLowerCase().endsWith('.mp3')) { 
     res.textContent = "Only .mp3 files are allowed."; 
     res.className = "mt-3 text-center small fw-bold text-danger";
     return; 
  }

  const fd = new FormData();
  fd.append('file', file, file.name);

  res.textContent = "Uploading... please wait.";
  res.className = "mt-3 text-center small fw-bold text-primary";
  document.getElementById('uploadBtn').disabled = true;

  try {
    const r = await fetch('/upload', { method: 'POST', body: fd });
    if (r.ok) {
      res.textContent = "Upload Successful!";
      res.className = "mt-3 text-center small fw-bold text-success";
      fi.value = ''; // clear input
      setTimeout(()=>{ refreshFiles(true); }, 1000);
    } else {
      res.textContent = "Upload Failed.";
      res.className = "mt-3 text-center small fw-bold text-danger";
    }
  } catch (e) {
    res.textContent = "Network Error.";
    res.className = "mt-3 text-center small fw-bold text-danger";
  }
  document.getElementById('uploadBtn').disabled = false;
});

// --- Init ---
loadSettings();
refreshStatus();
refreshFiles();
setInterval(refreshStatus, 2000);
</script>
</body>
</html>
  )rawliteral";
  _server->send(200, "text/html", html);
}

// safe JSON escape for filenames
static String jsonEscape(const String &s) {
  String out;
  out.reserve(s.length() * 1.2 + 8);
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s.charAt(i);
    switch (c) {
      case '\"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b";  break;
      case '\f': out += "\\f";  break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default:
        if ((uint8_t)c < 0x20) {
          // control char -> encode as \u00XX
          char buf[8];
          sprintf(buf, "\\u%04x", (uint8_t)c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out;
}

// GET /api/files?start=0&count=50
void WebHandler::handleFiles() {
  if (!_server) return;

  int start = 0;
  int count = 50; // default page size
  if (_server->hasArg("start")) start = _server->arg("start").toInt();
  if (_server->hasArg("count")) count = _server->arg("count").toInt();
  if (count < 1) count = 1;
  if (count > 200) count = 200; // sane upper bound

  int total = (_fs ? _fs->getCount("/dhun") : 0);

  // compute slice
  int from = start;
  if (from < 0) from = 0;
  if (from >= total) {
    _server->send(200, "application/json", String("{\"dhun\":[],\"total\":") + total + ",\"start\":" + start + ",\"count\":" + 0 + "}");
    return;
  }
  int to = from + count;
  if (to > total) to = total;

  // Build JSON for just this slice (small)
  String out;
  out.reserve(256 + (to - from) * 40); // pre-reserve some heap to reduce fragmentation
  out += "{\"dhun\":[";
  for (int i = from; i < to; ++i) {
    String p = _fs->getPath("/dhun", i);
    String esc = jsonEscape(p);
    out += "\"";
    out += esc;
    out += "\"";
    if (i < to - 1) out += ",";
  }
  out += "],\"total\":";
  out += String(total);
  out += ",\"start\":";
  out += String(from);
  out += ",\"count\":";
  out += String(to - from);
  out += "}";
  _server->send(200, "application/json", out);

  // debug
  Serial.printf("handleFiles: start=%d count=%d returned=%d total=%d freeHeap=%u\n",
                start, count, (to - from), total, (unsigned int)ESP.getFreeHeap());
}

void WebHandler::handlePlay() {
  if (!_server) return;
  if (!_server->hasArg("path")) { _server->send(400, "application/json", "{\"error\":\"missing path\"}"); return; }
  
  // Check if power is on
  if (!_powerState) {
    _server->send(403, "application/json", "{\"error\":\"system power is off\"}");
    return;
  }
  
  String path = _server->arg("path");
  if (!SD.exists(path)) {
    _server->send(404, "application/json", "{\"error\":\"file not found\"}");
    return;
  }
  if (_audio) {
    bool ok = _audio->start(path);
    if (ok) {
      _server->send(200, "application/json", "{\"ok\":true}");
      return;
    } else {
      _server->send(500, "application/json", "{\"error\":\"playback failed\"}");
      return;
    }
  }
  _server->send(500, "application/json", "{\"error\":\"no audio manager\"}");
}

void WebHandler::handleVolume() {
  if (!_server) return;
  if (!_server->hasArg("level")) { _server->send(400, "application/json", "{\"error\":\"missing level\"}"); return; }
  int v = _server->arg("level").toInt(); if (v<0) v=0; if (v>21) v=21;
  if (_audio) _audio->setVolume(v);
  // respond with the actual stored value (read back)
  int actual = (_audio ? _audio->getVolume() : v);
  _server->send(200, "application/json", String("{\"ok\":true,\"volume\":") + actual + "}");
}

void WebHandler::handlePower() {
  if (!_server) return;
  if (!_server->hasArg("on")) { _server->send(400, "application/json", "{\"error\":\"missing on\"}"); return; }
  bool on = (_server->arg("on") == "1");
  
  // Store the power state
  _powerState = on;
  
  // Stop audio if power is turned off
  if (!on && _audio) {
    _audio->stop();
  }
  
  _server->send(200, "application/json", String("{\"ok\":true,\"power\":") + (on?"true":"false") + "}");
}

void WebHandler::handleStatus() {
  if (!_server) return;
  bool running = (_audio ? _audio->isRunning() : false);
  int vol = (_audio ? _audio->getVolume() : DEFAULT_VOLUME);
  String json = String("{\"volume\":") + vol + ",\"power\":" + (_powerState?"true":"false") + ",\"isPlaying\":" + (running?"true":"false") + ",\"nowPlaying\":\"\"}";
  _server->send(200, "application/json", json);
}

// Delete a file: /api/delete?path=/dhun/foo.mp3
void WebHandler::handleDelete() {
  if (!_server) return;
  if (!_server->hasArg("path")) { _server->send(400, "application/json", "{\"error\":\"missing path\"}"); return; }
  String path = _server->arg("path");
  // Basic validation: allow only under /dhun or root mp3
  if (!(path.startsWith("/dhun/") || path.startsWith("/"))) {
    _server->send(400, "application/json", "{\"error\":\"invalid path\"}");
    return;
  }
  if (!SD.exists(path)) {
    _server->send(404, "application/json", "{\"error\":\"file not found\"}");
    return;
  }
  bool ok = SD.remove(path);
  if (ok) {
    // Optionally: trigger a rescanning of the file list in FileScanner
    // if (_fs) _fs->rescan(); // implement rescan if you want
    _server->send(200, "application/json", "{\"ok\":true}");
  } else {
    _server->send(500, "application/json", "{\"error\":\"delete failed\"}");
  }
}

/*
  Upload handling notes:
  - WebServer provides server->upload() in the upload handler
  - We'll accept a single file field named "file"
  - We'll stream directly to SD to avoid buffering large files in RAM
*/
void WebHandler::handleUploadStream() {
  if (!_server) return;

  HTTPUpload &upload = _server->upload();

  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    Serial.print("Upload start: ");
    Serial.println(filename);

    // Strip any path the browser sends (e.g. "C:\\foo\\bar.mp3")
    int slash = filename.lastIndexOf('/');
    int bslash = filename.lastIndexOf('\\'); // Windows
    int pos = max(slash, bslash);
    if (pos >= 0) {
      filename = filename.substring(pos + 1);
    }

    // Only accept .mp3
    if (!filename.endsWith(".mp3") && !filename.endsWith(".MP3")) {
      Serial.println("❌ Upload rejected: not an MP3");
      _uploadPath = "";
      return;
    }

    // base path
    _uploadPath = "/dhun/" + filename;

    // Avoid overwrite
    if (SD.exists(_uploadPath)) {
      String base = filename;
      int dot = base.lastIndexOf('.');
      String nameOnly = (dot >= 0) ? base.substring(0, dot) : base;
      String ext      = (dot >= 0) ? base.substring(dot)     : "";
      _uploadPath = "/dhun/" + nameOnly + "_" + String(millis()) + ext;
    }

    Serial.print("  saving to: ");
    Serial.println(_uploadPath);

    _uploadFile = SD.open(_uploadPath, FILE_WRITE);
    if (!_uploadFile) {
      Serial.println("  ❌ Failed to open file for write");
      _uploadPath = "";
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // called with chunks
    if (_uploadFile) {
      _uploadFile.write(upload.buf, upload.currentSize);
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    if (_uploadFile) {
      _uploadFile.close();
      Serial.print("Upload finished -> ");
      Serial.println(_uploadPath);

      // update scanner so new file appears in /api/files
      if (_fs) {
        _fs->rescan();
      }
    } else {
      Serial.println("Upload finished but file wasn't open");
      _uploadPath = "";
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Serial.println("❌ Upload aborted");
    if (_uploadFile) {
      _uploadFile.close();
    }
    if (_uploadPath.length() && SD.exists(_uploadPath)) {
      SD.remove(_uploadPath); // cleanup partial
    }
    _uploadPath = "";
  }
}

// Final upload handler (called once after upload completed)
void WebHandler::handleUploadPost() {
  if (!_server) return;

  // If we have a path and the file exists, assume success
  if (_uploadPath.length() > 0 && SD.exists(_uploadPath)) {
    _server->send(200, "application/json", "{\"ok\":true}");
  } else {
    _server->send(500, "application/json",
                  "{\"ok\":false,\"error\":\"upload failed or no file\"}");
  }

  // reset state
  _uploadPath = "";
}

void WebHandler::handleChimeSettings() {
  if (!_server || !_sm) return;
  
  if (_server->method() == HTTP_GET) {
    // Return current chime settings
    String json = "{\"enabled\":";
    json += _sm->isDNDEnabled() ? "true" : "false";
    json += ",\"startHour\":";
    json += _sm->getDNDStartHour();
    json += ",\"endHour\":";
    json += _sm->getDNDEndHour();
    json += ",\"windowSec\":";
    json += _sm->getChimeWindowSec();
    json += "}";
    _server->send(200, "application/json", json);
  } 
  else if (_server->method() == HTTP_POST) {
    // Parse JSON body for POST requests
    String body = _server->arg("plain");
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
      _server->send(400, "application/json", "{\"ok\":false,\"message\":\"Invalid JSON\"}");
      return;
    }
    
    // Check if DND is enabled
    bool enabled = true; // Default to enabled if not specified
    if (doc.containsKey("enabled")) {
      enabled = doc["enabled"];
    }
    
    // Only validate and set time settings if DND is enabled
    if (enabled) {
      if (!doc.containsKey("startHour") || !doc.containsKey("endHour") || !doc.containsKey("windowSec")) {
        _server->send(400, "application/json", "{\"ok\":false,\"message\":\"Missing required parameters\"}");
        return;
      }
      
      int startHour = doc["startHour"];
      int endHour = doc["endHour"];
      int windowSec = doc["windowSec"];
      
      // Validate input
      if (startHour < 0 || startHour > 23 || 
          endHour < 0 || endHour > 23 ||
          windowSec <= 0 || windowSec > 60) {
        _server->send(400, "application/json", "{\"ok\":false,\"message\":\"Invalid parameter values\"}");
        return;
      }
      
      _sm->setDNDHours(startHour, endHour);
      _sm->setChimeWindowSec(windowSec);
    }
    
    // Set DND enabled state
    _sm->setDNDEnabled(enabled);
    
    _server->send(200, "application/json", "{\"ok\":true}");
  }
}
