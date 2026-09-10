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
