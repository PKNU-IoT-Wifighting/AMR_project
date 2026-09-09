#!/usr/bin/env python3
"""Persistent low-latency Twist publisher controlled through stdin."""

import sys
import threading

import rclpy
from geometry_msgs.msg import Twist
from rclpy.node import Node


class VelocityPublisher(Node):
    def __init__(self, topic):
        super().__init__("manager_screen_velocity_publisher")
        self.publisher = self.create_publisher(Twist, topic, 10)
        self.lock = threading.Lock()
        self.pending_velocity = None
        self.create_timer(0.05, self.publish_pending)

    def set_velocity(self, linear_x, angular_z):
        with self.lock:
            self.pending_velocity = (linear_x, angular_z)
        self.publish_pending()

    def publish_pending(self):
        if self.publisher.get_subscription_count() == 0:
            return
        with self.lock:
            velocity = self.pending_velocity
            self.pending_velocity = None
        if velocity is None:
            return
        message = Twist()
        message.linear.x = velocity[0]
        message.angular.z = velocity[1]
        self.publisher.publish(message)


def read_commands(node):
    for line in sys.stdin:
        try:
            linear_x, angular_z = map(float, line.split())
            node.set_velocity(linear_x, angular_z)
        except ValueError:
            continue


def main():
    rclpy.init()
    topic = sys.argv[1] if len(sys.argv) > 1 else "/cmd_vel"
    node = VelocityPublisher(topic)
    threading.Thread(target=read_commands, args=(node,), daemon=True).start()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == "__main__":
    main()
