# hyunbeen 서버 사용자 UI

이 폴더는 Jaewook의 사용자 UI 디자인을 `hyunbeen` ROS 2 C++ 서버 API에 맞춰 적용한 실제 실행본입니다.

## 원본과 실행본

- 디자인 원본: `Webserver/Jaewook/user-ui-template`
- 서버 실행본: `Webserver/hyunbeen/web`

두 위치의 `index.html`, `app.css`, `app.js`, `robot-mark.svg`는 함께 변경하고 동일하게 유지합니다.

## 서버 API 규격

| 기능 | 규격 |
| --- | --- |
| 목적지 전송 | `POST /api/command` + `{"destination":"room_301"}` |
| 안내 취소 | `POST /api/command` + `{"command":"cancel"}` |
| 상태 확인 | `GET /api/status` |
| 수동모드 | `manual_mode: true/false` |
| 도착 상태 | `status: "arrived"` |

목적지 ID는 `restroom`, `room_301`, `room_302`, `elevator`를 사용합니다. 좌표는 웹이 아니라 C++ 서버의 `config/destinations.yaml`에서 관리합니다.

## 빌드와 실행

`colcon build`를 실행하면 CMake가 이 폴더를 ROS 2 패키지의 `share/guiderobot_server/web`에 함께 설치합니다.

```bash
cd Webserver/hyunbeen
colcon build --packages-select guiderobot_server
source install/setup.bash
ros2 launch guiderobot_server server.launch.py
```

실행 후 같은 네트워크에서 `http://서버주소:8080/`으로 접속합니다.

