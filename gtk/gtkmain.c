/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gdk/gdk.h"
#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkprofilerprivate.h"
#include "gdk/gdkdebugprivate.h"
#include "gdk/gdkseatprivate.h"
#include "gsk/gskprivate.h"
#include "gsk/gskrendernodeprivate.h"
#include "gtknative.h"

#include <locale.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>          /* For uid_t, gid_t */

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#include <hb-glib.h>

#include <glib/gi18n-lib.h>

#include "gtkdropprivate.h"
#include "gtkmain.h"
#include "gtkmediafileprivate.h"
#include "gtkmodulesprivate.h"
#include "gtkprivate.h"
#include "gtkrecentmanagerprivate.h"
#include "gtktooltipprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowprivate.h"
#include "gtkwindowgroup.h"
#include "print/gtkprintbackendprivate.h"
#include "gtkimmoduleprivate.h"
#include "gtkroot.h"
#include "gtknative.h"
#include "gtkpopcountprivate.h"

#include "inspector/init.h"

#include "gdk/gdkeventsprivate.h"
#include "gdk/gdksurfaceprivate.h"

#ifdef GDK_WINDOWING_WAYLAND
#include "gdk/wayland/gdkwaylandprivate.h"
#endif

/* {{{ Locale handling */

/**
 * gtk_get_locale_direction:
 *
 * Gets the direction of the current locale.
 *
 * This is the expected reading direction for text and UI.
 *
 * This function depends on the current locale being set with
 * `setlocale()` and will default to setting the `GTK_TEXT_DIR_LTR`
 * direction otherwise. `GTK_TEXT_DIR_NONE` will never be returned.
 *
 * GTK sets the default text direction according to the locale during
 * [func@Gtk.init], and you should normally use [method@Gtk.Widget.get_direction]
 * or [func@Gtk.Widget.get_default_direction] to obtain the current direction.
 *
 * This function is only needed rare cases when the locale is
 * changed after GTK has already been initialized. In this case,
 * you can use it to update the default text direction as follows:
 *
 * ```c
 * #include <locale.h>
 *
 * static void
 * update_locale (const char *new_locale)
 * {
 *   setlocale (LC_ALL, new_locale);
 *   gtk_widget_set_default_direction (gtk_get_locale_direction ());
 * }
 * ```
 *
 * Returns: the direction of the current locale
 */
GtkTextDirection
gtk_get_locale_direction (void)
{
  PangoLanguage *language;
  const PangoScript *scripts;
  int n_scripts;

  language = gtk_get_default_language ();
  scripts = pango_language_get_scripts (language, &n_scripts);

  if (n_scripts > 0)
    {
      for (int i = 0; i < n_scripts; i++)
        {
          hb_script_t script;

          script = hb_glib_script_to_script ((GUnicodeScript) scripts[i]);

          switch (hb_script_get_horizontal_direction (script))
            {
            case HB_DIRECTION_LTR:
              return GTK_TEXT_DIR_LTR;
            case HB_DIRECTION_RTL:
              return GTK_TEXT_DIR_RTL;
            case HB_DIRECTION_TTB:
            case HB_DIRECTION_BTT:
            case HB_DIRECTION_INVALID:
            default:
              break;
            }
        }
    }

  return GTK_TEXT_DIR_LTR;
}

/**
 * gtk_get_default_language:
 *
 * Returns the `PangoLanguage` for the default language
 * currently in effect.
 *
 * Note that this can change over the life of an
 * application.
 *
 * The default language is derived from the current
 * locale. It determines, for example, whether GTK uses
 * the right-to-left or left-to-right text direction.
 *
 * This function is equivalent to [func@Pango.Language.get_default].
 * See that function for details.
 *
 * Returns: (transfer none): the default language
 */
PangoLanguage *
gtk_get_default_language (void)
{
  return pango_language_get_default ();
}

/* }}} */
/* {{{ Clipboard sync */

typedef struct {
  GMainLoop *store_loop;
  guint n_clipboards;
  guint timeout_id;
} ClipboardStore;

static void
clipboard_store_finished (GObject      *source,
                          GAsyncResult *result,
                          gpointer      data)
{
  ClipboardStore *store;
  GError *error = NULL;

  if (!gdk_clipboard_store_finish (GDK_CLIPBOARD (source), result, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_error_free (error);
          return;
        }

      g_error_free (error);
    }

  store = data;
  store->n_clipboards--;
  if (store->n_clipboards == 0)
    g_main_loop_quit (store->store_loop);
}

static void
sync_timed_out_cb (ClipboardStore *store)
{
  store->timeout_id = 0;
  g_main_loop_quit (store->store_loop);
}

void
gtk_main_sync (void)
{
  ClipboardStore store = { NULL, };
  GSList *displays, *l;
  GCancellable *cancel;

  /* Try storing all clipboard data we have */
  displays = gdk_display_manager_list_displays (gdk_display_manager_get ());
  if (displays == NULL)
    return;

  cancel = g_cancellable_new ();

  for (l = displays; l; l = l->next)
    {
      GdkDisplay *display = l->data;
      GdkClipboard *clipboard = gdk_display_get_clipboard (display);

      gdk_clipboard_store_async (clipboard,
                                 G_PRIORITY_HIGH,
                                 cancel,
                                 clipboard_store_finished,
                                 &store);
      store.n_clipboards++;
    }
  g_slist_free (displays);

  store.store_loop = g_main_loop_new (NULL, TRUE);
  store.timeout_id = g_timeout_add_seconds_once (10, (GSourceOnceFunc) sync_timed_out_cb, &store);
  gdk_source_set_static_name_by_id (store.timeout_id, "[gtk] gtk_main_sync clipboard store timeout");

  if (g_main_loop_is_running (store.store_loop))
    g_main_loop_run (store.store_loop);

  g_cancellable_cancel (cancel);
  g_object_unref (cancel);
  g_clear_handle_id (&store.timeout_id, g_source_remove);
  g_clear_pointer (&store.store_loop, g_main_loop_unref);

  /* Synchronize the recent manager singleton */
  _gtk_recent_manager_sync ();
}

/* }}} */

/* vim:set foldmethod=marker: */
