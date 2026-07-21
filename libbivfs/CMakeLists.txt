cmake_minimum_required(VERSION 3.15)
project(libbifuse)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(Python3_FIND_VIRTUALENV FIRST)
set(PYBIND11_FINDPYTHON ON)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
# Locates pybind11 installed on your system
find_package(pybind11 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)

# Compiles your C++ code directly into a Python-importable shared library
pybind11_add_module(${PROJECT_NAME} src/module.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE Python3::Python ${FUSE3_LIBRARIES})
target_include_directories(${PROJECT_NAME} PRIVATE ${Python3_INCLUDE_DIRS} ${FUSE3_INCLUDE_DIRS})
