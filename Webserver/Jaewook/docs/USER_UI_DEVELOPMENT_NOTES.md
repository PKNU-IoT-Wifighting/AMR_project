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
| Spring Boot 서버 | 로봇·ROS와 웹 사이의 데이터 처리 | 목적지 좌표 전달, 수동모드와 도착 상태 관리 |

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

새 Spring Boot 서버에 적용된 위치:

```text
haktae/demo/src/main/resources/static/
```

Spring Boot는 `static` 폴더의 파일을 자동으로 웹에 제공합니다. 서버가 기본 포트로 실행되면 다음과 같이 접속합니다.

```text
http://서버주소:8080/
```

별도의 사용자 UI 서버를 추가로 실행하지 않습니다.

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

- Spring Boot 서버 하나만 실행하면 됨
- 웹과 API가 모두 8080 포트를 사용함
- 서버 주소를 JavaScript에 고정하지 않아도 됨
- CORS 설정이 필요하지 않음
- 서버 담당자가 정적 파일을 함께 빌드하고 배포할 수 있음

### 3단계: 새 Spring Boot 서버 규격 적용

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
POST /api/navigation으로 목적지 ID 전송
  ↓
“로봇이 안내 중입니다” 화면 표시
  ↓
GET /api/status를 1초마다 확인
  ↓
navigationStatus가 arrived이면 도착 화면 표시
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

## 5. 서버와 주고받는 데이터

### 목적지 전송

301호를 선택한 예시:

```http
POST /api/navigation
Content-Type: application/json
```

```json
{
  "destination": "301"
}
```

웹은 좌표를 알 필요가 없습니다. 서버가 `301`이라는 ID를 좌표로 바꾸고 ROS 2에 전달합니다.

사용 가능한 목적지 ID:

- `toilet`: 화장실 앞
- `301`: 강의실 301호
- `302`: 강의실 302호
- `elevator`: 엘리베이터 앞

### 상태 확인

웹은 다음 주소를 1초마다 요청합니다.

```http
GET /api/status
```

현재 서버 응답 예시:

```json
{
  "status": "ok",
  "manual_mode": false,
  "navigationStatus": "moving"
}
```

각 필드의 의미:

- `status`: 서버 자체가 정상인지 나타냄
- `manual_mode`: 관리자가 로봇을 수동으로 제어 중인지 나타냄
- `navigationStatus`: 로봇의 이동 상태를 나타냄

`navigationStatus` 값은 다음과 같이 사용할 예정입니다.

- `idle`: 대기 중
- `moving`: 목적지로 이동 중
- `arrived`: 목적지에 도착함

현재 서버에 `navigationStatus`가 없어도 웹 오류는 발생하지 않습니다. 다만 도착 화면으로 자동 전환되지 않고 안내 중 화면을 유지합니다.

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

Spring Boot 서버는 이 명령을 받으면 설정된 `/cmd_vel` 토픽에 `linear.x: 0.0`, `angular.z: 0.0`인 `Twist` 메시지를 한 번 발행하고 주행 상태를 `idle`로 변경합니다.

속도 0 발행은 즉시 정지 명령이지만 Nav2 목표 자체를 취소하는 것은 아닙니다. Nav2가 다시 속도 명령을 발행하는 환경에서는 액션 목표 취소 기능을 추가로 검토해야 합니다.

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
await fetch("/api/navigation", {
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

관리자 화면이 서버에 수동모드 상태를 전달하면 사용자 UI는 직접 관리자 화면과 통신하지 않고 서버 상태만 확인합니다.

```text
관리자 화면
  ↓ manual_mode 변경 요청
Spring Boot 서버
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
Spring Boot 서버가 신호를 받아 navigationStatus를 arrived로 변경
  ↓
사용자 UI가 GET /api/status로 확인
  ↓
도착 화면 표시
```

`haktae/demo/ros/navigation_status_bridge.py`가 Nav2의 `/navigate_to_pose/_action/status`를 구독하고 최신 목표의 `STATUS_SUCCEEDED`를 감지하면 서버에 `arrived`를 보고합니다. 사용자 UI는 서버가 제공하는 값을 화면에 표현합니다.

## 10. 현재 구현 상태

- [x] 목적지 4개 표시
- [x] 목적지 선택 및 안내 시작 확인
- [x] 새 서버의 `/api/navigation`으로 목적지 ID 전송
- [x] 수동모드 전체 화면 표시 및 자동 해제
- [x] 안내 중 화면 표시
- [x] 안내 취소 버튼 및 서버 요청 복원
- [x] 취소 성공 후에만 목적지 선택 화면으로 복귀
- [x] 취소 실패 시 안내 화면 유지 및 오류 표시
- [x] 여러 도착 응답 형식을 인식해 도착 화면 표시
- [x] 도착 확인 후 초기 화면 복귀
- [x] 서버 상태에 `navigationStatus`가 없어도 오류 없이 동작
- [x] 브라우저 동작 테스트
- [x] 서버에서 `{"command":"cancel"}`을 처리하고 정지 토픽 발행
- [x] 서버에 `navigationStatus` 상태 저장 및 도착 보고 API 추가
- [x] Nav2 성공 상태를 서버에 전달하는 ROS 2 브리지 추가
- [ ] 실제 로봇 환경에서 Nav2 상태 토픽 이름과 메시지 수신 확인
- [ ] 실제 로봇과 통합 테스트

## 11. 추천 학습 순서

1. `index.html`을 열어 버튼과 화면 구조 찾기
2. `app.css`에서 같은 클래스 이름을 검색해 디자인 확인하기
3. `app.js`의 `elements`에서 HTML 요소가 어떻게 연결되는지 확인하기
4. 버튼의 `addEventListener`부터 따라가기
5. `startGuidance()`에서 목적지 전송 과정 확인하기
6. `refreshStatus()`에서 수동모드와 도착 상태 확인하기
7. `render()`에서 상태에 따라 화면이 어떻게 바뀌는지 확인하기

처음부터 모든 문법을 외우기보다는 버튼 하나를 기준으로 HTML → CSS → JavaScript 순서로 연결해보는 것이 좋습니다.

## 12. 인텔리제이에서 확인하는 방법

1. `C:/SourceBank/AMR_project`를 프로젝트로 열기
2. Git 브랜치를 `Web_server`로 선택하기
3. `Webserver/haktae/demo`를 Gradle 프로젝트로 불러오기
4. Spring Boot 애플리케이션 실행하기
5. 브라우저에서 `http://localhost:8080/` 접속하기
6. 개발자 도구의 Network 탭에서 `/api/status`와 `/api/navigation` 요청 확인하기

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
