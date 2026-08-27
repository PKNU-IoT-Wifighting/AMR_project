# 2026 ROS 2 자율주행 안내 로봇 프로젝트

윈도우 플랫폼 기반 IoT 시스템 개발 과정 파이널 팀프로젝트

## 사전 학습 및 시뮬레이션

### ROS 2 Jazzy SLAM·자율탐사 연습

- [VMware + Gazebo + TurtleBot3를 활용한 SLAM·자율탐사](../../docs/ROS2_SLAM_PRACTICE.md)

## 실물 자율주행 안내 로봇 프로젝트

### 프로젝트 요구사항

- [자율주행 안내 로봇 요구사항 정의서](../../docs/PROJECT_REQUIREMENTS.md)

### 팀원 역할 분담

- [5인 팀 개발 역할 및 협업 구조](../../docs/TEAM_ROLES.md)

## 웹 HMI (GuideRobot.StaticHmi)

사용자 화면과 로봇 부착 디스플레이는 하나의 반응형 웹 HMI로 제공합니다.

- 프로젝트 위치: `GuideRobot/GuideRobot.StaticHmi`
- 기술: 정적 HTML, CSS, JavaScript
- 지원 화면: 로봇 디스플레이(1024×600), 휴대폰, PC 브라우저
- 관리자 제어 화면: Qt
- 별도 .NET Web HMI 서버: 사용하지 않음

### 현재 시스템 구조

```text
휴대폰 · 로봇 디스플레이
          |
   C++ 로봇 API 서버 :8080
   ├─ 정적 웹 HMI 제공
   └─ /api/status, /api/command
          |
라즈베리파이 · 로봇 하드웨어

Qt 관리자 화면 ──> C++ 로봇 API 서버 ──> 웹 HMI 상태 전달
```

정적 웹 파일은 C++ 서버가 직접 제공합니다. 브라우저는 웹을 받은 동일한 서버의 `/api/status`, `/api/command`를 호출하므로 별도 Web HMI 실행 포트와 CORS 설정이 필요하지 않습니다.

### 웹 HMI → 서버 API 규격

기본 API 경로는 같은 출처의 `POST /api/command`입니다. 별도 API 주소 설정 파일은 사용하지 않습니다.

#### 목적지 안내

```json
{
  "destination": "room_301"
}
```

기존 `POST /api/command` 경로와 `destination` JSON 형식은 그대로 유지합니다. 웹은 목적지 ID만 전송하며, 서버가 내부에 저장된 좌표를 찾아 ROS 2 목표로 전달합니다.

| 목적지 ID | 화면 표시 |
| --- | --- |
| `restroom` | 화장실 앞 |
| `room_301` | 강의실 301호 |
| `room_302` | 강의실 302호 |
| `elevator` | 엘리베이터 앞 |

좌표와 `frame_id`는 서버·ROS 제어부에서만 관리하며 웹 소스와 웹 배포 문서에는 중복 저장하지 않습니다.

#### 안내 취소

```json
{
  "command": "cancel"
}
```

서버는 정상 처리 시 HTTP 200과 JSON 응답을 반환해야 합니다. HTTP 요청 본문이 TCP 수신 과정에서 나뉘어 도착할 수 있으므로, 서버는 `Content-Length`만큼 본문을 모두 받은 뒤 JSON을 파싱해야 합니다.

### 관리자 수동 제어 연동 계획

Qt 관리자 화면이 수동 제어를 활성화하면 서버의 `manual_mode` 상태가 변경됩니다.

```json
{
  "manual_mode": true
}
```

서버의 `GET /api/status` 응답에는 최소한 아래 상태가 포함되어야 합니다.

```json
{
  "status": "moving",
  "manual_mode": true
}
```

웹 HMI는 이 상태를 1초마다 조회하여, `manual_mode`가 `true`인 동안 목적지 선택·안내 시작·안내 취소를 비활성화하고 “관리자가 제어 중입니다” 전체 화면을 표시합니다.

Qt 관리자 화면이 서버의 `manual_mode`를 변경하는 API는 관리자 화면과 C++ 서버 간 규격으로 정합니다. 웹 HMI에는 상태 변경 API 경로와 관계없이 `GET /api/status` 응답의 `manual_mode` 값만 전달되면 됩니다.

### 목적지 도착 상태 연동

로봇이 실제 목적지에 도착해 멈추면 서버는 `GET /api/status`의 `status`를 `arrived`로 반환합니다.

```json
{
  "status": "arrived",
  "manual_mode": false
}
```

웹 HMI는 안내 중 1초마다 상태를 확인하며, `status: "arrived"`를 받으면 “목적지에 도착했습니다” 화면으로 자동 전환합니다. 숫자 진행률은 사용하지 않습니다.

서버는 목적지 명령 접수 시 `moving`, 실제 도착 시 `arrived`, 안내 취소·대기 시 `idle`로 상태를 관리하는 것을 기준으로 합니다.

### 서버 배포 구조

최종 배포 시 C++ 서버 하나에서 다음 구성으로 실행합니다.

```text
휴대폰 · 로봇 디스플레이
          |
 http://192.168.0.9:8080/
          |
 C++ Robot API 서버 :8080
 ├─ index.html, app.css, app.js
 └─ /api/status, /api/command
```

- 사용자는 같은 네트워크에서 `http://192.168.0.9:8080/`으로 접속합니다.
- C++ 서버는 `/`에서 `index.html`을, `/app.css`, `/app.js`, `/robot-mark.svg`에서 정적 파일을 제공합니다.
- 브라우저 JavaScript는 같은 8080 서버의 API를 직접 호출합니다.

### 남은 실제 연동 항목

- 로봇 제어부의 실제 도착·정지 신호를 서버의 `status: "arrived"` 상태에 연결
- 안내 취소 명령을 실제 Nav2 목표 취소 동작과 연결

Web HMI의 숫자 진행률, 임시 증가 타이머, 진행바 애니메이션은 제거했습니다. 안내 중에는 목적지와 안내 상태만 간결하게 표시하고, 실제 도착 신호를 받으면 도착 완료 화면을 표시합니다.
