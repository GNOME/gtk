/* MS-Windows Engine (aka GTK-Wimp)
 *
 * Copyright (C) 2003, 2004 Raymond Penners <raymond@dotsphinx.com>
 * Includes code adapted from redmond95 by Owen Taylor, and
 * gtk-nativewin by Evan Martin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <windows.h>
#include <gmodule.h>
#include <gtk/gtk.h>

#include "msw_style.h"
#include "msw_rc_style.h"

#ifndef WM_THEMECHANGED
#define WM_THEMECHANGED 0x031A /* winxp only */
#endif

/* TODO - look into whether we need to handle these:
 *
 * WM_STYLECHANGED
 * WM_PALETTECHANGED
 */
static GdkFilterReturn
global_filter_func (void     *xevent,
		    GdkEvent *event,
		    gpointer  data)
{
  MSG *msg = (MSG *) xevent;

  switch (msg->message)
    {
#if ENABLE_THEME_CHANGING
      /* catch theme changes */
    case WM_THEMECHANGED:
    case WM_SYSCOLORCHANGE:
      xp_theme_reset ();
      msw_style_init ();

      /* force all gtkwidgets to redraw */
      gtk_rc_reparse_all_for_settings (gtk_settings_get_default(), TRUE);
      return GDK_FILTER_REMOVE;
#endif

    case WM_SETTINGCHANGE:
      /* catch cursor blink, etc... changes */
      msw_style_setup_system_settings (); 
      return GDK_FILTER_REMOVE;

    default:
      return GDK_FILTER_CONTINUE;
    }
}

G_MODULE_EXPORT void
theme_init (GTypeModule *module)
{
  msw_rc_style_register_type (module);
  msw_style_register_type (module);
  msw_style_init ();
  gdk_window_add_filter (NULL, global_filter_func, NULL);
}

G_MODULE_EXPORT void
theme_exit (void)
{
  gdk_window_remove_filter (NULL, global_filter_func, NULL);
}

G_MODULE_EXPORT GtkRcStyle *
theme_create_rc_style (void)
{
  return GTK_RC_STYLE (g_object_new (MSW_TYPE_RC_STYLE, NULL));
}

/* The following function will be called by GTK+ when the module
 * is loaded and checks to see if we are compatible with the
 * version of GTK+ that loads us.
 */
G_MODULE_EXPORT const gchar* g_module_check_init (GModule *module);
const gchar*
g_module_check_init (GModule *module)
{
  return gtk_check_version (GTK_MAJOR_VERSION,
			    GTK_MINOR_VERSION,
			    GTK_MICRO_VERSION - GTK_INTERFACE_AGE);
}
