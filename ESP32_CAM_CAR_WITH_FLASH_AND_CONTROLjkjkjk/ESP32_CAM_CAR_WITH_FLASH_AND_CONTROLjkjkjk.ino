 //BOARD SELECTED WILL BE AI THINKER ESP32-CAM
//PARTITION SCHEME (HUGE APP)
// EVERYTHING ELSE WILL BE DEFAULT

#include "esp_camera.h"
#include <WiFi.h>

extern "C" {
  #include "esp_http_server.h"
  #include "esp_timer.h"
  #include "img_converters.h"
}

/* ===================== USER CONFIG (lightweight knobs) ===================== */
static const char* WIFI_SSID = "homeiot";
static const char* WIFI_PASS = "homeiot123";

// Default stream framerate (JS side will pull ~FPS snapshots)
#define SNAPSHOT_FPS       14   // try 12–16; raise slowly if link is solid

// Camera defaults (speed-biased but still decent quality)
#define DEFAULT_FRAMESIZE  FRAMESIZE_QVGA   // QQVGA for max fps, QVGA is a nice balance
#define DEFAULT_JPEG_Q     24               // 20–30 = faster/smaller (bigger number = lower quality)

/* ===================== L298N PINS (ESP32-CAM) ===================== */
const int IN1 = 12;   // Left FWD
const int IN2 = 13;   // Left REV
const int IN3 = 15;   // Right FWD
const int IN4 = 14;   // Right REV

/* ===================== CAMERA PINS (AI-Thinker) ===================== */
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

/* ===================== FLASH LEDS ===================== */
#define LED_WHITE_GPIO     4   // white flash (active-high)
#define LED_RED_GPIO      33   // tiny red (active-low)

/* ===================== MINIMAL LOG ===================== */
#define LOG(...) do{ Serial.printf(__VA_ARGS__); }while(0)

/* ===================== STATE ===================== */
static httpd_handle_t httpd_handle = NULL;
static bool flash_on = false;

/* ===================== MOTOR HELPERS ===================== */
void motorStop(){   digitalWrite(IN1,LOW);  digitalWrite(IN2,LOW);  digitalWrite(IN3,LOW);  digitalWrite(IN4,LOW); }
void motorForward(){digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);  digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW); }
void motorBack(){   digitalWrite(IN1,LOW);  digitalWrite(IN2,HIGH); digitalWrite(IN3,LOW);  digitalWrite(IN4,HIGH); }
void motorLeft(){   digitalWrite(IN1,LOW);  digitalWrite(IN2,HIGH); digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW); }
void motorRight(){  digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);  digitalWrite(IN3,LOW);  digitalWrite(IN4,HIGH); }

/* ===================== FLASH HELPERS ===================== */
void flash_apply(bool on){
  pinMode(LED_WHITE_GPIO, OUTPUT);
  pinMode(LED_RED_GPIO, OUTPUT);
  if(on){
    digitalWrite(LED_WHITE_GPIO, HIGH);   // white on (active-high)
    digitalWrite(LED_RED_GPIO, LOW);      // red   on (active-low)
  }else{
    digitalWrite(LED_WHITE_GPIO, LOW);
    digitalWrite(LED_RED_GPIO, HIGH);
  }
}

/* ===================== UI (snapshot stream, hold-to-drive) ===================== */
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>ESP32-CAM Rover</title>
<style>
  :root{ --bg:#0f1115; --panel:#161a22; --text:#eaeef7; --muted:#9aa4b2; --btn:#1f2430; --danger:#ef4444; --accent:#4f8cff; }
  *{box-sizing:border-box;-webkit-touch-callout:none}
  html,body{height:100%}
  body{
    margin:0;background:var(--bg);color:var(--text);
    font-family:system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,"Helvetica Neue",Arial;
    -webkit-text-size-adjust:100%;
    user-select:none; -webkit-user-select:none; -ms-user-select:none; -moz-user-select:none;
    overscroll-behavior:none; touch-action:none;
  }
  .wrap{max-width:980px;margin:0 auto;padding:16px}
  .card{background:var(--panel);border:1px solid #232a35;border-radius:16px;box-shadow:0 10px 30px rgba(0,0,0,.35)}
  .header{padding:16px 18px;border-bottom:1px solid #232a35;display:flex;align-items:center;justify-content:space-between}
  .title{font-size:18px;font-weight:700;letter-spacing:.3px}
  .pill{padding:6px 10px;border-radius:999px;background:#0b1220;border:1px solid #1f2736;color:var(--muted);font-size:12px}
  .content{padding:16px}
  .grid{display:grid;gap:16px}
  @media(min-width:800px){ .grid{grid-template-columns:2fr 1fr} }
  .video{overflow:hidden;border-radius:12px;border:1px solid #232a35}
  img{width:100%;height:auto;display:block;pointer-events:none}
  .section h3{margin:0 0 8px 0;font-size:14px;color:var(--muted);letter-spacing:.4px;text-transform:uppercase}
  .row{display:flex;gap:10px;flex-wrap:wrap}
  button{
    appearance:none;background:var(--btn);color:var(--text);
    border:1px solid #2a3240;border-radius:12px;padding:14px 16px;
    font-size:16px;cursor:pointer;transition:.1s transform,.1s filter;
    touch-action:none; -webkit-tap-highlight-color: transparent;
  }
  button:active,button.active{transform:translateY(1px);outline:none}
  .btn-danger{border-color:#7a1c1c;background:#1c0d0d}
  .btn-accent{border-color:#2b5fcc;background:#0d1324}
  .controls{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
  .controls .blank{visibility:hidden}
  .footer{padding:12px 18px;border-top:1px solid #232a35;color:var(--muted);font-size:12px;display:flex;justify-content:space-between}
</style>
</head>
<body>
<div class="wrap">
  <div class="card">
    <div class="header">
      <div class="title">ESP32-CAM Rover</div>
      <div class="pill"><span id="ip">Loading…</span></div>
    </div>
    <div class="content grid">
      <div class="left">
        <div class="video"><img id="stream" src="/jpg"></div>
      </div>
      <div class="right">
        <div class="section">
          <h3>Drive</h3>
          <div class="controls">
            <div class="blank"></div>
            <button id="btn-forward">▲ Forward</button>
            <div class="blank"></div>

            <button id="btn-left">◀ Left</button>
            <button id="btn-stop" class="btn-danger">■ Stop</button>
            <button id="btn-right">▶ Right</button>

            <div class="blank"></div>
            <button id="btn-back">▼ Back</button>
            <div class="blank"></div>
          </div>
        </div>

        <div class="section" style="margin-top:14px">
          <h3>Lights</h3>
          <div class="row">
            <button id="btn-flash" class="btn-accent">Flash Toggle</button>
          </div>
        </div>

      </div>
    </div>
    <div class="footer">
      <span>Open from any device: http://<span id="ip2">IP</span>/</span>
      <span id="status">Ready</span>
    </div>
  </div>
</div>

<script>
// ---- status text + helpers ----
function setStatus(s){ const el=document.getElementById('status'); el.textContent=s; setTimeout(()=>{el.textContent='Ready'}, 500); }
function go(p){ return fetch(p,{cache:'no-store',headers:{'Cache-Control':'no-store'}}).then(r=>r.text()).then(t=>{setStatus(t||'OK');return t;}).catch(()=>{setStatus('ERR');}); }

// ---- snapshot pump (single short request per frame) ----
let fps = %FPS%;
let inflight = false;
async function pump(){
  if(inflight) return;
  inflight = true;
  const img = document.getElementById('stream');
  const ts = performance.now().toString().replace('.','');
  img.onload = ()=>{ inflight = false; setTimeout(pump, 1000/fps); };
  img.onerror = ()=>{ inflight = false; setTimeout(pump, 200); };
  img.src = '/jpg?t='+ts;
}

// ---- hold-to-drive: press=command, release=stop ----
function addHoldButton(id, path){
  const el = document.getElementById(id);
  const down = (e)=>{ e.preventDefault(); el.classList.add('active'); go(path); };
  const up   = (e)=>{ e.preventDefault(); el.classList.remove('active'); go('/stop'); };
  el.addEventListener('pointerdown', down);
  el.addEventListener('pointerup', up);
  el.addEventListener('pointercancel', up);
  el.addEventListener('pointerleave', up);
}
addHoldButton('btn-forward','/forward');
addHoldButton('btn-left','/left');
addHoldButton('btn-right','/right');
addHoldButton('btn-back','/back');

document.getElementById('btn-stop').addEventListener('pointerdown', (e)=>{ e.preventDefault(); go('/stop'); });
document.getElementById('btn-flash').addEventListener('pointerdown', (e)=>{ e.preventDefault(); go('/flash'); });

// init
window.addEventListener('load', ()=>{
  const ip = location.hostname || 'ESP32';
  document.getElementById('ip').textContent = ip;
  document.getElementById('ip2').textContent = ip;
  pump();
});
window.oncontextmenu = ()=>false; // no copy/menus
</script>
</body></html>
)HTML";

/* ===================== HTTP HELPERS ===================== */
static esp_err_t send_text(httpd_req_t *req, const char* msg){
  httpd_resp_set_type(req, "text/plain; charset=utf-8");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_set_hdr(req, "Connection", "close");
  return httpd_resp_send(req, msg, HTTPD_RESP_USE_STRLEN);
}

static String html_with_fps(){
  String s = INDEX_HTML;
  s.replace("%FPS%", String(SNAPSHOT_FPS));
  return s;
}

static esp_err_t index_handler(httpd_req_t *req){
  auto page = html_with_fps();
  httpd_resp_set_hdr(req, "Content-Type", "text/html; charset=utf-8");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_set_hdr(req, "Connection", "close");
  return httpd_resp_send(req, page.c_str(), page.length());
}

/* ===================== SNAPSHOT JPEG ===================== */
static esp_err_t jpg_handler(httpd_req_t *req){
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb){ httpd_resp_send_500(req); return ESP_FAIL; }

  esp_err_t res = ESP_OK;
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_set_hdr(req, "Connection", "close");

  if (fb->format == PIXFORMAT_JPEG){
    res = httpd_resp_send(req, (const char*)fb->buf, fb->len);
  } else {
    uint8_t *jpg_buf = NULL; size_t jpg_len = 0;
    if(frame2jpg(fb, DEFAULT_JPEG_Q, &jpg_buf, &jpg_len)){
      res = httpd_resp_send(req, (const char*)jpg_buf, jpg_len);
      free(jpg_buf);
    } else {
      httpd_resp_send_500(req); res = ESP_FAIL;
    }
  }
  esp_camera_fb_return(fb);
  return res;
}

/* ===================== CONTROLS ===================== */
static esp_err_t forward_handler(httpd_req_t *r){ motorForward(); return send_text(r, "forward"); }
static esp_err_t back_handler   (httpd_req_t *r){ motorBack();    return send_text(r, "back"); }
static esp_err_t left_handler   (httpd_req_t *r){ motorLeft();    return send_text(r, "left"); }
static esp_err_t right_handler  (httpd_req_t *r){ motorRight();   return send_text(r, "right"); }
static esp_err_t stop_handler   (httpd_req_t *r){ motorStop();    return send_text(r, "stop"); }
static esp_err_t flash_handler  (httpd_req_t *r){
  flash_on = !flash_on; flash_apply(flash_on);
  return send_text(r, flash_on? "flash on" : "flash off");
}

/* ===================== SERVER ===================== */
static void start_server(){
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.server_port = 80;
  cfg.lru_purge_enable = true;    // reclaim handlers if needed
  // Keep defaults for sockets/backlog (snapshot mode is light)

  if(httpd_start(&httpd_handle, &cfg) != ESP_OK){
    LOG("[HTTP] start FAILED\n"); return;
  }

  httpd_uri_t uri_index  = { .uri="/",        .method=HTTP_GET, .handler=index_handler,   .user_ctx=NULL };
  httpd_uri_t uri_jpg    = { .uri="/jpg",     .method=HTTP_GET, .handler=jpg_handler,     .user_ctx=NULL };
  httpd_uri_t uri_fwd    = { .uri="/forward", .method=HTTP_GET, .handler=forward_handler, .user_ctx=NULL };
  httpd_uri_t uri_back   = { .uri="/back",    .method=HTTP_GET, .handler=back_handler,    .user_ctx=NULL };
  httpd_uri_t uri_left   = { .uri="/left",    .method=HTTP_GET, .handler=left_handler,    .user_ctx=NULL };
  httpd_uri_t uri_right  = { .uri="/right",   .method=HTTP_GET, .handler=right_handler,   .user_ctx=NULL };
  httpd_uri_t uri_stop   = { .uri="/stop",    .method=HTTP_GET, .handler=stop_handler,    .user_ctx=NULL };
  httpd_uri_t uri_flash  = { .uri="/flash",   .method=HTTP_GET, .handler=flash_handler,   .user_ctx=NULL };

  httpd_register_uri_handler(httpd_handle, &uri_index);
  httpd_register_uri_handler(httpd_handle, &uri_jpg);
  httpd_register_uri_handler(httpd_handle, &uri_fwd);
  httpd_register_uri_handler(httpd_handle, &uri_back);
  httpd_register_uri_handler(httpd_handle, &uri_left);
  httpd_register_uri_handler(httpd_handle, &uri_right);
  httpd_register_uri_handler(httpd_handle, &uri_stop);
  httpd_register_uri_handler(httpd_handle, &uri_flash);

  LOG("[HTTP] server :%d up\n", cfg.server_port);
}

/* ===================== SETUP / LOOP ===================== */
void setup(){
  // Motors
  pinMode(IN1,OUTPUT); pinMode(IN2,OUTPUT); pinMode(IN3,OUTPUT); pinMode(IN4,OUTPUT);
  motorStop();

  // Flash default off
  flash_apply(false);

  Serial.begin(115200); delay(150);
  LOG("\n=== ESP32-CAM Rover (light/optimized) ===\n");

  // Camera init (speed-biased)
  camera_config_t c;
  c.ledc_channel = LEDC_CHANNEL_0;
  c.ledc_timer   = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM;  c.pin_d1 = Y3_GPIO_NUM;  c.pin_d2 = Y4_GPIO_NUM;  c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM;  c.pin_d5 = Y7_GPIO_NUM;  c.pin_d6 = Y8_GPIO_NUM;  c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM; c.pin_pclk = PCLK_GPIO_NUM; c.pin_vsync = VSYNC_GPIO_NUM; c.pin_href = HREF_GPIO_NUM;
  c.pin_sscb_sda = SIOD_GPIO_NUM; c.pin_sscb_scl = SIOC_GPIO_NUM; c.pin_pwdn = PWDN_GPIO_NUM; c.pin_reset = RESET_GPIO_NUM;

  c.xclk_freq_hz = 20000000;        // safe & fast (try 24000000 if your module is stable)
  c.pixel_format = PIXFORMAT_JPEG;  // fastest pipeline

  // Extra speed hints (present in Arduino-ESP32 2.0+/3.0+)
  c.grab_mode   = CAMERA_GRAB_LATEST;           // prefer newest frame
  c.fb_location = CAMERA_FB_IN_PSRAM;           // PSRAM for buffers

  if(psramFound()){
    c.frame_size    = DEFAULT_FRAMESIZE;        // QVGA default
    c.jpeg_quality  = DEFAULT_JPEG_Q;           // ~24
    c.fb_count      = 3;                        // 3 buffers for smoother pipeline
    LOG("[CAM] PSRAM ok\n");
  } else {
    c.frame_size    = FRAMESIZE_QQVGA;
    c.jpeg_quality  = DEFAULT_JPEG_Q + 4;       // a touch more compression
    c.fb_count      = 1;
    LOG("[CAM] PSRAM not found\n");
  }

  if(esp_camera_init(&c) != ESP_OK){
    LOG("[CAM] init failed\n");
    while(true){ delay(1000); }
  }
  LOG("[CAM] init ok\n");

  // Wi-Fi (speed/stability)
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);   // push radio power for throughput
  LOG("[WiFi] connecting");
  while(WiFi.status() != WL_CONNECTED){ delay(400); LOG("."); }
  LOG("\n[WiFi] http://%s/\n", WiFi.localIP().toString().c_str());

  start_server();
}

void loop(){ /* httpd handles everything */ }
