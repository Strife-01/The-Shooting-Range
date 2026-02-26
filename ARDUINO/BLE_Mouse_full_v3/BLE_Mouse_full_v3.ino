#include <Wire.h>
#include <math.h>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

//  constants
#define MPU6050_ADDR 0x68      // I2C address of MPU6050
#define REG_PWR_MGMT_1 0x6B    // MPU6050 register
#define REG_ACCEL_XOUT_H 0x3B  // Starting register
#define SDA_PIN 21             // I2C SDA pin for ESP32
#define SCL_PIN 22             // I2C SCL pin for ESP32
#define RMB_PIN 26             // Right Mouse Button input pin
#define LMB_PIN 27             // Left Mouse Button input pin

#define IR_ADDR 0x58  // I2C address of IR camera sensor

// BLE configuration
#define DEVICE_NAME "Bluetooth Gun"  // BLE device name

// Screen IR led distances
static const float SCREEN_HEIGHT_WORLD = 60.0f;        // screen height in cm
static const float SCREEN_WIDTH_WORLD = 80.0f;         // screen width in cm
static const float SENSOR_WIDTH_WORLD = 20.0f;         // distance between left and right IR LEDs in the sensorbar
static const float SENSOR_DISTANCE_TO_MIDDLE = 10.0f;  // distance from the sensorbar to the middle of the screen

// MAX DISTANCE FOR THE GAME:
static const float DIST_MIN_CM = 50.0f;   // closest valid distance in cm
static const float DIST_MAX_CM = 1500.0f;  // farthest valid distance in cm


// IMU scale factors for default ranges (+-2g accel, +-250/s gyro)
static const float ACCEL_SF = 16384.0f;                         // LSB per 1g for accelerometer
static const float GYRO_SF = 131.0f;                            // LSB per 1/s for gyro
static const float DEG2RAD = 3.14159265358979323846f / 180.0f;  // deg to radian conversion

// IR
static const float width = 1024.0f;       // IR sensor pixel width
static const float height = 768.0f;       // IR sensor pixel height
static const float IR_FOV_X_DEG = 33.0f;  // use dfrobot ircamera
static const float IR_FOV_Y_DEG = 23.0f;  // use dfrobot ircamera for fov
static const float IR_FOV_X = IR_FOV_X_DEG * DEG2RAD;
static const float IR_FOV_Y = IR_FOV_Y_DEG * DEG2RAD;

//  Data Structures
struct Vec3 {
  float x, y, z;
};
struct Quat {
  float w, x, y, z;
};
struct ImuSample {
  int16_t ax, ay, az;
  int16_t gx, gy, gz;
};
struct SimpleCalib {
  float accel_bias_x = 0.0f;
  float accel_bias_y = 0.0f;
  float accel_bias_z = 0.0f;
  float gyro_bias_x = 0.0f;
  float gyro_bias_y = 0.0f;
  float gyro_bias_z = 0.0f;
  bool valid = false;
};
struct VendorReportData {
  int16_t yaw;              // degrees
  int16_t pitch;            // degrees
  int16_t roll;             // degrees
  int16_t distance;         // cm
  int16_t mpuTempCentiDeg;  // degrees Celsius
  uint8_t ledCount;         // number of visible IR dots
  uint8_t reserved;         // alignment / padding (can use later)
  struct {
    int16_t x;
    int16_t y;
  } leds[4];  // raw IR detections (px, py)
} __attribute__((packed));



//  Global State Variables
SimpleCalib simpleCalib;
ImuSample latest;  // most recent calibrated IMU reading (accelerometer & gyro)

// IR sensor readings
int px[4] = { -1, -1, -1, -1 };     // X positions of up to 4 IR dots
int py[4] = { -1, -1, -1, -1 };     // Y positions of up to 4 IR dots
uint8_t sbyte[4] = { 0, 0, 0, 0 };  // Size/brightness byte of each dot

// BLE HID device
BLEHIDDevice *hidDevice;
// BLEHIDDevice *hidGamepad;
BLECharacteristic *inputReport;   // HID input report
BLECharacteristic *vendorReport;  // our own data
BLECharacteristic *motionReport;

bool deviceConnected = false;

// quaternions
Quat Gquat = { 1, 0, 0, 0 };
bool Gstarted = false;
uint32_t GLastMicros = 0;
float g_yawZero = 0.0f;  // heading offset (for user zeroing)

//  vector/Matrix helper functions
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
inline float wrapPi(float a) {
  while (a > M_PI)
    a -= 2.0f * (float)M_PI;
  while (a <= -M_PI)
    a += 2.0f * (float)M_PI;
  return a;
}
inline Vec3 v_norm(Vec3 a) {
  float L = v_len(a);
  return (L > 1e-8f ? v3(a.x / L, a.y / L, a.z / L) : a);
}
inline Vec3 v_cross(Vec3 a, Vec3 b) {
  return v3(a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x);
}

inline Quat q_make(float w, float x, float y, float z) {
  return Quat{ w, x, y, z };
}
inline Quat q_mul(const Quat &a, const Quat &b) {
  return q_make(
    a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
    a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
    a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w);
}
inline Quat q_normalize(Quat q) {
  float n = sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
  if (n < 1e-9f)
    return q_make(1, 0, 0, 0);
  float s = 1.0f / n;
  return q_make(q.w * s, q.x * s, q.y * s, q.z * s);
}

// Small rotation from body angular velocity (rad/s) over dt
inline Quat q_from_omega_dt(Vec3 w, float dt) {
  float wx = w.x * dt * 0.5f, wy = w.y * dt * 0.5f, wz = w.z * dt * 0.5f;
  float theta2 = wx * wx + wy * wy + wz * wz;
  float s, c;
  if (theta2 < 1e-12f) {
    c = 1.0f;
    s = 1.0f;
  } else {
    float theta = sqrtf(theta2);
    s = sinf(theta) / theta;
    c = cosf(theta);
  }
  return q_make(c, wx * s, wy * s, wz * s);
}
inline Vec3 q_rotate_vec(const Quat &q, Vec3 v) {
  // v-1 = q * (0,v) * q_conj
  Quat p = q_make(0, v.x, v.y, v.z);
  Quat qc = q_make(q.w, -q.x, -q.y, -q.z);
  Quat r = q_mul(q_mul(q, p), qc);
  return v3(r.x, r.y, r.z);
}
inline Vec3 q_to_euler_rpy(const Quat &q) {
  // plane stuff (roll X, pitch Y, yaw Z). Range: roll,yaw -π,π, pitch -π/2, π/2
  float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
  float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
  float roll = atan2f(sinr_cosp, cosr_cosp);

  float sinp = 2.0f * (q.w * q.y - q.z * q.x);
  float pitch = fabsf(sinp) >= 1.0f ? copysignf((float)M_PI / 2.0f, sinp) : asinf(sinp);

  float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
  float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
  float yaw = atan2f(siny_cosp, cosy_cosp);
  return v3(roll, pitch, yaw);
}

inline int16_t clamp_to_I16(float v) {
  if (v > 32767.0f)
    return 32767;
  if (v < -32768.0f)
    return -32768;
  return (int16_t)lrintf(v);
}

//  I2C helpers
// NOTE: no testing, hardware wrapper
void i2cWriteByte(uint8_t addr, uint8_t reg, uint8_t data) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

// NOTE: no testing, hardware wrapper
void i2cReadBytes(uint8_t addr, uint8_t reg, uint8_t count, uint8_t *dest) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission(false);  // restart
  Wire.requestFrom((int)addr, (int)count);
  int i = 0;
  while (Wire.available() && i < count) {
    dest[i++] = Wire.read();
  }
}

// NOTE: no testing, hardware wrapper
bool initIR() {
  Serial.println("Initializing IR camera...");

  // Test if device responds at all
  Wire.beginTransmission(IR_ADDR);
  uint8_t error = Wire.endTransmission();
  if (error != 0) {
    return false;
  }

  // Try initialization sequence
  i2cWriteByte(IR_ADDR, 0x30, 0x01);
  delay(10);
  i2cWriteByte(IR_ADDR, 0x30, 0x08);
  delay(10);
  i2cWriteByte(IR_ADDR, 0x06, 0x90);
  delay(10);
  i2cWriteByte(IR_ADDR, 0x08, 0xC0);
  delay(10);
  i2cWriteByte(IR_ADDR, 0x1A, 0x40);
  delay(10);
  i2cWriteByte(IR_ADDR, 0x33, 0x33);
  delay(100);

  uint8_t test = 0;
  i2cReadBytes(IR_ADDR, 0x30, 1, &test);

  return true;
}

// NOTE: no testing, hardware wrapper
void readIR() {
  uint8_t buf[16] = { 0 };

  Wire.beginTransmission(IR_ADDR);
  Wire.write(0x36);
  Wire.endTransmission();
  Wire.requestFrom((int)IR_ADDR, 16);

  int i = 0;
  while (Wire.available() && i < 16) {
    buf[i++] = Wire.read();
  }

  decodeIRBuffer(buf, px, py, sbyte);
}

// Decode 4 possible points from IR buffer
void decodeIRBuffer(const uint8_t buf[16], int px[4], int py[4], uint8_t sbyte[4]) {
  // Decode 4 possible points
  uint8_t k = 1;
  for (int p = 0; p < 4; ++p) {
    int16_t x = buf[k];
    int16_t y = buf[k + 1];
    uint8_t s = buf[k + 2];
    x += (int16_t)((s & 0x30) << 4);  // lower 2 bits of s carry high bits of x
    y += (int16_t)((s & 0xC0) << 2);  // upper 2 bits of s carry high bits of y
    if (buf[k] == 0xFF && buf[k + 1] == 0xFF) {
      // 0xFF,0xFF indicates no point detected
      x = -1;
      y = -1;
    }
    // we typecast immediately to int so we have a bit more room later without a whole slew of implicit conversions
    px[p] = (int)x;
    py[p] = (int)y;
    sbyte[p] = s;
    k += 3;
  }
}

// NOTE: no testing, hardware wrapper
//  IMU (MPU6050) Functions
bool mpuBegin() {
  // wake the MPU6050 (clear sleep bit)
  i2cWriteByte(MPU6050_ADDR, REG_PWR_MGMT_1, 0x00);
  delay(50);
  // Test by reading one byte from accel register to ensure device present
  uint8_t test;
  i2cReadBytes(MPU6050_ADDR, REG_ACCEL_XOUT_H, 1, &test);

  if (test == 0 || test == 0xFF) {
    return false;
  }
  // of I2C read did not error, assume MPU6050 is present
  return true;
}

// NOTE: no testing, hardware wrapper
void mpuRead(ImuSample &out) {
  int16_t tempIMU = 0;
  uint8_t raw[14];
  i2cReadBytes(MPU6050_ADDR, REG_ACCEL_XOUT_H, 14, raw);
  decodeMPUBuffer(raw, out, tempIMU);
}

// Decode the raw data gathered from the MPU6050
void decodeMPUBuffer(const uint8_t raw[14], ImuSample &out, int16_t &tempOut) {
  int16_t ax = (int16_t)((raw[0] << 8) | raw[1]);
  int16_t ay = (int16_t)((raw[2] << 8) | raw[3]);
  int16_t az = (int16_t)((raw[4] << 8) | raw[5]);
  int16_t gx = (int16_t)((raw[8] << 8) | raw[9]);
  int16_t gy = (int16_t)((raw[10] << 8) | raw[11]);
  int16_t gz = (int16_t)((raw[12] << 8) | raw[13]);

  // Camera frame: +X right, +Y up, +Z forward (lens direction)
  out.ax = -ay;  // MPU +Y left → camera +X right
  out.ay = az;   // MPU +Z up   → camera +Y up
  out.az = -ax;  // MPU -X forward → camera +Z forward

  out.gx = -gy;
  out.gy = gz;
  out.gz = -gx;

  // temperature
  int16_t tRaw = (int16_t)((raw[6] << 8) | raw[7]);
  tempOut = (tRaw / 340.0f) + 36.53f;
}

// Calibrate IMU biases by sampling when device is stationary
// NOTE: no testing, hardware wrapper
bool simpleCalibrateRemove1g(uint16_t sample_count = 1500, uint16_t sample_delay_ms = 2) {
  long double sum_ax = 0, sum_ay = 0, sum_az = 0;
  long double sum_gx = 0, sum_gy = 0, sum_gz = 0;
  ImuSample s;
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

  return computeCalibration(mean_ax, mean_ay, mean_az, mean_gx, mean_gy, mean_gz, simpleCalib);
}

bool computeCalibration(
  float mean_ax, float mean_ay, float mean_az,
  float mean_gx, float mean_gy, float mean_gz,
  SimpleCalib &out) {
  // Compute gravity direction from accel means
  float g_len = sqrtf(mean_ax * mean_ax + mean_ay * mean_ay + mean_az * mean_az);
  if (g_len < 1e-3f)
    return false;
  float dir_x = mean_ax / g_len;
  float dir_y = mean_ay / g_len;
  float dir_z = mean_az / g_len;
  // Ideal accelerometer readings for 1g in that direction
  float ideal_ax = ACCEL_SF * dir_x;
  float ideal_ay = ACCEL_SF * dir_y;
  float ideal_az = ACCEL_SF * dir_z;
  // bias = measured - ideal
  out.accel_bias_x = mean_ax - ideal_ax;
  out.accel_bias_y = mean_ay - ideal_ay;
  out.accel_bias_z = mean_az - ideal_az;
  // gyro biases (assuming still, so mean is bias)
  out.gyro_bias_x = mean_gx;
  out.gyro_bias_y = mean_gy;
  out.gyro_bias_z = mean_gz;
  out.valid = true;
  return true;
}

// NOTE clamp_to_i16 is just used on all axis, no test case needed other then the test case for clamp_toi16
void mpuReadCalibratedSimple(ImuSample &calib_out) {
  ImuSample r;
  mpuRead(r);
  float ax_corr = r.ax - simpleCalib.accel_bias_x;
  float ay_corr = r.ay - simpleCalib.accel_bias_y;
  float az_corr = r.az - simpleCalib.accel_bias_z;
  float gx_corr = r.gx - simpleCalib.gyro_bias_x;
  float gy_corr = r.gy - simpleCalib.gyro_bias_y;
  float gz_corr = r.gz - simpleCalib.gyro_bias_z;
  calib_out.ax = clamp_to_I16(ax_corr);
  calib_out.ay = clamp_to_I16(ay_corr);
  calib_out.az = clamp_to_I16(az_corr);
  calib_out.gx = clamp_to_I16(gx_corr);
  calib_out.gy = clamp_to_I16(gy_corr);
  calib_out.gz = clamp_to_I16(gz_corr);
}

static void initializeIMU(const ImuSample &sample) {
  // Use current 'latest' already calibrated sample is assumeed
  Vec3 a = v3(sample.ax / ACCEL_SF, sample.ay / ACCEL_SF, sample.az / ACCEL_SF);
  if (v_len(a) < 1e-3f) {
    Gquat = q_make(1, 0, 0, 0);
    return;
  }

  Vec3 up_body = v_norm(v3(-a.x, -a.y, -a.z));  // accel points "down", so -a is up
  // Build a body frame with Zb = up_body, Xb from world X projected, Yb = Zb*Xb
  Vec3 Zb = up_body;
  Vec3 Xw = v3(1, 0, 0);
  Vec3 Xb = v_norm(v_sub(Xw, v3(Zb.x * v_dot(Xw, Zb), Zb.y * v_dot(Xw, Zb), Zb.z * v_dot(Xw, Zb))));
  if (v_len(Xb) < 1e-3f)
    Xb = v3(0, 1, 0);
  Vec3 Yb = v_cross(Zb, Xb);

  // Rotation matrix columns are body axes in world frame
  // Convert to quaternion
  float m00 = Xb.x, m01 = Yb.x, m02 = Zb.x;
  float m10 = Xb.y, m11 = Yb.y, m12 = Zb.y;
  float m20 = Xb.z, m21 = Yb.z, m22 = Zb.z;
  float tr = m00 + m11 + m22;
  Quat q;
  if (tr > 0.0f) {
    float S = sqrtf(tr + 1.0f) * 2.0f;
    q.w = 0.25f * S;
    q.x = (m21 - m12) / S;
    q.y = (m02 - m20) / S;
    q.z = (m10 - m01) / S;
  } else if ((m00 > m11) && (m00 > m22)) {
    float S = sqrtf(1.0f + m00 - m11 - m22) * 2.0f;
    q.w = (m21 - m12) / S;
    q.x = 0.25f * S;
    q.y = (m01 + m10) / S;
    q.z = (m02 + m20) / S;
  } else if (m11 > m22) {
    float S = sqrtf(1.0f + m11 - m00 - m22) * 2.0f;
    q.w = (m02 - m20) / S;
    q.x = (m01 + m10) / S;
    q.y = 0.25f * S;
    q.z = (m12 + m21) / S;
  } else {
    float S = sqrtf(1.0f + m22 - m00 - m11) * 2.0f;
    q.w = (m10 - m01) / S;
    q.x = (m02 + m20) / S;
    q.y = (m12 + m21) / S;
    q.z = 0.25f * S;
  }

  Gquat = q_normalize(q);
  Gstarted = true;
  GLastMicros = micros();
}

// Gain Higher = faster correction, but noisy
const float kp = 2.0f;

void imuUpdate() {
  if (!Gstarted)
    initializeIMU(latest);

  uint32_t now = micros();
  float dt = (GLastMicros == 0) ? 0.01f : (now - GLastMicros) * 1e-6f;
  if (dt < 0.0005f)
    dt = 0.0005f;
  if (dt > 0.02f)
    dt = 0.02f;  // 20 ms not 10 ms

  GLastMicros = now;

  // Gyro to radians
  Vec3 w = v3((latest.gx / GYRO_SF) * DEG2RAD,
              (latest.gy / GYRO_SF) * DEG2RAD,
              (latest.gz / GYRO_SF) * DEG2RAD);

  // Integrate gyro
  Quat deltaQ = q_from_omega_dt(w, dt);
  Gquat = q_normalize(q_mul(Gquat, deltaQ));

  // Accelerometer tilt correction (complementary filter)
  Vec3 a = v3(latest.ax / ACCEL_SF, latest.ay / ACCEL_SF, latest.az / ACCEL_SF);
  float amag = v_len(a);

  if (amag > 0.5f && amag < 1.5f) {  // looks like 1 g

    Vec3 a_hat = v_norm(a);       // measured gravity (down) in body
    Vec3 g_world = v3(0, 0, -1);  // gravity in world

    // Expected gravity in body
    Quat q = Gquat;
    Quat qc = q_make(q.w, -q.x, -q.y, -q.z);
    Vec3 g_body_est = q_rotate_vec(qc, g_world);

    // Error to rotate g_body_est a_hat
    Vec3 err = v_cross(g_body_est, a_hat);  // body-frame tilt error
    Vec3 w_corr = v3(err.x * kp, err.y * kp, err.z * kp);
    Quat deltaQ2 = q_from_omega_dt(w_corr, dt);
    Gquat = q_normalize(q_mul(Gquat, deltaQ2));
  }
}

void imuZeroYaw() {
  Vec3 rpy = q_to_euler_rpy(Gquat);
  g_yawZero = rpy.z;
}

const float kYawFuseAlpha = 0.35f;
// since we had some fuckery with the yaw we use the camera to set the yaw to what it should be for the IMU :)
void setYawToVision(float targetYaw) {
  float rawYaw = q_to_euler_rpy(Gquat).z;
  float desiredZero = wrapPi(rawYaw - targetYaw);  // what yawZero should be
  float err = wrapPi(desiredZero - g_yawZero);     // blend toward it
  g_yawZero = wrapPi(g_yawZero + kYawFuseAlpha * err);
}

Vec3 imuGetEulerRPY() {
  Vec3 rpy = q_to_euler_rpy(Gquat);
  rpy.z -= g_yawZero;
  // wrap to (-pi, pi]
  if (rpy.z <= -(float)M_PI)
    rpy.z += 2.0f * (float)M_PI;
  if (rpy.z > (float)M_PI)
    rpy.z -= 2.0f * (float)M_PI;
  return rpy;
}

float imageRoll() {
  // gravity in body/camera frame
  Vec3 g_b = q_rotate_vec(q_make(Gquat.w, -Gquat.x, -Gquat.y, -Gquat.z), v3(0, 0, -1));
  // If image Y increases downward, this gives the in-plane rotation
  return atan2f(g_b.x, -g_b.y);  // change +g_b.y to -g_b.y if Y is up or down, this should change if something is wrong basically
}

// NOTE: no unit test, Hardware interface callbacks
//  BLE HID Setup and Callbacks
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *server) override {
    deviceConnected = true;
    // Enable notifications for input report
    BLE2902 *cccDesc = (BLE2902 *)inputReport->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    if (cccDesc)
      cccDesc->setNotifications(true);
  }
  void onDisconnect(BLEServer *server) override {
    deviceConnected = false;
    BLE2902 *cccDesc = (BLE2902 *)inputReport->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    if (cccDesc)
      cccDesc->setNotifications(false);
    // Restart advertising so host can reconnect
    server->getAdvertising()->start();
  }
};

// NOTE: no unit test, Hardware protocol
// HID report descriptor for absolute pointing mouse with 2 buttons
uint8_t hidReportDescriptor[] = {
  0x05, 0x01,  // USAGE_PAGE (Generic Desktop)
  0x09, 0x02,  // USAGE (Mouse)
  0xA1, 0x01,  // COLLECTION (Application)
  0x85, 0x01,  //   REPORT_ID (1)
  0x09, 0x01,  //   USAGE (Pointer)
  0xA1, 0x00,  //   COLLECTION (Physical)
  // Buttons (2)
  0x05, 0x09,  //     USAGE_PAGE (Button)
  0x19, 0x01,  //     USAGE_MINIMUM (Button 1)
  0x29, 0x02,  //     USAGE_MAXIMUM (Button 2)
  0x15, 0x00,  //     LOGICAL_MINIMUM (0)
  0x25, 0x01,  //     LOGICAL_MAXIMUM (1)
  0x95, 0x02,  //     REPORT_COUNT (2)
  0x75, 0x01,  //     REPORT_SIZE (1)
  0x81, 0x02,  //     INPUT (Data,Var,Abs) - 2 button bits
  // Padding to byte boundary
  0x95, 0x01,  //     REPORT_COUNT (1)
  0x75, 0x06,  //     REPORT_SIZE (6)
  0x81, 0x03,  //     INPUT (Const,Var,Abs) - padding
  // Absolute X and Y coordinates (16-bit each)
  0x05, 0x01,        //     USAGE_PAGE (Generic Desktop)
  0x09, 0x30,        //     USAGE (X)
  0x09, 0x31,        //     USAGE (Y)
  0x15, 0x00,        //     LOGICAL_MINIMUM (0)
  0x26, 0xFF, 0x7F,  //     LOGICAL_MAXIMUM (32767)
  0x75, 0x10,        //     REPORT_SIZE (16)
  0x95, 0x02,        //     REPORT_COUNT (2) - X and Y
  0x81, 0x02,        //     INPUT (Data,Var,Abs) - absolute X, Y coordinates
  0xC0,              //   END_COLLECTION (Physical)
  0xC0,              // END_COLLECTION (Application)
  // // Vendor-defined input report (Report ID 2)
  // 0x06, 0x00, 0xFF,  // USAGE_PAGE (Vendor Defined 0xFF00)
  // 0x09, 0x01,        // USAGE (Vendor Usage 1)
  // 0xA1, 0x01,        // COLLECTION (Application)
  // 0x85, 0x02,        //   REPORT_ID (2)
  // 0x15, 0x00,        //   LOGICAL_MINIMUM (0)
  // 0x26, 0xFF, 0x00,  //   LOGICAL_MAXIMUM (255)
  // 0x75, 0x08,        //   REPORT_SIZE (8 bits)
  // 0x95, 0x20,        //   REPORT_COUNT (32 bytes total)
  // 0x09, 0x01,        //   USAGE (Vendor Usage 1)
  // 0x81, 0x02,        //   INPUT (Data,Var,Abs)
  // 0xC0,              // END_COLLECTION
  // -------------------- GAMEPAD (Report ID 3) --------------------
  0x05, 0x01,        // USAGE_PAGE (Generic Desktop)
  0x09, 0x05,        // USAGE (Game Pad)
  0xA1, 0x01,        // COLLECTION (Application)
  0x85, 0x03,        //   REPORT_ID (3)
  0x09, 0x30,        //   USAGE (X)
  0x09, 0x31,        //   USAGE (Y)
  0x09, 0x32,        //   USAGE (Z)
  0x09, 0x35,        //   USAGE (Rz)
  0x16, 0x00, 0x80,  //   LOGICAL_MINIMUM (-32768)
  0x26, 0xFF, 0x7F,  //   LOGICAL_MAXIMUM (32767)
  0x75, 0x10,        //   REPORT_SIZE (16)
  0x95, 0x04,        //   REPORT_COUNT (4)
  0x81, 0x02,        //   INPUT (Data,Var,Abs)
  0xC0               // END_COLLECTION
};

// NOTE: no unit test, Hardware/lib based protocol
void setupBLEMouse() {
  Serial.println("Starting BLE mouse...");

  BLEDevice::init(DEVICE_NAME);
  BLEServer *bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new MyServerCallbacks());

  hidDevice = new BLEHIDDevice(bleServer);

  inputReport = hidDevice->inputReport(1);  // Mouse
  // vendorReport = hidDevice->inputReport(2);  // Vendor
  motionReport = hidDevice->inputReport(3);  // Gamepad

  hidDevice->manufacturer()->setValue("Espressif");
  hidDevice->pnp(0x02, 0x1234, 0x5678, 0x0100);
  hidDevice->hidInfo(0x00, 0x02);
  hidDevice->reportMap(hidReportDescriptor, sizeof(hidReportDescriptor));
  hidDevice->startServices();

  BLEAdvertising *advertising = bleServer->getAdvertising();
  advertising->addServiceUUID(hidDevice->hidService()->getUUID());
  advertising->setAppearance(HID_MOUSE);
  bleServer->getAdvertising()->setScanResponse(true);
  bleServer->getAdvertising()->setMinPreferred(0x00);  // Remove some OS's issues.

  advertising->start();

  // Security
  // BLESecurity *security = new BLESecurity();
  // security->setAuthenticationMode(ESP_LE_AUTH_BOND);

  hidDevice->setBatteryLevel(100);
}

float distanceToScreen = -1.0f;

// NOTE: no unit test, BLE LIB protocol
// Send an absolute mouse report (X, Y are absolute coords 0-32767)
void sendMouseReport(uint16_t x, uint16_t y, bool leftButton, bool rightButton) {
  uint8_t report[5];
  uint8_t buttons = 0;
  if (leftButton)
    buttons |= 0x01;
  if (rightButton)
    buttons |= 0x02;
  report[0] = buttons;
  report[1] = x & 0xFF;
  report[2] = (x >> 8) & 0xFF;
  report[3] = y & 0xFF;
  report[4] = (y >> 8) & 0xFF;
  inputReport->setValue(report, sizeof(report));
  inputReport->notify();
}

/*
Overview made by chatGPT:
-------------------------------------------------------------------------------
VendorReportData - HID Report ID 2 (32 bytes total, 28 used)
Sent little-endian, packed with no padding.

Byte Offset | Field            | Size | Type    | Notes
------------+------------------+------+---------+-------------------------------
0–1         | yaw              | 2    | int16_t | centi-degrees (×0.01°)
2–3         | pitch            | 2    | int16_t | centi-degrees (×0.01°)
4–5         | roll             | 2    | int16_t | centi-degrees (×0.01°)
6–7         | distance         | 2    | int16_t | centimeters
8–9         | mpuTempCentiDeg  | 2    | int16_t | centi-degrees Celsius
10          | ledCount         | 1    | uint8_t | number of visible IR points
11          | reserved         | 1    | uint8_t | alignment / unused
12–13       | leds[0].x        | 2    | int16_t | raw IR pixel x
14–15       | leds[0].y        | 2    | int16_t | raw IR pixel y
16–17       | leds[1].x        | 2    | int16_t |
18–19       | leds[1].y        | 2    | int16_t |
20–21       | leds[2].x        | 2    | int16_t |
22–23       | leds[2].y        | 2    | int16_t |
24–25       | leds[3].x        | 2    | int16_t |
26–27       | leds[3].y        | 2    | int16_t |
28–31       | (unused)         | 4    | —       | padding to 32 bytes
-------------------------------------------------------------------------------
*/

// NOTE: no unit test, BLE LIB protocol
// we can send out vendor specific report on a different timer then the mousereport since this is less important, we do pack everything in already in the void
void sendVendorReport() {
  if (!deviceConnected)
    return;

  VendorReportData data;

  // Pose (yaw, pitch, roll) in degrees
  Vec3 rpy = imuGetEulerRPY();
  data.yaw = (int16_t)lrintf(rpy.z * 18000.0f / M_PI);
  data.pitch = (int16_t)lrintf(rpy.y * 18000.0f / M_PI);
  data.roll = (int16_t)lrintf(rpy.x * 18000.0f / M_PI);

  // Distance to screen (already computed elsewhere)
  data.distance = (int16_t)lrintf(distanceToScreen);

  // MPU temperature (already read in mpuRead)
  // float tempC = (tempIMU); // already converted to C in mpuRead()
  // data.mpuTempCentiDeg = (int16_t)lrintf(tempC * 100.0f);

  // Count visible LEDs and copy raw positions
  uint8_t count = 0;
  for (int i = 0; i < 4; ++i) {
    if (px[i] >= 0 && py[i] >= 0) {
      count++;
      data.leds[i].x = (int16_t)px[i];
      data.leds[i].y = (int16_t)py[i];
    } else {
      data.leds[i].x = -1;
      data.leds[i].y = -1;
    }
  }
  data.ledCount = count;
  data.reserved = 0;  // not used yet

  // vendorReport->setValue((uint8_t *)&data, sizeof(data));
  // vendorReport->notify();
  // telemetryChar->setValue((uint8_t *)&data, sizeof(data));
  // telemetryChar->notify();
  // --- Send Motion Controller HID report (Report ID 3) ---
  int16_t motionData[4];
  motionData[0] = data.yaw;    // X = yaw
  motionData[1] = data.pitch;  // Y = pitch
  motionData[2] = data.roll;   // Z = roll

  // Convert distanceToScreen (meters) into a joystick axis [-32768..32767]
  float d = distanceToScreen;

  // Clamp physical range
  if (d < DIST_MIN_CM)
    d = DIST_MIN_CM;
  if (d > DIST_MAX_CM)
    d = DIST_MAX_CM;

  // Normalize to [0, 1]
  float norm = (d - DIST_MIN_CM) / (DIST_MAX_CM - DIST_MIN_CM);

  // Map to full signed 16-bit axis range
  int16_t rz = (int16_t)lrintf(norm * 65535.0f - 32768.0f);

  motionData[3] = rz;  // Rz = distance axis

  motionReport->setValue((uint8_t *)motionData, sizeof(motionData));
  motionReport->notify();
}

uint16_t mouseX;
uint16_t mouseY;
// smoothing
float filtX = -1.0f;
float filtY = -1.0f;
float SMOOTH_ALPHA = 0.5f;

void setMouseFiltered(float normX, float normY) {
  normX = fminf(fmaxf(normX, 0.0f), 1.0f);
  normY = fminf(fmaxf(normY, 0.0f), 1.0f);

  // Exponential smoothing
  if (filtX < 0.0f || filtY < 0.0f) {
    filtX = normX;
    filtY = normY;
  } else {
    filtX += SMOOTH_ALPHA * (normX - filtX);
    filtY += SMOOTH_ALPHA * (normY - filtY);
  }

  mouseX = (uint16_t)lrintf(fminf(fmaxf(filtX * 32767.0f, 0.0f), 32767.0f));
  mouseY = (uint16_t)lrintf(fminf(fmaxf(filtY * 32767.0f, 0.0f), 32767.0f));
}

// Unrotate IR points around image center based on roll from accelerometer
void unrotateIRPoints(const int px[4], const int py[4], int pxnew[4], int pynew[4]) {
  float roll = imageRoll();
  float c = cosf(roll);
  float s = sinf(roll);

  float cx = 0.5f * (float)width;
  float cy = 0.5f * (float)height;

  for (int i = 0; i < 4; i++) {
    if (px[i] < 0 || py[i] < 0) {
      pxnew[i] = -1;
      pynew[i] = -1;
      continue;
    }

    float dx = (float)px[i] - cx;
    float dy = (float)py[i] - cy;

    float ux = c * dx - s * dy;
    float uy = s * dx + c * dy;

    float xf = cx + ux;
    float yf = cy + uy;

    pxnew[i] = (int)lrintf(xf);
    pynew[i] = (int)lrintf(yf);
  }
}

inline void irPixelToAngles(float x_pix, float y_pix, float &ax, float &ay) {
  // normalize to [-1, +1] with (0,0) at center; flip Y so +ay = up
  float u = (x_pix / width) * 2.0f - 1.0f;
  float v = 1.0f - (y_pix / height) * 2.0f;

  // half-FOV tangents
  const float tx = tanf(0.5f * IR_FOV_X);
  const float ty = tanf(0.5f * IR_FOV_Y);

  // mapping: angle = atan( normalized * tan(FOV/2) )
  ax = atanf(u * tx);
  ay = atanf(v * ty);
}


void selftest() {
  Serial.println("Starting selftest...");
  {
    Vec3 a = v3(2, 3, 5);
    if (a.x != 2 || a.y != 3 || a.z != 5) {
      Serial.printf("FAILED: function v3,\n Expected output: 2,3,5 \n Actual output: %.1f,%.1f,%.1f\n", a.x, a.y, a.z);
    } else {
      Serial.println("v3 function passed.");
    }
  }

  // test v_add
  {
    Vec3 a = v3(2, 3, 5);
    Vec3 b = v3(1, 1, 1);
    Vec3 c = v_add(a, b);
    if (c.x != 3 || c.y != 4 || c.z != 6) {
      Serial.printf("FAILED: function v_add,\n Expected output: 3,4,6 \n Actual output: %.1f,%.1f,%.1f\n", c.x, c.y, c.z);
    } else {
      Serial.println("v_add function passed.");
    }
  }

  // test v_sub
  {
    Vec3 a = v3(2, 3, 5);
    Vec3 b = v3(1, 1, 1);
    Vec3 d = v_sub(a, b);
    if (d.x != 1 || d.y != 2 || d.z != 4) {
      Serial.printf("FAILED: function v_sub,\n Expected output: 1,2,4 \n Actual output: %.1f,%.1f,%.1f\n", d.x, d.y, d.z);
    } else {
      Serial.println("v_sub function passed.");
    }
  }

  // test v_dot
  {
    Vec3 a = v3(3, 5, 6);
    Vec3 b = v3(2, 1, 4);
    float c = v_dot(a, b);
    if (fabs(c - 35.0f) > 1e-6f) {
      Serial.printf("FAILED: function v_dot,\n Expected output: 35 \n Actual output: %.2f\n", c);
    } else {
      Serial.println("v_dot function passed.");
    }
  }

  // test v_len
  {
    Vec3 a = v3(3, 5, 6);
    float c = v_len(a);
    float expected = sqrtf(3 * 3 + 5 * 5 + 6 * 6);
    if (fabs(c - expected) > 1e-6f) {
      Serial.printf("FAILED: function v_len,\n Expected: %.3f \n Actual: %.3f\n", expected, c);
    } else {
      Serial.println("v_len function passed.");
    }
  }

  // test v_norm
  {
    Vec3 a = v3(3, 5, 6);
    Vec3 n = v_norm(a);
    float len = v_len(n);
    if (fabs(len - 1.0f) > 1e-6f) {
      Serial.printf("FAILED: function v_norm,\n Expected: 1 \n Actual: %.6f\n", len);
    } else {
      Serial.println("v_norm function passed.");
    }
  }

  // test v_cross
  {
    Vec3 a = v3(1, 0, 0);
    Vec3 b = v3(0, 1, 0);
    Vec3 c = v_cross(a, b);
    if (c.x != 0 || c.y != 0 || c.z != 1) {
      Serial.printf("FAILED: function v_cross,\n Expected: (0,0,1)\n Actual: (%.1f,%.1f,%.1f)\n", c.x, c.y, c.z);
    } else {
      Serial.println("v_cross function passed.");
    }
  }
  // test v_cross

  Serial.println("moving on to quaternion and rotation math...");

  {
    // test q_make
    Quat q = q_make(1, 2, 3, 4);
    if (q.w != 1 || q.x != 2 || q.y != 3 || q.z != 4) {
      Serial.printf("FAILED: q_make -> expected (1,2,3,4) got (%.1f,%.1f,%.1f,%.1f)\n", q.w, q.x, q.y, q.z);
    } else {
      Serial.println("q_make function passed.");
    }
  }

  {
    // test q_mul
    // Quaternion multiplication: identity * q = q
    Quat q1 = q_make(1, 0, 0, 0);
    Quat q2 = q_make(0, 1, 2, 3);
    Quat r = q_mul(q1, q2);
    if (fabs(r.w - 0) > 1e-6 || fabs(r.x - 1) > 1e-6 || fabs(r.y - 2) > 1e-6 || fabs(r.z - 3) > 1e-6) {
      Serial.printf("FAILED: q_mul -> expected (0,1,2,3) got (%.3f,%.3f,%.3f,%.3f)\n", r.w, r.x, r.y, r.z);
    } else {
      Serial.println("q_mul function passed.");
    }
  }

  {
    // test q_normalize
    Quat q = q_make(0, 3, 0, 4);
    Quat n = q_normalize(q);
    float len = sqrtf(n.w * n.w + n.x * n.x + n.y * n.y + n.z * n.z);
    if (fabs(len - 1.0f) > 1e-3f) {
      Serial.printf("FAILED: q_normalize -> expected length 1, got %.3f\n", len);
    } else {
      Serial.println("q_normalize function passed.");
    }
  }

  {
    // test q_from_omega_dt
    Vec3 w = v3(0, 0, M_PI / 2.0f);  // 90 deg/s rotation around Z
    float dt = 1.0f;                 // 1 second
    Quat q = q_from_omega_dt(w, dt);
    // Expect rotation roughly 90, so w = cos(45)=0.707, z=sin(45)=0.707
    if (fabs(q.w - 0.707f) > 0.05f || fabs(q.z - 0.707f) > 0.05f) {
      Serial.printf("FAILED: q_from_omega_dt -> expected 0.707,0.707 got (%.3f,%.3f)\n", q.w, q.z);
    } else {
      Serial.println("q_from_omega_dt function passed.");
    }
  }

  {
    // test q_rotate_vec
    // 90 rotation about Z should turn (1,0,0) into (0,1,0)
    float s = sinf(M_PI / 4.0f);
    float c = cosf(M_PI / 4.0f);
    Quat q = q_make(c, 0, 0, s);
    Vec3 v = v3(1, 0, 0);
    Vec3 r = q_rotate_vec(q, v);
    if (fabs(r.x) > 0.1f || fabs(r.y - 1.0f) > 0.1f) {
      Serial.printf("FAILED: q_rotate_vec -> expected (0,1,0) got (%.2f,%.2f,%.2f)\n", r.x, r.y, r.z);
    } else {
      Serial.println("q_rotate_vec function passed.");
    }
  }

  {
    // test q_to_euler_rpy
    // Quaternion for 90 rotation about X should be roll=90, pitch=0, yaw=0
    float s = sinf(M_PI / 4.0f);
    float c = cosf(M_PI / 4.0f);
    Quat q = q_make(c, s, 0, 0);
    Vec3 rpy = q_to_euler_rpy(q);
    float rollDeg = rpy.x * 180.0f / M_PI;
    if (fabs(rollDeg - 90.0f) > 2.0f) {
      Serial.printf("FAILED: q_to_euler_rpy -> expected roll=90, got %.2f\n", rollDeg);
    } else {
      Serial.println("q_to_euler_rpy function passed.");
    }
  }

  {
    // test clamp_to_I16
    bool ok = true;
    if (clamp_to_I16(40000.0f) != 32767)
      ok = false;
    if (clamp_to_I16(-40000.0f) != -32768)
      ok = false;
    if (clamp_to_I16(123.4f) != 123)
      ok = false;
    if (ok)
      Serial.println("clamp_to_I16 function passed.");
    else
      Serial.println("FAILED: clamp_to_I16 produced wrong results.");
  }

  Serial.println("Quaternion math selftests complete.");
  Serial.println("Moving on to IO data conversion and pointer math tests.");
  {
    Serial.println("Testing decodeIRBuffer...");

    // --- Prepare fake input buffer ---
    uint8_t buf[16] = {
      0,                 // byte 0 (unused)
      10, 20, 0x00,      // point 0 (normal)
      30, 40, 0x00,      // point 1 (normal)
      0xFF, 0xFF, 0x00,  // point 2 (invalid, should become -1,-1)
      50, 60, 0xB0,      // point 3 (has high bits set)
      0, 0, 0            // padding (not used)
    };

    int px[4], py[4];
    uint8_t sbyte[4];

    decodeIRBuffer(buf, px, py, sbyte);

    // Check results
    bool ok = true;

    // Point 0
    if (px[0] != 10 || py[0] != 20) {
      Serial.printf("FAILED: Point 0 failed: got (%d,%d)\n", px[0], py[0]);
      ok = false;
    }

    // Point 1
    if (px[1] != 30 || py[1] != 40) {
      Serial.printf("FAILED: Point 1 failed: got (%d,%d)\n", px[1], py[1]);
      ok = false;
    }

    // Point 2 (invalid marker)
    if (px[2] != -1 || py[2] != -1) {
      Serial.printf("FAILED: Point 2 failed: expected -1,-1 got (%d,%d)\n", px[2], py[2]);
      ok = false;
    }

    // Point 3 (has high bits)
    int expectedX3 = 50 + ((0xB0 & 0x30) << 4);
    int expectedY3 = 60 + ((0xB0 & 0xC0) << 2);
    if (px[3] != expectedX3 || py[3] != expectedY3) {
      Serial.printf("FAILED: Point 3 failed: expected (%d,%d) got (%d,%d)\n",
                    expectedX3, expectedY3, px[3], py[3]);
      ok = false;
    }

    if (ok)
      Serial.println("decodeIRBuffer test passed.");
    else
      Serial.println("FAILED: decodeIRBuffer test failed.");
  }

  Serial.println("Test decodeMPUBuffer...");
  {
    Serial.println("Testing decodeMPUBuffer...");

    // Create fake MPU data bytes
    uint8_t raw[14] = {
      0x00, 0x10,  // ax = 16
      0x00, 0x20,  // ay = 32
      0x00, 0x30,  // az = 48
      0x00, 0x40,  // temp raw (tRaw = 0x0040 = 64)
      0x00, 0x50,  // gx = 80
      0x00, 0x60,  // gy = 96
      0x00, 0x70   // gz = 112
    };

    ImuSample out;
    int16_t tempOut;

    decodeMPUBuffer(raw, out, tempOut);
    bool ok = true;

    // Expected transformations:
    // out.ax = -ay = -32
    // out.ay = az  = 48
    // out.az = -ax = -16
    if (out.ax != -32 || out.ay != 48 || out.az != -16) {
      Serial.printf("FAILED: Accel failed: (%d,%d,%d)\n", out.ax, out.ay, out.az);
      ok = false;
    }

    // out.gx = -gy = -96
    // out.gy = gz  = 112
    // out.gz = -gx = -80
    if (out.gx != -96 || out.gy != 112 || out.gz != -80) {
      Serial.printf("FAILED: Gyro failed: (%d,%d,%d)\n", out.gx, out.gy, out.gz);
      ok = false;
    }

    // Temperature check
    float expectedTemp = 36.0f;
    if (fabs(tempOut - expectedTemp) > 0.5f) {
      Serial.printf("FAILED: Temp %.2f expected near %.2f\n", tempOut, expectedTemp);
      ok = false;
    }

    if (ok)
      Serial.println("DecodeMPUBuffer test passed.");
    else
      Serial.println("FAILED: DecodeMPUBuffer test failed.");
  }

  {
    Serial.println("Testing computeCalibration");
    SimpleCalib result;
    bool ok = true;

    // Simulated "mean" readings:
    // Device is stationary, gravity vector = 1g downward along +Z (az)
    float mean_ax = 0.0f;
    float mean_ay = 0.0f;
    float mean_az = ACCEL_SF;  // perfect 1g reading
    float mean_gx = 0.0f;
    float mean_gy = 0.0f;
    float mean_gz = 0.0f;

    bool valid = computeCalibration(mean_ax, mean_ay, mean_az, mean_gx, mean_gy, mean_gz, result);

    if (!valid) {
      Serial.println("FAILED: computeCalibration failed unexpectedly");
      return;
    }

    // Expected: accel bias should be near zero since mean == ideal
    if (fabs(result.accel_bias_x) > 0.001 || fabs(result.accel_bias_y) > 0.001 || fabs(result.accel_bias_z) > 0.001) {
      Serial.printf("FAILED: Accel bias not near zero: (%.3f, %.3f, %.3f)\n",
                    result.accel_bias_x, result.accel_bias_y, result.accel_bias_z);
      ok = false;
    }

    // Expected: gyro bias = mean gyro (zero)
    if (result.gyro_bias_x != 0 || result.gyro_bias_y != 0 || result.gyro_bias_z != 0) {
      Serial.println("FAILED: Gyro bias not zero");
      ok = false;
    }

    if (ok)
      Serial.println("computeCalibration test passed.");
    else
      Serial.println("FAILED: computeCalibration test failed.");
  }

  {
    Serial.println("Running test: initializeIMU...");
    ImuSample sample;
    sample.ax = 0;
    sample.ay = 0;
    sample.az = ACCEL_SF;  // +1g upward (flat orientation)
    sample.gx = sample.gy = sample.gz = 0;

    initializeIMU(sample);
    bool pass = true;

    // quaternion near identity
    if (fabs(Gquat.w) > 0.01f)
      pass = false;
    if (fabs(Gquat.x - 1.0f) > 0.01f)
      pass = false;
    if (fabs(Gquat.y) > 0.01f)
      pass = false;
    if (fabs(Gquat.z) > 0.01f)
      pass = false;

    if (!Gstarted)
      pass = false;
    if (GLastMicros == 0)
      pass = false;

    if (pass)
      Serial.println("initializeIMU correctly sets orientation for flat sample");
    else
      Serial.println("FAIL: initializeIMU quaternion incorrect or state flags wrong");

    // Optional: print quaternion values for inspection
    Serial.printf("Result: q=(%.3f, %.3f, %.3f, %.3f)\n", Gquat.w, Gquat.x, Gquat.y, Gquat.z);
    Gstarted = false;
    GLastMicros = 0;
  }

  {
    // Backup global state
    bool startedBackup = Gstarted;
    uint32_t microsBackup = GLastMicros;
    Quat quatBackup = Gquat;
    ImuSample latestBackup = latest;

    bool ok = true;

    // Stationary device (flat)
    Serial.println("Test 1: imuUpdate stationary...");
    Gstarted = false;
    GLastMicros = 0;
    Gquat = q_make(1, 0, 0, 0);
    latest.ax = 0;
    latest.ay = 0;
    latest.az = ACCEL_SF;  // +1g up
    latest.gx = latest.gy = latest.gz = 0;

    imuUpdate();

    // Expect: Gstarted = true, quaternion  identity, and time updated
    if (!Gstarted) {
      Serial.println("FAILED: imuUpdate did not set Gstarted = true");
      ok = false;
    }
    // Expect quaternion ≈ (0, 1, 0, 0)
    if (fabs(Gquat.w) > 0.02f || fabs(Gquat.x - 1.0f) > 0.02f || fabs(Gquat.y) > 0.02f || fabs(Gquat.z) > 0.02f) {
      Serial.printf("FAILED: imuUpdate stationary quaternion drifted: q=(%.3f,%.3f,%.3f,%.3f)\n",
                    Gquat.w, Gquat.x, Gquat.y, Gquat.z);
      ok = false;
    }

    if (GLastMicros == 0) {
      Serial.println("FAILED: imuUpdate did not update GLastMicros");
      ok = false;
    }
    if (ok)
      Serial.println("imuUpdate stationary test passed.");
    else
      Serial.println("imuUpdate stationary test failed.");

    // Second tes Constant rotation about Z
    Serial.println("Test 2: imuUpdate constant rotation about Z...");
    Gquat = q_make(1, 0, 0, 0);
    GLastMicros = micros();  // simulate valid previous time
    delay(10);
    latest.ax = 0;
    latest.ay = 0;
    latest.az = ACCEL_SF;  // still 1g
    latest.gx = 0;
    latest.gy = 0;
    latest.gz = GYRO_SF * 90.0f / DEG2RAD;  // simulate 90 degs around Z axis

    imuUpdate();

    Vec3 euler = q_to_euler_rpy(Gquat);
    float yawDeg = euler.z * 180.0f / M_PI;

    // Expect a small positive yaw (a few degrees)
    if (yawDeg < 1.0f || yawDeg > 10.0f) {
      Serial.printf("FAILED: imuUpdate rotation test: expected 5 degrees, got %.2f\n", yawDeg);
      ok = false;
    } else {
      Serial.printf("imuUpdate rotation test passed: yaw=%.2f\n", yawDeg);
    }

    // Restore globals
    Gstarted = startedBackup;
    GLastMicros = microsBackup;
    Gquat = quatBackup;
    latest = latestBackup;

    if (ok)
      Serial.println("imuUpdate selftest passed.");
    else
      Serial.println("FAILED: imuUpdate selftest failed.");
  }

  {
    Serial.println("Running test: imu orientation helpers (imuZeroYaw, setYawToVision, imuGetEulerRPY, imageRoll)...");

    // backup global state
    Quat gquatBackup = Gquat;
    float yawZeroBackup = g_yawZero;

    bool ok = true;

    // Set Gquat to a known rotation: roll = 10, pitch = 20, yaw = 30
    float roll = radians(10.0f);
    float pitch = radians(20.0f);
    float yaw = radians(30.0f);

    float cr = cosf(roll * 0.5f);
    float sr = sinf(roll * 0.5f);
    float cp = cosf(pitch * 0.5f);
    float sp = sinf(pitch * 0.5f);
    float cy = cosf(yaw * 0.5f);
    float sy = sinf(yaw * 0.5f);

    Gquat.w = cr * cp * cy + sr * sp * sy;
    Gquat.x = sr * cp * cy - cr * sp * sy;
    Gquat.y = cr * sp * cy + sr * cp * sy;
    Gquat.z = cr * cp * sy - sr * sp * cy;

    {
      Serial.println("Test imuZeroYaw...");
      imuZeroYaw();
      Vec3 rpyBefore = q_to_euler_rpy(Gquat);
      if (fabs(g_yawZero - rpyBefore.z) > 1e-3f) {
        Serial.printf("FAILED: imuZeroYaw -> expected %.3f, got %.3f\n", rpyBefore.z, g_yawZero);
        ok = false;
      } else {
        Serial.println("imuZeroYaw passed.");
      }
    }

    {
      Serial.println("Test setYawToVision...");
      // Suppose camera detects yaw = 20, IMU currently thinks 30
      float visionYaw = radians(20.0f);
      float prevZero = g_yawZero;
      setYawToVision(visionYaw);
      float newZero = g_yawZero;

      // Expect yawZero to move toward correct offset less difference
      if (fabs(newZero - visionYaw) > fabs(prevZero - visionYaw)) {
        Serial.printf("FAILED: setYawToVision -> yawZero moved wrong direction: before %.3f, after %.3f\n", prevZero, newZero);
        ok = false;
      } else {
        Serial.println("setYawToVision passed.");
      }
    }

    {
      Serial.println("Test imuGetEulerRPY...");
      Vec3 correctedRPY = imuGetEulerRPY();

      // Yaw should now be near zero because imuZeroYaw + setYawToVision compensate it
      if (fabs(correctedRPY.z) > radians(10.0f)) {
        Serial.printf("FAILED: imuGetEulerRPY -> expected yaw near 0, got %.2f\n", degrees(correctedRPY.z));
        ok = false;
      } else {
        Serial.println("imuGetEulerRPY passed.");
      }
    }

    {
      Serial.println("Test 4: imageRoll...");
      float rollOut = imageRoll();
      float expectedRoll = roll;  // roughly matches roll angle
      if (fabs(rollOut - expectedRoll) > radians(5.0f)) {
        Serial.printf("FAILED: imageRoll -> expected %.2f°, got %.2f°\n",
                      degrees(expectedRoll), degrees(rollOut));
        ok = false;
      } else {
        Serial.println("imageRoll passed.");
      }
    }

    // Let us put it back
    Gquat = gquatBackup;
    g_yawZero = yawZeroBackup;

    if (ok)
      Serial.println("imu orientation helper tests passed.");
    else
      Serial.println("FAILED: SOME imu orientation helper tests failed.");
  }

  {
    // unrotateIRPoints
    Serial.println("Testing unrotateIRPoints...");

    // backup original quaternion
    Quat backupQ = Gquat;

    // Force a known roll, PI/2 -> 90 degrees
    float roll = M_PI / 2.0f;
    float c = cosf(roll / 2.0f);
    float s = sinf(roll / 2.0f);
    Gquat = q_make(c, s, 0, 0);  // roll-only rotation

    // fake IR points
    int pxIn[4] = { 512, 600, -1, -1 };
    int pyIn[4] = { 384, 384, -1, -1 };
    int pxOut[4], pyOut[4];

    unrotateIRPoints(pxIn, pyIn, pxOut, pyOut);

    bool ok = true;
    // behavior: (600,384) -> (600,384) unchanged (or 512,296) depending on sign
    int expX = 600;
    int expY = 384;


    if (abs(pxOut[1] - expX) > 5 || abs(pyOut[1] - expY) > 5) {
      Serial.printf("FAILED: unrotateIRPoints -> expected (%d,%d), got (%d,%d)\n",
                    expX, expY, pxOut[1], pyOut[1]);
      ok = false;
    }

    if (pxOut[2] != -1 || pyOut[2] != -1) {
      Serial.println("FAILED: unrotateIRPoints, input not the same as the output");
      ok = false;
    }

    if (ok)
      Serial.println("unrotateIRPoints function passed.");
    else
      Serial.println("FAILED: unrotateIRPoints function failed.");

    // back to how it was
    Gquat = backupQ;
  }

  {
    // irPixelToAngles
    Serial.println("Testing irPixelToAngles...");

    bool ok = true;
    float ax, ay;

    // Center pixel should be 0,0
    irPixelToAngles(width / 2.0f, height / 2.0f, ax, ay);
    if (fabs(ax) > 1e-3 || fabs(ay) > 1e-3) {
      Serial.printf("FAILED: center pixel -> expected 0,0 got %.4f,%.4f\n", ax, ay);
      ok = false;
    }

    // Right edge pixel should be roughly +FOV/2 in X
    irPixelToAngles(width - 1.0f, height / 2.0f, ax, ay);
    float expectedX = IR_FOV_X / 2.0f;
    if (fabs(ax - expectedX) > 0.05f) {
      Serial.printf("FAILED: right edge -> expected %.3f rad, got %.3f\n", expectedX, ax);
      ok = false;
    }

    // Top edge pixel should be roughly +FOV/2 in Y
    irPixelToAngles(width / 2.0f, 0.0f, ax, ay);
    float expectedY = IR_FOV_Y / 2.0f;
    if (fabs(ay - expectedY) > 0.05f) {
      Serial.printf("FAILED: top edge -> expected %.3f rad, got %.3f\n", expectedY, ay);
      ok = false;
    }

    if (ok)
      Serial.println("irPixelToAngles function passed.");
    else
      Serial.println("FAILED: irPixelToAngles function failed.");
  }

  Serial.println("ALL Selftests complete.");
}

int leftId = -1, rightId = -1;
int lastLeftOrRight = 0;  // 0=none, 1=left, 2=right
int lastLeftX, lastRightX;

//  Pointer Calculation and Fusion
bool updatePointer() {
  // Determine how many IR points are currently visible
  int visibleCount = 0;
  int idxList[4];
  for (int i = 0; i < 4; ++i) {
    if (px[i] >= 0 && py[i] >= 0) {
      idxList[visibleCount++] = i;
    }
  }

  if (visibleCount == 0) {
    // No points visible: do not move cursor (hold last position)
    return false;
  } else if (visibleCount == 2) {
    // unroll points
    int pxu[4], pyu[4];
    unrotateIRPoints(px, py, pxu, pyu);

    // if either point is invalid after unrotation stop
    if (pxu[idxList[0]] < 0 || pxu[idxList[1]] < 0 || pyu[idxList[0]] < 0 || pyu[idxList[1]] < 0)
      return false;

    // find left and right point ID's in unrotated frame

    if (pxu[idxList[0]] < pxu[idxList[1]]) {
      leftId = idxList[0];
      rightId = idxList[1];
    } else {
      leftId = idxList[1];
      rightId = idxList[0];
    }

    // Store last seen positions for identification
    lastLeftX = pxu[leftId];
    lastRightX = pxu[rightId];

    // convert it to angles using the intrinsics of the camera
    float angleXLeft, angleYLeft;
    float angleXRight, angleYRight;
    irPixelToAngles(pxu[leftId], pyu[leftId], angleXLeft, angleYLeft);
    irPixelToAngles(pxu[rightId], pyu[rightId], angleXRight, angleYRight);

    // delta angles in unrotated frame
    float deltaAngleX = fabsf(angleXRight - angleXLeft);

    // hey look, if the points are too close together, it goes boom, we dont like that you know!
    if (deltaAngleX < 0.002f) {
      return false;
    }

    // find the middle of the bar
    float midAngleX = 0.5f * (angleXLeft + angleXRight);
    float midAngleY = 0.5f * (angleYLeft + angleYRight);

    // set yaw for the IMU
    setYawToVision(midAngleX);

    // distance to the screen
    float d_meters = fabs((SENSOR_WIDTH_WORLD / 100.0f) / (2.0f * tanf(deltaAngleX / 2.0f)));
    distanceToScreen = d_meters * 100.0f; // convert to centimeters

    // now we find the the bar in world space
    float barX = distanceToScreen * tanf(midAngleX);
    float barY = distanceToScreen * tanf(midAngleY);

    // find the screen center
    float screenX = barX;  // middle of the screen assumption
    float screenY = barY + SENSOR_DISTANCE_TO_MIDDLE;

    // project back to pixel space
    float nx = 0.5f + (screenX / SCREEN_WIDTH_WORLD);   // right increases nx
    float ny = 0.5f - (screenY / SCREEN_HEIGHT_WORLD);  // up decreases ny

    // flip y because 0,0 is topleft
    ny = 1.0f - ny;

    setMouseFiltered(nx, ny);

    // update state
    lastLeftOrRight = 0;
    return true;
  } else if (visibleCount == 1) {
    // only one point visible, use last known distance to screen if we have it
    if (distanceToScreen <= 0.0f)
      return false;

    int pxu[4], pyu[4];
    unrotateIRPoints(px, py, pxu, pyu);
    if (lastLeftOrRight == 0) {
      if (fabs(pxu[idxList[0]] - lastLeftX) < fabs(pxu[idxList[0]] - lastRightX)) {
        lastLeftOrRight = 1;
      } else {
        lastLeftOrRight = 2;
      }
      return false;  // skip this frame to avoid jump
    }

    if (pxu[idxList[0]] < 0 || pyu[idxList[0]] < 0)
      return false;

    // convert it to angles using the intrinsics of the camera
    float angleX, angleY;
    irPixelToAngles(pxu[idxList[0]], pyu[idxList[0]], angleX, angleY);

    // first we have to compensate for the offset of the point on the bar
    float halfBaselineAngle = atanf((0.5f * SENSOR_WIDTH_WORLD) / distanceToScreen);
    float midAngleX, midAngleY;
    if (lastLeftOrRight == 1) {
      // left point
      midAngleX = angleX + halfBaselineAngle;
    } else if (lastLeftOrRight == 2) {
      // right point
      midAngleX = angleX - halfBaselineAngle;
    } else {
      return false;  // safety
    }

    midAngleY = angleY;

    // now we find the bar in world space
    float barX = distanceToScreen * tanf(midAngleX);
    float barY = distanceToScreen * tanf(midAngleY);

    // screen center from reconstructed bar mid
    float screenX = barX;
    float screenY = barY + SENSOR_DISTANCE_TO_MIDDLE;

    // project back to pixel space
    float nx = 0.5f + (screenX / SCREEN_WIDTH_WORLD);   // right increases nx
    float ny = 0.5f - (screenY / SCREEN_HEIGHT_WORLD);  // up decreases ny

    // flip y because 0,0 is topleft
    ny = 1.0f - ny;

    setMouseFiltered(nx, ny);
    return true;
  } else if (visibleCount == 0) {
    // use gyro
    return false;
  } else {
    return false;  // too many points, ignore
  }

  return false;  // should not get here
}

void setup() {
  Serial.begin(115200);
  delay(100);
  // Initialize I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  // Initialize IMU (MPU6050)
  if (!mpuBegin()) {
    Serial.println("MPU6050 initialization failed!");
  } else {
    Serial.println("MPU6050 ready");
  }
  Serial.println("Calibrating set the thing on a flat surface");
  bool calibOk = simpleCalibrateRemove1g();
  Serial.println(calibOk ? "IMU calibrated" : "IMU calibration failed");

  mpuReadCalibratedSimple(latest);
  initializeIMU();
  imuZeroYaw();

  // Initialize IR camera
  if (initIR()) {
    Serial.println("IR camera initialized");
  } else {
    Serial.println("Unable to get a response from IR camera module");
  }

  // Initialize BLE HID mouse
  setupBLEMouse();
  delay(100);

  pinMode(LMB_PIN, INPUT_PULLDOWN);
  pinMode(RMB_PIN, INPUT_PULLDOWN);
}

unsigned long lastUpdate = 0;
static unsigned long lastVendorSend = 0;
const unsigned long updateIntervalConnected = 10;

void loop() {
  unsigned long now = millis();

  // Always read sensors
  mpuReadCalibratedSimple(latest);
  imuUpdate();
  readIR();
  bool ok = updatePointer();




  // BLE connected
  if (deviceConnected) {
    if (now - lastUpdate >= updateIntervalConnected) {
      lastUpdate = now;
      if (ok) {
        // Read buttons (active low)
        bool leftButton = (digitalRead(LMB_PIN) == HIGH);
        bool rightButton = (digitalRead(RMB_PIN) == HIGH);
        if (rightButton || leftButton) {
          Serial.printf("LMB=%d RMB=%d\n", leftButton, rightButton);
        }

        sendMouseReport(mouseX, mouseY, leftButton, rightButton);
      }
    }
  }

  // we also send a vendor report about what the gun is seeing / doing right now
  if (deviceConnected && now - lastVendorSend > 50) {  // 20 Hz
    lastVendorSend = now;
    sendVendorReport();
  }
}
