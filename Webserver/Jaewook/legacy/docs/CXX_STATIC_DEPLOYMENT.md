# GuideRobot 정적 Web HMI Ubuntu 배포

## 구성

- C++ Robot API 서버: `http://192.168.0.9:8080`
- 정적 Web HMI: C++ 서버의 같은 8080 포트에서 제공
- 별도 .NET Runtime과 `GuideRobot.WebHmi.dll` 실행 불필요
- 별도 5000·7090 Web HMI 포트 불필요

## 배포 파일

`GuideRobot.StaticHmi.zip`에는 다음 파일만 포함됩니다.

- `index.html`
- `app.css`
- `app.js`
- `robot-mark.svg`
- `README_SERVER.md`

압축을 푼 뒤 C++ 서버의 정적 파일 디렉터리에 배치합니다.

## C++ 서버 정적 파일 라우팅

| 요청 경로 | 제공 파일 |
| --- | --- |
| `/` 또는 `/index.html` | `index.html` |
| `/app.css` | `app.css` |
| `/app.js` | `app.js` |
| `/robot-mark.svg` | `robot-mark.svg` |

적용 후 같은 네트워크의 PC·휴대폰·로봇 디스플레이에서 아래 주소로 접속합니다.

```text
http://192.168.0.9:8080/
```

## API 연동

브라우저는 웹을 제공한 서버의 상대 경로로 API를 호출합니다.

```http
GET /api/status
POST /api/command
```

따라서 웹 파일 안에 `192.168.0.9`나 좌표값을 저장하지 않습니다.

목적지 예시:

```json
{"destination":"room_301"}
```

안내 취소:

```json
{"command":"cancel"}
```

상태 응답 예시:

```json
{
  "manual_mode": false,
  "status": "idle"
}
```

- `manual_mode: true`: 관리자 제어 화면 표시 및 사용자 조작 잠금
- `manual_mode: false`: 관리자 제어 화면 해제
- 안내 중 `status: "arrived"`: 목적지 도착 화면 표시

상세 규격은 ZIP 내부 `README_SERVER.md`를 확인합니다.
