# 로컬 Nav2 서버 연동 시험 환경

이 폴더는 실제 배포 서버에 포함하지 않는 로컬 검증 도구입니다.

Nav2 공식 `tb3_sandbox` 예제 지도와 TurtleBot3 Gazebo 시뮬레이션을 사용해
`../Server`의 API 및 `NavigateToPose` 연동을 검사합니다.

최초 한 번 준비:

```bash
cd /home/ubuntu/wifiting_ws
./Nav2Test/setup.sh
```

전체 테스트 환경 실행:

```bash
./Nav2Test/start_all.sh
```

화면 없이 실행:

```bash
GUIDEROBOT_HEADLESS=true GUIDEROBOT_USE_RVIZ=false ./Nav2Test/start_all.sh
```

다른 터미널에서 상태 검사:

```bash
./Nav2Test/check.sh
```

웹은 `http://localhost:8080/`에서 확인하며 종료는 `Ctrl+C`입니다.
`config.yaml`의 좌표는 예제 지도 전용으로 실제 로봇에 사용하지 않습니다.
