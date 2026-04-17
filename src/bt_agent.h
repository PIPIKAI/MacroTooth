#ifndef BT_AGENT_H
#define BT_AGENT_H

#include <dbus/dbus.h>
#include <string>

// Implements org.bluez.Agent1 with capability "NoInputNoOutput".
//
// When registered, BlueZ will call this agent for pairing decisions.
// All pairing requests are auto-accepted so that new hosts can connect
// without any manual confirmation on the device.
class BluetoothAgent {
public:
    BluetoothAgent();
    ~BluetoothAgent();

    // Register the agent with BlueZ AgentManager1 and optionally make it
    // the default agent.  Must be called after the adapter is powered on.
    bool start();

    // Unregister from BlueZ.
    void stop();

private:
    DBusConnection* conn_;
    bool registered_;

    bool registerWithManager();
    void unregisterFromManager();

    static DBusHandlerResult staticHandler(DBusConnection*, DBusMessage*, void*);
    DBusHandlerResult onMessage(DBusConnection*, DBusMessage*);
};

#endif // BT_AGENT_H
