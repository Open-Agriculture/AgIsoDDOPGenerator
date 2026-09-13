add_executable(example_pool_test example_pool_test.cpp)
set_property(TARGET example_pool_test PROPERTY CXX_STANDARD 17)
set_property(TARGET example_pool_test PROPERTY CXX_STANDARD_REQUIRED true)
target_link_libraries(example_pool_test PRIVATE isobus::Isobus isobus::Utility)

add_test(NAME example_v3_pool_round_trip
         COMMAND example_pool_test "${PROJECT_SOURCE_DIR}/EXAMPLE.iop" 3)

add_test(NAME example_v4_pool_round_trip
         COMMAND example_pool_test "${PROJECT_SOURCE_DIR}/EXAMPLE_V4.iop" 4)
