/*
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gdk/gdk.h>
#include <gdk/gdkdisplayprivate.h>

#include "gdkmacos-private.h"
#include "gdkmacosdisplay.h"

/**
 * SECTION:macos_interaction
 * @Short_description: macOS backend-specific functions
 * @Title: macOS Interaction
 * @Include: gdk/macos/gdkmacos.h
 *
 * The functions in this section are specific to the GDK macOS backend.
 * To use them, you need to include the `<gdk/macos/gdkmacos.h>` header and
 * use the macOS-specific pkg-config `gtk4-macos` file to build your
 * application.
 *
 * To make your code compile with other GDK backends, guard backend-specific
 * calls by an ifdef as follows. Since GDK may be built with multiple
 * backends, you should also check for the backend that is in use (e.g. by
 * using the GDK_IS_MACOS_DISPLAY() macro).
 * |[<!-- language="C" -->
 * #ifdef GDK_WINDOWING_MACOS
 *   if (GDK_IS_MACOS_DISPLAY (display))
 *     {
 *       // make macOS-specific calls here
 *     }
 *   else
 * #endif
 * #ifdef GDK_WINDOWING_X11
 *   if (GDK_IS_X11_DISPLAY (display))
 *     {
 *       // make X11-specific calls here
 *     }
 *   else
 * #endif
 *   g_error ("Unsupported GDK backend");
 * ]|
 */

struct _GdkMacosDisplay
{
  GdkDisplay parent_instance;
};

struct _GdkMacosDisplayClass
{
  GdkDisplayClass parent_class;
};

G_DEFINE_TYPE (GdkMacosDisplay, gdk_macos_display, GDK_TYPE_DISPLAY)

static void
gdk_macos_display_finalize (GObject *object)
{
  G_OBJECT_CLASS (gdk_macos_display_parent_class)->finalize (object);
}

static void
gdk_macos_display_class_init (GdkMacosDisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_macos_display_finalize;
}

static void
gdk_macos_display_init (GdkMacosDisplay *self)
{
}

GdkDisplay *
_gdk_macos_display_open (const gchar *name)
{
  GdkDisplay *ret;

  /* Make the current process a foreground application, i.e. an app
   * with a user interface, in case we're not running from a .app bundle
   */
  //TransformProcessType (&psn, kProcessTransformToForegroundApplication);

  ret = g_object_new (GDK_TYPE_MACOS_DISPLAY, NULL);

  return g_steal_pointer (&ret);
}
