#!/usr/bin/env python3

import time

from action_msgs.srv import CancelGoal
from geometry_msgs.msg import Twist
import rclpy
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy
from std_msgs.msg import String


class SafetyWatchdog(Node):
    """Gate velocity commands using a heartbeat from the operator PC."""

    def __init__(self) -> None:
        super().__init__('operator_safety_watchdog')
        self.declare_parameter('heartbeat_topic', '/operator/heartbeat')
        self.declare_parameter('expected_operator_id', 'iot72@100.73.201.99')
        self.declare_parameter('cmd_vel_input_topic', '/cmd_vel_guard_input')
        self.declare_parameter('cmd_vel_output_topic', '/cmd_vel')
        self.declare_parameter('heartbeat_timeout_sec', 1.0)
        self.declare_parameter('cmd_vel_timeout_sec', 0.5)
        self.declare_parameter('recovery_hold_sec', 0.5)
        self.declare_parameter('control_rate_hz', 20.0)
        self.declare_parameter('cancel_navigation_on_timeout', True)

        self._expected_id = self.get_parameter('expected_operator_id').value
        self._heartbeat_timeout = float(
            self.get_parameter('heartbeat_timeout_sec').value
        )
        self._cmd_vel_timeout = float(
            self.get_parameter('cmd_vel_timeout_sec').value
        )
        self._recovery_hold = float(
            self.get_parameter('recovery_hold_sec').value
        )
        control_rate = float(self.get_parameter('control_rate_hz').value)
        self._cancel_navigation = bool(
            self.get_parameter('cancel_navigation_on_timeout').value
        )
        if min(self._heartbeat_timeout, self._cmd_vel_timeout, control_rate) <= 0.0:
            raise ValueError('timeouts and control_rate_hz must be greater than zero')
        if self._recovery_hold < 0.0:
            raise ValueError('recovery_hold_sec cannot be negative')

        heartbeat_qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE,
        )
        velocity_qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
        )

        heartbeat_topic = self.get_parameter('heartbeat_topic').value
        input_topic = self.get_parameter('cmd_vel_input_topic').value
        output_topic = self.get_parameter('cmd_vel_output_topic').value
        self._heartbeat_sub = self.create_subscription(
            String, heartbeat_topic, self._on_heartbeat, heartbeat_qos
        )
        self._velocity_sub = self.create_subscription(
            Twist, input_topic, self._on_velocity, velocity_qos
        )
        self._velocity_pub = self.create_publisher(Twist, output_topic, velocity_qos)

        self._cancel_clients = [
            self.create_client(CancelGoal, f'/{action}/_action/cancel_goal')
            for action in (
                'navigate_to_pose',
                'navigate_through_poses',
                'follow_waypoints',
            )
        ]
        self._zero = Twist()
        self._last_heartbeat = None
        self._last_velocity = None
        self._recovery_until = float('inf')
        self._link_active = False
        self._timer = self.create_timer(1.0 / control_rate, self._control_tick)

        self.get_logger().warning(
            f'Safety stop active: waiting for {self._expected_id!r} heartbeat '
            f'on {heartbeat_topic}'
        )
        self.get_logger().info(
            f'Velocity gate: {input_topic} -> {output_topic}; '
            f'heartbeat timeout={self._heartbeat_timeout:.2f} s'
        )

    def _on_heartbeat(self, message: String) -> None:
        if message.data != self._expected_id:
            self.get_logger().warning(
                f'Ignoring heartbeat from unexpected operator {message.data!r}',
                throttle_duration_sec=5.0,
            )
            return

        now = time.monotonic()
        was_stale = (
            self._last_heartbeat is None
            or now - self._last_heartbeat > self._heartbeat_timeout
        )
        self._last_heartbeat = now
        if was_stale:
            self._recovery_until = now + self._recovery_hold

    def _on_velocity(self, message: Twist) -> None:
        self._last_velocity = time.monotonic()
        if self._link_active:
            self._velocity_pub.publish(message)
        else:
            self._publish_stop()

    def _control_tick(self) -> None:
        now = time.monotonic()
        heartbeat_fresh = (
            self._last_heartbeat is not None
            and now - self._last_heartbeat <= self._heartbeat_timeout
        )
        motion_allowed = heartbeat_fresh and now >= self._recovery_until

        if motion_allowed and not self._link_active:
            self._link_active = True
            self.get_logger().info('Operator heartbeat restored; velocity gate enabled')
        elif not motion_allowed and self._link_active:
            self._link_active = False
            self.get_logger().error(
                'Operator ROS 2 heartbeat lost; stopping robot and cancelling Nav2 goals'
            )
            self._cancel_navigation_goals()

        velocity_fresh = (
            self._last_velocity is not None
            and now - self._last_velocity <= self._cmd_vel_timeout
        )
        if not self._link_active or not velocity_fresh:
            self._publish_stop()

    def _publish_stop(self) -> None:
        self._velocity_pub.publish(self._zero)

    def _cancel_navigation_goals(self) -> None:
        if not self._cancel_navigation:
            return

        request = CancelGoal.Request()
        for client in self._cancel_clients:
            if client.service_is_ready():
                client.call_async(request)


def main(args=None) -> None:
    rclpy.init(args=args)
    node = SafetyWatchdog()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
