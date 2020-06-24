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
  GdkMacosSurface parent_instance;
  guint           decorated : 1;
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
  NSWindow *nswindow = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (self));
  GdkGeometry geometry;
  GdkSurfaceHints mask;
  NSWindowStyleMask style_mask;

  g_assert (GDK_IS_MACOS_TOPLEVEL_SURFACE (self));
  g_assert (GDK_IS_MACOS_WINDOW (nswindow));

  style_mask = [nswindow styleMask];

  if (gdk_toplevel_layout_get_resizable (layout))
    {
      geometry.min_width = gdk_toplevel_layout_get_min_width (layout);
      geometry.min_height = gdk_toplevel_layout_get_min_height (layout);
      mask = GDK_HINT_MIN_SIZE;

      style_mask |= NSWindowStyleMaskResizable;
    }
  else
    {
      geometry.max_width = geometry.min_width = width;
      geometry.max_height = geometry.min_height = height;
      mask = GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE;

      style_mask &= ~NSWindowStyleMaskResizable;
    }

  if (style_mask != [nswindow styleMask])
    [nswindow setStyleMask:style_mask];

  _gdk_macos_surface_set_geometry_hints (GDK_MACOS_SURFACE (self), &geometry, mask);
  gdk_surface_constrain_size (&geometry, mask, width, height, &width, &height);
  _gdk_macos_surface_resize (GDK_MACOS_SURFACE (self), width, height);

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

  if (GDK_SURFACE (self)->transient_for != NULL)
    {
    }
  else
    {
      if (!self->decorated &&
          !GDK_MACOS_SURFACE (self)->did_initial_present &&
          GDK_SURFACE (self)->x == 0 &&
          GDK_SURFACE (self)->y == 0 &&
          (GDK_MACOS_SURFACE (self)->shadow_left ||
           GDK_MACOS_SURFACE (self)->shadow_top))
        {
          GdkMonitor *monitor = _gdk_macos_surface_get_best_monitor (GDK_MACOS_SURFACE (self));
          int x = GDK_SURFACE (self)->x;
          int y = GDK_SURFACE (self)->y;

          if (monitor != NULL)
            {
              GdkRectangle visible;

              gdk_monitor_get_workarea (monitor, &visible);

              if (x < visible.x)
                x = visible.x;

              if (y < visible.y)
                y = visible.y;
            }

          x -= GDK_MACOS_SURFACE (self)->shadow_left;
          y -= GDK_MACOS_SURFACE (self)->shadow_top;

          _gdk_macos_surface_move (GDK_MACOS_SURFACE (self), x, y);
        }
    }

  _gdk_macos_surface_show (GDK_MACOS_SURFACE (self));

  GDK_MACOS_SURFACE (self)->did_initial_present = TRUE;

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
  NSWindow *nswindow;

  if (GDK_SURFACE_DESTROYED (self))
    return;

  nswindow = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (self));
  [nswindow makeKeyAndOrderFront:nswindow];
}

static void
_gdk_macos_toplevel_surface_begin_resize (GdkToplevel    *toplevel,
                                          GdkSurfaceEdge  edge,
                                          GdkDevice      *device,
                                          int             button,
                                          double          root_x,
                                          double          root_y,
                                          guint32         timestamp)
{
  NSWindow *nswindow;

  g_assert (GDK_IS_MACOS_SURFACE (toplevel));

  if (GDK_SURFACE_DESTROYED (toplevel))
    return;

  /* Release passive grab */
  if (button != 0)
    gdk_seat_ungrab (gdk_device_get_seat (device));

  if ((nswindow = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (toplevel))))
    [(GdkMacosWindow *)nswindow beginManualResize:edge];
}

static void
_gdk_macos_toplevel_surface_begin_move (GdkToplevel *toplevel,
                                        GdkDevice   *device,
                                        int          button,
                                        double       root_x,
                                        double       root_y,
                                        guint32      timestamp)
{
  NSWindow *nswindow;

  g_assert (GDK_IS_MACOS_SURFACE (toplevel));

  if (GDK_SURFACE_DESTROYED (toplevel))
    return;

  /* Release passive grab */
  if (button != 0)
    gdk_seat_ungrab (gdk_device_get_seat (device));

  if ((nswindow = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (toplevel))))
    [(GdkMacosWindow *)nswindow beginManualMove];
}


static void
toplevel_iface_init (GdkToplevelInterface *iface)
{
  iface->present = _gdk_macos_toplevel_surface_present;
  iface->minimize = _gdk_macos_toplevel_surface_minimize;
  iface->lower = _gdk_macos_toplevel_surface_lower;
  iface->focus = _gdk_macos_toplevel_surface_focus;
  iface->begin_resize = _gdk_macos_toplevel_surface_begin_resize;
  iface->begin_move = _gdk_macos_toplevel_surface_begin_move;
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

  _gdk_macos_toplevel_surface_detach_from_parent (self);
  g_clear_object (&GDK_SURFACE (self)->transient_for);

  if (g_set_object (&GDK_SURFACE (self)->transient_for, GDK_SURFACE (parent)))
    _gdk_macos_toplevel_surface_attach_to_parent (self);
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
      [(GdkMacosWindow *)window setDecorated:(BOOL)decorated];
    }
}

static void
_gdk_macos_toplevel_surface_hide (GdkSurface *surface)
{
  GdkMacosToplevelSurface *self = (GdkMacosToplevelSurface *)surface;

  _gdk_macos_toplevel_surface_detach_from_parent (self);

  GDK_SURFACE_CLASS (_gdk_macos_toplevel_surface_parent_class)->hide (surface);
}

static void
_gdk_macos_toplevel_surface_destroy (GdkSurface *surface,
                                     gboolean    foreign_destroy)
{
  GdkMacosToplevelSurface *self = (GdkMacosToplevelSurface *)surface;

  g_clear_object (&GDK_SURFACE (self)->transient_for);

  GDK_SURFACE_CLASS (_gdk_macos_toplevel_surface_parent_class)->destroy (surface, foreign_destroy);
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
      g_value_set_object (value, GDK_SURFACE (toplevel)->transient_for);
      break;

    case LAST_PROP + GDK_TOPLEVEL_PROP_MODAL:
      g_value_set_boolean (value, GDK_SURFACE (toplevel)->modal_hint);
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
      GDK_SURFACE (surface)->modal_hint = g_value_get_boolean (value);
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

  object_class->get_property = _gdk_macos_toplevel_surface_get_property;
  object_class->set_property = _gdk_macos_toplevel_surface_set_property;

  surface_class->destroy = _gdk_macos_toplevel_surface_destroy;
  surface_class->hide = _gdk_macos_toplevel_surface_hide;

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
  screen_rect = [screen visibleFrame];
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

void
_gdk_macos_toplevel_surface_attach_to_parent (GdkMacosToplevelSurface *self)
{
  GdkSurface *surface = (GdkSurface *)self;

  g_return_if_fail (GDK_IS_MACOS_TOPLEVEL_SURFACE (self));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (surface->transient_for != NULL &&
      !GDK_SURFACE_DESTROYED (surface->transient_for))
    {
      NSWindow *parent = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (surface->transient_for));
      NSWindow *window = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (self));

      [parent addChildWindow:window ordered:NSWindowAbove];

      if (GDK_SURFACE (self)->modal_hint)
        [window setLevel:NSModalPanelWindowLevel];

      _gdk_macos_display_clear_sorting (GDK_MACOS_DISPLAY (surface->display));
    }
}

void
_gdk_macos_toplevel_surface_detach_from_parent (GdkMacosToplevelSurface *self)
{
  GdkSurface *surface = (GdkSurface *)self;

  g_return_if_fail (GDK_IS_MACOS_TOPLEVEL_SURFACE (self));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (surface->transient_for != NULL &&
      !GDK_SURFACE_DESTROYED (surface->transient_for))
    {
      NSWindow *parent = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (surface->transient_for));
      NSWindow *window = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (self));

      [parent removeChildWindow:window];
      [window setLevel:NSNormalWindowLevel];

      _gdk_macos_display_clear_sorting (GDK_MACOS_DISPLAY (surface->display));
    }
}
