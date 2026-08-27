# 이전 구현 자료

이 폴더는 개발 과정에서 사용했던 과거 자료를 보존하기 위한 곳입니다. **현재 Spring Boot 서버 실행이나 배포에는 사용하지 않습니다.**

## 구성

| 위치 | 내용 |
| --- | --- |
| `dotnet-prototype/` | 초기 .NET 10 Blazor Web HMI 프로젝트와 당시 빌드·배포 결과물 |
| `docs/` | 이전 C++ API 서버와 정적 웹 배포를 기준으로 작성된 문서 |

현재 사용해야 할 파일은 다음과 같습니다.

- 사용자 UI 원본: `../user-ui-template/`
- 실제 Spring Boot 서버 적용본: `../../haktae/demo/src/main/resources/static/`
- 현재 ROS·서버 연동 문서: `../../haktae/demo/README_ROS_INTEGRATION.md`

이전 문서에는 `C++ 서버`, `GuideRobot.WebHmi`, `5000`, `7090` 등 현재 구조와 다른 내용이 포함될 수 있습니다. 새 구현의 기준으로 사용하지 마세요.

