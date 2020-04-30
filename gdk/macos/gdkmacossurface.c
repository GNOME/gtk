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
#include <float.h>
#include <gdk/gdk.h>

#import "GdkMacosWindow.h"

#include "gdkframeclockidleprivate.h"
#include "gdkinternals.h"
#include "gdksurfaceprivate.h"

#include "gdkmacosdisplay-private.h"
#include "gdkmacosdragsurface-private.h"
#include "gdkmacospopupsurface-private.h"
#include "gdkmacossurface-private.h"
#include "gdkmacostoplevelsurface-private.h"
#include "gdkmacosutils-private.h"

typedef struct
{
  GdkMacosWindow *window;

  char *title;

  int shadow_top;
  int shadow_right;
  int shadow_bottom;
  int shadow_left;

  int scale;

  guint modal_hint : 1;
} GdkMacosSurfacePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GdkMacosSurface, gdk_macos_surface, GDK_TYPE_SURFACE)

enum {
  PROP_0,
  PROP_NATIVE,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static gboolean
window_is_fullscreen (GdkMacosSurface *self)
{
  GdkMacosSurfacePrivate *priv = gdk_macos_surface_get_instance_private (self);

  g_assert (GDK_IS_MACOS_SURFACE (self));

  return ([priv->window styleMask] & NSWindowStyleMaskFullScreen) != 0;
}

static void
gdk_macos_surface_set_input_region (GdkSurface     *surface,
                                    cairo_region_t *region)
{
  g_assert (GDK_IS_MACOS_SURFACE (surface));

  /* TODO: */
}

static void
gdk_macos_surface_hide (GdkSurface *surface)
{
  GdkMacosSurface *self = (GdkMacosSurface *)surface;
  GdkMacosSurfacePrivate *priv = gdk_macos_surface_get_instance_private (self);

  g_assert (GDK_IS_MACOS_SURFACE (self));

  [priv->window hide];
}

static gint
gdk_macos_surface_get_scale_factor (GdkSurface *surface)
{
  GdkMacosSurface *self = (GdkMacosSurface *)surface;
  GdkMacosSurfacePrivate *priv = gdk_macos_surface_get_instance_private (self);

  g_assert (GDK_IS_MACOS_SURFACE (self));

  return [priv->window backingScaleFactor];
}

static void
gdk_macos_surface_destroy (GdkSurface *surface,
                           gboolean    foreign_destroy)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  GdkMacosSurface *self = (GdkMacosSurface *)surface;
  GdkMacosSurfacePrivate *priv = gdk_macos_surface_get_instance_private (self);
  GdkMacosWindow *window = g_steal_pointer (&priv->window);

  g_clear_pointer (&priv->title, g_free);

  if (window != NULL)
    [window close];

  GDK_END_MACOS_ALLOC_POOL;
}


static void
gdk_macos_surface_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GdkMacosSurface *self = GDK_MACOS_SURFACE (object);
  GdkMacosSurfacePrivate *priv = gdk_macos_surface_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_NATIVE:
      g_value_set_pointer (value, priv->window);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_macos_surface_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GdkMacosSurface *self = GDK_MACOS_SURFACE (object);
  GdkMacosSurfacePrivate *priv = gdk_macos_surface_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_NATIVE:
      priv->window = g_value_get_pointer (value);
      [priv->window setGdkSurface:self];
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gdk_macos_surface_class_init (GdkMacosSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSurfaceClass *surface_class = GDK_SURFACE_CLASS (klass);

  object_class->get_property = gdk_macos_surface_get_property;
  object_class->set_property = gdk_macos_surface_set_property;

  surface_class->destroy = gdk_macos_surface_destroy;
  surface_class->hide = gdk_macos_surface_hide;
  surface_class->set_input_region = gdk_macos_surface_set_input_region;
  surface_class->get_scale_factor = gdk_macos_surface_get_scale_factor;

  properties [PROP_NATIVE] =
    g_param_spec_pointer ("native",
                          "Native",
                          "The native NSWindow",
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
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
      break;

    case GDK_SURFACE_POPUP:
      ret = _gdk_macos_popup_surface_new (display, parent, frame_clock, x, y, width, height);
      break;

    case GDK_SURFACE_TEMP:
      ret = _gdk_macos_drag_surface_new (display, parent, frame_clock, x, y, width, height);
      break;

    default:
      g_warn_if_reached ();
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

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  if (title == NULL)
    title = "";

  if (g_strcmp0 (priv->title, title) != 0)
    {
      g_free (priv->title);
      priv->title = g_strdup (title);

      GDK_BEGIN_MACOS_ALLOC_POOL;
      [priv->window setTitle:[NSString stringWithUTF8String:title]];
      GDK_END_MACOS_ALLOC_POOL;
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
  GdkMacosSurfacePrivate *priv = gdk_macos_surface_get_instance_private (self);

  g_return_val_if_fail (GDK_IS_MACOS_SURFACE (self), (CGDirectDisplayID)-1);

  if (priv->window != NULL)
    {
      NSScreen *screen = [priv->window screen];
      return [[[screen deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue];
    }

  return (CGDirectDisplayID)-1;
}

NSWindow *
_gdk_macos_surface_get_native (GdkMacosSurface *self)
{
  GdkMacosSurfacePrivate *priv = gdk_macos_surface_get_instance_private (self);

  g_return_val_if_fail (GDK_IS_MACOS_SURFACE (self), NULL);

  return (NSWindow *)priv->window;
}

void
_gdk_macos_surface_set_geometry_hints (GdkMacosSurface   *self,
                                       const GdkGeometry *geometry,
                                       GdkSurfaceHints    geom_mask)
{
  GdkMacosSurfacePrivate *priv = gdk_macos_surface_get_instance_private (self);
  NSSize max_size;
  NSSize min_size;

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));
  g_return_if_fail (geometry != NULL);
  g_return_if_fail (priv->window != NULL);

  if (geom_mask & GDK_HINT_MAX_SIZE)
    max_size = NSMakeSize (geometry->max_width, geometry->max_height);
  else
    max_size = NSMakeSize (FLT_MAX, FLT_MAX);

  if (geom_mask & GDK_HINT_MIN_SIZE)
    min_size = NSMakeSize (geometry->min_width, geometry->min_height);
  else
    min_size = NSMakeSize (0, 0);

  [priv->window setMaxSize:max_size];
  [priv->window setMinSize:min_size];
}

void
_gdk_macos_surface_resize (GdkMacosSurface *self,
                           int              width,
                           int              height,
                           int              scale)
{
  GdkMacosSurfacePrivate *priv = gdk_macos_surface_get_instance_private (self);
  GdkSurface *surface = (GdkSurface *)self;
  GdkDisplay *display;
  NSRect content_rect;
  NSRect frame_rect;
  int gx;
  int gy;

  g_return_if_fail (GDK_IS_MACOS_SURFACE (surface));

  display = gdk_surface_get_display (surface);
  _gdk_macos_display_to_display_coords (GDK_MACOS_DISPLAY (display),
                                        surface->x,
                                        surface->y + surface->height,
                                        &gx,
                                        &gy);
  content_rect = NSMakeRect (gx, gy, width, height);
  frame_rect = [priv->window frameRectForContentRect:content_rect];
  [priv->window setFrame:frame_rect display:YES];
}

void
_gdk_macos_surface_update_fullscreen_state (GdkMacosSurface *self)
{
  GdkSurfaceState state;
  gboolean is_fullscreen;
  gboolean was_fullscreen;

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  state = GDK_SURFACE (self)->state;
  is_fullscreen = window_is_fullscreen (self);
  was_fullscreen = (state & GDK_SURFACE_STATE_FULLSCREEN) != 0;

  if (is_fullscreen != was_fullscreen)
    {
      if (is_fullscreen)
        gdk_synthesize_surface_state (GDK_SURFACE (self), 0, GDK_SURFACE_STATE_FULLSCREEN);
      else
        gdk_synthesize_surface_state (GDK_SURFACE (self), GDK_SURFACE_STATE_FULLSCREEN, 0);
    }
}
