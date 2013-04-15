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

#include <stdlib.h>
#include "gdkdisplay-broadway.h"
#include "gdkbroadwaydisplaymanager.h"
#include "gdkprivate-broadway.h"

#include "gdkdisplaymanagerprivate.h"
#include "gdkinternals.h"

struct _GdkBroadwayDisplayManager
{
  GdkDisplayManager parent;

  gboolean init_failed;
};

static void g_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkBroadwayDisplayManager, gdk_broadway_display_manager, GDK_TYPE_DISPLAY_MANAGER,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, g_initable_iface_init))

static gboolean
gdk_broadway_display_manager_initable_init (GInitable     *initable,
                                            GCancellable  *cancellable,
                                            GError       **error)
{
  const gchar *display_name;
  gint port;
  GdkBroadwayServer *server;

  display_name = g_getenv ("BROADWAY_DISPLAY");

  port = 0;
  if (display_name != NULL)
    {
      if (*display_name == ':')
        display_name++;
      port = strtol(display_name, NULL, 10);
    }
  if (port == 0)
    port = 1;

  server = _gdk_broadway_server_new (port, NULL);
  if (server == NULL)
    {
      GDK_BROADWAY_DISPLAY_MANAGER (initable)->init_failed = TRUE;
      return FALSE;
    }

  g_object_unref (server);

  return TRUE;
}

void
g_initable_iface_init (GInitableIface *iface)
{
  iface->init = gdk_broadway_display_manager_initable_init;
}

static GdkDisplay *
gdk_broadway_display_manager_open_display (GdkDisplayManager *manager,
                                         const gchar       *name)
{
  return _gdk_broadway_display_open (name);
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
  if (!GDK_BROADWAY_DISPLAY_MANAGER (object)->init_failed)
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
  manager_class->lookup_keyval = gdk_broadway_display_manager_lookup_keyval;
  manager_class->get_keyval_name = gdk_broadway_display_manager_get_keyval_name;
}
