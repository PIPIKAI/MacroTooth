#include "gatt_application.h"
#include "hid_reporter.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>

// HID Report Descriptor - 键盘 + 鼠标组合
static const uint8_t hid_report_desc[] = {
    // 鼠标部分
    // 0x05, 0x01,        // Usage Page (Generic Desktop)
    // 0x09, 0x02,        // Usage (Mouse)
    // 0xA1, 0x01,        // Collection (Application)
    // 0x09, 0x01,        //   Usage (Pointer)
    // 0xA1, 0x00,        //   Collection (Physical)
    // 0x05, 0x09,        //     Usage Page (Button)
    // 0x19, 0x01,        //     Usage Minimum (1)
    // 0x29, 0x03,        //     Usage Maximum (3)
    // 0x15, 0x00,        //     Logical Minimum (0)
    // 0x25, 0x01,        //     Logical Maximum (1)
    // 0x95, 0x03,        //     Report Count (3)
    // 0x75, 0x01,        //     Report Size (1)
    // 0x81, 0x02,        //     Input (Data,Var,Abs) ; 3 button bits
    // 0x95, 0x01,        //     Report Count (1)
    // 0x75, 0x05,        //     Report Size (5)
    // 0x81, 0x01,        //     Input (Const) ; 5 bit padding
    // 0x05, 0x01,        //     Usage Page (Generic Desktop)
    // 0x09, 0x30,        //     Usage (X)
    // 0x09, 0x31,        //     Usage (Y)
    // 0x09, 0x38,        //     Usage (Wheel)
    // 0x15, 0x81,        //     Logical Minimum (-127)
    // 0x25, 0x7F,        //     Logical Maximum (127)
    // 0x75, 0x08,        //     Report Size (8)
    // 0x95, 0x03,        //     Report Count (3)
    // 0x81, 0x06,        //     Input (Data,Var,Rel) ; 3 bytes (X,Y,Wheel)
    // 0xC0,              //   End Collection
    // 0xC0,              // End Collection

    // 键盘部分
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x05, 0x07,        //   Usage Page (Keyboard)
    0x19, 0xE0,        //   Usage Minimum (Left Control)
    0x29, 0xE7,        //   Usage Maximum (Right GUI)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data,Var,Abs) ; 8 modifier bits
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x01,        //   Input (Const) ; Reserved byte
    0x95, 0x06,        //   Report Count (6)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x05, 0x07,        //   Usage Page (Keyboard)
    0x19, 0x00,        //   Usage Minimum (0)
    0x29, 0x65,        //   Usage Maximum (101)
    0x81, 0x00,        //   Input (Data,Array) ; 6 key array bytes
    0xC0               // End Collection
};

static bool running = true;

void signalHandler(int signum) {
    running = false;
}

// Dispatch D-Bus messages for the given duration while remaining interruptible.
static void dispatchFor(GattApplication& gatt, std::chrono::milliseconds total) {
    auto deadline = std::chrono::steady_clock::now() + total;
    while (running && std::chrono::steady_clock::now() < deadline)
        gatt.dispatch(10);
}

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    HIDReporter    hid;
    GattApplication gatt;

    // Register GATT application and start BLE advertisement.
    if (!gatt.start(hid_report_desc, sizeof(hid_report_desc))) {
        std::cerr << "Failed to start BLE GATT application" << std::endl;
        return 1;
    }

    std::cout << "BLE HID device is ready!" << std::endl;
    std::cout << "Advertising as 'MacroTooth' – connect with a BLE host." << std::endl;

    // Called when the host sends an Output report (e.g. keyboard LED state).
    gatt.setOutputReportCallback([](const uint8_t* data, size_t len) {
        std::cout << "Output report from host, length: " << len << std::endl;
    });

    // Demo: repeatedly send 'H' then 'e' once a BLE host has connected and
    // enabled notifications.  D-Bus messages are dispatched between key
    // events so that BlueZ read/write requests are handled promptly.
    std::cout << "\nStarting demo in 3 seconds..." << std::endl;
    dispatchFor(gatt, std::chrono::seconds(3));

    while (running) {
        std::cout << "Typing 'H'" << std::endl;

        hid.keyPress(HIDReporter::HID_KEY_H);
        gatt.sendInputReport(hid.getKeyboardReport(), hid.getKeyboardReportSize());
        dispatchFor(gatt, std::chrono::milliseconds(100));

        hid.keyRelease();
        gatt.sendInputReport(hid.getKeyboardReport(), hid.getKeyboardReportSize());
        dispatchFor(gatt, std::chrono::milliseconds(100));

        std::cout << "Typing 'e'" << std::endl;
        hid.keyPress(HIDReporter::HID_KEY_E);
        gatt.sendInputReport(hid.getKeyboardReport(), hid.getKeyboardReportSize());
        dispatchFor(gatt, std::chrono::milliseconds(100));

        hid.keyRelease();
        gatt.sendInputReport(hid.getKeyboardReport(), hid.getKeyboardReportSize());

        dispatchFor(gatt, std::chrono::seconds(2));
    }

    std::cout << "Shutting down..." << std::endl;
    gatt.stop();

    return 0;
}