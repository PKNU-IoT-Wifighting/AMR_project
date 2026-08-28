#!/usr/bin/env python3

import rclpy
from rclpy.node import Node

from nav_msgs.msg import Odometry
from geometry_msgs.msg import TransformStamped
from tf2_ros import TransformBroadcaster


class OdomTfNode(Node):

    def __init__(self):
        super().__init__('odom_tf_node')

        self.declare_parameter('odom_topic', '/wheel/odom_raw')
        self.declare_parameter('odom_frame', 'odom')
        self.declare_parameter('base_frame', 'base_link')

        self.odom_topic = (
            self.get_parameter('odom_topic')
            .get_parameter_value()
            .string_value
        )

        self.odom_frame = (
            self.get_parameter('odom_frame')
            .get_parameter_value()
            .string_value
        )

        self.base_frame = (
            self.get_parameter('base_frame')
            .get_parameter_value()
            .string_value
        )

        self.tf_broadcaster = TransformBroadcaster(self)

        self.subscription = self.create_subscription(
            Odometry,
            self.odom_topic,
            self.odom_callback,
            20
        )

        self.get_logger().info(
            f'Subscribing to {self.odom_topic}'
        )

        self.get_logger().info(
            f'Broadcasting TF: {self.odom_frame} -> {self.base_frame}'
        )

    def odom_callback(self, msg: Odometry):

        transform = TransformStamped()

        # 오도메트리 메시지의 timestamp 사용
        transform.header.stamp = msg.header.stamp

        # 프레임 이름은 강제로 통일
        transform.header.frame_id = self.odom_frame
        transform.child_frame_id = self.base_frame

        # Odometry pose를 TF translation으로 복사
        transform.transform.translation.x = msg.pose.pose.position.x
        transform.transform.translation.y = msg.pose.pose.position.y
        transform.transform.translation.z = msg.pose.pose.position.z

        # Odometry orientation을 TF rotation으로 복사
        transform.transform.rotation.x = msg.pose.pose.orientation.x
        transform.transform.rotation.y = msg.pose.pose.orientation.y
        transform.transform.rotation.z = msg.pose.pose.orientation.z
        transform.transform.rotation.w = msg.pose.pose.orientation.w

        self.tf_broadcaster.sendTransform(transform)


def main(args=None):
    rclpy.init(args=args)

    node = OdomTfNode()

    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()

        if rclpy.ok():
            rclpy.shutdown()


if __name__ == '__main__':
    main()
