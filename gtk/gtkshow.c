/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 2008  Jaap Haitsma <jaap@haitsma.org>
 *
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gdk/gdk.h>

#include "gtkshow.h"

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#endif

/**
 * gtk_show_uri:
 * @screen: (allow-none): screen to show the uri on
 *     or %NULL for the default screen
 * @uri: the uri to show
 * @timestamp: a timestamp to prevent focus stealing
 * @error: a #GError that is returned in case of errors
 *
 * This is a convenience function for launching the default application
 * to show the uri. The uri must be of a form understood by GIO (i.e. you
 * need to install gvfs to get support for uri schemes such as http://
 * or ftp://, as only local files are handled by GIO itself).
 * Typical examples are
 * - `file:///home/gnome/pict.jpg`
 * - `http://www.gnome.org`
 * - `mailto:me@gnome.org`
 *
 * Ideally the timestamp is taken from the event triggering
 * the gtk_show_uri() call. If timestamp is not known you can take
 * %GDK_CURRENT_TIME.
 *
 * Returns: %TRUE on success, %FALSE on error
 *
 * Since: 2.14
 */
gboolean
gtk_show_uri (GdkScreen    *screen,
              const gchar  *uri,
              guint32       timestamp,
              GError      **error)
{
  GdkAppLaunchContext *context;
  gboolean ret;
  GdkDisplay *display;

  g_return_val_if_fail (uri != NULL, FALSE);

  if (screen != NULL)
    display = gdk_screen_get_display (screen);
  else
    display = gdk_display_get_default ();

  context = gdk_display_get_app_launch_context (display);
  gdk_app_launch_context_set_screen (context, screen);
  gdk_app_launch_context_set_timestamp (context, timestamp);

  ret = g_app_info_launch_default_for_uri (uri, G_APP_LAUNCH_CONTEXT (context), error);
  g_object_unref (context);

  return ret;
}

/**
 * gtk_show_uri_on_window:
 * @parent: parent window
 * @uri: the uri to show
 * @timestamp: a timestamp to prevent focus stealing
 * @error: a #GError that is returned in case of errors
 *
 * A convenience function for launching the default application
 * to show the uri. Like gtk_show_uri(), but takes a window
 * as transient parent instead of a screen.
 *
 * Returns: %TRUE on success, %FALSE on error
 *
 * Since: 3.22
 */
gboolean
gtk_show_uri_on_window (GtkWindow   *parent,
                        const char  *uri,
                        guint32      timestamp,
                        GError     **error)
{
  GdkAppLaunchContext *context;
  gboolean ret;
  GdkDisplay *display;

  g_return_val_if_fail (uri != NULL, FALSE);

  if (parent)
    display = gtk_widget_get_display (GTK_WIDGET (parent));
  else
    display = gdk_display_get_default ();

  context = gdk_display_get_app_launch_context (display);

  if (parent)
    {
      GdkWindow *window = gtk_widget_get_window (GTK_WIDGET (parent));
#ifdef GDK_WINDOWING_X11
      if (GDK_IS_X11_WINDOW(window))
        {
          char *parent_window_str;

          parent_window_str = g_strdup_printf ("x11:%x", (guint32)gdk_x11_window_get_xid (window));
          g_app_launch_context_setenv (G_APP_LAUNCH_CONTEXT (context),
                                       "PARENT_WINDOW_ID",
                                       parent_window_str);
          g_free (parent_window_str);
        }
#endif
    }

  gdk_app_launch_context_set_timestamp (context, timestamp);

  ret = g_app_info_launch_default_for_uri (uri, G_APP_LAUNCH_CONTEXT (context), error);

  g_object_unref (context);

  return ret;

}
