add_executable(example_pool_test example_pool_test.cpp)
set_property(TARGET example_pool_test PROPERTY CXX_STANDARD 17)
set_property(TARGET example_pool_test PROPERTY CXX_STANDARD_REQUIRED true)
target_link_libraries(example_pool_test PRIVATE isobus::Isobus isobus::Utility)

add_test(NAME example_pool_round_trip
         COMMAND example_pool_test "${PROJECT_SOURCE_DIR}/EXAMPLE.iop")

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
