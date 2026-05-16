/*
  File: BarometerAppUi.ino
  Purpose:
  This file handles the web server, JSON output and OLED drawing.
*/

// Return the current WiFi mode as text.
String BarometerApp::wifiModeText() const {
  switch (WiFi.getMode()) {
    case WIFI_AP:
      return "AP Mode";
    case WIFI_STA:
      return "WiFi";
    default:
      return "Offline";
  }
}

// Return the current screen view name.
String BarometerApp::viewModeText() const {
  switch (viewMode) {
    case VIEW_PRECISION:
      return "Precise";
    case VIEW_STATUS:
      return "Status";
    default:
      return "Normal";
  }
}

// Build a simple error message for the display and web page.
String BarometerApp::errorText() const {
  if (!baroReady && !imuReady) {
    return "BMP388 and MPU6050 not connected";
  }
  if (!baroReady) {
    return "BMP388 not connected";
  }
  if (!imuReady) {
    return "MPU6050 not connected";
  }
  if (!baroHealthy) {
    return "BMP388 read error";
  }
  return "OK";
}

// Send the dashboard page.
void BarometerApp::handleHome() {
  web.send_P(200, "text/html; charset=utf-8", webcontent::INDEX_HTML);
}

// Send all current values as JSON.
void BarometerApp::handleData() {
  const float webHeight =
      (viewMode == VIEW_PRECISION) ? getPreciseHeightForView() : height;

  String json = "{";
  json += "\"height\":" + String(webHeight, 2) + ",";
  json += "\"temp\":" + String(temp, 1) + ",";
  json += "\"pressure\":" + String(pressure, 2) + ",";
  json += "\"roll\":" + String(roll, 1) + ",";
  json += "\"pitch\":" + String(pitch, 1) + ",";
  json += "\"gyro\":" + String(gyroLevel, 2) + ",";
  json += "\"accel\":" + String(accelLevel, 3) + ",";
  json += "\"baro\":" + String(baroReady ? "true" : "false") + ",";
  json += "\"imu\":" + String(imuReady ? "true" : "false") + ",";
  json += "\"rawStill\":" + String(rawStill ? "true" : "false") + ",";
  json += "\"still\":" + String(still ? "true" : "false") + ",";
  json += "\"mode\":\"" + wifiModeText() + "\",";
  json += "\"view\":\"" + viewModeText() + "\",";
  json += "\"ip\":\"" + ipText + "\",";
  json += "\"error\":\"" + errorText() + "\"";
  json += "}";

  web.send(200, "application/json; charset=utf-8", json);
}

// Mark that the user wants a new zero reference.
void BarometerApp::handleZero() {
  noteActivity();
  zeroPending = true;
  web.send(200, "application/json; charset=utf-8", "{\"ok\":true}");
}

// Restart the ESP from the web page.
void BarometerApp::handleRestart() {
  noteActivity();
  web.send(200, "application/json; charset=utf-8",
           "{\"ok\":true,\"restarting\":true}");
  delay(150);
  ESP.restart();
}

// Change the current display view from the web page.
void BarometerApp::handleView() {
  noteActivity();

  if (web.hasArg("value")) {
    const String value = web.arg("value");
    if (value == "normal") {
      viewMode = VIEW_NORMAL;
    } else if (value == "precise") {
      viewMode = VIEW_PRECISION;
    } else if (value == "status") {
      viewMode = VIEW_STATUS;
    }
  }

  web.send(200, "application/json; charset=utf-8", "{\"ok\":true}");
}

// Register all web routes and start the server.
void BarometerApp::setupWeb() {
  web.on("/", HTTP_GET, [this]() { handleHome(); });
  web.on("/data", HTTP_GET, [this]() { handleData(); });
  web.on("/zero", HTTP_GET, [this]() { handleZero(); });
  web.on("/zero", HTTP_POST, [this]() { handleZero(); });
  web.on("/restart", HTTP_GET, [this]() { handleRestart(); });
  web.on("/restart", HTTP_POST, [this]() { handleRestart(); });
  web.on("/view", HTTP_GET, [this]() { handleView(); });
  web.on("/view", HTTP_POST, [this]() { handleView(); });
  web.begin();
}

// Start the ESP32 as its own WiFi access point.
void BarometerApp::startWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAP(config::AP_SSID, config::AP_PASSWORD);
  ipText = WiFi.softAPIP().toString();

  Serial.print("AP started: ");
  Serial.print(config::AP_SSID);
  Serial.print("  http://");
  Serial.println(ipText);

  setupWeb();
}

// Pick the current OLED screen.
void BarometerApp::drawScreen(unsigned long now) {
  if (!baroReady || !imuReady) {
    drawErrorView();
    return;
  }

  Serial.printf("H:%.2fm T:%.1fC P:%.2fhPa X:%.1f Y:%.1f\n",
                height, temp, pressure, roll, pitch);

  switch (viewMode) {
    case VIEW_PRECISION:
      drawPreciseView();
      return;
    case VIEW_STATUS:
      drawStatusView();
      return;
    default:
      drawNormalView(now);
      return;
  }
}

// Draw the precise height screen.
void BarometerApp::drawPreciseView() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print("Precise Height");

  oled.setTextSize(2);
  oled.setCursor(0, 12);
  oled.print(getPreciseHeightForView(), 2);
  oled.print("m");
  oled.setTextSize(1);

  oled.setCursor(88, 0);
  oled.print(temp, 1);
  oled.print("C");

  oled.setCursor(76, 22);
  oled.print(pressure, 1);
  oled.print("h");

  oled.display();
}

// Draw sensor state and motion values.
void BarometerApp::drawStatusView() {
  oled.clearDisplay();

  oled.setCursor(0, 0);
  oled.print(baroReady ? "BMP:OK" : "BMP:ERR");
  oled.setCursor(66, 0);
  oled.print(imuReady ? "IMU:OK" : "IMU:ERR");

  oled.setCursor(0, 10);
  oled.print("G:");
  oled.print(gyroLevel, 2);
  oled.setCursor(64, 10);
  oled.print("A:");
  oled.print(accelLevel, 2);

  oled.setCursor(0, 20);
  oled.print(still ? "Still " : "Move  ");
  oled.print(rawStill ? "rawY" : "rawN");

  oled.display();
}

// Draw the main bubble level view.
void BarometerApp::drawNormalView(unsigned long now) {
  oled.clearDisplay();

  const int centerX = 16;
  const int centerY = 16;
  const int radius = 13;

  oled.drawCircle(centerX, centerY, radius, SSD1306_WHITE);

  const float normX = constrain(roll / config::MAX_ANGLE, -1.0f, 1.0f);
  const float normY = constrain(pitch / config::MAX_ANGLE, -1.0f, 1.0f);

  bubbleX = 0.8f * bubbleX + 0.2f * normX;
  bubbleY = 0.8f * bubbleY + 0.2f * normY;

  const int dotX = centerX + bubbleX * (radius - 4);
  const int dotY = centerY - bubbleY * (radius - 4);

  oled.fillCircle(dotX, dotY, 3, SSD1306_WHITE);

  oled.setCursor(34, 0);
  oled.print("H:");
  oled.print(height, 2);
  oled.print("m");

  oled.setCursor(34, 10);
  oled.print("T:");
  oled.print(temp, 1);
  if (now - lastBaroErrorMs < config::BARO_ERROR_MS) {
    oled.setCursor(88, 10);
    oled.print("BMP!");
  }

  oled.setCursor(34, 20);
  oled.print("X:");
  oled.print(roll, 0);
  oled.print(" Y:");
  oled.print(pitch, 0);

  oled.display();
}

// Draw an error screen if a sensor is missing.
void BarometerApp::drawErrorView() {
  if (!oledReady) {
    return;
  }

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  oled.setCursor(0, 0);
  oled.print("Sensor Error");

  int y = 10;
  if (!baroReady) {
    oled.setCursor(0, y);
    oled.print("BMP388 missing");
    y += 10;
  }
  if (!imuReady) {
    oled.setCursor(0, y);
    oled.print("MPU6050 missing");
    y += 10;
  }
  if (baroReady && imuReady) {
    oled.setCursor(0, y);
    oled.print("Unknown error");
  }

  oled.display();
}
