#include <Wire.h>
// ---------- IMU (MPU6050) ----------
#define MPU6050_ADDR 0x68
#define REG_PWR_MGMT_1 0x6B
#define REG_ACCEL_XOUT_H 0x3B  // Burst: Accel(6) + Temp(2) + Gyro(6) = 14 bytes

#define SDA_PIN 21
#define SCL_PIN 22
#define NAME "Mouse"

#include <math.h>
#include <stdio.h>

#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>



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
struct H8x8 {
  float A[8][8];
  float b[8];
};

struct ImuSample {
  int16_t ax, ay, az;
  int16_t gx, gy, gz;
};

struct SimpleCalib {
  float accel_bias_x = 0.0f, accel_bias_y = 0.0f, accel_bias_z = 0.0f;  // LSB
  float gyro_bias_x = 0.0f, gyro_bias_y = 0.0f, gyro_bias_z = 0.0f;     // LSB
  bool valid = false;
};

SimpleCalib simpleCalib;
ImuSample latest;  // holds the most recent reading
PoseRT currentPose;
bool validPose = false;
Mat3 R_ref = { { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } } };  // identity initial

// BLE HID device and input report characteristic
BLEHIDDevice* hidDevice;
BLECharacteristic* inputReport;
bool deviceConnected = false;


inline Vec3 v3(float x, float y, float z) {
  return Vec3{ x, y, z };
}
inline Vec3 v_add(Vec3 a, Vec3 b) {
  return v3(a.x + b.x, a.y + b.y, a.z + b.z);
}
inline Vec3 v_sub(Vec3 a, Vec3 b) {
  return v3(a.x - b.x, a.y - b.y, a.z - b.z);
}
inline float v_dot(Vec3 a, Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline float v_len(Vec3 a) {
  return sqrtf(v_dot(a, a));
}
inline Vec3 v_norm(Vec3 a) {
  float L = v_len(a);
  return (L > 1e-8f) ? v3(a.x / L, a.y / L, a.z / L) : a;
}

inline Mat3 I3() {
  Mat3 R{ { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } } };
  return R;
}
inline Vec3 m3_mul_vec(const Mat3& R, Vec3 a) {
  return v3(
    R.m[0][0] * a.x + R.m[0][1] * a.y + R.m[0][2] * a.z,
    R.m[1][0] * a.x + R.m[1][1] * a.y + R.m[1][2] * a.z,
    R.m[2][0] * a.x + R.m[2][1] * a.y + R.m[2][2] * a.z);
}
inline Mat3 m3_mul(const Mat3& A, const Mat3& B) {
  Mat3 C{};
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 3; c++)
      C.m[r][c] = A.m[r][0] * B.m[0][c] + A.m[r][1] * B.m[1][c] + A.m[r][2] * B.m[2][c];
  return C;
}
inline Mat3 m3_T(const Mat3& A) {
  Mat3 B{};
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 3; c++) B.m[r][c] = A.m[c][r];
  return B;
}
inline Vec3 v_cross(Vec3 a, Vec3 b) {
  return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

inline Mat3 m3_add(const Mat3& A, const Mat3& B) {
  Mat3 C{};
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 3; c++) C.m[r][c] = A.m[r][c] + B.m[r][c];
  return C;
}
inline Mat3 m3_scale(const Mat3& A, float s) {
  Mat3 C{};
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 3; c++) C.m[r][c] = A.m[r][c] * s;
  return C;
}
inline Mat3 skew(Vec3 w) {  // [w]_x
  Mat3 S{};
  S.m[0][0] = 0;
  S.m[0][1] = -w.z;
  S.m[0][2] = w.y;
  S.m[1][0] = w.z;
  S.m[1][1] = 0;
  S.m[1][2] = -w.x;
  S.m[2][0] = -w.y;
  S.m[2][1] = w.x;
  S.m[2][2] = 0;
  return S;
}

// quick Gram–Schmidt re-orthonormalization
inline Mat3 m3_orthonormalize(const Mat3& R) {
  Vec3 x = v3(R.m[0][0], R.m[1][0], R.m[2][0]);
  Vec3 y = v3(R.m[0][1], R.m[1][1], R.m[2][1]);
  Vec3 z = v3(R.m[0][2], R.m[1][2], R.m[2][2]);

  x = v_norm(x);
  y = v_sub(y, v3(x.x * v_dot(x, y), x.y * v_dot(x, y), x.z * v_dot(x, y)));
  y = v_norm(y);
  z = v_cross(x, y);

  Mat3 O{};
  O.m[0][0] = x.x;
  O.m[0][1] = y.x;
  O.m[0][2] = z.x;
  O.m[1][0] = x.y;
  O.m[1][1] = y.y;
  O.m[1][2] = z.y;
  O.m[2][0] = x.z;
  O.m[2][1] = y.z;
  O.m[2][2] = z.z;
  return O;
}

inline Vec3 m3_toEuler(const Mat3& R) {
  float roll = atan2f(R.m[2][1], R.m[2][2]);
  float pitch = -asinf(R.m[2][0]);
  float yaw = atan2f(R.m[1][0], R.m[0][0]);
  return v3(roll, pitch, yaw);
}

inline int16_t clampToI16(float v) {
  if (v > 32767.0f) return 32767;
  if (v < -32768.0f) return -32768;
  return (int16_t)lrintf(v);  // round to nearest
}

// Bluetooth LE server callbacks to track connection
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    // Enable notifications for the input report (Client Characteristic Config 0x2902)
    BLE2902* cccDesc = (BLE2902*)inputReport->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    if (cccDesc) cccDesc->setNotifications(true);
  }
  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    BLE2902* cccDesc = (BLE2902*)inputReport->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    if (cccDesc) cccDesc->setNotifications(false);
    // Advertising restart on disconnect (optional)
    pServer->getAdvertising()->start();
  }
};

// HID report descriptor for an absolute-positioning mouse with 2 buttons (LMB/RMB) and X,Y axes
const uint8_t hidReportDescriptor[] = {
  0x05, 0x01,  // USAGE_PAGE (Generic Desktop)
  0x09, 0x02,  // USAGE (Mouse)
  0xA1, 0x01,  // COLLECTION (Application)
  0x85, 0x01,  //   REPORT_ID (1)
  0x09, 0x01,  //   USAGE (Pointer)
  0xA1, 0x00,  //   COLLECTION (Physical)
  // Buttons (2 bits for LMB/RMB)
  0x05, 0x09,  //     USAGE_PAGE (Button)
  0x19, 0x01,  //     USAGE_MINIMUM (Button 1)
  0x29, 0x02,  //     USAGE_MAXIMUM (Button 2)
  0x15, 0x00,  //     LOGICAL_MINIMUM (0)
  0x25, 0x01,  //     LOGICAL_MAXIMUM (1)
  0x95, 0x02,  //     REPORT_COUNT (2 buttons)
  0x75, 0x01,  //     REPORT_SIZE (1)
  0x81, 0x02,  //     INPUT (Data,Var,Abs) – 2 button bits (LMB, RMB)
  // Padding to next byte boundary
  0x95, 0x01,  //     REPORT_COUNT (1)
  0x75, 0x06,  //     REPORT_SIZE (6)
  0x81, 0x03,  //     INPUT (Const,Var,Abs) – 6-bit padding
  // Absolute X and Y coordinates (16-bit each)
  0x05, 0x01,        //     USAGE_PAGE (Generic Desktop)
  0x09, 0x30,        //     USAGE (X)
  0x09, 0x31,        //     USAGE (Y)
  0x15, 0x00,        //     LOGICAL_MINIMUM (0)
  0x26, 0xFF, 0x7F,  //     LOGICAL_MAXIMUM (32767) – 16-bit max:contentReference[oaicite:3]{index=3}:contentReference[oaicite:4]{index=4}
  0x75, 0x10,        //     REPORT_SIZE (16)
  0x95, 0x02,        //     REPORT_COUNT (2) – X and Y
  0x81, 0x02,        //     INPUT (Data,Var,Abs) – 16-bit absolute coords
  0xC0,              //   END_COLLECTION (Physical)
  0xC0               // END_COLLECTION (Application)
};



void setupBLEMouse() {
  Serial.println("Starting bluetooth mouse");

  // Initialize BLE and set up the HID device
  BLEDevice::init(NAME);  // Device name seen by hosts
  BLEServer* bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new MyServerCallbacks());

  // Create HID device and input report characteristic (report ID 1)
  hidDevice = new BLEHIDDevice(bleServer);
  inputReport = hidDevice->inputReport(1);           // Report ID = 1 (matches descriptor)
  hidDevice->manufacturer()->setValue("Espressif");  // Manufacturer string
  hidDevice->pnp(0x02, 0x1234, 0x5678, 0x0100);      // PnP parameters: Bluetooth sig, VID, PID, version
  hidDevice->hidInfo(0x00, 0x02);                    // HID info: country code 0, HID flags (normally connectable) 0x02

  // Set the HID report map (using the descriptor defined above) and start the HID service
  hidDevice->reportMap((uint8_t*)hidReportDescriptor, sizeof(hidReportDescriptor));
  hidDevice->startServices();

  // Start BLE advertising so host can find our device
  BLEAdvertising* advertising = bleServer->getAdvertising();
  advertising->setAppearance(HID_MOUSE);  // Set BLE appearance to HID Mouse:contentReference[oaicite:5]{index=5}
  advertising->addServiceUUID(hidDevice->hidService()->getUUID());
  advertising->start();

  // Optional: require bonding (pairing) for security, since HID usually needs it
  BLESecurity* bleSecurity = new BLESecurity();
  bleSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
  hidDevice->setBatteryLevel(100);  // Initialize battery level at 100%
}


// Helper function to send an absolute mouse report
void sendMouseReport(uint16_t x, uint16_t y, bool leftButton, bool rightButton) {
  // Prepare 5-byte input report: [buttons, X_LSB, X_MSB, Y_LSB, Y_MSB]
  uint8_t report[5];
  uint8_t buttons = 0;
  if (leftButton) buttons |= 0x01;   // LMB = bit0
  if (rightButton) buttons |= 0x02;  // RMB = bit1
  report[0] = buttons;
  report[1] = x & 0xFF;
  report[2] = (x >> 8) & 0xFF;
  report[3] = y & 0xFF;
  report[4] = (y >> 8) & 0xFF;
  // Send the report via notification
  inputReport->setValue(report, sizeof(report));
  inputReport->notify();
}


// scale factors for default MPU6050 ranges (±2g, ±250 dps)
static constexpr float ACCEL_SF = 16384.0f;  // LSB/g
static constexpr float GYRO_SF = 131.0f;     // LSB/(°/s)
static constexpr float DEG2RAD = 3.14159265358979323846f / 180.0f;

// Minimal I2C helpers
void i2cWriteByte(uint8_t addr, uint8_t reg, uint8_t data) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

void i2cReadBytes(uint8_t addr, uint8_t reg, uint8_t count, uint8_t* dest) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission(false);  // repeated start
  Wire.requestFrom(addr, count);
  for (uint8_t i = 0; i < count && Wire.available(); i++) {
    dest[i] = Wire.read();
  }
}

bool mpuBegin() {
  // Wake the device (clear sleep bit)
  i2cWriteByte(MPU6050_ADDR, REG_PWR_MGMT_1, 0x00);
  delay(50);
  // Quick presence check by reading one byte from data register
  uint8_t buf[1];
  i2cReadBytes(MPU6050_ADDR, REG_ACCEL_XOUT_H, 1, buf);
  return true;  // If the bus didn’t error, assume present (minimal)
}

void mpuRead(ImuSample& out) {
  uint8_t raw[14];
  i2cReadBytes(MPU6050_ADDR, REG_ACCEL_XOUT_H, 14, raw);
  out.ax = (int16_t)((raw[0] << 8) | raw[1]);
  out.ay = (int16_t)((raw[2] << 8) | raw[3]);
  out.az = (int16_t)((raw[4] << 8) | raw[5]);
  // raw[6], raw[7] = temperature (ignored)
  out.gx = (int16_t)((raw[8] << 8) | raw[9]);
  out.gy = (int16_t)((raw[10] << 8) | raw[11]);
  out.gz = (int16_t)((raw[12] << 8) | raw[13]);
}


// void handlePose() {
//   // R_display = R_est * R_ref^T
//   Mat3 R_disp = m3_mul(currentPose.R, m3_T(R_ref));
//   Vec3 rpy = m3_toEuler(R_disp);
//   char buf[160];
//   snprintf(buf, sizeof(buf), "{\"roll\":%.6f,\"pitch\":%.6f,\"yaw\":%.6f}", rpy.x, rpy.y, rpy.z);
//   server.send(200, "application/json", buf);
// }

// Hold the device still in any orientation while this runs.
bool simpleCalibrateRemove1g(uint16_t sample_count = 1500, uint16_t sample_delay_ms = 2) {
  long double sum_ax = 0, sum_ay = 0, sum_az = 0;
  long double sum_gx = 0, sum_gy = 0, sum_gz = 0;

  ImuSample s{};
  for (uint16_t i = 0; i < sample_count; ++i) {
    mpuRead(s);
    sum_ax += s.ax;
    sum_ay += s.ay;
    sum_az += s.az;
    sum_gx += s.gx;
    sum_gy += s.gy;
    sum_gz += s.gz;
    delay(sample_delay_ms);
  }

  float mean_ax = (float)(sum_ax / sample_count);
  float mean_ay = (float)(sum_ay / sample_count);
  float mean_az = (float)(sum_az / sample_count);

  float mean_gx = (float)(sum_gx / sample_count);
  float mean_gy = (float)(sum_gy / sample_count);
  float mean_gz = (float)(sum_gz / sample_count);

  // Gravity direction from accel means
  float g_len = sqrtf(mean_ax * mean_ax + mean_ay * mean_ay + mean_az * mean_az);
  if (g_len < 1e-3f) return false;

  float dir_x = mean_ax / g_len;
  float dir_y = mean_ay / g_len;
  float dir_z = mean_az / g_len;

  // Ideal 1 g vector in *raw LSB*
  float ideal_x = ACCEL_SF * dir_x;
  float ideal_y = ACCEL_SF * dir_y;
  float ideal_z = ACCEL_SF * dir_z;

  // Biases in LSB (what to subtract from raw)
  simpleCalib.accel_bias_x = mean_ax - ideal_x;
  simpleCalib.accel_bias_y = mean_ay - ideal_y;
  simpleCalib.accel_bias_z = mean_az - ideal_z;

  // Gyro biases in LSB (mean while still)
  simpleCalib.gyro_bias_x = mean_gx;
  simpleCalib.gyro_bias_y = mean_gy;
  simpleCalib.gyro_bias_z = mean_gz;

  simpleCalib.valid = true;
  return true;
}

void zeroOrientationToCurrent() {
  // current estimated pose is the reference
  R_ref = currentPose.R;
}


void mpuReadCalibratedSimple(ImuSample& raw_out, ImuSample& calc_out) {
  ImuSample r{};
  mpuRead(r);
  raw_out = r;

  // accel subtract biases
  float ax_corr = r.ax - simpleCalib.accel_bias_x;
  float ay_corr = r.ay - simpleCalib.accel_bias_y;
  float az_corr = r.az - simpleCalib.accel_bias_z;

  // gyro subtract biases
  float gx_corr = r.gx - simpleCalib.gyro_bias_x;
  float gy_corr = r.gy - simpleCalib.gyro_bias_y;
  float gz_corr = r.gz - simpleCalib.gyro_bias_z;

  calc_out.ax = clampToI16(ax_corr);
  calc_out.ay = clampToI16(ay_corr);
  calc_out.az = clampToI16(az_corr);
  calc_out.gx = clampToI16(gx_corr);
  calc_out.gy = clampToI16(gy_corr);
  calc_out.gz = clampToI16(gz_corr);
}

PoseRT setPose(ImuSample sample) {
  static bool init = false;
  static PoseRT pose;
  static uint32_t lastMu = 0;

  // dt
  uint32_t nowMu = micros();
  float DeltaTs = init ? (nowMu - lastMu) * 1e-6f : 0.0f;
  if (DeltaTs < 0.0f || DeltaTs > 0.1f) DeltaTs = 0.01f;  // clamp first cycle
  lastMu = nowMu;

  // accelerometer in gravities
  Vec3 accel_body_g = v3(sample.ax / ACCEL_SF, sample.ay / ACCEL_SF, sample.az / ACCEL_SF);
  Vec3 body_dir_accel = v_norm(accel_body_g);  // mostly gravity when still

  // gyro in rad/s (body frame)
  Vec3 angularVelocity_body_rad_s = v3(
    (sample.gx / GYRO_SF) * DEG2RAD,
    (sample.gy / GYRO_SF) * DEG2RAD,
    (sample.gz / GYRO_SF) * DEG2RAD);

  if (!init) {
    // to align body-up with world (opposite of mpureadd gravity direction).
    // the accelerometer indicates direction of gravity.
    Vec3 world_up = v3(0.0f, 0.0f, 1.0f);
    Vec3 body_down_accel = body_dir_accel;  // body down = calibratedd accel direction
    Vec3 body_up_accel = v3(-body_down_accel.x, -body_down_accel.y, -body_down_accel.z);

    // choose body X axis as projection of world X onto the plane orthogonal to body up.
    Vec3 world_x = v3(1.0f, 0.0f, 0.0f);
    float world_x_dot_body_up = v_dot(world_x, body_up_accel);
    Vec3 world_x_parallel_to_body_up = v3(
      body_up_accel.x * world_x_dot_body_up,
      body_up_accel.y * world_x_dot_body_up,
      body_up_accel.z * world_x_dot_body_up);

    Vec3 body_x_axis_in_world = v_norm(v_sub(world_x, world_x_parallel_to_body_up));

    if (v_len(body_x_axis_in_world) < 1e-3f) {
      // Fallback if world_x was nearly the same with body_up; use world Y
      body_x_axis_in_world = v3(0.0f, 1.0f, 0.0f);
    }

    // Body Y is orthogonal completion: y = up × x
    Vec3 body_y_axis_in_world = v_cross(body_up_accel, body_x_axis_in_world);

    // Construct rotation matrix whose columns are body axes expressed in world coordinates.
    // This defines R such that X_world -> X_body via X_body = R * X_world.
    pose.R = Mat3{};
    pose.R.m[0][0] = body_x_axis_in_world.x;
    pose.R.m[0][1] = body_y_axis_in_world.x;
    pose.R.m[0][2] = body_up_accel.x;
    pose.R.m[1][0] = body_x_axis_in_world.y;
    pose.R.m[1][1] = body_y_axis_in_world.y;
    pose.R.m[1][2] = body_up_accel.y;
    pose.R.m[2][0] = body_x_axis_in_world.z;
    pose.R.m[2][1] = body_y_axis_in_world.z;
    pose.R.m[2][2] = body_up_accel.z;

    pose.t = v3(0.0f, 0.0f, 0.0f);
    init = true;
    validPose = true;
    currentPose = pose;
    return pose;
  }

  Mat3 identityMatrix = I3();
  Mat3 smallRotationFromGyro =
    m3_add(identityMatrix, m3_scale(skew(angularVelocity_body_rad_s), DeltaTs));
  pose.R = m3_mul(pose.R, smallRotationFromGyro);
  pose.R = m3_orthonormalize(pose.R);

  float accelMagnitude_g = v_len(accel_body_g);
  bool accelLooksLikeGravity = fabsf(accelMagnitude_g - 1.0f) < 0.15f;  // ~±0.15 g window

  if (accelLooksLikeGravity) {
    // Expected gravity in body from current orientation: gravity_world = (0,0,-1)
    Vec3 gravity_world = v3(0.0f, 0.0f, -1.0f);
    Mat3 rotationTranspose = m3_T(pose.R);
    Vec3 gravity_body_estimate = m3_mul_vec(rotationTranspose, gravity_world);

    // Error is estimated vs calibratedsimd gravity directions in body frame
    Vec3 tiltError_body = v_cross(gravity_body_estimate, body_dir_accel);

    // Small proportional blend to damp roll/pitch drift (tune gain)
    const float proportionalGain = 2.0f;
    Vec3 correctiveAngularVelocity_body = v3(
      tiltError_body.x * proportionalGain,
      tiltError_body.y * proportionalGain,
      tiltError_body.z * proportionalGain);

    Mat3 smallRotationFromAccel =
      m3_add(identityMatrix, m3_scale(skew(correctiveAngularVelocity_body), DeltaTs));
    pose.R = m3_mul(pose.R, smallRotationFromAccel);
    pose.R = m3_orthonormalize(pose.R);
  }

  pose.t = v3(0.0f, 0.0f, 0.0f);
  validPose = true;
  currentPose = pose;
  return pose;
}


// ---------- Arduino lifecycle ----------
void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);  // SDA=21, SCL=22 (ESP32)

  if (!mpuBegin()) {
    Serial.println("MPU6050 init failed (continuing anyway)");
  } else {
    Serial.println("MPU6050 ready");
  }

  Serial.println("calibrating the MPU6050");
  bool ok = simpleCalibrateRemove1g();
  Serial.println(ok ? "finished calibrating the MPU6050" : "failed to calibrate MPU6050");

  ImuSample raw;
  mpuReadCalibratedSimple(raw, latest);

  currentPose = setPose(latest);

  zeroOrientationToCurrent();
  setupBLEMouse();
  delay(100);
  zeroOrientationToCurrent();
  
}

void loop() {
  // Keep the latest sample fresh
  ImuSample raw;
  mpuReadCalibratedSimple(raw, latest);

  currentPose = setPose(latest);

  if (deviceConnected) {
    Mat3 R_disp = m3_mul(currentPose.R, m3_T(R_ref));
    Vec3 movement = m3_toEuler(R_disp);
    float x_ =  (2 * M_PI-movement.x) / 2 * M_PI;
    float y_ = movement.y / 2 * M_PI;


    uint16_t y = y_ < 0 ? 0 : ((uint16_t)(y_ * 32767) > 32767 ? 32767 : (uint16_t)(y_ * 32767));
    uint16_t x = x_ < 0 ? 0 : ((uint16_t)(x_ * 32767) > 32767 ? 32767 : (uint16_t)(x_ * 32767));

    sendMouseReport(x, y, false, false);  // move pointer to (0,0) with no buttons pressed
    delay(10);
  }
}
