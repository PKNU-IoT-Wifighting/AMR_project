#!/usr/bin/env bash
set -eo pipefail

source /opt/ros/jazzy/setup.bash
set -u

failed=0

if timeout 5s ros2 action list -t | grep -q '^/navigate_to_pose '; then
    echo "[OK] /navigate_to_pose 액션 발견"
else
    echo "[FAIL] /navigate_to_pose 액션 없음"
    failed=1
fi

for node in controller_server planner_server bt_navigator; do
    state="$(timeout 5s ros2 lifecycle get "/$node" 2>&1 || true)"
    if [[ "$state" == active* ]]; then
        echo "[OK] /$node: $state"
    else
        echo "[FAIL] /$node: $state"
        failed=1
    fi
done

if status="$(curl --fail --silent --max-time 3 http://localhost:8080/api/status)"; then
    echo "[OK] Web/API 서버: $status"
else
    echo "[FAIL] http://localhost:8080/api/status 접속 실패"
    failed=1
fi

exit "$failed"
