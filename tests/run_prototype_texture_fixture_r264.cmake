# R264: run the existing no-ROM S105/S108 source-selection regression inside
# an isolated, writable directory.  Preserve the test's actual assertions.
if(NOT DEFINED TEST_BINARY OR NOT EXISTS "${TEST_BINARY}")
  message(FATAL_ERROR "S105 test executable unavailable: ${TEST_BINARY}")
endif()
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef fixture_id)
set(fixture "${CMAKE_CURRENT_BINARY_DIR}/s105_fixture_${fixture_id}")
file(MAKE_DIRECTORY "${fixture}")
execute_process(
  COMMAND "${TEST_BINARY}" "${fixture}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error)
file(REMOVE_RECURSE "${fixture}")
if(NOT "${result}" STREQUAL "0")
  message(FATAL_ERROR "S105 test failed (${result})\n${output}\n${error}")
endif()
message(STATUS "${output}")
