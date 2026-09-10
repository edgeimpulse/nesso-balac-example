# micro-ROS on Nesso N1 (ESP32-C6)

This guide helps you run a micro-ROS example on the Nesso N1, inspired by the Zephyr emulator tutorial:
https://micro.ros.org/docs/tutorials/core/zephyr_emulator/

## Prerequisites
- ESP32-C6 board (Nesso N1)
- Arduino IDE or PlatformIO
- [micro-ROS Arduino library](https://github.com/micro-ROS/micro_ros_arduino)
- [Arduino_BMI270_BMM150 library](https://github.com/arduino-libraries/Arduino_BMI270_BMM150)
- [Adafruit_ST7789 library](https://github.com/adafruit/Adafruit-ST7735-Library)

## Steps

### 1. Install Libraries
- In Arduino IDE, go to Tools > Manage Libraries and install:
  - micro_ros_arduino
  - Arduino_BMI270_BMM150
  - Adafruit_ST7789

### 2. micro-ROS Agent
- On your PC, install the micro-ROS agent:
  ```
  docker run -it --rm -p 8888:8888 microros/micro-ros-agent serial --dev <your_serial_port>
  ```
  Replace `<your_serial_port>` with your ESP32's port (e.g., /dev/ttyUSB0 or /dev/cu.usbserial-xxxx).

### 3. Example Sketch
- A complete, working sketch lives in
  [`microros_imu_publisher/microros_imu_publisher.ino`](microros_imu_publisher/microros_imu_publisher.ino).
  The essentials:

```cpp
#include <micro_ros_arduino.h>
#include <Wire.h>
#include <Arduino_BMI270_BMM150.h>   // provides the global IMU object

rcl_publisher_t imu_pub;
sensor_msgs__msg__Imu imu_msg;

void setup() {
  set_microros_transports();
  Wire.begin();
  IMU.begin();
  // micro-ROS node, publisher, executor setup here
}

void loop() {
  float gx, gy, gz, ax, ay, az;
  IMU.readGyroscope(gx, gy, gz);
  IMU.readAcceleration(ax, ay, az);
  imu_msg.angular_velocity.x = gx;
  imu_msg.angular_velocity.y = gy;
  imu_msg.angular_velocity.z = gz;
  imu_msg.linear_acceleration.x = ax;
  imu_msg.linear_acceleration.y = ay;
  imu_msg.linear_acceleration.z = az;
  // rcl_publish(&imu_pub, &imu_msg, NULL);
  delay(10);
}
```

- See the [micro-ROS Arduino examples](https://github.com/micro-ROS/micro_ros_arduino/tree/foxy/examples) for full node setup and publisher code.

### 4. Build and Upload
- Select your ESP32-C6 board and upload the sketch.

### 5. Test
- Use `ros2 topic echo /imu` on your ROS 2 PC to see IMU data.

---

## References
- [micro-ROS Zephyr Emulator Tutorial](https://micro.ros.org/docs/tutorials/core/zephyr_emulator/)
- [micro-ROS Arduino Library](https://github.com/micro-ROS/micro_ros_arduino)
- [DFRobot_BMI270](https://github.com/DFRobot/DFRobot_BMI270)

---

For a full, working sketch or further integration (display, buttons, etc.), let me know your requirements!