#!/usr/bin/env bash
set -eo pipefail

source /opt/ros/jazzy/setup.bash
set -u

# 기본값 false: Gazebo 창과 RViz를 모두 표시합니다.
# 창 없이 실행하려면 GUIDEROBOT_HEADLESS=true로 실행하세요.
case "${GUIDEROBOT_HEADLESS:-false}" in
    true|True|TRUE|1) HEADLESS="True" ;;
    *) HEADLESS="False" ;;
esac
case "${GUIDEROBOT_USE_RVIZ:-true}" in
    true|True|TRUE|1) USE_RVIZ="True" ;;
    *) USE_RVIZ="False" ;;
esac

exec ros2 launch nav2_bringup tb3_simulation_launch.py \
    headless:="$HEADLESS" \
    use_rviz:="$USE_RVIZ" \
    use_sim_time:=True \
    autostart:=True \
    slam:=False
