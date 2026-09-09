#!/usr/bin/env python3

import rclpy
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy
from std_msgs.msg import String


class OperatorHeartbeat(Node):
    """Publish a lightweight heartbeat from the operator PC."""

    def __init__(self) -> None:
        super().__init__('operator_heartbeat')
        self.declare_parameter('topic', '/operator/heartbeat')
        self.declare_parameter('operator_id', 'iot72@100.73.201.99')
        self.declare_parameter('rate_hz', 5.0)

        topic = self.get_parameter('topic').value
        self._operator_id = self.get_parameter('operator_id').value
        rate_hz = float(self.get_parameter('rate_hz').value)
        if rate_hz <= 0.0:
            raise ValueError('rate_hz must be greater than zero')

        qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE,
        )
        self._publisher = self.create_publisher(String, topic, qos)
        self._message = String(data=self._operator_id)
        self._timer = self.create_timer(1.0 / rate_hz, self._publish)
        self.get_logger().info(
            f'Publishing operator heartbeat as {self._operator_id!r} '
            f'on {topic} at {rate_hz:.1f} Hz'
        )

    def _publish(self) -> None:
        self._publisher.publish(self._message)


def main(args=None) -> None:
    rclpy.init(args=args)
    node = OperatorHeartbeat()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
