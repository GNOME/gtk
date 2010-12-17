/* GDK - The GIMP Drawing Kit
 * gdkdisplaymanager-quartz.c
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

#include "gdkdisplay-quartz.h"
#include "gdkprivate-quartz.h"

#include "gdkdisplaymanagerprivate.h"
#include "gdkinternals.h"

#define GDK_TYPE_DISPLAY_MANAGER_QUARTZ    (gdk_display_manager_quartz_get_type ())
#define GDK_DISPLAY_MANAGER_QUARTZ(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DISPLAY_MANAGER_QUARTZ, GdkDisplayManagerQuartz))

typedef struct _GdkDisplayManagerQuartz GdkDisplayManagerQuartz;
typedef struct _GdkDisplayManagerClass GdkDisplayManagerQuartzClass;

struct _GdkDisplayManagerQuartz
{
  GdkDisplayManager parent;

  GdkDisplay *default_display;
  GSList *displays;
};

G_DEFINE_TYPE (GdkDisplayManagerQuartz, gdk_display_manager_quartz, GDK_TYPE_DISPLAY_MANAGER)

static GdkDisplay *
gdk_display_manager_quartz_open_display (GdkDisplayManager *manager,
                                      const gchar       *name)
{
  return _gdk_quartz_display_open (name);
}

static GSList *
gdk_display_manager_quartz_list_displays (GdkDisplayManager *manager)
{
  GdkDisplayManagerQuartz *manager_quartz = GDK_DISPLAY_MANAGER_QUARTZ (manager);

  return g_slist_copy (manager_quartz->displays);
}

static GdkDisplay *
gdk_display_manager_quartz_get_default_display (GdkDisplayManager *manager)
{
  return GDK_DISPLAY_MANAGER_QUARTZ (manager)->default_display;
}

static void
gdk_display_manager_quartz_set_default_display (GdkDisplayManager *manager,
                                                GdkDisplay        *display)
{
  GdkDisplayManagerQuartz *manager_quartz = GDK_DISPLAY_MANAGER_QUARTZ (manager);

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
gdk_quartz_display_manager_keyval_convert_case (GdkDisplayManager *manager,
                                                guint              symbol,
                                                guint             *lower,
                                                guint             *upper)
{
  /* FIXME implement this */
  if (lower)
    *lower = symbol;
  if (upper)
    *upper = symbol;
}

static void
gdk_display_manager_quartz_init (GdkDisplayManagerQuartz *manager)
{
  _gdk_quartz_windowing_init ();
}

static void
gdk_display_manager_quartz_finalize (GObject *object)
{
  g_error ("A GdkDisplayManagerQuartz object was finalized. This should not happen");
  G_OBJECT_CLASS (gdk_display_manager_quartz_parent_class)->finalize (object);
}

static void
gdk_display_manager_quartz_class_init (GdkDisplayManagerQuartzClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayManagerClass *manager_class = GDK_DISPLAY_MANAGER_CLASS (class);

  object_class->finalize = gdk_display_manager_quartz_finalize;

  manager_class->open_display = gdk_display_manager_quartz_open_display;
  manager_class->list_displays = gdk_display_manager_quartz_list_displays;
  manager_class->set_default_display = gdk_display_manager_quartz_set_default_display;
  manager_class->get_default_display = gdk_display_manager_quartz_get_default_display;
  manager_class->atom_intern = _gdk_quartz_display_manager_atom_intern;
  manager_class->get_atom_name = _gdk_quartz_display_manager_get_atom_name;
  manager_class->lookup_keyval = gdk_quartz_display_manager_lookup_keyval;
  manager_class->get_keyval_name = gdk_quartz_display_manager_get_keyval_name;
  manager_class->keyval_convert_case = gdk_quartz_display_manager_keyval_convert_case;
}

void
_gdk_quartz_display_manager_add_display (GdkDisplayManager *manager,
                                         GdkDisplay        *display)
{
  GdkDisplayManagerQuartz *manager_quartz = GDK_DISPLAY_MANAGER_QUARTZ (manager);

  if (manager_quartz->displays == NULL)
    gdk_display_manager_set_default_display (manager, display);

  manager_quartz->displays = g_slist_prepend (manager_quartz->displays, display);
}

void
_gdk_quartz_display_manager_remove_display (GdkDisplayManager *manager,
                                            GdkDisplay        *display)
{
  GdkDisplayManagerQuartz *manager_quartz = GDK_DISPLAY_MANAGER_QUARTZ (manager);

  manager_quartz->displays = g_slist_remove (manager_quartz->displays, display);

  if (manager_quartz->default_display == display)
    {
      if (manager_quartz->displays)
        gdk_display_manager_set_default_display (manager, manager_quartz->displays->data);
      else
        gdk_display_manager_set_default_display (manager, NULL);
    }
}
