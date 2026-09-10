// micro-ROS IMU Publisher Example for Nesso N1 (ESP32-C6)
// Adapted from Zephyr/Arduino micro-ROS tutorials
// Requires: micro_ros_arduino, DFRobot_BMI270

/*
  Arduino BMI270 - Simple Accelerometer & micro-ROS Publisher

  This example reads acceleration and gyroscope values from the BMI270 sensor
  and publishes them as ROS 2 IMU messages using micro-ROS.
  It can also print values to the Serial Monitor or Serial Plotter for debugging.

  The circuit:
  - Adapted for Arduino Nesso N1 (ESP32-C6)

  created 10 Jul 2019
  by Riccardo Rizzo
  Adapted for Nesso N1 and micro-ROS by Eoin Jordan, 2026

  This example code is in the public domain.
*/

#include <micro_ros_arduino.h>
#include <Wire.h>
#include <Arduino_BMI270_BMM150.h>

#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <sensor_msgs/msg/imu.h>
#include <geometry_msgs/msg/twist.h>

rcl_publisher_t imu_pub;
sensor_msgs__msg__Imu imu_msg;
rclc_support_t support;
rcl_node_t node;
rcl_allocator_t allocator;
rclc_executor_t executor;

// Command message storage for the /cmd_vel subscription (must persist).
geometry_msgs__msg__Twist twist_msg;

// Motor control variables (update as needed for your hardware)
float left_motor_cmd = 0.0;
float right_motor_cmd = 0.0;

#define MOTOR_I2C_ADDR 0x38

void error_loop() {
  while (1) {
    delay(100);
  }
}

void twist_callback(const void * msgin) {
  const geometry_msgs__msg__Twist * msg = (const geometry_msgs__msg__Twist *)msgin;
  // Example: differential drive mapping
  float linear = msg->linear.x;
  float angular = msg->angular.z;
  left_motor_cmd = linear - angular;
  right_motor_cmd = linear + angular;
  // TODO: Map to your motor driver, e.g. drvMotorL/R(left_motor_cmd, right_motor_cmd)
}

rcl_subscription_t twist_sub;

void drvMotor(byte ch, int8_t sp) {
  Wire.beginTransmission(MOTOR_I2C_ADDR);
  Wire.write(ch);
  Wire.write(sp);
  Wire.endTransmission();
}

void drvMotorL(float cmd) {
  // Clamp and scale from -1.0..1.0 to -127..127
  int8_t pwm = (int8_t)(fmax(-1.0, fmin(1.0, cmd)) * 127);
  drvMotor(0, pwm);
}

void drvMotorR(float cmd) {
  int8_t pwm = (int8_t)(fmax(-1.0, fmin(1.0, cmd)) * 127);
  drvMotor(1, -pwm); // Negate for correct direction
}

void setup() {
  set_microros_transports();
  Wire.begin();
  if (!IMU.begin()) error_loop();

  allocator = rcl_get_default_allocator();
  rclc_support_init(&support, 0, NULL, &allocator);
  rclc_node_init_default(&node, "nesso_n1_node", "", &support);
  rclc_publisher_init_default(
    &imu_pub,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Imu),
    "imu"
  );
  rclc_subscription_init_default(
    &twist_sub,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
    "cmd_vel"
  );
  rclc_executor_init(&executor, &support.context, 1, &allocator);
  rclc_executor_add_subscription(&executor, &twist_sub, &twist_msg, &twist_callback, ON_NEW_DATA);
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

  rcl_publish(&imu_pub, &imu_msg, NULL);
  // Drive motors with latest ROS command
  drvMotorL(left_motor_cmd);
  drvMotorR(right_motor_cmd);
  rclc_executor_spin_some(&executor, 10);
  delay(10);
}
