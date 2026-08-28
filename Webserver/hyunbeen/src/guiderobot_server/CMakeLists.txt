cmake_minimum_required(VERSION 3.16)

project(guiderobot_server)

if(NOT CMAKE_CXX_STANDARD)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
add_compile_options(-Wall -Wextra)
endif()

# ============================================================

# ROS 2 dependencies

# ============================================================

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_action REQUIRED)
find_package(std_msgs REQUIRED)
find_package(nav2_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(SQLite3 REQUIRED)

# ============================================================

# cpp-httplib

# ============================================================

include(FetchContent)

FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.18.5
)

FetchContent_GetProperties(httplib)

if(NOT httplib_POPULATED)
  FetchContent_Populate(httplib)
endif()

add_library(httplib_interface INTERFACE)

target_include_directories(
  httplib_interface
  INTERFACE
    ${httplib_SOURCE_DIR}
)
# ============================================================

# Server executable

# ============================================================

add_executable(guiderobot_server_node
src/main.cpp
)

# ============================================================

# Include

# ============================================================

target_include_directories(
guiderobot_server_node
PRIVATE
include
)

# ============================================================

# Libraries

# ============================================================

target_link_libraries(
  guiderobot_server_node
  httplib_interface
  SQLite::SQLite3
)

# ============================================================

# ROS 2 dependencies

# ============================================================

ament_target_dependencies(
guiderobot_server_node
rclcpp
rclcpp_action
std_msgs
nav2_msgs
geometry_msgs
)

# ============================================================

# Install executable

# ============================================================

install(
TARGETS
guiderobot_server_node
DESTINATION
lib/${PROJECT_NAME}
)

# ============================================================

# Install config / launch

# ============================================================

install(
DIRECTORY
config
launch
DESTINATION
share/${PROJECT_NAME}
)

# ============================================================

# Web UI

# ============================================================

set(
GUIDEROBOT_WEB_SOURCE
"${CMAKE_CURRENT_SOURCE_DIR}/../../web"
)

if(NOT EXISTS "${GUIDEROBOT_WEB_SOURCE}/index.html")
message(FATAL_ERROR
"GuideRobot web UI not found: ${GUIDEROBOT_WEB_SOURCE}"
)
endif()

install(
DIRECTORY
"${GUIDEROBOT_WEB_SOURCE}/"
DESTINATION
share/${PROJECT_NAME}/web
)

# ============================================================

# Tests

# ============================================================

if(BUILD_TESTING)

find_package(ament_lint_auto REQUIRED)

ament_lint_auto_find_test_dependencies()

endif()

# ============================================================

# ROS 2 package

# ============================================================

ament_package()

