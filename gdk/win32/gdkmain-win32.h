
#pragma once

#include <stdbool.h>

typedef enum {
  OSVersionWindows7,
  OSVersionWindows8,
  OSVersionWindows8_1,
  OSVersionWindows10,
  OSVersionWindows11,
} OSVersion;

OSVersion
gdk_win32_get_os_version (void);

void
gdk_win32_invoke_callback (void (*callback)(void *arg),
                           void *user_data,
                           ...);

struct invoke_context;
typedef struct invoke_context invoke_context_t;

void
gdk_win32_with_loader_error_mode (invoke_context_t *context);

void
gdk_win32_with_activation_context (invoke_context_t *context);

bool
gdk_win32_check_app_packaged (void);

#ifdef _MSC_VER
#define SEH_TRY __try
#define SEH_FINALLY __finally
#define SEH_ABNORMAL_TERMINATION() AbnormalTermination()
#else
#define SEH_TRY
#define SEH_FINALLY
#define SEH_ABNORMAL_TERMINATION() FALSE
#endif
