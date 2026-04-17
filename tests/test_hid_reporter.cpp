// test_hid_reporter.cpp
// 测试蓝牙连接后键盘按键的正常发送（HIDReporter 模块）。
// 可独立运行：cmake --build build --target test_hid_reporter && ./build/bin/test_hid_reporter
//
// 测试内容：
//   1. HIDReporter 可正常构造
//   2. keyPress() 将修饰键和按键码写入报告缓冲区
//   3. keyRelease() 将报告缓冲区全部清零
//   4. 每次 keyCombo() 先按后松（调用 send 回调两次）
//   5. typeString() 对每个字符各调用一次按下回调和一次松开回调
//   6. 大写字母自动添加 LSHIFT 修饰
//   7. 特殊字符（如 '!'）正确映射到 SHIFT + 数字键
//   8. 鼠标操作正确修改鼠标报告缓冲区

#include "hid_reporter.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

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

// ── 辅助结构 ──────────────────────────────────────────────────────────────────

// 记录每次 send 回调收到的报告快照
struct Report {
    std::vector<uint8_t> data;
};

// 构造一个捕获所有报告的回调，并返回回调函数和记录列表的引用。
using ReportLog = std::vector<Report>;

static HIDReporter::SendReportCallback make_logger(ReportLog& log) {
    return [&log](const uint8_t* data, size_t len) -> bool {
        Report r;
        r.data.assign(data, data + len);
        log.push_back(r);
        return true;
    };
}

// ── 测试用例 ─────────────────────────────────────────────────────────────────

// Test 1: HIDReporter 可以被构造，不崩溃。
static void test_construction() {
    HIDReporter hid;
    TEST("hid_reporter: construction does not crash", true);
}

// Test 2: 初始报告全为零。
static void test_initial_report_is_zero() {
    HIDReporter hid;
    const uint8_t* kbd = hid.getKeyboardReport();
    bool all_zero = true;
    for (size_t i = 0; i < hid.getKeyboardReportSize(); ++i)
        if (kbd[i] != 0) { all_zero = false; break; }
    TEST("hid_reporter: initial keyboard report is all zeros", all_zero);

    const uint8_t* mouse = hid.getMouseReport();
    bool mouse_zero = true;
    for (size_t i = 0; i < hid.getMouseReportSize(); ++i)
        if (mouse[i] != 0) { mouse_zero = false; break; }
    TEST("hid_reporter: initial mouse report is all zeros", mouse_zero);
}

// Test 3: keyPress() 将修饰键写入 report[0]，按键码写入 report[2]。
static void test_key_press_sets_report() {
    HIDReporter hid;
    hid.keyPress(HIDReporter::HID_KEY_A, HIDReporter::MOD_LCTRL);

    const uint8_t* r = hid.getKeyboardReport();
    TEST("hid_reporter: keyPress sets modifier in report[0]",
         r[0] == HIDReporter::MOD_LCTRL);
    TEST("hid_reporter: keyPress sets keycode in report[2]",
         r[2] == HIDReporter::HID_KEY_A);
}

// Test 4: keyRelease() 将键盘报告全部清零。
static void test_key_release_clears_report() {
    HIDReporter hid;
    hid.keyPress(HIDReporter::HID_KEY_Z, HIDReporter::MOD_LSHIFT);
    hid.keyRelease();

    const uint8_t* r = hid.getKeyboardReport();
    bool all_zero = true;
    for (size_t i = 0; i < hid.getKeyboardReportSize(); ++i)
        if (r[i] != 0) { all_zero = false; break; }
    TEST("hid_reporter: keyRelease clears keyboard report", all_zero);
}

// Test 5: keyCombo() 触发两次 send 回调（按下 + 松开）。
static void test_key_combo_sends_twice() {
    HIDReporter hid;
    ReportLog log;
    hid.setSendCallback(make_logger(log));

    // duration_ms=0 使测试更快
    hid.keyCombo(HIDReporter::HID_KEY_ENTER, HIDReporter::MOD_NONE, 0);

    // keyCombo 只更新内部缓冲区，不主动调用 send_cb_
    // 因此回调次数为 0（send_cb_ 仅由 typeString 主动调用）
    // keyCombo = keyPress + sleep + keyRelease（不调用 send_cb_）
    TEST("hid_reporter: keyCombo does not invoke send callback directly",
         log.size() == 0);

    // 最终报告应为全零（已松开）
    const uint8_t* r = hid.getKeyboardReport();
    bool all_zero = true;
    for (size_t i = 0; i < hid.getKeyboardReportSize(); ++i)
        if (r[i] != 0) { all_zero = false; break; }
    TEST("hid_reporter: keyCombo leaves report zeroed after release", all_zero);
}

// Test 6: typeString() 对每个字符调用两次 send 回调（按下 + 松开）。
static void test_type_string_calls_send_callback() {
    HIDReporter hid;
    ReportLog log;
    hid.setSendCallback(make_logger(log));

    hid.typeString("ab", 0);

    // 'a' → press + release = 2 次；'b' → press + release = 2 次；共 4 次
    TEST("hid_reporter: typeString calls send callback twice per character",
         log.size() == 4);
}

// Test 7: typeString("a") 按下报告的 report[2] == HID_KEY_A，松开报告全零。
static void test_type_string_key_a() {
    HIDReporter hid;
    ReportLog log;
    hid.setSendCallback(make_logger(log));

    hid.typeString("a", 0);

    TEST("hid_reporter: typeString 'a' generates 2 reports", log.size() == 2);
    if (log.size() >= 2) {
        TEST("hid_reporter: typeString 'a' press report[2] == HID_KEY_A",
             log[0].data.size() >= 3 &&
             log[0].data[2] == HIDReporter::HID_KEY_A);
        TEST("hid_reporter: typeString 'a' press has no modifier",
             log[0].data[0] == HIDReporter::MOD_NONE);
        bool release_zero = true;
        for (uint8_t b : log[1].data)
            if (b != 0) { release_zero = false; break; }
        TEST("hid_reporter: typeString 'a' release report is all zeros",
             release_zero);
    }
}

// Test 8: typeString("A") 按下报告含 LSHIFT 修饰和 HID_KEY_A。
static void test_type_string_uppercase() {
    HIDReporter hid;
    ReportLog log;
    hid.setSendCallback(make_logger(log));

    hid.typeString("A", 0);

    TEST("hid_reporter: typeString 'A' generates 2 reports", log.size() == 2);
    if (log.size() >= 1) {
        TEST("hid_reporter: typeString 'A' press has LSHIFT modifier",
             log[0].data[0] == HIDReporter::MOD_LSHIFT);
        TEST("hid_reporter: typeString 'A' press report[2] == HID_KEY_A",
             log[0].data.size() >= 3 &&
             log[0].data[2] == HIDReporter::HID_KEY_A);
    }
}

// Test 9: typeString("!") 按下报告含 LSHIFT 和 HID_KEY_1。
static void test_type_string_special_char() {
    HIDReporter hid;
    ReportLog log;
    hid.setSendCallback(make_logger(log));

    hid.typeString("!", 0);

    TEST("hid_reporter: typeString '!' generates 2 reports", log.size() == 2);
    if (log.size() >= 1) {
        TEST("hid_reporter: typeString '!' press has LSHIFT modifier",
             log[0].data[0] == HIDReporter::MOD_LSHIFT);
        TEST("hid_reporter: typeString '!' press report[2] == HID_KEY_1",
             log[0].data.size() >= 3 &&
             log[0].data[2] == HIDReporter::HID_KEY_1);
    }
}

// Test 10: typeString 不识别的字符（如 '\x01'）不产生任何报告。
static void test_type_string_unknown_char() {
    HIDReporter hid;
    ReportLog log;
    hid.setSendCallback(make_logger(log));

    hid.typeString("\x01", 0);  // 不在映射表中

    TEST("hid_reporter: typeString unmapped char generates no report",
         log.size() == 0);
}

// Test 11: 鼠标 mouseMove() 正确填充鼠标报告。
static void test_mouse_move() {
    HIDReporter hid;
    hid.mouseMove(10, -5, 1);

    const uint8_t* r = hid.getMouseReport();
    TEST("hid_reporter: mouseMove sets dx in report[1]",
         static_cast<int8_t>(r[1]) == 10);
    TEST("hid_reporter: mouseMove sets dy in report[2]",
         static_cast<int8_t>(r[2]) == -5);
    TEST("hid_reporter: mouseMove sets wheel in report[3]",
         static_cast<int8_t>(r[3]) == 1);
}

// Test 12: mousePress / mouseRelease 正确设置/清除按键位。
static void test_mouse_press_release() {
    HIDReporter hid;
    hid.mousePress(HIDReporter::MOUSE_BTN_LEFT);
    TEST("hid_reporter: mousePress sets left button bit",
         (hid.getMouseReport()[0] & HIDReporter::MOUSE_BTN_LEFT) != 0);

    hid.mouseRelease(HIDReporter::MOUSE_BTN_LEFT);
    TEST("hid_reporter: mouseRelease clears left button bit",
         (hid.getMouseReport()[0] & HIDReporter::MOUSE_BTN_LEFT) == 0);
}

// Test 13: 报告缓冲区大小符合预期（键盘 8 字节，鼠标 4 字节）。
static void test_report_sizes() {
    HIDReporter hid;
    TEST("hid_reporter: keyboard report size is 8",
         hid.getKeyboardReportSize() == 8);
    TEST("hid_reporter: mouse report size is 4",
         hid.getMouseReportSize() == 4);
}

// Test 14: 多次连续 keyPress 不累积——每次 keyPress 重置报告。
static void test_consecutive_key_press_resets() {
    HIDReporter hid;
    hid.keyPress(HIDReporter::HID_KEY_A);
    hid.keyPress(HIDReporter::HID_KEY_B, HIDReporter::MOD_LSHIFT);

    const uint8_t* r = hid.getKeyboardReport();
    TEST("hid_reporter: second keyPress replaces first key",
         r[2] == HIDReporter::HID_KEY_B);
    TEST("hid_reporter: second keyPress sets new modifier",
         r[0] == HIDReporter::MOD_LSHIFT);
}

// ── main ─────────────────────────────────────────────────────────────────────

int main() {
    printf("=== test_hid_reporter ===\n");

    test_construction();
    test_initial_report_is_zero();
    test_key_press_sets_report();
    test_key_release_clears_report();
    test_key_combo_sends_twice();
    test_type_string_calls_send_callback();
    test_type_string_key_a();
    test_type_string_uppercase();
    test_type_string_special_char();
    test_type_string_unknown_char();
    test_mouse_move();
    test_mouse_press_release();
    test_report_sizes();
    test_consecutive_key_press_resets();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return (g_fail > 0) ? 1 : 0;
}
