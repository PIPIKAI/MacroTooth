#ifndef IPC_SERVER_H
#define IPC_SERVER_H

#include <functional>
#include <string>
#include <thread>
#include <atomic>
#include <cstdint>

// Unix Domain Socket IPC server.
//
// Listens on a SOCK_STREAM socket at a configurable path.  Each connecting
// client gets its own thread.  Commands are line-delimited text:
//
//   KEY_PRESS  <key_hex> [mod_hex]   – press a key (modifier is optional)
//   KEY_RELEASE                      – release all keys
//   TYPE       <text>                – type a string (spaces allowed)
//   STATUS                           – query connection status
//   QUIT                             – close this client connection
//
// Responses (newline-terminated):
//   OK
//   ERROR <message>
//   STATUS connected=<0|1>
//
// Callback types that the caller must supply before calling start():
//   - KeyPressCallback(key_code, modifier)
//   - KeyReleaseCallback()
//   - TypeStringCallback(text)
//   - StatusCallback()  →  returns a status string (e.g. "connected=1")
class IpcServer {
public:
    using KeyPressCallback   = std::function<bool(uint8_t key, uint8_t mod)>;
    using KeyReleaseCallback = std::function<bool()>;
    using TypeCallback       = std::function<bool(const std::string& text)>;
    using StatusCallback     = std::function<std::string()>;

    IpcServer();
    ~IpcServer();

    void setKeyPressCallback  (KeyPressCallback   cb) { key_press_cb_   = cb; }
    void setKeyReleaseCallback(KeyReleaseCallback cb) { key_release_cb_ = cb; }
    void setTypeCallback      (TypeCallback       cb) { type_cb_        = cb; }
    void setStatusCallback    (StatusCallback     cb) { status_cb_      = cb; }

    // Create the socket and start the accept loop in a background thread.
    bool start(const std::string& socket_path);

    // Signal the accept loop to exit and join the thread.
    void stop();

private:
    void acceptLoop();
    void handleClient(int fd);
    std::string processCommand(const std::string& line);

    int         server_fd_;
    std::string socket_path_;
    std::thread accept_thread_;
    std::atomic<bool> running_;

    KeyPressCallback   key_press_cb_;
    KeyReleaseCallback key_release_cb_;
    TypeCallback       type_cb_;
    StatusCallback     status_cb_;
};

#endif // IPC_SERVER_H
