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
cb_appwin32_session_query_end (void)
{
  GApplication *application = g_application_get_default ();

  if (GTK_IS_APPLICATION (application))
    g_signal_emit_by_name (application, "query-end");
}

static void
cb_appwin32_session_end (void)
{
  GApplication *application = g_application_get_default ();

  if (G_IS_APPLICATION (application))
    g_application_quit (application);
}

static void
gtk_application_impl_win32_shutdown (GtkApplicationImpl *impl)
{
  GtkApplicationImplWin32 *appwin32 = (GtkApplicationImplWin32 *) impl;
  GList *iter;

  for (iter = gtk_application_get_windows (impl->application); iter; iter = iter->next)
    {
      if (!gtk_widget_get_realized (GTK_WIDGET (iter->data)))
        continue;

      gdk_win32_surface_set_session_callbacks (gtk_native_get_surface (GTK_NATIVE (iter->data)),
                                               NULL,
                                               NULL);
    }

  g_slist_free_full (appwin32->inhibitors, (GDestroyNotify) appwin32_inhibitor_free);
  appwin32->inhibitors = NULL;
}

static void
gtk_application_impl_win32_handle_window_realize (GtkApplicationImpl *impl,
                                                  GtkWindow          *window)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  gdk_win32_surface_set_session_callbacks (gtk_native_get_surface (GTK_NATIVE (window)),
                                           cb_appwin32_session_query_end,
                                           cb_appwin32_session_end);
}

static void
gtk_application_impl_win32_window_added (GtkApplicationImpl *impl,
                                         GtkWindow          *window,
                                         GVariant           *state)
{
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (!gtk_widget_get_realized (GTK_WIDGET (window)))
    return;   /* no surface yet, wait for handle_window_realize */

  gdk_win32_surface_set_session_callbacks (gtk_native_get_surface (GTK_NATIVE (window)),
                                           cb_appwin32_session_query_end,
                                           cb_appwin32_session_end);
}

static void
gtk_application_impl_win32_window_removed (GtkApplicationImpl *impl,
                                           GtkWindow          *window)
{
  GtkApplicationImplWin32 *appwin32 = (GtkApplicationImplWin32 *) impl;
  GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (window));
  GSList *iter;

  if (!surface)
    return;

  gdk_win32_surface_set_session_callbacks (surface,
                                           NULL,
                                           NULL);
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
  klass->handle_window_realize = gtk_application_impl_win32_handle_window_realize;
  klass->window_added = gtk_application_impl_win32_window_added;
  klass->window_removed = gtk_application_impl_win32_window_removed;
  klass->inhibit = gtk_application_impl_win32_inhibit;
  klass->uninhibit = gtk_application_impl_win32_uninhibit;
  klass->is_inhibited = gtk_application_impl_win32_is_inhibited;
}

static void
gtk_application_impl_win32_init (GtkApplicationImplWin32 *appwin32)
{
  DWORD dwLevel, dwFlags;

  appwin32->next_cookie = 0;
  appwin32->inhibitors = NULL;

  if (GetProcessShutdownParameters (&dwLevel, &dwFlags))
    {
      dwLevel = MAX (dwLevel, 0x300);
      SetProcessShutdownParameters (dwLevel, dwFlags);
    }
}
