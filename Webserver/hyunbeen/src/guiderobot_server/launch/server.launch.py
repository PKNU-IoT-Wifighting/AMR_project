from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    config = os.path.join(
        get_package_share_directory('guiderobot_server'),
        'config',
        'destinations.yaml',
    )

    return LaunchDescription([
        Node(
            package='guiderobot_server',
            executable='guiderobot_server_node',
            name='guiderobot_server_node',
            output='screen',
            parameters=[config],
        ),
    ])
