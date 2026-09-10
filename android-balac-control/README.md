# BalaC Control — Android BLE remote

A minimal Android app that drives the **BalaC-Nesso** balancing robot over Bluetooth LE using the
Nordic UART Service (NUS). It scans for the robot by service UUID, connects, sends drive commands
while you hold the on-screen buttons, and shows the telemetry the robot notifies back.

## Requirements

- Android Studio (Giraffe or newer) **or** a command-line setup with JDK 17 and the Android SDK.
- A phone running Android 8.0 (API 24) or newer with Bluetooth LE.
- The robot flashed with [`../BalaCplus/BalaCplus.ino`](../BalaCplus/BalaCplus.ino) and advertising
  as `BalaC-Nesso`.

## Build & run

### Android Studio
1. `File → Open…` and select this `android-balac-control` folder.
2. Let Gradle sync (it downloads the Gradle 8.2 wrapper and dependencies).
3. Run the `app` configuration on your device.

### Command line
The Gradle wrapper JAR is not checked in. Generate it once (needs a system Gradle ≥ 8.2), then
build:

```sh
cd android-balac-control
gradle wrapper --gradle-version 8.2      # creates gradlew + gradle-wrapper.jar
./gradlew assembleDebug
./gradlew installDebug                    # with a device connected
```

## Using the app

1. Tap **Connect** and grant the Bluetooth permissions when prompted
   (`BLUETOOTH_SCAN` / `BLUETOOTH_CONNECT` on Android 12+, Location on older versions).
2. Once the status shows **Ready**, hold **Forward / Back / Left / Right** to drive — releasing a
   button sends **stop**. **STOP** forces a halt; **Toggle Stand / Demo** switches robot mode.
3. Remote control only takes effect while the robot is in **Stand** mode and actively balancing.

## Protocol

See the *BLE control protocol* section in the [repository README](../README.md). The app writes
`D,<throttle>,<steer>` / `S` / `M` lines to the RX characteristic and parses `T,...` telemetry
from the TX characteristic.
