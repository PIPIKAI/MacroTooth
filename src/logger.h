#ifndef LOGGER_H
#define LOGGER_H

#include <syslog.h>

// Initialise syslog.  Call once at program startup.
inline void logger_init(const char* ident) {
    openlog(ident, LOG_PID | LOG_CONS, LOG_DAEMON);
}

inline void logger_close() {
    closelog();
}

// Convenience macros that write to syslog.
// Use exactly like printf: LOG_I("value is %d", x);
#define LOG_I(fmt, ...) syslog(LOG_INFO,    "[INFO]  " fmt, ##__VA_ARGS__)
#define LOG_W(fmt, ...) syslog(LOG_WARNING, "[WARN]  " fmt, ##__VA_ARGS__)
#define LOG_E(fmt, ...) syslog(LOG_ERR,     "[ERROR] " fmt, ##__VA_ARGS__)
#define LOG_D(fmt, ...) syslog(LOG_DEBUG,   "[DEBUG] " fmt, ##__VA_ARGS__)

#endif // LOGGER_H
