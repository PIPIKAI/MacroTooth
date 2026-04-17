#include "uhid_device.h"
#include "hid_reporter.h"
#include "bluez_profile.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <unistd.h>

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

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 1. 创建 UHID 设备
    UHIDDevice uhid;
    HIDReporter hid;
    
    if (!uhid.create("MacroTooth HID", 
                    "00:11:22:33:44:55",
                    hid_report_desc, 
                    sizeof(hid_report_desc),
                    0x1234,  // Vendor ID
                    0x5678)) { // Product ID
        std::cerr << "Failed to create UHID device" << std::endl;
        return 1;
    }
    
    std::cout << "UHID device created successfully!" << std::endl;
    
    // 设置回调处理主机数据（如键盘 LED 状态）
    uhid.setDataCallback([](const uint8_t* data, size_t len) {
        std::cout << "Received data from host, length: " << len << std::endl;
        // 这里可以处理键盘 LED 状态
    });
    
    uhid.setControlCallback([](uint8_t event, const uint8_t* data, size_t len) {
        if (event == UHID_GET_REPORT) {
            std::cout << "GET_REPORT request received" << std::endl;
        } else if (event == UHID_SET_REPORT) {
            std::cout << "SET_REPORT request received, length: " << len << std::endl;
        }
    });
    
    // 2. 注册 BlueZ HID Profile
    BlueZProfile profile;
    if (!profile.registerHIDProfile()) {
        std::cerr << "Failed to register HID profile" << std::endl;
        return 1;
    }
    
    std::cout << "HID Profile registered. Device is ready!" << std::endl;
    std::cout << "You can now connect to this device via Bluetooth." << std::endl;
    
    // 3. 演示：模拟一些按键和鼠标操作
    std::cout << "\nStarting demo in 3 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    while (running) {
        std::cout << "Typing 'H'" << std::endl;
    
        // 手动发送 'H'
        hid.keyPress(HIDReporter::HID_KEY_H);
        uhid.sendInputReport(hid.getKeyboardReport(), hid.getKeyboardReportSize());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        hid.keyRelease();
        uhid.sendInputReport(hid.getKeyboardReport(), hid.getKeyboardReportSize());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        std::cout << "Typing 'e'" << std::endl;
        hid.keyPress(HIDReporter::HID_KEY_E);
        uhid.sendInputReport(hid.getKeyboardReport(), hid.getKeyboardReportSize());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        hid.keyRelease();
        uhid.sendInputReport(hid.getKeyboardReport(), hid.getKeyboardReportSize());
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    std::cout << "Shutting down..." << std::endl;
    uhid.destroy();
    
    return 0;
}