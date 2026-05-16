/*
  File: BarometerAppCore.ino
  Purpose:
  This file runs the main app flow, sensor cycle and high level device logic.
*/

BarometerApp::BarometerApp()
    : oled(config::SCREEN_WIDTH, config::SCREEN_HEIGHT, &Wire, -1),
      web(80) {}

// Start display, sensors, filters, reference values and WiFi.
void BarometerApp::setup() {
  Serial.begin(115200);
  delay(config::SERIAL_BOOT_MS);

  Wire.begin(config::SDA_PIN, config::SCL_PIN);
  Wire.setClock(400000);

  pinMode(config::ZERO_BUTTON_PIN, INPUT_PULLUP);
  pinMode(config::MODE_BUTTON_PIN, INPUT_PULLUP);

  initDisplay();

  baroReady = startBarometer();
  if (!baroReady) {
    baroHealthy = false;
    Serial.println("BMP388 init failed");
  }

  imuReady = startImu();
  if (!imuReady) {
    Serial.println("MPU6050 init failed");
  } else {
    calibrateImu();
  }

  initFilter(&pitchFilter);
  initFilter(&rollFilter);

  if (baroReady &&
      !captureReference(config::START_REF_SAMPLES,
                        config::START_REF_DELAY_MS,
                        &refPressure, &refTemp)) {
    baroReady = false;
    baroHealthy = false;
    Serial.println("No valid BMP388 samples");
  }

  if (!baroReady) {
    refPressure = 1013.25f;
    refTemp = 20.0f;
  }

  fillPressureBuffer(refPressure);
  fillPrecisePressureBuffer(refPressure);
  fillHeightBuffer(0.0f);

  temp = refTemp;
  pressure = refPressure;

  const unsigned long now = millis();
  lastCalcMs = now;
  lastDisplayMs = now;
  bootMs = now;
  lastMoveMs = now;
  stillSinceMs = 0;

  startWiFi();
}

// Keep the app running.
void BarometerApp::loop() {
  web.handleClient();

  const unsigned long now = millis();

  if (now - lastCalcMs >= config::CALC_MS) {
    runCycle(now);
  }

  if (now - lastDisplayMs >= config::DISPLAY_MS) {
    lastDisplayMs = now;
    drawScreen(now);
  }
}

// Start the OLED and show a short boot screen.
void BarometerApp::initDisplay() {
  if (!oled.begin(SSD1306_SWITCHCAPVCC, config::OLED_ADDR)) {
    Serial.println("SSD1306 init failed");
    while (true) {
      delay(10);
    }
  }

  oledReady = true;
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(2);
  oled.setCursor(20, 8);
  oled.print("Inclina");
  oled.display();
  delay(900);

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.display();
}

// Run one full update step.
void BarometerApp::runCycle(unsigned long now) {
  const float dt = (now - lastCalcMs) / 1000.0f;
  lastCalcMs = now;

  checkSensors(now);

  float tempNow = temp;
  float trimPressure = pressure;
  float trimHeight = 0.0f;
  bool hasTrimSample = false;

  const bool baroReadOk =
      readBarometer(tempNow, trimPressure, trimHeight, hasTrimSample);
  const bool justZeroed = handleZeroReset(now, baroReadOk);

  handleModeSwitch(now);
  updateImu(dt, now);

  if (baroReadOk && hasTrimSample) {
    trimReference(trimPressure, tempNow, trimHeight);
  }

  updateHeight(justZeroed, tempNow, now);
}

// Check if sensors are still alive and try to reconnect them if needed.
void BarometerApp::checkSensors(unsigned long now) {
  if (now - lastSensorCheckMs < config::SENSOR_CHECK_MS) {
    return;
  }

  lastSensorCheckMs = now;

  if (!baroReady || !imuReady) {
    resetI2c();
    reconnectSensors();
  }

  if (imuReady && !imu.testConnection()) {
    imuReady = false;
    Serial.println("MPU6050 connection lost");
  }
}

// Read the barometer and update filtered pressure and height values.
bool BarometerApp::readBarometer(float& tempNow, float& trimPressure,
                                 float& trimHeight, bool& hasTrimSample) {
  bool readOk = false;
  float filteredPressure = pressure;

  if (baroReady) {
    readOk = baro.performReading();
  }

  if (!readOk) {
    if (baroReady) {
      baroHealthy = false;
      lastBaroErrorMs = millis();
      if (baroFailCount < 255) {
        baroFailCount++;
      }
      if (baroFailCount >= config::BARO_FAIL_LIMIT) {
        baroReady = false;
        Serial.println("BMP388 connection lost");
      }
    }
    return false;
  }

  baroFailCount = 0;
  baroHealthy = true;

  const float currentPressure = baro.pressure / 100.0f;
  tempNow = baro.temperature;
  pressure = currentPressure;

  pressureBuffer[pressurePos++] = currentPressure;
  if (pressurePos >= config::MEDIAN_SIZE) {
    pressurePos = 0;
    pressureFilled = true;
  }

  if (pressureFilled) {
    filteredPressure = median(pressureBuffer, config::MEDIAN_SIZE);
  }

  precisePressureBuffer[precisePressurePos++] = currentPressure;
  if (precisePressurePos >= config::PRECISION_PRESSURE_SIZE) {
    precisePressurePos = 0;
    precisePressureFilled = true;
  }

  float precisePressure = filteredPressure;
  if (precisePressureFilled) {
    precisePressure =
        average(precisePressureBuffer, config::PRECISION_PRESSURE_SIZE);
  }

  const float currentHeight =
      calcHeight(refPressure, filteredPressure, tempNow);
  const float currentPreciseHeight =
      calcHeight(refPressure, precisePressure, tempNow);

  heightBuffer[heightPos++] = currentHeight;
  if (heightPos >= config::HEIGHT_MEDIAN_SIZE) {
    heightPos = 0;
    heightFilled = true;
  }

  float filteredHeight = currentHeight;
  if (heightFilled) {
    filteredHeight = heightMedian(heightBuffer, config::HEIGHT_MEDIAN_SIZE);
  }

  smoothHeight = smoothValue(smoothHeight, filteredHeight);

  if (still) {
    if (preciseStillCount < config::PRECISION_STILL_SAMPLES) {
      preciseStillCount++;
    }

    const float alpha =
        (preciseStillCount >= config::PRECISION_STILL_SAMPLES)
            ? config::PRECISION_HOLD_ALPHA
            : config::PRECISION_ALPHA;
    preciseHeight += alpha * (currentPreciseHeight - preciseHeight);
  } else {
    preciseStillCount = 0;
    preciseHeight += config::PRECISION_ALPHA *
                     (currentPreciseHeight - preciseHeight);
  }

  trimPressure = precisePressure;
  trimHeight = currentPreciseHeight;
  hasTrimSample = true;
  return true;
}

// Set a new zero reference from button or web request.
bool BarometerApp::handleZeroReset(unsigned long now, bool baroReadOk) {
  const bool zeroButton = digitalRead(config::ZERO_BUTTON_PIN);
  const bool zeroRequest = zeroPending;
  zeroPending = false;

  bool justZeroed = false;
  const bool waitDone = (now - lastZeroButtonMs > 400);
  const bool buttonPressed = (lastZeroButton == HIGH && zeroButton == LOW);

  if (baroReady && baroReadOk && waitDone &&
      (buttonPressed || zeroRequest)) {
    if (setReference(config::ZERO_SAMPLES, config::ZERO_DELAY_MS)) {
      zeroHoldUntilMs = now + config::ZERO_HOLD_MS;
      lastZeroButtonMs = now;
      lastMoveMs = now;
      justZeroed = true;
    }
  }

  lastZeroButton = zeroButton;
  return justZeroed;
}

// Change the display view with the mode button.
void BarometerApp::handleModeSwitch(unsigned long now) {
  const bool modeButton = digitalRead(config::MODE_BUTTON_PIN);

  if (lastModeButton == HIGH && modeButton == LOW &&
      (now - lastModeButtonMs > 300)) {
    viewMode = static_cast<ViewMode>((viewMode + 1) % 3);
    lastModeButtonMs = now;
    lastMoveMs = now;
  }

  lastModeButton = modeButton;
}

// Read the IMU and update tilt and motion state.
void BarometerApp::updateImu(float dt, unsigned long now) {
  if (!imuReady) {
    still = false;
    rawStill = false;
    sleepMoveCount = 0;
    stillCount = 0;
    preciseStillCount = 0;
    accelLevel = 0.0f;
    gyroLevel = 0.0f;
    pitch = 0.0f;
    roll = 0.0f;
    return;
  }

  int16_t ax, ay, az, gx, gy, gz;
  imu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  const float xg = (ax - accelOffsetX) / config::ACCEL_SCALE;
  const float yg = (ay - accelOffsetY) / config::ACCEL_SCALE;
  const float zg = (az - accelOffsetZ) / config::ACCEL_SCALE;

  const float xRate = (gx - gyroOffsetX) / config::GYRO_SCALE;
  const float yRate = (gy - gyroOffsetY) / config::GYRO_SCALE;
  const float zRate = (gz - gyroOffsetZ) / config::GYRO_SCALE;

  const float accelNow = sqrtf(xg * xg + yg * yg + zg * zg);
  const float gyroNow = sqrtf(xRate * xRate + yRate * yRate + zRate * zRate);
  accelLevel = accelNow;
  gyroLevel = gyroNow;

  const bool rawStillNow =
      (fabs(accelNow - 1.0f) <= config::STILL_ACCEL_LIMIT) &&
      (gyroNow <= config::STILL_GYRO_LIMIT);
  rawStill = rawStillNow;

  const bool movingNow =
      (fabs(accelNow - 1.0f) >= config::MOVE_ACCEL_LIMIT) ||
      (gyroNow >= config::MOVE_GYRO_LIMIT);

  if (movingNow) {
    if (sleepMoveCount < config::SLEEP_MOVE_COUNT) {
      sleepMoveCount++;
    }
  } else {
    sleepMoveCount = 0;
  }

  if (sleepMoveCount >= config::SLEEP_MOVE_COUNT) {
    lastMoveMs = now;
    sleepMoveCount = 0;
  }

  if (rawStillNow) {
    if (stillCount < config::STILL_COUNT_ENTER) {
      stillCount++;
    }
  } else if (stillCount > 0) {
    stillCount--;
  }

  if (still) {
    if (stillCount <= config::STILL_COUNT_EXIT) {
      still = false;
    }
  } else if (stillCount >= config::STILL_COUNT_ENTER) {
    still = true;
  }

  if (!movedAfterZero && movingNow) {
    movedAfterZero = true;
  }

  const float pitchFromAccel = atan2Deg(xg, sqrtf(yg * yg + zg * zg));
  const float rollFromAccel = atan2Deg(yg, sqrtf(xg * xg + zg * zg));

  pitch = updateFilter(&pitchFilter, pitchFromAccel, yRate, dt);
  roll = updateFilter(&rollFilter, rollFromAccel, xRate, dt);

  if (pitchLocked) {
    if (fabs(pitch) > config::ANGLE_LOCK_EXIT) {
      pitchLocked = false;
    } else {
      pitch = 0.0f;
    }
  } else if (fabs(pitch) < config::ANGLE_LOCK_ENTER) {
    pitchLocked = true;
  }

  if (rollLocked) {
    if (fabs(roll) > config::ANGLE_LOCK_EXIT) {
      rollLocked = false;
    } else {
      roll = 0.0f;
    }
  } else if (fabs(roll) < config::ANGLE_LOCK_ENTER) {
    rollLocked = true;
  }

  pitch = round1(constrain(pitch, -90.0f, 90.0f));
  roll = round1(constrain(roll, -90.0f, 90.0f));
}

// Decide what height to show and when sleep is allowed.
void BarometerApp::updateHeight(bool justZeroed, float tempNow,
                                unsigned long now) {
  if (still) {
    if (stillSinceMs == 0) {
      stillSinceMs = now;
    }
  } else {
    stillSinceMs = 0;
    holdActive = false;
  }

  const float normalHeight =
      movedAfterZero
          ? ((1.0f - config::NORMAL_BLEND) * smoothHeight +
             (config::NORMAL_BLEND * preciseHeight))
          : smoothHeight;

  if (!zeroArmed && still && stillSinceMs != 0 &&
      (now - stillSinceMs) >= config::HEIGHT_HOLD_MS &&
      !holdActive) {
    heldHeight = normalHeight;
    heldPreciseHeight = preciseHeight;
    holdActive = true;
  }

  if (!zeroArmed && still && stillSinceMs != 0 &&
      (now - stillSinceMs) >= config::ZERO_REARM_MS &&
      fabs(smoothHeight) < config::ZERO_REARM_HEIGHT &&
      fabs(preciseHeight) < config::ZERO_REARM_HEIGHT) {
    zeroArmed = true;
    heightLocked = true;
    holdActive = false;
    movedAfterZero = false;
  }

  const float viewHeight = holdActive ? heldHeight : normalHeight;
  float shownHeight =
      (justZeroed || now < zeroHoldUntilMs) ? 0.0f : viewHeight;

  if (zeroArmed && movedAfterZero &&
      fabs(shownHeight) > config::ZERO_UNLOCK_HEIGHT) {
    zeroArmed = false;
    heightLocked = false;
    holdActive = false;
  }

  if (zeroArmed) {
    if (!movedAfterZero) {
      heightLocked = true;
      shownHeight = 0.0f;
    } else if (heightLocked) {
      if (fabs(shownHeight) > config::HEIGHT_LOCK_EXIT) {
        heightLocked = false;
      } else {
        shownHeight = 0.0f;
      }
    } else if (fabs(shownHeight) < config::HEIGHT_LOCK_ENTER && still) {
      heightLocked = true;
      shownHeight = 0.0f;
    }
  } else {
    heightLocked = false;
  }

  if (fabs(shownHeight) < config::HEIGHT_DEADBAND) {
    shownHeight = 0.0f;
  }

  height = round2(shownHeight);
  temp = tempNow;

  if (shouldSleep(now)) {
    goToSleep();
  }
}
