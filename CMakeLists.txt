cmake_minimum_required(VERSION 3.16)
project(camera VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(CTest)
option(CAMERA_BUILD_DEMO "Build the camera acquisition executable" ON)
option(CAMERA_USE_MOCK_SDK "Use a test SDK; never accesses a physical camera" OFF)
set(GALAXY_SDK_ROOT "" CACHE PATH "Galaxy SDK installation directory")

find_package(Threads REQUIRED)
add_library(rm_log STATIC util/rm_log/src/rm_log.cpp)
target_include_directories(rm_log PUBLIC util/rm_log/include)
target_include_directories(rm_log SYSTEM PUBLIC util)
target_link_libraries(rm_log PUBLIC Threads::Threads)
if(MSVC)
    target_compile_options(rm_log PUBLIC /utf-8)
endif()

if(CAMERA_BUILD_DEMO)
    find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui)
    add_executable(camera_demo src/main.cpp)
    target_include_directories(camera_demo PRIVATE include ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(camera_demo PRIVATE rm_log ${OpenCV_LIBS})
    if(MSVC)
        target_compile_options(camera_demo PRIVATE /W4)
    else()
        target_compile_options(camera_demo PRIVATE -Wall -Wextra)
    endif()

    if(CAMERA_USE_MOCK_SDK)
        message(WARNING "MOCK SDK enabled: camera_demo uses simulated frames only")
        add_library(galaxy_mock STATIC tests/mock_sdk.cpp)
        target_include_directories(galaxy_mock PUBLIC tests/mock_include)
        target_link_libraries(camera_demo PRIVATE galaxy_mock)
        target_compile_definitions(camera_demo PRIVATE CAMERA_MOCK_SDK=1)
    else()
        # Prefer headers from the installed SDK to avoid a header/library mismatch.
        find_path(GXIAPI_INCLUDE_DIR NAMES GxIAPI.h
            HINTS "${GALAXY_SDK_ROOT}" ENV GALAXY_SDK_ROOT
            PATH_SUFFIXES include inc Development/Include
            PATHS /usr/include /usr/local/include)
        if(NOT GXIAPI_INCLUDE_DIR)
            message(FATAL_ERROR "Galaxy SDK headers missing. Install the official SDK and set GALAXY_SDK_ROOT or GXIAPI_INCLUDE_DIR.")
        endif()
        find_library(GXIAPI_LIBRARY NAMES gxiapi GxIAPI
            HINTS "${GALAXY_SDK_ROOT}" ENV GALAXY_SDK_ROOT
            PATH_SUFFIXES lib lib64 lib/x64 lib/win64 Development/Lib/x64)
        find_library(DXIMAGEPROC_LIBRARY NAMES dximageproc DxImageProc
            HINTS "${GALAXY_SDK_ROOT}" ENV GALAXY_SDK_ROOT
            PATH_SUFFIXES lib lib64 lib/x64 lib/win64 Development/Lib/x64)
        if(NOT GXIAPI_LIBRARY)
            message(FATAL_ERROR "Galaxy SDK library missing. Install the SDK and set GALAXY_SDK_ROOT or GXIAPI_LIBRARY.")
        endif()
        target_include_directories(camera_demo SYSTEM PRIVATE "${GXIAPI_INCLUDE_DIR}")
        target_link_libraries(camera_demo PRIVATE "${GXIAPI_LIBRARY}")
        if(DXIMAGEPROC_LIBRARY)
            target_link_libraries(camera_demo PRIVATE "${DXIMAGEPROC_LIBRARY}")
        endif()
        # Some Linux SDKs export Dx functions from libgxiapi; others need a separate library.
        include(CheckCXXSourceCompiles)
        set(CMAKE_REQUIRED_INCLUDES "${GXIAPI_INCLUDE_DIR}")
        set(CMAKE_REQUIRED_LIBRARIES "${GXIAPI_LIBRARY}")
        if(DXIMAGEPROC_LIBRARY)
            list(APPEND CMAKE_REQUIRED_LIBRARIES "${DXIMAGEPROC_LIBRARY}")
        endif()
        unset(GALAXY_HAS_REQUIRED_API CACHE)
        check_cxx_source_compiles("#include <cstddef>
            #include <GxIAPI.h>
            #include <DxImageProc.h>
            int main() {
                GX_DEV_HANDLE device = nullptr;
                GX_DEVICE_INFO info{};
                GX_FLOAT_VALUE value{};
                GXInitLib(); GXGetDeviceInfo(1, &info); GXOpenDeviceByIndex(1, &device);
                GXGetFloatValue(device, \"ExposureTime\", &value);
                GXSetEnumValueByString(device, \"TriggerMode\", \"Off\");
                DxRaw8toRGB24Ex(nullptr, nullptr, 0, 0, RAW2RGB_NEIGHBOUR, BAYERRG, false, DX_ORDER_BGR);
                return 0;
            }" GALAXY_HAS_REQUIRED_API)
        if(NOT GALAXY_HAS_REQUIRED_API)
            message(FATAL_ERROR "Galaxy SDK API/link check failed. Match SDK headers/libraries and set DXIMAGEPROC_LIBRARY if required. See CMake configure logs.")
        endif()
    endif()
endif()

if(BUILD_TESTING)
    add_executable(core_tests tests/core_tests.cpp)
    target_include_directories(core_tests PRIVATE include)
    target_link_libraries(core_tests PRIVATE rm_log)
    add_test(NAME logging_and_options COMMAND core_tests)
    if(CAMERA_BUILD_DEMO AND CAMERA_USE_MOCK_SDK)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        add_test(NAME mock_acquisition COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_acquisition.py" "$<TARGET_FILE:camera_demo>")
        set_tests_properties(mock_acquisition PROPERTIES TIMEOUT 90)
    endif()
endif()
