from glob import glob
import os

from setuptools import find_packages, setup


package_name = 'robot_bringup'


setup(
    name=package_name,
    version='0.1.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        (
            'share/ament_index/resource_index/packages',
            ['resource/' + package_name],
        ),
        ('share/' + package_name, ['package.xml', 'README.md']),
        (
            os.path.join('share', package_name, 'launch'),
            glob(os.path.join('launch', '*.launch.py')),
        ),
        (
            os.path.join('share', package_name, 'config'),
            glob(os.path.join('config', '*.yaml')),
        ),
        (
            os.path.join('share', package_name, 'description'),
            glob(os.path.join('description', '*.xacro')),
        ),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='iot7272',
    maintainer_email='iot7272@todo.todo',
    description='Distributed Raspberry Pi Nav2/SLAM bringup with a micro-ROS STM32 base.',
    license='Apache-2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'send_goal = robot_bringup.send_goal:main',
        ],
    },
)
