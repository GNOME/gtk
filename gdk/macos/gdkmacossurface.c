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

#include <AppKit/AppKit.h>
#include <gdk/gdk.h>

#include "gdkframeclockidleprivate.h"
#include "gdksurfaceprivate.h"

#include "gdkmacosdragsurface-private.h"
#include "gdkmacospopupsurface-private.h"
#include "gdkmacossurface-private.h"
#include "gdkmacostoplevelsurface-private.h"

typedef struct
{
  char *title;

  gint shadow_top;
  gint shadow_right;
  gint shadow_bottom;
  gint shadow_left;

  guint modal_hint : 1;
} GdkMacosSurfacePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdkMacosSurface, gdk_macos_surface, GDK_TYPE_SURFACE)

static void
gdk_macos_surface_set_input_region (GdkSurface     *surface,
                                    cairo_region_t *region)
{
  g_assert (GDK_IS_MACOS_SURFACE (surface));

  /* TODO: */
}

static void
gdk_macos_surface_finalize (GObject *object)
{
  GdkMacosSurface *self = (GdkMacosSurface *)object;
  GdkMacosSurfacePrivate *priv = gdk_macos_surface_get_instance_private (self);

  g_clear_pointer (&priv->title, g_free);

  G_OBJECT_CLASS (gdk_macos_surface_parent_class)->finalize (object);
}

static void
gdk_macos_surface_class_init (GdkMacosSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSurfaceClass *surface_class = GDK_SURFACE_CLASS (klass);

  object_class->finalize = gdk_macos_surface_finalize;

  surface_class->set_input_region = gdk_macos_surface_set_input_region;
}

static void
gdk_macos_surface_init (GdkMacosSurface *self)
{
}

GdkMacosSurface *
_gdk_macos_surface_new (GdkMacosDisplay   *display,
                        GdkSurfaceType     surface_type,
                        GdkSurface        *parent,
                        int                x,
                        int                y,
                        int                width,
                        int                height)
{
  GdkFrameClock *frame_clock;
  GdkMacosSurface *ret;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (display), NULL);

  if (parent != NULL)
    frame_clock = g_object_ref (gdk_surface_get_frame_clock (parent));
  else
    frame_clock = _gdk_frame_clock_idle_new ();

  switch (surface_type)
    {
    case GDK_SURFACE_TOPLEVEL:
      ret = _gdk_macos_toplevel_surface_new (display, parent, frame_clock, x, y, width, height);

    case GDK_SURFACE_POPUP:
      ret = _gdk_macos_popup_surface_new (display, parent, frame_clock, x, y, width, height);

    case GDK_SURFACE_TEMP:
      ret = _gdk_macos_drag_surface_new (display, parent, frame_clock, x, y, width, height);

    default:
      ret = NULL;
    }

  g_object_unref (frame_clock);

  return g_steal_pointer (&ret);
}

void
_gdk_macos_surface_get_shadow (GdkMacosSurface *self,
                               gint            *top,
                               gint            *right,
                               gint            *bottom,
                               gint            *left)
{
  GdkMacosSurfacePrivate *priv = gdk_macos_surface_get_instance_private (self);

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  if (top)
    *top = priv->shadow_top;

  if (left)
    *left = priv->shadow_left;

  if (bottom)
    *bottom = priv->shadow_bottom;

  if (right)
    *right = priv->shadow_right;
}

const char *
_gdk_macos_surface_get_title (GdkMacosSurface *self)
{
  GdkMacosSurfacePrivate *priv = gdk_macos_surface_get_instance_private (self);

  return priv->title;
}

void
_gdk_macos_surface_set_title (GdkMacosSurface *self,
                              const gchar     *title)
{
  GdkMacosSurfacePrivate *priv = gdk_macos_surface_get_instance_private (self);

  if (g_strcmp0 (priv->title, title) != 0)
    {
      g_free (priv->title);
      priv->title = g_strdup (title);
    }
}

gboolean
_gdk_macos_surface_get_modal_hint (GdkMacosSurface *self)
{
  GdkMacosSurfacePrivate *priv = gdk_macos_surface_get_instance_private (self);

  g_return_val_if_fail (GDK_IS_MACOS_SURFACE (self), FALSE);

  return priv->modal_hint;
}

void
_gdk_macos_surface_set_modal_hint (GdkMacosSurface *self,
                                   gboolean         modal_hint)
{
  GdkMacosSurfacePrivate *priv = gdk_macos_surface_get_instance_private (self);

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  priv->modal_hint = !!modal_hint;
}

CGDirectDisplayID
_gdk_macos_surface_get_screen_id (GdkMacosSurface *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_SURFACE (self), 0);

  return GDK_MACOS_SURFACE_GET_CLASS (self)->get_screen_id (self);
}
