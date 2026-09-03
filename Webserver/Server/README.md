# GuideRobot FastAPI + ROS 2 서버

기존 `ManagerScreen`과 `Jaewook/user-ui-template`을 수정하지 않고 연결하는 서버입니다.

현재 PC에서만 사용하는 Gazebo·예제 지도·Nav2 검증 도구는 서버 배포 코드와
분리하여 `../Nav2Test`에 보관합니다. `Server`는 Nav2나 Gazebo를 실행하지 않고,
이미 실행된 `/navigate_to_pose` 액션 서버에 연결만 합니다. 실제 서버 컴퓨터에는
`Server`만 전달하면 됩니다.

## 연결 구조

- 웹 UI: `../Jaewook/user-ui-template`의 정적 파일을 `http://localhost:8080/`에서 제공
- 웹 명령: `POST /api/command`
- 웹 상태: `GET /api/status`
- Nav2: `/navigate_to_pose` (`nav2_msgs/action/NavigateToPose`) 액션 클라이언트
- Qt 수동 모드 요청: `POST /api/manual-mode`
- 안내로봇 수동 모드 알림: `/manual_mode` (`std_msgs/msg/Bool`) 발행
- 안내 이력: `data/navigation.db` SQLite 데이터베이스

Qt가 서버에 수동 모드를 요청하면 서버가 상태를 저장하고 안내로봇에
`/manual_mode` 토픽을 발행합니다. 수동 모드를 켜면 웹 조작이 잠기며,
진행 중인 Nav2 안내도 취소됩니다. 로봇 노드가 나중에 실행되는 경우에도
상태를 받을 수 있도록 서버는 현재 값을 2 Hz로 반복 발행합니다.

Nav2가 목적지 목표를 수락하여 실제 안내가 시작되면 `navigation_history`에
목적지 ID, 목적지 이름, 안내 시작 시간이 저장됩니다. 도착, 취소 또는 실패로
안내가 끝나면 같은 행에 안내 종료 시간이 저장됩니다. 시간은 한국 표준시
`Asia/Seoul`의 ISO 8601 형식(`+09:00`)입니다. 이전 버전에서 저장한 UTC 기록은
서버를 다시 시작할 때 같은 순간의 한국시간으로 자동 변환됩니다.

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

## Cloudflare 주소와 서버를 한 번에 실행

최초 한 번 `cloudflared`를 설치합니다.

```bash
sudo mkdir -p --mode=0755 /usr/share/keyrings
curl -fsSL https://pkg.cloudflare.com/cloudflare-main.gpg \
  | sudo tee /usr/share/keyrings/cloudflare-main.gpg >/dev/null
echo 'deb [signed-by=/usr/share/keyrings/cloudflare-main.gpg] https://pkg.cloudflare.com/cloudflared any main' \
  | sudo tee /etc/apt/sources.list.d/cloudflared.list
sudo apt-get update
sudo apt-get install -y cloudflared
chmod +x Server/run_with_cloudflare.sh
```

이후 다음 명령 하나로 서버와 임시 Cloudflare 주소를 함께 실행합니다.

```bash
./Server/run_with_cloudflare.sh
```

터미널에 `https://...trycloudflare.com` 주소가 출력됩니다. Quick Tunnel은
계정 없이 사용하는 테스트용 임시 주소이며 실행할 때마다 주소가 바뀝니다.
`Ctrl+C`를 누르면 Cloudflare Tunnel과 GuideRobot 서버가 함께 종료됩니다.
이 주소는 웹뿐 아니라 로봇 제어 API도 공개하므로 신뢰할 수 있는 사람에게만
공유해야 합니다.

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
./Server/run.sh --config /path/to/config.yaml --web-dir /path/to/web \
  --database /path/to/navigation.db
```

데이터베이스 경로는 `GUIDEROBOT_DATABASE` 환경 변수로도 지정할 수 있습니다.
기본 경로는 `Server/data/navigation.db`이며, 파일과 테이블은 서버 시작 시 자동으로
생성됩니다. 최근 이력은 다음과 같이 확인할 수 있습니다.

```bash
sqlite3 Server/data/navigation.db \
  'SELECT destination_name, started_at, ended_at FROM navigation_history ORDER BY id DESC;'
```
