cmake_minimum_required(VERSION 3.15)
project(libbifuse)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(Python3_FIND_VIRTUALENV FIRST)
set(PYBIND11_FINDPYTHON ON)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 REQUIRED)

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
    set(FUSE_INCLUDE_DIRS "${WINFSP_DIR}/inc/fuse3")
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FUSE3 REQUIRED fuse3)
    set(FUSE_LIBRARIES ${FUSE3_LIBRARIES})
    set(FUSE_INCLUDE_DIRS ${FUSE3_INCLUDE_DIRS})
endif()

pybind11_add_module(${PROJECT_NAME} src/module.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE Python3::Python ${FUSE_LIBRARIES})
target_include_directories(${PROJECT_NAME} PRIVATE ${Python3_INCLUDE_DIRS} ${FUSE_INCLUDE_DIRS})
