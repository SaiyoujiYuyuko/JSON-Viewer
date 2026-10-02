cmake_minimum_required(VERSION 3.20)
project(JsonViewerNdd VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

option(BUILD_NDD_PLUGIN "Build the Notepad-- Qt 5 plugin" ON)
include(CTest)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/rapidjson/include/rapidjson/reader.h")
    message(FATAL_ERROR "Run: git submodule update --init external/rapidjson")
endif()
if(MSVC)
    add_compile_options(/utf-8 /W4 /permissive- /Zc:preprocessor)
endif()

add_library(json_viewer_core STATIC
    src/NppJsonViewer/JsonHandler.cpp
    src/JsonCore/OrderedJson.cpp)
set_target_properties(json_viewer_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(json_viewer_core PUBLIC src/NppJsonViewer src/JsonCore)
target_include_directories(json_viewer_core SYSTEM PUBLIC external/rapidjson/include)

if(BUILD_NDD_PLUGIN)
    if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "The validated Notepad-- target requires Windows x64 with MSVC and Qt 5.15.2.")
    endif()
    find_package(Qt5 5.15.2 EXACT REQUIRED COMPONENTS Core Gui Widgets)
    set(NDD_SDK_ROOT "" CACHE PATH "QScintilla SDK with include/Qsci and lib/qmyedit_qt5.lib")
    find_path(NDD_QSCINTILLA_INCLUDE_DIR Qsci/qsciscintilla.h HINTS "${NDD_SDK_ROOT}/include" REQUIRED)
    find_library(NDD_QSCINTILLA_LIBRARY qmyedit_qt5 HINTS "${NDD_SDK_ROOT}/lib" REQUIRED)
    add_library(ndd-json-viewer-upstream SHARED
        src/NddJsonViewer/Plugin.cpp
        src/NddJsonViewer/EditorBridge.cpp
        src/NddJsonViewer/JsonViewer.cpp
        src/NddJsonViewer/Resources.qrc)
    set_target_properties(ndd-json-viewer-upstream PROPERTIES AUTORCC ON PREFIX "")
    target_include_directories(ndd-json-viewer-upstream SYSTEM PRIVATE "${NDD_QSCINTILLA_INCLUDE_DIR}")
    target_link_libraries(ndd-json-viewer-upstream PRIVATE json_viewer_core Qt5::Widgets "${NDD_QSCINTILLA_LIBRARY}")
    install(TARGETS ndd-json-viewer-upstream RUNTIME DESTINATION plugin)
endif()

if(BUILD_TESTING)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/googletest/CMakeLists.txt")
        message(FATAL_ERROR "Run: git submodule update --init external/googletest")
    endif()
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory(external/googletest EXCLUDE_FROM_ALL)
    add_executable(json_viewer_tests
        tests/OrderedJsonTest.cpp
        tests/UnitTest/JsonHandlerTest.cpp
        tests/UnitTest/JsonFormatTest.cpp
        tests/UnitTest/JsonCompressTest.cpp
        tests/UnitTest/JsonSortByKeyTest.cpp)
    target_link_libraries(json_viewer_tests PRIVATE json_viewer_core GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(json_viewer_tests)
endif()
