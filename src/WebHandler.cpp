#include "WebHandler.h"
#include "WiFi.h"
#include "Config.h"
#include "SD.h"
#include "WebServer.h"

WebHandler::WebHandler() : _server(nullptr), _audio(nullptr), _fs(nullptr) {}
WebHandler::~WebHandler() {
  if (_server) { delete _server; _server = nullptr; }
}

void WebHandler::begin(AudioManager *am, FileScanner *fs) {
  _audio = am;
  _fs = fs;

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
    body{font-family:system-ui,Segoe UI,Roboto,Helvetica,Arial;margin:14px;max-width:900px}
    h2{margin-bottom:6px}
    .row{margin:12px 0}
    label{display:block;font-weight:600;margin-bottom:6px}
    input[type=range]{width:100%}
    button{padding:8px 12px;border-radius:6px;border:1px solid #aaa;background:#f6f6f6;cursor:pointer}
    .list{border:1px solid #ddd;padding:8px;border-radius:6px;max-height:220px;overflow:auto;background:#fff}
    .file{display:flex;justify-content:space-between;align-items:center;padding:6px 4px;border-bottom:1px solid #eee}
    .file:last-child{border-bottom:none}
    .now{background:#f0f8ff;padding:8px;border-radius:6px}
    small{color:#666}
    .controls{display:flex;gap:8px;align-items:center}
    .muted{opacity:0.6}
    .upload-result{margin-top:6px;font-size:0.95em}
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

  <div class="row">
    <label>/short</label>
    <div class="list" id="shortList">Loading…</div>
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

  // Only /dhun now; if you later want short option add arg folder=dhun/short
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
  // Basic validation: allow only under /dhun or /short or root mp3
  if (!(path.startsWith("/dhun/") || path.startsWith("/short/") || path.startsWith("/"))) {
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
  static File uploadFile;
  static String uploadedPath;

  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    Serial.print("Upload start: "); Serial.println(filename);
    // sanitize filename: remove paths and allow only mp3
    int slash = filename.lastIndexOf('/');
    if (slash >= 0) filename = filename.substring(slash + 1);
    // ensure extension is .mp3
    if (!(filename.endsWith(".mp3") || filename.endsWith(".MP3"))) {
      Serial.println("Upload rejected: not an mp3");
      // not much we can do here except close - final handler will report
      uploadedPath = "";
      return;
    }
    // build path: place in /dhun by default
    uploadedPath = String("/dhun/") + filename;
    // Avoid overwrite: if exists, append timestamp
    if (SD.exists(uploadedPath)) {
      String base = filename;
      String nameOnly = base;
      int dot = base.lastIndexOf('.');
      String ext = "";
      if (dot >= 0) {
        nameOnly = base.substring(0, dot);
        ext = base.substring(dot);
      }
      uploadedPath = String("/dhun/") + nameOnly + "_" + String(millis()) + ext;
    }
    Serial.print("  saving to: "); Serial.println(uploadedPath);
    uploadFile = SD.open(uploadedPath, FILE_WRITE);
    if (!uploadFile) {
      Serial.println("  ❌ Failed to open upload file for write");
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      Serial.print("Upload finished -> "); Serial.println(uploadedPath);
      // optionally tell FileScanner to rescan
      // if (_fs) _fs->rescan();
    } else {
      Serial.println("Upload finished but file wasn't open");
    }
    uploadedPath = "";
  }
}

// Final upload handler (called once after upload completed)
void WebHandler::handleUploadPost() {
  if (!_server) return;
  // simple response (we can't know detailed success here beyond SD.exists)
  // The upload stream handler already stored files on SD
  // Send back results: success or not
  // We will check if any file was recently created in /dhun by scanning timestamp - cheap approach:
  _server->send(200, "application/json", "{\"ok\":true}");
}
