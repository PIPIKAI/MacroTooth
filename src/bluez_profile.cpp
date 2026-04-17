#include "bluez_profile.h"
#include "logger.h"
#include <iostream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <thread>
#include <vector>

BlueZProfile::BlueZProfile() : conn_(nullptr), interrupt_fd_(-1) {
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
        LOG_I("BlueZProfile: new Classic BT HID connection");
        
        // Parse arguments: device_path (o), fd (h), fd_properties (a{sv})
        DBusMessageIter args;
        if (!dbus_message_iter_init(msg, &args)) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
        
        const char* device_path;
        dbus_message_iter_get_basic(&args, &device_path);
        dbus_message_iter_next(&args);
        
        DBusBasicValue fd_value;
        dbus_message_iter_get_basic(&args, &fd_value);
        int new_fd = fd_value.fd;
        
        LOG_I("BlueZProfile: device connected: %s, fd=%d", device_path, new_fd);
        
        // Close any previous connection FD.
        int old_fd = self->interrupt_fd_.exchange(new_fd);
        if (old_fd >= 0)
            close(old_fd);
        
        // Read output reports (e.g. LED state) from the host in a background thread.
        std::thread([self, new_fd]() {
            uint8_t buffer[64];
            while (true) {
                ssize_t len = read(new_fd, buffer, sizeof(buffer));
                if (len <= 0) break;
                LOG_D("BlueZProfile: received %zd bytes from host", len);
            }
            // Connection closed – clear the stored FD only if it still matches
            // the one this thread was reading, to avoid clearing a newer FD.
            // If the CAS fails it means a new connection replaced this one;
            // the disconnect callback is implicitly handled when that newer
            // connection eventually closes.
            int expected = new_fd;
            if (self->interrupt_fd_.compare_exchange_strong(expected, -1)) {
                LOG_I("BlueZProfile: Classic BT HID connection closed");
                if (self->conn_callback_)
                    self->conn_callback_(false);
            }
            // Always close our copy of the FD regardless of the CAS result.
            close(new_fd);
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
        LOG_I("BlueZProfile: RequestDisconnection");
        
        DBusMessageIter args;
        dbus_message_iter_init(msg, &args);
        const char* device_path;
        dbus_message_iter_get_basic(&args, &device_path);
        
        LOG_I("BlueZProfile: device disconnecting: %s", device_path);
        
        // Close and clear the interrupt FD.
        int fd = self->interrupt_fd_.exchange(-1);
        if (fd >= 0) close(fd);
        
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
        LOG_I("BlueZProfile: Release called");
        DBusMessage* reply = dbus_message_new_method_return(msg);
        if (reply) {
            dbus_connection_send(conn, reply, nullptr);
            dbus_message_unref(reply);
        }
        return DBUS_HANDLER_RESULT_HANDLED;
        
    } else if (dbus_message_is_method_call(msg, "org.bluez.Profile1", "Cancel")) {
        LOG_I("BlueZProfile: Cancel called");
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

// ── sendInputReport ───────────────────────────────────────────────────────────

bool BlueZProfile::sendInputReport(const uint8_t* data, size_t len) {
    int fd = interrupt_fd_.load();
    if (fd < 0) return false;

    // Classic BT HID interrupt channel format:
    //   byte 0: 0xA1  (DATA message, INPUT report type)
    //   bytes 1..N: report payload
    std::vector<uint8_t> buf;
    buf.reserve(1 + len);
    buf.push_back(0xA1);
    buf.insert(buf.end(), data, data + len);

    size_t total   = buf.size();
    size_t written = 0;
    while (written < total) {
        ssize_t ret = write(fd, buf.data() + written, total - written);
        if (ret < 0) {
            LOG_E("BlueZProfile: sendInputReport write error: %s", strerror(errno));
            return false;
        }
        written += static_cast<size_t>(ret);
    }
    return true;
}

bool BlueZProfile::registerHIDProfile() {
    if (!createDbusConnection()) {
        return false;
    }
    
    // Register object path
    DBusObjectPathVTable vtable;
    memset(&vtable, 0, sizeof(vtable));
    vtable.message_function = messageHandler;
    
    const char* profile_path = "/com/macrotooth/hid";
    
    if (!dbus_connection_register_object_path(conn_, 
                                             profile_path,
                                             &vtable,
                                             this)) {
        LOG_E("BlueZProfile: failed to register object path");
        return false;
    }
    
    LOG_I("BlueZProfile: object path registered: %s", profile_path);
    
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
        LOG_E("BlueZProfile: RegisterProfile D-Bus error: %s", error.message);
        dbus_error_free(&error);
        return false;
    }
    
    if (reply) {
        dbus_message_unref(reply);
    }
    
    LOG_I("BlueZProfile: HID profile registered successfully");
    return true;
}

void BlueZProfile::unregisterProfile() {
    if (!conn_) return;
    
    // Close the interrupt FD if still open.
    int fd = interrupt_fd_.exchange(-1);
    if (fd >= 0) close(fd);
    
    dbus_connection_unregister_object_path(conn_, "/com/macrotooth/hid");
    
    DBusMessage* msg = dbus_message_new_method_call(
        "org.bluez",
        "/org/bluez",
        "org.bluez.ProfileManager1",
        "UnregisterProfile"
    );
    
    if (msg) {
        DBusMessageIter args;
        dbus_message_iter_init_append(msg, &args);
        
        const char* profile_path = "/com/macrotooth/hid";
        dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &profile_path);
        
        dbus_connection_send(conn_, msg, nullptr);
        dbus_connection_flush(conn_);
        dbus_message_unref(msg);
    }
}