cmake_minimum_required(VERSION 3.16)

project(AgIsoDDOPGenerator
        VERSION 1.0
        LANGUAGES CXX
        DESCRIPTION "DDOP Generator based on AgIsoStack++"
)

add_executable(AgIsoDDOPGenerator)
set_property(TARGET AgIsoDDOPGenerator PROPERTY CXX_STANDARD 17)
set_property(TARGET AgIsoDDOPGenerator PROPERTY CXX_STANDARD_REQUIRED true)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
set(CPACK_DEBIAN_FILENAME DEB-DEFAULT)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_PACKAGE_VENDOR "Open-Agriculture")
set(CPACK_PACKAGE_CONTACT ${contact})
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/Open-Agriculture")
set(CPACK_PACKAGE_VERSION ${CMAKE_PROJECT_VERSION})
if(CPack_CMake_INCLUDED EQUAL 1)
    include(CPack)
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)

add_subdirectory(submodules/agisostack)
add_subdirectory(submodules/sdl)

target_sources(AgIsoDDOPGenerator
               PRIVATE
               src/main.cpp
               src/gui.cpp
            
               submodules/imgui/imgui.cpp
               submodules/imgui/imgui_demo.cpp
               submodules/imgui/imgui_draw.cpp
               submodules/imgui/imgui_tables.cpp
               submodules/imgui/imgui_widgets.cpp
               submodules/imgui/backends/imgui_impl_sdl2.cpp
               submodules/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(AgIsoDDOPGenerator
                           PUBLIC
                           "include"
                           submodules/imgui
                           submodules/imgui/backends
                           submodules/sdl/include
)

target_link_libraries(AgIsoDDOPGenerator
                      PRIVATE
                      isobus::Isobus
                      isobus::Utility
                      OpenGL::GL
                      SDL2 
                      SDL2main
                      ${CMAKE_DL_LIBS}
)

install(TARGETS AgIsoDDOPGenerator RUNTIME DESTINATION bin)

if (WIN32)
    add_custom_command(
        TARGET AgIsoDDOPGenerator POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:SDL2::SDL2>" "$<TARGET_FILE_DIR:AgIsoDDOPGenerator>"
        VERBATIM
    )
endif()
