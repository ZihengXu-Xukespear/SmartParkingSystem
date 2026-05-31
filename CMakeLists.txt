cmake_minimum_required(VERSION 3.16)
project(smart_parking LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: enable UTF-8 source and execution charset
if(MSVC)
    add_compile_options(/utf-8)
    # Use static runtime library to avoid DLL dependencies
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime library" FORCE)
endif()

# Find OpenCV (optional for plate recognition)
find_package(OpenCV COMPONENTS core imgproc imgcodecs highgui)
if(OpenCV_FOUND)
    message(STATUS "OpenCV found: ${OpenCV_VERSION}")
    include_directories(${OpenCV_INCLUDE_DIRS})
    add_definitions(-DENABLE_OPENCV)
else()
    message(STATUS "OpenCV not found. Plate recognition will be disabled.")
endif()

# MySQL paths - adjust to your environment
if(NOT DEFINED MYSQL_INCLUDE_DIR)
    if(WIN32)
        set(MYSQL_INCLUDE_DIR "C:/Program Files/MySQL/MySQL Server 8.0/include")
    else()
        set(MYSQL_INCLUDE_DIR "/usr/include/mysql")
    endif()
endif()

if(NOT DEFINED MYSQL_LIB_DIR)
    if(WIN32)
        set(MYSQL_LIB_DIR "C:/Program Files/MySQL/MySQL Server 8.0/lib")
    else()
        set(MYSQL_LIB_DIR "/usr/lib/x86_64-linux-gnu")
    endif()
endif()

# Third-party includes (Crow, picojson)
include_directories(${CMAKE_SOURCE_DIR}/third_party)

# Asio (install via conan or system package manager)
if(NOT DEFINED ASIO_INCLUDE_DIR)
    if(WIN32)
        # If using conan, set ASIO_INCLUDE_DIR accordingly
        set(ASIO_INCLUDE_DIR "$ENV{USERPROFILE}/.conan2/p/asioad7b630cd44e4/p/include")
    else()
        set(ASIO_INCLUDE_DIR "/usr/include")
    endif()
endif()

# Main executable (all code is header-only, just compile main.cpp)
file(GLOB_RECURSE SOURCES src/*.cpp)
add_executable(smart_parking ${SOURCES})
target_compile_definitions(smart_parking PRIVATE _WIN32_WINNT=0x0601 WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(smart_parking PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${MYSQL_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party
    ${ASIO_INCLUDE_DIR}
    ${OpenCV_INCLUDE_DIRS}
)

# Link MySQL + system libs
if(WIN32)
    target_link_libraries(smart_parking PRIVATE
        ${MYSQL_LIB_DIR}/libmysql.lib
        ws2_32 secur32 bcrypt crypt32 mswsock
    )
else()
    target_link_libraries(smart_parking PRIVATE
        mysqlclient pthread
    )
endif()

# Link OpenCV if available
if(OpenCV_FOUND)
    target_link_libraries(smart_parking PRIVATE ${OpenCV_LIBS})
    target_include_directories(smart_parking PRIVATE ${OpenCV_INCLUDE_DIRS})
endif()

# Copy frontend, config, and OCR bridge to build directory
add_custom_command(TARGET smart_parking POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/frontend
        $<TARGET_FILE_DIR:smart_parking>/frontend
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/config
        $<TARGET_FILE_DIR:smart_parking>/config
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/src/hyperlpr_bridge.py
        $<TARGET_FILE_DIR:smart_parking>/hyperlpr_bridge.py
)

# Windows: copy MySQL DLL and OpenCV DLL
if(WIN32)
    add_custom_command(TARGET smart_parking POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MYSQL_LIB_DIR}/libmysql.dll"
            $<TARGET_FILE_DIR:smart_parking>
    )
    if(OpenCV_FOUND)
        file(GLOB OPENCV_DLL "${OpenCV_DIR}/x64/vc16/bin/opencv_world*.dll")
        foreach(DLL ${OPENCV_DLL})
            add_custom_command(TARGET smart_parking POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DLL}" $<TARGET_FILE_DIR:smart_parking>
            )
        endforeach()
    endif()
endif()
