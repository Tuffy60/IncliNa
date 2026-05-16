/*
  File: BarometerAppSensors.ino
  Purpose:
  This file handles sensor startup, reconnect logic and IMU calibration.
*/

// Start the BMP388 and apply its settings.
bool BarometerApp::startBarometer() {
  if (!baro.begin_I2C(config::BARO_ADDR)) {
    return false;
  }

  baro.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  baro.setPressureOversampling(BMP3_OVERSAMPLING_16X);
  baro.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_7);
  baro.setOutputDataRate(BMP3_ODR_50_HZ);
  return true;
}

// Check whether an I2C device answers on the bus.
bool BarometerApp::i2cFound(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

// Recreate the I2C bus.
void BarometerApp::resetI2c() {
  Wire.begin(config::SDA_PIN, config::SCL_PIN);
  Wire.setClock(400000);
}

// Try to bring missing sensors back online.
void BarometerApp::reconnectSensors() {
  bool reconnected = false;

  if (!baroReady && i2cFound(config::BARO_ADDR) && startBarometer()) {
    baroReady = true;
    baroHealthy = true;
    baroFailCount = 0;
    setReference(config::RECONNECT_REF_SAMPLES,
                 config::RECONNECT_REF_DELAY_MS);
    Serial.println("BMP388 reconnected");
    reconnected = true;
  }

  if (!imuReady && i2cFound(config::IMU_ADDR) && startImu()) {
    imuReady = true;
    calibrateImu();
    still = false;
    stillCount = 0;
    preciseStillCount = 0;
    pitchLocked = false;
    rollLocked = false;
    pitch = 0.0f;
    roll = 0.0f;
    Serial.println("MPU6050 reconnected");
    reconnected = true;
  }

  if (reconnected && oledReady) {
    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.print("Sensor OK");
    oled.display();
    delay(400);
  }
}

// Start the MPU6050 and set its ranges.
bool BarometerApp::startImu() {
  imu.initialize();

  if (!imu.testConnection()) {
    return false;
  }

  imu.setFullScaleAccelRange(MPU6050_ACCEL_FS_2);
  imu.setFullScaleGyroRange(MPU6050_GYRO_FS_250);
  imu.setDLPFMode(MPU6050_DLPF_BW_42);
  imu.setRate(4);
  return true;
}

// Measure IMU offsets so the readings are more stable.
void BarometerApp::calibrateImu() {
  long sumGx = 0;
  long sumGy = 0;
  long sumGz = 0;
  long sumAx = 0;
  long sumAy = 0;
  long sumAz = 0;

  for (int i = 0; i < config::IMU_WARMUP_SAMPLES; ++i) {
    int16_t ax, ay, az, gx, gy, gz;
    imu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    delay(config::IMU_WARMUP_DELAY_MS);
  }

  for (int i = 0; i < config::IMU_CAL_SAMPLES; ++i) {
    int16_t ax, ay, az, gx, gy, gz;
    imu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    sumGx += gx;
    sumGy += gy;
    sumGz += gz;
    sumAx += ax;
    sumAy += ay;
    sumAz += az;
    delay(config::IMU_CAL_DELAY_MS);
  }

  gyroOffsetX = sumGx / static_cast<float>(config::IMU_CAL_SAMPLES);
  gyroOffsetY = sumGy / static_cast<float>(config::IMU_CAL_SAMPLES);
  gyroOffsetZ = sumGz / static_cast<float>(config::IMU_CAL_SAMPLES);
  accelOffsetX = sumAx / static_cast<float>(config::IMU_CAL_SAMPLES);
  accelOffsetY = sumAy / static_cast<float>(config::IMU_CAL_SAMPLES);
  accelOffsetZ = (sumAz / static_cast<float>(config::IMU_CAL_SAMPLES)) -
                 config::ACCEL_SCALE;
}
