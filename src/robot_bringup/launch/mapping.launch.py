#!/usr/bin/env python3

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    package_share = get_package_share_directory('robot_bringup')
    nav2_share = get_package_share_directory('nav2_bringup')
    slam_share = get_package_share_directory('slam_toolbox')

    base_launch = os.path.join(package_share, 'launch', 'base.launch.py')
    nav_launch = os.path.join(nav2_share, 'launch', 'navigation_launch.py')
    slam_launch = os.path.join(slam_share, 'launch', 'online_async_launch.py')
    nav_params = os.path.join(package_share, 'config', 'nav2.yaml')
    slam_params = os.path.join(package_share, 'config', 'slam.yaml')

    return LaunchDescription([
        DeclareLaunchArgument(
            'agent_device',
            default_value=(
                '/dev/serial/by-id/'
                'usb-Raspberry_Pi_Pico_D2639C77633C2924-if00'
            ),
        ),
        DeclareLaunchArgument('agent_baud', default_value='115200'),
        DeclareLaunchArgument('start_agent', default_value='true'),
        DeclareLaunchArgument('lidar_launch', default_value='sllidar_a1_launch.py'),
        DeclareLaunchArgument(
            'lidar_port',
            default_value=(
                '/dev/serial/by-id/'
                'usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_'
                '0001-if00-port0'
            ),
        ),
        DeclareLaunchArgument('lidar_baud', default_value='115200'),
        DeclareLaunchArgument('start_lidar', default_value='true'),
        DeclareLaunchArgument('start_ekf', default_value='true'),
        DeclareLaunchArgument('autostart', default_value='true'),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(base_launch),
            launch_arguments={
                'agent_device': LaunchConfiguration('agent_device'),
                'agent_baud': LaunchConfiguration('agent_baud'),
                'start_agent': LaunchConfiguration('start_agent'),
                'lidar_launch': LaunchConfiguration('lidar_launch'),
                'lidar_port': LaunchConfiguration('lidar_port'),
                'lidar_baud': LaunchConfiguration('lidar_baud'),
                'start_lidar': LaunchConfiguration('start_lidar'),
                'start_ekf': LaunchConfiguration('start_ekf'),
            }.items(),
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(slam_launch),
            launch_arguments={
                'slam_params_file': slam_params,
                'use_sim_time': 'false',
                'autostart': LaunchConfiguration('autostart'),
            }.items(),
        ),
        Node(
            package='nav2_map_server',
            executable='map_saver_server',
            name='map_saver',
            output='screen',
            parameters=[nav_params, {'use_sim_time': False}],
        ),
        Node(
            package='nav2_lifecycle_manager',
            executable='lifecycle_manager',
            name='lifecycle_manager_map_saver',
            output='screen',
            parameters=[{
                'autostart': LaunchConfiguration('autostart'),
                'node_names': ['map_saver'],
            }],
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(nav_launch),
            launch_arguments={
                'use_sim_time': 'false',
                'autostart': LaunchConfiguration('autostart'),
                'params_file': nav_params,
                'use_composition': 'False',
                'use_respawn': 'True',
            }.items(),
        ),
    ])
