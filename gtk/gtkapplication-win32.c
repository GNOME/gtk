/*
 * GtkApplication implementation for Win32.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileCopyrightText: 2025-2026 GNOME Foundation
 *
 * ## IDLE and SUSPEND inhibition
 * https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-powersetrequest
 * Requests counters are automatically managed by PowerSetRequest().
 * It could be possible to use the old SetThreadExecutionState() API instead,
 * but this one needs to be called periodically.
 * The underlying Win32 APIs need a reason string, if not provided the
 * inhibition will be ignored.
 * Note that power requests will be cancelled if the user manually stops them,
 * e.g. by sleeping from the start menu or by closing the lid.
 */

#define WIN32_LEAN_AND_MEAN

#include "config.h"

#include "gtkapplicationprivate.h"
#include "gtknative.h"

#include "win32/gdkprivate-win32.h"

#include <windows.h>


typedef GtkApplicationImplClass GtkApplicationImplWin32Class;

typedef struct
{
  GtkApplicationImpl impl;
  GSList *inhibitors;
  guint next_cookie;
} GtkApplicationImplWin32;

typedef struct
{
  guint cookie;
  GtkApplicationInhibitFlags flags;
  HANDLE pwr_handle;
} GtkApplicationWin32Inhibitor;

G_DEFINE_TYPE (GtkApplicationImplWin32, gtk_application_impl_win32, GTK_TYPE_APPLICATION_IMPL)


static void
appwin32_inhibitor_free (GtkApplicationWin32Inhibitor *inhibitor)
{
  CloseHandle (inhibitor->pwr_handle);
  g_free (inhibitor);
}

static void
gtk_application_impl_win32_shutdown (GtkApplicationImpl *impl)
{
  GtkApplicationImplWin32 *appwin32 = (GtkApplicationImplWin32 *) impl;
  GList *iter;

  g_slist_free_full (appwin32->inhibitors, (GDestroyNotify) appwin32_inhibitor_free);
  appwin32->inhibitors = NULL;
}

static guint
gtk_application_impl_win32_inhibit (GtkApplicationImpl         *impl,
                                    GtkWindow                  *window,
                                    GtkApplicationInhibitFlags  flags,
                                    const char                 *reason)
{
  GtkApplicationImplWin32 *appwin32 = (GtkApplicationImplWin32 *) impl;
  GtkApplicationWin32Inhibitor *inhibitor = g_new0 (GtkApplicationWin32Inhibitor, 1);
  wchar_t *reason_w = g_utf8_to_utf16 (reason, -1, NULL, NULL, NULL);

  inhibitor->cookie = ++appwin32->next_cookie;

  if (flags & (GTK_APPLICATION_INHIBIT_SUSPEND | GTK_APPLICATION_INHIBIT_IDLE))
    {
      REASON_CONTEXT context;

      memset (&context, 0, sizeof (context));
      context.Version = POWER_REQUEST_CONTEXT_VERSION;
      context.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
      context.Reason.SimpleReasonString = reason_w;

      inhibitor->pwr_handle = PowerCreateRequest (&context);

      if (flags & GTK_APPLICATION_INHIBIT_SUSPEND)
        {
          if (PowerSetRequest (inhibitor->pwr_handle, PowerRequestSystemRequired))
            inhibitor->flags |= GTK_APPLICATION_INHIBIT_SUSPEND;
          else
            g_warning ("Failed to apply suspend inhibition");
        }

      if (flags & GTK_APPLICATION_INHIBIT_IDLE)
        {
          if (PowerSetRequest (inhibitor->pwr_handle, PowerRequestDisplayRequired))
            inhibitor->flags |= GTK_APPLICATION_INHIBIT_IDLE;
          else
            g_warning ("Failed to apply idle inhibition");
        }
    }

  g_free (reason_w);

  if (inhibitor->flags)
    {
      appwin32->inhibitors = g_slist_prepend (appwin32->inhibitors, inhibitor);
      return inhibitor->cookie;
    }

  appwin32_inhibitor_free (inhibitor);
  return 0;
}

static void
gtk_application_impl_win32_uninhibit (GtkApplicationImpl *impl, guint cookie)
{
  GtkApplicationImplWin32 *appwin32 = (GtkApplicationImplWin32 *) impl;
  GSList *iter;

  for (iter = appwin32->inhibitors; iter; iter = iter->next)
    {
      GtkApplicationWin32Inhibitor *inhibitor = iter->data;

      if (inhibitor->cookie == cookie)
        {
          if (inhibitor->flags & GTK_APPLICATION_INHIBIT_SUSPEND)
            PowerClearRequest (inhibitor->pwr_handle, PowerRequestSystemRequired);

          if (inhibitor->flags & GTK_APPLICATION_INHIBIT_IDLE)
            PowerClearRequest (inhibitor->pwr_handle, PowerRequestDisplayRequired);

          appwin32_inhibitor_free (inhibitor);
          appwin32->inhibitors = g_slist_delete_link (appwin32->inhibitors, iter);
          return;
        }
    }

  g_warning ("Invalid inhibitor cookie: %d", cookie);
}

static gboolean
gtk_application_impl_win32_is_inhibited (GtkApplicationImpl         *impl,
                                         GtkApplicationInhibitFlags  flags)
{
  GtkApplicationImplWin32 *appwin32 = (GtkApplicationImplWin32 *) impl;
  GtkApplicationInhibitFlags active_flags = 0;
  GSList *iter;

  for (iter = appwin32->inhibitors; iter; iter = iter->next)
    {
      GtkApplicationWin32Inhibitor *inhibitor = iter->data;
      active_flags |= inhibitor->flags;
    }

  return (active_flags & flags) == flags;
}

static void
gtk_application_impl_win32_class_init (GtkApplicationImplClass *klass)
{
  klass->shutdown = gtk_application_impl_win32_shutdown;
  klass->inhibit = gtk_application_impl_win32_inhibit;
  klass->uninhibit = gtk_application_impl_win32_uninhibit;
  klass->is_inhibited = gtk_application_impl_win32_is_inhibited;
}

static void
gtk_application_impl_win32_init (GtkApplicationImplWin32 *appwin32)
{
  appwin32->next_cookie = 0;
  appwin32->inhibitors = NULL;
}
