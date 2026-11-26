cmake_minimum_required(VERSION 3.30)
project(VirtualClub VERSION 0.1.0 LANGUAGES C CXX)

# who tf uses modules here
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

include(GNUInstallDirs)
if(WIN32)
  # Keep the Windows runtime flat in the package root, alongside the executable.
  # DLL lookup on Windows is based around the executable rather than GNUInstallDirs'
  # bin/lib convention, and Qt's deploy script uses CMAKE_INSTALL_BINDIR too.
  set(CMAKE_INSTALL_BINDIR ".")
endif()
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "MSVC is not supported. Please use Clang instead. (i used clang extensively for development sorry abt that)")
endif()

# assume Debug if unset
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if(NOT DEFINED MVC_VERSION)
  set(MVC_VERSION "${PROJECT_VERSION}")
endif()

if(NOT DEFINED MVC_CHANNEL)
  set(MVC_CHANNEL "stable")
endif()

if(NOT DEFINED MVC_BUILD_ID)
  execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE MVC_BUILD_ID
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
endif()

# if build type is not release, ask git for the current commit hash and append it to the version string
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  find_package(Git REQUIRED)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  set(MVC_VERSION "${PROJECT_VERSION}-local-${GIT_COMMIT_HASH}")
  set(MVC_CHANNEL "local")
endif()



add_compile_definitions(
  MVC_VERSION="${MVC_VERSION}"
  MVC_CHANNEL="${MVC_CHANNEL}"
  MVC_BUILD_ID="${MVC_BUILD_ID}"
  MVC_LAUNCHER_VERSION="${PROJECT_VERSION}"
)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable Qt's automatic compilation tools

function(target_declare_qt_autogen target)
    set_target_properties(${target} PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
        AUTORCC ON
    )
endfunction()


if(CMAKE_CROSSCOMPILING AND MINGW)
  find_package(Python REQUIRED COMPONENTS Interpreter)

  set(MINGW_PREFIX "/usr/x86_64-w64-mingw32")
  set(MINGW_PY_VER_MAJOR 3)
  set(MINGW_PY_VER_MINOR 14)

  set(Python_VERSION_MAJOR "${MINGW_PY_VER_MAJOR}")
  set(Python_VERSION_MINOR "${MINGW_PY_VER_MINOR}")
  set(Python_INCLUDE_DIRS "${MINGW_PREFIX}/include/python${MINGW_PY_VER_MAJOR}${MINGW_PY_VER_MINOR}")
  set(Python_LIBRARIES "${MINGW_PREFIX}/lib/libpython${MINGW_PY_VER_MAJOR}${MINGW_PY_VER_MINOR}.dll.a")
  set(Python_MINGW_STDLIB "${MINGW_PREFIX}/lib/python${MINGW_PY_VER_MAJOR}${MINGW_PY_VER_MINOR}")
  set(Python_RUNTIME_LIBRARY "${MINGW_PREFIX}/bin/python${MINGW_PY_VER_MAJOR}${MINGW_PY_VER_MINOR}.dll")

  set(PYBIND11_NOPYTHON ON CACHE BOOL "" FORCE)
else()
  find_package(Python REQUIRED COMPONENTS Interpreter Development)
  message(STATUS "Python binary path: ${Python_EXECUTABLE}")
endif()

# Find Qt6 components
# (also requires ImageFormats plugin for the online mods browser)
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia Svg LinguistTools)

find_package(pybind11 REQUIRED CONFIG)
find_package(LibArchive REQUIRED)

qt_standard_project_setup(I18N_TRANSLATED_LANGUAGES vi)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.c" "src/*.hpp") 
list(REMOVE_ITEM SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vfs/vfs_fuse3.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vfs/vfs_winfsp.cpp"
)
if(UNIX AND NOT APPLE AND NOT ANDROID AND
   NOT CMAKE_SYSTEM_NAME STREQUAL "Android" AND
   NOT DEFINED ENV{TERMUX_VERSION} AND
   NOT "$ENV{PREFIX}" MATCHES "com.termux")
 message(STATUS "Using the Fuse3 implementation")
  list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/vfs/vfs.cpp")
  list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/vfs/vfs_fuse3.cpp")
elseif(WIN32)
  message(STATUS "Using the WinFsp implementation")
  list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/vfs/vfs.cpp")
  list(APPEND SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/vfs/vfs_winfsp.cpp")
endif()
qt_add_executable(${PROJECT_NAME}
    ${SOURCES}
    #resources.qrc
)

if(CMAKE_CROSSCOMPILING AND MINGW)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${Python_LIBRARIES})
  target_include_directories(${PROJECT_NAME} PRIVATE ${Python_INCLUDE_DIRS})
endif()

target_declare_qt_autogen(${PROJECT_NAME})
file(GLOB_RECURSE UPDATER_SOURCES CONFIGURE_DEPENDS "updater_src/*.cpp" "updater_src/*.hpp")
qt_add_executable(VirtualClub-Updater ${UPDATER_SOURCES})
target_compile_features(VirtualClub-Updater PRIVATE cxx_std_20)
target_include_directories(VirtualClub-Updater PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/updater_src)
target_link_libraries(VirtualClub-Updater PRIVATE Qt6::Core Qt6::Widgets LibArchive::LibArchive)

target_declare_qt_autogen(VirtualClub-Updater)

qt_add_translations(
  ${PROJECT_NAME} 
  SOURCE_TARGETS ${PROJECT_NAME}
  #QM_OUTPUT_DIRECTORY translations
  TS_OUTPUT_DIRECTORY translations
  RESOURCE_PREFIX "/translations"

  #LUPDATE_TARGET CustomLupdateWrapper
)


if(WIN32 AND MVC_CHANNEL STREQUAL "stable")
  set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
  set_target_properties(VirtualClub-Updater PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)

endif()

target_precompile_headers(
  ${PROJECT_NAME} PRIVATE 
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h>"
)


if(UNIX AND NOT APPLE)
  add_compile_definitions(ZIP_HAVE_SYMLINK=1)
endif()

# this thing is for windows 
#add_compile_definitions(WIN32_LEAN_AND_MEAN)

file(GLOB_RECURSE RC_FILES CONFIGURE_DEPENDS "resources/*.*")

qt_add_resources(${PROJECT_NAME} "rc"
    BASE "resources"
    FILES ${RC_FILES}
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
    if(TARGET cpptrace-lib)
      set_target_properties(cpptrace-lib PROPERTIES CXX_SCAN_FOR_MODULES OFF)
    endif()

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

if(MINGW)
  if(TARGET yaml-cpp)
    get_target_property(yaml_type yaml-cpp TYPE)
    if(yaml_type STREQUAL "STATIC_LIBRARY")
      target_compile_definitions(yaml-cpp PUBLIC YAML_CPP_STATIC_DEFINE)
    endif()
  endif()
endif()

set(LEXBOR_INSTALL_HEADERS OFF CACHE BOOL "" FORCE)
set(LEXBOR_BUILD_STATIC OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    lexbor
    GIT_REPOSITORY https://github.com/lexbor/lexbor.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(lexbor)

# Lexbor is third-party code and emits excessive compiler warnings in Release builds.
# Keep warnings focused on VirtualClub sources.
if(TARGET lexbor)
  if(MSVC OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(lexbor PRIVATE /W0)
  else()
    target_compile_options(lexbor PRIVATE -w)
  endif()
endif()

target_link_libraries(
  ${PROJECT_NAME} PRIVATE 
  yaml-cpp::yaml-cpp lexbor
  Qt6::Widgets Qt6::Multimedia Qt6::Svg pybind11::embed
  LibArchive::LibArchive
)
if(WIN32)
  if(MINGW)
    target_link_libraries(${PROJECT_NAME} PRIVATE advapi32)
  else()
    target_link_libraries(${PROJECT_NAME} PRIVATE Advapi32)
  endif()
endif()

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
  if(DEFINED WINFSP_DIR)
      file(TO_CMAKE_PATH "${WINFSP_DIR}" WINFSP_DIR)
  elseif(DEFINED ENV{WINFSP_DIR})
      file(TO_CMAKE_PATH "$ENV{WINFSP_DIR}" WINFSP_DIR)
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
elseif(UNIX AND NOT APPLE AND NOT IS_ANDROID AND NOT IS_TERMUX)
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
if(NOT (CMAKE_CROSSCOMPILING AND MINGW))
  qt_generate_deploy_app_script(
      TARGET ${PROJECT_NAME}
      OUTPUT_SCRIPT deploy_script
      NO_UNSUPPORTED_PLATFORM_ERROR
      DEPLOY_TOOL_OPTIONS NO_TRANSLATIONS
  )
  install(SCRIPT ${deploy_script})
else()
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  install(CODE "
    execute_process(
      COMMAND \"${Python3_EXECUTABLE}\" \"${CMAKE_SOURCE_DIR}/scripts/mingw-deploy.py\"
        --target \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.exe\"
        --target \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/VirtualClub-Updater.exe\"
        --dist-dir \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}\"
      RESULT_VARIABLE _qt_deploy_rc
    )
    if(NOT _qt_deploy_rc EQUAL 0)
      message(FATAL_ERROR \"deploy_qt6_mingw.py failed with exit code \${_qt_deploy_rc}\")
    endif()
  ")
endif()

if (NOT WIN32)
  set_target_properties(${PROJECT_NAME} PROPERTIES
      BUILD_RPATH "\$ORIGIN/lib"
      INSTALL_RPATH "\$ORIGIN/lib"
  )
endif()

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
)
install(TARGETS VirtualClub-Updater
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
)

if(CMAKE_CROSSCOMPILING AND MINGW)
  set(CMAKE_INSTALL_BINDIR .)
endif()

if(WIN32)
  set(EXTLIB_INSTALL_DIR ${CMAKE_INSTALL_BINDIR})
else()
  set(EXTLIB_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
endif()

message(STATUS "External libraries will be installed to: ${EXTLIB_INSTALL_DIR}")

if(CMAKE_CROSSCOMPILING AND MINGW)
  # https://gitlab.kitware.com/cmake/cmake/-/work_items/20753#note_1022693
  install(CODE "
    execute_process(
      COMMAND \"${Python3_EXECUTABLE}\" ${CMAKE_SOURCE_DIR}/scripts/mingw-depends.py --objdump=/usr/bin/x86_64-w64-mingw32-objdump --pre-exclude-regex \"api-ms-\" --pre-exclude-regex \"ext-ms-\" --post-exclude-regex \"qt6.*\" --post-exclude-regex \".*system32/.*\\\\.dll\" --userlib \"${CMAKE_CURRENT_BINARY_DIR}\" --userlib \"${WINFSP_DIR}/bin\" $<TARGET_FILE:${PROJECT_NAME}>
      OUTPUT_VARIABLE RUNTIME_DEPS
      OUTPUT_STRIP_TRAILING_WHITESPACE
      COMMAND_ECHO STDERR
      COMMAND_ERROR_IS_FATAL ANY
    )
    message(AUTHOR_WARNING \"resolved dependencies: ${RUNTIME_DEPS}\")
    file(INSTALL DESTINATION \${CMAKE_INSTALL_PREFIX}/bin TYPE EXECUTABLE FILES \${RUNTIME_DEPS})
    # this is Qt5-specific, obviously
    file(INSTALL DESTINATION \${CMAKE_INSTALL_PREFIX}/bin/platforms TYPE EXECUTABLE FILES \"/usr/x86_64-w64-mingw32/lib/qt6/plugins/platforms/qwindows.dll\")
  " COMPONENT foobar)
else()
  install(CODE "
    file(GET_RUNTIME_DEPENDENCIES
      EXECUTABLES \"$<TARGET_FILE:${PROJECT_NAME}>\"
      RESOLVED_DEPENDENCIES_VAR _resolved_deps
      UNRESOLVED_DEPENDENCIES_VAR _unresolved_deps
      CONFLICTING_DEPENDENCIES_PREFIX _conflicting_deps
      PRE_EXCLUDE_REGEXES \"api-ms-\" \"ext-ms-\"
      POST_EXCLUDE_REGEXES \".*system32/.*\\\\.dll\"
      DIRECTORIES
        \"$<TARGET_FILE_DIR:Qt6::Core>\"
        \"$<TARGET_FILE_DIR:Qt6::Multimedia>\"
        \"$<TARGET_FILE_DIR:Qt6::Svg>\"
        \"$<TARGET_FILE_DIR:LibArchive::LibArchive>\"
    )

    list(LENGTH _unresolved_deps _unresolved_count)
    if(_unresolved_count GREATER 0)
      message(STATUS \"Unresolved runtime dependencies (\${_unresolved_count}):\")
      foreach(_dep IN LISTS _unresolved_deps)
        message(STATUS \"  - \${_dep}\")
      endforeach()
    else()
      message(STATUS \"Zero unresolved runtime dependencies.\")
    endif()

    foreach(_dep IN LISTS _resolved_deps)
      message(STATUS \"Installing \${_dep}\")
      file(INSTALL
        DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${EXTLIB_INSTALL_DIR}\"
        FOLLOW_SYMLINK_CHAIN
        FILES \"\${_dep}\"
      )
    endforeach()
  ")
endif()

install(DIRECTORY assets DESTINATION .)

if(WIN32)
    # Target the DLL in the bin directory corresponding to architecture
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WINFSP_DLL "${WINFSP_DIR}/bin/winfsp-x64.dll")
    else()
        set(WINFSP_DLL "${WINFSP_DIR}/bin/winfsp-x86.dll")
    endif()

    if(EXISTS "${WINFSP_DLL}")
        install(FILES "${WINFSP_DLL}" DESTINATION ${EXTLIB_INSTALL_DIR})
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
        install(FILES "${FUSE3_REAL_PATH}" DESTINATION ${EXTLIB_INSTALL_DIR})

        # 3. Explicitly create symlinks for required names (libfuse.so.3 and libfuse3.so.3)
        install(CODE "
            if(NOT \"${FUSE3_REAL_NAME}\" STREQUAL \"libfuse3.so.3\")
                file(CREATE_LINK \"${FUSE3_REAL_NAME}\"
                    \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${EXTLIB_INSTALL_DIR}/libfuse3.so.3\"
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
            install(FILES ${QT_ICU_LIBS} DESTINATION ${EXTLIB_INSTALL_DIR})
        endif()
    endif()
endif()


# Define target directory for minimal stdlib
if(WIN32)
  if(CMAKE_CROSSCOMPILING AND MINGW)
    set(PY_STDLIB_DEST "lib")
  else()
    set(PY_STDLIB_DEST "Lib")
  endif()
else()
    set(PY_STDLIB_DEST "lib/python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}")
endif()

file(TO_CMAKE_PATH "${Python_STDLIB}" Python_STDLIB_CMAKE)
file(TO_CMAKE_PATH "${Python_MINGW_STDLIB}" Python_MINGW_STDLIB_CMAKE)

install(
  DIRECTORY "${Python_STDLIB_CMAKE}/"
    DESTINATION "${PY_STDLIB_DEST}"
    USE_SOURCE_PERMISSIONS
    PATTERN "__pycache__" EXCLUDE
    PATTERN "lib-dynload" EXCLUDE
    PATTERN "profiling" EXCLUDE
    PATTERN "site-packages" EXCLUDE
    PATTERN "test" EXCLUDE
    PATTERN "unittest" EXCLUDE
    PATTERN "idlelib" EXCLUDE
    PATTERN "turtledemo" EXCLUDE
    PATTERN "ensurepip" EXCLUDE
    PATTERN "venv" EXCLUDE
    PATTERN "tkinter" EXCLUDE
    PATTERN "sqlite3" EXCLUDE
    PATTERN "email" EXCLUDE
    PATTERN "html" EXCLUDE
    PATTERN "http" EXCLUDE
    PATTERN "xml" EXCLUDE
    PATTERN "urllib" EXCLUDE
    PATTERN "wsgiref" EXCLUDE
    PATTERN "xmlrpc" EXCLUDE
    PATTERN "multiprocessing" EXCLUDE
    PATTERN "concurrent" EXCLUDE
    PATTERN "asyncio" EXCLUDE
    PATTERN "distutils" EXCLUDE
    PATTERN "pydoc_data" EXCLUDE
    PATTERN "lib2to3" EXCLUDE
    PATTERN "ctypes" EXCLUDE
    PATTERN "logging" EXCLUDE
    PATTERN "json" EXCLUDE
    PATTERN "imaplib.py" EXCLUDE
    PATTERN "poplib.py" EXCLUDE
    PATTERN "smtplib.py" EXCLUDE
    PATTERN "telnetlib.py" EXCLUDE
    PATTERN "webbrowser.py" EXCLUDE
    PATTERN "pydoc.py" EXCLUDE
    PATTERN "doctest.py" EXCLUDE
    PATTERN "turtle.py" EXCLUDE
)

if(MINGW)
  install(
    DIRECTORY "${Python_MINGW_STDLIB_CMAKE}/"
      DESTINATION "${PY_STDLIB_DEST}"
      USE_SOURCE_PERMISSIONS
  )
endif()
# Install C-runtime DLL/SO only, skipping lib-dynload
if(Python_RUNTIME_LIBRARY)
  file(TO_CMAKE_PATH "${Python_RUNTIME_LIBRARY}" Python_RUNTIME_LIBRARY_CMAKE)
    install(FILES "${Python_RUNTIME_LIBRARY_CMAKE}" DESTINATION ${EXTLIB_INSTALL_DIR})
endif()


# After all installs are done, nuke the potential `include` folder
install(CODE "
    file(REMOVE_RECURSE \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/include\")
")

# Test targets and test-specific dependencies are kept outside the main build file.
include(cmake/Tests.cmake)
