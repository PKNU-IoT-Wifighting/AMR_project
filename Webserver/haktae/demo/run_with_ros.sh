#!/usr/bin/env bash

set -Eeuo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ros_setup_file="${ROS2_SETUP_FILE:-/opt/ros/jazzy/setup.bash}"
python_executable="${PYTHON_EXECUTABLE:-python3}"

if [[ ! -f "$ros_setup_file" ]]; then
    echo "ROS 2 setup file not found: $ros_setup_file" >&2
    exit 1
fi

# shellcheck disable=SC1090
source "$ros_setup_file"

export WEB_SERVER_URL="${WEB_SERVER_URL:-http://127.0.0.1:8080}"
export NAV_ACTION_STATUS_TOPIC="${NAV_ACTION_STATUS_TOPIC:-/navigate_to_pose/_action/status}"

"$python_executable" "$script_dir/ros/navigation_status_bridge.py" &
bridge_pid=$!

cleanup() {
    if kill -0 "$bridge_pid" 2>/dev/null; then
        kill "$bridge_pid" 2>/dev/null || true
        wait "$bridge_pid" 2>/dev/null || true
    fi
}

trap cleanup EXIT INT TERM

cd "$script_dir"

if [[ $# -gt 0 ]]; then
    "$@"
else
    ./gradlew bootRun
fi
