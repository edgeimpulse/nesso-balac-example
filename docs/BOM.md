# Bill of materials (BOM)

Everything needed to build one BLE-controlled balancing robot. The **Nesso N1 replaces the
original M5StickC controller** on the Kiraku Labo BalaC base; the two connect over the base's
I²C bus (motor driver at `0x38`).

## Core build

| # | Item | Qty | Purpose / notes |
|---|------|-----|-----------------|
| 1 | **Arduino Nesso N1** (ESP32-C6) | 1 | Main controller — MCU, display, KEY1/KEY2 buttons, on-board **BMI270 IMU** (`0x68`), battery gauge, Bluetooth LE. Runs `BalaCplus`. |
| 2 | **Kiraku Labo BalaC base** (a.k.a. BalaC / BalaC+ kit) | 1 | Chassis, two geared motors + wheels, and the **I²C motor driver at `0x38`** the firmware talks to. |
| 3 | **LiPo battery** for the BalaC base | 1 | Powers motors + controller (use the cell the BalaC kit is designed for). |
| 4 | **USB-C cable** | 1 | Flashing the Nesso N1 and serial debug. |
| 5 | **Android phone** (Android 8.0 / API 24+, BLE) | 1 | Runs the `BalaC Control` app. Most modern phones qualify. |

## Optional — ROS 2 / camera control path

| # | Item | Qty | Purpose / notes |
|---|------|-----|-----------------|
| 6 | **Raspberry Pi** (any Wi-Fi model) | 1 | Runs the micro-ROS agent + `camera_control_node`. |
| 7 | **USB webcam** | 1 | Vision input for the camera controller. |
| 8 | microSD card + PSU for the Pi | 1 | Standard Raspberry Pi accessories. |

## Notes

- **No extra electronics are required for the BLE build** — items 1–5 are enough to drive the
  robot from the phone.
- The IMU and display are **built into the Nesso N1**, so they are not separate line items.
- Firmware libraries (IMU driver, etc.) are listed in [../README-libs.md](../README-libs.md);
  Bluetooth LE needs no extra library (built into the ESP32-C6 Arduino core).
- Exact SKUs/links depend on your region — search the item names above at the Arduino Store
  (Nesso N1) and Kiraku Labo / Switch Science (BalaC base).
