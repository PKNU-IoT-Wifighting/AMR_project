# guiderobot_server — 진행 상황 (2026-08-27 기준)

GuideRobot 프로젝트의 C++ Robot API 서버. Qt 관리자 화면과 웹 사용자 HMI가
공통으로 호출하는 REST API + 정적 웹 파일 서빙 + ROS2 연동을 한 프로세스에서 담당한다.

담당: 형진 (서버)

---

## ✅ 지금까지 실제로 확인된 것

우분투 서버(`192.168.0.9`)에서 아래 항목들 curl / ros2 CLI로 직접 실행해서 확인 완료:

| 기능 | 상태 | 확인 방법 |
|---|---|---|
| 정적 웹 파일 서빙 (`GET /`) | ✅ | `curl http://192.168.0.9:8080/` → index.html 정상 반환 |
| `GET /api/status` | ✅ | `{"status":"idle","manual_mode":false}` 형식대로 응답 |
| `/manual_mode` ROS2 토픽 연동 | ✅ | 토픽 발행 → `/api/status`에 즉시(1초 이내) 반영, true/false 양방향 확인 |
| `POST /api/command {"command":"cancel"}` | ✅ | `{"success":true}` 응답 |
| `POST /api/command {"destination":"..."}` 에러 처리 | ✅ | Nav2 없을 때 `503 nav2_unavailable`, 잘못된 ID면 `400 invalid_destination`으로 구분 |

**아직 확인 안 된 것**
- `POST /api/command {"destination":"..."}` 실제 성공 케이스 (Nav2 필요)
- 목적지 도착 시 `status: arrived` 전환 (Nav2 + 실좌표 필요)
- 실제 Qt `ManagerScreen` 앱을 이 서버에 붙여서 하는 end-to-end 테스트 (지금까지는 curl/ros2 CLI로 흉내만 냄)
- 실제 웹 브라우저로 접속해서 화면 전환(잠금 화면 등) 확인 (지금까지는 curl로 응답값만 확인)

---

## 실행 방법 (팀원 공통)

### 1. 워크스페이스 구조

```
<워크스페이스 루트>/
└── src/
    └── guiderobot_server/   <- 이 패키지
```

`colcon build`는 반드시 `src/`의 **부모 디렉터리**(워크스페이스 루트)에서 실행해야 한다.

### 2. 빌드

```bash
cd <워크스페이스 루트>
colcon build --packages-select guiderobot_server
source install/setup.bash
```

빌드 중 `cpp-httplib`을 GitHub에서 자동으로 받아오므로 인터넷 연결이 필요하다.
빌드가 실패하고 `CMake Error: The source directory ... does not exist` 같은 메시지가 뜨면
이전에 다른 경로에서 빌드한 캐시가 남아있는 것이니 아래처럼 지우고 다시 빌드한다.

```bash
rm -rf build install log
colcon build --packages-select guiderobot_server
```

### 3. 실행

```bash
ros2 run guiderobot_server guiderobot_server_node \
  --ros-args -p static_web_root:=/home/ubuntu/webfolder/AMR_project/Webserver/Jaewook/GuideRobot/GuideRobot.StaticHmi
```

`static_web_root`는 `GuideRobot.StaticHmi`(index.html, app.css, app.js, robot-mark.svg가 있는 폴더) 실제 경로로 지정한다.

정상 기동 시 로그:

```
[INFO] [...] [guiderobot_server_node]: GuideRobot server node started.
[INFO] [...] [guiderobot_server_node]: HTTP 서버 시작: 0.0.0.0:8080 (정적 파일 루트: ...)
```

### 4. 접속 주소

- **같은 네트워크 내 어디서든**: `http://192.168.0.9:8080/`
- 방화벽(`ufw`)은 꺼져있는 상태(`inactive`)라 별도 포트 개방 작업 불필요.

---

## API 스펙 (구현 완료)

### `GET /api/status`

```json
{"status": "idle", "manual_mode": false}
```

- `status`: `idle` | `moving` | `arrived`
- `manual_mode`: JSON boolean

### `POST /api/command`

목적지 이동:
```json
{"destination": "room_301"}
```
목적지 ID: `restroom`, `room_301`, `room_302`, `elevator`

응답:
- 성공: `200 {"success": true}`
- 목적지 ID 자체가 틀림: `400 {"error": "invalid_destination"}`
- Nav2가 아직 안 떠 있음: `503 {"error": "nav2_unavailable"}`

안내 취소:
```json
{"command": "cancel"}
```
응답: `200 {"success": true}`

### `/manual_mode` (ROS2 토픽, std_msgs/Bool)

Qt 관리자 화면이 이 토픽으로 직접 발행한다 (HTTP API 아님). 서버가 구독해서
`GET /api/status`의 `manual_mode`에 반영하고, `true`가 들어오면 진행 중인 안내를 자동 취소한다.

테스트 방법:
```bash
ros2 topic pub /manual_mode std_msgs/msg/Bool "{data: true}" --once
```

---

## Qt / 웹 팀 연동 시 참고사항

- **Qt 관리자 화면(`ManagerScreen`)**: 환경변수 `MANAGER_SERVER_URL`로 이 서버 주소를 지정해서 실행
  ```bash
  MANAGER_SERVER_URL=http://192.168.0.9:8080 ./ManagerScreen
  ```
  5초마다 `GET /api/status`로 연결 상태 확인함. 수동 이동 토글 시 `/manual_mode` 토픽 발행 + `POST /api/command {"command":"stop"}` 호출.

- **웹 HMI**: 정적 파일이 이 서버에서 직접 서빙되므로 별도 서버/포트 필요 없음.
  `http://192.168.0.9:8080/`로 접속하면 바로 화면 나옴. API는 상대경로(`/api/status`, `/api/command`) 사용.

---

## 남은 작업 (TODO)

1. **`config/destinations.yaml` 좌표 채우기** — 현재 전부 `x:0, y:0` placeholder.
   지도 담당(팀원 A/B)에게 4개 목적지(restroom, room_301, room_302, elevator)의
   map 프레임 기준 좌표(x, y, yaw) 받아서 채워야 실제 목적지 이동이 동작함.
2. **Nav2 연동 테스트** — Nav2가 뜬 상태(시뮬레이션 또는 실로봇)에서
   `POST /api/command {"destination":"room_301"}` 실제 성공 케이스 확인 필요.
3. **Qt 앱 실물 연동 테스트** — 지금까지는 curl/ros2 CLI로 서버 단독 테스트만 함.
   실제 `ManagerScreen` 빌드본으로 이 서버에 붙여서 확인 필요 (팀원 D 협업 필요).
4. **웹 브라우저 실물 확인** — `http://192.168.0.9:8080/`을 실제 브라우저로 열어서
   목적지 선택 → 안내 시작 → 수동모드 잠금화면 → 도착화면까지 화면 전환 확인.

---

## 문의

서버 관련 이슈나 API 스펙 변경 필요하면 형진한테 연락.
