/*
  Project: Inclina
  Author: Tuffy60
  Description:
  This program runs on an ESP32 XIAO.
  It reads height, temperature and tilt, shows the values on an OLED,
  and also serves a small web page over its own WiFi access point.
*/

#include <Adafruit_BMP3XX.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MPU6050.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_sleep.h>
#include <math.h>
#include <string.h>

namespace config {

// Pins
const uint8_t SDA_PIN = D4;
const uint8_t SCL_PIN = D5;
const uint8_t ZERO_BUTTON_PIN = D6;
const uint8_t MODE_BUTTON_PIN = D7;

// Display and sensor addresses
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 32;
const uint8_t OLED_ADDR = 0x3C;
const uint8_t BARO_ADDR = 0x77;
const uint8_t IMU_ADDR = 0x68;

// WiFi access point
const char AP_SSID[] = "XIAO-Baro";
const char AP_PASSWORD[] = "12345678";

// Math and sensor scaling
const float ACCEL_SCALE = 16384.0f;
const float GYRO_SCALE = 131.0f;
const float MAX_ANGLE = 90.0f;
const float AIR_HEIGHT_FACTOR = 29.271f;

// Timing
const unsigned long CALC_MS = 20;
const unsigned long DISPLAY_MS = 200;
const unsigned long BARO_ERROR_MS = 1500;
const unsigned long SERIAL_BOOT_MS = 150;
const unsigned long BOOT_SLEEP_DELAY_MS = 30UL * 1000UL;
const unsigned long AUTO_SLEEP_MS = 10UL * 60UL * 1000UL;
const unsigned long ZERO_REARM_MS = 1800;
const unsigned long ZERO_HOLD_MS = 1200;
const unsigned long HEIGHT_HOLD_MS = 1800;
const unsigned long SENSOR_CHECK_MS = 1000;

// Startup and reference capture
const int START_REF_SAMPLES = 12;
const int START_REF_DELAY_MS = 15;
const int RECONNECT_REF_SAMPLES = 15;
const int RECONNECT_REF_DELAY_MS = 20;
const int MAX_REF_SAMPLES = 64;
const int ZERO_SAMPLES = 45;
const int ZERO_DELAY_MS = 12;

// IMU calibration
const int IMU_WARMUP_SAMPLES = 20;
const int IMU_WARMUP_DELAY_MS = 1;
const int IMU_CAL_SAMPLES = 80;
const int IMU_CAL_DELAY_MS = 2;

// Height filtering
const float HEIGHT_DEADBAND = 0.03f;
const float HEIGHT_SCALE = 1.08f;
const float HEIGHT_SLOW_ALPHA = 0.16f;
const float HEIGHT_FAST_ALPHA = 0.35f;
const float HEIGHT_FAST_DELTA = 0.04f;
const int HEIGHT_MEDIAN_SIZE = 3;
const float NORMAL_BLEND = 0.55f;
const float HEIGHT_LOCK_ENTER = 0.03f;
const float HEIGHT_LOCK_EXIT = 0.08f;
const float ZERO_UNLOCK_HEIGHT = 0.18f;
const float ZERO_REARM_HEIGHT = 0.10f;
const float REF_TRIM_HEIGHT = 0.12f;
const float REF_TRIM_ALPHA = 0.03f;
const float REF_TEMP_ALPHA = 0.02f;

// Motion checks
const float STILL_GYRO_LIMIT = 1.10f;
const float STILL_ACCEL_LIMIT = 0.08f;
const float MOVE_GYRO_LIMIT = 2.0f;
const float MOVE_ACCEL_LIMIT = 0.10f;
const uint8_t SLEEP_MOVE_COUNT = 4;
const uint8_t STILL_COUNT_ENTER = 8;
const uint8_t STILL_COUNT_EXIT = 3;
const uint8_t BARO_FAIL_LIMIT = 8;

// Precision mode
const int PRECISION_PRESSURE_SIZE = 15;
const float PRECISION_ALPHA = 0.12f;
const float PRECISION_HOLD_ALPHA = 0.03f;
const uint8_t PRECISION_STILL_SAMPLES = 10;

// Generic buffer size
const int MEDIAN_SIZE = 3;

// Angle locking
const float ANGLE_LOCK_ENTER = 1.0f;
const float ANGLE_LOCK_EXIT = 2.0f;

}  // namespace config

class BarometerApp {
 public:
  BarometerApp();

  // Start all hardware and app state.
  void setup();

  // Main loop that runs forever.
  void loop();

 private:
  enum ViewMode {
    VIEW_NORMAL = 0,
    VIEW_PRECISION = 1,
    VIEW_STATUS = 2
  };

  struct FilterState {
    float angle = 0.0f;
    float bias = 0.0f;
    float p[2][2] = {{1.0f, 0.0f}, {0.0f, 1.0f}};
  };

  Adafruit_SSD1306 oled;
  Adafruit_BMP3XX baro;
  MPU6050 imu;
  WebServer web;

  float pitch = 0.0f;
  float roll = 0.0f;
  float height = 0.0f;
  float temp = 0.0f;
  float pressure = 0.0f;
  float accelLevel = 0.0f;
  float gyroLevel = 0.0f;
  float preciseHeight = 0.0f;
  float smoothHeight = 0.0f;
  float heldHeight = 0.0f;
  float heldPreciseHeight = 0.0f;
  float gyroOffsetX = 0.0f;
  float gyroOffsetY = 0.0f;
  float gyroOffsetZ = 0.0f;
  float accelOffsetX = 0.0f;
  float accelOffsetY = 0.0f;
  float accelOffsetZ = 0.0f;
  float refPressure = 0.0f;
  float refTemp = 0.0f;
  float pressureBuffer[config::MEDIAN_SIZE] = {};
  float precisePressureBuffer[config::PRECISION_PRESSURE_SIZE] = {};
  float heightBuffer[config::HEIGHT_MEDIAN_SIZE] = {};
  float bubbleX = 0.0f;
  float bubbleY = 0.0f;

  int pressurePos = 0;
  int precisePressurePos = 0;
  int heightPos = 0;

  bool oledReady = false;
  bool baroReady = false;
  bool imuReady = false;
  bool baroHealthy = true;
  bool pressureFilled = false;
  bool precisePressureFilled = false;
  bool heightFilled = false;
  bool zeroPending = false;
  bool pitchLocked = false;
  bool rollLocked = false;
  bool heightLocked = true;
  bool zeroArmed = true;
  bool holdActive = false;
  bool movedAfterZero = false;
  bool still = false;
  bool rawStill = false;
  bool lastZeroButton = HIGH;
  bool lastModeButton = HIGH;

  unsigned long lastBaroErrorMs = 0;
  unsigned long lastSensorCheckMs = 0;
  unsigned long lastCalcMs = 0;
  unsigned long lastDisplayMs = 0;
  unsigned long lastZeroButtonMs = 0;
  unsigned long lastModeButtonMs = 0;
  unsigned long zeroHoldUntilMs = 0;
  unsigned long stillSinceMs = 0;
  unsigned long bootMs = 0;
  unsigned long lastMoveMs = 0;

  uint8_t baroFailCount = 0;
  uint8_t sleepMoveCount = 0;
  uint8_t stillCount = 0;
  uint8_t preciseStillCount = 0;

  ViewMode viewMode = VIEW_NORMAL;
  FilterState pitchFilter;
  FilterState rollFilter;
  String ipText;

  // Core flow
  void initDisplay();
  void runCycle(unsigned long now);
  void checkSensors(unsigned long now);
  bool readBarometer(float& tempNow, float& trimPressure,
                     float& trimHeight, bool& hasTrimSample);
  bool handleZeroReset(unsigned long now, bool baroReadOk);
  void handleModeSwitch(unsigned long now);
  void updateImu(float dt, unsigned long now);
  void updateHeight(bool justZeroed, float tempNow, unsigned long now);

  // Math and filters
  float round1(float value) const;
  float round2(float value) const;
  void initFilter(FilterState* filter);
  float updateFilter(FilterState* filter, float newAngle, float newRate,
                     float dt);
  float calcHeight(float basePressure, float currentPressure,
                   float tempNow) const;
  float smoothValue(float currentValue, float targetValue) const;
  float atan2Deg(float y, float x) const;
  float median(const float* values, int count) const;
  float heightMedian(const float* values, int count) const;
  float average(const float* values, int count) const;
  bool captureReference(int samples, int delayMs, float* pressureOut,
                        float* tempOut);
  void fillPressureBuffer(float value);
  void fillPrecisePressureBuffer(float value);
  void fillHeightBuffer(float value);
  void setReferenceFromCurrent(float currentPressure, float currentTemp);
  void trimReference(float currentPressure, float currentTemp,
                     float currentHeight);
  float getPreciseHeightForView() const;
  bool setReference(int samples, int delayMs);
  void noteActivity();
  bool shouldSleep(unsigned long now) const;
  void goToSleep();

  // Web and display
  String wifiModeText() const;
  String viewModeText() const;
  String errorText() const;
  void handleHome();
  void handleData();
  void handleZero();
  void handleRestart();
  void handleView();
  void setupWeb();
  void startWiFi();
  void drawScreen(unsigned long now);
  void drawPreciseView();
  void drawStatusView();
  void drawNormalView(unsigned long now);
  void drawErrorView();

  // Sensors
  bool startBarometer();
  bool i2cFound(uint8_t address);
  void resetI2c();
  void reconnectSensors();
  bool startImu();
  void calibrateImu();
};

BarometerApp app;

void setup() {
  app.setup();
}

void loop() {
  app.loop();
}
