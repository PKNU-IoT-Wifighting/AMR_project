# 재사용 가능한 사용자 UI 템플릿

이 폴더에는 안내 로봇 사용자 디스플레이의 정적 웹 원본이 들어 있습니다. 특정 웹 프레임워크나 별도 UI 서버가 필요하지 않아 Spring Boot, C++, Node.js 등 정적 파일을 제공할 수 있는 서버에 복사하여 사용할 수 있습니다.

## 파일 설명

| 파일 | 역할 |
| --- | --- |
| `index.html` | 목적지 선택, 안내 중, 도착, 관리자 제어 화면의 구조와 문구 |
| `app.css` | 색상, 크기, 배치, 1024×600 및 모바일 반응형 디자인 |
| `app.js` | 목적지 선택, 서버 요청, 안내 취소, 수동모드와 도착 상태 처리 |
| `robot-mark.svg` | 화면과 브라우저 아이콘에 사용하는 로봇 이미지 |

## 다른 서버에 적용하는 방법

1. 위 네 파일을 서버의 정적 파일 폴더에 복사합니다.
2. 서버가 `/`에서 `index.html`을 제공하도록 설정합니다.
3. 아래 API를 같은 서버 주소에서 제공하거나 `app.js`의 요청 경로를 새 서버 규격에 맞게 변경합니다.
4. 목적지 전송, 안내 취소, 수동모드, 도착 상태를 실제 서버와 확인합니다.

Spring Boot에서는 일반적으로 다음 위치에 네 파일을 넣습니다.

```text
src/main/resources/static/
```

현재 팀 프로젝트의 실제 적용 위치는 다음과 같습니다.

```text
Webserver/haktae/demo/src/main/resources/static/
```

## 필요한 API 규격

### 목적지 전송

```http
POST /api/navigation
Content-Type: application/json
```

```json
{"destination":"301"}
```

사용하는 목적지 ID:

| ID | 화면 표시 |
| --- | --- |
| `toilet` | 화장실 앞 |
| `301` | 강의실 301호 |
| `302` | 강의실 302호 |
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

```json
{
  "manual_mode": false,
  "navigationStatus": "moving"
}
```

- `manual_mode: true`: “관리자가 제어 중입니다” 전체 화면 표시
- `manual_mode: false`: 관리자 제어 화면 해제
- `navigationStatus: "arrived"`: “목적지에 도착했습니다” 화면 표시

도착 상태는 호환을 위해 `navigation_status`, `status`, `arrived: true` 형식도 인식합니다.

## 디자인을 수정할 위치

- 목적지 버튼과 화면 문구: `index.html`
- 색상·여백·크기·반응형 구성: `app.css`
- 목적지 이름·서버 요청·화면 상태 변화: `app.js`
- 로봇 아이콘: `robot-mark.svg`

서버 주소를 IP로 고정하거나 목적지 좌표를 UI 안에 넣지 마세요. 상대 API 경로를 사용하면 서버 주소가 바뀌어도 같은 웹 파일을 재사용할 수 있습니다.

## 확인 방법

`index.html`만 브라우저로 열어 디자인을 볼 수 있지만 서버 API가 없으므로 통신 오류가 표시될 수 있습니다. 전체 동작은 Spring Boot 서버를 실행한 뒤 `http://localhost:8080/`에서 확인합니다.

UI의 제작 과정과 JavaScript·CSS 설명은 [`../docs/USER_UI_DEVELOPMENT_NOTES.md`](../docs/USER_UI_DEVELOPMENT_NOTES.md)를 참고합니다.
