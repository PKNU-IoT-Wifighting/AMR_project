# Jaewook 사용자 UI 작업 공간

이 폴더는 안내 로봇의 **사용자 디스플레이 UI 원본**, 개발·학습 문서, 이전 구현 자료를 구분하여 보관합니다.

처음 보는 작업자는 아래 기준으로 필요한 위치만 확인하면 됩니다.

| 필요한 것 | 확인할 위치 |
| --- | --- |
| 사용자 UI 디자인을 실행하거나 다른 서버에 적용 | [`user-ui-template/`](./user-ui-template/) |
| 현재 실제 웹·ROS 2 서버 | [`../Server/`](../Server/) |
| UI가 만들어진 과정과 HTML·CSS·JavaScript 공부 | [`docs/USER_UI_DEVELOPMENT_NOTES.md`](./docs/USER_UI_DEVELOPMENT_NOTES.md) |
| 프로젝트 요구사항과 역할 분담 | [`docs/`](./docs/) |
| 예전 C++ 서버·.NET Web HMI 자료 | [`legacy/`](./legacy/) |

## 폴더 구조

```text
Jaewook/
├─ README.md                  # 이 안내 문서
├─ user-ui-template/         # 현재 서버가 직접 사용하는 정적 사용자 UI
├─ docs/                     # 현재 참고할 개발·학습·프로젝트 문서
└─ legacy/                   # 현재 실행에는 사용하지 않는 과거 자료
```

## 현재 사용되는 사용자 UI

현재 사용자 UI는 별도의 UI 서버가 필요 없는 정적 웹입니다.

```text
index.html       화면 구조와 문구
app.css          색상, 배치, 반응형 디자인
app.js           버튼 동작, 서버 요청, 상태에 따른 화면 전환
robot-mark.svg   로봇 아이콘
```

실제 FastAPI 서버인 `../Server/server.py`가 `user-ui-template` 폴더를 직접 읽어 `http://서버주소:8080/`에서 제공합니다. 이전처럼 서버용 웹 폴더에 같은 파일을 한 번 더 복사할 필요가 없습니다. UI를 수정하면 이 폴더의 네 파일만 확인하면 됩니다.

## 현재 서버 연동 규격

브라우저는 웹을 제공한 서버의 상대 주소를 사용합니다. IP 주소와 로봇 좌표는 UI 파일에 저장하지 않습니다.

| 기능 | 요청 또는 상태 |
| --- | --- |
| 목적지 전송 | `POST /api/command`에 목적지 ID 전송 |
| 안내 취소 | `POST /api/command`에 `{"command":"cancel"}` 전송 |
| 수동모드·주행 상태 확인 | `GET /api/status` |
| 관리자 수동모드 변경 | 관리자 프로그램이 `POST /api/manual-mode` 사용 |

목적지 ID는 `restroom`, `room_301`, `room_302`, `elevator`를 사용합니다. 서버가 목적지 ID에 맞는 실제 좌표를 관리합니다.

## 최근 UI 변경: 여러 사용자 동시 조작 방지

서버 상태가 `sending`, `moving`, `canceling` 중 하나이면 로봇이 이미 다른 요청을 처리 중인 것으로 판단합니다. 이때 새 사용자가 목적지를 누르거나 안내 시작을 시도하면 다음 팝업을 표시하고 추가 명령을 보내지 않습니다.

```text
다른 사용자가 사용 중입니다
현재 안내가 끝난 후 다시 이용해 주세요.
```

상태 확인 직후 다른 사용자가 먼저 요청하는 상황도 있으므로, 서버가 `POST /api/command`에 HTTP `409 Conflict`를 반환하는 경우에도 같은 팝업을 표시합니다. 자세한 동작과 학습 설명은 [`docs/USER_UI_DEVELOPMENT_NOTES.md`](./docs/USER_UI_DEVELOPMENT_NOTES.md)를 참고합니다.

## 작업할 때 주의할 점

- 실제 배포 서버 코드는 `../Server`에 있습니다.
- `legacy`와 `hyunbeen`은 과거 구현을 공부하거나 변경 이력을 확인할 때만 참고합니다.
- `user-ui-template`은 원본인 동시에 현재 서버 실행본이므로, 별도 복사본을 만들지 않습니다.
- 새 문서는 목적에 맞게 `docs`에 추가하고, 이름만 보고 내용을 알 수 있게 작성합니다.
- 자동 생성되는 `bin`, `obj`, `publish` 파일은 소스 코드가 아닙니다.
