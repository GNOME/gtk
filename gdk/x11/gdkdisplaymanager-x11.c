/* GDK - The GIMP Drawing Kit
 * gdkdisplaymanager-x11.c
 *
 * Copyright 2010 Red Hat, Inc.
 *
 * Author: Matthias clasen
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkx11displaymanager.h"
#include "gdkdisplaymanagerprivate.h"
#include "gdkdisplay-x11.h"
#include "gdkprivate-x11.h"

#include "gdkinternals.h"

struct _GdkX11DisplayManager
{
  GdkDisplayManager parent;

  GdkDisplay *default_display;
  GSList *displays;
};

struct _GdkX11DisplayManagerClass
{
  GdkDisplayManagerClass parent_class;
};

G_DEFINE_TYPE (GdkX11DisplayManager, gdk_x11_display_manager, GDK_TYPE_DISPLAY_MANAGER)

static GdkDisplay *
gdk_x11_display_manager_open_display (GdkDisplayManager *manager,
                                      const gchar       *name)
{
  GdkX11DisplayManager *manager_x11 = GDK_X11_DISPLAY_MANAGER (manager);
  GdkDisplay *display;

  display = _gdk_x11_display_open (name);
  if (display != NULL)
    {
      if (manager_x11->default_display == NULL)
        gdk_display_manager_set_default_display (manager, display);

      g_signal_emit_by_name (manager, "display-opened", display);
    }

  return display;
}

static GSList *
gdk_x11_display_manager_list_displays (GdkDisplayManager *manager)
{
  return g_slist_copy (GDK_X11_DISPLAY_MANAGER (manager)->displays);
}

static GdkDisplay *
gdk_x11_display_manager_get_default_display (GdkDisplayManager *manager)
{
  return GDK_X11_DISPLAY_MANAGER (manager)->default_display;
}

static void
gdk_x11_display_manager_set_default_display (GdkDisplayManager *manager,
                                             GdkDisplay        *display)
{
  GDK_X11_DISPLAY_MANAGER (manager)->default_display = display;

  if (display)
    _gdk_x11_display_make_default (display);
}

static void
gdk_x11_display_manager_init (GdkX11DisplayManager *manager)
{
  _gdk_x11_windowing_init ();
}

static void
gdk_x11_display_manager_finalize (GObject *object)
{
  g_error ("A GdkX11DisplayManager object was finalized. This should not happen");
  G_OBJECT_CLASS (gdk_x11_display_manager_parent_class)->finalize (object);
}

static void
gdk_x11_display_manager_class_init (GdkX11DisplayManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayManagerClass *manager_class = GDK_DISPLAY_MANAGER_CLASS (class);

  object_class->finalize = gdk_x11_display_manager_finalize;

  manager_class->open_display = gdk_x11_display_manager_open_display;
  manager_class->list_displays = gdk_x11_display_manager_list_displays;
  manager_class->set_default_display = gdk_x11_display_manager_set_default_display;
  manager_class->get_default_display = gdk_x11_display_manager_get_default_display;
  manager_class->atom_intern = _gdk_x11_display_manager_atom_intern;
  manager_class->get_atom_name = _gdk_x11_display_manager_get_atom_name;
  manager_class->lookup_keyval = _gdk_x11_display_manager_lookup_keyval;
  manager_class->get_keyval_name = _gdk_x11_display_manager_get_keyval_name;
  manager_class->keyval_convert_case = _gdk_x11_display_manager_keyval_convert_case;
}

void
_gdk_x11_display_manager_add_display (GdkDisplayManager *manager,
                                      GdkDisplay        *display)
{
  GdkX11DisplayManager *manager_x11 = GDK_X11_DISPLAY_MANAGER (manager);

  manager_x11->displays = g_slist_prepend (manager_x11->displays, display);
}

void
_gdk_x11_display_manager_remove_display (GdkDisplayManager *manager,
                                         GdkDisplay        *display)
{
  GdkX11DisplayManager *manager_x11 = GDK_X11_DISPLAY_MANAGER (manager);

  manager_x11->displays = g_slist_remove (manager_x11->displays, display);

  if (manager_x11->default_display == display)
    {
      if (manager_x11->displays)
        gdk_display_manager_set_default_display (manager, manager_x11->displays->data);
      else
        gdk_display_manager_set_default_display (manager, NULL);
    }
}
