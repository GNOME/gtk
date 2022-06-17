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

#include "gdkscreen.h"
#include "gdkproperty.h"
#include "gdkselection.h"
#include "gdkprivate-win32.h"
#include "gdkmonitor-win32.h"
#include "gdkwin32.h"

GdkAtom
_gdk_win32_display_manager_atom_intern (GdkDisplayManager *manager,
					const gchar *atom_name,
					gint         only_if_exists)
{
  ATOM win32_atom;
  GdkAtom retval;
  static GHashTable *atom_hash = NULL;

  if (!atom_hash)
    atom_hash = g_hash_table_new (g_str_hash, g_str_equal);

  retval = g_hash_table_lookup (atom_hash, atom_name);
  if (!retval)
    {
      if (strcmp (atom_name, "PRIMARY") == 0)
	retval = GDK_SELECTION_PRIMARY;
      else if (strcmp (atom_name, "SECONDARY") == 0)
	retval = GDK_SELECTION_SECONDARY;
      else if (strcmp (atom_name, "CLIPBOARD") == 0)
	retval = GDK_SELECTION_CLIPBOARD;
      else if (strcmp (atom_name, "ATOM") == 0)
	retval = GDK_SELECTION_TYPE_ATOM;
      else if (strcmp (atom_name, "BITMAP") == 0)
	retval = GDK_SELECTION_TYPE_BITMAP;
      else if (strcmp (atom_name, "COLORMAP") == 0)
	retval = GDK_SELECTION_TYPE_COLORMAP;
      else if (strcmp (atom_name, "DRAWABLE") == 0)
	retval = GDK_SELECTION_TYPE_DRAWABLE;
      else if (strcmp (atom_name, "INTEGER") == 0)
	retval = GDK_SELECTION_TYPE_INTEGER;
      else if (strcmp (atom_name, "PIXMAP") == 0)
	retval = GDK_SELECTION_TYPE_PIXMAP;
      else if (strcmp (atom_name, "WINDOW") == 0)
	retval = GDK_SELECTION_TYPE_WINDOW;
      else if (strcmp (atom_name, "STRING") == 0)
	retval = GDK_SELECTION_TYPE_STRING;
      else
	{
	  win32_atom = GlobalAddAtom (atom_name);
	  retval = GUINT_TO_POINTER ((guint) win32_atom);
	}
      g_hash_table_insert (atom_hash,
			   g_strdup (atom_name),
			   retval);
    }

  return retval;
}

gchar *
_gdk_win32_display_manager_get_atom_name (GdkDisplayManager *manager,
					  GdkAtom            atom)
{
  ATOM win32_atom;
  gchar name[256];

  if (GDK_NONE == atom) return g_strdup ("<none>");
  else if (GDK_SELECTION_PRIMARY == atom) return g_strdup ("PRIMARY");
  else if (GDK_SELECTION_SECONDARY == atom) return g_strdup ("SECONDARY");
  else if (GDK_SELECTION_CLIPBOARD == atom) return g_strdup ("CLIPBOARD");
  else if (GDK_SELECTION_TYPE_ATOM == atom) return g_strdup ("ATOM");
  else if (GDK_SELECTION_TYPE_BITMAP == atom) return g_strdup ("BITMAP");
  else if (GDK_SELECTION_TYPE_COLORMAP == atom) return g_strdup ("COLORMAP");
  else if (GDK_SELECTION_TYPE_DRAWABLE == atom) return g_strdup ("DRAWABLE");
  else if (GDK_SELECTION_TYPE_INTEGER == atom) return g_strdup ("INTEGER");
  else if (GDK_SELECTION_TYPE_PIXMAP == atom) return g_strdup ("PIXMAP");
  else if (GDK_SELECTION_TYPE_WINDOW == atom) return g_strdup ("WINDOW");
  else if (GDK_SELECTION_TYPE_STRING == atom) return g_strdup ("STRING");

  win32_atom = GPOINTER_TO_UINT (atom);

  if (win32_atom < 0xC000)
    return g_strdup_printf ("#%p", atom);
  else if (GlobalGetAtomName (win32_atom, name, sizeof (name)) == 0)
    return NULL;
  return g_strdup (name);
}

gint
_gdk_win32_window_get_property (GdkWindow   *window,
		  GdkAtom      property,
		  GdkAtom      type,
		  gulong       offset,
		  gulong       length,
		  gint         pdelete,
		  GdkAtom     *actual_property_type,
		  gint        *actual_format_type,
		  gint        *actual_length,
		  guchar     **data)
{
  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (GDK_WINDOW_DESTROYED (window))
    return FALSE;

  g_warning ("gdk_property_get: Not implemented");

  return FALSE;
}

void
_gdk_win32_window_change_property (GdkWindow         *window,
                                   GdkAtom            property,
                                   GdkAtom            type,
                                   gint               format,
                                   GdkPropMode        mode,
                                   const guchar      *data,
                                   gint               nelements)
{
  GdkWin32Selection *win32_sel = _gdk_win32_selection_get ();

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  GDK_NOTE (DND, {
      gchar *prop_name = gdk_atom_name (property);
      gchar *type_name = gdk_atom_name (type);
      gchar *datastring = _gdk_win32_data_to_string (data, MIN (10, format*nelements/8));

      g_print ("gdk_property_change: %p %s %s %s %d*%d bits: %s\n",
	       GDK_WINDOW_HWND (window),
	       prop_name,
	       type_name,
	       (mode == GDK_PROP_MODE_REPLACE ? "REPLACE" :
		(mode == GDK_PROP_MODE_PREPEND ? "PREPEND" :
		 (mode == GDK_PROP_MODE_APPEND ? "APPEND" :
		  "???"))),
	       format, nelements,
	       datastring);
      g_free (datastring);
      g_free (prop_name);
      g_free (type_name);
    });

#ifndef G_DISABLE_CHECKS
  /* We should never come here for these types */
  if (G_UNLIKELY (type == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_COMPOUND_TEXT) ||
                  type == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_SAVE_TARGETS)))
    {
      g_return_if_fail_warning (G_LOG_DOMAIN,
                                G_STRFUNC,
                                "change_property called with a bad type");
      return;
    }
#endif

  if (property == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_GDK_SELECTION) ||
      property == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_OLE2_DND) ||
      property == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_LOCAL_DND_SELECTION))
    {
      _gdk_win32_selection_property_change (win32_sel,
                                            window,
                                            property,
                                            type,
                                            format,
                                            mode,
                                            data,
                                            nelements);
    }
  else
    g_warning ("gdk_property_change: General case not implemented");
}

void
_gdk_win32_window_delete_property (GdkWindow *window,
				   GdkAtom    property)
{
  gchar *prop_name;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  GDK_NOTE (DND, {
      prop_name = gdk_atom_name (property);

      g_print ("gdk_property_delete: %p %s\n",
	       GDK_WINDOW_HWND (window),
	       prop_name);
      g_free (prop_name);
    });

  if (property == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_GDK_SELECTION) ||
      property == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_OLE2_DND))
    _gdk_selection_property_delete (window);
  else if (property == _gdk_win32_selection_atom (GDK_WIN32_ATOM_INDEX_WM_TRANSIENT_FOR))
    {
      GdkScreen *screen;

      screen = gdk_window_get_screen (window);
      gdk_window_set_transient_for (window, gdk_screen_get_root_window (screen));
    }
  else
    {
      prop_name = gdk_atom_name (property);
      g_warning ("gdk_property_delete: General case (%s) not implemented",
		 prop_name);
      g_free (prop_name);
    }
}

static gchar*
_get_system_font_name (HDC hdc)
{
  NONCLIENTMETRICSW ncm;
  PangoFontDescription *font_desc;
  gchar *result, *font_desc_string;
  int logpixelsy;
  gint font_size;

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
_gdk_win32_screen_get_setting (GdkScreen   *screen,
                        const gchar *name,
                        GValue      *value)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  /*
   * XXX : if these values get changed through the Windoze UI the
   *       respective gdk_events are not generated yet.
   */
  if (strcmp ("gtk-double-click-time", name) == 0)
    {
      gint i = GetDoubleClickTime ();
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : %d\n", name, i));
      g_value_set_int (value, i);
      return TRUE;
    }
  else if (strcmp ("gtk-double-click-distance", name) == 0)
    {
      gint i = MAX(GetSystemMetrics (SM_CXDOUBLECLK), GetSystemMetrics (SM_CYDOUBLECLK));
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : %d\n", name, i));
      g_value_set_int (value, i);
      return TRUE;
    }
  else if (strcmp ("gtk-dnd-drag-threshold", name) == 0)
    {
      gint i = MAX(GetSystemMetrics (SM_CXDRAG), GetSystemMetrics (SM_CYDRAG));
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : %d\n", name, i));
      g_value_set_int (value, i);
      return TRUE;
    }
  else if (strcmp ("gtk-split-cursor", name) == 0)
    {
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : FALSE\n", name));
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }
  else if (strcmp ("gtk-alternative-button-order", name) == 0)
    {
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : TRUE\n", name));
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }
  else if (strcmp ("gtk-alternative-sort-arrows", name) == 0)
    {
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : TRUE\n", name));
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }
  else if (strcmp ("gtk-shell-shows-desktop", name) == 0)
    {
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : TRUE\n", name));
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
      BOOL val = TRUE;
      SystemParametersInfoW (SPI_GETFONTSMOOTHING, 0, &val, 0);
      g_value_set_int (value, val ? 1 : 0);

      GDK_NOTE(MISC, g_print ("gdk_screen_get_setting(\"%s\") : %u\n", name, val));
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
      const gchar *rgb_value;
      GdkMonitor *monitor;
      monitor = gdk_display_get_monitor (gdk_display_get_default (), 0);
      rgb_value = _gdk_win32_monitor_get_pixel_structure (monitor);
      g_value_set_static_string (value, rgb_value);

      GDK_NOTE(MISC, g_print ("gdk_screen_get_setting(\"%s\") : %s\n", name, g_value_get_string (value)));
      return TRUE;
    }
  else if (strcmp ("gtk-font-name", name) == 0)
    {
      gchar *font_name = _get_system_font_name (_gdk_display_hdc);

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
  else if (strcmp ("gtk-overlay-scrolling", name) == 0)
    {
      DWORD val = 0;
      DWORD sz = sizeof (val);
      LSTATUS ret = 0;

      ret = RegGetValueW (HKEY_CURRENT_USER, L"Control Panel\\Accessibility", L"DynamicScrollbars", RRF_RT_DWORD, NULL, &val, &sz);
      if (ret == ERROR_SUCCESS)
        {
          g_value_set_boolean (value, val != 0);
          return TRUE;
        }
    }

  return FALSE;
}
