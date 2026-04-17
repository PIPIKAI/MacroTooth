#include "gatt_application.h"
#include <iostream>
#include <cstring>
#include <algorithm>

// ── D-Bus object paths ────────────────────────────────────────────────────────
static const char* const APP_PATH   = "/com/macrotooth/app";
static const char* const SVC0_PATH  = "/com/macrotooth/app/service0";        // HID 0x1812
static const char* const CHAR0_PATH = "/com/macrotooth/app/service0/char0";  // Report Map
static const char* const CHAR1_PATH = "/com/macrotooth/app/service0/char1";  // HID Information
static const char* const CHAR2_PATH = "/com/macrotooth/app/service0/char2";  // HID Control Point
static const char* const CHAR3_PATH = "/com/macrotooth/app/service0/char3";  // Protocol Mode
static const char* const CHAR4_PATH = "/com/macrotooth/app/service0/char4";  // Input Report (notify)
static const char* const DESC0_PATH = "/com/macrotooth/app/service0/char4/desc0"; // Report Reference
static const char* const SVC1_PATH  = "/com/macrotooth/app/service1";        // DevInfo 0x180A
static const char* const CHAR5_PATH = "/com/macrotooth/app/service1/char0";  // Manufacturer Name
static const char* const CHAR6_PATH = "/com/macrotooth/app/service1/char1";  // PnP ID
static const char* const ADV_PATH   = "/com/macrotooth/advertisement0";

// ── Bluetooth UUIDs (full 128-bit form) ──────────────────────────────────────
static const char* const UUID_HID_SVC    = "00001812-0000-1000-8000-00805f9b34fb";
static const char* const UUID_DEVINFO    = "0000180a-0000-1000-8000-00805f9b34fb";
static const char* const UUID_REPORT_MAP = "00002a4b-0000-1000-8000-00805f9b34fb";
static const char* const UUID_HID_INFO   = "00002a4a-0000-1000-8000-00805f9b34fb";
static const char* const UUID_HID_CP     = "00002a4c-0000-1000-8000-00805f9b34fb";
static const char* const UUID_PROTO_MODE = "00002a4e-0000-1000-8000-00805f9b34fb";
static const char* const UUID_REPORT     = "00002a4d-0000-1000-8000-00805f9b34fb";
static const char* const UUID_REPORT_REF = "00002908-0000-1000-8000-00805f9b34fb";
static const char* const UUID_MFR_NAME   = "00002a29-0000-1000-8000-00805f9b34fb";
static const char* const UUID_PNP_ID     = "00002a50-0000-1000-8000-00805f9b34fb";

// BLE keyboard appearance value (Bluetooth Assigned Numbers)
static const uint16_t BLE_APPEARANCE_KEYBOARD = 0x03C1;

// bcdHID = 0x0111 → HID specification version 1.11 (stored little-endian)
static const uint8_t HID_BCD_LO = 0x11;
static const uint8_t HID_BCD_HI = 0x01;

// ── Static D-Bus variant / property helpers ───────────────────────────────────

static void appendStrVariant(DBusMessageIter* i, const char* v) {
    DBusMessageIter var;
    dbus_message_iter_open_container(i, DBUS_TYPE_VARIANT, "s", &var);
    dbus_message_iter_append_basic(&var, DBUS_TYPE_STRING, &v);
    dbus_message_iter_close_container(i, &var);
}

static void appendBoolVariant(DBusMessageIter* i, dbus_bool_t v) {
    DBusMessageIter var;
    dbus_message_iter_open_container(i, DBUS_TYPE_VARIANT, "b", &var);
    dbus_message_iter_append_basic(&var, DBUS_TYPE_BOOLEAN, &v);
    dbus_message_iter_close_container(i, &var);
}

static void appendObjVariant(DBusMessageIter* i, const char* v) {
    DBusMessageIter var;
    dbus_message_iter_open_container(i, DBUS_TYPE_VARIANT, "o", &var);
    dbus_message_iter_append_basic(&var, DBUS_TYPE_OBJECT_PATH, &v);
    dbus_message_iter_close_container(i, &var);
}

static void appendU16Variant(DBusMessageIter* i, uint16_t v) {
    DBusMessageIter var;
    dbus_message_iter_open_container(i, DBUS_TYPE_VARIANT, "q", &var);
    dbus_message_iter_append_basic(&var, DBUS_TYPE_UINT16, &v);
    dbus_message_iter_close_container(i, &var);
}

static void appendAyVariant(DBusMessageIter* i, const uint8_t* data, size_t len) {
    DBusMessageIter var, arr;
    dbus_message_iter_open_container(i, DBUS_TYPE_VARIANT, "ay", &var);
    dbus_message_iter_open_container(&var, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE_AS_STRING, &arr);
    for (size_t j = 0; j < len; j++)
        dbus_message_iter_append_basic(&arr, DBUS_TYPE_BYTE, &data[j]);
    dbus_message_iter_close_container(&var, &arr);
    dbus_message_iter_close_container(i, &var);
}

static void appendAsVariant(DBusMessageIter* i, const char** strs, size_t count) {
    DBusMessageIter var, arr;
    dbus_message_iter_open_container(i, DBUS_TYPE_VARIANT, "as", &var);
    dbus_message_iter_open_container(&var, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING_AS_STRING, &arr);
    for (size_t j = 0; j < count; j++)
        dbus_message_iter_append_basic(&arr, DBUS_TYPE_STRING, &strs[j]);
    dbus_message_iter_close_container(&var, &arr);
    dbus_message_iter_close_container(i, &var);
}

// Append a named {sv} entry into an open a{sv} iterator.
static void propStr(DBusMessageIter* props, const char* key, const char* val) {
    DBusMessageIter e;
    dbus_message_iter_open_container(props, DBUS_TYPE_DICT_ENTRY, nullptr, &e);
    dbus_message_iter_append_basic(&e, DBUS_TYPE_STRING, &key);
    appendStrVariant(&e, val);
    dbus_message_iter_close_container(props, &e);
}

static void propBool(DBusMessageIter* props, const char* key, dbus_bool_t val) {
    DBusMessageIter e;
    dbus_message_iter_open_container(props, DBUS_TYPE_DICT_ENTRY, nullptr, &e);
    dbus_message_iter_append_basic(&e, DBUS_TYPE_STRING, &key);
    appendBoolVariant(&e, val);
    dbus_message_iter_close_container(props, &e);
}

static void propObj(DBusMessageIter* props, const char* key, const char* val) {
    DBusMessageIter e;
    dbus_message_iter_open_container(props, DBUS_TYPE_DICT_ENTRY, nullptr, &e);
    dbus_message_iter_append_basic(&e, DBUS_TYPE_STRING, &key);
    appendObjVariant(&e, val);
    dbus_message_iter_close_container(props, &e);
}

static void propAy(DBusMessageIter* props, const char* key,
                   const uint8_t* data, size_t len) {
    DBusMessageIter e;
    dbus_message_iter_open_container(props, DBUS_TYPE_DICT_ENTRY, nullptr, &e);
    dbus_message_iter_append_basic(&e, DBUS_TYPE_STRING, &key);
    appendAyVariant(&e, data, len);
    dbus_message_iter_close_container(props, &e);
}

static void propAs(DBusMessageIter* props, const char* key,
                   const char** strs, size_t cnt) {
    DBusMessageIter e;
    dbus_message_iter_open_container(props, DBUS_TYPE_DICT_ENTRY, nullptr, &e);
    dbus_message_iter_append_basic(&e, DBUS_TYPE_STRING, &key);
    appendAsVariant(&e, strs, cnt);
    dbus_message_iter_close_container(props, &e);
}

static void propU16(DBusMessageIter* props, const char* key, uint16_t val) {
    DBusMessageIter e;
    dbus_message_iter_open_container(props, DBUS_TYPE_DICT_ENTRY, nullptr, &e);
    dbus_message_iter_append_basic(&e, DBUS_TYPE_STRING, &key);
    appendU16Variant(&e, val);
    dbus_message_iter_close_container(props, &e);
}

// ── GetManagedObjects builder helpers ─────────────────────────────────────────

// Open one object entry in a{oa{sa{sv}}}.
static void beginObj(DBusMessageIter* outer, DBusMessageIter* obj_e,
                     DBusMessageIter* iface_arr, const char* path) {
    dbus_message_iter_open_container(outer, DBUS_TYPE_DICT_ENTRY, nullptr, obj_e);
    dbus_message_iter_append_basic(obj_e, DBUS_TYPE_OBJECT_PATH, &path);
    dbus_message_iter_open_container(obj_e, DBUS_TYPE_ARRAY, "{sa{sv}}", iface_arr);
}

static void endObj(DBusMessageIter* outer, DBusMessageIter* obj_e,
                   DBusMessageIter* iface_arr) {
    dbus_message_iter_close_container(obj_e, iface_arr);
    dbus_message_iter_close_container(outer, obj_e);
}

// Open one interface entry in a{sa{sv}}.
static void beginIface(DBusMessageIter* iface_arr, DBusMessageIter* iface_e,
                       DBusMessageIter* props, const char* iface_name) {
    dbus_message_iter_open_container(iface_arr, DBUS_TYPE_DICT_ENTRY, nullptr, iface_e);
    dbus_message_iter_append_basic(iface_e, DBUS_TYPE_STRING, &iface_name);
    dbus_message_iter_open_container(iface_e, DBUS_TYPE_ARRAY, "{sv}", props);
}

static void endIface(DBusMessageIter* iface_arr, DBusMessageIter* iface_e,
                     DBusMessageIter* props) {
    dbus_message_iter_close_container(iface_e, props);
    dbus_message_iter_close_container(iface_arr, iface_e);
}

// ── GattApplication ───────────────────────────────────────────────────────────

GattApplication::GattApplication()
    : conn_(nullptr)
    , protocol_mode_(0x01)   // Report Protocol Mode
    , notify_enabled_(false)
    , registered_(false)
    , adv_registered_(false) {
    input_report_.assign(8, 0);  // 8-byte keyboard report, all zeros
    // Default PnP ID: Logitech K380 (VID src=BT SIG, VID=0x046D, PID=0xB342, ver=0x0001)
    pnp_id_ = {0x01, 0x6D, 0x04, 0x42, 0xB3, 0x01, 0x00};
}

GattApplication::~GattApplication() {
    stop();
    if (conn_) {
        dbus_connection_unref(conn_);
        conn_ = nullptr;
    }
}

void GattApplication::setPnpId(uint8_t vid_source, uint16_t vid,
                                 uint16_t pid, uint16_t version) {
    // PnP ID format (7 bytes, all values little-endian):
    //   byte 0   : Vendor ID Source
    //   bytes 1-2: Vendor ID
    //   bytes 3-4: Product ID
    //   bytes 5-6: Product Version
    pnp_id_ = {
        vid_source,
        static_cast<uint8_t>(vid & 0xFF),
        static_cast<uint8_t>((vid >> 8) & 0xFF),
        static_cast<uint8_t>(pid & 0xFF),
        static_cast<uint8_t>((pid >> 8) & 0xFF),
        static_cast<uint8_t>(version & 0xFF),
        static_cast<uint8_t>((version >> 8) & 0xFF)
    };
}

// ── Public API ────────────────────────────────────────────────────────────────

bool GattApplication::start(const uint8_t* report_desc, size_t len,
                             const std::string& device_name) {
    device_name_ = device_name;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        report_desc_.assign(report_desc, report_desc + len);
    }
    if (!connectDBus())          return false;
    if (!registerGattApplication()) return false;
    if (!registerAdvertisement())   return false;
    std::cout << "BLE HID GATT application started successfully" << std::endl;
    return true;
}

void GattApplication::stop() {
    unregisterAll();
}

bool GattApplication::sendInputReport(const uint8_t* data, size_t len) {
    if (!registered_) return false;

    bool notify = false;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        size_t copy_len = std::min(len, input_report_.size());
        std::copy(data, data + copy_len, input_report_.begin());
        notify = notify_enabled_;
    }

    if (!notify) return true;  // stored but host not yet subscribed

    // Emit PropertiesChanged on the Input Report characteristic so BlueZ
    // forwards it as a BLE notification to the connected host.
    DBusMessage* sig = dbus_message_new_signal(
        CHAR4_PATH,
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged");
    if (!sig) return false;

    DBusMessageIter iter;
    dbus_message_iter_init_append(sig, &iter);

    // arg1: interface name
    const char* iface_name = "org.bluez.GattCharacteristic1";
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &iface_name);

    // arg2: changed properties {sv} with "Value" → ay
    DBusMessageIter changed;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &changed);
    {
        const char* key = "Value";
        DBusMessageIter entry;
        dbus_message_iter_open_container(&changed, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
        dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
        appendAyVariant(&entry, data, len);
        dbus_message_iter_close_container(&changed, &entry);
    }
    dbus_message_iter_close_container(&iter, &changed);

    // arg3: invalidated properties (empty string array)
    DBusMessageIter inv;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
                                     DBUS_TYPE_STRING_AS_STRING, &inv);
    dbus_message_iter_close_container(&iter, &inv);

    dbus_connection_send(conn_, sig, nullptr);
    dbus_connection_flush(conn_);
    dbus_message_unref(sig);
    return true;
}

void GattApplication::dispatch(int timeout_ms) {
    if (conn_)
        dbus_connection_read_write_dispatch(conn_, timeout_ms);
}

// ── Private: setup ────────────────────────────────────────────────────────────

bool GattApplication::connectDBus() {
    DBusError err;
    dbus_error_init(&err);

    conn_ = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (dbus_error_is_set(&err)) {
        std::cerr << "D-Bus connection error: " << err.message << std::endl;
        dbus_error_free(&err);
        return false;
    }

    // Register a fallback handler for every path under /com/macrotooth so
    // that messages destined for services, characteristics and descriptors
    // are all routed to our single handler.
    static const DBusObjectPathVTable vtable = {
        nullptr,            // unregister_function
        staticHandler,      // message_function
        nullptr, nullptr, nullptr, nullptr
    };

    if (!dbus_connection_register_fallback(conn_, "/com/macrotooth",
                                           &vtable, this)) {
        std::cerr << "Failed to register D-Bus fallback path" << std::endl;
        return false;
    }

    return true;
}

bool GattApplication::registerGattApplication() {
    // org.bluez.GattManager1.RegisterApplication on /org/bluez/hci0
    DBusMessage* msg = dbus_message_new_method_call(
        "org.bluez",
        "/org/bluez/hci0",
        "org.bluez.GattManager1",
        "RegisterApplication");
    if (!msg) {
        std::cerr << "Failed to create RegisterApplication message" << std::endl;
        return false;
    }

    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);

    // arg1: application object path
    const char* app = APP_PATH;
    dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &app);

    // arg2: options dict (empty)
    DBusMessageIter opts;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &opts);
    dbus_message_iter_close_container(&args, &opts);

    DBusError err;
    dbus_error_init(&err);
    // Blocking call – BlueZ calls GetManagedObjects on APP_PATH before
    // returning the reply; our fallback handler processes it inline.
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(
        conn_, msg, -1, &err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&err)) {
        std::cerr << "RegisterApplication error: " << err.message << std::endl;
        dbus_error_free(&err);
        return false;
    }
    if (reply) dbus_message_unref(reply);

    registered_ = true;
    std::cout << "GATT application registered on /org/bluez/hci0" << std::endl;
    return true;
}

bool GattApplication::registerAdvertisement() {
    // org.bluez.LEAdvertisingManager1.RegisterAdvertisement on /org/bluez/hci0
    DBusMessage* msg = dbus_message_new_method_call(
        "org.bluez",
        "/org/bluez/hci0",
        "org.bluez.LEAdvertisingManager1",
        "RegisterAdvertisement");
    if (!msg) {
        std::cerr << "Failed to create RegisterAdvertisement message" << std::endl;
        return false;
    }

    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);

    // arg1: advertisement object path
    const char* adv = ADV_PATH;
    dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &adv);

    // arg2: options dict (empty)
    DBusMessageIter opts;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &opts);
    dbus_message_iter_close_container(&args, &opts);

    DBusError err;
    dbus_error_init(&err);
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(
        conn_, msg, -1, &err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&err)) {
        std::cerr << "RegisterAdvertisement error: " << err.message << std::endl;
        dbus_error_free(&err);
        return false;
    }
    if (reply) dbus_message_unref(reply);

    adv_registered_ = true;
    std::cout << "LE advertisement registered" << std::endl;
    return true;
}

void GattApplication::unregisterAll() {
    if (!conn_) return;

    if (adv_registered_) {
        DBusMessage* msg = dbus_message_new_method_call(
            "org.bluez", "/org/bluez/hci0",
            "org.bluez.LEAdvertisingManager1", "UnregisterAdvertisement");
        if (msg) {
            dbus_message_append_args(msg,
                DBUS_TYPE_OBJECT_PATH, &ADV_PATH, DBUS_TYPE_INVALID);
            DBusError err;
            dbus_error_init(&err);
            DBusMessage* r = dbus_connection_send_with_reply_and_block(
                conn_, msg, 1000, &err);
            dbus_message_unref(msg);
            if (r) dbus_message_unref(r);
            if (dbus_error_is_set(&err)) dbus_error_free(&err);
        }
        adv_registered_ = false;
    }

    if (registered_) {
        DBusMessage* msg = dbus_message_new_method_call(
            "org.bluez", "/org/bluez/hci0",
            "org.bluez.GattManager1", "UnregisterApplication");
        if (msg) {
            const char* app = APP_PATH;
            dbus_message_append_args(msg,
                DBUS_TYPE_OBJECT_PATH, &app, DBUS_TYPE_INVALID);
            DBusError err;
            dbus_error_init(&err);
            DBusMessage* r = dbus_connection_send_with_reply_and_block(
                conn_, msg, 1000, &err);
            dbus_message_unref(msg);
            if (r) dbus_message_unref(r);
            if (dbus_error_is_set(&err)) dbus_error_free(&err);
        }
        registered_ = false;
    }

    dbus_connection_unregister_object_path(conn_, "/com/macrotooth");
}

// ── Private: D-Bus message dispatch ──────────────────────────────────────────

DBusHandlerResult GattApplication::staticHandler(DBusConnection* conn,
                                                  DBusMessage* msg,
                                                  void* user_data) {
    return static_cast<GattApplication*>(user_data)->onMessage(conn, msg);
}

DBusHandlerResult GattApplication::onMessage(DBusConnection* conn,
                                              DBusMessage* msg) {
    const char* path   = dbus_message_get_path(msg);
    const char* iface  = dbus_message_get_interface(msg);
    const char* method = dbus_message_get_member(msg);

    if (!path || !iface || !method)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    DBusMessage* reply = nullptr;

    // ObjectManager.GetManagedObjects – called by BlueZ during
    // RegisterApplication so it can discover all GATT objects.
    if (strcmp(path, APP_PATH) == 0 &&
        strcmp(iface, "org.freedesktop.DBus.ObjectManager") == 0 &&
        strcmp(method, "GetManagedObjects") == 0) {
        reply = handleGetManagedObjects(msg);
    }

    // GattCharacteristic1 methods
    else if (strcmp(iface, "org.bluez.GattCharacteristic1") == 0) {
        if      (strcmp(method, "ReadValue")    == 0)
            reply = handleCharacteristicRead(msg, path);
        else if (strcmp(method, "WriteValue")   == 0)
            reply = handleCharacteristicWrite(msg, path);
        else if (strcmp(method, "StartNotify")  == 0)
            reply = handleStartNotify(msg);
        else if (strcmp(method, "StopNotify")   == 0)
            reply = handleStopNotify(msg);
    }

    // GattDescriptor1 methods
    else if (strcmp(iface, "org.bluez.GattDescriptor1") == 0) {
        if      (strcmp(method, "ReadValue")    == 0)
            reply = handleDescriptorRead(msg, path);
        else if (strcmp(method, "WriteValue")   == 0)
            reply = dbus_message_new_method_return(msg);  // no-op
    }

    // LEAdvertisement1.Release – BlueZ released the advertisement.
    else if (strcmp(path, ADV_PATH) == 0 &&
             strcmp(iface, "org.bluez.LEAdvertisement1") == 0 &&
             strcmp(method, "Release") == 0) {
        handleAdvertisementRelease();
        reply = dbus_message_new_method_return(msg);
    }

    // Properties.GetAll – BlueZ may query advertisement or char properties.
    else if (strcmp(iface, "org.freedesktop.DBus.Properties") == 0 &&
             strcmp(method, "GetAll") == 0) {
        reply = handleGetAllProperties(msg, path);
    }

    // Introspectable – benign; return a minimal XML so introspection works.
    else if (strcmp(iface, "org.freedesktop.DBus.Introspectable") == 0 &&
             strcmp(method, "Introspect") == 0) {
        reply = dbus_message_new_method_return(msg);
        if (reply) {
            const char* xml = "<node/>";
            dbus_message_append_args(reply,
                DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID);
        }
    }

    if (reply) {
        dbus_connection_send(conn, reply, nullptr);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

// ── GetManagedObjects ─────────────────────────────────────────────────────────
//
// Returns a{oa{sa{sv}}} describing every GATT service, characteristic and
// descriptor that BlueZ should expose over BLE.

DBusMessage* GattApplication::handleGetManagedObjects(DBusMessage* msg) {
    DBusMessage* reply = dbus_message_new_method_return(msg);
    if (!reply) return nullptr;
    appendManagedObjects(reply);
    return reply;
}

void GattApplication::appendManagedObjects(DBusMessage* reply) {
    // Take a snapshot of state under lock so we don't hold it while
    // building the D-Bus message.
    std::vector<uint8_t> report_desc_snap;
    std::vector<uint8_t> input_report_snap;
    uint8_t proto_mode_snap;
    bool    notifying_snap;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        report_desc_snap  = report_desc_;
        input_report_snap = input_report_;
        proto_mode_snap   = protocol_mode_;
        notifying_snap    = notify_enabled_;
    }

    // bcdHID=0x0111 (HID 1.11), bCountryCode=0x00, Flags=0x02 (Normally Connectable)
    static const uint8_t HID_INFO[]    = {HID_BCD_LO, HID_BCD_HI, 0x00, 0x02};
    static const uint8_t HID_CP_VAL[]  = {0x00};
    static const uint8_t REPORT_REF[]  = {0x00, 0x01};  // Report ID=0, Type=Input(1)
    static const char    MFR_NAME[]    = "Logitech";

    DBusMessageIter iter;
    dbus_message_iter_init_append(reply, &iter);

    // Open outer dict  a{oa{sa{sv}}}
    DBusMessageIter outer;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{oa{sa{sv}}}", &outer);

    // ── service0: HID Service (0x1812, primary) ───────────────────────────
    {
        DBusMessageIter obj_e, iface_arr, iface_e, props;
        beginObj(&outer, &obj_e, &iface_arr, SVC0_PATH);
        beginIface(&iface_arr, &iface_e, &props, "org.bluez.GattService1");
        propStr (&props, "UUID",    UUID_HID_SVC);
        propBool(&props, "Primary", TRUE);
        endIface(&iface_arr, &iface_e, &props);
        endObj  (&outer, &obj_e, &iface_arr);
    }

    // ── char0: Report Map (0x2A4B, read) ─────────────────────────────────
    {
        const char* flags[] = {"read"};
        DBusMessageIter obj_e, iface_arr, iface_e, props;
        beginObj(&outer, &obj_e, &iface_arr, CHAR0_PATH);
        beginIface(&iface_arr, &iface_e, &props, "org.bluez.GattCharacteristic1");
        propStr(&props, "UUID",    UUID_REPORT_MAP);
        propObj(&props, "Service", SVC0_PATH);
        propAs (&props, "Flags",   flags, 1);
        propAy (&props, "Value",   report_desc_snap.data(), report_desc_snap.size());
        endIface(&iface_arr, &iface_e, &props);
        endObj  (&outer, &obj_e, &iface_arr);
    }

    // ── char1: HID Information (0x2A4A, read) ────────────────────────────
    {
        const char* flags[] = {"read"};
        DBusMessageIter obj_e, iface_arr, iface_e, props;
        beginObj(&outer, &obj_e, &iface_arr, CHAR1_PATH);
        beginIface(&iface_arr, &iface_e, &props, "org.bluez.GattCharacteristic1");
        propStr(&props, "UUID",    UUID_HID_INFO);
        propObj(&props, "Service", SVC0_PATH);
        propAs (&props, "Flags",   flags, 1);
        propAy (&props, "Value",   HID_INFO, sizeof(HID_INFO));
        endIface(&iface_arr, &iface_e, &props);
        endObj  (&outer, &obj_e, &iface_arr);
    }

    // ── char2: HID Control Point (0x2A4C, write-without-response) ────────
    {
        const char* flags[] = {"write-without-response"};
        DBusMessageIter obj_e, iface_arr, iface_e, props;
        beginObj(&outer, &obj_e, &iface_arr, CHAR2_PATH);
        beginIface(&iface_arr, &iface_e, &props, "org.bluez.GattCharacteristic1");
        propStr(&props, "UUID",    UUID_HID_CP);
        propObj(&props, "Service", SVC0_PATH);
        propAs (&props, "Flags",   flags, 1);
        propAy (&props, "Value",   HID_CP_VAL, sizeof(HID_CP_VAL));
        endIface(&iface_arr, &iface_e, &props);
        endObj  (&outer, &obj_e, &iface_arr);
    }

    // ── char3: Protocol Mode (0x2A4E, read + write-without-response) ─────
    {
        const char* flags[] = {"read", "write-without-response"};
        DBusMessageIter obj_e, iface_arr, iface_e, props;
        beginObj(&outer, &obj_e, &iface_arr, CHAR3_PATH);
        beginIface(&iface_arr, &iface_e, &props, "org.bluez.GattCharacteristic1");
        propStr(&props, "UUID",    UUID_PROTO_MODE);
        propObj(&props, "Service", SVC0_PATH);
        propAs (&props, "Flags",   flags, 2);
        propAy (&props, "Value",   &proto_mode_snap, 1);
        endIface(&iface_arr, &iface_e, &props);
        endObj  (&outer, &obj_e, &iface_arr);
    }

    // ── char4: Input Report (0x2A4D, read + notify) ───────────────────────
    {
        const char* flags[] = {"read", "notify"};
        dbus_bool_t notifying = notifying_snap ? TRUE : FALSE;
        DBusMessageIter obj_e, iface_arr, iface_e, props;
        beginObj(&outer, &obj_e, &iface_arr, CHAR4_PATH);
        beginIface(&iface_arr, &iface_e, &props, "org.bluez.GattCharacteristic1");
        propStr (&props, "UUID",      UUID_REPORT);
        propObj (&props, "Service",   SVC0_PATH);
        propAs  (&props, "Flags",     flags, 2);
        propAy  (&props, "Value",     input_report_snap.data(), input_report_snap.size());
        propBool(&props, "Notifying", notifying);
        endIface(&iface_arr, &iface_e, &props);
        endObj  (&outer, &obj_e, &iface_arr);
    }

    // ── desc0: Report Reference descriptor (0x2908, read) ────────────────
    {
        const char* flags[] = {"read"};
        DBusMessageIter obj_e, iface_arr, iface_e, props;
        beginObj(&outer, &obj_e, &iface_arr, DESC0_PATH);
        beginIface(&iface_arr, &iface_e, &props, "org.bluez.GattDescriptor1");
        propStr(&props, "UUID",           UUID_REPORT_REF);
        propObj(&props, "Characteristic", CHAR4_PATH);
        propAs (&props, "Flags",          flags, 1);
        propAy (&props, "Value",          REPORT_REF, sizeof(REPORT_REF));
        endIface(&iface_arr, &iface_e, &props);
        endObj  (&outer, &obj_e, &iface_arr);
    }

    // ── service1: Device Information Service (0x180A, primary) ───────────
    {
        DBusMessageIter obj_e, iface_arr, iface_e, props;
        beginObj(&outer, &obj_e, &iface_arr, SVC1_PATH);
        beginIface(&iface_arr, &iface_e, &props, "org.bluez.GattService1");
        propStr (&props, "UUID",    UUID_DEVINFO);
        propBool(&props, "Primary", TRUE);
        endIface(&iface_arr, &iface_e, &props);
        endObj  (&outer, &obj_e, &iface_arr);
    }

    // ── char5: Manufacturer Name String (0x2A29, read) ────────────────────
    {
        const char* flags[] = {"read"};
        DBusMessageIter obj_e, iface_arr, iface_e, props;
        beginObj(&outer, &obj_e, &iface_arr, CHAR5_PATH);
        beginIface(&iface_arr, &iface_e, &props, "org.bluez.GattCharacteristic1");
        propStr(&props, "UUID",    UUID_MFR_NAME);
        propObj(&props, "Service", SVC1_PATH);
        propAs (&props, "Flags",   flags, 1);
        propAy (&props, "Value",   reinterpret_cast<const uint8_t*>(MFR_NAME),
                strlen(MFR_NAME));
        endIface(&iface_arr, &iface_e, &props);
        endObj  (&outer, &obj_e, &iface_arr);
    }

    // ── char6: PnP ID (0x2A50, read) ─────────────────────────────────────
    {
        const char* flags[] = {"read"};
        DBusMessageIter obj_e, iface_arr, iface_e, props;
        beginObj(&outer, &obj_e, &iface_arr, CHAR6_PATH);
        beginIface(&iface_arr, &iface_e, &props, "org.bluez.GattCharacteristic1");
        propStr(&props, "UUID",    UUID_PNP_ID);
        propObj(&props, "Service", SVC1_PATH);
        propAs (&props, "Flags",   flags, 1);
        propAy (&props, "Value",   pnp_id_.data(), pnp_id_.size());
        endIface(&iface_arr, &iface_e, &props);
        endObj  (&outer, &obj_e, &iface_arr);
    }

    dbus_message_iter_close_container(&iter, &outer);
}

// ── Characteristic / descriptor handlers ─────────────────────────────────────

DBusMessage* GattApplication::handleCharacteristicRead(DBusMessage* msg,
                                                        const char* path) {
    DBusMessage* reply = dbus_message_new_method_return(msg);
    if (!reply) return nullptr;

    // bcdHID=HID 1.11, bCountryCode=0x00, Flags=0x02 (Normally Connectable)
    static const uint8_t HID_INFO[]   = {HID_BCD_LO, HID_BCD_HI, 0x00, 0x02};
    static const uint8_t HID_CP_VAL[] = {0x00};
    static const char    MFR_NAME[]   = "Logitech";

    std::vector<uint8_t> value;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (strcmp(path, CHAR0_PATH) == 0) {
            value = report_desc_;
        } else if (strcmp(path, CHAR1_PATH) == 0) {
            value.assign(HID_INFO, HID_INFO + sizeof(HID_INFO));
        } else if (strcmp(path, CHAR2_PATH) == 0) {
            value.assign(HID_CP_VAL, HID_CP_VAL + sizeof(HID_CP_VAL));
        } else if (strcmp(path, CHAR3_PATH) == 0) {
            value = {protocol_mode_};
        } else if (strcmp(path, CHAR4_PATH) == 0) {
            value = input_report_;
        } else if (strcmp(path, CHAR5_PATH) == 0) {
            value.assign(reinterpret_cast<const uint8_t*>(MFR_NAME),
                         reinterpret_cast<const uint8_t*>(MFR_NAME) + strlen(MFR_NAME));
        } else if (strcmp(path, CHAR6_PATH) == 0) {
            value = pnp_id_;
        }
    }

    DBusMessageIter iter, arr;
    dbus_message_iter_init_append(reply, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
                                     DBUS_TYPE_BYTE_AS_STRING, &arr);
    for (uint8_t b : value)
        dbus_message_iter_append_basic(&arr, DBUS_TYPE_BYTE, &b);
    dbus_message_iter_close_container(&iter, &arr);

    return reply;
}

DBusMessage* GattApplication::handleCharacteristicWrite(DBusMessage* msg,
                                                         const char* path) {
    DBusMessageIter iter;
    if (dbus_message_iter_init(msg, &iter) &&
        dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY) {

        DBusMessageIter arr;
        dbus_message_iter_recurse(&iter, &arr);

        std::vector<uint8_t> data;
        while (dbus_message_iter_get_arg_type(&arr) == DBUS_TYPE_BYTE) {
            uint8_t b;
            dbus_message_iter_get_basic(&arr, &b);
            data.push_back(b);
            dbus_message_iter_next(&arr);
        }

        if (!data.empty()) {
            std::lock_guard<std::mutex> lk(mutex_);
            if (strcmp(path, CHAR3_PATH) == 0) {
                protocol_mode_ = data[0];
                std::cout << "Protocol mode: " << (int)protocol_mode_ << std::endl;
            } else if (strcmp(path, CHAR2_PATH) == 0) {
                // 0x00 = Suspend, 0x01 = Exit Suspend
                std::cout << "HID Control Point: " << (int)data[0] << std::endl;
            }
        }

        // Output / Feature report from host
        if (strcmp(path, CHAR4_PATH) == 0 && output_cb_ && !data.empty())
            output_cb_(data.data(), data.size());
    }

    return dbus_message_new_method_return(msg);
}

DBusMessage* GattApplication::handleStartNotify(DBusMessage* msg) {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        notify_enabled_ = true;
    }
    std::cout << "HID report notifications enabled" << std::endl;
    if (conn_cb_) conn_cb_(true);
    return dbus_message_new_method_return(msg);
}

DBusMessage* GattApplication::handleStopNotify(DBusMessage* msg) {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        notify_enabled_ = false;
    }
    std::cout << "HID report notifications disabled" << std::endl;
    if (conn_cb_) conn_cb_(false);
    return dbus_message_new_method_return(msg);
}

DBusMessage* GattApplication::handleDescriptorRead(DBusMessage* msg,
                                                    const char* path) {
    DBusMessage* reply = dbus_message_new_method_return(msg);
    if (!reply) return nullptr;

    // Report Reference: Report ID=0, Report Type=Input(1)
    static const uint8_t REPORT_REF[] = {0x00, 0x01};

    const uint8_t* data = nullptr;
    size_t          len = 0;
    if (strcmp(path, DESC0_PATH) == 0) {
        data = REPORT_REF;
        len  = sizeof(REPORT_REF);
    }

    DBusMessageIter iter, arr;
    dbus_message_iter_init_append(reply, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
                                     DBUS_TYPE_BYTE_AS_STRING, &arr);
    for (size_t i = 0; i < len; i++)
        dbus_message_iter_append_basic(&arr, DBUS_TYPE_BYTE, &data[i]);
    dbus_message_iter_close_container(&iter, &arr);

    return reply;
}

// ── Properties.GetAll ─────────────────────────────────────────────────────────
//
// BlueZ queries the advertisement object's properties via this interface.

DBusMessage* GattApplication::handleGetAllProperties(DBusMessage* msg,
                                                      const char* path) {
    DBusMessage* reply = dbus_message_new_method_return(msg);
    if (!reply) return nullptr;

    DBusMessageIter iter, props;
    dbus_message_iter_init_append(reply, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &props);

    if (strcmp(path, ADV_PATH) == 0) {
        // LEAdvertisement1 properties
        const char* type       = "peripheral";
        const char* svc_uuids[] = {UUID_HID_SVC};
        const char* name       = device_name_.c_str();
        propStr (&props, "Type",          type);
        propAs  (&props, "ServiceUUIDs",  svc_uuids, 1);
        propStr (&props, "LocalName",     name);
        propU16 (&props, "Appearance",    BLE_APPEARANCE_KEYBOARD);
        propBool(&props, "IncludeTxPower", TRUE);
    } else if (strcmp(path, SVC0_PATH) == 0) {
        propStr (&props, "UUID",    UUID_HID_SVC);
        propBool(&props, "Primary", TRUE);
    } else if (strcmp(path, SVC1_PATH) == 0) {
        propStr (&props, "UUID",    UUID_DEVINFO);
        propBool(&props, "Primary", TRUE);
    } else if (strcmp(path, CHAR4_PATH) == 0) {
        const char* flags[] = {"read", "notify"};
        std::vector<uint8_t> val;
        bool notifying;
        {
            std::lock_guard<std::mutex> lk(mutex_);
            val      = input_report_;
            notifying = notify_enabled_;
        }
        propStr (&props, "UUID",      UUID_REPORT);
        propObj (&props, "Service",   SVC0_PATH);
        propAs  (&props, "Flags",     flags, 2);
        propAy  (&props, "Value",     val.data(), val.size());
        propBool(&props, "Notifying", notifying ? TRUE : FALSE);
    }
    // Other paths return an empty dict – sufficient for BlueZ.

    dbus_message_iter_close_container(&iter, &props);
    return reply;
}

void GattApplication::handleAdvertisementRelease() {
    std::cout << "LE advertisement released by BlueZ" << std::endl;
    adv_registered_ = false;
}
