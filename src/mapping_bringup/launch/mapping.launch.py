#!/usr/bin/env python3

import os
from launch.event_handlers import OnProcessExit
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    TimerAction,
    ExecuteProcess,
    RegisterEventHandler,
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    lidar_port = LaunchConfiguration('lidar_port')
    lidar_baudrate = LaunchConfiguration('lidar_baudrate')

    laser_x = LaunchConfiguration('laser_x')
    laser_y = LaunchConfiguration('laser_y')
    laser_z = LaunchConfiguration('laser_z')
    laser_roll = LaunchConfiguration('laser_roll')
    laser_pitch = LaunchConfiguration('laser_pitch')
    laser_yaw = LaunchConfiguration('laser_yaw')

    map_file = LaunchConfiguration('map')
    params_file = LaunchConfiguration('params_file')
    use_sim_time = LaunchConfiguration('use_sim_time')

    sllidar_launch = os.path.join(
        get_package_share_directory('sllidar_ros2'),
        'launch',
        'sllidar_a1_launch.py'
    )

    nav2_launch_file = os.path.join(
        get_package_share_directory('mapping_bringup'),
        'launch',
        'bringup_no_docking.launch.py'
    )


    # RPLIDAR A1M8
    lidar = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(sllidar_launch),
        launch_arguments={
            'serial_port': lidar_port,
            'serial_baudrate': lidar_baudrate,
            'frame_id': 'laser',
        }.items()
    )

    # Encoder RPS -> /wheel/odom_raw + odom -> base_link
    wheel_odom_node = Node(
        package='wheel_odom_cpp',
        executable='wheel_odom_node',
        name='wheel_odom_node',
        output='screen',
        parameters=[{
            'wheel_radius': 0.0325,
            'wheel_separation': 0.24,
            'left_rps_topic': '/encoder/left_rps',
            'right_rps_topic': '/encoder/right_rps',
            'odom_topic': '/wheel/odom_raw',
            'odom_frame': 'odom',
            'base_frame': 'base_link',
            'use_sim_time': use_sim_time,
        }]
    )
    ekf_node = Node(
        package='amr_ekf',
        executable='amr_ekf_node',
        name='amr_ekf_node',
        output='screen',
    )

    network_watchdog_node = Node(
        package='network_watchdog',
        executable='network_watchdog_node',
        name='network_watchdog',
        output='screen',
    )

    micro_ros_agent = Node(
        package='micro_ros_agent',
        executable='micro_ros_agent',
        name='micro_ros_agent',
        output='screen',
        arguments=[
            'serial',
            '--dev', '/dev/ttyACM0',
            '-b', '115200',
            '-v4',
        ],
        respawn=True,
        respawn_delay=2.0,
    )
    # base_link -> laser
    laser_tf = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='base_link_to_laser',
        output='screen',
        arguments=[
            '--x', laser_x,
            '--y', laser_y,
            '--z', laser_z,
            '--roll', laser_roll,
            '--pitch', laser_pitch,
            '--yaw', laser_yaw,
            '--frame-id', 'base_link',
            '--child-frame-id', 'laser',
        ]
    )

    # Saved-map navigation only. slam_toolbox is intentionally not launched.
    nav2 = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(nav2_launch_file),
        launch_arguments={
            'map': map_file,
            'params_file': params_file,
            'use_sim_time': use_sim_time,
            'autostart': 'true',
            'use_composition': 'False',
            'use_respawn': 'false',
        }.items()
    )

    # Give lidar, odometry, and TF time to start first.
        # 센서와 TF가 실제로 준비될 때까지 순서대로 대기
    wait_for_robot = ExecuteProcess(
        cmd=[
            'bash',
            '-c',
            '''
            echo "[BRINGUP] Waiting for /scan..."
            until timeout 2 ros2 topic echo --once /scan > /dev/null 2>&1; do
                sleep 1
            done
            echo "[BRINGUP] /scan ready"

            echo "[BRINGUP] Waiting for left encoder..."
            until timeout 3 ros2 topic echo --once /encoder/left_rps > /dev/null 2>&1; do
                sleep 1
            done
            echo "[BRINGUP] left encoder ready"

            echo "[BRINGUP] Waiting for right encoder..."
            until timeout 3 ros2 topic echo --once /encoder/right_rps > /dev/null 2>&1; do
                sleep 1
            done
            echo "[BRINGUP] right encoder ready"

            echo "[BRINGUP] Waiting for odom -> base_link TF..."
            until timeout 5 ros2 run tf2_ros tf2_echo odom base_link 2>/dev/null \
                | grep -m1 -q "Translation"; do
                sleep 1
            done
            echo "[BRINGUP] odom -> base_link ready"

            echo "[BRINGUP] Waiting for base_link -> laser TF..."
            until timeout 8 ros2 run tf2_ros tf2_echo base_link laser 2>/dev/null \
                | grep -m1 -q "Translation"; do
                sleep 1
            done
            echo "[BRINGUP] Hardware ready"
            '''
        ],
        output='screen',
    )

    start_nav2_after_ready = RegisterEventHandler(
        OnProcessExit(
            target_action=wait_for_robot,
            on_exit=[nav2],
        )
    )
    return LaunchDescription([
        DeclareLaunchArgument(
            'lidar_port',
            default_value='/dev/ttyUSB0',
            description='RPLIDAR serial port'
        ),
        DeclareLaunchArgument(
            'lidar_baudrate',
            default_value='115200',
            description='RPLIDAR A1 baudrate'
        ),

        DeclareLaunchArgument('laser_x', default_value='0.11'),
        DeclareLaunchArgument('laser_y', default_value='0.0'),
        DeclareLaunchArgument('laser_z', default_value='0.17'),
        DeclareLaunchArgument('laser_roll', default_value='0.0'),
        DeclareLaunchArgument('laser_pitch', default_value='0.0'),
        DeclareLaunchArgument('laser_yaw', default_value='0.0'),

        DeclareLaunchArgument(
            'map',
            default_value='/home/iot7272/Desktop/maps/my_map.yaml',
            description='Saved map YAML file'
        ),
        DeclareLaunchArgument(
            'params_file',
            default_value='/home/iot7272/ros2_ws/src/navigation_bringup/config/nav2_params.yaml',
            description='Nav2 parameter YAML file'
        ),
        DeclareLaunchArgument(
            'use_sim_time',
            default_value='false'
        ),
	micro_ros_agent,
        lidar,
        wheel_odom_node,
        ekf_node,
	network_watchdog_node,
        laser_tf,
        wait_for_robot,
        start_nav2_after_ready,
    ])
