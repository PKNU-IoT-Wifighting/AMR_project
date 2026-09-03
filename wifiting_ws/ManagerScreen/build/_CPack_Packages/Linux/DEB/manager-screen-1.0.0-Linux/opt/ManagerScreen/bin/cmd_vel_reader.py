#!/usr/bin/env python3
"""Subscribe to /cmd_vel_watchdog_input and print velocity commands."""

import rclpy
from rclpy.executors import ExternalShutdownException
from geometry_msgs.msg import Twist
from rclpy.node import Node


class CmdVelReader(Node):
    def __init__(self):
        super().__init__("cmd_vel_reader")
        self.subscription = self.create_subscription(
            Twist,
            "/cmd_vel_watchdog_input",
            self.on_cmd_vel,
            10,
        )

    def on_cmd_vel(self, message):
        self.get_logger().info(
            "linear=(x=%.3f, y=%.3f, z=%.3f), "
            "angular=(x=%.3f, y=%.3f, z=%.3f)"
            % (
                message.linear.x,
                message.linear.y,
                message.linear.z,
                message.angular.x,
                message.angular.y,
                message.angular.z,
            )
        )


def main():
    rclpy.init()
    node = CmdVelReader()
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == "__main__":
    main()
