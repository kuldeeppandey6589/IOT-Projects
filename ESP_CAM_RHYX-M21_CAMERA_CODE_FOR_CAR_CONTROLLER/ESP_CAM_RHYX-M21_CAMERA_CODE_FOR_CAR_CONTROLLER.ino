/* ESP32-CAM Rover for RHYX M21-45 / GC2145 camera.
   Board: AI Thinker ESP32-CAM | Partition: Huge APP | Serial: 115200
   RGB565 capture + software JPEG conversion (GC2145 lacks JPEG encoder).
   Motors: L298N, pins preserved from your original code.
   WARNING: GPIO12/15 are boot-strap pins. Check startup with L298N connected.
   Power motors separately; share grounds; use a stable 5V ESP32 supply.
*/
#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include "img_converters.h"

// WiFi values retained from uploaded code; change as necessary.
static const char *WIFI_SSID = "Robotutor";
static const char *WIFI_PASS = "Robotutor";
#define FPS_TARGET 5
#define JPEG_QUALITY 65       // software JPEG encoding quality: 0..100
#define CAMERA_XCLK 10000000  // 10 MHz for stability
#define CAMERA_SIZE FRAMESIZE_QVGA // 320x240; QQVGA 160x120 if unstable
#define MOTOR_TIMEOUT 850UL  // stop if commands stop arriving

#define IN1 12
#define IN2 13
#define IN3 15
#define IN4 14
#define WHITE_LED 4
#define RED_LED 33

// AI-Thinker ESP32-CAM camera pins (same as original upload)
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

static httpd_handle_t controlsServer = NULL;  // port 80
static httpd_handle_t pictureServer = NULL;   // port 81
static volatile bool moving = false;
static volatile uint32_t lastDrive = 0;
static volatile char lastDirection = 'S';
static bool flashOn = false;

void motorStop() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  moving = false;
  lastDirection = 'S';
}
void motorForward() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}
void motorBack() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}
void motorLeft() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}
void motorRight() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}
void drive(char cmd) {
  // Browser repeats the active direction as a heartbeat. Avoid motor jerks.
  if (moving && lastDirection == cmd) {
    lastDrive = millis();
    return;
  }
  motorStop();
  switch(cmd) {
    case 'F': motorForward(); break;
    case 'B': motorBack(); break;
    case 'L': motorLeft(); break;
    case 'R': motorRight(); break;
    default: return;
  }
  lastDrive = millis();
  lastDirection = cmd;
  moving = true;
}
void setFlash(bool enabled) {
  flashOn = enabled;
  digitalWrite(WHITE_LED, enabled ? HIGH : LOW);
  digitalWrite(RED_LED, enabled ? LOW : HIGH);
}

// Dashboard: snapshots from :81, driving from :80. Responsive to touch.
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>ESP32-CAM Rover</title>
<style>
:root{--bg:#0f1115;--panel:#161a22;--text:#eaeef7;--muted:#9aa4b2;--btn:#1f2430;--border:#283040}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:var(--bg);color:var(--text);font-family:system-ui,Arial,sans-serif;user-select:none;-webkit-user-select:none;overscroll-behavior:none}
.wrap{max-width:960px;margin:auto;padding:16px}
.card{background:var(--panel);border:1px solid var(--border);border-radius:16px;overflow:hidden}
.header,.footer{display:flex;justify-content:space-between;align-items:center;gap:10px;padding:14px 16px;flex-wrap:wrap}
.header{border-bottom:1px solid var(--border)}.footer{border-top:1px solid var(--border);font-size:12px;color:var(--muted)}
.title{font-weight:700;font-size:18px}.pill{font-size:12px;color:var(--muted);border:1px solid var(--border);border-radius:30px;padding:5px 10px}
.content{display:grid;gap:16px;padding:16px}@media(min-width:800px){.content{grid-template-columns:2fr 1fr}}
.video{aspect-ratio:4/3;background:#050607;border-radius:12px;border:1px solid var(--border);overflow:hidden;display:flex;align-items:center}
#stream{width:100%;height:100%;object-fit:contain;pointer-events:none}.small{font-size:12px;color:var(--muted)}
.controls{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px}.blank{visibility:hidden}
button{padding:14px 8px;background:var(--btn);color:var(--text);border:1px solid #30394a;border-radius:12px;font-size:15px;cursor:pointer;touch-action:none}
button:active,button.active{background:#173363;border-color:#4f8cff}.stop{background:#361516;border-color:#853238}
h3{margin:0 0 10px;font-size:13px;text-transform:uppercase;color:var(--muted)}.flash{width:100%;margin-top:8px}
</style></head><body>
<div class="wrap"><div class="card">
<div class="header"><div class="title">ESP32-CAM Rover</div><div class="pill" id="ip">Connecting...</div></div>
<div class="content"><div>
<div class="video"><img id="stream" alt="Rover camera feed"></div>
<p class="small">RHYX/GC2145 camera | RGB565 + software JPEG | target %FPS% FPS</p>
<p class="small" id="camstatus">Starting camera...</p>
</div><div><h3>Drive — press and hold</h3><div class="controls">
<span class="blank"></span><button id="forward">▲ Forward</button><span class="blank"></span>
<button id="left">◀ Left</button><button class="stop" id="stop">■ Stop</button><button id="right">Right ▶</button>
<span class="blank"></span><button id="back">▼ Back</button><span class="blank"></span>
</div><h3 style="margin-top:25px">Lights</h3><button class="flash" id="flash">Toggle Flash</button>
<p class="small">Safety: rover stops if control commands are lost.</p></div></div>
<div class="footer"><span id="address">Local rover controls</span><span id="status">Ready</span></div>
</div></div>
<script>
const fps=%FPS%,stream=document.getElementById('stream'),camstatus=document.getElementById('camstatus');
const picUrl=location.protocol+'//'+location.hostname+':81/jpg';
let active=null,heartbeat=null;
function setStatus(s){document.getElementById('status').textContent=s}
function request(path){return fetch(path,{cache:'no-store'}).catch(()=>setStatus('Connection error'))}
function nextFrame(){
 const started=Date.now();
 stream.onload=()=>{camstatus.textContent='Camera connected';setTimeout(nextFrame,Math.max(0,1000/fps-(Date.now()-started)))};
 stream.onerror=()=>{camstatus.textContent='Camera retrying...';setTimeout(nextFrame,800)};
 stream.src=picUrl+'?t='+started;
}
function stopDrive(){
 if(heartbeat!==null){clearInterval(heartbeat);heartbeat=null}
 if(active){active.classList.remove('active');active=null}
 request('/stop');setStatus('Stopped');
}
function hold(id,dir){
 const btn=document.getElementById(id);
 btn.addEventListener('pointerdown',e=>{
  e.preventDefault();if(active)stopDrive();active=btn;
  btn.classList.add('active');try{btn.setPointerCapture(e.pointerId)}catch(ex){}
  request('/'+dir);setStatus(dir);
  heartbeat=setInterval(()=>request('/'+dir),250);
 });
 ['pointerup','pointercancel','lostpointercapture'].forEach(event=>btn.addEventListener(event,()=>{if(active===btn)stopDrive()}));
}
hold('forward','forward');hold('back','back');hold('left','left');hold('right','right');
document.getElementById('stop').addEventListener('click',stopDrive);
document.getElementById('flash').addEventListener('click',()=>request('/flash'));
window.addEventListener('blur',stopDrive);
document.addEventListener('visibilitychange',()=>{if(document.hidden)stopDrive()});
window.addEventListener('pagehide',stopDrive);
window.oncontextmenu=e=>e.preventDefault();
document.getElementById('ip').textContent=location.hostname;
document.getElementById('address').textContent=location.origin;
nextFrame();
</script></body></html>
)HTML";

static esp_err_t sendText(httpd_req_t *req, const char *text) {
  httpd_resp_set_type(req, "text/plain; charset=utf-8");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_sendstr(req, text);
}
static esp_err_t indexHandler(httpd_req_t *req) {
  String page = INDEX_HTML;
  page.replace("%FPS%", String(FPS_TARGET));
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, page.c_str(), page.length());
}
static esp_err_t jpgHandler(httpd_req_t *req) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[CAM] capture failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  // GC2145 supplies RGB565. Convert it to a real JPEG for web browsers.
  uint8_t *jpg = nullptr;
  size_t jpgLen = 0;
  if (fb->format == PIXFORMAT_JPEG) {
    jpg = fb->buf;
    jpgLen = fb->len;
  } else if (!frame2jpg(fb, JPEG_QUALITY, &jpg, &jpgLen)) {
    Serial.println("[CAM] software JPEG conversion failed");
    if (jpg) free(jpg);
    esp_camera_fb_return(fb);
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate");
  esp_err_t result = httpd_resp_send(req, (const char *)jpg, jpgLen);
  if (fb->format != PIXFORMAT_JPEG && jpg) free(jpg);
  esp_camera_fb_return(fb);
  return result;
}

static esp_err_t forwardHandler(httpd_req_t *r){drive('F');return sendText(r,"forward");}
static esp_err_t backHandler(httpd_req_t *r)   {drive('B');return sendText(r,"back");}
static esp_err_t leftHandler(httpd_req_t *r)   {drive('L');return sendText(r,"left");}
static esp_err_t rightHandler(httpd_req_t *r)  {drive('R');return sendText(r,"right");}
static esp_err_t stopHandler(httpd_req_t *r)   {motorStop();return sendText(r,"stop");}
static esp_err_t flashHandler(httpd_req_t *r)  {
  setFlash(!flashOn);
  return sendText(r, flashOn ? "flash on" : "flash off");
}

static void addRoute(httpd_handle_t server, const char *path, esp_err_t (*func)(httpd_req_t *)) {
  httpd_uri_t route = {};
  route.uri = path;
  route.method = HTTP_GET;
  route.handler = func;
  route.user_ctx = nullptr;
  esp_err_t err = httpd_register_uri_handler(server, &route);
  if (err != ESP_OK) Serial.printf("[HTTP] route %s failed: 0x%x\n", path, err);
}
static void startServers() {
  if (!controlsServer) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.lru_purge_enable = true;
    if (httpd_start(&controlsServer, &cfg) == ESP_OK) {
      addRoute(controlsServer, "/", indexHandler);
      addRoute(controlsServer, "/forward", forwardHandler);
      addRoute(controlsServer, "/back", backHandler);
      addRoute(controlsServer, "/left", leftHandler);
      addRoute(controlsServer, "/right", rightHandler);
      addRoute(controlsServer, "/stop", stopHandler);
      addRoute(controlsServer, "/flash", flashHandler);
      Serial.println("[HTTP] drive/control server ready on port 80");
    } else Serial.println("[HTTP] failed starting port 80");
  }
  if (!pictureServer) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 81;
    cfg.ctrl_port = 32769;  // distinct from default 32768 (port 80 server)
    cfg.stack_size = 8192;
    cfg.lru_purge_enable = true;
    if (httpd_start(&pictureServer, &cfg) == ESP_OK) {
      addRoute(pictureServer, "/jpg", jpgHandler);
      Serial.println("[HTTP] camera snapshot server ready on port 81");
    } else Serial.println("[HTTP] failed starting port 81");
  }
}

static bool startCamera() {
  camera_config_t c = {};   // fully zero-initialize configuration
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM; c.pin_d1 = Y3_GPIO_NUM;
  c.pin_d2 = Y4_GPIO_NUM; c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM; c.pin_d5 = Y7_GPIO_NUM;
  c.pin_d6 = Y8_GPIO_NUM; c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM;
  c.pin_pclk = PCLK_GPIO_NUM;
  c.pin_vsync = VSYNC_GPIO_NUM;
  c.pin_href = HREF_GPIO_NUM;
  // Use the same aliases your uploaded sketch successfully compiled with.
  c.pin_sscb_sda = SIOD_GPIO_NUM;
  c.pin_sscb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM;
  c.pin_reset = RESET_GPIO_NUM;
  c.xclk_freq_hz = CAMERA_XCLK;
  c.pixel_format = PIXFORMAT_RGB565; // CRITICAL RHYX / GC2145 FIX
  c.frame_size = psramFound() ? CAMERA_SIZE : FRAMESIZE_QQVGA;
  c.jpeg_quality = 12;  // sensor setting ignored in RGB565 mode
  c.fb_count = 1;        // use one buffer for uncompressed capture
  c.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  c.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;

  Serial.printf("[CAM] PSRAM %s\n", psramFound() ? "OK" : "NOT FOUND");
  Serial.println("[CAM] Starting RGB565 sensor at 10 MHz");
  esp_err_t err = esp_camera_init(&c);
  if (err != ESP_OK) {
    Serial.printf("[CAM] init FAILED: 0x%x\n", err);
    return false;
  }
  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor) Serial.printf("[CAM] sensor PID: 0x%04X\n", sensor->id.PID);

  camera_fb_t *frame = esp_camera_fb_get();
  if (frame) {
    Serial.printf("[CAM] test image: %ux%u, %u bytes, format=%d\n",
                  (unsigned)frame->width, (unsigned)frame->height,
                  (unsigned)frame->len, (int)frame->format);
    esp_camera_fb_return(frame);
  } else {
    Serial.println("[CAM] WARNING: first frame capture failed");
  }
  Serial.println("[CAM] init OK");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== ESP32-CAM Rover RHYX / GC2145 fixed ===");

  // Keep L298N stopped on power-up.
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  motorStop();
  pinMode(WHITE_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  setFlash(false);

  if (!startCamera()) {
    motorStop();
    Serial.println("Check camera ribbon, 5V supply, sensor and selected board.");
    while (true) delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[WiFi] connecting");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000UL) {
    Serial.print('.');
    delay(400);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] connected: %s\n", WiFi.localIP().toString().c_str());
    startServers();
    Serial.printf("[OPEN] http://%s/\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] disconnected. Will retry automatically.");
  }
}

void loop() {
  if (moving && (uint32_t)(millis() - lastDrive) > MOTOR_TIMEOUT) {
    motorStop();
    Serial.println("[SAFETY] No drive heartbeat - motors stopped");
  }
  if (WiFi.status() != WL_CONNECTED) {
    if (moving) motorStop();
    static uint32_t retryAt = 0;
    if (millis() - retryAt > 5000UL) {
      retryAt = millis();
      WiFi.reconnect();
    }
  } else if (!controlsServer || !pictureServer) {
    static uint32_t retryServerAt = 0;
    if (millis() - retryServerAt > 5000UL) {
      retryServerAt = millis();
      startServers();
    }
  }
  delay(20);
}
