# 재사용 가능한 사용자 UI 템플릿

이 폴더에는 안내 로봇 사용자 디스플레이의 정적 웹 원본이 들어 있습니다. 특정 웹 프레임워크나 별도 UI 서버가 필요하지 않아 FastAPI, Spring Boot, C++, Node.js 등 정적 파일을 제공할 수 있는 서버에서 사용할 수 있습니다.

현재 팀 프로젝트에서는 `Webserver/Server/server.py`가 이 폴더를 직접 제공하므로 이곳이 **디자인 원본이자 실제 실행본**입니다.

## 파일 설명

| 파일 | 역할 | 공부할 내용 |
| --- | --- | --- |
| `index.html` | 목적지 선택, 안내 중, 도착, 관리자 제어, 다른 사용자 사용 중 팝업의 구조와 문구 | 태그, `id`, `class`, 접근성 속성 |
| `app.css` | 색상, 크기, 배치, 1024×600 및 모바일 반응형 디자인 | 선택자, Grid, Flex, 미디어 쿼리 |
| `app.js` | 목적지 선택, 서버 요청, 안내 취소, 수동모드, 도착 및 동시 사용자 처리 | 상태 객체, 이벤트, `fetch`, 폴링, 예외 처리 |
| `robot-mark.svg` | 화면과 브라우저 아이콘에 사용하는 로봇 이미지 | SVG 이미지 사용법 |

## 현재 서버에서 실행하는 방법

Ubuntu에서 Nav2를 먼저 실행한 뒤 프로젝트 루트에서 다음 명령을 실행합니다.

```bash
./Webserver/Server/run.sh
```

브라우저 접속 주소:

```text
같은 서버 컴퓨터: http://localhost:8080/
같은 네트워크의 다른 기기: http://서버컴퓨터IP:8080/
```

서버는 기본값으로 이 `user-ui-template` 폴더를 직접 읽습니다. 다른 정적 웹 폴더를 시험할 때만 `--web-dir` 옵션을 사용합니다.

## 다른 서버에 적용하는 방법

1. 이 폴더의 `index.html`, `app.css`, `app.js`, `robot-mark.svg`를 새 서버의 정적 파일 폴더에 복사합니다.
2. 서버가 `/`에서 `index.html`을 제공하도록 설정합니다.
3. 아래 API를 같은 서버 주소에서 제공하거나 `app.js`의 상대 요청 경로를 새 서버 규격에 맞게 변경합니다.
4. 목적지 전송, 안내 취소, 수동모드, 도착 상태, 동시 사용자 충돌을 실제 서버와 확인합니다.

Spring Boot에서는 일반적으로 다음 위치에 네 파일을 넣습니다.

```text
src/main/resources/static/
```

## 필요한 API 규격

### 목적지 전송

```http
POST /api/command
Content-Type: application/json
```

```json
{"destination":"room_301"}
```

사용하는 목적지 ID:

| ID | 화면 표시 |
| --- | --- |
| `restroom` | 화장실 앞 |
| `room_301` | 강의실 301호 |
| `room_302` | 강의실 302호 |
| `elevator` | 엘리베이터 앞 |

웹은 목적지 ID만 전송합니다. 좌표와 ROS 목표 처리는 서버가 담당합니다.

### 안내 취소

```http
POST /api/command
Content-Type: application/json
```

```json
{"command":"cancel"}
```

### 상태 확인

```http
GET /api/status
```

현재 서버 응답 예시:

```json
{
  "manual_mode": false,
  "status": "moving",
  "destination": "room_301"
}
```

- `manual_mode: true`: “관리자가 제어 중입니다” 전체 화면 표시
- `manual_mode: false`: 관리자 제어 화면 해제
- `status: "sending"`, `"moving"`, `"canceling"`: 다른 사용자의 새 안내 시작 차단
- `status: "arrived"`: “목적지에 도착했습니다” 화면 표시
- `status: "idle"`, `"canceled"`: 다시 목적지를 선택할 수 있는 상태

도착 상태는 다른 서버에서도 재사용할 수 있도록 `navigationStatus`, `navigation_status`, `status`, `arrived: true` 형식을 모두 인식합니다.

### 여러 사용자 충돌 응답

로봇이 이미 안내 중인데 새로운 목적지 명령이 들어오면 서버는 HTTP `409 Conflict`를 반환합니다.

```http
HTTP/1.1 409 Conflict
```

웹은 이 응답을 일반 통신 오류와 구분해 “다른 사용자가 사용 중입니다” 팝업을 표시합니다.

## 여러 사용자 처리 흐름

```text
사용자 A가 목적지 명령 전송
  ↓
서버 상태가 sending 또는 moving으로 변경
  ↓
사용자 B의 웹이 GET /api/status에서 사용 중 상태 확인
  ↓
사용자 B가 목적지를 누르면 팝업 표시, POST 요청은 보내지 않음
```

1초 상태 확인 전에 두 사용자가 거의 동시에 누를 수도 있습니다. 이 경우 사용자 B의 `POST /api/command`가 서버에서 `409`를 받고, 웹은 같은 팝업을 표시합니다. 즉 **상태 사전 확인**과 **409 응답 처리**를 함께 사용해야 경쟁 상황에도 안전합니다.

## 디자인을 수정할 위치

- 목적지 버튼과 화면 문구: `index.html`
- 색상·여백·크기·반응형 구성: `app.css`
- 목적지 이름·서버 요청·화면 상태 변화: `app.js`
- 로봇 아이콘: `robot-mark.svg`

서버 주소를 IP로 고정하거나 목적지 좌표를 UI 안에 넣지 마세요. `/api/status`처럼 상대 경로를 사용하면 접속한 서버의 주소와 포트를 브라우저가 자동으로 사용하므로 웹 파일을 재사용하기 쉽습니다.

## 확인 방법

`index.html`만 브라우저로 열어 디자인을 볼 수 있지만 서버 API가 없으므로 통신 오류가 표시될 수 있습니다. 전체 동작은 `Webserver/Server`를 실행한 뒤 `http://localhost:8080/`에서 확인합니다.

동시 사용자 기능은 일반 창과 시크릿 창처럼 서로 다른 두 브라우저 창을 열어 확인합니다. 첫 번째 창에서 안내를 시작한 뒤 두 번째 창에서 목적지를 누르면 사용 중 팝업이 보여야 합니다.

UI의 제작 과정과 JavaScript·CSS 설명은 [`../docs/USER_UI_DEVELOPMENT_NOTES.md`](../docs/USER_UI_DEVELOPMENT_NOTES.md)를 참고합니다.
