# GuideRobot FastAPI + ROS 2 서버

기존 `ManagerScreen`과 `Jaewook/user-ui-template`을 수정하지 않고 연결하는 서버입니다.

현재 PC에서만 사용하는 Gazebo·예제 지도·Nav2 검증 도구는 서버 배포 코드와
분리하여 `../Nav2Test`에 보관합니다. 실제 서버 컴퓨터에는 `Server`만 전달하면
됩니다.

## 연결 구조

- 웹 UI: `../Jaewook/user-ui-template`의 정적 파일을 `http://localhost:8080/`에서 제공
- 웹 명령: `POST /api/command`
- 웹 상태: `GET /api/status`
- Nav2: `/navigate_to_pose` (`nav2_msgs/action/NavigateToPose`) 액션 클라이언트
- Qt 수동 모드 요청: `POST /api/manual-mode`
- 안내로봇 수동 모드 알림: `/manual_mode` (`std_msgs/msg/Bool`) 발행

Qt가 서버에 수동 모드를 요청하면 서버가 상태를 저장하고 안내로봇에
`/manual_mode` 토픽을 발행합니다. 수동 모드를 켜면 웹 조작이 잠기며,
진행 중인 Nav2 안내도 취소됩니다. 로봇 노드가 나중에 실행되는 경우에도
상태를 받을 수 있도록 서버는 현재 값을 2 Hz로 반복 발행합니다.

## 1. 목적지 좌표 설정

`config.yaml`의 `x`, `y`, `yaw`는 예시값입니다. 실제 저장한 지도의 `map` 좌표로 반드시 변경하세요. `yaw`의 단위는 radian입니다.

## 2. 설치

ROS 2 Jazzy와 Nav2가 설치된 컴퓨터에서:

```bash
sudo apt install python3-pip python3-venv ros-jazzy-rclpy ros-jazzy-nav2-msgs
python3 -m pip install -r Server/requirements.txt
chmod +x Server/run.sh
```

Ubuntu의 externally-managed Python 오류가 발생하면 가상환경을 사용하되, ROS 2 Python 패키지를 볼 수 있도록 만듭니다.

```bash
python3 -m venv --system-site-packages Server/.venv
Server/.venv/bin/pip install -r Server/requirements.txt
```

이 경우 `run.sh`의 마지막 `python3`를 `"$SERVER_DIR/.venv/bin/python"`으로 바꾸거나 아래처럼 직접 실행합니다.

```bash
source /opt/ros/jazzy/setup.bash
Server/.venv/bin/python Server/server.py
```

## 3. 실행

Nav2를 먼저 실행한 후 새 터미널에서:

```bash
./Server/run.sh
```

브라우저에서 로컬 접속:

```text
http://localhost:8080/
```

같은 LAN의 다른 기기에서는 서버 컴퓨터의 IP를 사용합니다.

```text
http://서버컴퓨터IP:8080/
```

Qt 관리자 프로그램은 별도 프로세스로 실행하면 됩니다. Qt와 서버의 `ROS_DOMAIN_ID`가 같아야 합니다.

## 확인 명령

```bash
ros2 action list -t | grep navigate_to_pose
curl http://localhost:8080/api/status
curl -X POST http://localhost:8080/api/command \
  -H 'Content-Type: application/json' \
  -d '{"destination":"restroom"}'
curl -X POST http://localhost:8080/api/command \
  -H 'Content-Type: application/json' \
  -d '{"command":"cancel"}'
curl -X POST http://localhost:8080/api/manual-mode \
  -H 'Content-Type: application/json' \
  -d '{"manual_mode":true}'
```

Nav2가 실행되지 않았다면 목적지 요청은 HTTP 503을 반환합니다. 웹 화면과 상태 API는 계속 사용할 수 있습니다.

## 설정 변경

다른 설정 또는 웹 폴더를 사용할 수 있습니다.

```bash
./Server/run.sh --config /path/to/config.yaml --web-dir /path/to/web
```
