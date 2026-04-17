#include "uhid_device.h"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sys/ioctl.h>

UHIDDevice::UHIDDevice() 
    : fd_(-1)
    , running_(false)
    , created_(false)
    , handle_(0) {
}

UHIDDevice::~UHIDDevice() {
    destroy();
}

bool UHIDDevice::create(const std::string& name, 
                        const std::string& phys,
                        const uint8_t* report_desc, 
                        size_t report_desc_len,
                        uint16_t vendor_id,
                        uint16_t product_id) {
    if (created_) {
        std::cerr << "UHID device already created" << std::endl;
        return false;
    }
    
    // 打开 UHID 控制接口
    fd_ = open("/dev/uhid", O_RDWR);
    if (fd_ < 0) {
        perror("open /dev/uhid");
        return false;
    }
    
    // 准备创建设备的事件
    struct uhid_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = UHID_CREATE2;
    
    // 填充设备信息
    strncpy((char*)ev.u.create2.name, name.c_str(), sizeof(ev.u.create2.name) - 1);
    strncpy((char*)ev.u.create2.phys, phys.c_str(), sizeof(ev.u.create2.phys) - 1);
    ev.u.create2.vendor = vendor_id;
    ev.u.create2.product = product_id;
    ev.u.create2.version = 0x0100;
    ev.u.create2.country = 0x00;  // Not localized
    
    // 设置 Report Descriptor
    if (report_desc_len > sizeof(ev.u.create2.rd_data)) {
        std::cerr << "Report descriptor too large" << std::endl;
        close(fd_);
        return false;
    }
    
    ev.u.create2.rd_size = report_desc_len;
    memcpy(ev.u.create2.rd_data, report_desc, report_desc_len);
    
    // 发送创建设备事件
    if (!writeUhidEvent(ev)) {
        close(fd_);
        return false;
    }
    
    // 启动事件处理线程
    running_ = true;
    event_thread_ = std::thread(&UHIDDevice::eventLoop, this);
    
    created_ = true;
    return true;
}

void UHIDDevice::destroy() {
    if (!created_) return;
    
    running_ = false;
    
    // 发送销毁设备事件
    struct uhid_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = UHID_DESTROY;
    writeUhidEvent(ev);
    
    if (event_thread_.joinable()) {
        event_thread_.join();
    }
    
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
    
    created_ = false;
}

bool UHIDDevice::sendInputReport(const uint8_t* data, size_t len) {
    if (!created_) return false;
    
    struct uhid_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = UHID_INPUT2;
    ev.u.input2.size = len;
    memcpy(ev.u.input2.data, data, len);
    
    return writeUhidEvent(ev);
}

void UHIDDevice::eventLoop() {
    struct uhid_event ev;
    
    while (running_) {
        ssize_t ret = read(fd_, &ev, sizeof(ev));
        if (ret < 0) {
            if (running_) perror("read uhid event");
            break;
        }
        
        if (ret != sizeof(ev)) {
            std::cerr << "Invalid UHID event size: " << ret << std::endl;
            continue;
        }
        
        switch (ev.type) {
        case UHID_START:
            handle_ = ev.u.start.dev_flags;
            std::cout << "UHID device started, handle: " << handle_ << std::endl;
            break;
            
        case UHID_STOP:
            std::cout << "UHID device stopped" << std::endl;
            break;
            
        case UHID_OPEN:
            std::cout << "UHID device opened by host" << std::endl;
            break;
            
        case UHID_CLOSE:
            std::cout << "UHID device closed by host" << std::endl;
            break;
            
        case UHID_OUTPUT:
            // 主机发送数据到设备（例如键盘 LED 状态）
            if (data_callback_) {
                data_callback_(ev.u.output.data, ev.u.output.size);
            }
            break;
            
        case UHID_GET_REPORT:
            // 主机请求获取报告 - 修复结构体成员名
            if (control_callback_) {
                control_callback_(UHID_GET_REPORT, 
                                nullptr,  // 简化，不传递具体类型
                                ev.u.get_report.id);
            }
            
            // 发送响应
            {
                struct uhid_event reply;
                memset(&reply, 0, sizeof(reply));
                reply.type = UHID_GET_REPORT_REPLY;
                reply.u.get_report_reply.id = ev.u.get_report.id;
                reply.u.get_report_reply.err = 0;
                reply.u.get_report_reply.size = 0;
                writeUhidEvent(reply);
            }
    break;
            
        case UHID_SET_REPORT:
            // 主机设置报告
            if (control_callback_) {
                control_callback_(UHID_SET_REPORT,
                                ev.u.set_report.data,
                                ev.u.set_report.size);
            }
            
            // 发送响应
            {
                struct uhid_event reply;
                memset(&reply, 0, sizeof(reply));
                reply.type = UHID_SET_REPORT_REPLY;
                reply.u.set_report_reply.id = ev.u.set_report.id;
                reply.u.set_report_reply.err = 0;
                writeUhidEvent(reply);
            }
            break;
            
        default:
            std::cerr << "Unknown UHID event: " << ev.type << std::endl;
            break;
        }
    }
}

bool UHIDDevice::writeUhidEvent(const struct uhid_event& ev) {
    ssize_t ret = write(fd_, &ev, sizeof(ev));
    if (ret < 0) {
        perror("write uhid event");
        return false;
    }
    if (ret != sizeof(ev)) {
        std::cerr << "Partial write to uhid: " << ret << std::endl;
        return false;
    }
    return true;
}