#ifndef BLUEZ_PROFILE_H
#define BLUEZ_PROFILE_H

#include <dbus/dbus.h>
#include <string>
#include <functional>
#include <atomic>
#include <cstdint>

// Registers a Classic Bluetooth (BR/EDR) HID profile with BlueZ via
// org.bluez.ProfileManager1.  When a host connects over the HID interrupt
// channel BlueZ calls NewConnection, passing an L2CAP socket FD.
// sendInputReport() writes an HID INPUT report to that socket.
class BlueZProfile {
public:
    BlueZProfile();
    ~BlueZProfile();

    bool registerHIDProfile();
    void unregisterProfile();

    // Send a keyboard/mouse HID Input Report over the Classic BT L2CAP
    // interrupt channel.  Prepends the 0xA1 DATA|INPUT transaction header.
    // Returns false if no Classic BT host is currently connected.
    bool sendInputReport(const uint8_t* data, size_t len);

    bool isConnected() const { return interrupt_fd_ >= 0; }

    void setConnectionCallback(std::function<void(bool)> cb) { conn_callback_ = cb; }

private:
    DBusConnection* conn_;
    std::atomic<int> interrupt_fd_;
    std::function<void(bool)> conn_callback_;

    bool createDbusConnection();
    static DBusHandlerResult messageHandler(DBusConnection* conn,
                                            DBusMessage* msg,
                                            void* user_data);
};

#endif // BLUEZ_PROFILE_H
