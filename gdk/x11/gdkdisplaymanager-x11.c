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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gdkdisplay-x11.h"
#include "gdkprivate-x11.h"

#include "gdkdisplaymanagerprivate.h"
#include "gdkinternals.h"

#define GDK_TYPE_DISPLAY_MANAGER_X11    (gdk_display_manager_x11_get_type ())
#define GDK_DISPLAY_MANAGER_X11(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DISPLAY_MANAGER_X11, GdkDisplayManagerX11))

typedef struct _GdkDisplayManagerX11 GdkDisplayManagerX11;
typedef struct _GdkDisplayManagerClass GdkDisplayManagerX11Class;

struct _GdkDisplayManagerX11
{
  GdkDisplayManager parent;

  GdkDisplay *default_display;
  GSList *displays;
};

G_DEFINE_TYPE (GdkDisplayManagerX11, gdk_display_manager_x11, GDK_TYPE_DISPLAY_MANAGER)

static GdkDisplay *
gdk_display_manager_x11_open_display (GdkDisplayManager *manager,
                                      const gchar       *name)
{
  return _gdk_x11_display_open (name);
}

static GSList *
gdk_display_manager_x11_list_displays (GdkDisplayManager *manager)
{
  GdkDisplayManagerX11 *manager_x11 = GDK_DISPLAY_MANAGER_X11 (manager);

  return g_slist_copy (manager_x11->displays);
}

static GdkDisplay *
gdk_display_manager_x11_get_default_display (GdkDisplayManager *manager)
{
  return GDK_DISPLAY_MANAGER_X11 (manager)->default_display;
}

static void
gdk_display_manager_x11_set_default_display (GdkDisplayManager *manager,
                                             GdkDisplay        *display)
{
  GdkDisplayManagerX11 *manager_x11 = GDK_DISPLAY_MANAGER_X11 (manager);

  manager_x11->default_display = display;

  _gdk_x11_display_make_default (display);
}

static void
gdk_display_manager_x11_init (GdkDisplayManagerX11 *manager)
{
  _gdk_x11_windowing_init ();
}

static void
gdk_display_manager_x11_finalize (GObject *object)
{
  g_error ("A GdkDisplayManagerX11 object was finalized. This should not happen");
  G_OBJECT_CLASS (gdk_display_manager_x11_parent_class)->finalize (object);
}

static void
gdk_display_manager_x11_class_init (GdkDisplayManagerX11Class *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayManagerClass *manager_class = GDK_DISPLAY_MANAGER_CLASS (class);

  object_class->finalize = gdk_display_manager_x11_finalize;

  manager_class->open_display = gdk_display_manager_x11_open_display;
  manager_class->list_displays = gdk_display_manager_x11_list_displays;
  manager_class->set_default_display = gdk_display_manager_x11_set_default_display;
  manager_class->get_default_display = gdk_display_manager_x11_get_default_display;
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
  GdkDisplayManagerX11 *manager_x11 = GDK_DISPLAY_MANAGER_X11 (manager);

  if (manager_x11->displays == NULL)
    gdk_display_manager_set_default_display (manager, display);

  manager_x11->displays = g_slist_prepend (manager_x11->displays, display);
}

void
_gdk_x11_display_manager_remove_display (GdkDisplayManager *manager,
                                         GdkDisplay        *display)
{
  GdkDisplayManagerX11 *manager_x11 = GDK_DISPLAY_MANAGER_X11 (manager);

  manager_x11->displays = g_slist_remove (manager_x11->displays, display);

  if (manager_x11->default_display == display)
    {
      if (manager_x11->displays)
        gdk_display_manager_set_default_display (manager, manager_x11->displays->data);
      else
        gdk_display_manager_set_default_display (manager, NULL);
    }
}
