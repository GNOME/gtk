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

#import "GdkMacosWindow.h"

#include "gdkinternals.h"
#include "gdktoplevelprivate.h"

#include "gdkmacosdisplay-private.h"
#include "gdkmacostoplevelsurface-private.h"
#include "gdkmacosutils-private.h"

struct _GdkMacosToplevelSurface
{
  GdkMacosSurface  parent_instance;

  GdkMacosSurface *transient_for;

  guint            decorated : 1;
};

struct _GdkMacosToplevelSurfaceClass
{
  GdkMacosSurfaceClass parent_instance;
};

static void
_gdk_macos_toplevel_surface_fullscreen (GdkMacosToplevelSurface *self)
{
  NSWindow *window;

  g_assert (GDK_IS_MACOS_TOPLEVEL_SURFACE (self));

  window = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (self));

  if (([window styleMask] & NSWindowStyleMaskFullScreen) == 0)
    [window toggleFullScreen:window];
}

static void
_gdk_macos_toplevel_surface_unfullscreen (GdkMacosToplevelSurface *self)
{
  NSWindow *window;

  g_assert (GDK_IS_MACOS_TOPLEVEL_SURFACE (self));

  window = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (self));

  if (([window styleMask] & NSWindowStyleMaskFullScreen) != 0)
    [window toggleFullScreen:window];
}

static void
_gdk_macos_toplevel_surface_maximize (GdkMacosToplevelSurface *self)
{
  NSWindow *window;

  g_assert (GDK_IS_MACOS_TOPLEVEL_SURFACE (self));

  window = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (self));

  if (![window isZoomed])
    [window zoom:window];
}

static void
_gdk_macos_toplevel_surface_unmaximize (GdkMacosToplevelSurface *self)
{
  NSWindow *window;

  g_assert (GDK_IS_MACOS_TOPLEVEL_SURFACE (self));

  window = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (self));

  if ([window isZoomed])
    [window zoom:window];
}

static gboolean
_gdk_macos_toplevel_surface_present (GdkToplevel       *toplevel,
                                     int                width,
                                     int                height,
                                     GdkToplevelLayout *layout)
{
  GdkMacosToplevelSurface *self = (GdkMacosToplevelSurface *)toplevel;
  NSWindow *window = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (self));
  GdkGeometry geometry;
  GdkSurfaceHints mask;

  g_assert (GDK_IS_MACOS_TOPLEVEL_SURFACE (self));
  g_assert (window != NULL);

  if (gdk_toplevel_layout_get_resizable (layout))
    {
      geometry.min_width = gdk_toplevel_layout_get_min_width (layout);
      geometry.min_height = gdk_toplevel_layout_get_min_height (layout);
      mask = GDK_HINT_MIN_SIZE;
    }
  else
    {
      geometry.max_width = geometry.min_width = width;
      geometry.max_height = geometry.min_height = height;
      mask = GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE;
    }

  _gdk_macos_surface_set_geometry_hints (GDK_MACOS_SURFACE (self), &geometry, mask);
  gdk_surface_constrain_size (&geometry, mask, width, height, &width, &height);
  _gdk_macos_surface_resize (GDK_MACOS_SURFACE (self), width, height, -1);

  /* Maximized state */
  if (gdk_toplevel_layout_get_maximized (layout))
    _gdk_macos_toplevel_surface_maximize (self);
  else
    _gdk_macos_toplevel_surface_unmaximize (self);

  /* Fullscreen state */
  if (gdk_toplevel_layout_get_fullscreen (layout))
    _gdk_macos_toplevel_surface_fullscreen (self);
  else
    _gdk_macos_toplevel_surface_unfullscreen (self);

  /* Now present the window */
  [window showAndMakeKey:YES];

  return TRUE;
}

static gboolean
_gdk_macos_toplevel_surface_minimize (GdkToplevel *toplevel)
{
  GdkMacosToplevelSurface *self = (GdkMacosToplevelSurface *)toplevel;
  NSWindow *window = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (self));
  [window miniaturize:window];
  return TRUE;
}

static gboolean
_gdk_macos_toplevel_surface_lower (GdkToplevel *toplevel)
{
  GdkMacosToplevelSurface *self = (GdkMacosToplevelSurface *)toplevel;
  NSWindow *window = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (self));
  [window orderBack:window];
  return TRUE;
}

static void
_gdk_macos_toplevel_surface_focus (GdkToplevel *toplevel,
                                   guint32      timestamp)
{
  GdkMacosToplevelSurface *self = (GdkMacosToplevelSurface *)toplevel;
  NSWindow *window = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (self));
  [window makeKeyAndOrderFront:window];
}

static void
toplevel_iface_init (GdkToplevelInterface *iface)
{
  iface->present = _gdk_macos_toplevel_surface_present;
  iface->minimize = _gdk_macos_toplevel_surface_minimize;
  iface->lower = _gdk_macos_toplevel_surface_lower;
  iface->focus = _gdk_macos_toplevel_surface_focus;
}

G_DEFINE_TYPE_WITH_CODE (GdkMacosToplevelSurface, _gdk_macos_toplevel_surface, GDK_TYPE_MACOS_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_TOPLEVEL, toplevel_iface_init))

enum {
  PROP_0,
  LAST_PROP
};

static void
_gdk_macos_toplevel_surface_set_transient_for (GdkMacosToplevelSurface *self,
                                               GdkMacosSurface         *parent)
{
  g_assert (GDK_IS_MACOS_TOPLEVEL_SURFACE (self));
  g_assert (!parent || GDK_IS_MACOS_SURFACE (parent));

  g_set_object (&self->transient_for, parent);
}

static void
_gdk_macos_toplevel_surface_set_decorated (GdkMacosToplevelSurface *self,
                                           gboolean                 decorated)
{
  g_assert (GDK_IS_MACOS_TOPLEVEL_SURFACE (self));

  decorated = !!decorated;

  if (decorated != self->decorated)
    {
      NSWindow *window = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (self));
      self->decorated = decorated;
      [window setHasShadow:decorated ? YES : NO];
    }
}

static void
_gdk_macos_toplevel_surface_destroy (GdkSurface *surface,
                                     gboolean    foreign_destroy)
{
  GdkMacosToplevelSurface *self = (GdkMacosToplevelSurface *)surface;

  g_clear_object (&self->transient_for);

  GDK_SURFACE_CLASS (_gdk_macos_toplevel_surface_parent_class)->destroy (surface, foreign_destroy);
}

static void
_gdk_macos_toplevel_surface_constructed (GObject *object)
{
  //GdkMacosToplevelSurface *self = (GdkMacosToplevelSurface *)object;

  G_OBJECT_CLASS (_gdk_macos_toplevel_surface_parent_class)->constructed (object);

}

static void
_gdk_macos_toplevel_surface_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);
  GdkMacosSurface *base = GDK_MACOS_SURFACE (surface);
  GdkMacosToplevelSurface *toplevel = GDK_MACOS_TOPLEVEL_SURFACE (base);

  switch (prop_id)
    {
    case LAST_PROP + GDK_TOPLEVEL_PROP_STATE:
      g_value_set_flags (value, surface->state);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TITLE:
      g_value_set_string (value, _gdk_macos_surface_get_title (base));
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_STARTUP_ID:
      g_value_set_string (value, "");
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TRANSIENT_FOR:
      g_value_set_object (value, toplevel->transient_for);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_MODAL:
      g_value_set_boolean (value, _gdk_macos_surface_get_modal_hint (base));
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_ICON_LIST:
      g_value_set_pointer (value, NULL);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DECORATED:
      g_value_set_boolean (value, toplevel->decorated);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DELETABLE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_FULLSCREEN_MODE:
      g_value_set_enum (value, surface->fullscreen_mode);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
      g_value_set_boolean (value, surface->shortcuts_inhibited);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_gdk_macos_toplevel_surface_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);
  GdkMacosSurface *base = GDK_MACOS_SURFACE (surface);
  GdkMacosToplevelSurface *toplevel = GDK_MACOS_TOPLEVEL_SURFACE (base);

  switch (prop_id)
    {
    case LAST_PROP + GDK_TOPLEVEL_PROP_TITLE:
      _gdk_macos_surface_set_title (base, g_value_get_string (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_STARTUP_ID:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_TRANSIENT_FOR:
      _gdk_macos_toplevel_surface_set_transient_for (toplevel, g_value_get_object (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_MODAL:
      _gdk_macos_surface_set_modal_hint (base, g_value_get_boolean (value));
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_ICON_LIST:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DECORATED:
      _gdk_macos_toplevel_surface_set_decorated (toplevel, g_value_get_boolean (value));
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_DELETABLE:
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_FULLSCREEN_MODE:
      surface->fullscreen_mode = g_value_get_enum (value);
      g_object_notify_by_pspec (G_OBJECT (surface), pspec);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_SHORTCUTS_INHIBITED:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_gdk_macos_toplevel_surface_class_init (GdkMacosToplevelSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSurfaceClass *surface_class = GDK_SURFACE_CLASS (klass);

  object_class->constructed = _gdk_macos_toplevel_surface_constructed;
  object_class->get_property = _gdk_macos_toplevel_surface_get_property;
  object_class->set_property = _gdk_macos_toplevel_surface_set_property;

  surface_class->destroy = _gdk_macos_toplevel_surface_destroy;

  gdk_toplevel_install_properties (object_class, LAST_PROP);
}

static void
_gdk_macos_toplevel_surface_init (GdkMacosToplevelSurface *self)
{
  self->decorated = TRUE;
}

GdkMacosSurface *
_gdk_macos_toplevel_surface_new (GdkMacosDisplay *display,
                                 GdkSurface      *parent,
                                 GdkFrameClock   *frame_clock,
                                 int              x,
                                 int              y,
                                 int              width,
                                 int              height)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  GdkMacosWindow *window;
  GdkMacosSurface *self;
  NSScreen *screen;
  NSUInteger style_mask;
  NSRect content_rect;
  NSRect screen_rect;
  int nx;
  int ny;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (display), NULL);
  g_return_val_if_fail (!frame_clock || GDK_IS_FRAME_CLOCK (frame_clock), NULL);
  g_return_val_if_fail (!parent || GDK_IS_MACOS_SURFACE (parent), NULL);

  style_mask = (NSWindowStyleMaskTitled |
                NSWindowStyleMaskClosable |
                NSWindowStyleMaskMiniaturizable |
                NSWindowStyleMaskResizable);

  _gdk_macos_display_to_display_coords (display, x, y, &nx, &ny);

  screen = _gdk_macos_display_get_screen_at_display_coords (display, nx, ny);
  screen_rect = [screen frame];
  nx -= screen_rect.origin.x;
  ny -= screen_rect.origin.y;
  content_rect = NSMakeRect (nx, ny - height, width, height);

  window = [[GdkMacosWindow alloc] initWithContentRect:content_rect
                                             styleMask:style_mask
                                               backing:NSBackingStoreBuffered
                                                 defer:NO
                                                screen:screen];

  self = g_object_new (GDK_TYPE_MACOS_TOPLEVEL_SURFACE,
                       "display", display,
                       "frame-clock", frame_clock,
                       "native", window,
                       NULL);

  GDK_END_MACOS_ALLOC_POOL;

  return g_steal_pointer (&self);
}
