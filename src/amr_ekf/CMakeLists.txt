cmake_minimum_required(VERSION 3.8)
project(amr_ekf)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ROS2
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(tf2_ros REQUIRED)

# 행렬 계산용 Eigen
find_package(Eigen3 REQUIRED NO_MODULE)


# 실행파일 생성
add_executable(amr_ekf_node
  src/ekf.cpp
  src/ekf_node.cpp
)


# 우리가 만든 header 경로
target_include_directories(amr_ekf_node
  PRIVATE
    include
)


# ROS2 의존성
ament_target_dependencies(amr_ekf_node
  rclcpp
  nav_msgs
  sensor_msgs
  geometry_msgs
  tf2_ros
)


# Eigen 연결
target_link_libraries(amr_ekf_node
  Eigen3::Eigen
)


# ros2 run amr_ekf amr_ekf_node 로 실행 가능하게 설치
install(
  TARGETS amr_ekf_node
  DESTINATION lib/${PROJECT_NAME}
)


ament_package()