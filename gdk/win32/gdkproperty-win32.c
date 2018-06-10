/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
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
#include <string.h>
#include <stdlib.h>
#include <glib/gprintf.h>

#include "gdkproperty.h"
#include "gdkdisplayprivate.h"
#include "gdkprivate-win32.h"
#include "gdkwin32.h"

/*
  For reference, from gdk/x11/gdksettings.c:

  "Net/DoubleClickTime\0"     "gtk-double-click-time\0"
  "Net/DoubleClickDistance\0" "gtk-double-click-distance\0"
  "Net/DndDragThreshold\0"    "gtk-dnd-drag-threshold\0"
  "Net/CursorBlink\0"         "gtk-cursor-blink\0"
  "Net/CursorBlinkTime\0"     "gtk-cursor-blink-time\0"
  "Net/ThemeName\0"           "gtk-theme-name\0"
  "Net/IconThemeName\0"       "gtk-icon-theme-name\0"
  "Gtk/ColorPalette\0"        "gtk-color-palette\0"
  "Gtk/FontName\0"            "gtk-font-name\0"
  "Gtk/KeyThemeName\0"        "gtk-key-theme-name\0"
  "Gtk/Modules\0"             "gtk-modules\0"
  "Gtk/CursorBlinkTimeout\0"  "gtk-cursor-blink-timeout\0"
  "Gtk/CursorThemeName\0"     "gtk-cursor-theme-name\0"
  "Gtk/CursorThemeSize\0"     "gtk-cursor-theme-size\0"
  "Gtk/ColorScheme\0"         "gtk-color-scheme\0"
  "Gtk/EnableAnimations\0"    "gtk-enable-animations\0"
  "Xft/Antialias\0"           "gtk-xft-antialias\0"
  "Xft/Hinting\0"             "gtk-xft-hinting\0"
  "Xft/HintStyle\0"           "gtk-xft-hintstyle\0"
  "Xft/RGBA\0"                "gtk-xft-rgba\0"
  "Xft/DPI\0"                 "gtk-xft-dpi\0"
  "Gtk/EnableAccels\0"        "gtk-enable-accels\0"
  "Gtk/ScrolledWindowPlacement\0" "gtk-scrolled-window-placement\0"
  "Gtk/IMModule\0"            "gtk-im-module\0"
  "Fontconfig/Timestamp\0"    "gtk-fontconfig-timestamp\0"
  "Net/SoundThemeName\0"      "gtk-sound-theme-name\0"
  "Net/EnableInputFeedbackSounds\0" "gtk-enable-input-feedback-sounds\0"
  "Net/EnableEventSounds\0"  "gtk-enable-event-sounds\0";

  More, from various places in gtk sources:

  gtk-entry-select-on-focus
  gtk-split-cursor

*/
gboolean
_gdk_win32_get_setting (const gchar *name,
                        GValue      *value)
{
  /*
   * XXX : if these values get changed through the Windoze UI the
   *       respective gdk_events are not generated yet.
   */
  if (strcmp ("gtk-double-click-time", name) == 0)
    {
      gint i = GetDoubleClickTime ();
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : %d\n", name, i));
      g_value_set_int (value, i);
      return TRUE;
    }
  else if (strcmp ("gtk-double-click-distance", name) == 0)
    {
      gint i = MAX(GetSystemMetrics (SM_CXDOUBLECLK), GetSystemMetrics (SM_CYDOUBLECLK));
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : %d\n", name, i));
      g_value_set_int (value, i);
      return TRUE;
    }
  else if (strcmp ("gtk-dnd-drag-threshold", name) == 0)
    {
      gint i = MAX(GetSystemMetrics (SM_CXDRAG), GetSystemMetrics (SM_CYDRAG));
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : %d\n", name, i));
      g_value_set_int (value, i);
      return TRUE;
    }
  else if (strcmp ("gtk-split-cursor", name) == 0)
    {
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : FALSE\n", name));
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }
  else if (strcmp ("gtk-alternative-button-order", name) == 0)
    {
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : TRUE\n", name));
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }
  else if (strcmp ("gtk-alternative-sort-arrows", name) == 0)
    {
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : TRUE\n", name));
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }
  else if (strcmp ("gtk-shell-shows-desktop", name) == 0)
    {
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : TRUE\n", name));
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }
  else if (strcmp ("gtk-font-name", name) == 0)
    {
      NONCLIENTMETRICS ncm;
      CPINFOEX cpinfoex_default, cpinfoex_curr_thread;
      OSVERSIONINFO info;
      BOOL result_default, result_curr_thread;

      info.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);

      /* TODO: Fallback to using Pango on Windows 8 and later,
       * as this method of handling gtk-font-name does not work
       * well there, where garbled text will be displayed for texts
       * that are not supported by the default menu font.  Look for
       * whether there is a better solution for this on Windows 8 and
       * later
       */
      if (!GetVersionEx (&info) ||
          info.dwMajorVersion > 6 ||
          (info.dwMajorVersion == 6 && info.dwMinorVersion >= 2))
        return FALSE;

      /* check whether the system default ANSI codepage matches the
       * ANSI code page of the running thread.  If so, continue, otherwise
       * fall back to using Pango to handle gtk-font-name
       */
      result_default = GetCPInfoEx (CP_ACP, 0, &cpinfoex_default);
      result_curr_thread = GetCPInfoEx (CP_THREAD_ACP, 0, &cpinfoex_curr_thread);

      if (!result_default ||
          !result_curr_thread ||
          cpinfoex_default.CodePage != cpinfoex_curr_thread.CodePage)
        return FALSE;

      ncm.cbSize = sizeof(NONCLIENTMETRICS);
      if (SystemParametersInfo (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, FALSE))
        {
          /* Pango finally uses GetDeviceCaps to scale, we use simple
	   * approximation here.
	   */
          int nHeight = (0 > ncm.lfMenuFont.lfHeight ? - 3 * ncm.lfMenuFont.lfHeight / 4 : 10);
          if (OUT_STRING_PRECIS == ncm.lfMenuFont.lfOutPrecision)
            GDK_NOTE(MISC, g_print("gdk_display_get_setting(%s) : ignoring bitmap font '%s'\n",
                                   name, ncm.lfMenuFont.lfFaceName));
          else if (ncm.lfMenuFont.lfFaceName && strlen(ncm.lfMenuFont.lfFaceName) > 0 &&
                   /* Avoid issues like those described in bug #135098 */
                   g_utf8_validate (ncm.lfMenuFont.lfFaceName, -1, NULL))
            {
              char *s = g_strdup_printf ("%s %d", ncm.lfMenuFont.lfFaceName, nHeight);
              GDK_NOTE(MISC, g_print("gdk_display_get_setting(%s) : %s\n", name, s));
              g_value_set_string (value, s);

              g_free(s);
              return TRUE;
            }
        }
    }

  return FALSE;
}
