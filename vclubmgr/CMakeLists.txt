cmake_minimum_required(VERSION 3.15)
project(vclubmgr LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

add_library(${PROJECT_NAME} src/module.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${FUSE_LIBRARIES} nlohmann_json::nlohmann_json)
target_include_directories(${PROJECT_NAME} PRIVATE ${FUSE_INCLUDE_DIRS})
