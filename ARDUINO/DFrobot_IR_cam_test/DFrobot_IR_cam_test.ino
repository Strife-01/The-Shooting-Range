// ESP32 AP + Wii IR camera viewer (I2C on SDA=21, SCL=22)
// - Serves a page with a live canvas ("image"), raw bytes, and decoded points
// - No motor/serial code; based on your minimal scaffold

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Wire.h>

// ---------- Config ----------
const char* SYSTEM_NAME = "ROVER NET";  // AP SSID
const byte DNS_PORT = 53;               // captive-portal style DNS
const int I2C_SDA = 21;                 // per your request
const int I2C_SCL = 22;                 // per your request
const uint8_t IR_ADDR = 0x58;           // Wii IR camera (7-bit address)
const uint16_t CANVAS_W = 512;          // web canvas size (scaled from 1024x768)
const uint16_t CANVAS_H = 384;

WebServer server(80);
DNSServer dnsServer;

// Wii IR state (volatile-ish; no ISRs here, but keep local copies when serving)
uint8_t rawBuf[16] = { 0 };
int16_t px[4] = { -1, -1, -1, -1 };
int16_t py[4] = { -1, -1, -1, -1 };
uint8_t sbyte[4] = { 0, 0, 0, 0 };
uint32_t lastCaptureMs = 0;
const uint32_t POLL_INTERVAL_MS = 15;  // ~66Hz


// from here we solve the camera position when observing 2,3 or 4 blobs.
#include <math.h>
#include <stdio.h>
#define NUM_LEDS 4

struct Vec3 {
  float x, y, z;
};

struct Mat3 {
  float m[3][3];
};

// Pose (world -> camera): Xc = R*Xw + t
struct PoseRT {
  Mat3 R;
  Vec3 t;
};

//homography struct as described by Mihai
// essentially we want to solve A*x = b for x[8]
struct H8x8 {
  float A[8][8];
  float b[8];
};

// camera intrinsics
const float width = 1024.0f;
const float height = 768.0f;
const float fx = 1731.0f;
const float fy = 1919.0f;
const float cx = width * 0.5f;
const float cy = height * 0.5f;

// World LED positions
Vec3 leds[NUM_LEDS] = {
  { 0.0f, 0.0f, 0.0f },     // top-left
  { 100.0f, 0.0f, 0.0f },   // top-right
  { 100.0f, 60.0f, 0.0f },  // bottom-right
  { 0.0f, 60.0f, 0.0f }     // bottom-left
};

// pose mapping
static PoseRT currentPose;
static bool validPose = false;
static int lastAssign[NUM_LEDS] = { -1, -1, -1, -1 };

static inline Vec3 v3(float x, float y, float z) {
  return Vec3{ x, y, z };
}
static inline Vec3 v_add(Vec3 a, Vec3 b) {
  return v3(a.x + b.x, a.y + b.y, a.z + b.z);
}
static inline Vec3 v_sub(Vec3 a, Vec3 b) {
  return v3(a.x - b.x, a.y - b.y, a.z - b.z);
}
static inline float v_dot(Vec3 a, Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
static inline float v_len(Vec3 a) {
  return sqrtf(v_dot(a, a));
}
static inline Vec3 v_norm(Vec3 a) {
  float L = v_len(a);
  return (L > 1e-8f) ? v3(a.x / L, a.y / L, a.z / L) : a;
}

static inline Mat3 I3() {
  Mat3 R{ { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } } };
  return R;
}
static inline Vec3 m3_mul_vec(const Mat3& R, Vec3 a) {
  return v3(
    R.m[0][0] * a.x + R.m[0][1] * a.y + R.m[0][2] * a.z,
    R.m[1][0] * a.x + R.m[1][1] * a.y + R.m[1][2] * a.z,
    R.m[2][0] * a.x + R.m[2][1] * a.y + R.m[2][2] * a.z);
}
static inline Mat3 m3_mul(const Mat3& A, const Mat3& B) {
  Mat3 C{};
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 3; c++)
      C.m[r][c] = A.m[r][0] * B.m[0][c] + A.m[r][1] * B.m[1][c] + A.m[r][2] * B.m[2][c];
  return C;
}
static inline Mat3 m3_T(const Mat3& A) {
  Mat3 B{};
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 3; c++) B.m[r][c] = A.m[c][r];
  return B;
}

// Small-angle rotation delta: R_delta ≈ I + [w]_x
static inline Mat3 rot_delta(float rx, float ry, float rz) {
  Mat3 R = I3();
  R.m[0][1] = -rz;
  R.m[0][2] = ry;
  R.m[1][0] = rz;
  R.m[1][2] = -rx;
  R.m[2][0] = -ry;
  R.m[2][1] = rx;
  return R;
}

// Camera position in world C = -R^T * t
static inline Vec3 camera_center_world(const PoseRT& P) {
  Mat3 Rt = m3_T(P.R);
  Vec3 ct = v3(
    -(Rt.m[0][0] * P.t.x + Rt.m[0][1] * P.t.y + Rt.m[0][2] * P.t.z),
    -(Rt.m[1][0] * P.t.x + Rt.m[1][1] * P.t.y + Rt.m[1][2] * P.t.z),
    -(Rt.m[2][0] * P.t.x + Rt.m[2][1] * P.t.y + Rt.m[2][2] * P.t.z));
  return ct;
}

// Convert pixel to normalized camera ray (unit direction)
static inline Vec3 normalised_ray_from_pixel(float u, float v) {
  float x = (u - cx) / fx;
  float y = (v - cy) / fy;
  return v_norm(v3(x, y, 1.0f));
}

static inline Vec3 v_safe_norm(Vec3 a) {
  float L2 = v_dot(a, a);
  if (L2 <= 1e-12f) return v3(0, 0, 1);  // fallback ray (shouldn't happen)
  float invL = 1.0f / sqrtf(L2);
  return v3(a.x * invL, a.y * invL, a.z * invL);
}

static inline void project_point(const PoseRT& P, const Vec3& Xw, float& u, float& v) {
  const Vec3 Xc = v_add(m3_mul_vec(P.R, Xw), P.t);

  const float z_safe = (Xc.z > 1e-6f) ? Xc.z : 1e-6f;
  const float invZ = 1.0f / z_safe;

  u = fx * (Xc.x * invZ) + cx;
  v = fy * (Xc.y * invZ) + cy;
}

static inline float clampf(float x, float lo, float hi) {
  return (x < lo) ? lo : (x > hi ? hi : x);
}

static float repro_error_RMS(const PoseRT& P, const int n, const int assign[], const float imgU[], const float imgV[]) {
  constexpr float kHugeError = 1e9f;

  float sumSq = 0.0f;
  int used = 0;

  for (int i = 0; i < n; ++i) {
    const int ledIdx = assign[i];
    if (ledIdx < 0) continue;          // this blob not assigned
    if (ledIdx >= NUM_LEDS) continue;  // out-of-range safety

    float uProj = 0.0f, vProj = 0.0f;
    project_point(P, leds[ledIdx], uProj, vProj);

    const float du = imgU[i] - uProj;
    const float dv = imgV[i] - vProj;

    sumSq += du * du + dv * dv;
    ++used;
  }

  if (used == 0) return kHugeError;  // no valid correspondences
  return sqrtf(sumSq / used);        // RMS in pixels
}

static inline bool pixel_to_norm_safe(float u, float v, float& xn, float& yn) {
  if (!(u >= 0.0f && v >= 0.0f)) {
    xn = 0.0f;
    yn = 0.0f;
    return false;
  }
  // normalize with the camera intrics rest of the pixels
  xn = (u - cx) / fx;
  yn = (v - cy) / fy;

  //chat told me to use fabsf cuz its faster and safer yay AI?
  if (!isfinite(xn) || !isfinite(yn) || fabsf(xn) > 1e6f || fabsf(yn) > 1e6f) {
    xn = 0.0f;
    yn = 0.0f;
    return false;
  }
  return true;
}

// I just need this for shortcut
float absf(float x){
  if(x<0) return -x;
  return x;
}
// Gaussian solver implemented by Mihai
bool solve8x8(H8x8* H, float x[8]){
  // 1. Forward elimination
  for(int i = 0; i<8; i++){
    // Pivoting to avoid potential arithmetic errors
    int r = i;
    float largest_value = absf(H->A[i][i]);
    for(int k = i+1; k<8; k++){
      if(absf(H->A[k][i]) > largest_value){
        largest_value = absf(H->A[k][i]);
        r = k;
      }
    }
    if(r!=i){
      // Swap rows in A
      for(int c = 0; c<8; c++){
        float tmp = H->A[i][c];
        H->A[i][c] = H->A[r][c];
        H->A[r][c] = tmp;
      }
      // Swap elements in b
      float tmpb = H->b[i];
      H->b[i] = H->b[r];
      H->b[r] = tmpb;
    }
    // It is better to refuse computing the matrix rather than working with near-zero divisions
    if (absf(H->A[i][i]) < 1e-6f) return false; // singular matrix, cannot proceed
    // Forward elimination
    for(int j = i+1; j < 8; j++){
      float m = H->A[j][i] / H->A[i][i];
      for (int c = i; c < 8; c++) {
        H->A[j][c] -= m * H->A[i][c];
      }
      H->b[j] -= m * H->b[i];
    }
  }
  // 2. Back-substitution
  for(int i = 7; i >= 0; i--){
    float sum = 0.0f;
    for(int j = i+1; j < 8; j++){
        sum += H->A[i][j] * x[j];
    }
    x[i] = (H->b[i] - sum) / H->A[i][i];
  }

  // 3. Return true if successful, this is the most complicated section of the algorithm
  return true;
}
/* I am not entirely sure whether this is required but in case you need it you can use it
 * otherwise you can remove it
 */
bool computeHomography(const float srcX[4], const float srcY[4], const float dstX[4], const float dstY[4], Mat3& H){
  H8x8 sys;   // the 8x8 system A*x = b
    float x[8]; // solution vector

    for (int i = 0; i < 4; i++) {
        float u = dstX[i];
        float v = dstY[i];
        float xw = srcX[i];
        float yw = srcY[i];

        // Row 2*i
        sys.A[2*i][0] = xw;
        sys.A[2*i][1] = yw;
        sys.A[2*i][2] = 1.0f;
        sys.A[2*i][3] = 0.0f;
        sys.A[2*i][4] = 0.0f;
        sys.A[2*i][5] = 0.0f;
        sys.A[2*i][6] = -u * xw;
        sys.A[2*i][7] = -u * yw;
        sys.b[2*i] = u;

        // Row 2*i + 1
        sys.A[2*i + 1][0] = 0.0f;
        sys.A[2*i + 1][1] = 0.0f;
        sys.A[2*i + 1][2] = 0.0f;
        sys.A[2*i + 1][3] = xw;
        sys.A[2*i + 1][4] = yw;
        sys.A[2*i + 1][5] = 1.0f;
        sys.A[2*i + 1][6] = -v * xw;
        sys.A[2*i + 1][7] = -v * yw;
        sys.b[2*i + 1] = v;
    }

    // Solve the system
    if (!solve8x8(&sys, x)) return false; // singular matrix

    // Fill the homography matrix H
    H.m[0][0] = x[0]; H.m[0][1] = x[1]; H.m[0][2] = x[2];
    H.m[1][0] = x[3]; H.m[1][1] = x[4]; H.m[1][2] = x[5];
    H.m[2][0] = x[6]; H.m[2][1] = x[7]; H.m[2][2] = 1.0f;

    return true;
}



// static bool ray_angle_from_pixels(float u0, float v0, float u1, float v1, float& cosTheta, float& thetaRad) {
//   if (u0 < 0 || v0 < 0 || u1 < 0 || v1 < 0) {
//     cosTheta = 1.0f;
//     thetaRad = 0.0f;
//     return false;
//   }

//   Vec3 f0 = v_safe_norm(normalised_ray_from_pixel(u0, v0));
//   Vec3 f1 = v_safe_norm(normalised_ray_from_pixel(u1, v1));

//   float c = v_dot(f0, f1);
//   c = clampf(c, -1.0f, 1.0f);

//   cosTheta = c;
//   thetaRad = acosf(c);
//   return true;
// }


// --------- IR camera helpers ----------
static inline void write2(uint8_t d1, uint8_t d2) {
  Wire.beginTransmission(IR_ADDR);
  Wire.write(d1);
  Wire.write(d2);
  Wire.endTransmission();
}

bool initIR() {
  // Init sequence used by common PixArt modules
  write2(0x30, 0x01);
  delay(10);
  write2(0x30, 0x08);
  delay(10);
  write2(0x06, 0x90);
  delay(10);
  write2(0x08, 0xC0);
  delay(10);
  write2(0x1A, 0x40);
  delay(10);
  write2(0x33, 0x33);
  delay(10);
  delay(100);
  return true;
}

void readIR() {
  // Request 16 bytes starting at 0x36
  Wire.beginTransmission(IR_ADDR);
  Wire.write(0x36);
  Wire.endTransmission();

  uint8_t buf[16] = { 0 };
  Wire.requestFrom((int)IR_ADDR, 16);
  int i = 0;
  while (Wire.available() && i < 16) {
    buf[i++] = Wire.read();
  }
  memcpy(rawBuf, buf, 16);

  // Decode 4 points
  uint8_t k = 1;
  for (int p = 0; p < 4; p++) {
    int16_t x = buf[k + 0];
    int16_t y = buf[k + 1];
    uint8_t s = buf[k + 2];
    x += (int16_t)((s & 0x30) << 4);
    y += (int16_t)((s & 0xC0) << 2);

    // no point detected: many modules return 0xFF,0xFF
    if (buf[k] == 0xFF && buf[k + 1] == 0xFF) {
      x = -1;
      y = -1;
    }

    px[p] = x;
    py[p] = y;
    sbyte[p] = s;
    k += 3;
  }
  lastCaptureMs = millis();
}

// ---------- Web UI ----------
const char PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no" />
<title>ESP32 IR Camera</title>
<style>
  html,body{margin:0;padding:0;background:#111;color:#eee;font-family:system-ui,Segoe UI,Roboto,Helvetica,Arial,sans-serif}
  .wrap{max-width:1000px;margin:0 auto;padding:16px;display:grid;gap:16px}
  .card{background:#1a1a1a;border:1px solid #2a2a2a;border-radius:16px;padding:16px;box-shadow:0 8px 24px rgba(0,0,0,.35)}
  h1{margin:0 0 8px;font-size:20px}
  .muted{opacity:.75}
  canvas{width:100%;height:auto;background:#050505;border:1px solid #2a2a2a;border-radius:12px}
  table{width:100%;border-collapse:collapse}
  th,td{padding:8px;border-bottom:1px solid #2a2a2a;text-align:left}
  .pill{display:inline-block;padding:4px 8px;border:1px solid #2a2a2a;border-radius:999px;background:#202020;font-size:12px}
  .grid{display:grid;grid-template-columns:2fr 1fr;gap:16px}
  @media (max-width:900px){ .grid{grid-template-columns:1fr} }
</style>
</head>
<body>
<div class="wrap">
  <div class="card">
    <h1>ESP32 Wii IR Camera <span id="stat" class="pill">starting…</span></h1>
    <div class="muted">Sensor space 1024×768 → canvas %CW%x%CH% (scaled). Up to 4 blobs.</div>
  </div>
  <div class="grid">
    <div class="card">
      <h1>Camera View</h1>
      <canvas id="view" width="%CW%" height="%CH%"></canvas>
      <div class="muted" style="margin-top:8px">This is a rendered view of the IR sensor's tracked blobs (the module doesn't stream full grayscale frames).</div>
    </div>
    <div class="card">
      <h1>Data</h1>
      <div><strong>Raw 16 bytes:</strong><pre id="raw" style="white-space:pre-wrap;background:#0f0f0f;border:1px solid #2a2a2a;border-radius:8px;padding:8px;margin-top:8px;"></pre></div>
      <table style="margin-top:8px">
        <thead><tr><th>Idx</th><th>X</th><th>Y</th><th>S byte</th></tr></thead>
        <tbody id="tbl"></tbody>
      </table>
      <div class="muted" id="ts" style="margin-top:8px"></div>
    </div>
  </div>
</div>
<script>
const CW=%CW%, CH=%CH%;
const SW=1024, SH=768;
const sx=CW/SW, sy=CH/SH;

const c=document.getElementById('view');
const ctx=c.getContext('2d');
const stat=document.getElementById('stat');
const rawEl=document.getElementById('raw');
const tbl=document.getElementById('tbl');
const ts=document.getElementById('ts');

function draw(points){
  ctx.clearRect(0,0,c.width,c.height);
  // grid
  ctx.globalAlpha=0.2; ctx.strokeStyle='#444';
  for(let x=0;x<=c.width;x+=64){ ctx.beginPath(); ctx.moveTo(x,0); ctx.lineTo(x,c.height); ctx.stroke(); }
  for(let y=0;y<=c.height;y+=48){ ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(c.width,y); ctx.stroke(); }
  ctx.globalAlpha=1;

  // points
  points.forEach((p,i)=>{
    if(p.x>=0 && p.y>=0){
      const x=p.x*sx, y=p.y*sy;
      const r=6+((p.s&0x0F)); // tiny size hint from S nibble
      ctx.beginPath(); ctx.arc(x,y,r,0,Math.PI*2);
      ctx.fillStyle='#0bf'; ctx.fill(); ctx.strokeStyle='#09f'; ctx.stroke();
      ctx.fillStyle='#fff'; ctx.font='12px system-ui';
      ctx.fillText(`#${i} (${p.x},${p.y})`, x+10, y-8);
    }
  });
}

function table(points){
  tbl.innerHTML = points.map((p,i)=>`<tr><td>#${i}</td><td>${p.x}</td><td>${p.y}</td><td>${p.s}</td></tr>`).join('');
}

let ok=false;
async function tick(){
  try{
    const r = await fetch('/data',{cache:'no-store'});
    if(!r.ok) throw new Error(r.status);
    const j = await r.json();
    ok=true; stat.textContent='online';

    rawEl.textContent = j.raw;
    table(j.points);
    draw(j.points);
    ts.textContent = 'Updated: '+new Date().toLocaleTimeString();
  }catch(e){
    ok=false; stat.textContent='connecting…';
  }finally{
    setTimeout(tick, ok?50:500);
  }
}
tick();
</script>
</body>
</html>
)HTML";

String indexPage() {
  String html = FPSTR(PAGE);
  html.replace("%CW%", String(CANVAS_W));
  html.replace("%CH%", String(CANVAS_H));
  return html;
}

String hex2(uint8_t b) {
  char t[3];
  snprintf(t, sizeof(t), "%02X", b);
  return String(t);
}

void handleRoot() {
  server.send(200, "text/html; charset=utf-8", indexPage());
}

void handleData() {
  // snapshot
  uint8_t rb[16];
  memcpy(rb, rawBuf, 16);
  int16_t lx[4], ly[4];
  uint8_t ls[4];
  for (int i = 0; i < 4; i++) {
    lx[i] = px[i];
    ly[i] = py[i];
    ls[i] = sbyte[i];
  }

  String rawHex;
  for (int i = 0; i < 16; i++) {
    rawHex += hex2(rb[i]);
    if (i != 15) rawHex += " ";
  }

  String json = "{";
  json += "\"raw\":\"" + rawHex + "\",";
  json += "\"points\":[";
  for (int i = 0; i < 4; i++) {
    json += "{\"x\":" + String(lx[i]) + ",\"y\":" + String(ly[i]) + ",\"s\":" + String(ls[i]) + "}";
    if (i != 3) json += ",";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleNotFound() {
  handleRoot();
}

// ---------- Wi-Fi / Web ----------
void wifiSetup() {
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  delay(100);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(SYSTEM_NAME);  // open AP; add password if desired
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));

  Serial.println();
  Serial.println(F("=== ESP32 IR Cam ==="));
  Serial.printf("AP SSID: %s\n", SYSTEM_NAME);
  Serial.printf("AP IP:   %s\n", WiFi.softAPIP().toString().c_str());
  Serial.println(F("Open:    http://192.168.4.1/"));

  server.on("/", HTTP_GET, handleRoot);
  server.on("/data", HTTP_GET, handleData);
  server.onNotFound(handleNotFound);
  server.begin();

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
}

void setup() {
  Serial.begin(115200);
  delay(100);

  //sanity check
  currentPose.R = I3();
  currentPose.t = v3(0, 0, 300.0f);  // 300 cm in front
  for (int i = 0; i < NUM_LEDS; i++) {
    float u, v;
    project_point(currentPose, leds[i], u, v);
    Serial.printf("LED%d -> (%.1f, %.1f)\n", i, u, v);
  }

  // I2C on pins you asked for
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);
  initIR();

  wifiSetup();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  // poll sensor
  uint32_t now = millis();
  if (now - lastCaptureMs >= POLL_INTERVAL_MS) {
    readIR();
  }
}
