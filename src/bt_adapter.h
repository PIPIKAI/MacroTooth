#ifndef BT_ADAPTER_H
#define BT_ADAPTER_H

#include <dbus/dbus.h>
#include <cstdint>
#include <string>

// Manages the local Bluetooth adapter via org.bluez.Adapter1 D-Bus properties.
// Also sets the Class of Device via hciconfig (CoD is read-only in BlueZ D-Bus).
class BluetoothAdapter {
public:
    BluetoothAdapter();
    ~BluetoothAdapter();

    // Connect to D-Bus and validate that the adapter path exists.
    // hci_index: 0 → /org/bluez/hci0, 1 → /org/bluez/hci1, …
    bool init(int hci_index = 0);

    // Power the adapter on or off.
    bool setPowered(bool on);

    // Set the human-readable name shown to remote devices.
    bool setAlias(const std::string& name);

    // Make the adapter discoverable (visible to scanning devices).
    // When timeout_s > 0 BlueZ automatically turns off discoverability after
    // that many seconds; 0 means "always discoverable".
    bool setDiscoverable(bool on, uint32_t timeout_s = 180);

    // Allow the adapter to accept incoming connections.
    bool setConnectable(bool on);

    // Set the Bluetooth Class of Device via hciconfig.
    // cod: e.g. 0x002540 (keyboard), 0x0025C0 (keyboard+mouse combo).
    bool setClassOfDevice(uint32_t cod);

    // Property queries.
    bool getPowered()      const;
    bool getDiscoverable() const;
    bool getConnectable()  const;

private:
    DBusConnection* conn_;
    std::string     adapter_path_;   // "/org/bluez/hciN"
    std::string     hci_name_;       // "hci0", "hci1", …

    bool setBoolProperty  (const char* name, bool val);
    bool setStringProperty(const char* name, const std::string& val);
    bool setUint32Property(const char* name, uint32_t val);
    bool getBoolProperty  (const char* name) const;
};

#endif // BT_ADAPTER_H
