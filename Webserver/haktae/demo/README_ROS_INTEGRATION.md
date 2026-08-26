# ROS 2 취소·도착 연동

Spring Boot 서버는 사용자 UI의 안내 취소 요청과 ROS 2 도착 상태를 다음 방식으로 연결합니다.

## 안내 취소

사용자 UI가 다음 요청을 보냅니다.

```http
POST /api/command
Content-Type: application/json
```

```json
{"command":"cancel"}
```

서버는 설정된 `/cmd_vel` 토픽에 다음 `geometry_msgs/msg/Twist` 값을 한 번 발행합니다.

```yaml
linear:
  x: 0.0
  y: 0.0
  z: 0.0
angular:
  x: 0.0
  y: 0.0
  z: 0.0
```

토픽 이름은 환경변수로 변경할 수 있습니다.

```bash
export CMD_VEL_TOPIC=/cmd_vel
```

주의: 속도 0 발행은 로봇을 즉시 정지시키지만 Nav2 목표 자체를 취소하지는 않습니다. 로봇이 다시 속도 명령을 받을 수 있는 구성이라면 서버 담당자가 Nav2 액션 취소 기능도 추가해야 합니다.

## 도착 상태

`ros/navigation_status_bridge.py`는 Nav2 표준 액션 상태 토픽을 구독합니다.

기본 토픽:

```text
/navigate_to_pose/_action/status
```

가장 최근 목표가 `STATUS_SUCCEEDED`가 되면 서버에 다음 요청을 보냅니다.

```http
POST /api/navigation/status
Content-Type: application/json
```

```json
{"status":"arrived"}
```

서버의 `GET /api/status` 응답은 다음처럼 변경됩니다.

```json
{
  "status": "ok",
  "manualMode": false,
  "navigationStatus": "arrived"
}
```

사용자 UI는 이 값을 1초 안에 확인하고 도착 화면으로 전환합니다.

## Ubuntu 실행

권장 방법은 서버와 브리지를 실행 스크립트로 함께 시작하는 것입니다.

개발 실행:

```bash
cd Webserver/haktae/demo
bash run_with_ros.sh
```

이미 빌드한 JAR로 실행:

```bash
cd Webserver/haktae/demo
bash run_with_ros.sh java -jar build/libs/demo-0.0.1-SNAPSHOT.jar
```

스크립트는 다음 작업을 자동으로 수행합니다.

1. ROS 2 Jazzy 환경 불러오기
2. `navigation_status_bridge.py` 실행
3. Spring Boot 서버 실행
4. 서버 종료 시 브리지 함께 종료

서버와 브리지를 별도 터미널에서 실행하려면 Spring Boot 서버를 먼저 실행한 뒤 다음 명령을 사용합니다.

```bash
source /opt/ros/jazzy/setup.bash
cd Webserver/haktae/demo
python3 ros/navigation_status_bridge.py
```

서버 주소나 Nav2 상태 토픽이 다르면 환경변수로 변경합니다.

```bash
export WEB_SERVER_URL=http://127.0.0.1:8080
export NAV_ACTION_STATUS_TOPIC=/navigate_to_pose/_action/status
python3 ros/navigation_status_bridge.py
```

통합 실행 스크립트에서도 같은 환경변수를 사용할 수 있습니다.

```bash
export ROS2_SETUP_FILE=/opt/ros/jazzy/setup.bash
export WEB_SERVER_URL=http://127.0.0.1:8080
export NAV_ACTION_STATUS_TOPIC=/navigate_to_pose/_action/status
bash run_with_ros.sh
```

## 확인 명령

상태 확인:

```bash
curl http://127.0.0.1:8080/api/status
```

도착 보고 API 단독 확인:

```bash
curl -X POST http://127.0.0.1:8080/api/navigation/status \
  -H 'Content-Type: application/json' \
  -d '{"status":"arrived"}'
```

안내 취소 확인:

```bash
curl -X POST http://127.0.0.1:8080/api/command \
  -H 'Content-Type: application/json' \
  -d '{"command":"cancel"}'
```
