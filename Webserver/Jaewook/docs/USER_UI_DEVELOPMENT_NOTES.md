# 사용자 UI 개발 진행 기록 및 학습 노트

작성일: 2026-08-26  
담당 영역: 안내 로봇 사용자 디스플레이 UI  
현재 통합 브랜치: `Web_server`

이 문서는 사용자 UI가 어떻게 만들어지고 서버에 연결되었는지 기록하고, HTML·CSS·JavaScript를 처음 공부할 때 참고하기 위해 작성했습니다.

## 1. 가장 먼저 알아둘 점

이번 작업은 **CSS로 만들었던 화면을 JavaScript로 바꾼 것**이 아닙니다.

HTML, CSS, JavaScript는 서로 다른 역할을 담당하며 함께 사용됩니다.

| 구분 | 담당 역할 | 이 프로젝트의 예시 |
| --- | --- | --- |
| HTML | 화면의 구조와 내용 | 목적지 버튼, 안내 확인창, 관리자 제어창 |
| CSS | 색상·크기·배치·반응형 디자인 | 버튼 색상, 카드 배치, 1024×600 화면 대응 |
| JavaScript | 사용자의 조작과 화면 동작 | 버튼 클릭, 서버 요청, 수동모드 및 도착 화면 전환 |
| FastAPI + ROS 2 서버 | 로봇·ROS와 웹 사이의 데이터 처리 | 목적지 좌표 전달, 수동모드·도착 상태·동시 사용 관리 |

따라서 기존 CSS 디자인은 그대로 사용되고 있으며, JavaScript가 그 디자인을 움직이게 만드는 역할을 합니다.

## 2. 현재 사용되는 파일

사용자 UI 원본:

```text
Jaewook/user-ui-template/
├─ index.html
├─ app.css
├─ app.js
└─ robot-mark.svg
```

현재 실제 서버가 읽는 위치:

```text
Jaewook/user-ui-template/
```

`Webserver/Server/server.py`는 위 폴더를 직접 정적 웹으로 제공합니다. 서버가 기본 포트로 실행되면 다음과 같이 접속합니다.

```text
http://서버주소:8080/
```

별도의 사용자 UI 서버나 복사된 실행용 웹 폴더를 추가로 만들지 않습니다.

## 3. 개발 진행 과정

### 1단계: 사용자 화면 디자인

- 목적지 선택 화면 구성
- 안내 시작 확인창 구성
- 안내 중 화면 구성
- 관리자 수동제어 전체 화면 구성
- 작은 로봇 디스플레이에서도 보이도록 반응형 디자인 적용

이 단계에서는 주로 HTML과 CSS가 사용되었습니다.

### 2단계: 정적 웹으로 변환

초기에는 .NET Web HMI가 별도의 웹 서버로 동작했습니다. 서버가 두 개 실행되는 문제를 피하기 위해 HTML, CSS, JavaScript만 사용하는 정적 웹 구조를 만들었습니다.

정적 웹으로 바꾸면서 다음 장점이 생겼습니다.

- FastAPI + ROS 2 서버 하나만 실행하면 됨
- 웹과 API가 모두 8080 포트를 사용함
- 서버 주소를 JavaScript에 고정하지 않아도 됨
- CORS 설정이 필요하지 않음
- 서버 담당자가 정적 파일을 함께 빌드하고 배포할 수 있음

### 3단계: 임시 Spring Boot 서버 규격 적용

새 서버의 임시 UI 대신 기존 GuideRobot 디자인을 적용했습니다.

변경된 연결 규격:

| 항목 | 이전 규격 | 현재 규격 |
| --- | --- | --- |
| 목적지 전송 주소 | `/api/command` | `/api/navigation` |
| 화장실 ID | `restroom` | `toilet` |
| 301호 ID | `room_301` | `301` |
| 302호 ID | `room_302` | `302` |
| 수동모드 필드 | `manual_mode` | `manual_mode` |
| 도착 상태 | `status: "arrived"` | `navigationStatus: "arrived"` |

### 4단계: 안내 취소 제거 후 복원

새 서버에 로봇 이동 취소 기능이 없을 때는 웹 화면만 초기화되는 위험을 막기 위해 안내 취소를 잠시 제거했습니다. 이후 서버가 취소 요청을 받으면 `/cmd_vel` 토픽에 `linear.x: 0.0`, `angular.z: 0.0`을 발행하기로 정하면서 버튼을 다시 복원했습니다.

웹은 `POST /api/command`에 `{"command":"cancel"}`을 보냅니다. 서버가 성공 응답을 보낸 경우에만 목적지 선택 화면으로 돌아가며, 실패하면 안내 중 화면을 유지하고 오류를 표시합니다.

로봇이 도착한 뒤에는 같은 버튼이 `확인`으로 바뀝니다. 이때는 서버 명령을 보내지 않고 목적지 선택 화면으로 돌아갑니다.

### 5단계: ROS 2 C++ 서버 규격 적용(과거 기록)

당시 `hyunbeen` 폴더의 C++ 서버에 UI 디자인을 유지한 채 API 규격을 맞췄습니다. 이후 실제 서버가 `Webserver/Server`의 FastAPI 서버로 변경되었으므로 이 단계는 변경 이력으로만 참고합니다.

| 항목 | 최종 규격 |
| --- | --- |
| 목적지 전송 주소 | `POST /api/command` |
| 목적지 ID | `restroom`, `room_301`, `room_302`, `elevator` |
| 수동모드 필드 | `manual_mode` |
| 도착 상태 | `status: "arrived"` |

현재 웹 실행본은 `Jaewook/user-ui-template`이며 FastAPI 서버가 이 폴더를 직접 제공합니다.

### 6단계: 실제 FastAPI 서버 연결 및 동시 사용자 처리

현재 실제 서버는 `Webserver/Server`입니다. 사용자 UI는 서버가 공유하는 `status`를 기준으로 로봇이 사용 중인지 판단합니다.

- `sending`, `moving`, `canceling`: 다른 사용자의 새 목적지 선택과 안내 시작 차단
- `idle`, `arrived`, `canceled`: 사용 중 잠금 해제
- 서버가 HTTP `409 Conflict`를 반환한 경우: “다른 사용자가 사용 중입니다” 팝업 표시

브라우저마다 JavaScript 상태는 따로 존재하므로 `isGuiding`만 확인해서는 여러 사용자를 구분할 수 없습니다. 모든 브라우저가 함께 보는 서버 상태와 서버의 `409` 응답을 기준으로 처리해야 합니다.

## 4. 현재 화면 동작 순서

```text
웹 접속
  ↓
GET /api/status로 서버 연결 및 수동모드 확인
  ↓
목적지 선택
  ↓
안내 시작 확인창
  ↓
POST /api/command로 목적지 ID 전송
  ↓
“로봇이 안내 중입니다” 화면 표시
  ↓
GET /api/status를 1초마다 확인
  ↓
status가 arrived이면 도착 화면 표시
  ↓
사용자가 확인 버튼을 누르면 목적지 선택 화면으로 복귀
```

안내 중 사용자가 취소한 경우의 흐름:

```text
안내 취소 클릭
  ↓
POST /api/command에 {"command":"cancel"} 전송
  ↓
서버가 로봇 정지 토픽 발행
  ↓
서버 성공 응답 후 목적지 선택 화면으로 복귀
```

다른 사용자가 이미 안내를 실행 중인 경우의 흐름:

```text
사용자 A가 안내 시작
  ↓
서버 상태가 sending 또는 moving으로 변경
  ↓
사용자 B의 웹이 GET /api/status로 사용 중 상태 확인
  ↓
사용자 B가 목적지를 누르면 “다른 사용자가 사용 중입니다” 팝업 표시
  ↓
안내가 끝나 서버 상태가 사용 중이 아니게 되면 팝업 자동 해제
```

## 5. 서버와 주고받는 데이터

### 목적지 전송

301호를 선택한 예시:

```http
POST /api/command
Content-Type: application/json
```

```json
{
  "destination": "room_301"
}
```

웹은 좌표를 알 필요가 없습니다. 서버가 `room_301`이라는 ID를 좌표로 바꾸고 ROS 2에 전달합니다.

사용 가능한 목적지 ID:

- `restroom`: 화장실 앞
- `room_301`: 강의실 301호
- `room_302`: 강의실 302호
- `elevator`: 엘리베이터 앞

### 상태 확인

웹은 다음 주소를 1초마다 요청합니다.

```http
GET /api/status
```

현재 서버 응답 예시:

```json
{
  "status": "moving",
  "manual_mode": false,
  "destination": "room_301"
}
```

각 필드의 의미:

- `status`: 로봇의 이동 상태를 나타냄
- `manual_mode`: 관리자가 로봇을 수동으로 제어 중인지 나타냄
- `destination`: 현재 이동 중인 목적지 ID이며 대기 중에는 생략될 수 있음

`status` 값은 다음과 같이 사용합니다.

- `idle`: 대기 중
- `sending`: Nav2에 목적지 목표를 전달하는 중
- `moving`: 목적지로 이동 중
- `canceling`: 현재 안내를 취소하는 중
- `arrived`: 목적지에 도착함
- `canceled`: 안내가 취소됨

호환성을 위해 웹은 다음 응답을 모두 도착으로 인식합니다.

- `navigationStatus: "arrived"`
- `navigation_status: "arrived"`
- `status: "arrived"`
- `arrived: true`

### 안내 취소 요청

```http
POST /api/command
Content-Type: application/json
```

```json
{
  "command": "cancel"
}
```

FastAPI + ROS 2 서버는 이 명령을 받으면 현재 Nav2 목표에 취소 요청을 보내고 주행 상태를 갱신합니다.

## 6. JavaScript에서 공부할 부분

JavaScript 파일은 `user-ui-template/app.js`입니다.

### `const`와 `let`

```javascript
const destinations = new Map([...]);
let changed = false;
```

- `const`: 다른 값으로 다시 바꾸지 않을 변수
- `let`: 실행 중 값이 변경될 변수

### 객체와 상태

```javascript
const state = {
    selectedDestination: null,
    isGuiding: false,
    hasArrived: false,
    isManualMode: false
};
```

`state`는 현재 화면 상황을 한곳에 보관합니다. 예를 들어 `hasArrived`가 `true`이면 도착 화면을 보여줍니다.

### HTML 요소 찾기

```javascript
document.querySelector("#start-button");
```

HTML에서 `id="start-button"`인 요소를 찾아 JavaScript에서 조작할 수 있게 만듭니다.

### 클릭 이벤트

```javascript
elements.startButton.addEventListener("click", openConfirmation);
```

사용자가 안내 시작 버튼을 클릭하면 `openConfirmation` 함수가 실행됩니다.

### 서버 요청 `fetch`

```javascript
await fetch("/api/command", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ destination })
});
```

`fetch`는 웹에서 서버로 요청을 보내는 기능입니다. 여기서는 선택된 목적지 ID를 JSON으로 전송합니다.

### `async`와 `await`

서버 응답은 즉시 도착하지 않을 수 있습니다. `async` 함수 안에서 `await`를 사용하면 응답이 올 때까지 기다린 뒤 다음 코드를 실행할 수 있습니다.

### 주기적인 상태 확인

```javascript
window.setInterval(refreshStatus, 1000);
```

`refreshStatus` 함수를 1초마다 실행해 수동모드와 도착 상태를 확인합니다. 이 방식을 폴링이라고 합니다.

### 조건문으로 도착 확인

```javascript
const hasArrived = [status.navigationStatus, status.navigation_status, status.status]
    .some(value => typeof value === "string" && value.toLowerCase() === "arrived")
    || status.arrived === true;

if (hasArrived) {
    // 도착 화면 표시
}
```

서버가 `arrived`를 보낸 경우에만 도착 화면으로 전환합니다. 서버 응답에 필드가 없을 때도 오류가 나지 않도록 확인하는 것이 중요합니다.

### 여러 브라우저가 공유하는 상태

```javascript
const isRobotBusy = ["sending", "moving", "canceling"].includes(serverStatus);
```

`state.isGuiding`은 현재 브라우저에서 안내를 시작했는지만 기억합니다. 다른 휴대폰이나 브라우저가 시작한 안내는 알 수 없으므로, 서버의 `status`를 읽어 `state.isRobotBusy`에 저장합니다.

```text
isGuiding     = 이 브라우저가 안내를 시작했는가?
isRobotBusy   = 서버 기준으로 로봇이 현재 사용 중인가?
```

이 둘을 구분해야 안내를 시작한 사용자에게는 정상 안내 화면을 보여주면서, 나중에 접속한 사용자에게만 사용 중 팝업을 표시할 수 있습니다.

### HTTP 409와 사용자 정의 오류

```javascript
if (response.status === 409) {
    throw new RobotBusyError();
}
```

HTTP `409 Conflict`는 요청 형식이 틀린 것이 아니라 현재 서버 상태와 요청이 충돌한다는 뜻입니다. 이 프로젝트에서는 이미 안내 중일 때 새 목적지 요청이 들어온 경우입니다.

`RobotBusyError`라는 별도 오류 클래스를 사용하면 네트워크 단절이나 서버 고장과 구분할 수 있습니다. `catch`에서 이 오류만 찾아 일반 오류 문구 대신 사용 중 팝업을 엽니다.

### 왜 상태 확인과 409 처리를 둘 다 하는가

```text
1. 사용자 B가 GET /api/status에서 idle을 확인
2. 거의 동시에 사용자 A가 안내 시작
3. 사용자 B도 목적지 POST 전송
4. 서버가 사용자 B에게 409 반환
```

이런 상황을 경쟁 상태(race condition)라고 합니다. `GET /api/status`만 확인하면 1번과 3번 사이에 상태가 바뀔 수 있습니다. 따라서 웹은 미리 사용 중인지 확인하고, 서버도 최종적으로 중복 명령을 거절해야 합니다.

### 팝업 열기와 닫기

- `openBusyDialog()`: `isBusyDialogOpen`을 `true`로 바꾸고 팝업을 표시
- `closeBusyDialog()`: 값을 `false`로 바꾸고 팝업을 숨김
- `busyBackdrop`: 팝업 바깥의 어두운 배경
- `busyConfirm`: 확인 버튼
- `event.stopPropagation()`: 팝업 안을 눌렀을 때 배경 클릭으로 전달되어 닫히는 것을 방지
- `Escape` 키: 사용자가 키보드로 팝업을 닫을 수 있게 처리

### `render` 함수

`render()`는 현재 `state` 값을 읽고 제목, 버튼, 안내창을 알맞게 표시합니다.

```text
상태 변경 → render() 실행 → 화면 변경
```

이 구조를 사용하면 여러 곳에서 HTML을 직접 수정하는 것보다 화면 동작을 이해하기 쉽습니다.

## 7. CSS에서 공부할 부분

CSS 파일은 `user-ui-template/app.css`입니다.

### CSS 변수

```css
:root {
    --navy: #12355b;
    --mint: #7ee6c4;
}
```

자주 사용하는 색상을 이름으로 저장합니다. 색상을 변경할 때 한곳만 수정할 수 있습니다.

### 클래스 선택자

```css
.destination-button {
    /* 모든 목적지 버튼 디자인 */
}

.destination-button.selected {
    /* 선택된 목적지 버튼 디자인 */
}
```

JavaScript가 `selected` 클래스를 추가하거나 제거하면 CSS가 선택된 버튼의 모양을 바꿉니다.

### Grid와 Flex

- `display: grid`: 목적지 버튼처럼 행과 열로 배치할 때 사용
- `display: flex`: 제목과 상태처럼 한 방향으로 정렬할 때 사용

### 반응형 화면

```css
@media (max-width: 620px) {
    /* 작은 화면용 디자인 */
}
```

화면 크기에 따라 버튼 크기와 배치를 변경합니다.

### 숨김 처리

```css
[hidden] {
    display: none !important;
}
```

JavaScript가 HTML 요소에 `hidden` 속성을 설정하면 CSS가 해당 요소를 화면에서 숨깁니다.

## 8. 수동모드 동작

관리자 화면이 `POST /api/manual-mode`로 서버에 수동모드 상태를 전달하면 사용자 UI는 직접 관리자 화면과 통신하지 않고 서버 상태만 확인합니다.

```text
관리자 화면
  ↓ manual_mode 변경 요청
FastAPI + ROS 2 서버
  ↓ GET /api/status 응답
사용자 UI
  ↓
“관리자가 제어 중입니다” 전체 화면 표시 또는 해제
```

`manual_mode`가 `true`이면 목적지 선택과 안내 시작을 막고 전체 화면 안내창을 표시합니다. `false`가 되면 창을 자동으로 숨깁니다.

## 9. 도착 상태의 담당 범위

사용자 UI는 로봇이 실제로 도착했는지 직접 알 수 없습니다.

```text
로봇 도착
  ↓
ROS 2가 도착 신호 발생
  ↓
FastAPI + ROS 2 서버의 Nav2 결과 콜백이 status를 arrived로 변경
  ↓
사용자 UI가 GET /api/status로 확인
  ↓
도착 화면 표시
```

`Webserver/Server/server.py`는 Nav2 `NavigateToPose` 액션 결과를 직접 받습니다. 결과가 성공이면 서버 상태를 `arrived`로 바꾸고, 사용자 UI는 `GET /api/status`에서 이 값을 확인해 화면에 표현합니다. 별도의 도착 상태 브리지를 추가로 실행하지 않습니다.

## 10. 현재 구현 상태

- [x] 목적지 4개 표시
- [x] 목적지 선택 및 안내 시작 확인
- [x] 최종 서버의 `/api/command`로 목적지 ID 전송
- [x] 수동모드 전체 화면 표시 및 자동 해제
- [x] 안내 중 화면 표시
- [x] 안내 취소 버튼 및 서버 요청 복원
- [x] 취소 성공 후에만 목적지 선택 화면으로 복귀
- [x] 취소 실패 시 안내 화면 유지 및 오류 표시
- [x] 여러 도착 응답 형식을 인식해 도착 화면 표시
- [x] 도착 확인 후 초기 화면 복귀
- [x] 서버 상태에 `navigationStatus`가 없어도 오류 없이 동작
- [x] 브라우저 동작 테스트
- [x] 서버에서 `{"command":"cancel"}`을 처리하고 Nav2 목표 취소
- [x] FastAPI 서버가 Nav2 성공 결과를 받아 `status: "arrived"`로 변경
- [x] `manual_mode` 상태에 따른 관리자 제어 화면 표시
- [x] FastAPI 서버가 `Jaewook/user-ui-template`을 직접 제공하도록 연결
- [x] 서버의 `sending`, `moving`, `canceling` 상태를 사용 중으로 인식
- [x] 다른 사용자의 목적지 선택과 안내 시작 차단
- [x] 서버의 HTTP `409 Conflict`를 사용 중 팝업으로 처리
- [x] “다른 사용자가 사용 중입니다” 팝업과 키보드·배경 닫기 동작 추가
- [ ] 실제 로봇 환경에서 Nav2 이동·취소·도착 결과 확인
- [ ] 실제 기기 두 대에서 동시 사용자 팝업과 409 처리 확인
- [ ] 실제 로봇과 통합 테스트

## 11. 추천 학습 순서

1. `index.html`을 열어 버튼과 화면 구조 찾기
2. `app.css`에서 같은 클래스 이름을 검색해 디자인 확인하기
3. `app.js`의 `elements`에서 HTML 요소가 어떻게 연결되는지 확인하기
4. 버튼의 `addEventListener`부터 따라가기
5. `startGuidance()`에서 목적지 전송 과정 확인하기
6. `refreshStatus()`에서 수동모드와 도착 상태 확인하기
7. `isRobotBusy`와 `isGuiding`의 차이 비교하기
8. `postCommand()`의 HTTP 409 처리와 `RobotBusyError` 따라가기
9. `openBusyDialog()`부터 팝업 HTML과 CSS 연결해보기
10. `render()`에서 상태에 따라 화면이 어떻게 바뀌는지 확인하기

처음부터 모든 문법을 외우기보다는 버튼 하나를 기준으로 HTML → CSS → JavaScript 순서로 연결해보는 것이 좋습니다.

## 12. 현재 FastAPI 서버에서 확인하는 방법

1. `C:/SourceBank/AMR_project`를 프로젝트로 열기
2. Git 브랜치를 `Web_server`로 선택하기
3. Ubuntu에서 Nav2를 먼저 실행하기
4. 프로젝트 루트에서 `./Webserver/Server/run.sh` 실행하기
5. 브라우저에서 `http://localhost:8080/` 접속하기
6. 같은 네트워크의 다른 기기에서는 `http://서버컴퓨터IP:8080/`로 접속하기
7. 개발자 도구의 Network 탭에서 `/api/status`와 `/api/command` 요청 확인하기
8. 일반 창과 시크릿 창을 함께 열어 동시 사용자 팝업 확인하기

## 13. 2026-08-26 작업 기록

- `Web_server` 브랜치의 haktae Spring Boot 서버에 사용자 UI 통합
- Jaewook의 기존 사용자 UI 디자인을 haktae Spring Boot 서버에 적용
- 목적지 ID와 전송 API를 새 서버 규격에 맞춤
- `manual_mode` 수동제어 화면 연결
- 서버 기능이 준비되기 전 안내 취소 기능을 제거
- 서버의 정지 토픽 발행 계획에 맞춰 안내 취소 버튼과 요청을 다시 복원
- `navigationStatus`, `navigation_status`, `status`, `arrived` 도착 응답 호환 처리
- Spring Boot 서버에 `idle`, `moving`, `arrived` 주행 상태 관리 추가
- 안내 취소 시 `/cmd_vel`에 0 속도를 발행하도록 ROS 2 연결 추가
- Nav2 액션 성공 상태를 서버에 보고하는 `navigation_status_bridge.py` 추가
- Spring Boot 서버 시작 시 ROS 도착 브리지가 자동으로 실행되고 서버 종료 시 함께 종료되도록 변경
- 목적지 선택, 안내 시작, 수동모드, 도착 화면, 초기화 동작을 브라우저에서 검증
- JavaScript 문법 검사 통과
- Gradle 자동 테스트와 배포 JAR 생성 확인 완료

이후 작업이 생기면 날짜별로 이 아래에 변경 이유, 수정한 파일, 확인 결과를 계속 추가합니다.

## 14. 2026-08-27 폴더 정리 기록

- 재사용 가능한 정적 UI 원본을 `Jaewook/user-ui-template`로 분리
- 프로젝트·학습 문서를 `Jaewook/docs`로 분류
- 이전 C++ 서버와 .NET Web HMI 자료를 `Jaewook/legacy`로 이동
- 최상위 `README.md`에 현재 사용 위치, 재사용 방법, 과거 자료 구분 추가
- UI 템플릿의 API 규격과 파일별 수정 위치를 별도 README에 정리

## 15. 2026-08-27 hyunbeen C++ 서버 통합 기록

- Jaewook 사용자 UI 디자인을 `hyunbeen/web`에 적용
- 목적지 전송을 `POST /api/command`로 변경
- 목적지 ID를 `restroom`, `room_301`, `room_302`, `elevator`로 변경
- `manual_mode`와 `status: "arrived"` 응답 연결
- CMake 빌드 시 UI 파일을 ROS 2 패키지에 함께 설치하도록 변경
- `ros2 launch`가 설치된 UI 경로를 자동으로 사용하도록 변경
- 임시 `haktae` Spring Boot 서버는 변경 이력으로만 유지

이 항목은 과거 통합 기록입니다. 현재 실제 서버는 `Webserver/Server`이며 `hyunbeen`은 실행 대상으로 사용하지 않습니다.

## 16. 2026-09-04 실제 Server 연결 및 동시 사용자 처리 기록

### 변경 이유

기존에는 각 브라우저의 `isGuiding` 값만으로 버튼을 막았습니다. 이 값은 브라우저마다 따로 저장되므로 사용자 A가 안내 중이어도 사용자 B의 웹에서는 사용 중이라는 사실을 알 수 없었습니다.

실제 `Webserver/Server`는 모든 사용자에게 같은 `status`를 제공하고, 이미 이동 중인 상태에서 새 목적지 명령이 오면 HTTP `409`를 반환합니다. UI가 이 두 정보를 사용하도록 변경했습니다.

### 수정한 파일

| 파일 | 수정 내용 |
| --- | --- |
| `user-ui-template/index.html` | 다른 사용자 사용 중 팝업 구조와 문구 추가 |
| `user-ui-template/app.css` | 팝업 아이콘, 설명 문구, 창 너비 디자인 추가 |
| `user-ui-template/app.js` | 서버 사용 중 상태, 409 오류, 팝업 열기·닫기 처리 추가 |
| `Jaewook/README.md` | 실제 서버 위치와 현재 UI 구조로 수정 |
| `user-ui-template/README.md` | API 규격, 실행법, 동시 사용자 흐름 설명 추가 |

### 실제 동작 순서

```text
GET /api/status
  ├─ status가 sending/moving/canceling
  │    └─ 이 브라우저가 시작한 안내가 아니면 목적지 조작 차단
  │         └─ 사용자가 누를 때 사용 중 팝업 표시
  └─ 그 외 상태
       └─ 팝업 해제 및 정상 조작 가능

POST /api/command
  ├─ 성공 응답 → 안내 화면 표시
  └─ 409 응답 → RobotBusyError → 사용 중 팝업 표시
```

### 두 브라우저로 확인할 항목

1. 창 A와 창 B에서 같은 서버 주소에 접속합니다.
2. 창 A에서 목적지를 선택하고 안내를 시작합니다.
3. 창 B에서 목적지 버튼을 누릅니다.
4. “다른 사용자가 사용 중입니다” 팝업이 나타나는지 확인합니다.
5. 창 A의 안내를 취소하거나 도착 상태가 된 뒤 창 B에서 다시 선택 가능한지 확인합니다.
6. 두 창에서 거의 동시에 안내 시작을 눌러, 한쪽의 409 응답도 같은 팝업으로 처리되는지 확인합니다.

### 공부할 때 핵심 질문

- 왜 `isGuiding`만으로는 다른 사용자의 상태를 알 수 없을까?
- `GET /api/status`로 미리 확인했는데도 서버의 409 처리가 필요한 이유는 무엇일까?
- 일반 통신 오류와 `RobotBusyError`를 나누면 사용자 경험이 어떻게 좋아질까?
- HTML의 `hidden` 속성과 CSS의 `[hidden]`, JavaScript의 `element.hidden`은 어떻게 연결될까?
- 팝업 안쪽 클릭에서 `stopPropagation()`을 제거하면 어떤 일이 생길까?
