#!/usr/bin/env bash
set -eo pipefail

TEST_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SERVER_DIR="$TEST_DIR/../Server"
VENV_DIR="$TEST_DIR/.venv"

source /opt/ros/jazzy/setup.bash
set -u

required_packages=(nav2_bringup nav2_minimal_tb3_sim ros_gz_sim nav2_msgs)
for package in "${required_packages[@]}"; do
    if ! ros2 pkg prefix "$package" >/dev/null 2>&1; then
        echo "필요한 ROS 2 패키지가 없습니다: $package" >&2
        echo "다음을 설치하세요:" >&2
        echo "sudo apt install ros-jazzy-navigation2 ros-jazzy-nav2-bringup ros-jazzy-nav2-minimal-tb3-sim ros-jazzy-ros-gz" >&2
        exit 1
    fi
done

python3 -m venv --system-site-packages "$VENV_DIR"
"$VENV_DIR/bin/python" -m pip install --upgrade pip
"$VENV_DIR/bin/python" -m pip install -r "$SERVER_DIR/requirements.txt"

echo
echo "데모 환경 준비 완료"
echo "실행: $TEST_DIR/start_all.sh"
