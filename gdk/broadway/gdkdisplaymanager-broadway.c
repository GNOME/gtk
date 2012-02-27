/* GDK - The GIMP Drawing Kit
 * gdkdisplaymanager-broadway.c
 *
 * Copyright (C) 2005 Imendio AB
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

#include "gdkdisplay-broadway.h"
#include "gdkbroadwaydisplaymanager.h"
#include "gdkprivate-broadway.h"

#include "gdkdisplaymanagerprivate.h"
#include "gdkinternals.h"

struct _GdkBroadwayDisplayManager
{
  GdkDisplayManager parent;

  GdkDisplay *default_display;
  GSList *displays;
};

G_DEFINE_TYPE (GdkBroadwayDisplayManager, gdk_broadway_display_manager, GDK_TYPE_DISPLAY_MANAGER)

static GdkDisplay *
gdk_broadway_display_manager_open_display (GdkDisplayManager *manager,
                                         const gchar       *name)
{
  return _gdk_broadway_display_open (name);
}

static GSList *
gdk_broadway_display_manager_list_displays (GdkDisplayManager *manager)
{
  GdkBroadwayDisplayManager *manager_broadway = GDK_BROADWAY_DISPLAY_MANAGER (manager);

  return g_slist_copy (manager_broadway->displays);
}

static GdkDisplay *
gdk_broadway_display_manager_get_default_display (GdkDisplayManager *manager)
{
  return GDK_BROADWAY_DISPLAY_MANAGER (manager)->default_display;
}

static void
gdk_broadway_display_manager_set_default_display (GdkDisplayManager *manager,
                                                GdkDisplay        *display)
{
  GdkBroadwayDisplayManager *manager_broadway = GDK_BROADWAY_DISPLAY_MANAGER (manager);

  manager_broadway->default_display = display;
}

#include "../gdkkeynames.c"

static gchar *
gdk_broadway_display_manager_get_keyval_name (GdkDisplayManager *manager,
                                            guint              keyval)
{
  return _gdk_keyval_name (keyval);
}

static guint
gdk_broadway_display_manager_lookup_keyval (GdkDisplayManager *manager,
                                          const gchar       *name)
{
  return _gdk_keyval_from_name (name);
}

static void
gdk_broadway_display_manager_init (GdkBroadwayDisplayManager *manager)
{
  _gdk_broadway_windowing_init ();
}

static void
gdk_broadway_display_manager_finalize (GObject *object)
{
  g_error ("A GdkBroadwayDisplayManager object was finalized. This should not happen");
  G_OBJECT_CLASS (gdk_broadway_display_manager_parent_class)->finalize (object);
}

static void
gdk_broadway_display_manager_class_init (GdkBroadwayDisplayManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayManagerClass *manager_class = GDK_DISPLAY_MANAGER_CLASS (class);

  object_class->finalize = gdk_broadway_display_manager_finalize;

  manager_class->open_display = gdk_broadway_display_manager_open_display;
  manager_class->list_displays = gdk_broadway_display_manager_list_displays;
  manager_class->set_default_display = gdk_broadway_display_manager_set_default_display;
  manager_class->get_default_display = gdk_broadway_display_manager_get_default_display;
  manager_class->atom_intern = _gdk_broadway_display_manager_atom_intern;
  manager_class->get_atom_name = _gdk_broadway_display_manager_get_atom_name;
  manager_class->lookup_keyval = gdk_broadway_display_manager_lookup_keyval;
  manager_class->get_keyval_name = gdk_broadway_display_manager_get_keyval_name;
}

void
_gdk_broadway_display_manager_add_display (GdkDisplayManager *manager,
					   GdkDisplay        *display)
{
  GdkBroadwayDisplayManager *manager_broadway = GDK_BROADWAY_DISPLAY_MANAGER (manager);

  if (manager_broadway->displays == NULL)
    gdk_display_manager_set_default_display (manager, display);

  manager_broadway->displays = g_slist_prepend (manager_broadway->displays, display);
}

void
_gdk_broadway_display_manager_remove_display (GdkDisplayManager *manager,
					      GdkDisplay        *display)
{
  GdkBroadwayDisplayManager *manager_broadway = GDK_BROADWAY_DISPLAY_MANAGER (manager);

  manager_broadway->displays = g_slist_remove (manager_broadway->displays, display);

  if (manager_broadway->default_display == display)
    {
      if (manager_broadway->displays)
        gdk_display_manager_set_default_display (manager, manager_broadway->displays->data);
      else
        gdk_display_manager_set_default_display (manager, NULL);
    }
}
