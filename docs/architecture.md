# Architecture

The robot has two control paths that share the same balancing firmware on the Nesso N1. One is
a Bluetooth LE remote (the Android app). The other is an optional micro-ROS transport, used by
the camera controller and the Blockly example over ROS 2.

## Bluetooth LE control path

The signal flow is:

1. The Android app (a Kotlin Bluetooth LE client) writes short ASCII commands to the RX
   characteristic of the Nordic UART Service: `D,throttle,steer` to drive, `S` to stop and `M`
   to change mode.
2. The BalaCplus firmware maps `throttle` and `steer` onto the balance controller (`moveRate`
   and yaw) while it keeps the robot upright using the BMI270 IMU, read over I²C at address
   `0x68`.
3. The firmware drives the left and right motors through the BalaC motor driver over I²C at
   address `0x38`, and updates the display and reads the KEY1 and KEY2 buttons.
4. Roughly once a second the firmware notifies telemetry back to the app on the TX
   characteristic: battery, tilt and whether the robot is standing.

If no command arrives within 600 ms the firmware zeroes the throttle, so the robot stops if the
Bluetooth link drops.

## micro-ROS control path (optional)

Here the robot runs the `microros_imu_publisher` sketch instead of the Bluetooth firmware. It
publishes `/imu` and subscribes to `/cmd_vel`. On a Raspberry Pi or PC, a micro-ROS agent
bridges those topics over a USB serial link. Two example publishers send `/cmd_vel`: the
`camera_control_node`, and the Blockly example, which generates `rclpy` code. The robot streams
`/imu` back the same way.

## Repository map

| Path | Role |
|------|------|
| `BalaCplus/` | Balancing firmware for the Nesso N1 (+ BLE remote control) |
| `android-balac-control/` | Android app (BLE client) |
| `microros_imu_publisher/` | micro-ROS node: publishes `/imu`, subscribes to `/cmd_vel` |
| `ros2_camera_control/` | Raspberry Pi camera node that publishes `/cmd_vel` |
| `blockly/` | Blockly teaching example that generates `rclpy` |
| `.github/workflows/` | CI: builds the APK and publishes release artifacts |
