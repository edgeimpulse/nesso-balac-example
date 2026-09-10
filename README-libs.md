# Nesso N1 BalaC Example - Required Arduino Libraries

Below are the libraries you need to install for compiling and uploading the BalaCplus.ino sketch to your Nesso N1 hardware:

> **Board core first:** install the **Arduino Nesso N1** board package in the Arduino IDE Boards
> Manager (or via Arduino CLI). It provides `Arduino_Nesso_N1.h` (display, battery, buttons) and
> the global `IMU`, and bundles the Bluetooth LE stack used for remote control.

## 1. Arduino_BMI270_BMM150 (IMU)
The BMI270 IMU is read through the board's global `IMU` object, so the sketches use Arduino's
official driver.
- [Arduino_BMI270_BMM150 (GitHub)](https://github.com/arduino-libraries/Arduino_BMI270_BMM150)
- [Arduino_BMI270_BMM150 (Arduino Library Manager)](https://registry.arduino.cc/libraries/Arduino_BMI270_BMM150)

> Alternative: [DFRobot_BMI270](https://github.com/DFRobot/DFRobot_BMI270) also works if you
> prefer to address the sensor directly over I²C.

## 2. Adafruit_ST7789 (Display)
- [Adafruit ST7789 Arduino Library (GitHub)](https://github.com/adafruit/Adafruit-ST7735-Library)
- [Adafruit ST7789 (Arduino Library Manager)](https://registry.arduino.cc/libraries/Adafruit_ST7735_and_ST7789_Library)

## 3. Adafruit_GFX (Display Core)
- [Adafruit GFX Arduino Library (GitHub)](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit GFX (Arduino Library Manager)](https://registry.arduino.cc/libraries/Adafruit_GFX_Library)

## 4. Wire (I2C)
- Built-in with Arduino ESP32 core

## 5. SPI (Display)
- Built-in with Arduino ESP32 core

## 6. M5StickCPlus (Original BalaC code only)
- [M5StickCPlus Arduino Library (GitHub)](https://github.com/m5stack/M5StickCPlus)
- [M5StickCPlus (Arduino Library Manager)](https://registry.arduino.cc/libraries/M5StickCPlus)

---

**How to install:**
- Open Arduino IDE > Tools > Manage Libraries
- Search for each library name above and click Install
- Or download from GitHub and install via "Add .ZIP Library"

**Note:**
- The Nesso N1 port uses the `Arduino_Nesso_N1` board core (display, battery, buttons) plus the
  `Arduino_BMI270_BMM150` IMU driver. Adafruit_ST7789 / Adafruit_GFX and DFRobot_BMI270 are
  drop-in alternatives if you drive the display/IMU yourself.
- Use M5StickCPlus only for the original BalaC hardware.
- Bluetooth LE remote control needs no extra library — it is built into the ESP32-C6 Arduino core.
  See the root [README.md](README.md) for the BLE protocol and the `android-balac-control` app.
