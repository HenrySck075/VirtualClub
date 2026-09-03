cmake_minimum_required(VERSION 3.20)
project(VirtualClub VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable Qt's automatic compilation tools
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(pybind11 REQUIRED CONFIG)

file(GLOB_RECURSE SOURCES "src/*.cpp")
qt_add_executable(${PROJECT_NAME}
    ${SOURCES}
    resources.qrc
)


if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    include(FetchContent)
    FetchContent_Declare(
      cpptrace
      GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
      GIT_TAG        v1.0.4 # <HASH or TAG>
    )
    FetchContent_MakeAvailable(cpptrace)

# Needed for shared library builds on windows:  copy cpptrace.dll to the same directory as the
# executable for your_target
    if(WIN32)
      add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:cpptrace::cpptrace>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
      )
    endif()
    target_link_libraries(${PROJECT_NAME} PRIVATE cpptrace::cpptrace)
endif()

# Link Qt6 libraries
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets pybind11::embed)

# On finish compilation, if the build mode is debug, symlink build/assets to assets
# if on release, copy the folder

# i know qrc exists but its not really what im aiming for
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    )
else()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    )
endif()
