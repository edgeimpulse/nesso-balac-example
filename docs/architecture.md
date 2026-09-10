# Architecture

The robot has two independent control paths that share the same balancing firmware on the
Nesso N1: a **Bluetooth LE** remote (the Android app) and an optional **micro-ROS** transport
(camera / Blockly over ROS 2).

## Bluetooth LE control path

```mermaid
flowchart LR
    subgraph Phone["Android phone"]
        App["BalaC Control app<br/>(Kotlin · BLE client)"]
    end

    subgraph Nesso["Nesso N1 · ESP32-C6"]
        FW["BalaCplus firmware<br/>balance controller (PID)"]
        BLE["BLE · Nordic UART Service"]
        IMU["BMI270 IMU"]
        UI["Display + KEY1/KEY2"]
    end

    subgraph Base["BalaC base"]
        MD["Motor driver<br/>I²C 0x38"]
        ML["Left motor"]
        MR["Right motor"]
    end

    App -- "write: D,throttle,steer / S / M" --> BLE
    BLE -- "notify: T,batt,tilt,standing" --> App
    BLE <--> FW
    IMU -- "I²C 0x68 (tilt, gyro)" --> FW
    FW --> UI
    FW -- "I²C 0x38 (PWM)" --> MD
    MD --> ML
    MD --> MR
```

The app writes short ASCII commands to the RX characteristic; the firmware maps `throttle`/`steer`
onto the balance controller (`moveRate` and yaw) while it keeps the robot upright from the IMU.
If no command arrives within 600 ms the firmware zeroes the throttle (failsafe).

## micro-ROS control path (optional)

```mermaid
flowchart LR
    subgraph Host["Raspberry Pi / PC · ROS 2"]
        Cam["camera_control_node<br/>(publishes /cmd_vel)"]
        Blk["Blockly teleop<br/>(rclpy → /cmd_vel)"]
        Agent["micro-ROS agent"]
    end

    subgraph Nesso2["Nesso N1 · ESP32-C6"]
        Node["microros_imu_publisher<br/>/imu (pub) · /cmd_vel (sub)"]
    end

    Cam -- "/cmd_vel" --> Agent
    Blk -- "/cmd_vel" --> Agent
    Agent <-- "serial over USB (XRCE-DDS)" --> Node
    Node -- "/imu" --> Agent
```

## Repository map

| Path | Role |
|------|------|
| `BalaCplus/` | Balancing firmware for the Nesso N1 (+ BLE remote control) |
| `android-balac-control/` | Android app (BLE client) |
| `microros_imu_publisher/` | micro-ROS node: publishes `/imu`, subscribes to `/cmd_vel` |
| `ros2_camera_control/` | Raspberry Pi camera node that publishes `/cmd_vel` |
| `blockly/` | Blockly → `rclpy` teaching example |
| `.github/workflows/` | CI: builds the APK and publishes release artifacts |
