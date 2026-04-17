#ifndef GATT_APPLICATION_H
#define GATT_APPLICATION_H

#include <dbus/dbus.h>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <mutex>

// BLE HID over GATT application.
//
// Registers a GATT application with org.bluez.GattManager1 and an LE
// advertisement with org.bluez.LEAdvertisingManager1.  The HID service
// (0x1812) exposes a keyboard Input Report characteristic that can be
// updated via sendInputReport().  A Device Information service (0x180A)
// is also included.
//
// The caller must call dispatch() regularly (or in a loop) to process
// incoming D-Bus method calls from BlueZ (ReadValue, WriteValue,
// StartNotify, StopNotify, etc.).
class GattApplication {
public:
    // Called when the host writes an Output/Feature report.
    using OutputReportCallback = std::function<void(const uint8_t*, size_t)>;

    // Called when a host enables (connected=true) or disables (connected=false)
    // HID report notifications – a reliable proxy for BLE connection state.
    using ConnectionCallback = std::function<void(bool connected)>;

    GattApplication();
    ~GattApplication();

    // Connect to D-Bus, register GATT application and LE advertisement.
    // report_desc / report_desc_len: HID Report Descriptor bytes.
    // device_name: local name shown in the BLE advertisement.
    bool start(const uint8_t* report_desc, size_t report_desc_len,
               const std::string& device_name = "MacroTooth Keyboard");

    // Unregister advertisement and GATT application, disconnect from D-Bus.
    void stop();

    // Send an HID Input Report via GATT notification.
    // Returns true if the report was stored (notification sent only when a
    // host has called StartNotify).
    bool sendInputReport(const uint8_t* data, size_t len);

    void setOutputReportCallback(OutputReportCallback cb) { output_cb_ = cb; }
    void setConnectionCallback  (ConnectionCallback   cb) { conn_cb_   = cb; }

    // Override the default PnP ID returned by the Device Information Service.
    // vid_source: 0x01 = Bluetooth SIG, 0x02 = USB IF.
    // Call before start().
    void setPnpId(uint8_t vid_source, uint16_t vid, uint16_t pid, uint16_t version);

    // Process pending D-Bus messages.  timeout_ms = 0 → non-blocking.
    void dispatch(int timeout_ms = 0);

    bool isStarted()   const { return registered_; }
    bool isConnected() const { return notify_enabled_; }

private:
    bool connectDBus();
    bool registerGattApplication();
    bool registerAdvertisement();
    void unregisterAll();

    // D-Bus fallback message handler (all paths under /com/macrotooth).
    static DBusHandlerResult staticHandler(DBusConnection* conn,
                                           DBusMessage* msg,
                                           void* user_data);
    DBusHandlerResult onMessage(DBusConnection* conn, DBusMessage* msg);

    // Handler implementations.
    DBusMessage* handleGetManagedObjects(DBusMessage* msg);
    DBusMessage* handleCharacteristicRead(DBusMessage* msg, const char* path);
    DBusMessage* handleCharacteristicWrite(DBusMessage* msg, const char* path);
    DBusMessage* handleStartNotify(DBusMessage* msg);
    DBusMessage* handleStopNotify(DBusMessage* msg);
    DBusMessage* handleDescriptorRead(DBusMessage* msg, const char* path);
    DBusMessage* handleGetAllProperties(DBusMessage* msg, const char* path);
    void         handleAdvertisementRelease();

    // Build the GetManagedObjects return value into reply.
    void appendManagedObjects(DBusMessage* reply);

    DBusConnection*      conn_;
    OutputReportCallback output_cb_;
    ConnectionCallback   conn_cb_;
    std::string          device_name_;
    std::vector<uint8_t> pnp_id_;       // 7-byte PnP ID characteristic value

    // State protected by mutex_.
    mutable std::mutex   mutex_;
    std::vector<uint8_t> report_desc_;
    std::vector<uint8_t> input_report_;  // current 8-byte keyboard report
    uint8_t              protocol_mode_; // 0x01 = Report Protocol
    bool                 notify_enabled_;

    bool registered_;
    bool adv_registered_;
};

#endif // GATT_APPLICATION_H
