from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    package_share = get_package_share_directory('guiderobot_server')
    config = os.path.join(
        package_share,
        'config',
        'destinations.yaml',
    )
    static_web_root = os.path.join(package_share, 'web')

    return LaunchDescription([
        Node(
            package='guiderobot_server',
            executable='guiderobot_server_node',
            name='guiderobot_server_node',
            output='screen',
            parameters=[config, {'static_web_root': static_web_root}],
        ),
    ])
