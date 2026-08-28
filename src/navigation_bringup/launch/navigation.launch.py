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
    nav2_bringup_dir = get_package_share_directory('nav2_bringup')
    navigation_bringup_dir = get_package_share_directory(
        'navigation_bringup'
    )

    default_map = '/home/iot7272/Desktop/maps/my_map.yaml'

    default_params = os.path.join(
        navigation_bringup_dir,
        'config',
        'nav2_params.yaml'
    )

    # -------------------------------------------------
    # Launch arguments
    # -------------------------------------------------

    map_arg = DeclareLaunchArgument(
        'map',
        default_value=default_map,
        description='저장된 지도 YAML 경로'
    )

    params_arg = DeclareLaunchArgument(
        'params_file',
        default_value=default_params,
        description='Nav2 parameter YAML 경로'
    )

    use_sim_time_arg = DeclareLaunchArgument(
        'use_sim_time',
        default_value='false'
    )

    autostart_arg = DeclareLaunchArgument(
        'autostart',
        default_value='true'
    )

    # -------------------------------------------------
    # 1. micro-ROS agent
    # STM32 포트가 ttyACM1이면 이 부분만 변경
    # -------------------------------------------------

    micro_ros_agent = Node(
        package='micro_ros_agent',
        executable='micro_ros_agent',
        name='micro_ros_agent',
        output='screen',
        arguments=[
            'serial',
            '--dev', '/dev/ttyACM0',
            '-b', '115200',
            '-v6',
        ]
    )

    # -------------------------------------------------
    # 2. Slamtec A1M8 LiDAR
    # -------------------------------------------------

    lidar_node = Node(
        package='sllidar_ros2',
        executable='sllidar_node',
        name='sllidar_node',
        output='screen',
        parameters=[{
            'channel_type': 'serial',
            'serial_port': '/dev/ttyUSB0',
            'serial_baudrate': 115200,
            'frame_id': 'laser',
            'inverted': False,
            'angle_compensate': True,
            'scan_mode': 'Standard',
        }]
    )

    # -------------------------------------------------
    # 3. base_link → laser 고정 TF
    # 실제 라이다 장착 위치에 맞게 수정
    # -------------------------------------------------

    base_link_to_laser = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='base_link_to_laser',
        output='screen',
        arguments=[
            '--x', '0.15',
            '--y', '0.0',
            '--z', '0.20',
            '--roll', '0.0',
            '--pitch', '0.0',
            '--yaw', '0.0',
            '--frame-id', 'base_link',
            '--child-frame-id', 'laser',
        ]
    )

    # -------------------------------------------------
    # 4. 좌우 RPS → Odometry + odom → base_link
    # -------------------------------------------------

    wheel_odom_node = Node(
        package='wheel_odom_cpp',
        executable='wheel_odom_node',
        name='wheel_odom_node',
        output='screen',
        parameters=[{
            'wheel_radius': 0.0325,
            'wheel_separation': 0.24,
            'publish_rate': 50.0,

            'left_rps_topic': '/encoder/left_rps',
            'right_rps_topic': '/encoder/right_rps',

            'odom_topic': '/wheel/odom_raw',
            'odom_frame': 'odom',
            'base_frame': 'base_link',
        }]
    )

    # -------------------------------------------------
    # 5. 저장된 지도 + AMCL + Nav2
    # -------------------------------------------------

    nav2 = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(
                nav2_bringup_dir,
                'launch',
                'bringup_launch.py'
            )
        ),
        launch_arguments={
            'map': LaunchConfiguration('map'),
            'params_file': LaunchConfiguration('params_file'),
            'use_sim_time': LaunchConfiguration('use_sim_time'),
            'autostart': LaunchConfiguration('autostart'),
            'use_composition': 'False',
        }.items()
    )

    return LaunchDescription([
        map_arg,
        params_arg,
        use_sim_time_arg,
        autostart_arg,

        micro_ros_agent,

        # STM32 agent 연결 후 실행
        TimerAction(
            period=2.0,
            actions=[
                lidar_node,
                base_link_to_laser,
                wheel_odom_node,
            ]
        ),

        # 센서 및 odom TF가 준비된 후 Nav2 실행
        TimerAction(
            period=5.0,
            actions=[nav2]
        ),
    ])
