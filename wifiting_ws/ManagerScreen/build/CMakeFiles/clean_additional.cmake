# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles/ManagerScreen_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/ManagerScreen_autogen.dir/ParseCache.txt"
  "ManagerScreen_autogen"
  )
endif()
