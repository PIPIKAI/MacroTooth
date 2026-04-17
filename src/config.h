#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>
#include <string>

// Central configuration for MacroTooth.
// All values can be changed here before compiling, or extended to read from
// a config file (e.g. /etc/macrotooth.conf) in a future iteration.
struct Config {
    // ── Device identity ────────────────────────────────────────────────────
    // Name shown to the host during pairing and in the BLE advertisement.
    std::string device_name    = "Logitech K380 Multi-Device Keyboard";

    // Bluetooth adapter HCI index (0 → /org/bluez/hci0).
    int         hci_index      = 0;

    // Vendor / Product ID for the PnP ID GATT characteristic.
    // VID source 0x01 = Bluetooth SIG, VID 0x046D = Logitech, PID 0xB342 = K380.
    uint8_t     pnp_vid_src    = 0x01;
    uint16_t    pnp_vid        = 0x046D;
    uint16_t    pnp_pid        = 0xB342;
    uint16_t    pnp_version    = 0x0001;

    // Bluetooth Class of Device.
    // 0x002540 = Major=Peripheral, Minor=Keyboard.
    uint32_t    class_of_device = 0x002540;

    // ── Bluetooth behaviour ────────────────────────────────────────────────
    bool     auto_power_on          = true;
    bool     auto_discoverable      = true;
    // 0 = always discoverable; positive value = timeout in seconds.
    uint32_t discoverable_timeout_s = 180;

    // ── IPC ────────────────────────────────────────────────────────────────
    std::string ipc_socket_path = "/tmp/macrotooth.sock";
};

#endif // CONFIG_H
