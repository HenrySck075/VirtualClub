include(CTest)

if(BUILD_TESTING)
  find_package(Qt6 REQUIRED COMPONENTS Test)

  set(MVC_RENPY_TEST_SDK_DIR "${CMAKE_BINARY_DIR}/_deps/renpy-6.99.2-sdk")
  set(MVC_RENPY_TEST_BASE "${CMAKE_SOURCE_DIR}/tests/subject/base")
  set(MVC_RENPY_STAMP_FILE "${CMAKE_BINARY_DIR}/_deps/renpy-6.99.2-assets.stamp")

  set(MVC_SDK_DEPS "")
  if(NOT EXISTS "${MVC_RENPY_TEST_SDK_DIR}/renpy.py")
    include(ExternalProject)
    ExternalProject_Add(mvc_renpy_6992_test_sdk
      URL "https://www.renpy.org/dl/6.99.2/renpy-6.99.2-sdk.zip"
      SOURCE_DIR "${MVC_RENPY_TEST_SDK_DIR}"
      UPDATE_DISCONNECTED TRUE
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      INSTALL_COMMAND ""
    )
    list(APPEND MVC_SDK_DEPS mvc_renpy_6992_test_sdk)
  endif()

  add_custom_command(
    OUTPUT "${MVC_RENPY_STAMP_FILE}"
    DEPENDS ${MVC_SDK_DEPS}
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${MVC_RENPY_TEST_BASE}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${MVC_RENPY_TEST_SDK_DIR}/lib"
            "${MVC_RENPY_TEST_BASE}/lib"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${MVC_RENPY_TEST_SDK_DIR}/renpy"
            "${MVC_RENPY_TEST_BASE}/renpy"
    COMMAND "${CMAKE_COMMAND}" -E copy
            "${MVC_RENPY_TEST_SDK_DIR}/renpy.py"
            "${MVC_RENPY_TEST_BASE}/renpy.py"
    COMMAND "${CMAKE_COMMAND}" -E touch "${MVC_RENPY_STAMP_FILE}"
    COMMENT "Preparing Ren'Py test assets (once)"
    VERBATIM
  )

  add_custom_target(mvc_prepare_test_assets
    DEPENDS "${MVC_RENPY_STAMP_FILE}"
  )

  qt_add_executable(VirtualClub-Tests
    tests/main.cpp
    tests/ModIndexTests.cpp
    tests/VFSPathResolverTests.cpp
    src/utils/ModIndex.cpp
    src/utils/YamlSettings.cpp
    src/utils/ProfileSettings.cpp
    src/utils/ProfileSingleApp.cpp
    src/utils/RenpyArchive.cpp
    src/vfs/VFSPathResolver.cpp
    src/vfs/vfs_core.cpp
  )

if(UNIX AND NOT APPLE AND NOT ANDROID AND
   NOT CMAKE_SYSTEM_NAME STREQUAL "Android" AND
   NOT DEFINED ENV{TERMUX_VERSION} AND
   NOT "$ENV{PREFIX}" MATCHES "com.termux")
 target_sources(VirtualClub-Tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/vfs/vfs_fuse3.cpp")
elseif(WIN32)
  target_sources(VirtualClub-Tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/vfs/vfs_winfsp.cpp")
endif()

  target_include_directories(VirtualClub-Tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${FUSE_INCLUDE_DIRS}
  )

if(CMAKE_CROSSCOMPILING AND MINGW)
  target_link_libraries(${PROJECT_NAME}-Tests PRIVATE ${Python_LIBRARIES})
  target_include_directories(${PROJECT_NAME}-Tests PRIVATE ${Python_INCLUDE_DIRS})
endif()
  target_link_libraries(VirtualClub-Tests PRIVATE
    ${FUSE_LIBRARIES}
    Qt6::Test Qt6::Widgets Qt6::Network Qt6::Svg Qt6::Multimedia
    pybind11::embed yaml-cpp::yaml-cpp LibArchive::LibArchive
    nlohmann_json::nlohmann_json
  )

  target_compile_definitions(VirtualClub-Tests PRIVATE MVC_TEST_BUILD)
  if(TARGET cpptrace::cpptrace)
    target_link_libraries(VirtualClub-Tests PRIVATE cpptrace::cpptrace)
  endif()

  add_dependencies(VirtualClub-Tests mvc_prepare_test_assets)
  set_target_properties(VirtualClub-Tests PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
        AUTORCC ON
    )
  add_test(NAME VirtualClub.UnitTests COMMAND VirtualClub-Tests)
endif()
