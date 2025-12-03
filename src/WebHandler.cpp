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

  // create the WebServer instance dynamically
  if (!_server) _server = new WebServer(80);

  WiFi.mode(WIFI_AP);
  IPAddress local_IP(192,168,10,1);
  IPAddress gateway(192,168,10,1);
  IPAddress subnet(255,255,255,0);
  WiFi.softAPConfig(local_IP,gateway,subnet);
  WiFi.softAP("bharat","bharat@123");

  // register handlers
  _server->on("/", [this]() { this->handleRoot(); });

  _server->on("/api/files", [this]() { this->handleFiles(); });
  _server->on("/api/play",  [this]() { this->handlePlay(); });
  _server->on("/api/volume",[this]() { this->handleVolume(); });
  _server->on("/api/power", [this]() { this->handlePower(); });
  _server->on("/api/status",[this]() { this->handleStatus(); });
  _server->on("/api/chime-settings", HTTP_GET, [this]() { this->handleChimeSettings(); });
  _server->on("/api/chime-settings", HTTP_POST, [this]() { this->handleChimeSettings(); });

  // delete endpoint (GET for simplicity)
  _server->on("/api/delete", [this]() { this->handleDelete(); });

  // Upload: first arg = path, second lambda = final handler, third = upload stream handler
  _server->on("/upload", HTTP_POST,
               [this]() { this->handleUploadPost(); },    // called after upload done
               [this]() { this->handleUploadStream(); }   // called during upload chunks
  );

  _server->begin();
}

void WebHandler::handleClient() {
  if (_server) _server->handleClient();
}

// Serve the same dashboard root (unchanged except includes upload UI in the HTML)
// (Assume you already have the big HTML — see next section for upload UI additions)
// Serve a full dashboard UI (HTML + JS)
void WebHandler::handleRoot() {
  if (!_server) return;
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32 Audio Dashboard</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f5f5f5; }
    .container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    h1 { color: #333; margin-top: 0; }
    .row { margin-bottom: 20px; padding: 15px; background: #f9f9f9; border-radius: 6px; }
    .row label { display: block; font-weight: bold; margin-bottom: 8px; }
    .list { max-height: 200px; overflow-y: auto; border: 1px solid #ddd; padding: 10px; background: white; border-radius: 4px; }
    button { padding: 8px 16px; background: #4CAF50; color: white; border: none; border-radius: 4px; cursor: pointer; }
    button:hover { background: #45a049; }
    .file { display: flex; justify-content: space-between; padding: 8px; border-bottom: 1px solid #eee; }
    .file:last-child { border-bottom: none; }
    .file button { margin-left: 10px; padding: 4px 8px; font-size: 0.9em; }
    .now { background: #e9f7ef !important; border-left: 4px solid #4CAF50; }
    .upload-result { margin-top: 8px; padding: 8px; border-radius: 4px; }
    .success { background: #e9f7ef; color: #2e7d32; }
    .error { background: #ffebee; color: #c62828; }
    .settings-panel { background: #f0f7fb; padding: 15px; border-radius: 6px; margin: 20px 0; }
    .settings-row { display: flex; align-items: center; margin-bottom: 10px; }
    .settings-row label { min-width: 150px; margin: 0 10px 0 0; }
    input[type="number"] { padding: 5px; border: 1px solid #ddd; border-radius: 4px; }
    .save-btn { background: #2196F3; }
    .save-btn:hover { background: #0b7dda; }
    
    /* Toggle Switch */
    .switch { position: relative; display: inline-block; width: 60px; height: 34px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 34px; }
    .slider:before { position: absolute; content: ""; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%; }
    input:checked + .slider { background-color: #2196F3; }
    input:focus + .slider { box-shadow: 0 0 1px #2196F3; }
    input:checked + .slider:before { transform: translateX(26px); }
    .slider.round { border-radius: 34px; }
    .slider.round:before { border-radius: 50%; }
  </style>
</head>
<body>
  <h2>ESP32 Audio Dashboard</h2>

  <div class="row">
    <div class="controls">
      <label style="margin:0 8px 0 0">Power</label>
      <input id="power" type="checkbox">
      <label style="margin-left:18px">Volume <span id="volLabel"></span></label>
      <input id="vol" type="range" min="0" max="21" step="1" style="width:260px">
    </div>
  </div>

  <div class="row now">
    <strong>Now Playing:</strong>
    <div id="nowPlaying">—</div>
    <small id="statusText"></small>
  </div>

  <div class="row">
    <label>/dhun (random)</label>
    <div class="list" id="dhunList">Loading…</div>
    <div style="margin-top:8px">
      <button id="refreshFiles">Refresh files</button>
    </div>
  </div>

  <div class="settings-panel">
    <h3>DND Settings</h3>
    <div class="settings-row">
      <label>Enable DND:</label>
      <label class="switch">
        <input type="checkbox" id="dndEnabled" checked>
        <span class="slider round"></span>
      </label>
    </div>
    <div id="dndSettings" style="margin-top: 15px;">
      <div class="settings-row">
        <label>Active Hours (0-23):</label>
        <div>
          <input type="number" id="activeStart" min="0" max="23" value="6" style="width: 60px;">
          <span>to</span>
          <input type="number" id="activeEnd" min="0" max="23" value="23" style="width: 60px;">
          <span>hours</span>
        </div>
      </div>
      <div class="settings-row">
        <label>Chime Window (seconds):</label>
        <input type="number" id="chimeWindow" min="1" max="60" value="5" style="width: 60px;">
      </div>
      <div style="margin-top: 15px; font-size: 0.9em; color: #666;">
        <p><strong>Note:</strong> System will be in DND mode outside active hours when DND is enabled.</p>
        <p>Chime will only sound during active hours when DND is enabled.</p>
      </div>
    </div>
    <button id="saveChimeSettings" class="save-btn" style="margin-top: 10px;">Save Settings</button>
    <span id="saveStatus" style="margin-left: 12px;"></span>
  </div>

  <div class="row">
    <label>Upload MP3 to /dhun</label>
    <input type="file" id="fileInput" accept=".mp3">
    <button id="uploadBtn">Upload</button>
    <div id="uploadResult" class="upload-result"></div>
  </div>

<script>
async function apiGet(path){ const r = await fetch(path); if (!r.ok) return null; return r.json(); }
async function apiAction(path){ await fetch(path); }

async function refreshStatus(){
  const s = await apiGet('/api/status');
  if (!s) return;
  document.getElementById('vol').value = s.volume;
  document.getElementById('volLabel').textContent = s.volume;
  document.getElementById('power').checked = s.power;
  document.getElementById('nowPlaying').textContent = s.nowPlaying || '—';
  document.getElementById('statusText').textContent = s.isPlaying ? 'Playing' : 'Stopped';
}

let dhunStart = 0;
const dhunPageSize = 40; // adjust as needed

async function fetchDhunPage(start) {
  const r = await fetch('/api/files?start=' + start + '&count=' + dhunPageSize);
  if (!r.ok) return null;
  return await r.json();
}

async function refreshFiles(reset=true) {
  if (reset) {
    dhunStart = 0;
    document.getElementById('dhunList').innerHTML = '';
  }
  const page = await fetchDhunPage(dhunStart);
  if (!page) {
    document.getElementById('dhunList').textContent = 'Error loading files';
    return;
  }
  (page.dhun || []).forEach(p=>{
    const div = document.createElement('div'); div.className='file';
    const name = document.createElement('div'); name.textContent = p.replace(/^\/dhun\//,'');
    const btns = document.createElement('div');
    const play = document.createElement('button'); play.textContent='Play'; play.onclick=()=>playPath(p);
    const del = document.createElement('button'); del.textContent='Delete'; del.style.marginLeft='6px';
    del.onclick = async () => {
      if (!confirm('Delete ' + name.textContent + '?')) return;
      const r = await fetch('/api/delete?path='+encodeURIComponent(p));
      if (r.ok) { refreshFiles(true); refreshStatus(); } else { alert('Delete failed'); }
    };
    btns.appendChild(play); btns.appendChild(del);
    div.appendChild(name); div.appendChild(btns);
    document.getElementById('dhunList').appendChild(div);
  });

  // show load-more button or hide if done
  const total = page.total || 0;
  const returned = page.count || 0;
  dhunStart += returned;
  let moreBtn = document.getElementById('loadMoreBtn');
  if (!moreBtn) {
    moreBtn = document.createElement('button');
    moreBtn.id = 'loadMoreBtn';
    moreBtn.textContent = 'Load more';
    moreBtn.onclick = ()=> refreshFiles(false);
    document.getElementById('dhunList').parentNode.appendChild(moreBtn);
  }
  if (dhunStart >= total) {
    moreBtn.style.display = 'none';
  } else {
    moreBtn.style.display = 'inline-block';
  }
}

async function playPath(path){
  await apiAction('/api/play?path='+encodeURIComponent(path));
  setTimeout(refreshStatus,200);
}

// ---- Volume debounce + update ----
function debounce(fn, wait){
  let t = null;
  return function(...args){
    if (t) clearTimeout(t);
    t = setTimeout(()=>{ fn.apply(this, args); t = null; }, wait);
  }
}

// send volume to server (called by debounced handler)
async function sendVolume(level){
  try {
    const res = await fetch('/api/volume?level=' + encodeURIComponent(level));
    if (res.ok) {
      const j = await res.json();
      if (j && typeof j.volume !== 'undefined') {
        document.getElementById('vol').value = j.volume;
        document.getElementById('volLabel').textContent = j.volume;
      }
    } else {
      console.warn('Volume API error', res.status);
    }
  } catch(e) {
    console.warn('Volume send failed', e);
  }
}

// Toggle DND settings visibility
function toggleDNDSettings(enable) {
  const dndSettings = document.getElementById('dndSettings');
  if (enable) {
    dndSettings.style.display = 'block';
  } else {
    dndSettings.style.display = 'none';
  }
}

// Load chime and DND settings
function loadSettings() {
  // Load volume
  fetch('/api/volume')
    .then(r => r.json())
    .then(data => {
      document.getElementById('vol').value = data.volume || 0;
      document.getElementById('volLabel').textContent = data.volume || 0;
    });

  // Load chime and DND settings
  fetch('/api/chime-settings')
    .then(r => r.json())
    .then(settings => {
      // Set DND enabled state
      const dndEnabled = settings.enabled !== false; // Default to true if not set
      document.getElementById('dndEnabled').checked = dndEnabled;
      toggleDNDSettings(dndEnabled);
      
      // Set other settings if they exist
      if (settings.startHour !== undefined) document.getElementById('activeStart').value = settings.startHour;
      if (settings.endHour !== undefined) document.getElementById('activeEnd').value = settings.endHour;
      if (settings.windowSec !== undefined) document.getElementById('chimeWindow').value = settings.windowSec;
    });
}

// Toggle DND settings when checkbox changes
document.getElementById('dndEnabled').addEventListener('change', function() {
  toggleDNDSettings(this.checked);
});

// call sendVolume after 150ms of idle while sliding
const debouncedSend = debounce((v)=> sendVolume(v), 150);

// update label immediately on input, send after debounce
document.getElementById('vol').addEventListener('input', (e)=>{
  const v = e.target.value;
  document.getElementById('volLabel').textContent = v;
  debouncedSend(v);
});

// keep the old change handler too (safety)
document.getElementById('vol').addEventListener('change', async (e)=>{
  await fetch('/api/volume?level=' + encodeURIComponent(e.target.value));
  refreshStatus();
});

document.getElementById('power').addEventListener('change', async (e)=>{
  await apiAction('/api/power?on='+(e.target.checked?1:0));
  refreshStatus();
});
document.getElementById('refreshFiles').addEventListener('click', refreshFiles);

// Save chime and DND settings
document.getElementById('saveChimeSettings').addEventListener('click', () => {
  const saveBtn = document.getElementById('saveChimeSettings');
  const statusEl = document.getElementById('saveStatus');
  const dndEnabled = document.getElementById('dndEnabled').checked;
  
  const settings = {
    enabled: dndEnabled,
    startHour: parseInt(document.getElementById('activeStart').value),
    endHour: parseInt(document.getElementById('activeEnd').value),
    windowSec: parseInt(document.getElementById('chimeWindow').value)
  };

  // Validate inputs when DND is enabled
  if (dndEnabled) {
    if (isNaN(settings.startHour) || settings.startHour < 0 || settings.startHour > 23 ||
        isNaN(settings.endHour) || settings.endHour < 0 || settings.endHour > 23 ||
        isNaN(settings.windowSec) || settings.windowSec <= 0 || settings.windowSec > 60) {
      statusEl.textContent = 'Invalid values. Please check your inputs.';
      statusEl.style.color = 'red';
      return;
    }
  }

  // Disable button and show loading state
  saveBtn.disabled = true;
  statusEl.textContent = 'Saving...';
  statusEl.style.color = '';

  fetch('/api/chime-settings', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(settings)
  })
  .then(r => {
    if (!r.ok) {
      throw new Error('Network response was not ok');
    }
    return r.json();
  })
  .then(data => {
    if (data.ok) {
      statusEl.textContent = 'Settings saved successfully!';
      statusEl.style.color = 'green';
      // Clear the success message after 3 seconds
      setTimeout(() => {
        statusEl.textContent = '';
      }, 3000);
    } else {
      throw new Error(data.message || 'Error saving settings');
    }
  })
  .catch(error => {
    console.error('Error:', error);
    statusEl.textContent = error.message || 'Error saving settings';
    statusEl.style.color = 'red';
  })
  .finally(() => {
    saveBtn.disabled = false;
  });
});

// upload logic
document.getElementById('uploadBtn').addEventListener('click', async () => {
  const fi = document.getElementById('fileInput');
  const resultEl = document.getElementById('uploadResult');
  if (!fi.files || fi.files.length === 0) { alert('Select a file'); return; }
  const file = fi.files[0];
  if (!file.name.toLowerCase().endsWith('.mp3')) { alert('Only .mp3 allowed'); return; }

  const fd = new FormData();
  fd.append('file', file, file.name);

  resultEl.textContent = 'Uploading…';
  try {
    const res = await fetch('/upload', { method: 'POST', body: fd });
    if (res.ok) {
      resultEl.textContent = 'Upload OK';
      setTimeout(()=>{ refreshFiles(); refreshStatus(); }, 600);
    } else {
      resultEl.textContent = 'Upload failed';
    }
  } catch (e) {
    resultEl.textContent = 'Upload error';
  }
});

// initial load
refreshStatus();
refreshFiles();
setInterval(refreshStatus,2000);
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
  if (!on && _audio) _audio->stop();
  _server->send(200, "application/json", String("{\"ok\":true,\"power\":") + (on?"true":"false") + "}");
}

void WebHandler::handleStatus() {
  if (!_server) return;
  bool running = (_audio ? _audio->isRunning() : false);
  int vol = (_audio ? _audio->getVolume() : DEFAULT_VOLUME);
  String json = String("{\"volume\":") + vol + ",\"power\":true,\"isPlaying\":" + (running?"true":"false") + ",\"nowPlaying\":\"\"}";
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
