cmake_minimum_required(VERSION 3.20)
project(VirtualClub VERSION 0.1.4 LANGUAGES C CXX)

if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "MSVC is not supported (i used some clang-specific syntax for ease of development sorry abt that). Please use a different compiler.")
endif()

add_compile_definitions(
  MVC_VERSION=${PROJECT_VERSION}
)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable Qt's automatic compilation tools
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Python REQUIRED COMPONENTS Interpreter Development)

message(STATUS "Python binary path: ${Python_EXECUTABLE}")

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia LinguistTools)

find_package(pybind11 REQUIRED CONFIG)


get_target_property(REAL_LUPDATE_PATH ${QT_CMAKE_EXPORT_NAMESPACE}::lupdate IMPORTED_LOCATION)
message(STATUS "Qt6::lupdate path: ${REAL_LUPDATE_PATH}")
if(NOT REAL_LUPDATE_PATH)
  get_target_property(_configs ${QT_CMAKE_EXPORT_NAMESPACE}::lupdate IMPORTED_CONFIGURATIONS)
    list(GET _configs 0 _first_config)
    get_target_property(REAL_LUPDATE_PATH ${QT_CMAKE_EXPORT_NAMESPACE}::lupdate IMPORTED_LOCATION_${_first_config})
endif()

# 2. Configure the Python wrapper
set(LUPDATE_WRAPPER_DEST "${CMAKE_CURRENT_BINARY_DIR}/lupdate_wrapper.py")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/lupdate2.py.in"
    "${LUPDATE_WRAPPER_DEST}"
    @ONLY
)

# 3. Make the script executable
execute_process(COMMAND chmod +x "${LUPDATE_WRAPPER_DEST}")

# 4. Create the custom target as an IMPORTED executable
#add_executable(CustomLupdateWrapper IMPORTED)
set_target_properties(${QT_CMAKE_EXPORT_NAMESPACE}::lupdate PROPERTIES
  IMPORTED_LOCATION "${LUPDATE_WRAPPER_DEST}"
  IMPORTED_LOCATION_RELEASE "${LUPDATE_WRAPPER_DEST}"
  IMPORTED_LOCATION_DEBUG "${LUPDATE_WRAPPER_DEST}"
  IMPORTED_LOCATION_RELWITHDEBINFO "${LUPDATE_WRAPPER_DEST}"
  IMPORTED_LOCATION_MINSIZEREL "${LUPDATE_WRAPPER_DEST}"
  IMPORTED_LOCATION_NOCONFIG "${LUPDATE_WRAPPER_DEST}"
)



qt_standard_project_setup(I18N_TRANSLATED_LANGUAGES vi)

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c") 
qt_add_executable(${PROJECT_NAME}
    ${SOURCES}
    #resources.qrc
)

qt_add_translations(
  ${PROJECT_NAME} 
  SOURCE_TARGETS ${PROJECT_NAME}
  #QM_OUTPUT_DIRECTORY translations
  TS_OUTPUT_DIRECTORY translations
  RESOURCE_PREFIX "/translations"

  #LUPDATE_TARGET CustomLupdateWrapper
  LUPDATE_OPTIONS
  "-tr-function-alias" "QT_TR_NOOP+=trNoop,QT_TRANSLATE_NOOP+=translateNoop"
  #      "tr+=$setLabelTextForAutoTL,tr+=$setLabelTextAndRegisterAutoTL,tr+=$createAutoTLLabelInline"
)



if(UNIX AND NOT APPLE)
  add_compile_definitions(ZIP_HAVE_SYMLINK=1)
endif()

file(GLOB_RECURSE TL_RC_FILES "resources/*.*")

qt_add_resources(${PROJECT_NAME} "tl_rc"
    BASE "resources"
    FILES ${TL_RC_FILES}
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

set(IS_ANDROID FALSE)
set(IS_TERMUX FALSE)
# --- Detect Standard Android NDK ---
if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(IS_ANDROID TRUE)
    message(STATUS "Environment: Android NDK")
endif()

# --- Detect Termux ---
if(DEFINED ENV{TERMUX_VERSION} OR ENV{PREFIX} MATCHES "com.termux")
    set(IS_TERMUX TRUE)
    message(STATUS "Environment: Termux")
    
    # Pass macro definition to C++ source
    add_compile_definitions(TERMUX_ENV)
endif()

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
elseif(NOT IS_ANDROID AND NOT IS_TERMUX)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FUSE3 REQUIRED fuse3)
  set(FUSE_LIBRARIES ${FUSE3_LIBRARIES})
  set(FUSE_INCLUDE_DIRS ${FUSE3_INCLUDE_DIRS})
  message(STATUS "fuse libs: ${FUSE_LIBRARIES}")
endif()


FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

target_link_libraries(${PROJECT_NAME} PRIVATE ${FUSE_LIBRARIES} nlohmann_json::nlohmann_json)

if(NOT IS_ANDROID AND NOT IS_TERMUX)
  target_include_directories(${PROJECT_NAME} PRIVATE ${FUSE_INCLUDE_DIRS})
endif()


# On finish compilation, if the build mode is debug, symlink build/assets to assets
# if on release, copy the folder

# i know qrc exists but its not really what im aiming for
if(NOT DEFINED ENV{CI})
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
endif()

# =============== Deployment ================
# Add deployment script generation for Qt6
qt_generate_deploy_app_script(
    TARGET ${PROJECT_NAME}
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_TOOL_OPTIONS NO_TRANSLATIONS
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_RPATH "\$ORIGIN/lib"
    INSTALL_RPATH "\$ORIGIN/lib"
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
)

install(SCRIPT ${deploy_script})
install(DIRECTORY assets DESTINATION .)

if(WIN32)
    # Target the DLL in the bin directory corresponding to architecture
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WINFSP_DLL "${WINFSP_DIR}/bin/winfsp-x64.dll")
    else()
        set(WINFSP_DLL "${WINFSP_DIR}/bin/winfsp-x86.dll")
    endif()

    if(EXISTS "${WINFSP_DLL}")
        install(FILES "${WINFSP_DLL}" DESTINATION .)
    endif()

    # in practice termux is my development environment and also this mostly runs on github actions so this wouldn't be happening
elseif(NOT IS_ANDROID AND NOT IS_TERMUX AND NOT APPLE)
    # On Linux CI, resolve absolute path to libfuse3.so using PkgConfig Link Libraries
    find_library(FUSE3_SO_PATH 
        NAMES fuse3 libfuse3.so.3 libfuse3.so
        HINTS ${FUSE3_LIBRARY_DIRS}
    )
    if(FUSE3_SO_PATH)
        # idk how well this work
        # 1. Get the actual binary file and its filename
        get_filename_component(FUSE3_REAL_PATH "${FUSE3_SO_PATH}" REALPATH)
        get_filename_component(FUSE3_REAL_NAME "${FUSE3_REAL_PATH}" NAME)

# 2. Install the real binary file into lib/
        install(FILES "${FUSE3_REAL_PATH}" DESTINATION lib)

        # 3. Explicitly create symlinks for required names (libfuse.so.3 and libfuse3.so.3)
        install(CODE "
            if(NOT \"${FUSE3_REAL_NAME}\" STREQUAL \"libfuse3.so.3\")
                file(CREATE_LINK \"${FUSE3_REAL_NAME}\"
                    \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/lib/libfuse3.so.3\"
                    SYMBOLIC)
            endif()
        ")
    endif()

    # Locate Qt's library directory and bundle its ICU dependencies
    get_target_property(QT_CORE_LIB_PATH Qt6::Core LOCATION)
    message(STATUS "Qt6::Core library path: ${QT_CORE_LIB_PATH}")
    if(QT_CORE_LIB_PATH)
        get_filename_component(QT_LIB_DIR "${QT_CORE_LIB_PATH}" DIRECTORY)
        get_filename_component(QT_ROOT_DIR "${QT_LIB_DIR}" DIRECTORY)
        file(GLOB QT_ICU_LIBS "${QT_ROOT_DIR}/libicu*.so*")
        if(QT_ICU_LIBS)
            install(FILES ${QT_ICU_LIBS} DESTINATION lib)
        endif()
    endif()
endif()


# Define target directory for minimal stdlib
if(WIN32)
    set(PY_STDLIB_DEST "Lib")
else()
    set(PY_STDLIB_DEST "lib/python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}")
endif()

# Core pure-Python modules required for sys, os, io, pickle, struct to function
set(MINIMAL_PYTHON_MODULES
    "os.py"
    "pickle.py"
    "io.py"
    "_collections_abc.py"
    "_sitebuiltins.py"
    "codecs.py"
    "copyreg.py"
    "sre_compile.py"
    "sre_constants.py"
    "sre_parse.py"
    "enum.py"
    "stat.py"
    "genericpath.py"
    "posixpath.py"
    "ntpath.py"
    "_compat_pickle.py"
    "struct.py"
)

# Copy individual files into build directory during build
foreach(PY_FILE ${MINIMAL_PYTHON_MODULES})
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${Python_STDLIB}/${PY_FILE}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${PY_STDLIB_DEST}/${PY_FILE}"
    )
    
    # Install step for final distribution
    install(
        FILES "${Python_STDLIB}/${PY_FILE}"
        DESTINATION "${PY_STDLIB_DEST}"
        OPTIONAL
    )
endforeach()

# Install C-runtime DLL/SO only, skipping lib-dynload
if(WIN32)
    if(Python_RUNTIME_LIBRARY)
        install(FILES "${Python_RUNTIME_LIBRARY}" DESTINATION .)
    endif()
else()
    if(Python_RUNTIME_LIBRARY)
        install(FILES "${Python_RUNTIME_LIBRARY}" DESTINATION lib)
    endif()
endif()
