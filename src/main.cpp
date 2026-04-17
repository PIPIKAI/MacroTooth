#include "config.h"
#include "logger.h"
#include "bt_adapter.h"
#include "bt_agent.h"
#include "bluez_profile.h"
#include "gatt_application.h"
#include "hid_reporter.h"
#include "ipc_server.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <dbus/dbus.h>

// ── HID Report Descriptor ─────────────────────────────────────────────────────
// Standard 8-byte keyboard report (no Report ID, matches a vanilla USB HID
// keyboard descriptor accepted by all major operating systems).
static const uint8_t hid_report_desc[] = {
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
    0x81, 0x02,        //   Input (Data,Var,Abs)  – 8 modifier bits
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x01,        //   Input (Const)         – reserved byte
    0x95, 0x06,        //   Report Count (6)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x05, 0x07,        //   Usage Page (Keyboard)
    0x19, 0x00,        //   Usage Minimum (0)
    0x29, 0x65,        //   Usage Maximum (101)
    0x81, 0x00,        //   Input (Data,Array)    – 6 key-code bytes
    0xC0               // End Collection
};

// ── Signal handling ───────────────────────────────────────────────────────────

static volatile bool g_running = true;

static void signalHandler(int /*signum*/) {
    g_running = false;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

// Dispatch D-Bus messages for `total` milliseconds while remaining
// interruptible by the global running flag.
static void dispatchFor(GattApplication& gatt, int total_ms) {
    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::milliseconds(total_ms);
    while (g_running && std::chrono::steady_clock::now() < deadline)
        gatt.dispatch(10);
}

// ── main ──────────────────────────────────────────────────────────────────────

int main() {
    signal(SIGINT,  signalHandler);
    signal(SIGTERM, signalHandler);

    // Initialise D-Bus thread support (required when multiple threads use the
    // shared system bus connection).
    dbus_threads_init_default();

    logger_init("macrotooth");
    LOG_I("MacroTooth starting");

    // ── Load configuration ─────────────────────────────────────────────────
    Config cfg;
    // cfg.device_name, cfg.ipc_socket_path, etc. can be overridden here or
    // loaded from a file before the components are initialised.

    // ── Bluetooth adapter setup ────────────────────────────────────────────
    BluetoothAdapter adapter;
    if (!adapter.init(cfg.hci_index)) {
        LOG_E("Failed to initialise Bluetooth adapter");
        return 1;
    }

    if (cfg.auto_power_on) {
        adapter.setPowered(true);
    }

    adapter.setAlias(cfg.device_name);
    adapter.setClassOfDevice(cfg.class_of_device);

    if (cfg.auto_discoverable) {
        adapter.setDiscoverable(true, cfg.discoverable_timeout_s);
    }

    adapter.setConnectable(true);

    // ── Pairing agent ──────────────────────────────────────────────────────
    BluetoothAgent agent;
    if (!agent.start()) {
        LOG_W("Failed to register pairing agent – manual confirmation may be required");
    }

    // ── HID report builder ─────────────────────────────────────────────────
    HIDReporter hid;

    // ── BLE GATT application (HID over GATT) ──────────────────────────────
    GattApplication gatt;

    gatt.setPnpId(cfg.pnp_vid_src, cfg.pnp_vid, cfg.pnp_pid, cfg.pnp_version);

    if (!gatt.start(hid_report_desc, sizeof(hid_report_desc), cfg.device_name)) {
        LOG_E("Failed to start BLE GATT application");
        return 1;
    }

    gatt.setConnectionCallback([](bool connected) {
        LOG_I("BLE host %s", connected ? "connected" : "disconnected");
    });

    gatt.setOutputReportCallback([](const uint8_t* data, size_t len) {
        // Output report from host, e.g. keyboard LED state (byte 0 = LED bits).
        LOG_D("Output report from BLE host, len=%zu, LED=0x%02X",
              len, len > 0 ? data[0] : 0);
    });

    // Wire HIDReporter's send callback to both BLE GATT and Classic BT.
    // Classic BT profile is set up below; capture by pointer so the lambda
    // can see it after profile_ is constructed.
    BlueZProfile profile;

    hid.setSendCallback([&gatt, &profile](const uint8_t* data, size_t len) -> bool {
        bool ok = gatt.sendInputReport(data, len);
        if (profile.isConnected())
            profile.sendInputReport(data, len);
        return ok;
    });

    // ── Classic BT HID profile (BR/EDR) ───────────────────────────────────
    profile.setConnectionCallback([](bool connected) {
        LOG_I("Classic BT host %s", connected ? "connected" : "disconnected");
    });

    if (!profile.registerHIDProfile()) {
        LOG_W("Failed to register Classic BT HID profile – BLE-only mode");
    }

    // ── IPC server ─────────────────────────────────────────────────────────
    IpcServer ipc;

    ipc.setKeyPressCallback([&hid, &gatt, &profile](uint8_t key, uint8_t mod) -> bool {
        hid.keyPress(static_cast<HIDReporter::KeyCode>(key),
                     static_cast<HIDReporter::Modifier>(mod));
        bool ok = gatt.sendInputReport(hid.getKeyboardReport(),
                                       hid.getKeyboardReportSize());
        if (profile.isConnected())
            profile.sendInputReport(hid.getKeyboardReport(),
                                    hid.getKeyboardReportSize());
        return ok;
    });

    ipc.setKeyReleaseCallback([&hid, &gatt, &profile]() -> bool {
        hid.keyRelease();
        bool ok = gatt.sendInputReport(hid.getKeyboardReport(),
                                       hid.getKeyboardReportSize());
        if (profile.isConnected())
            profile.sendInputReport(hid.getKeyboardReport(),
                                    hid.getKeyboardReportSize());
        return ok;
    });

    ipc.setTypeCallback([&hid](const std::string& text) -> bool {
        // typeString() calls the HIDReporter send callback after each key
        // press and release, so reports are dispatched automatically.
        hid.typeString(text);
        return true;
    });

    ipc.setStatusCallback([&gatt, &profile]() -> std::string {
        bool ble_conn     = gatt.isConnected();
        bool classic_conn = profile.isConnected();
        char buf[64];
        snprintf(buf, sizeof(buf),
                 "ble_connected=%d classic_connected=%d",
                 ble_conn ? 1 : 0, classic_conn ? 1 : 0);
        return buf;
    });

    if (!ipc.start(cfg.ipc_socket_path)) {
        LOG_E("Failed to start IPC server on %s", cfg.ipc_socket_path.c_str());
        return 1;
    }

    LOG_I("MacroTooth ready – IPC socket: %s", cfg.ipc_socket_path.c_str());
    std::cout << "MacroTooth ready." << std::endl
              << "  Device name  : " << cfg.device_name      << std::endl
              << "  IPC socket   : " << cfg.ipc_socket_path  << std::endl
              << "  Press Ctrl+C to quit." << std::endl;

    // ── Main D-Bus dispatch loop ───────────────────────────────────────────
    while (g_running) {
        gatt.dispatch(10);
    }

    // ── Cleanup ────────────────────────────────────────────────────────────
    LOG_I("MacroTooth shutting down");
    ipc.stop();
    gatt.stop();
    profile.unregisterProfile();
    agent.stop();

    logger_close();
    return 0;
}
