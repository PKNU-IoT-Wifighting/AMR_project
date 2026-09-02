# Jaewook 사용자 UI 작업 공간

이 폴더는 안내 로봇의 **사용자 디스플레이 UI 원본**, 개발·학습 문서, 이전 구현 자료를 구분하여 보관합니다.

처음 보는 작업자는 아래 기준으로 필요한 위치만 확인하면 됩니다.

| 필요한 것 | 확인할 위치 |
| --- | --- |
| 사용자 UI 디자인을 다른 서버에 적용 | [`user-ui-template/`](./user-ui-template/) |
| 현재 ROS 2 C++ 서버에서 실제 실행되는 화면 | [`../hyunbeen/web/`](../hyunbeen/web/) |
| UI가 만들어진 과정과 HTML·CSS·JavaScript 공부 | [`docs/USER_UI_DEVELOPMENT_NOTES.md`](./docs/USER_UI_DEVELOPMENT_NOTES.md) |
| 프로젝트 요구사항과 역할 분담 | [`docs/`](./docs/) |
| 예전 C++ 서버·.NET Web HMI 자료 | [`legacy/`](./legacy/) |

## 폴더 구조

```text
Jaewook/
├─ README.md                  # 이 안내 문서
├─ user-ui-template/         # 재사용 가능한 정적 사용자 UI 원본
├─ docs/                     # 현재 참고할 개발·학습·프로젝트 문서
└─ legacy/                   # 현재 실행에는 사용하지 않는 과거 자료
```

## 현재 사용되는 사용자 UI

현재 사용자 UI는 별도의 웹 서버가 필요 없는 정적 웹입니다.

```text
index.html       화면 구조와 문구
app.css          색상, 배치, 반응형 디자인
app.js           버튼 동작, 서버 요청, 상태에 따른 화면 전환
robot-mark.svg   로봇 아이콘
```

디자인 원본은 `user-ui-template`에 보관하고, ROS 2 C++ 서버에서 실제 실행되는 복사본은 `../hyunbeen/web`에 둡니다. 두 위치의 네 파일은 2026-08-27 기준으로 동일합니다.

UI를 수정할 때는 디자인 원본과 서버 적용본이 서로 달라지지 않도록 같은 변경을 반영하고 함께 확인합니다.

## 현재 서버 연동 규격

브라우저는 웹을 제공한 ROS 2 C++ 서버의 상대 주소를 사용합니다. IP 주소와 로봇 좌표는 UI 파일에 저장하지 않습니다.

| 기능 | 요청 또는 상태 |
| --- | --- |
| 목적지 전송 | `POST /api/command` |
| 안내 취소 | `POST /api/command` |
| 수동모드·주행 상태 확인 | `GET /api/status` |

목적지 ID는 `restroom`, `room_301`, `room_302`, `elevator`를 사용합니다. 서버가 목적지 ID에 맞는 실제 좌표를 관리합니다.

## 작업할 때 주의할 점

- 실제 배포 서버 코드는 `hyunbeen/src/guiderobot_server`에 있습니다.
- `legacy`는 과거 구현을 공부하거나 변경 이력을 확인할 때만 참고합니다.
- 새 문서는 목적에 맞게 `docs`에 추가하고, 이름만 보고 내용을 알 수 있게 작성합니다.
- 자동 생성되는 `bin`, `obj`, `publish` 파일은 소스 코드가 아닙니다.
