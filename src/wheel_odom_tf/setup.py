from setuptools import find_packages, setup

package_name = 'wheel_odom_tf'

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
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='iot7272',
    maintainer_email='iot7272@example.com',
    description='Broadcast odom to base_link TF from wheel odometry',
    license='Apache-2.0',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'odom_tf_node = wheel_odom_tf.odom_tf_node:main',
        ],
    },
)
