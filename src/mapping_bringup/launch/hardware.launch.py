#!/usr/bin/env python3

import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    TimerAction,
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration

from launch_ros.actions import Node


def generate_launch_description():

    serial_port = LaunchConfiguration('serial_port')
    lidar_port = LaunchConfiguration('lidar_port')
    lidar_baudrate = LaunchConfiguration('lidar_baudrate')
    slam_params_file = LaunchConfiguration('slam_params_file')

    laser_x = LaunchConfiguration('laser_x')
    laser_y = LaunchConfiguration('laser_y')
    laser_z = LaunchConfiguration('laser_z')
    laser_roll = LaunchConfiguration('laser_roll')
    laser_pitch = LaunchConfiguration('laser_pitch')
    laser_yaw = LaunchConfiguration('laser_yaw')

    sllidar_launch = os.path.join(
        get_package_share_directory('sllidar_ros2'),
        'launch',
        'sllidar_a1_launch.py'
    )

    # RP2040 micro-ROS agent
    micro_ros_agent = Node(
        package='micro_ros_agent',
        executable='micro_ros_agent',
        name='micro_ros_agent',
        output='screen',
        arguments=[
            'serial',
            '--dev', serial_port,
            '-b', '115200',
            '-v6'
        ]
    )

    # A1M8 LiDAR
    lidar = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(sllidar_launch),
        launch_arguments={
            'serial_port': lidar_port,
            'serial_baudrate': lidar_baudrate,
            'frame_id': 'laser',
        }.items()
    )

    # /wheel/odom_raw -> odom -> base_link
    odom_tf = Node(
        package='wheel_odom_tf',
        executable='odom_tf_node',
        name='odom_tf_node',
        output='screen',
        parameters=[{
            'odom_topic': '/wheel/odom_raw',
            'odom_frame': 'odom',
            'base_frame': 'base_link',
        }]
    )

    # base_link -> laser static TF
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

    return LaunchDescription([
        DeclareLaunchArgument(
            'serial_port',
            default_value='/dev/ttyACM0',
            description='RP2040 serial port'
        ),

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

        DeclareLaunchArgument(
            'slam_params_file',
            default_value=os.path.expanduser(
                '~/ros2_ws/config/a1_mapping.yaml'
            ),
            description='slam_toolbox parameter file'
        ),

        DeclareLaunchArgument('laser_x', default_value='0.15'),
        DeclareLaunchArgument('laser_y', default_value='0.0'),
        DeclareLaunchArgument('laser_z', default_value='0.20'),
        DeclareLaunchArgument('laser_roll', default_value='0.0'),
        DeclareLaunchArgument('laser_pitch', default_value='0.0'),
        DeclareLaunchArgument('laser_yaw', default_value='0.0'),

        micro_ros_agent,
        lidar,
        laser_tf,

        # micro-ROS agent가 먼저 RP2040과 연결할 시간을 조금 줌
        TimerAction(
            period=2.0,
            actions=[odom_tf]
        ),

    ])
