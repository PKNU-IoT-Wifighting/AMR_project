#!/usr/bin/env bash
set -eo pipefail

SERVER_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

source /opt/ros/jazzy/setup.bash
set -u

# 추가로 빌드한 ROS 2 워크스페이스가 있다면 실행 전에 source 하세요.
# 예: source /path/to/ros2_ws/install/setup.bash

exec python3 "$SERVER_DIR/server.py" "$@"
