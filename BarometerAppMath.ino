/*
  File: BarometerAppMath.ino
  Purpose:
  This file holds helper math, filtering and reference functions.
*/

// Round to one decimal place.
float BarometerApp::round1(float value) const {
  return roundf(value * 10.0f) / 10.0f;
}

// Round to two decimal places.
float BarometerApp::round2(float value) const {
  return roundf(value * 100.0f) / 100.0f;
}

// Reset a filter to its start state.
void BarometerApp::initFilter(FilterState* filter) {
  filter->angle = 0.0f;
  filter->bias = 0.0f;
  filter->p[0][0] = 1.0f;
  filter->p[0][1] = 0.0f;
  filter->p[1][0] = 0.0f;
  filter->p[1][1] = 1.0f;
}

// Update one Kalman style filter step.
float BarometerApp::updateFilter(FilterState* filter, float newAngle,
                                 float newRate, float dt) {
  const float qAngle = 0.001f;
  const float qBias = 0.003f;
  const float rMeasure = 0.03f;

  filter->angle += dt * (newRate - filter->bias);

  filter->p[0][0] += dt * (dt * filter->p[1][1] - filter->p[0][1] -
                           filter->p[1][0] + qAngle);
  filter->p[0][1] -= dt * filter->p[1][1];
  filter->p[1][0] -= dt * filter->p[1][1];
  filter->p[1][1] += qBias * dt;

  const float s = filter->p[0][0] + rMeasure;
  const float k0 = filter->p[0][0] / s;
  const float k1 = filter->p[1][0] / s;
  const float y = newAngle - filter->angle;

  filter->angle += k0 * y;
  filter->bias += k1 * y;

  const float p00 = filter->p[0][0];
  const float p01 = filter->p[0][1];

  filter->p[0][0] -= k0 * p00;
  filter->p[0][1] -= k0 * p01;
  filter->p[1][0] -= k1 * p00;
  filter->p[1][1] -= k1 * p01;

  return filter->angle;
}

// Convert pressure to relative height.
float BarometerApp::calcHeight(float basePressure, float currentPressure,
                               float tempNow) const {
  if (currentPressure <= 0.0f || basePressure <= 0.0f) {
    return 0.0f;
  }

  const float tempKelvin = ((refTemp + tempNow) * 0.5f) + 273.15f;
  return config::HEIGHT_SCALE *
         (config::AIR_HEIGHT_FACTOR * tempKelvin *
          logf(basePressure / currentPressure));
}

// Smooth the height, but react faster to bigger jumps.
float BarometerApp::smoothValue(float currentValue,
                                float targetValue) const {
  const float error = targetValue - currentValue;
  const float alpha = (fabs(error) >= config::HEIGHT_FAST_DELTA)
                          ? config::HEIGHT_FAST_ALPHA
                          : config::HEIGHT_SLOW_ALPHA;
  return currentValue + alpha * error;
}

// atan2 in degrees.
float BarometerApp::atan2Deg(float y, float x) const {
  return atan2f(y, x) * 180.0f / PI;
}

// Median for small value sets.
float BarometerApp::median(const float* values, int count) const {
  float sorted[config::MEDIAN_SIZE];
  memcpy(sorted, values, count * sizeof(float));

  for (int i = 1; i < count; ++i) {
    const float key = sorted[i];
    int j = i - 1;
    while (j >= 0 && sorted[j] > key) {
      sorted[j + 1] = sorted[j];
      --j;
    }
    sorted[j + 1] = key;
  }

  return sorted[count / 2];
}

// Median for the height buffer.
float BarometerApp::heightMedian(const float* values, int count) const {
  float sorted[config::HEIGHT_MEDIAN_SIZE];
  memcpy(sorted, values, count * sizeof(float));

  for (int i = 1; i < count; ++i) {
    const float key = sorted[i];
    int j = i - 1;
    while (j >= 0 && sorted[j] > key) {
      sorted[j + 1] = sorted[j];
      --j;
    }
    sorted[j + 1] = key;
  }

  return sorted[count / 2];
}

// Average of a float list.
float BarometerApp::average(const float* values, int count) const {
  float sum = 0.0f;
  for (int i = 0; i < count; ++i) {
    sum += values[i];
  }
  return sum / count;
}

// Capture several barometer samples to build a stable reference.
bool BarometerApp::captureReference(int samples, int delayMs,
                                    float* pressureOut, float* tempOut) {
  float pressureSamples[config::MAX_REF_SAMPLES];
  float tempSamples[config::MAX_REF_SAMPLES];
  int validCount = 0;

  for (int i = 0; i < samples; ++i) {
    if (baro.performReading() && validCount < config::MAX_REF_SAMPLES) {
      pressureSamples[validCount] = baro.pressure / 100.0f;
      tempSamples[validCount] = baro.temperature;
      validCount++;
    }
    delay(delayMs);
  }

  if (validCount == 0) {
    return false;
  }

  for (int i = 1; i < validCount; ++i) {
    const float pressureKey = pressureSamples[i];
    const float tempKey = tempSamples[i];
    int j = i - 1;

    while (j >= 0 && pressureSamples[j] > pressureKey) {
      pressureSamples[j + 1] = pressureSamples[j];
      tempSamples[j + 1] = tempSamples[j];
      --j;
    }

    pressureSamples[j + 1] = pressureKey;
    tempSamples[j + 1] = tempKey;
  }

  int trim = validCount / 5;
  if (trim * 2 >= validCount) {
    trim = 0;
  }

  float pressureSum = 0.0f;
  float tempSum = 0.0f;
  int usedCount = 0;

  for (int i = trim; i < (validCount - trim); ++i) {
    pressureSum += pressureSamples[i];
    tempSum += tempSamples[i];
    usedCount++;
  }

  if (usedCount == 0) {
    return false;
  }

  *pressureOut = pressureSum / usedCount;
  *tempOut = tempSum / usedCount;
  return true;
}

// Fill the normal pressure buffer with one start value.
void BarometerApp::fillPressureBuffer(float value) {
  for (int i = 0; i < config::MEDIAN_SIZE; ++i) {
    pressureBuffer[i] = value;
  }
  pressurePos = 0;
  pressureFilled = true;
}

// Fill the precision pressure buffer with one start value.
void BarometerApp::fillPrecisePressureBuffer(float value) {
  for (int i = 0; i < config::PRECISION_PRESSURE_SIZE; ++i) {
    precisePressureBuffer[i] = value;
  }
  precisePressurePos = 0;
  precisePressureFilled = true;
}

// Fill the height buffer with one start value.
void BarometerApp::fillHeightBuffer(float value) {
  for (int i = 0; i < config::HEIGHT_MEDIAN_SIZE; ++i) {
    heightBuffer[i] = value;
  }
  heightPos = 0;
  heightFilled = true;
}

// Use the current pressure and temperature as the new zero reference.
void BarometerApp::setReferenceFromCurrent(float currentPressure,
                                           float currentTemp) {
  refPressure = currentPressure;
  refTemp = currentTemp;
  pressure = currentPressure;
  preciseHeight = 0.0f;
  preciseStillCount = 0;
  smoothHeight = 0.0f;
  height = 0.0f;
  temp = currentTemp;
  heightLocked = true;
  zeroArmed = true;
  holdActive = false;
  movedAfterZero = false;
  heldHeight = 0.0f;
  heldPreciseHeight = 0.0f;
  stillSinceMs = 0;
  fillPressureBuffer(refPressure);
  fillPrecisePressureBuffer(refPressure);
  fillHeightBuffer(0.0f);
}

// Slowly trim the reference while the device is still and near zero.
void BarometerApp::trimReference(float currentPressure, float currentTemp,
                                 float currentHeight) {
  if (!zeroArmed || movedAfterZero || !still ||
      fabs(currentHeight) > config::REF_TRIM_HEIGHT) {
    return;
  }

  refPressure += config::REF_TRIM_ALPHA * (currentPressure - refPressure);
  refTemp += config::REF_TEMP_ALPHA * (currentTemp - refTemp);
}

// Get the height value used in the precise view.
float BarometerApp::getPreciseHeightForView() const {
  if (millis() < zeroHoldUntilMs) {
    return 0.0f;
  }

  if (zeroArmed && !movedAfterZero) {
    return 0.0f;
  }

  const float value = holdActive ? heldPreciseHeight : preciseHeight;
  if (zeroArmed && still && fabs(value) < config::HEIGHT_LOCK_ENTER) {
    return 0.0f;
  }
  if (fabs(value) < config::HEIGHT_DEADBAND) {
    return 0.0f;
  }

  return round2(value);
}

// Capture a fresh multi sample reference.
bool BarometerApp::setReference(int samples, int delayMs) {
  float currentPressure = 0.0f;
  float currentTemp = 0.0f;

  if (!baroReady ||
      !captureReference(samples, delayMs, &currentPressure, &currentTemp)) {
    return false;
  }

  setReferenceFromCurrent(currentPressure, currentTemp);
  return true;
}

// Save the time of the last activity.
void BarometerApp::noteActivity() {
  lastMoveMs = millis();
}

// Decide if the device should enter deep sleep.
bool BarometerApp::shouldSleep(unsigned long now) const {
  if (config::AUTO_SLEEP_MS == 0UL) {
    return false;
  }
  if ((now - bootMs) < config::BOOT_SLEEP_DELAY_MS) {
    return false;
  }
  if (!imuReady || !still) {
    return false;
  }
  return (now - lastMoveMs) >= config::AUTO_SLEEP_MS;
}

// Turn off screen and WiFi and enter deep sleep.
void BarometerApp::goToSleep() {
  if (oledReady) {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.print("Sleep Mode");
    oled.setCursor(0, 12);
    oled.print("Use switch/reset");
    oled.display();
    delay(1200);
    oled.ssd1306_command(SSD1306_DISPLAYOFF);
  }

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  esp_deep_sleep_start();
}
