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

#include "gtk/gtk.h"

#include "msw_style.h"
#include "msw_rc_style.h"
#include "xp_theme.h"

#ifndef WM_THEMECHANGED
#define WM_THEMECHANGED 0x031A	/* winxp only */
#endif

static GModule *this_module = NULL;
static void (*msw_rc_reset_styles) (GtkSettings *settings) = NULL;
static GdkWindow *hidden_msg_window = NULL;

static GdkWindow *
create_hidden_msg_window (void)
{
  GdkWindowAttr attributes;
  gint attributes_mask;

  attributes.x = -100;
  attributes.y = -100;
  attributes.width = 10;
  attributes.height = 10;
  attributes.window_type = GDK_WINDOW_TEMP;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.override_redirect = TRUE;
  attributes.event_mask = 0;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_NOREDIR;

  return gdk_window_new (gdk_get_default_root_window (),
			 &attributes, attributes_mask);
}

static GdkFilterReturn
global_filter_func (void *xevent, GdkEvent *event, gpointer data)
{
  MSG *msg = (MSG *) xevent;

  switch (msg->message)
    {
      /* catch theme changes */
    case WM_THEMECHANGED:
    case WM_SYSCOLORCHANGE:

      if (msw_rc_reset_styles != NULL)
	{
	  xp_theme_reset ();
	  msw_style_init ();

	  /* force all gtkwidgets to redraw */
	  (*msw_rc_reset_styles) (gtk_settings_get_default ());
	}

      return GDK_FILTER_REMOVE;

    case WM_SETTINGCHANGE:
      /* catch cursor blink, etc... changes */
      msw_style_setup_system_settings ();
      return GDK_FILTER_REMOVE;

    default:
      return GDK_FILTER_CONTINUE;
    }
}

G_MODULE_EXPORT void
theme_init (GTypeModule * module)
{
  msw_rc_style_register_type (module);
  msw_style_register_type (module);

  /* this craziness is required because only gtk 2.4.x and later have
     gtk_rc_reset_styles(). But we want to be able to run acceptly well on
     any GTK 2.x.x platform. */
  if (gtk_check_version (2, 4, 0) == NULL)
    {
      this_module = g_module_open (NULL, 0);

      if (this_module)
	g_module_symbol (this_module, "gtk_rc_reset_styles",
			 (gpointer *) (&msw_rc_reset_styles));
    }

  msw_style_init ();
  hidden_msg_window = create_hidden_msg_window ();
  gdk_window_add_filter (hidden_msg_window, global_filter_func, NULL);
}

G_MODULE_EXPORT void
theme_exit (void)
{
  gdk_window_remove_filter (hidden_msg_window, global_filter_func, NULL);
  gdk_window_destroy (hidden_msg_window);
  hidden_msg_window = NULL;
  msw_style_finalize ();

  if (this_module)
    {
      g_module_close (this_module);
      this_module = NULL;
    }
}

G_MODULE_EXPORT GtkRcStyle *
theme_create_rc_style (void)
{
  return g_object_new (MSW_TYPE_RC_STYLE, NULL);
}

/* The following function will be called by GTK+ when the module
 * is loaded and checks to see if we are compatible with the
 * version of GTK+ that loads us.
 */
G_MODULE_EXPORT const gchar *
g_module_check_init (GModule *module)
{
  return gtk_check_version (2, 0, 0);
}
