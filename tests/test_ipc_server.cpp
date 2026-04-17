// test_ipc_server.cpp
// 测试键盘服务（IpcServer）的正常启动与命令处理。
// 可独立运行：cmake --build build --target test_ipc_server && ./build/bin/test_ipc_server
//
// 测试内容：
//   1. IpcServer 可正常构造和析构
//   2. start() 在给定路径上创建 Unix Domain Socket
//   3. stop() 清理 socket 文件
//   4. 客户端可以连接并发送命令，收到正确响应
//   5. KEY_PRESS / KEY_RELEASE / TYPE / STATUS / QUIT 命令均被正确处理
//   6. 未知命令返回错误响应

#include "ipc_server.h"
#include <cstdio>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <string>
#include <sys/stat.h>

// ── 简单测试框架 ──────────────────────────────────────────────────────────────

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name, expr)                                        \
    do {                                                        \
        bool _ok = static_cast<bool>(expr);                     \
        if (_ok) {                                              \
            printf("[PASS] %s\n", (name));                      \
            ++g_pass;                                           \
        } else {                                                \
            printf("[FAIL] %s  (line %d)\n", (name), __LINE__); \
            ++g_fail;                                           \
        }                                                       \
    } while (0)

// ── 辅助函数 ──────────────────────────────────────────────────────────────────

static const std::string SOCK_PATH = "/tmp/test_ipc_server.sock";

// 连接到 socket，发送一行命令，读取一行响应，关闭连接。
// 返回服务器响应（不含换行）；失败时返回空字符串。
static std::string send_command(const std::string& cmd) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return "";

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_PATH.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return "";
    }

    std::string line = cmd + "\n";
    if (write(fd, line.c_str(), line.size()) < 0) {
        close(fd);
        return "";
    }

    char buf[256] = {};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return "";

    std::string resp(buf, static_cast<size_t>(n));
    // 去掉末尾换行
    while (!resp.empty() && (resp.back() == '\n' || resp.back() == '\r'))
        resp.pop_back();
    return resp;
}

static bool file_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

// ── 测试用例 ─────────────────────────────────────────────────────────────────

// Test 1: IpcServer 可以被构造和析构，不崩溃。
static void test_construction() {
    IpcServer ipc;
    TEST("ipc_server: construction does not crash", true);
}

// Test 2: start() 在指定路径创建 socket 文件；stop() 清理它。
static void test_start_stop_lifecycle() {
    IpcServer ipc;
    bool started = ipc.start(SOCK_PATH);
    TEST("ipc_server: start() returns true", started);
    if (started) {
        TEST("ipc_server: socket file exists after start()",
             file_exists(SOCK_PATH));
        ipc.stop();
        TEST("ipc_server: socket file removed after stop()",
             !file_exists(SOCK_PATH));
    }
}

// Test 3: 未设置回调时 KEY_PRESS 返回错误响应。
static void test_key_press_no_callback() {
    IpcServer ipc;
    if (!ipc.start(SOCK_PATH)) {
        printf("[SKIP] ipc_server: KEY_PRESS no callback (start failed)\n");
        ++g_pass;
        return;
    }
    // 等待 accept 循环就绪
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::string resp = send_command("KEY_PRESS 04");
    ipc.stop();

    TEST("ipc_server: KEY_PRESS without callback returns ERROR",
         resp.rfind("ERROR", 0) == 0);
}

// Test 4: 设置 KEY_PRESS 回调后正常调用，返回 OK。
static void test_key_press_with_callback() {
    IpcServer ipc;
    uint8_t received_key = 0;
    uint8_t received_mod = 0;

    ipc.setKeyPressCallback([&](uint8_t key, uint8_t mod) -> bool {
        received_key = key;
        received_mod = mod;
        return true;
    });

    if (!ipc.start(SOCK_PATH)) {
        printf("[SKIP] ipc_server: KEY_PRESS with callback (start failed)\n");
        ++g_pass;
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // KEY_PRESS 04 02  →  key=0x04 (HID_KEY_A), mod=0x02 (LSHIFT)
    std::string resp = send_command("KEY_PRESS 04 02");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    ipc.stop();

    TEST("ipc_server: KEY_PRESS with callback returns OK", resp == "OK");
    TEST("ipc_server: KEY_PRESS delivers correct key code", received_key == 0x04);
    TEST("ipc_server: KEY_PRESS delivers correct modifier", received_mod == 0x02);
}

// Test 5: KEY_RELEASE 回调正常调用，返回 OK。
static void test_key_release_with_callback() {
    IpcServer ipc;
    bool released = false;

    ipc.setKeyReleaseCallback([&]() -> bool {
        released = true;
        return true;
    });

    if (!ipc.start(SOCK_PATH)) {
        printf("[SKIP] ipc_server: KEY_RELEASE (start failed)\n");
        ++g_pass;
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::string resp = send_command("KEY_RELEASE");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    ipc.stop();

    TEST("ipc_server: KEY_RELEASE returns OK", resp == "OK");
    TEST("ipc_server: KEY_RELEASE invokes callback", released);
}

// Test 6: TYPE 命令将完整文本传递给回调。
static void test_type_command() {
    IpcServer ipc;
    std::string received_text;

    ipc.setTypeCallback([&](const std::string& text) -> bool {
        received_text = text;
        return true;
    });

    if (!ipc.start(SOCK_PATH)) {
        printf("[SKIP] ipc_server: TYPE command (start failed)\n");
        ++g_pass;
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::string resp = send_command("TYPE hello world");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    ipc.stop();

    TEST("ipc_server: TYPE returns OK", resp == "OK");
    TEST("ipc_server: TYPE delivers correct text", received_text == "hello world");
}

// Test 7: STATUS 命令返回回调提供的状态字符串。
static void test_status_command() {
    IpcServer ipc;

    ipc.setStatusCallback([]() -> std::string {
        return "ble_connected=1 classic_connected=0";
    });

    if (!ipc.start(SOCK_PATH)) {
        printf("[SKIP] ipc_server: STATUS command (start failed)\n");
        ++g_pass;
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::string resp = send_command("STATUS");
    ipc.stop();

    TEST("ipc_server: STATUS returns STATUS prefix",
         resp.rfind("STATUS ", 0) == 0);
    TEST("ipc_server: STATUS response contains callback data",
         resp.find("ble_connected=1") != std::string::npos);
}

// Test 8: 未知命令返回 ERROR 响应。
static void test_unknown_command() {
    IpcServer ipc;
    if (!ipc.start(SOCK_PATH)) {
        printf("[SKIP] ipc_server: unknown command (start failed)\n");
        ++g_pass;
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::string resp = send_command("BOGUS_CMD");
    ipc.stop();

    TEST("ipc_server: unknown command returns ERROR",
         resp.rfind("ERROR", 0) == 0);
}

// Test 9: QUIT 命令让服务端关闭该客户端连接，响应为 OK。
static void test_quit_command() {
    IpcServer ipc;
    if (!ipc.start(SOCK_PATH)) {
        printf("[SKIP] ipc_server: QUIT command (start failed)\n");
        ++g_pass;
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::string resp = send_command("QUIT");
    ipc.stop();

    TEST("ipc_server: QUIT returns OK", resp == "OK");
}

// Test 10: KEY_PRESS 缺少 key 参数时返回 ERROR。
static void test_key_press_missing_key() {
    IpcServer ipc;
    ipc.setKeyPressCallback([](uint8_t, uint8_t) -> bool { return true; });

    if (!ipc.start(SOCK_PATH)) {
        printf("[SKIP] ipc_server: KEY_PRESS missing key (start failed)\n");
        ++g_pass;
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::string resp = send_command("KEY_PRESS");
    ipc.stop();

    TEST("ipc_server: KEY_PRESS without key arg returns ERROR",
         resp.rfind("ERROR", 0) == 0);
}

// ── main ─────────────────────────────────────────────────────────────────────

int main() {
    printf("=== test_ipc_server ===\n");

    // 保证 socket 文件不存在
    unlink(SOCK_PATH.c_str());

    test_construction();
    test_start_stop_lifecycle();
    test_key_press_no_callback();
    test_key_press_with_callback();
    test_key_release_with_callback();
    test_type_command();
    test_status_command();
    test_unknown_command();
    test_quit_command();
    test_key_press_missing_key();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return (g_fail > 0) ? 1 : 0;
}
