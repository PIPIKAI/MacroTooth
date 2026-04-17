cmake_minimum_required(VERSION 3.10)

# ── 查找依赖（与根 CMakeLists.txt 保持一致） ──────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(DBUS REQUIRED dbus-1)
pkg_check_modules(GLIB REQUIRED glib-2.0)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${DBUS_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)

# ── 公共库目标（各测试共享源文件，避免重复编译） ─────────────────────────────
add_library(macrotooth_core STATIC
    ../src/bt_adapter.cpp
    ../src/bt_agent.cpp
    ../src/bluez_profile.cpp
    ../src/gatt_application.cpp
    ../src/hid_reporter.cpp
    ../src/ipc_server.cpp
)
target_link_libraries(macrotooth_core
    ${DBUS_LIBRARIES}
    ${GLIB_LIBRARIES}
    pthread
)

# ── Test 1: 蓝牙启动和连接功能 ────────────────────────────────────────────────
add_executable(test_bt_adapter test_bt_adapter.cpp)
target_link_libraries(test_bt_adapter macrotooth_core)

# ── Test 2: 键盘服务（IpcServer）正常启动 ─────────────────────────────────────
add_executable(test_ipc_server test_ipc_server.cpp)
target_link_libraries(test_ipc_server macrotooth_core)

# ── Test 3: 蓝牙连接后键盘按键正常发送 ────────────────────────────────────────
add_executable(test_hid_reporter test_hid_reporter.cpp)
target_link_libraries(test_hid_reporter macrotooth_core)

# ── CTest 集成（可选：cmake --build . --target test） ─────────────────────────
enable_testing()
add_test(NAME bt_adapter   COMMAND test_bt_adapter)
add_test(NAME ipc_server   COMMAND test_ipc_server)
add_test(NAME hid_reporter COMMAND test_hid_reporter)
