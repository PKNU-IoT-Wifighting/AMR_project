# GuideRobot 정적 Web HMI 서버 적용 안내

이 폴더는 .NET이나 별도 Web HMI 서버가 필요 없는 정적 웹 파일입니다.

## 파일

- `index.html`
- `app.css`
- `app.js`
- `robot-mark.svg`

C++ Robot API 서버가 위 파일을 정적 파일로 제공해야 합니다.

## 권장 URL

- 웹 화면: `GET http://192.168.0.9:8080/`
- 상태 API: `GET http://192.168.0.9:8080/api/status`
- 명령 API: `POST http://192.168.0.9:8080/api/command`

웹과 API가 같은 8080 포트를 사용하므로 별도 CORS 설정은 필요하지 않습니다.

## 정적 파일 라우팅

| 요청 경로 | 응답 파일 | Content-Type |
| --- | --- | --- |
| `/` 또는 `/index.html` | `index.html` | `text/html; charset=utf-8` |
| `/app.css` | `app.css` | `text/css; charset=utf-8` |
| `/app.js` | `app.js` | `text/javascript; charset=utf-8` |
| `/robot-mark.svg` | `robot-mark.svg` | `image/svg+xml` |

## 목적지 명령

기존 API 형식을 그대로 사용합니다.

```http
POST /api/command
Content-Type: application/json
```

```json
{"destination":"room_301"}
```

웹이 보내는 목적지 ID:

- `restroom`: 화장실 앞
- `room_301`: 강의실 301호
- `room_302`: 강의실 302호
- `elevator`: 엘리베이터 앞

좌표 조회와 ROS 2 목표 전달은 C++ 서버 내부에서 처리합니다.

안내 취소:

```json
{"command":"cancel"}
```

## 상태 응답

웹은 `GET /api/status`를 1초마다 호출합니다.

```json
{
  "manual_mode": false,
  "status": "idle"
}
```

- `manual_mode: true`: “관리자가 제어 중입니다” 전체 화면 표시
- `manual_mode: false`: 관리자 제어 화면 해제
- 안내 중 `status: "arrived"`: “목적지에 도착했습니다” 화면 표시

`camera_url`, `server`, `wheel_rpm` 등 다른 필드가 함께 있어도 문제없습니다.

## 서버 캐시 권장

개발·교체 중에는 `index.html`과 `app.js`에 `Cache-Control: no-cache`를 적용하면 새 UI 반영을 확인하기 쉽습니다.
