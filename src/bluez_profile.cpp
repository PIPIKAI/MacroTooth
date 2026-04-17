#include "bluez_profile.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <thread>

BlueZProfile::BlueZProfile() : conn_(nullptr) {
}

BlueZProfile::~BlueZProfile() {
    unregisterProfile();
    if (conn_) {
        dbus_connection_unref(conn_);
    }
}

bool BlueZProfile::createDbusConnection() {
    DBusError error;
    dbus_error_init(&error);
    
    conn_ = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
    if (dbus_error_is_set(&error)) {
        std::cerr << "D-Bus connection error: " << error.message << std::endl;
        dbus_error_free(&error);
        return false;
    }
    
    // 注册消息处理器
    if (!dbus_connection_add_filter(conn_, messageHandler, this, nullptr)) {
        std::cerr << "Failed to add message filter" << std::endl;
        return false;
    }
    
    return true;
}

DBusHandlerResult BlueZProfile::messageHandler(DBusConnection* conn, 
                                              DBusMessage* msg, 
                                              void* user_data) {
    BlueZProfile* self = static_cast<BlueZProfile*>(user_data);
    
    if (dbus_message_is_method_call(msg, "org.bluez.Profile1", "NewConnection")) {
        std::cout << "New Bluetooth connection request" << std::endl;
        
        // 解析参数
        DBusMessageIter args;
        if (!dbus_message_iter_init(msg, &args)) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
        
        // 参数1: device 对象路径
        const char* device_path;
        dbus_message_iter_get_basic(&args, &device_path);
        dbus_message_iter_next(&args);
        
        // 参数2: fd (文件描述符)
        DBusBasicValue fd_value;
        dbus_message_iter_get_basic(&args, &fd_value);
        int fd = fd_value.fd;
        
        std::cout << "Device connected: " << device_path << ", fd: " << fd << std::endl;
        
        // 重要：这里需要读取 fd 并转发给 UHID
        // 启动一个线程来处理 HID 数据
        std::thread([fd]() {
            uint8_t buffer[1024];
            while (true) {
                ssize_t len = read(fd, buffer, sizeof(buffer));
                if (len <= 0) break;
                // 处理接收到的数据
                std::cout << "Received " << len << " bytes from HID host" << std::endl;
            }
            close(fd);
        }).detach();

        // 发送成功响应
        DBusMessage* reply = dbus_message_new_method_return(msg);
        if (reply) {
            dbus_connection_send(conn, reply, nullptr);
            dbus_message_unref(reply);
        }
        
        if (self->conn_callback_) {
            self->conn_callback_(true);
        }
        
        return DBUS_HANDLER_RESULT_HANDLED;
        
    } else if (dbus_message_is_method_call(msg, "org.bluez.Profile1", "RequestDisconnection")) {
        std::cout << "Bluetooth disconnection request" << std::endl;
        
        DBusMessageIter args;
        dbus_message_iter_init(msg, &args);
        const char* device_path;
        dbus_message_iter_get_basic(&args, &device_path);
        
        std::cout << "Device disconnecting: " << device_path << std::endl;
        
        DBusMessage* reply = dbus_message_new_method_return(msg);
        if (reply) {
            dbus_connection_send(conn, reply, nullptr);
            dbus_message_unref(reply);
        }
        
        if (self->conn_callback_) {
            self->conn_callback_(false);
        }
        
        return DBUS_HANDLER_RESULT_HANDLED;
        
    } else if (dbus_message_is_method_call(msg, "org.bluez.Profile1", "Release")) {
        std::cout << "Profile Release called" << std::endl;
        DBusMessage* reply = dbus_message_new_method_return(msg);
        if (reply) {
            dbus_connection_send(conn, reply, nullptr);
            dbus_message_unref(reply);
        }
        return DBUS_HANDLER_RESULT_HANDLED;
        
    } else if (dbus_message_is_method_call(msg, "org.bluez.Profile1", "Cancel")) {
        std::cout << "Profile Cancel called" << std::endl;
        DBusMessage* reply = dbus_message_new_method_return(msg);
        if (reply) {
            dbus_connection_send(conn, reply, nullptr);
            dbus_message_unref(reply);
        }
        return DBUS_HANDLER_RESULT_HANDLED;
        
    } else if (dbus_message_is_method_call(msg, "org.freedesktop.DBus.Introspectable", "Introspect")) {
        const char* introspection_xml =
            "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
            "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
            "<node>\n"
            "  <interface name=\"org.bluez.Profile1\">\n"
            "    <method name=\"Release\">\n"
            "    </method>\n"
            "    <method name=\"Cancel\">\n"
            "    </method>\n"
            "    <method name=\"NewConnection\">\n"
            "      <arg name=\"device\" type=\"o\" direction=\"in\"/>\n"
            "      <arg name=\"fd\" type=\"h\" direction=\"in\"/>\n"
            "      <arg name=\"fd_properties\" type=\"a{sv}\" direction=\"in\"/>\n"
            "    </method>\n"
            "    <method name=\"RequestDisconnection\">\n"
            "      <arg name=\"device\" type=\"o\" direction=\"in\"/>\n"
            "    </method>\n"
            "  </interface>\n"
            "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
            "    <method name=\"Introspect\">\n"
            "      <arg name=\"xml\" type=\"s\" direction=\"out\"/>\n"
            "    </method>\n"
            "  </interface>\n"
            "</node>\n";
        
        DBusMessage* reply = dbus_message_new_method_return(msg);
        if (reply) {
            dbus_message_append_args(reply,
                                    DBUS_TYPE_STRING, &introspection_xml,
                                    DBUS_TYPE_INVALID);
            dbus_connection_send(conn, reply, nullptr);
            dbus_message_unref(reply);
        }
        
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

bool BlueZProfile::registerHIDProfile() {
    if (!createDbusConnection()) {
        return false;
    }
    
    // 注册对象路径
    DBusObjectPathVTable vtable;
    memset(&vtable, 0, sizeof(vtable));
    vtable.message_function = messageHandler;
    
    const char* profile_path = "/com/macrotooth/hid";
    
    if (!dbus_connection_register_object_path(conn_, 
                                             profile_path,
                                             &vtable,
                                             this)) {
        std::cerr << "Failed to register object path" << std::endl;
        return false;
    }
    
    std::cout << "Object path registered: " << profile_path << std::endl;
    
    // 关键修复：ProfileManager1 的正确对象路径是 /org/bluez，不是 /org/bluez/hci0
    DBusMessage* msg = dbus_message_new_method_call(
        "org.bluez",                    // 服务名
        "/org/bluez",                   // 正确的对象路径！
        "org.bluez.ProfileManager1",    // 接口名
        "RegisterProfile"               // 方法名
    );
    
    if (!msg) {
        std::cerr << "Failed to create D-Bus message" << std::endl;
        return false;
    }
    
    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);
    
    // 参数1: Profile 对象路径
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &profile_path)) {
        std::cerr << "Failed to append profile path" << std::endl;
        dbus_message_unref(msg);
        return false;
    }
    
    // 参数2: UUID (HID 服务)
    const char* uuid = "00001124-0000-1000-8000-00805f9b34fb";
    if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &uuid)) {
        std::cerr << "Failed to append UUID" << std::endl;
        dbus_message_unref(msg);
        return false;
    }
    
    // 参数3: 选项字典
    DBusMessageIter dict;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);
    
    // 添加 Name
    const char *name_key = "Name", *name_val = "MacroTooth HID";
    DBusMessageIter entry, var;
    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &name_key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &var);
    dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &name_val);
    dbus_message_iter_close_container(&entry, &var);
    dbus_message_iter_close_container(&dict, &entry);
    
    // 添加 Role = server
    const char *role_key = "Role", *role_val = "server";
    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &role_key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &var);
    dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &role_val);
    dbus_message_iter_close_container(&entry, &var);
    dbus_message_iter_close_container(&dict, &entry);
    
    // 添加 AutoConnect = true
    const char *auto_key = "AutoConnect";
    dbus_bool_t auto_val = TRUE;
    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &auto_key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b", &var);
    dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &auto_val);
    dbus_message_iter_close_container(&entry, &var);
    dbus_message_iter_close_container(&dict, &entry);
    
    // 添加 RequireAuthentication = false
    const char *auth_key = "RequireAuthentication";
    dbus_bool_t auth_val = FALSE;
    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &auth_key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b", &var);
    dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &auth_val);
    dbus_message_iter_close_container(&entry, &var);
    dbus_message_iter_close_container(&dict, &entry);
    
    // 添加 RequireAuthorization = false
    const char *authz_key = "RequireAuthorization";
    dbus_bool_t authz_val = FALSE;
    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &authz_key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b", &var);
    dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &authz_val);
    dbus_message_iter_close_container(&entry, &var);
    dbus_message_iter_close_container(&dict, &entry);
    
    // 添加 MITM = false
    const char *mitm_key = "MITM";
    dbus_bool_t mitm_val = FALSE;
    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, NULL, &entry);
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &mitm_key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b", &var);
    dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &mitm_val);
    dbus_message_iter_close_container(&entry, &var);
    dbus_message_iter_close_container(&dict, &entry);
    
    dbus_message_iter_close_container(&args, &dict);

    // 发送并等待回复
    DBusError error;
    dbus_error_init(&error);
    
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(
        conn_, msg, -1, &error
    );
    
    dbus_message_unref(msg);
    
    if (dbus_error_is_set(&error)) {
        std::cerr << "D-Bus error: " << error.message << std::endl;
        dbus_error_free(&error);
        return false;
    }
    
    if (reply) {
        dbus_message_unref(reply);
    }
    
    std::cout << "HID Profile registered successfully!" << std::endl;
    return true;
}

void BlueZProfile::unregisterProfile() {
    if (!conn_) return;
    
    // 先取消注册对象路径
    dbus_connection_unregister_object_path(conn_, "/com/macrotooth/hid");
    
    // 然后通知 BlueZ 取消注册 Profile
    // 注意：UnregisterProfile 的正确对象路径也是 /org/bluez
    DBusMessage* msg = dbus_message_new_method_call(
        "org.bluez",
        "/org/bluez",                       // 正确的对象路径
        "org.bluez.ProfileManager1",
        "UnregisterProfile"
    );
    
    if (msg) {
        DBusMessageIter args;
        dbus_message_iter_init_append(msg, &args);
        
        const char* profile_path = "/com/macrotooth/hid";
        dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &profile_path);
        
        // 直接发送，不等待回复
        dbus_connection_send(conn_, msg, nullptr);
        dbus_connection_flush(conn_);
        dbus_message_unref(msg);
    }
}