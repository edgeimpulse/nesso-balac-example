# ROS 2 Camera-Based Robot Control with Raspberry Pi

This guide describes how to use a Raspberry Pi with a USB camera to monitor and control your Nesso N1 robot via ROS 2 and micro-ROS.

## Overview
- The Raspberry Pi runs a ROS 2 node that:
  - Captures video from a USB camera
  - Processes the video (e.g., detects the robot, tracks position, or interprets gestures)
  - Publishes control commands (e.g., geometry_msgs/Twist) to the robot over ROS 2
- The Nesso N1 runs micro-ROS, subscribes to the control topic, and drives its motors accordingly.

## Requirements
- Raspberry Pi (any model with USB and Wi-Fi)
- USB webcam
- ROS 2 installed (e.g., Humble, Foxy)
- Python 3 and OpenCV
- micro-ROS agent running (see micro-ROS docs)

## Example Workflow
1. **Start the micro-ROS agent** on the Pi:
   ```sh
   docker run -it --rm -p 8888:8888 microros/micro-ros-agent serial --dev /dev/ttyUSB0
   ```
   (Replace `/dev/ttyUSB0` with your Nesso N1's serial port)

2. **Run the camera control script** (see below)

3. **Robot receives commands** via micro-ROS and moves accordingly

---

# Example Python Script: Camera-Based ROS 2 Control

This script captures video, processes frames, and publishes Twist messages to control the robot.

---

## camera_control_node.py
```python
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import cv2

class CameraControlNode(Node):
    def __init__(self):
        super().__init__('camera_control_node')
        self.publisher_ = self.create_publisher(Twist, 'cmd_vel', 10)
        self.cap = cv2.VideoCapture(0)
        self.timer = self.create_timer(0.1, self.timer_callback)

    def timer_callback(self):
        ret, frame = self.cap.read()
        if not ret:
            self.get_logger().warning('No camera frame')
            return
        # --- Example: Simple color tracking (replace with your logic) ---
        # Here, we just send a forward command if a key is pressed
        key = cv2.waitKey(1) & 0xFF
        twist = Twist()
        if key == ord('w'):
            twist.linear.x = 0.2
        elif key == ord('s'):
            twist.linear.x = -0.2
        elif key == ord('a'):
            twist.angular.z = 0.5
        elif key == ord('d'):
            twist.angular.z = -0.5
        self.publisher_.publish(twist)
        cv2.imshow('Camera', frame)

    def destroy_node(self):
        self.cap.release()
        cv2.destroyAllWindows()
        super().destroy_node()

def main(args=None):
    rclpy.init(args=args)
    node = CameraControlNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
```

---

## Usage
1. Install dependencies:
   ```sh
   sudo apt install python3-colcon-common-extensions python3-opencv
   pip3 install rclpy geometry_msgs
   ```
2. Run the script:
   ```sh
   python3 camera_control_node.py
   ```
3. Use WASD keys to send commands, or replace the logic with your own vision-based control.

---

## Next Steps
- Replace the WASD logic with OpenCV-based robot detection or gesture control.
- On the robot, add a micro-ROS subscriber for `cmd_vel` and drive the motors accordingly.
