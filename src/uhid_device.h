#ifndef UHID_DEVICE_H
#define UHID_DEVICE_H

#include <cstdint>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <linux/uhid.h>

class UHIDDevice {
public:
    // 回调函数类型定义
    using DataCallback = std::function<void(const uint8_t* data, size_t len)>;
    using ControlCallback = std::function<void(uint8_t event, const uint8_t* data, size_t len)>;
    
    UHIDDevice();
    ~UHIDDevice();
    
    // 创建和销毁 UHID 设备
    bool create(const std::string& name, 
                const std::string& phys,
                const uint8_t* report_desc, 
                size_t report_desc_len,
                uint16_t vendor_id = 0x1234,
                uint16_t product_id = 0x5678);
    
    void destroy();
    
    // 发送 HID Report 到主机
    bool sendInputReport(const uint8_t* data, size_t len);
    
    // 设置回调函数
    void setDataCallback(DataCallback cb) { data_callback_ = cb; }
    void setControlCallback(ControlCallback cb) { control_callback_ = cb; }
    
    // 检查设备是否已创建
    bool isCreated() const { return created_; }
    
private:
    void eventLoop();
    bool writeUhidEvent(const struct uhid_event& ev);
    
    int fd_;
    std::atomic<bool> running_;
    std::atomic<bool> created_;
    std::thread event_thread_;
    
    DataCallback data_callback_;
    ControlCallback control_callback_;
    
    uint32_t handle_;
};

#endif // UHID_DEVICE_H