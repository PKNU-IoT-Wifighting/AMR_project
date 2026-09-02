#!/usr/bin/env bash
set -eo pipefail

TEST_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
LOG_FILE="$TEST_DIR/simulation.log"
SIM_PID=""

cleanup() {
    if [[ -n "$SIM_PID" ]] && kill -0 "$SIM_PID" 2>/dev/null; then
        echo
        echo "Gazebo/Nav2를 종료하는 중..."
        kill -INT "$SIM_PID" 2>/dev/null || true
        wait "$SIM_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

if [[ ! -x "$TEST_DIR/.venv/bin/python" ]]; then
    echo "가상환경이 없습니다. 먼저 $TEST_DIR/setup.sh 를 실행하세요." >&2
    exit 1
fi

source /opt/ros/jazzy/setup.bash
set -u

echo "Gazebo + TurtleBot3 + 예제 지도 + Nav2를 시작합니다."
echo "시뮬레이션 로그: $LOG_FILE"
"$TEST_DIR/start_simulation.sh" >"$LOG_FILE" 2>&1 &
SIM_PID=$!

echo -n "시뮬레이션 노드 대기 중"
action_ready=false
for _ in $(seq 1 90); do
    if ! kill -0 "$SIM_PID" 2>/dev/null; then
        echo
        echo "시뮬레이션이 비정상 종료되었습니다. 로그를 확인하세요: $LOG_FILE" >&2
        tail -n 40 "$LOG_FILE" >&2
        exit 1
    fi
    if ros2 action list -t 2>/dev/null | grep -q '^/navigate_to_pose '; then
        action_ready=true
        break
    fi
    echo -n "."
    sleep 1
done
echo

if [[ "$action_ready" != true ]]; then
    echo "90초 안에 시뮬레이션 노드가 준비되지 않았습니다: $LOG_FILE" >&2
    exit 1
fi

# AMCL must know the simulated robot's spawn pose before the navigation
# lifecycle manager can activate the global costmap.
echo "초기 위치 설정: x=-2.0, y=-0.5, yaw=0.0"
timeout 10s ros2 topic pub --once \
    /initialpose geometry_msgs/msg/PoseWithCovarianceStamped \
    "{header: {frame_id: map}, pose: {pose: {position: {x: -2.0, y: -0.5, z: 0.0}, orientation: {z: 0.0, w: 1.0}}, covariance: [0.25, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.25, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0685]}}" \
    >/dev/null

echo -n "Nav2 활성화 대기 중"
ready=false
for _ in $(seq 1 90); do
    if ! kill -0 "$SIM_PID" 2>/dev/null; then
        echo
        echo "시뮬레이션이 비정상 종료되었습니다: $LOG_FILE" >&2
        exit 1
    fi
    all_active=true
    for node in controller_server planner_server bt_navigator; do
        state="$(timeout 2s ros2 lifecycle get "/$node" 2>/dev/null || true)"
        if [[ "$state" != active* ]]; then
            all_active=false
            break
        fi
    done
    if [[ "$all_active" == true ]]; then
        ready=true
        break
    fi
    echo -n "."
    sleep 1
done
echo

if [[ "$ready" != true ]]; then
    echo "90초 안에 Nav2가 active 상태가 되지 않았습니다: $LOG_FILE" >&2
    tail -n 40 "$LOG_FILE" >&2
    exit 1
fi

echo "Nav2 준비 완료"
echo "웹 주소: http://localhost:8080/"
echo "종료: Ctrl+C"
echo

"$TEST_DIR/start_server.sh"
