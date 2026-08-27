cmake_minimum_required(VERSION 3.16)
project(guiderobot_server)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(std_msgs REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)

# --- cpp-httplib (헤더 온리) : 빌드 시 자동으로 받아온다 ---------------------
include(FetchContent)
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.18.5
)
FetchContent_MakeAvailable(httplib)
# ----------------------------------------------------------------------------

add_executable(guiderobot_server_node
  src/main.cpp
)

target_include_directories(guiderobot_server_node PRIVATE
  include
)

target_link_libraries(guiderobot_server_node
  httplib::httplib
)

ament_target_dependencies(guiderobot_server_node
  rclcpp
  rclcpp_action
  std_msgs
  nav2_msgs
  geometry_msgs
)

install(TARGETS
  guiderobot_server_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY
  config
  launch
  DESTINATION share/${PROJECT_NAME}
)

# 정적 웹 파일(GuideRobot.StaticHmi)을 이 패키지 share 폴더 아래로 함께 설치하고 싶다면
# 팀 저장소 구조에 맞춰 아래 줄의 경로를 조정해서 사용하면 된다.
# install(DIRECTORY
#   web
#   DESTINATION share/${PROJECT_NAME}
# )

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
