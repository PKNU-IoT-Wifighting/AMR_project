# guiderobot_server

GuideRobot 프로젝트의 C++ Robot API 서버 (ROS2 패키지).

`SERVER_HANDOFF.md` / `README2.md`에서 정의한 API를 구현한다.

## 제공 기능

- `GET /api/status` → `{"status":"idle|moving|arrived","manual_mode":true|false}`
- `POST /api/command`
  - `{"destination":"room_301"}` → Nav2로 목적지 이동 시작
  - `{"command":"cancel"}` → 진행 중인 안내 취소
- 정적 파일 서빙: `/`, `/app.css`, `/app.js`, `/robot-mark.svg`
  (`Webserver/hyunbeen/web`의 UI를 빌드 시 패키지에 함께 설치)
- `/manual_mode` (std_msgs/Bool) 토픽 구독 → `manual_mode` 상태 반영, 수동모드 진입 시 안내 자동 취소
- Nav2 `navigate_to_pose` 액션 결과 구독 → 도착 시 `status: arrived`

## 빌드

```bash
# ros2 워크스페이스 src/ 아래에 위치시켰다고 가정
cd ~/ros2_ws
colcon build --packages-select guiderobot_server
source install/setup.bash
```

빌드 중 `FetchContent`가 cpp-httplib(github.com)을 자동으로 받아온다.
오프라인 환경이라면 미리 `~/.cache` 등에 clone해두고 `FetchContent_Declare`의
`GIT_REPOSITORY`를 로컬 경로로 바꿔도 된다.

## 실행

```bash
ros2 launch guiderobot_server server.launch.py
```

소스의 웹 폴더를 지정해 노드를 직접 실행하려면:

```bash
ros2 run guiderobot_server guiderobot_server_node \
  --ros-args -p static_web_root:=/path/to/AMR_project/Webserver/hyunbeen/web -p http_port:=8080
```

## 반드시 채워야 할 것 (TODO)

1. **`config/destinations.yaml`의 좌표** — 지금은 전부 0,0 placeholder.
   팀원 A/B가 만든 map 기준 실제 좌표로 채워야 목적지 이동이 동작한다.
2. **JSON 파싱** — 지금은 의존성을 늘리지 않으려고 아주 단순한 문자열 파싱만 사용.
   API가 더 복잡해지면 `nlohmann/json` 도입을 권장 (역시 FetchContent로 추가 가능).
3. **동시성 검증** — 현재 구조는 HTTP 스레드에서 `send_navigation_goal`을 호출해
   액션 클라이언트에 접근한다. rclcpp_action::Client의 호출 자체는 스레드 세이프하지만,
   콜백(`result_callback`)은 `rclcpp::spin()`이 도는 스레드에서 실행되므로 콜백 안에서
   상태를 바꿀 때는 항상 `RobotState`의 mutex를 통해서만 접근해야 한다 (이미 그렇게 구현됨).
4. **저전력/오류 상태(FR-29, FR-33)** 등 요구사항서에 있는 추가 상태는
   현재 버전에는 없음. 필요하면 `RobotState`에 필드 추가 + `/api/status` 응답에 포함.

## 관리자 화면(Qt)과의 연결 확인 방법

Qt `ManagerScreen`은 환경변수 `MANAGER_SERVER_URL`(기본 `http://127.0.0.1:8080`)로
이 서버에 접속하고, 5초마다 `GET /api/status`로 연결 상태를 확인한다.
수동모드 토글 시 `/manual_mode` 토픽을 직접 발행하므로, 이 서버가 ROS2 노드로
정상 기동되어 해당 토픽을 구독하고 있어야 Qt 화면의 "관리자가 제어 중입니다"
전체화면 잠금이 웹 HMI에도 반영된다.
