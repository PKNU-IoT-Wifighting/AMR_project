# Raspberry Pi + STM32 분산 Nav2/SLAM

이 패키지는 다음 역할 분리를 전제로 한다.

- Raspberry Pi (ROS 2 Jazzy): micro-ROS Agent, LiDAR, encoder/IMU 융합,
  SLAM Toolbox, Nav2
- STM32: encoder 적산, IMU 읽기, 모터 속도 제어, micro-ROS UART 클라이언트
- PC (ROS 2 Jazzy): RViz2 표시, 초기 위치/목표 전송, 필요 시 CLI 목표 전송

## 데이터와 TF 구조

```text
STM32 /wheel/odom ─┐
                   ├─ robot_localization ─ /odom + odom→base_footprint
STM32 /imu/data_raw┘

LiDAR ─ /scan

SLAM Toolbox 또는 AMCL ─ map→odom
robot_state_publisher ─ base_footprint→base_link→laser/imu_link

Nav2 controller ─ cmd_vel_nav
  → velocity_smoother ─ cmd_vel_smoothed
  → collision_monitor ─ /cmd_vel
  → STM32 motor controller
```

`odom→base_footprint`은 Pi의 `robot_localization`만 발행해야 한다. STM32가 같은
TF를 발행하면 TF가 튀므로 STM32에서는 TF publisher를 만들지 않는다.

## 먼저 실측해서 바꿀 값

자율주행 전에 아래 값은 반드시 실측값으로 수정한다.

- `description/robot.urdf.xacro`: 차체 길이/폭/높이, 바퀴 반지름,
  좌우 바퀴 간 거리, LiDAR와 IMU 장착 위치/방향
- `config/nav2.yaml`: local/global costmap의 `footprint`, 최대 속도와 가감속
- `config/slam.yaml`: LiDAR 모델의 최소/최대 유효 거리
- STM32 펌웨어: 바퀴 반지름, 바퀴 간 거리, encoder CPR/PPR와 감속비

현재 값은 320 x 280 mm 정도의 저속 differential-drive 로봇을 위한 안전한
초기 예시일 뿐이다.

## 빌드

Pi에서:

```bash
cd ~/ros2_ws
source /opt/ros/jazzy/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
```

Jazzy용 Agent는 이 Pi의 `~/ros2_ws/micro_ros_ws`에 별도로 빌드되어 있다.
새 터미널에서 자동으로 source되며, 수동으로 확인할 때는 다음처럼 실행한다.

```bash
source /opt/ros/jazzy/setup.bash
source ~/ros2_ws/micro_ros_ws/install/local_setup.bash
source ~/ros2_ws/install/setup.bash
ros2 pkg executables micro_ros_agent
```

Agent가 아직 없어도 launch는 종료되지 않고 경고만 출력한다. 이 경우 STM32
토픽과 모터 명령만 연결되지 않는다.

## Pi UART와 LiDAR 장치

Pi의 `raspi-config`에서 serial login shell은 끄고 hardware UART는 켠다.
STM32 UART와 Pi UART는 3.3 V 신호, TX-RX 교차, 공통 GND로 연결한다. 사용자를
`dialout` 그룹에 추가한 뒤 다시 로그인하거나 재부팅한다.

```bash
sudo usermod -aG dialout "$USER"
ls -l /dev/serial0
```

현재 MCU는 Raspberry Pi Pico USB CDC로 확인되었으며, 재연결해도 바뀌지 않는
`/dev/serial/by-id/usb-Raspberry_Pi_Pico_D2639C77633C2924-if00`를 기본
장치로 사용한다. 기본 속도는 115200 baud다.

현재 A1M8 LiDAR는 다음 고정 경로를 기본 장치로 사용한다.

```bash
ls -l /dev/serial/by-id/usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_0001-if00-port0
```

다른 USB-UART 칩을 쓰면 vendor/product ID에 맞는 udev rule을 별도로 만든다.

## Pi와 PC의 ROS 2 통신

두 장비에서 ROS 2 Jazzy를 사용하고 같은 LAN과 같은 `ROS_DOMAIN_ID`를 쓴다.
각 터미널에서 다음 값을 동일하게 설정한다.

```bash
source /opt/ros/jazzy/setup.bash
export ROS_DOMAIN_ID=30
unset ROS_LOCALHOST_ONLY
export ROS_AUTOMATIC_DISCOVERY_RANGE=SUBNET
```

Pi에서는 이어서 두 workspace를 source한다.

```bash
source ~/microros_ws/install/local_setup.bash
source ~/ros2_ws/install/setup.bash
```

PC에서 `ros2 topic list`에 Pi 토픽이 안 보이면 서로 ping이 되는지, Wi-Fi AP의
client isolation이 꺼져 있는지, 방화벽이 DDS multicast/UDP를 막지 않는지부터
확인한다. 양쪽의 `RMW_IMPLEMENTATION`을 강제로 지정했다면 같은 구현을 쓴다.

## STM32 micro-ROS 메시지 계약

STM32가 발행:

| 토픽 | 타입 | 권장 주기 | 필수 frame |
|---|---|---:|---|
| `/wheel/odom` | `nav_msgs/msg/Odometry` | 30 Hz | header=`odom`, child=`base_footprint` |
| `/imu/data_raw` | `sensor_msgs/msg/Imu` | 50 Hz | header=`imu_link` |

STM32가 구독:

| 토픽 | 타입 | 주기 |
|---|---|---:|
| `/cmd_vel` | `geometry_msgs/msg/Twist` | 최대 20 Hz |

구현 조건:

1. Agent와 세션이 연결되면 `rmw_uros_sync_session()`으로 epoch를 동기화하고
   두 발행 메시지의 `header.stamp`를 Agent 시간으로 채운다.
2. encoder odometry는 m, rad, m/s, rad/s 단위로 계산한다. `pose.covariance`와
   `twist.covariance`를 0으로 두지 말고 실측 오차에 맞는 양수를 넣는다.
3. IMU 축은 REP-103 기준인 X 전방, Y 좌측, Z 위쪽(ENU), 각속도 rad/s,
   가속도 m/s²로 변환한다. 절대 orientation을 제공하지 않으면
   `orientation_covariance[0] = -1.0`으로 둔다.
4. `/cmd_vel`을 250 ms 이상 못 받거나 Agent 연결이 끊기면 PWM을 즉시 0으로
   만드는 독립 watchdog을 STM32에 둔다.
5. 좌우 목표 속도는
   `v_left = linear.x - angular.z * wheel_separation / 2`,
   `v_right = linear.x + angular.z * wheel_separation / 2`로 계산하고 각 바퀴에
   폐루프 PID를 적용한다.
6. 통신은 reliable QoS, depth 5 정도로 먼저 시작한다. 921600 baud에서 손실을
   확인한 뒤 IMU만 best-effort로 바꾸는 편이 안전하다.

encoder odometry의 한 샘플 이동량은 다음 식을 사용한다.

```text
d_left  = left_ticks  * 2π * wheel_radius / ticks_per_wheel_revolution
d_right = right_ticks * 2π * wheel_radius / ticks_per_wheel_revolution
d_center = (d_right + d_left) / 2
d_yaw    = (d_right - d_left) / wheel_separation
```

`ticks_per_wheel_revolution`에는 encoder quadrature 배수와 감속비를 모두
반영한다.

## SLAM 실행

Pi:

```bash
ros2 launch robot_bringup mapping.launch.py
```

LiDAR나 UART가 기본값과 다르면:

```bash
ros2 launch robot_bringup mapping.launch.py \
  lidar_launch:=sllidar_a1_launch.py \
  lidar_port:=/dev/ttyUSB0 lidar_baud:=115200 \
  agent_device:=/dev/ttyACM0 agent_baud:=115200
```

PC에는 `robot_bringup` 패키지를 같은 방식으로 빌드한 뒤:

```bash
source ~/ros2_ws/install/setup.bash
ros2 launch robot_bringup pc.launch.py
```

패키지를 PC에 복사하지 않았다면 아래 기본 Nav2 RViz launch도 쓸 수 있다.

```bash
ros2 launch nav2_bringup rviz_launch.py use_sim_time:=false
```

초기 맵을 만들 때는 키보드 teleop 등으로 천천히 주행한다. Nav2 goal은 이미
관측된 영역 안에서만 보낸다. 맵이 완성되면 Pi에서 저장한다.

```bash
mkdir -p ~/maps
ros2 run nav2_map_server map_saver_cli -f ~/maps/site
ros2 service call /slam_toolbox/serialize_map \
  slam_toolbox/srv/SerializePoseGraph "{filename: '/home/iot7272/maps/site'}"
```

첫 명령은 AMCL용 `site.yaml`/`site.pgm`, 두 번째는 나중에 SLAM을 이어서 할
pose graph를 저장한다.

## 저장한 맵으로 Navigation 실행

Pi:

```bash
ros2 launch robot_bringup navigation.launch.py \
  map:=/home/iot7272/maps/site.yaml
```

PC RViz에서 `2D Pose Estimate`로 로봇의 초기 위치와 방향을 지정한 다음
`Nav2 Goal`로 목표를 보낸다. 숫자 좌표로 보내려면 PC에서:

```bash
ros2 run robot_bringup send_goal 1.50 -0.40 90
```

인자는 `X[m] Y[m] YAW[deg]`이고 기본 frame은 `map`이다.

## 모터를 띄우기 전 검증

처음에는 바퀴를 지면에서 띄우고 순서대로 확인한다.

```bash
ros2 topic hz /scan
ros2 topic hz /wheel/odom
ros2 topic hz /imu/data_raw
ros2 topic echo /odom --once
ros2 run tf2_ros tf2_echo odom base_footprint
ros2 run tf2_ros tf2_echo base_link laser
ros2 topic echo /cmd_vel
```

손으로 로봇을 전진시켰을 때 `/wheel/odom`의 X가 증가하고, 반시계 회전할 때
yaw와 `angular.z`가 양수가 되는지 확인한다. 방향이 하나라도 반대면 SLAM을
시작하기 전에 encoder/IMU 축과 모터 방향을 수정한다.
