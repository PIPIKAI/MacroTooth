#ifndef BLUEZ_PROFILE_H
#define BLUEZ_PROFILE_H

#include <dbus/dbus.h>
#include <string>
#include <functional>

class BlueZProfile {
public:
    BlueZProfile();
    ~BlueZProfile();
    
    bool registerHIDProfile();
    void unregisterProfile();
    
    void setConnectionCallback(std::function<void(bool)> cb) { conn_callback_ = cb; }
    
private:
    DBusConnection* conn_;
    std::function<void(bool)> conn_callback_;
    
    bool createDbusConnection();
    static DBusHandlerResult messageHandler(DBusConnection* conn, 
                                           DBusMessage* msg, 
                                           void* user_data);
};

#endif // BLUEZ_PROFILE_H