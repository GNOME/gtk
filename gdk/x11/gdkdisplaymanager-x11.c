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
};

struct _GdkX11DisplayManagerClass
{
  GdkDisplayManagerClass parent_class;
};

G_DEFINE_TYPE (GdkX11DisplayManager, gdk_x11_display_manager, GDK_TYPE_DISPLAY_MANAGER)

static void
gdk_x11_display_manager_init (GdkX11DisplayManager *manager)
{
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

  object_class->finalize = gdk_x11_display_manager_finalize;
}
