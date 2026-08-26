#!/usr/bin/env python3

import json
import os
import threading
from urllib.error import URLError
from urllib.request import Request, urlopen

import rclpy
from action_msgs.msg import GoalStatus, GoalStatusArray
from rclpy.node import Node
from rclpy.qos import qos_profile_action_status_default


class NavigationStatusBridge(Node):
    def __init__(self):
        super().__init__("navigation_status_bridge")
        self._server_url = os.getenv("WEB_SERVER_URL", "http://127.0.0.1:8080").rstrip("/")
        self._status_topic = os.getenv(
            "NAV_ACTION_STATUS_TOPIC",
            "/navigate_to_pose/_action/status",
        )
        self._reported_goal_id = None

        self.create_subscription(
            GoalStatusArray,
            self._status_topic,
            self._handle_status,
            qos_profile_action_status_default,
        )
        self.get_logger().info(
            f"Watching {self._status_topic} and reporting arrival to {self._server_url}"
        )

    @staticmethod
    def _stamp_key(status):
        stamp = status.goal_info.stamp
        return stamp.sec, stamp.nanosec

    @staticmethod
    def _goal_id(status):
        return bytes(status.goal_info.goal_id.uuid).hex()

    def _handle_status(self, message):
        if not message.status_list:
            return

        latest = max(message.status_list, key=self._stamp_key)
        goal_id = self._goal_id(latest)

        if latest.status in (
            GoalStatus.STATUS_ACCEPTED,
            GoalStatus.STATUS_EXECUTING,
            GoalStatus.STATUS_CANCELING,
        ):
            if goal_id != self._reported_goal_id:
                self._reported_goal_id = None
            return

        if latest.status != GoalStatus.STATUS_SUCCEEDED:
            return

        if goal_id == self._reported_goal_id:
            return

        self._reported_goal_id = goal_id
        threading.Thread(
            target=self._report_arrived,
            args=(goal_id,),
            daemon=True,
        ).start()

    def _report_arrived(self, goal_id):
        payload = json.dumps({"status": "arrived"}).encode("utf-8")
        request = Request(
            f"{self._server_url}/api/navigation/status",
            data=payload,
            headers={"Content-Type": "application/json"},
            method="POST",
        )

        try:
            with urlopen(request, timeout=3) as response:
                if 200 <= response.status < 300:
                    self.get_logger().info(f"Arrival reported for goal {goal_id}")
                    return
                raise RuntimeError(f"HTTP {response.status}")
        except (URLError, TimeoutError, RuntimeError) as error:
            self.get_logger().error(f"Failed to report arrival: {error}")
            self._reported_goal_id = None


def main(args=None):
    rclpy.init(args=args)
    node = NavigationStatusBridge()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
