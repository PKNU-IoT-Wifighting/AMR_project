#!/usr/bin/env python3
"""Emit ROS robot connection and destination events as JSON lines."""

import json

import rclpy
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, QoSProfile, ReliabilityPolicy
from std_msgs.msg import String


class RobotStatusMonitor(Node):
    def __init__(self):
        super().__init__("manager_screen_status_monitor")
        self._last_connected = None
        destination_qos = QoSProfile(depth=1)
        destination_qos.reliability = ReliabilityPolicy.RELIABLE
        destination_qos.durability = DurabilityPolicy.TRANSIENT_LOCAL
        self.create_subscription(
            String,
            "/guide_robot/destination",
            self._on_destination,
            destination_qos,
        )
        self.create_timer(1.0, self._check_amr_node)
        self._check_amr_node()

    @staticmethod
    def _emit(payload):
        print(json.dumps(payload, ensure_ascii=False), flush=True)

    def _check_amr_node(self):
        connected = any(name == "amr_node" for name, _namespace in self.get_node_names_and_namespaces())
        if connected != self._last_connected:
            self._last_connected = connected
            self._emit({"robot_connected": connected})

    def _on_destination(self, message):
        self._emit({"destination": message.data})


def main():
    rclpy.init()
    node = RobotStatusMonitor()
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
