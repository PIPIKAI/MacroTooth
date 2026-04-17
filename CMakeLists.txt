cmake_minimum_required(VERSION 3.10)
project(macrotooth VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找必需的库
find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUS REQUIRED dbus-1)
pkg_check_modules(GLIB REQUIRED glib-2.0)

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${DBUS_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)


# 源文件
set(SOURCES
    src/main.cpp
    src/uhid_device.cpp
    src/hid_reporter.cpp
    src/bluez_profile.cpp
)

# 创建可执行文件
add_executable(macrotooth ${SOURCES})


target_link_libraries(macrotooth
    ${DBUS_LIBRARIES}
    ${GLIB_LIBRARIES}
    bluetooth
    pthread
)