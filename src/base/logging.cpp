#include <cstdio>
#include <cstdlib>
#ifndef __CYGWIN__ // Add by affggh
#include <android/log.h>

#include <flags.h>
#endif // __CYGWIN__
#include <base.hpp>

// Just need to include it somewhere
#ifndef __CYGWIN__ // Add by affggh
#include <base-rs.cpp>
#endif // __CYGWIN__
using namespace std;
#ifndef __CYGWIN__ // Add by affggh
static int fmt_and_log_with_rs(LogLevel level, const char *fmt, va_list ap) {
    char buf[4096];
    int ret = vsnprintf(buf, sizeof(buf), fmt, ap);
    log_with_rs(level, rust::Str(buf, ret));
    return ret;
}

int (*cpp_logger)(LogLevel level, const char *fmt, va_list ap) = fmt_and_log_with_rs;

// Used to override external C library logging
extern "C" int magisk_log_print(int prio, const char *tag, const char *fmt, ...) {
    LogLevel level;
    switch (prio) {
    case ANDROID_LOG_DEBUG:
        level = LogLevel::Debug;
        break;
    case ANDROID_LOG_INFO:
        level = LogLevel::Info;
        break;
    case ANDROID_LOG_WARN:
        level = LogLevel::Warn;
        break;
    case ANDROID_LOG_ERROR:
        level = LogLevel::Error;
        break;
    default:
        return 0;
    }

    char fmt_buf[4096];
    auto len = strlcpy(fmt_buf, tag, sizeof(fmt_buf));
    // Prevent format specifications in the tag
    std::replace(fmt_buf, fmt_buf + len, '%', '_');
    snprintf(fmt_buf + len, sizeof(fmt_buf) - len, ": %s", fmt);
    va_list argv;
    va_start(argv, fmt);
    int ret = cpp_logger(level, fmt_buf, argv);
    va_end(argv);
    return ret;
}

#define LOG_BODY(level) { \
    va_list argv;        \
    va_start(argv, fmt); \
    cpp_logger(LogLevel::level, fmt, argv); \
    va_end(argv);        \
}

// LTO will optimize out the NOP function
#if MAGISK_DEBUG
void LOGD(const char *fmt, ...) { LOG_BODY(Debug) }
#else
void LOGD(const char *fmt, ...) {}
#endif
void LOGI(const char *fmt, ...) { LOG_BODY(Info) }
void LOGW(const char *fmt, ...) { LOG_BODY(Warn) }
void LOGE(const char *fmt, ...) { LOG_BODY(Error) }

// Export raw symbol to fortify compat
extern "C" void __vloge(const char* fmt, va_list ap) {
    cpp_logger(LogLevel::Error, fmt, ap);
}
#else
// Rollback to old version
// Which can compile on cygwin
int nop_log(const char *, va_list) { return 0; }

void nop_ex(int) {}

log_callback log_cb = {
    .d = nop_log,
    .i = nop_log,
    .w = nop_log,
    .e = nop_log,
    .ex = nop_ex
};

void no_logging() {
    log_cb.d = nop_log;
    log_cb.i = nop_log;
    log_cb.w = nop_log;
    log_cb.e = nop_log;
    log_cb.ex = nop_ex;
}

static int vprintfe(const char *fmt, va_list ap) {
    return vfprintf(stderr, fmt, ap);
}

void cmdline_logging() {
    log_cb.d = vprintfe;
    log_cb.i = vprintf;
    log_cb.w = vprintfe;
    log_cb.e = vprintfe;
    log_cb.ex = exit;
}

#define LOG_BODY(prio) { \
    va_list argv;        \
    va_start(argv, fmt); \
    log_cb.prio(fmt, argv); \
    va_end(argv);        \
}

// LTO will optimize out the NOP function
#if MAGISK_DEBUG
void LOGD(const char *fmt, ...) { LOG_BODY(d) }
#else
void LOGD(const char *fmt, ...) {}
#endif
void LOGI(const char *fmt, ...) { LOG_BODY(i) }
void LOGW(const char *fmt, ...) { LOG_BODY(w) }
void LOGE(const char *fmt, ...) { LOG_BODY(e); log_cb.ex(EXIT_FAILURE); }

// Export raw symbol to fortify compat
extern "C" int __vloge(const char* fmt, va_list ap) {
    return log_cb.e(fmt, ap);
}
#endif // __CYGWIN__