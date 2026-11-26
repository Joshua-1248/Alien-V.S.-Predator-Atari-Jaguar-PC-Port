# The original source tree is immutable. Build a disposable, hash-checked PC view.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "AVP requires an out-of-source build directory")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(AVP_ORIGINAL_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(AVP_PORT_BINARY_ROOT "${CMAKE_CURRENT_BINARY_DIR}")
set(AVP_PORT_VIEW "${CMAKE_CURRENT_BINARY_DIR}/avp_port_source")
foreach(folder src include new tests third_party cmake tools scripts docs)
  file(GLOB_RECURSE inputs CONFIGURE_DEPENDS "${AVP_ORIGINAL_SOURCE_ROOT}/${folder}/*")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${inputs})
endforeach()
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
  "${AVP_ORIGINAL_SOURCE_ROOT}/new/source_integrity/check_game_source.py"
  RESULT_VARIABLE avp_guard_status)
if(NOT avp_guard_status EQUAL 0)
  message(FATAL_ERROR "Protected original source or compatibility layout changed")
endif()
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
  "${AVP_ORIGINAL_SOURCE_ROOT}/new/port_overlay/project_source.py" --output "${AVP_PORT_VIEW}"
  RESULT_VARIABLE avp_projection_status)
if(NOT avp_projection_status EQUAL 0)
  message(FATAL_ERROR "Original source verification or PC compatibility projection failed")
endif()
enable_testing()
add_subdirectory("${AVP_PORT_VIEW}" "${CMAKE_CURRENT_BINARY_DIR}/avp_port_build")
