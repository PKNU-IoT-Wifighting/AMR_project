#!/usr/bin/env python3
"""Static Web HMI and HTTP-to-Nav2 bridge for GuideRobot."""

from __future__ import annotations

import argparse
import math
import os
import threading
from contextlib import asynccontextmanager
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import rclpy
import uvicorn
import yaml
from action_msgs.msg import GoalStatus
from fastapi import FastAPI, HTTPException
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from nav2_msgs.action import NavigateToPose
from pydantic import BaseModel
from rclpy.action import ActionClient
from rclpy.executors import MultiThreadedExecutor
from rclpy.node import Node
from rclpy.parameter import Parameter
from rclpy.signals import SignalHandlerOptions
from std_msgs.msg import Bool

from navigation_history import NavigationHistoryRepository


BASE_DIR = Path(__file__).resolve().parent
DEFAULT_CONFIG_PATH = BASE_DIR / "config.yaml"
DEFAULT_WEB_DIR = BASE_DIR.parent / "Jaewook" / "user-ui-template"
DEFAULT_DATABASE_PATH = BASE_DIR / "data" / "navigation.db"


@dataclass(frozen=True)
class Destination:
    name: str
    x: float
    y: float
    yaw: float


@dataclass(frozen=True)
class Settings:
    host: str
    port: int
    node_name: str
    action_name: str
    manual_mode_topic: str
    frame_id: str
    use_sim_time: bool
    destinations: dict[str, Destination]


def load_settings(path: Path) -> Settings:
    try:
        raw = yaml.safe_load(path.read_text(encoding="utf-8")) or {}
    except FileNotFoundError as exc:
        raise RuntimeError(f"설정 파일을 찾을 수 없습니다: {path}") from exc
    except yaml.YAMLError as exc:
        raise RuntimeError(f"YAML 설정을 읽을 수 없습니다: {exc}") from exc

    try:
        server = raw.get("server", {})
        ros = raw.get("ros", {})
        destinations = {
            destination_id: Destination(
                name=str(value["name"]),
                x=float(value["x"]),
                y=float(value["y"]),
                yaw=float(value["yaw"]),
            )
            for destination_id, value in raw["destinations"].items()
        }
        if not destinations:
            raise ValueError("목적지가 하나 이상 필요합니다")
        return Settings(
            host=str(server.get("host", "0.0.0.0")),
            port=int(server.get("port", 8080)),
            node_name=str(ros.get("node_name", "guiderobot_web_server")),
            action_name=str(ros.get("action_name", "/navigate_to_pose")),
            manual_mode_topic=str(ros.get("manual_mode_topic", "/manual_mode")),
            frame_id=str(ros.get("frame_id", "map")),
            use_sim_time=bool(ros.get("use_sim_time", False)),
            destinations=destinations,
        )
    except (KeyError, TypeError, ValueError) as exc:
        raise RuntimeError(f"설정 형식이 올바르지 않습니다: {exc}") from exc


class CommandRequest(BaseModel):
    destination: str | None = None
    command: str | None = None


class ManualModeRequest(BaseModel):
    manual_mode: bool


class NavigationBridge(Node):
    """Owns the Nav2 action client and state consumed by the HTTP API."""

    def __init__(
        self,
        settings: Settings,
        history_repository: NavigationHistoryRepository,
    ) -> None:
        super().__init__(
            settings.node_name,
            parameter_overrides=[
                Parameter("use_sim_time", Parameter.Type.BOOL, settings.use_sim_time)
            ],
        )
        self.settings = settings
        self._lock = threading.RLock()
        self._goal_handle: Any | None = None
        self._history_id: int | None = None
        self._manual_mode = False
        self._status = "idle"
        self._destination: str | None = None
        self._distance_remaining: float | None = None
        self._last_error: str | None = None
        self._action_client = ActionClient(
            self, NavigateToPose, settings.action_name
        )
        self._manual_mode_publisher = self.create_publisher(
            Bool, settings.manual_mode_topic, 10
        )
        # Repeat the latest state so a robot node that starts later also sees it.
        self._manual_mode_timer = self.create_timer(0.5, self._publish_manual_mode)
        self._history_repository = history_repository
        self.get_logger().info(
            f"Nav2 action={settings.action_name}, manual topic={settings.manual_mode_topic}"
        )

    def status_snapshot(self) -> dict[str, Any]:
        with self._lock:
            return {
                "status": self._status,
                "navigationStatus": self._status,
                "manual_mode": self._manual_mode,
                "destination": self._destination,
                "distance_remaining": self._distance_remaining,
                "nav2_available": self._action_client.server_is_ready(),
                "error": self._last_error,
            }

    def send_destination(self, destination_id: str) -> tuple[bool, str]:
        destination = self.settings.destinations.get(destination_id)
        if destination is None:
            return False, "등록되지 않은 목적지입니다."

        with self._lock:
            if self._manual_mode:
                return False, "관리자가 수동 제어 중입니다."
            if self._status in {"sending", "moving", "canceling"}:
                return False, "이미 안내가 진행 중입니다. 먼저 안내를 취소해 주세요."

        if not self._action_client.server_is_ready():
            return False, "Nav2 /navigate_to_pose 액션 서버에 연결되지 않았습니다."

        goal = NavigateToPose.Goal()
        goal.pose.header.frame_id = self.settings.frame_id
        goal.pose.header.stamp = self.get_clock().now().to_msg()
        goal.pose.pose.position.x = destination.x
        goal.pose.pose.position.y = destination.y
        goal.pose.pose.orientation.z = math.sin(destination.yaw / 2.0)
        goal.pose.pose.orientation.w = math.cos(destination.yaw / 2.0)

        with self._lock:
            self._status = "sending"
            self._destination = destination_id
            self._distance_remaining = None
            self._last_error = None

        future = self._action_client.send_goal_async(
            goal, feedback_callback=self._on_feedback
        )
        future.add_done_callback(self._on_goal_response)
        return True, destination.name

    def cancel_navigation(self) -> tuple[bool, str]:
        with self._lock:
            goal_handle = self._goal_handle
            if goal_handle is None:
                self._status = "idle"
                self._destination = None
                self._distance_remaining = None
                return True, "진행 중인 안내가 없습니다."
            self._status = "canceling"

        future = goal_handle.cancel_goal_async()
        future.add_done_callback(self._on_cancel_response)
        return True, "안내 취소 요청을 전달했습니다."

    def set_manual_mode(self, enabled: bool) -> None:
        """Apply a Qt HTTP request and publish the state for the robot."""
        with self._lock:
            changed = self._manual_mode != enabled
            self._manual_mode = enabled
            should_cancel = self._manual_mode and self._goal_handle is not None

        self._publish_manual_mode()
        if changed:
            self.get_logger().info(f"manual_mode={self._manual_mode}")
        if should_cancel:
            self.get_logger().warning("Manual mode enabled; canceling navigation")
            self.cancel_navigation()

    def _publish_manual_mode(self) -> None:
        with self._lock:
            enabled = self._manual_mode
        self._manual_mode_publisher.publish(Bool(data=enabled))

    def _on_goal_response(self, future: Any) -> None:
        try:
            goal_handle = future.result()
        except Exception as exc:  # rclpy future exceptions vary by middleware
            self._set_failure(f"목표 전송 오류: {exc}")
            return

        if not goal_handle.accepted:
            self._set_failure("Nav2가 목표를 거부했습니다.")
            return

        with self._lock:
            self._goal_handle = goal_handle
            self._status = "moving"
            destination_id = self._destination
            destination = (
                self.settings.destinations.get(destination_id)
                if destination_id is not None
                else None
            )
            if destination_id is not None and destination is not None:
                try:
                    self._history_id = self._history_repository.start(
                        destination_id, destination.name
                    )
                except Exception as exc:
                    self.get_logger().error(f"안내 시작 이력 저장 실패: {exc}")
        self.get_logger().info("Navigation goal accepted")
        result_future = goal_handle.get_result_async()
        result_future.add_done_callback(
            lambda completed, handle=goal_handle: self._on_result(completed, handle)
        )

        with self._lock:
            manual_mode = self._manual_mode
        if manual_mode:
            self.cancel_navigation()

    def _on_feedback(self, feedback_message: Any) -> None:
        distance = float(feedback_message.feedback.distance_remaining)
        with self._lock:
            self._distance_remaining = distance

    def _on_result(self, future: Any, goal_handle: Any) -> None:
        try:
            wrapped_result = future.result()
            result_status = wrapped_result.status
        except Exception as exc:
            self._set_failure(f"결과 수신 오류: {exc}")
            return

        with self._lock:
            if self._goal_handle is not goal_handle:
                return
            self._goal_handle = None
            history_id = self._history_id
            self._history_id = None
            self._distance_remaining = 0.0 if result_status == GoalStatus.STATUS_SUCCEEDED else None
            if result_status == GoalStatus.STATUS_SUCCEEDED:
                self._status = "arrived"
                self._last_error = None
            elif result_status == GoalStatus.STATUS_CANCELED:
                self._status = "canceled"
                self._destination = None
            else:
                self._status = "failed"
                self._last_error = f"Nav2 종료 상태 코드: {result_status}"
        if history_id is not None:
            try:
                self._history_repository.finish(history_id)
            except Exception as exc:
                self.get_logger().error(f"안내 종료 이력 저장 실패: {exc}")
        self.get_logger().info(f"Navigation finished with status={result_status}")

    def _on_cancel_response(self, future: Any) -> None:
        try:
            response = future.result()
            accepted = bool(response.goals_canceling)
        except Exception as exc:
            self._set_failure(f"취소 요청 오류: {exc}")
            return
        if not accepted:
            self._set_failure("Nav2가 취소 요청을 받아들이지 않았습니다.")

    def _set_failure(self, message: str) -> None:
        with self._lock:
            self._goal_handle = None
            history_id = self._history_id
            self._history_id = None
            self._status = "failed"
            self._distance_remaining = None
            self._last_error = message
        if history_id is not None:
            try:
                self._history_repository.finish(history_id)
            except Exception as exc:
                self.get_logger().error(f"안내 종료 이력 저장 실패: {exc}")
        self.get_logger().error(message)


class RosRuntime:
    def __init__(
        self,
        settings: Settings,
        history_repository: NavigationHistoryRepository,
    ) -> None:
        # Uvicorn owns SIGINT/SIGTERM. ROS is shut down from FastAPI's lifespan
        # so the executor thread can exit without a shutdown race.
        rclpy.init(args=None, signal_handler_options=SignalHandlerOptions.NO)
        self.node = NavigationBridge(settings, history_repository)
        self.executor = MultiThreadedExecutor(num_threads=2)
        self.executor.add_node(self.node)
        self.thread = threading.Thread(
            target=self.executor.spin, name="ros2-executor", daemon=True
        )
        self.thread.start()

    def close(self) -> None:
        self.executor.shutdown(timeout_sec=3.0)
        self.node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()
        self.thread.join(timeout=3.0)


def create_app(
    settings: Settings,
    web_dir: Path = DEFAULT_WEB_DIR,
    database_path: Path = DEFAULT_DATABASE_PATH,
) -> FastAPI:
    if not (web_dir / "index.html").is_file():
        raise RuntimeError(f"웹 UI를 찾을 수 없습니다: {web_dir}")

    runtime_holder: dict[str, RosRuntime] = {}
    history_repository = NavigationHistoryRepository(database_path)

    @asynccontextmanager
    async def lifespan(_: FastAPI):
        runtime_holder["runtime"] = RosRuntime(settings, history_repository)
        try:
            yield
        finally:
            runtime_holder.pop("runtime").close()

    app = FastAPI(title="GuideRobot Nav2 Server", lifespan=lifespan)

    def bridge() -> NavigationBridge:
        runtime = runtime_holder.get("runtime")
        if runtime is None:
            raise HTTPException(status_code=503, detail="ROS 2 서버가 준비되지 않았습니다.")
        return runtime.node

    @app.get("/api/status")
    def get_status() -> dict[str, Any]:
        return bridge().status_snapshot()

    @app.post("/api/command")
    def post_command(request: CommandRequest) -> dict[str, Any]:
        if request.destination and request.command:
            raise HTTPException(status_code=400, detail="destination과 command를 동시에 보낼 수 없습니다.")
        if request.destination:
            accepted, message = bridge().send_destination(request.destination)
            if not accepted:
                status_code = 409 if "진행 중" in message or "수동" in message else 503
                if "등록되지 않은" in message:
                    status_code = 400
                raise HTTPException(status_code=status_code, detail=message)
            return {"accepted": True, "destination": request.destination, "name": message}
        if request.command == "cancel":
            accepted, message = bridge().cancel_navigation()
            if not accepted:
                raise HTTPException(status_code=409, detail=message)
            return {"accepted": True, "command": "cancel", "message": message}
        raise HTTPException(status_code=400, detail="destination 또는 cancel command가 필요합니다.")

    @app.post("/api/manual-mode")
    def post_manual_mode(request: ManualModeRequest) -> dict[str, bool]:
        bridge().set_manual_mode(request.manual_mode)
        return {"manual_mode": request.manual_mode}

    @app.get("/", include_in_schema=False)
    def index() -> FileResponse:
        return FileResponse(web_dir / "index.html")

    app.mount("/", StaticFiles(directory=web_dir), name="web")
    return app


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="GuideRobot FastAPI/Nav2 server")
    parser.add_argument(
        "--config",
        type=Path,
        default=Path(os.environ.get("GUIDEROBOT_CONFIG", DEFAULT_CONFIG_PATH)),
    )
    parser.add_argument(
        "--web-dir",
        type=Path,
        default=Path(os.environ.get("GUIDEROBOT_WEB_DIR", DEFAULT_WEB_DIR)),
    )
    parser.add_argument(
        "--database",
        type=Path,
        default=Path(os.environ.get("GUIDEROBOT_DATABASE", DEFAULT_DATABASE_PATH)),
        help="SQLite 안내 이력 파일 경로",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    settings = load_settings(args.config.resolve())
    app = create_app(settings, args.web_dir.resolve(), args.database.resolve())
    uvicorn.run(app, host=settings.host, port=settings.port, log_level="info")


if __name__ == "__main__":
    main()
