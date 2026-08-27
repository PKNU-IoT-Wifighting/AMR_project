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

# 워크스페이스 루트의 web 폴더를 ROS 2 패키지와 함께 설치한다.
# 설치본은 share/guiderobot_server/web에 위치하며 launch 파일이 자동으로 사용한다.
set(GUIDEROBOT_WEB_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../web")
if(NOT EXISTS "${GUIDEROBOT_WEB_SOURCE}/index.html")
  message(FATAL_ERROR "GuideRobot web UI not found: ${GUIDEROBOT_WEB_SOURCE}")
endif()

install(DIRECTORY
  "${GUIDEROBOT_WEB_SOURCE}/"
  DESTINATION share/${PROJECT_NAME}/web
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
