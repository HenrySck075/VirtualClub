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
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)
find_package(pybind11 REQUIRED CONFIG)

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.hpp")
qt_add_executable(${PROJECT_NAME}
    ${SOURCES}
    resources.qrc
)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

include(FetchContent)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
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

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG yaml-cpp-0.9.0 # Can be a tag (yaml-cpp-x.x.x), a commit hash, or a branch name (master)
)
FetchContent_MakeAvailable(yaml-cpp)

target_link_libraries(${PROJECT_NAME} PUBLIC yaml-cpp::yaml-cpp) # The library or executable that require yaml-cpp library
# Link Qt6 libraries
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Multimedia pybind11::embed)

# For the VFS

if(WIN32)
    # WinFsp configuration
    if(DEFINED ENV{WINFSP_DIR})
        set(WINFSP_DIR "$ENV{WINFSP_DIR}")
    else()
        set(WINFSP_DIR "C:/Program Files (x86)/WinFsp")
    endif()

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WINFSP_LIB "${WINFSP_DIR}/lib/winfsp-x64.lib")
    else()
        set(WINFSP_LIB "${WINFSP_DIR}/lib/winfsp-x86.lib")
    endif()

    set(FUSE_LIBRARIES ${WINFSP_LIB})
    set(FUSE_INCLUDE_DIRS "${WINFSP_DIR}/inc")
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FUSE3 REQUIRED fuse3)
    set(FUSE_LIBRARIES ${FUSE3_LIBRARIES})
    set(FUSE_INCLUDE_DIRS ${FUSE3_INCLUDE_DIRS})
endif()


FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

target_link_libraries(${PROJECT_NAME} PRIVATE ${FUSE_LIBRARIES} nlohmann_json::nlohmann_json)
target_include_directories(${PROJECT_NAME} PRIVATE ${FUSE_INCLUDE_DIRS})


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
