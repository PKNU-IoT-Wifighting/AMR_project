include("/home/ubuntu/wifiting_ws/ManagerScreen/build/Desktop_Qt_6_11_1_Debug/.qt/QtDeploySupport.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ManagerScreen-plugins.cmake" OPTIONAL)
set(__QT_DEPLOY_I18N_CATALOGS "qtbase")

qt6_deploy_runtime_dependencies(
    EXECUTABLE "/home/ubuntu/wifiting_ws/ManagerScreen/build/Desktop_Qt_6_11_1_Debug/ManagerScreen"
    GENERATE_QT_CONF
)
