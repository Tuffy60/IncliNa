# IncliNa

IncliNa is an Arduino project for the **Seeed Studio XIAO ESP32S3**.
It combines a barometer, IMU, OLED display and a built in web dashboard to create a compact height and tilt monitor.

## Features

- BMP388 for pressure, temperature and relative height
- MPU6050 for roll, pitch and motion detection
- SSD1306 128x32 OLED for local live output
- built in WiFi access point with browser dashboard
- zero / reference reset from a button or the web page
- normal, precise and status views
- sensor reconnect handling
- automatic deep sleep after inactivity

## Hardware

- MCU board: `Seeed Studio XIAO ESP32S3`
- Barometer: `BMP388`
- IMU: `MPU6050`
- OLED: `SSD1306 128x32`
- Input: `2 buttons`

## Wiring

- `SDA` -> `D4`
- `SCL` -> `D5`
- `Zero button` -> `D6`
- `Mode button` -> `D7`

All I2C devices share the same bus:

- BMP388 on I2C
- MPU6050 on I2C
- SSD1306 OLED on I2C

## Project Structure

```text
IncliNa/
  IncliNa.ino           # includes, config values, class, setup/loop
  AWebContent.ino       # built in HTML for the dashboard
  BarometerAppCore.ino  # main app flow
  BarometerAppMath.ino  # math, filtering and reference helpers
  BarometerAppSensors.ino
  BarometerAppUi.ino
```

## Required Libraries

- `Adafruit BMP3XX Library`
- `Adafruit GFX Library`
- `Adafruit SSD1306`
- `MPU6050` by Electronic Cats, or another compatible library that provides `MPU6050.h`
- `WiFi` and `WebServer` from the ESP32 core

## Arduino IDE Setup

1. Install Arduino IDE.
2. Install the `ESP32 by Espressif Systems` board package.
3. Install the required libraries listed above.
4. Open `IncliNa/IncliNa.ino`.
5. Select the board: `XIAO ESP32S3`.
6. Select the correct COM port.
7. Click `Verify`, then `Upload`.

## Web Dashboard

After boot, the device starts its own WiFi access point.

- SSID: `XIAO-Baro`
- Password: `12345678`

Open the IP address shown in the Serial Monitor in a browser to access the dashboard.

Dashboard functions:

- set zero reference
- switch between `Normal`, `Precise` and `Status` views
- restart the ESP32

## Display Views

### Normal

- bubble level style tilt view
- height readout
- temperature readout
- roll and pitch values

### Precise

- precise height view
- pressure and temperature values

### Status

- BMP388 status
- MPU6050 status
- gyro activity level
- accel activity level
- still / moving state

## Notes

- Height is calculated relative to a captured pressure reference.
- Pressure and height values are smoothed to reduce noise.
- Roll and pitch are filtered for more stable readings.
- The device can enter deep sleep after long inactivity.
- If you publish this project, you should change the default WiFi credentials.

## License

No license file has been added yet.
If you plan to publish the project publicly, add a license such as `MIT`.
