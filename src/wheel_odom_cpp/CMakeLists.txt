cmake_minimum_required(VERSION 3.8)
project(wheel_odom_cpp)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(tf2_ros REQUIRED)

add_executable(wheel_odom_node
  src/wheel_odom_node.cpp
)

ament_target_dependencies(wheel_odom_node
  rclcpp
  std_msgs
  nav_msgs
  geometry_msgs
  tf2_ros
)

target_compile_features(wheel_odom_node PUBLIC cxx_std_17)

install(
  TARGETS wheel_odom_node
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
