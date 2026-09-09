#!/usr/bin/env python3

import os

from ament_index_python.packages import (
    get_package_prefix,
    get_package_share_directory,
    PackageNotFoundError,
)
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    LogInfo,
    OpaqueFunction,
)
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import (
    Command,
    FindExecutable,
    LaunchConfiguration,
    PathJoinSubstitution,
)
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from launch_ros.substitutions import FindPackageShare


def _is_true(value: str) -> bool:
    return value.strip().lower() in ('1', 'true', 'yes', 'on')


def _micro_ros_agent(context):
    if not _is_true(LaunchConfiguration('start_agent').perform(context)):
        return []

    try:
        get_package_prefix('micro_ros_agent')
    except PackageNotFoundError:
        return [
            LogInfo(
                msg=(
                    '[robot_bringup] micro_ros_agent is not installed. '
                    'LiDAR/Nav2 will continue, but STM32 topics and motor commands '
                    'will not be connected. See robot_bringup/README.md.'
                )
            )
        ]

    return [
        Node(
            package='micro_ros_agent',
            executable='micro_ros_agent',
            name='micro_ros_agent',
            output='screen',
            arguments=[
                'serial',
                '--dev',
                LaunchConfiguration('agent_device'),
                '-b',
                LaunchConfiguration('agent_baud'),
                '-v4',
            ],
        )
    ]


def generate_launch_description():
    package_share = get_package_share_directory('robot_bringup')
    xacro_file = os.path.join(
        package_share,
        'description',
        'robot.urdf.xacro',
    )
    ekf_file = os.path.join(package_share, 'config', 'ekf.yaml')

    robot_description = Command([FindExecutable(name='xacro'), ' ', xacro_file])

    return LaunchDescription([
        DeclareLaunchArgument(
            'start_agent',
            default_value='true',
            description='Start the STM32 micro-ROS serial agent',
        ),
        DeclareLaunchArgument(
            'agent_device',
            default_value=(
                '/dev/serial/by-id/'
                'usb-Raspberry_Pi_Pico_D2639C77633C2924-if00'
            ),
            description='Serial device connected to the micro-ROS MCU',
        ),
        DeclareLaunchArgument(
            'agent_baud',
            default_value='115200',
            description='Serial baud rate; MCU firmware must use the same value',
        ),
        DeclareLaunchArgument(
            'start_lidar',
            default_value='true',
            description='Start the SL Lidar driver',
        ),
        DeclareLaunchArgument(
            'lidar_launch',
            default_value='sllidar_a1_launch.py',
            description='Launch file from sllidar_ros2/launch for the fitted model',
        ),
        DeclareLaunchArgument(
            'lidar_port',
            default_value=(
                '/dev/serial/by-id/'
                'usb-Silicon_Labs_CP2102_USB_to_UART_Bridge_Controller_'
                '0001-if00-port0'
            ),
            description='Stable serial-by-id path for the LiDAR',
        ),
        DeclareLaunchArgument(
            'lidar_baud',
            default_value='115200',
            description='LiDAR baud rate for the selected model',
        ),
        DeclareLaunchArgument(
            'start_ekf',
            default_value='true',
            description='Fuse /wheel/odom and /imu/data_raw into /odom',
        ),
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            name='robot_state_publisher',
            output='screen',
            parameters=[{
                'robot_description': ParameterValue(
                    robot_description,
                    value_type=str,
                ),
                'use_sim_time': False,
            }],
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                PathJoinSubstitution([
                    FindPackageShare('sllidar_ros2'),
                    'launch',
                    LaunchConfiguration('lidar_launch'),
                ])
            ),
            condition=IfCondition(LaunchConfiguration('start_lidar')),
            launch_arguments={
                'serial_port': LaunchConfiguration('lidar_port'),
                'serial_baudrate': LaunchConfiguration('lidar_baud'),
                'frame_id': 'laser',
            }.items(),
        ),
        Node(
            package='robot_localization',
            executable='ekf_node',
            name='ekf_filter_node',
            output='screen',
            condition=IfCondition(LaunchConfiguration('start_ekf')),
            parameters=[ekf_file, {'use_sim_time': False}],
            remappings=[('odometry/filtered', 'odom')],
        ),
        OpaqueFunction(function=_micro_ros_agent),
    ])
