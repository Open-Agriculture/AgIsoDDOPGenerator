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
         COMMAND example_pool_test "${PROJECT_SOURCE_DIR}/EXAMPLE.ddop" 3)

add_test(NAME example_v4_pool_round_trip
         COMMAND example_pool_test "${PROJECT_SOURCE_DIR}/EXAMPLE_V4.ddop" 4)

add_test(NAME task_data_import COMMAND task_data_import_test)
