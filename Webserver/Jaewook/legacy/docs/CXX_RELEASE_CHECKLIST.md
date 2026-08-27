# GuideRobot Web HMI 최종 발행 체크리스트

## 1단계: 서버·관리자 화면 담당자에게 먼저 전달

전달 문서:

- `SERVER_HANDOFF.md`

서버·관리자 화면 담당자가 합의하고 구현할 항목:

- Qt 관리자 화면에서 수동 조종 기능을 켤 때 서버 상태를 `manual_mode: true`로 변경
- Qt 관리자 화면에서 수동 조종 기능을 끌 때 서버 상태를 `manual_mode: false`로 변경
- C++ 서버의 `GET /api/status` 응답에 JSON boolean 형식의 `manual_mode` 포함
- 로봇의 실제 도착·정지 신호를 `GET /api/status`의 `status: "arrived"`로 반영

권장 관리자 화면 → 서버 API:

```http
POST /api/manual-mode
Content-Type: application/json
```

```json
{
  "manual_mode": true
}
```

관리자 화면과 서버에 이미 다른 명령 규격이 있다면 그 규격을 사용해도 됩니다. Web HMI에는 `GET /api/status`의 `manual_mode` 값만 정확히 전달되면 됩니다.

## 2단계: 간소화된 안내 화면 확인

실제 값이 아닌 숫자 진행률과 약 25초짜리 임시 타이머는 제거했습니다. 안내 중에는 다음 정보만 표시합니다.

- 선택한 목적지
- “안내 중” 상태
- 안내 취소 버튼

서버는 숫자 `progress` 값을 제공하지 않아도 됩니다. 실제 도착 후 `status: "arrived"`를 반환하면 웹은 도착 완료 화면과 확인 버튼을 표시합니다.

## 3단계: 실제 통합 테스트

- [ ] 일반 상태에서 목적지 선택 가능
- [ ] 기존 `POST /api/command`와 `destination` 형식이 유지됨
- [ ] 목적지 ID `restroom`, `room_301`, `room_302`, `elevator`가 서버에 정확히 전달됨
- [ ] 안내 시작 명령이 서버에 정상 전달됨
- [ ] 안내 취소 명령이 서버에 정상 전달되고 실제 Nav2 목표가 취소됨
- [ ] 안내 명령 접수 시 서버 상태가 `status: "moving"`으로 변경됨
- [ ] 로봇이 실제 도착해 멈추면 서버 상태가 `status: "arrived"`로 변경됨
- [ ] 웹 화면이 `arrived` 수신 후 1초 안에 “목적지에 도착했습니다”로 전환됨
- [ ] 도착 화면의 확인 버튼을 누르면 목적지 선택 화면으로 돌아감
- [ ] 대기 중 수동 모드 ON 시 1초 안에 관리자 제어 화면 표시
- [ ] 수동 모드 OFF 시 1초 안에 관리자 제어 화면 자동 제거
- [ ] 안내 중 수동 모드 ON 시 안내 취소 버튼 잠금
- [ ] 안내 중 수동 모드 OFF 시 사용자 화면 조작 복구
- [ ] 페이지를 새로 열었을 때 이미 수동 모드라면 즉시 관리자 제어 화면 표시
- [ ] `manual_mode`가 문자열이 아닌 JSON boolean으로 전달됨
- [ ] Ubuntu 서버에서 Nginx 외부 주소로 접속 가능
- [ ] C++ 서버의 `/`에서 정적 `index.html`이 정상 제공됨
- [ ] `app.css`, `app.js`, `robot-mark.svg`가 8080에서 정상 제공됨
- [ ] 다른 PC·기기에서 `http://192.168.0.9:8080/` 접속 가능

## 4단계: 통합 테스트 후 Web HMI 최종 작업

- 테스트에서 발견된 API 필드명·화면 동작 수정
- 간소화된 안내 화면 최종 확인
- 정적 웹이 상대 경로 `/api/status`, `/api/command`를 사용하는지 확인
- JavaScript 문법 및 브라우저 오류 확인
- 1024×600 및 휴대폰 반응형 화면 확인
- 서버 전달 문서 최신화
- Git 최종 커밋

## 5단계: 최종 발행본 생성

별도 .NET 서버가 필요 없는 정적 웹 파일로 발행합니다.

최종 전달물:

- `GuideRobot.StaticHmi-20260825.zip`
- `index.html`, `app.css`, `app.js`, `robot-mark.svg`
- `README_SERVER.md`
- ZIP SHA-256 체크섬

현재 생성된 ZIP은 C++ 서버의 정적 파일 제공 기능과 실제 로봇 연동을 확인하기 위한 전달본입니다. 실제 서버 통합 테스트에서 규격 변경이 없으면 이 파일을 최종본으로 사용할 수 있습니다.
