import os
from glob import glob

from setuptools import find_packages, setup


package_name = 'mapping_bringup'


setup(
    name=package_name,
    version='0.0.1',
    packages=find_packages(exclude=['test']),

    data_files=[
        (
            'share/ament_index/resource_index/packages',
            ['resource/' + package_name]
        ),
        (
            'share/' + package_name,
            ['package.xml']
        ),
        (
            os.path.join('share', package_name, 'launch'),
            glob('launch/*.launch.py')
        ),
    ],

    install_requires=['setuptools'],
    zip_safe=True,

    maintainer='iot7272',
    maintainer_email='iot7272@example.com',

    description='Mapping bringup for RP2040, RPLIDAR A1 and slam_toolbox',
    license='Apache-2.0',

    tests_require=['pytest'],

    entry_points={
        'console_scripts': [],
    },
)
