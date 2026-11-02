foreach(test_target example_pool_test task_data_import_test)
  add_executable(${test_target} ${test_target}.cpp
                                ${PROJECT_SOURCE_DIR}/src/task_data_import.cpp)
  set_property(TARGET ${test_target} PROPERTY CXX_STANDARD 17)
  set_property(TARGET ${test_target} PROPERTY CXX_STANDARD_REQUIRED true)
  target_include_directories(${test_target}
                             PRIVATE ${PROJECT_SOURCE_DIR}/include)
  target_link_libraries(${test_target} PRIVATE isobus::Isobus isobus::Utility
                                               tinyxml2::tinyxml2)
endforeach()

add_test(NAME example_v3_pool_round_trip
         COMMAND example_pool_test
                 "${PROJECT_SOURCE_DIR}/examples/EXAMPLE.ddop" 3)

add_test(NAME example_v4_pool_round_trip
         COMMAND example_pool_test
                 "${PROJECT_SOURCE_DIR}/examples/EXAMPLE_V4.ddop" 4)

add_test(NAME task_data_import
         COMMAND task_data_import_test
                 "${PROJECT_SOURCE_DIR}/examples/TASKDATA.XML")

add_executable(
  ddop_validator_test ddop_validator_test.cpp
                      "${PROJECT_SOURCE_DIR}/src/ddop_validator.cpp")
set_property(TARGET ddop_validator_test PROPERTY CXX_STANDARD 17)
set_property(TARGET ddop_validator_test PROPERTY CXX_STANDARD_REQUIRED true)
target_include_directories(ddop_validator_test
                           PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(ddop_validator_test PRIVATE isobus::Isobus
                                                  isobus::Utility)

add_test(NAME ddop_validator_rules COMMAND ddop_validator_test)
