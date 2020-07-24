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
#include <pango/pangowin32.h>

#include "gdkdisplayprivate.h"
#include "gdkprivate-win32.h"
#include "gdkwin32.h"

static char *
_get_system_font_name (HDC hdc)
{
  NONCLIENTMETRICSW ncm;
  PangoFontDescription *font_desc;
  char *result, *font_desc_string;
  int logpixelsy;
  int font_size;

  ncm.cbSize = sizeof(NONCLIENTMETRICSW);
  if (!SystemParametersInfoW (SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
    return NULL;

  logpixelsy = GetDeviceCaps (hdc, LOGPIXELSY);
  font_desc = pango_win32_font_description_from_logfontw (&ncm.lfMessageFont);
  font_desc_string = pango_font_description_to_string (font_desc);
  pango_font_description_free (font_desc);

  /* https://docs.microsoft.com/en-us/windows/desktop/api/wingdi/ns-wingdi-taglogfonta */
  font_size = -MulDiv (ncm.lfMessageFont.lfHeight, 72, logpixelsy);
  result = g_strdup_printf ("%s %d", font_desc_string, font_size);
  g_free (font_desc_string);

  return result;
}

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
_gdk_win32_get_setting (const char *name,
                        GValue      *value)
{
  /*
   * XXX : if these values get changed through the Windoze UI the
   *       respective gdk_events are not generated yet.
   */
  if (strcmp ("gtk-double-click-time", name) == 0)
    {
      int i = GetDoubleClickTime ();
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : %d\n", name, i));
      g_value_set_int (value, i);
      return TRUE;
    }
  else if (strcmp ("gtk-double-click-distance", name) == 0)
    {
      int i = MAX(GetSystemMetrics (SM_CXDOUBLECLK), GetSystemMetrics (SM_CYDOUBLECLK));
      GDK_NOTE(MISC, g_print("gdk_display_get_setting(\"%s\") : %d\n", name, i));
      g_value_set_int (value, i);
      return TRUE;
    }
  else if (strcmp ("gtk-dnd-drag-threshold", name) == 0)
    {
      int i = MAX(GetSystemMetrics (SM_CXDRAG), GetSystemMetrics (SM_CYDRAG));
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
  else if (strcmp ("gtk-xft-hinting", name) == 0)
    {
      GDK_NOTE(MISC, g_print ("gdk_screen_get_setting(\"%s\") : 1\n", name));
      g_value_set_int (value, 1);
      return TRUE;
    }
  else if (strcmp ("gtk-xft-antialias", name) == 0)
    {
      GDK_NOTE(MISC, g_print ("gdk_screen_get_setting(\"%s\") : 1\n", name));
      g_value_set_int (value, 1);
      return TRUE;
    }
  else if (strcmp ("gtk-xft-hintstyle", name) == 0)
    {
      g_value_set_static_string (value, "hintfull");
      GDK_NOTE(MISC, g_print ("gdk_screen_get_setting(\"%s\") : %s\n", name, g_value_get_string (value)));
      return TRUE;
    }
  else if (strcmp ("gtk-xft-rgba", name) == 0)
    {
      unsigned int orientation = 0;
      if (SystemParametersInfoW (SPI_GETFONTSMOOTHINGORIENTATION, 0, &orientation, 0))
        {
          if (orientation == FE_FONTSMOOTHINGORIENTATIONRGB)
            g_value_set_static_string (value, "rgb");
          else if (orientation == FE_FONTSMOOTHINGORIENTATIONBGR)
            g_value_set_static_string (value, "bgr");
          else
            g_value_set_static_string (value, "none");
        }
      else
        g_value_set_static_string (value, "none");

      GDK_NOTE(MISC, g_print ("gdk_screen_get_setting(\"%s\") : %s\n", name, g_value_get_string (value)));
      return TRUE;
    }
  else if (strcmp ("gtk-font-name", name) == 0)
    {
      char *font_name = _get_system_font_name (_gdk_display_hdc);

      if (font_name)
        {
          /* The pango font fallback list got fixed during 1.43, before that
           * using anything but "Segoe UI" would lead to a poor glyph coverage */
          if (pango_version_check (1, 43, 0) != NULL &&
              g_ascii_strncasecmp (font_name, "Segoe UI", strlen ("Segoe UI")) != 0)
            {
              g_free (font_name);
              return FALSE;
            }

          GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : %s\n", name, font_name));
          g_value_take_string (value, font_name);
          return TRUE;
        }
      else
        {
          g_warning ("gdk_screen_get_setting: Detecting the system font failed");
          return FALSE;
        }
    }
  else if (strcmp ("gtk-im-module", name) == 0)
    {
      if (_gdk_input_locale_is_ime)
        g_value_set_static_string (value, "ime");
      else
        g_value_set_static_string (value, "");

      return TRUE;
    }

  return FALSE;
}
