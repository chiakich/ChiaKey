// [AUTO_HEADER]

// Diagnostic logging that only exists in dev builds.
//
// CHIAKEY_DEV_LOGGING is injected by Scripts/dev-install-local.sh alongside the
// dev connection name, so it tracks "built by the dev installer" rather than the
// Debug/Release configuration -- either script can be pointed at either one.
//
// It must stay a target-level define: OpenVanillaController.h gates ivars on it,
// so a translation unit compiled without it would disagree about the class
// layout. Every file that includes that header belongs to the IMK loader target,
// which gets the flag as a whole or not at all.

#ifndef CHIAKEYDEVLOG_H
#define CHIAKEYDEVLOG_H

#if CHIAKEY_DEV_LOGGING

#import <os/log.h>

static inline os_log_t ChiaKeyDevLog(void) {
  static os_log_t log;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    log = os_log_create("org.openvanilla.inputmethod.openvanilla", "input");
  });
  return log;
}

#define CHIAKEY_DEV_LOG(...) os_log(ChiaKeyDevLog(), __VA_ARGS__)
#define CHIAKEY_DEV_LOG_ERROR(...) os_log_error(ChiaKeyDevLog(), __VA_ARGS__)

#else

#define CHIAKEY_DEV_LOG(...) \
  do {                       \
  } while (0)
#define CHIAKEY_DEV_LOG_ERROR(...) \
  do {                             \
  } while (0)

#endif

#endif
