#include "bt_agent.h"
#include "logger.h"
#include <cstring>

static const char* const AGENT_PATH   = "/com/macrotooth/agent";
static const char* const BLUEZ_SVC    = "org.bluez";
static const char* const AGENT_MGR    = "org.bluez.AgentManager1";
static const char* const BLUEZ_OBJ    = "/org/bluez";
static const char* const CAPABILITY   = "NoInputNoOutput";

// Minimal introspection XML exposed so that bluetoothd can discover methods.
static const char* const AGENT_INTROSPECT_XML =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
    "  \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
    "<node>\n"
    "  <interface name=\"org.bluez.Agent1\">\n"
    "    <method name=\"Release\"/>\n"
    "    <method name=\"Cancel\"/>\n"
    "    <method name=\"RequestPinCode\">\n"
    "      <arg name=\"device\" type=\"o\" direction=\"in\"/>\n"
    "      <arg name=\"pincode\" type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"DisplayPinCode\">\n"
    "      <arg name=\"device\" type=\"o\" direction=\"in\"/>\n"
    "      <arg name=\"pincode\" type=\"s\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <method name=\"RequestPasskey\">\n"
    "      <arg name=\"device\" type=\"o\" direction=\"in\"/>\n"
    "      <arg name=\"passkey\" type=\"u\" direction=\"out\"/>\n"
    "    </method>\n"
    "    <method name=\"DisplayPasskey\">\n"
    "      <arg name=\"device\" type=\"o\" direction=\"in\"/>\n"
    "      <arg name=\"passkey\" type=\"u\" direction=\"in\"/>\n"
    "      <arg name=\"entered\" type=\"q\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <method name=\"RequestConfirmation\">\n"
    "      <arg name=\"device\" type=\"o\" direction=\"in\"/>\n"
    "      <arg name=\"passkey\" type=\"u\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <method name=\"RequestAuthorization\">\n"
    "      <arg name=\"device\" type=\"o\" direction=\"in\"/>\n"
    "    </method>\n"
    "    <method name=\"AuthorizeService\">\n"
    "      <arg name=\"device\" type=\"o\" direction=\"in\"/>\n"
    "      <arg name=\"uuid\"   type=\"s\" direction=\"in\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
    "    <method name=\"Introspect\">\n"
    "      <arg name=\"xml\" type=\"s\" direction=\"out\"/>\n"
    "    </method>\n"
    "  </interface>\n"
    "</node>\n";

// ── BluetoothAgent ─────────────────────────────────────────────────────────────

BluetoothAgent::BluetoothAgent()
    : conn_(nullptr), registered_(false) {}

BluetoothAgent::~BluetoothAgent() {
    stop();
    if (conn_) {
        dbus_connection_unref(conn_);
        conn_ = nullptr;
    }
}

bool BluetoothAgent::start() {
    DBusError err;
    dbus_error_init(&err);
    conn_ = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set(&err)) {
        LOG_E("BluetoothAgent: D-Bus connection failed: %s", err.message);
        dbus_error_free(&err);
        return false;
    }

    // Register the object path with an explicit handler (not fallback) so
    // that GattApplication's fallback at /com/macrotooth does not intercept.
    static const DBusObjectPathVTable vtable = {
        nullptr, staticHandler, nullptr, nullptr, nullptr, nullptr
    };
    if (!dbus_connection_register_object_path(conn_, AGENT_PATH, &vtable, this)) {
        LOG_E("BluetoothAgent: failed to register object path %s", AGENT_PATH);
        return false;
    }

    if (!registerWithManager()) {
        dbus_connection_unregister_object_path(conn_, AGENT_PATH);
        return false;
    }

    LOG_I("BluetoothAgent: registered at %s (capability: %s)",
          AGENT_PATH, CAPABILITY);
    return true;
}

void BluetoothAgent::stop() {
    if (registered_) {
        unregisterFromManager();
        registered_ = false;
    }
    if (conn_) {
        dbus_connection_unregister_object_path(conn_, AGENT_PATH);
    }
}

// ── Private ───────────────────────────────────────────────────────────────────

bool BluetoothAgent::registerWithManager() {
    // RegisterAgent(object_path, capability)
    DBusMessage* msg = dbus_message_new_method_call(
        BLUEZ_SVC, BLUEZ_OBJ, AGENT_MGR, "RegisterAgent");
    if (!msg) return false;

    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);
    const char* path = AGENT_PATH;
    const char* cap  = CAPABILITY;
    dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &path);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING,      &cap);

    DBusError err;
    dbus_error_init(&err);
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(
        conn_, msg, 3000, &err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&err)) {
        LOG_E("BluetoothAgent: RegisterAgent failed: %s", err.message);
        dbus_error_free(&err);
        return false;
    }
    if (reply) dbus_message_unref(reply);

    // Request to become the default agent so all pairing goes through us.
    DBusMessage* req = dbus_message_new_method_call(
        BLUEZ_SVC, BLUEZ_OBJ, AGENT_MGR, "RequestDefaultAgent");
    if (req) {
        DBusMessageIter ra;
        dbus_message_iter_init_append(req, &ra);
        dbus_message_iter_append_basic(&ra, DBUS_TYPE_OBJECT_PATH, &path);
        dbus_error_init(&err);
        DBusMessage* r = dbus_connection_send_with_reply_and_block(
            conn_, req, 3000, &err);
        dbus_message_unref(req);
        if (dbus_error_is_set(&err)) {
            LOG_W("BluetoothAgent: RequestDefaultAgent failed: %s", err.message);
            dbus_error_free(&err);
        }
        if (r) dbus_message_unref(r);
    }

    registered_ = true;
    return true;
}

void BluetoothAgent::unregisterFromManager() {
    DBusMessage* msg = dbus_message_new_method_call(
        BLUEZ_SVC, BLUEZ_OBJ, AGENT_MGR, "UnregisterAgent");
    if (!msg) return;

    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);
    const char* path = AGENT_PATH;
    dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &path);

    dbus_connection_send(conn_, msg, nullptr);
    dbus_connection_flush(conn_);
    dbus_message_unref(msg);
}

// ── D-Bus message handler ─────────────────────────────────────────────────────

DBusHandlerResult BluetoothAgent::staticHandler(DBusConnection* conn,
                                                 DBusMessage* msg,
                                                 void* user_data) {
    return static_cast<BluetoothAgent*>(user_data)->onMessage(conn, msg);
}

DBusHandlerResult BluetoothAgent::onMessage(DBusConnection* conn,
                                             DBusMessage* msg) {
    const char* iface  = dbus_message_get_interface(msg);
    const char* method = dbus_message_get_member(msg);
    if (!iface || !method)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    DBusMessage* reply = nullptr;

    if (strcmp(iface, "org.bluez.Agent1") == 0) {

        if (strcmp(method, "Release") == 0) {
            LOG_I("BluetoothAgent: Release called");
            registered_ = false;
            reply = dbus_message_new_method_return(msg);

        } else if (strcmp(method, "Cancel") == 0) {
            LOG_I("BluetoothAgent: Cancel called");
            reply = dbus_message_new_method_return(msg);

        } else if (strcmp(method, "RequestPinCode") == 0) {
            // NoInputNoOutput should never receive this, but handle it anyway.
            LOG_I("BluetoothAgent: RequestPinCode – auto-returning \"0000\"");
            reply = dbus_message_new_method_return(msg);
            if (reply) {
                const char* pin = "0000";
                dbus_message_append_args(reply,
                    DBUS_TYPE_STRING, &pin, DBUS_TYPE_INVALID);
            }

        } else if (strcmp(method, "DisplayPinCode") == 0) {
            reply = dbus_message_new_method_return(msg);

        } else if (strcmp(method, "RequestPasskey") == 0) {
            LOG_I("BluetoothAgent: RequestPasskey – auto-returning 0");
            reply = dbus_message_new_method_return(msg);
            if (reply) {
                dbus_uint32_t passkey = 0;
                dbus_message_append_args(reply,
                    DBUS_TYPE_UINT32, &passkey, DBUS_TYPE_INVALID);
            }

        } else if (strcmp(method, "DisplayPasskey") == 0) {
            reply = dbus_message_new_method_return(msg);

        } else if (strcmp(method, "RequestConfirmation") == 0) {
            // Auto-confirm; do not ask the user.
            LOG_I("BluetoothAgent: RequestConfirmation – auto-accepting");
            reply = dbus_message_new_method_return(msg);

        } else if (strcmp(method, "RequestAuthorization") == 0) {
            LOG_I("BluetoothAgent: RequestAuthorization – auto-accepting");
            reply = dbus_message_new_method_return(msg);

        } else if (strcmp(method, "AuthorizeService") == 0) {
            LOG_I("BluetoothAgent: AuthorizeService – auto-accepting");
            reply = dbus_message_new_method_return(msg);
        }

    } else if (strcmp(iface, "org.freedesktop.DBus.Introspectable") == 0 &&
               strcmp(method, "Introspect") == 0) {
        reply = dbus_message_new_method_return(msg);
        if (reply) {
            dbus_message_append_args(reply,
                DBUS_TYPE_STRING, &AGENT_INTROSPECT_XML, DBUS_TYPE_INVALID);
        }
    }

    if (reply) {
        dbus_connection_send(conn, reply, nullptr);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
