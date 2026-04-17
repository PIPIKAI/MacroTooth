#include "ipc_server.h"
#include "logger.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <sstream>

IpcServer::IpcServer()
    : server_fd_(-1), running_(false) {}

IpcServer::~IpcServer() {
    stop();
}

bool IpcServer::start(const std::string& socket_path) {
    socket_path_ = socket_path;

    server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        LOG_E("IpcServer: socket() failed: %s", strerror(errno));
        return false;
    }

    // Remove stale socket file from a previous run.
    unlink(socket_path_.c_str());

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(server_fd_, reinterpret_cast<struct sockaddr*>(&addr),
             sizeof(addr)) < 0) {
        LOG_E("IpcServer: bind() failed: %s", strerror(errno));
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    if (listen(server_fd_, 8) < 0) {
        LOG_E("IpcServer: listen() failed: %s", strerror(errno));
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    running_ = true;
    accept_thread_ = std::thread(&IpcServer::acceptLoop, this);
    LOG_I("IpcServer: listening on %s", socket_path_.c_str());
    return true;
}

void IpcServer::stop() {
    if (!running_.exchange(false)) return;

    // Close the server socket so accept() unblocks.
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
    if (accept_thread_.joinable())
        accept_thread_.join();

    unlink(socket_path_.c_str());
    LOG_I("IpcServer: stopped");
}

void IpcServer::acceptLoop() {
    while (running_) {
        int client = accept(server_fd_, nullptr, nullptr);
        if (client < 0) {
            if (running_)
                LOG_E("IpcServer: accept() failed: %s", strerror(errno));
            break;
        }
        LOG_D("IpcServer: new client fd=%d", client);
        // Handle each client in its own detached thread so the accept loop
        // stays responsive.
        std::thread([this, client]() {
            handleClient(client);
        }).detach();
    }
}

void IpcServer::handleClient(int fd) {
    FILE* f = fdopen(fd, "r+");
    if (!f) {
        close(fd);
        return;
    }

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        // Strip trailing CR/LF.
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        if (len == 0) continue;

        std::string response = processCommand(line);
        fprintf(f, "%s\n", response.c_str());
        fflush(f);

        // QUIT closes this connection.
        if (response == "OK" && strncasecmp(line, "QUIT", 4) == 0)
            break;
    }

    fclose(f);  // also closes fd
    LOG_D("IpcServer: client disconnected");
}

// ── Command processing ────────────────────────────────────────────────────────

std::string IpcServer::processCommand(const std::string& line) {
    if (line.empty()) return "ERROR empty command";

    // Split into tokens.
    std::istringstream ss(line);
    std::string cmd;
    ss >> cmd;

    // Normalise to upper-case for comparison.
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    if (cmd == "KEY_PRESS") {
        // KEY_PRESS <key_hex> [mod_hex]
        std::string key_str, mod_str;
        ss >> key_str;
        ss >> mod_str;
        if (key_str.empty())
            return "ERROR missing key code";
        uint8_t key = static_cast<uint8_t>(strtoul(key_str.c_str(), nullptr, 16));
        uint8_t mod = mod_str.empty()
                        ? 0
                        : static_cast<uint8_t>(strtoul(mod_str.c_str(), nullptr, 16));
        if (key_press_cb_ && key_press_cb_(key, mod))
            return "OK";
        return "ERROR key_press failed";

    } else if (cmd == "KEY_RELEASE") {
        if (key_release_cb_ && key_release_cb_())
            return "OK";
        return "ERROR key_release failed";

    } else if (cmd == "TYPE") {
        // TYPE <text...>  (remainder of line, including spaces)
        std::string text;
        std::getline(ss, text);
        // Remove leading space left by >> extraction.
        if (!text.empty() && text[0] == ' ')
            text.erase(text.begin());
        if (type_cb_ && type_cb_(text))
            return "OK";
        return "ERROR type failed";

    } else if (cmd == "STATUS") {
        if (status_cb_)
            return "STATUS " + status_cb_();
        return "STATUS connected=0";

    } else if (cmd == "QUIT") {
        return "OK";

    } else {
        return "ERROR unknown command: " + cmd;
    }
}
