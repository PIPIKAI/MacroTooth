#include "bt_adapter.h"
#include "logger.h"
#include <cstdio>
#include <cstring>
#include <sstream>

static const char* const BLUEZ_SERVICE = "org.bluez";
static const char* const PROPS_IFACE   = "org.freedesktop.DBus.Properties";
static const char* const ADAPTER_IFACE = "org.bluez.Adapter1";

BluetoothAdapter::BluetoothAdapter()
    : conn_(nullptr) {}

BluetoothAdapter::~BluetoothAdapter() {
    if (conn_) {
        dbus_connection_unref(conn_);
        conn_ = nullptr;
    }
}

bool BluetoothAdapter::init(int hci_index) {
    char buf[32];
    snprintf(buf, sizeof(buf), "hci%d", hci_index);
    hci_name_    = buf;
    adapter_path_ = std::string("/org/bluez/") + hci_name_;

    DBusError err;
    dbus_error_init(&err);
    conn_ = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set(&err)) {
        LOG_E("BluetoothAdapter: D-Bus connection failed: %s", err.message);
        dbus_error_free(&err);
        return false;
    }
    LOG_I("BluetoothAdapter: initialised on %s", adapter_path_.c_str());
    return true;
}

// ── Internal helpers ──────────────────────────────────────────────────────────

bool BluetoothAdapter::setBoolProperty(const char* name, bool val) {
    DBusMessage* msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, adapter_path_.c_str(), PROPS_IFACE, "Set");
    if (!msg) return false;

    DBusMessageIter args, var;
    dbus_message_iter_init_append(msg, &args);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &ADAPTER_IFACE);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &name);
    dbus_message_iter_open_container(&args, DBUS_TYPE_VARIANT, "b", &var);
    dbus_bool_t dval = val ? TRUE : FALSE;
    dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &dval);
    dbus_message_iter_close_container(&args, &var);

    DBusError err;
    dbus_error_init(&err);
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(
        conn_, msg, 3000, &err);
    dbus_message_unref(msg);

    bool ok = true;
    if (dbus_error_is_set(&err)) {
        LOG_E("BluetoothAdapter: Set %s failed: %s", name, err.message);
        dbus_error_free(&err);
        ok = false;
    }
    if (reply) dbus_message_unref(reply);
    return ok;
}

bool BluetoothAdapter::setStringProperty(const char* name,
                                          const std::string& val) {
    DBusMessage* msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, adapter_path_.c_str(), PROPS_IFACE, "Set");
    if (!msg) return false;

    DBusMessageIter args, var;
    dbus_message_iter_init_append(msg, &args);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &ADAPTER_IFACE);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &name);
    dbus_message_iter_open_container(&args, DBUS_TYPE_VARIANT, "s", &var);
    const char* cstr = val.c_str();
    dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &cstr);
    dbus_message_iter_close_container(&args, &var);

    DBusError err;
    dbus_error_init(&err);
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(
        conn_, msg, 3000, &err);
    dbus_message_unref(msg);

    bool ok = true;
    if (dbus_error_is_set(&err)) {
        LOG_E("BluetoothAdapter: Set %s failed: %s", name, err.message);
        dbus_error_free(&err);
        ok = false;
    }
    if (reply) dbus_message_unref(reply);
    return ok;
}

bool BluetoothAdapter::setUint32Property(const char* name, uint32_t val) {
    DBusMessage* msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, adapter_path_.c_str(), PROPS_IFACE, "Set");
    if (!msg) return false;

    DBusMessageIter args, var;
    dbus_message_iter_init_append(msg, &args);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &ADAPTER_IFACE);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &name);
    dbus_message_iter_open_container(&args, DBUS_TYPE_VARIANT, "u", &var);
    dbus_uint32_t dval = static_cast<dbus_uint32_t>(val);
    dbus_message_iter_append_basic(&var, DBUS_TYPE_UINT32, &dval);
    dbus_message_iter_close_container(&args, &var);

    DBusError err;
    dbus_error_init(&err);
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(
        conn_, msg, 3000, &err);
    dbus_message_unref(msg);

    bool ok = true;
    if (dbus_error_is_set(&err)) {
        LOG_E("BluetoothAdapter: Set %s failed: %s", name, err.message);
        dbus_error_free(&err);
        ok = false;
    }
    if (reply) dbus_message_unref(reply);
    return ok;
}

bool BluetoothAdapter::getBoolProperty(const char* name) const {
    if (!conn_) return false;

    DBusMessage* msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, adapter_path_.c_str(), PROPS_IFACE, "Get");
    if (!msg) return false;

    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &ADAPTER_IFACE);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &name);

    DBusError err;
    dbus_error_init(&err);
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(
        conn_, msg, 3000, &err);
    dbus_message_unref(msg);

    bool result = false;
    if (dbus_error_is_set(&err)) {
        dbus_error_free(&err);
    } else if (reply) {
        DBusMessageIter ri, vi;
        if (dbus_message_iter_init(reply, &ri) &&
            dbus_message_iter_get_arg_type(&ri) == DBUS_TYPE_VARIANT) {
            dbus_message_iter_recurse(&ri, &vi);
            if (dbus_message_iter_get_arg_type(&vi) == DBUS_TYPE_BOOLEAN) {
                dbus_bool_t v;
                dbus_message_iter_get_basic(&vi, &v);
                result = (v != FALSE);
            }
        }
        dbus_message_unref(reply);
    }
    return result;
}

// ── Public API ────────────────────────────────────────────────────────────────

bool BluetoothAdapter::setPowered(bool on) {
    LOG_I("BluetoothAdapter: setPowered(%s)", on ? "true" : "false");
    return setBoolProperty("Powered", on);
}

bool BluetoothAdapter::setAlias(const std::string& name) {
    LOG_I("BluetoothAdapter: setAlias(\"%s\")", name.c_str());
    return setStringProperty("Alias", name);
}

bool BluetoothAdapter::setDiscoverable(bool on, uint32_t timeout_s) {
    LOG_I("BluetoothAdapter: setDiscoverable(%s, timeout=%u)",
          on ? "true" : "false", timeout_s);
    bool ok = setUint32Property("DiscoverableTimeout", timeout_s);
    ok &= setBoolProperty("Discoverable", on);
    return ok;
}

bool BluetoothAdapter::setConnectable(bool on) {
    LOG_I("BluetoothAdapter: setConnectable(%s)", on ? "true" : "false");
    // "Pairable" controls whether the adapter accepts new pairings.
    // Some BlueZ builds also expose "Connectable" but it is not standard.
    return setBoolProperty("Pairable", on);
}

bool BluetoothAdapter::setClassOfDevice(uint32_t cod) {
    LOG_I("BluetoothAdapter: setClassOfDevice(0x%06X)", cod);
    // The Class property is read-only in org.bluez.Adapter1; set it via
    // hciconfig which writes directly to the HCI layer.
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "hciconfig %s class 0x%06X",
             hci_name_.c_str(), cod);
    int rc = system(cmd);
    if (rc != 0) {
        LOG_W("BluetoothAdapter: hciconfig class command returned %d", rc);
        return false;
    }
    return true;
}

bool BluetoothAdapter::getPowered() const {
    return getBoolProperty("Powered");
}

bool BluetoothAdapter::getDiscoverable() const {
    return getBoolProperty("Discoverable");
}

bool BluetoothAdapter::getConnectable() const {
    return getBoolProperty("Pairable");
}
