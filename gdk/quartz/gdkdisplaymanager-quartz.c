/* GDK - The GIMP Drawing Kit
 * gdkdisplaymanager-quartz.c
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

#include <dlfcn.h>
#include <ApplicationServices/ApplicationServices.h>

#include "gdkquartzdisplay.h"
#include "gdkquartzdisplaymanager.h"
#include "gdkprivate-quartz.h"

#include "gdkdisplaymanagerprivate.h"
#include "gdkinternals.h"

struct _GdkQuartzDisplayManager
{
  GdkDisplayManager parent;

  GdkDisplay *default_display;
  GSList *displays;
};


G_DEFINE_TYPE (GdkQuartzDisplayManager, gdk_quartz_display_manager, GDK_TYPE_DISPLAY_MANAGER)

static GdkDisplay *
gdk_quartz_display_manager_open_display (GdkDisplayManager *manager,
                                         const gchar       *name)
{
  return _gdk_quartz_display_open (name);
}

static GSList *
gdk_quartz_display_manager_list_displays (GdkDisplayManager *manager)
{
  GdkQuartzDisplayManager *manager_quartz = GDK_QUARTZ_DISPLAY_MANAGER (manager);

  return g_slist_copy (manager_quartz->displays);
}

static GdkDisplay *
gdk_quartz_display_manager_get_default_display (GdkDisplayManager *manager)
{
  return GDK_QUARTZ_DISPLAY_MANAGER (manager)->default_display;
}

static void
gdk_quartz_display_manager_set_default_display (GdkDisplayManager *manager,
                                                GdkDisplay        *display)
{
  GdkQuartzDisplayManager *manager_quartz = GDK_QUARTZ_DISPLAY_MANAGER (manager);

  manager_quartz->default_display = display;
}

#include "../gdkkeynames.c"

static gchar *
gdk_quartz_display_manager_get_keyval_name (GdkDisplayManager *manager,
                                            guint              keyval)
{
  return _gdk_keyval_name (keyval);
}

static guint
gdk_quartz_display_manager_lookup_keyval (GdkDisplayManager *manager,
                                          const gchar       *name)
{
  return _gdk_keyval_from_name (name);
}

static void
gdk_quartz_display_manager_init (GdkQuartzDisplayManager *manager)
{
  ProcessSerialNumber psn = { 0, kCurrentProcess };
  void (*_gtk_quartz_framework_init_ptr) (void);

  /* Make the current process a foreground application, i.e. an app
   * with a user interface, in case we're not running from a .app bundle
   */
  TransformProcessType (&psn, kProcessTransformToForegroundApplication);

  /* Initialize GTK+ framework if there is one. */
  _gtk_quartz_framework_init_ptr = dlsym (RTLD_DEFAULT, "_gtk_quartz_framework_init");
  if (_gtk_quartz_framework_init_ptr)
    _gtk_quartz_framework_init_ptr ();
}

static void
gdk_quartz_display_manager_finalize (GObject *object)
{
  g_error ("A GdkQuartzDisplayManager object was finalized. This should not happen");
  G_OBJECT_CLASS (gdk_quartz_display_manager_parent_class)->finalize (object);
}

static void
gdk_quartz_display_manager_class_init (GdkQuartzDisplayManagerClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayManagerClass *manager_class = GDK_DISPLAY_MANAGER_CLASS (class);

  object_class->finalize = gdk_quartz_display_manager_finalize;

  manager_class->open_display = gdk_quartz_display_manager_open_display;
  manager_class->list_displays = gdk_quartz_display_manager_list_displays;
  manager_class->set_default_display = gdk_quartz_display_manager_set_default_display;
  manager_class->get_default_display = gdk_quartz_display_manager_get_default_display;
  manager_class->atom_intern = _gdk_quartz_display_manager_atom_intern;
  manager_class->get_atom_name = _gdk_quartz_display_manager_get_atom_name;
  manager_class->lookup_keyval = gdk_quartz_display_manager_lookup_keyval;
  manager_class->get_keyval_name = gdk_quartz_display_manager_get_keyval_name;
}

void
_gdk_quartz_display_manager_add_display (GdkDisplayManager *manager,
                                         GdkDisplay        *display)
{
  GdkQuartzDisplayManager *manager_quartz = GDK_QUARTZ_DISPLAY_MANAGER (manager);

  if (manager_quartz->displays == NULL)
    gdk_display_manager_set_default_display (manager, display);

  manager_quartz->displays = g_slist_prepend (manager_quartz->displays, display);
}

void
_gdk_quartz_display_manager_remove_display (GdkDisplayManager *manager,
                                            GdkDisplay        *display)
{
  GdkQuartzDisplayManager *manager_quartz = GDK_QUARTZ_DISPLAY_MANAGER (manager);

  manager_quartz->displays = g_slist_remove (manager_quartz->displays, display);

  if (manager_quartz->default_display == display)
    {
      if (manager_quartz->displays)
        gdk_display_manager_set_default_display (manager, manager_quartz->displays->data);
      else
        gdk_display_manager_set_default_display (manager, NULL);
    }
}
