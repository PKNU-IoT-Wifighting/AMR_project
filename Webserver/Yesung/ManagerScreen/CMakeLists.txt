cmake_minimum_required(VERSION 3.19)
project(ManagerScreen VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.4.2 REQUIRED COMPONENTS Core Network Widgets LinguistTools)

qt_standard_project_setup()

qt_add_executable(ManagerScreen
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/velocity_publisher.py
    ${CMAKE_CURRENT_BINARY_DIR}/velocity_publisher.py
    COPYONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/ros_status_monitor.py
    ${CMAKE_CURRENT_BINARY_DIR}/ros_status_monitor.py
    COPYONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmd_vel_reader.py
    ${CMAKE_CURRENT_BINARY_DIR}/cmd_vel_reader.py
    COPYONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/manager-screen.in
    ${CMAKE_CURRENT_BINARY_DIR}/manager-screen
    @ONLY
)

qt_add_translations(
    TARGETS ManagerScreen
    TS_FILES ManagerScreen_ko_KR.ts
)

target_link_libraries(ManagerScreen
    PRIVATE
        Qt::Core
        Qt::Network
        Qt::Widgets
)

install(TARGETS ManagerScreen
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(PROGRAMS
    ${CMAKE_CURRENT_BINARY_DIR}/manager-screen
    ${CMAKE_CURRENT_SOURCE_DIR}/velocity_publisher.py
    ${CMAKE_CURRENT_SOURCE_DIR}/ros_status_monitor.py
    ${CMAKE_CURRENT_SOURCE_DIR}/cmd_vel_reader.py
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Desktop application entry. It launches the wrapper above so ROS 2 Jazzy is
# sourced even when the user starts the app from the Ubuntu application menu.
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/manager-screen.desktop
    DESTINATION /usr/share/applications
)
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/manager-screen.svg
    DESTINATION /usr/share/icons/hicolor/scalable/apps
)

qt_generate_deploy_app_script(
    TARGET ManagerScreen
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# Build an installable Ubuntu 24.04 / ROS 2 Jazzy Debian package with:
#   cmake --build <build-dir> --target package
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "manager-screen")
set(CPACK_PACKAGE_VENDOR "Wifiting")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Qt manager screen for the ROS 2 guide robot")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_CONTACT "Wifiting Team")
set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/ManagerScreen")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "python3, ros-jazzy-ros2cli, ros-jazzy-rclpy, ros-jazzy-std-msgs, ros-jazzy-geometry-msgs, gstreamer1.0-tools, gstreamer1.0-plugins-base, gstreamer1.0-plugins-good, gstreamer1.0-plugins-bad, gstreamer1.0-libav")

include(CPack)
