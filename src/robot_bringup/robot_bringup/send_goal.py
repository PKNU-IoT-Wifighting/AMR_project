#!/usr/bin/env python3

import argparse
import math
import sys

from action_msgs.msg import GoalStatus
from nav2_msgs.action import NavigateToPose
import rclpy
from rclpy.action import ActionClient
from rclpy.node import Node


class GoalSender(Node):
    """Send one map-frame NavigateToPose goal and wait for its result."""

    def __init__(self, args: argparse.Namespace) -> None:
        super().__init__('send_navigation_goal')
        self._args = args
        self._client = ActionClient(self, NavigateToPose, 'navigate_to_pose')

    def run(self) -> int:
        timeout = self._args.server_timeout
        self.get_logger().info(
            f'Waiting up to {timeout:.1f} s for /navigate_to_pose...'
        )
        if not self._client.wait_for_server(timeout_sec=timeout):
            self.get_logger().error('Nav2 action server is not available.')
            return 2

        goal = NavigateToPose.Goal()
        goal.pose.header.frame_id = self._args.frame
        goal.pose.header.stamp = self.get_clock().now().to_msg()
        goal.pose.pose.position.x = self._args.x
        goal.pose.pose.position.y = self._args.y

        yaw = math.radians(self._args.yaw_deg)
        goal.pose.pose.orientation.z = math.sin(yaw / 2.0)
        goal.pose.pose.orientation.w = math.cos(yaw / 2.0)

        self.get_logger().info(
            f'Sending goal: frame={self._args.frame}, '
            f'x={self._args.x:.3f}, y={self._args.y:.3f}, '
            f'yaw={self._args.yaw_deg:.1f} deg'
        )
        send_future = self._client.send_goal_async(
            goal,
            feedback_callback=self._feedback_callback,
        )
        rclpy.spin_until_future_complete(self, send_future)
        goal_handle = send_future.result()
        if goal_handle is None or not goal_handle.accepted:
            self.get_logger().error('Goal was rejected by Nav2.')
            return 3

        result_future = goal_handle.get_result_async()
        rclpy.spin_until_future_complete(self, result_future)
        wrapped_result = result_future.result()
        if wrapped_result is None:
            self.get_logger().error('Goal ended without a result.')
            return 4

        if wrapped_result.status == GoalStatus.STATUS_SUCCEEDED:
            self.get_logger().info('Goal reached.')
            return 0

        error_msg = wrapped_result.result.error_msg or 'no Nav2 error message'
        self.get_logger().error(
            f'Navigation failed: action_status={wrapped_result.status}, '
            f'nav2_error={wrapped_result.result.error_code}, {error_msg}'
        )
        return 5

    def _feedback_callback(self, feedback_msg) -> None:
        feedback = feedback_msg.feedback
        self.get_logger().info(
            f'remaining={feedback.distance_remaining:.2f} m, '
            f'recoveries={feedback.number_of_recoveries}',
            throttle_duration_sec=1.0,
        )


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description='Send a Nav2 goal: X Y YAW_DEG, normally in the map frame.',
    )
    parser.add_argument('x', type=float, help='goal X coordinate in metres')
    parser.add_argument('y', type=float, help='goal Y coordinate in metres')
    parser.add_argument('yaw_deg', type=float, help='goal heading in degrees')
    parser.add_argument('--frame', default='map', help='goal frame (default: map)')
    parser.add_argument(
        '--server-timeout',
        type=float,
        default=10.0,
        help='seconds to wait for Nav2 (default: 10)',
    )
    return parser.parse_args(argv)


def main(args=None) -> None:
    cli_args = parse_args(sys.argv[1:] if args is None else args)
    rclpy.init(args=None)
    node = GoalSender(cli_args)
    try:
        exit_code = node.run()
    except KeyboardInterrupt:
        node.get_logger().warning('Interrupted by operator.')
        exit_code = 130
    finally:
        node.destroy_node()
        rclpy.shutdown()
    raise SystemExit(exit_code)


if __name__ == '__main__':
    main()
