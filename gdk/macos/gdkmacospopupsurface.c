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
#include "gdkpopupprivate.h"

#include "gdkmacosdisplay-private.h"
#include "gdkmacospopupsurface-private.h"
#include "gdkmacosutils-private.h"

struct _GdkMacosPopupSurface
{
  GdkMacosSurface parent_instance;
};

struct _GdkMacosPopupSurfaceClass
{
  GdkMacosSurfaceClass parent_class;
};

static void
gdk_macos_popup_surface_layout (GdkMacosPopupSurface *self,
                                int                   width,
                                int                   height,
                                GdkPopupLayout       *layout)
{
  GdkRectangle final_rect;
  int x, y;

  g_assert (GDK_IS_MACOS_POPUP_SURFACE (self));
  g_assert (layout != NULL);
  g_assert (GDK_SURFACE (self)->parent);

  gdk_surface_layout_popup_helper (GDK_SURFACE (self),
                                   width,
                                   height,
                                   layout,
                                   &final_rect);

  gdk_surface_get_origin (GDK_SURFACE (self)->parent, &x, &y);

  x += final_rect.x;
  y += final_rect.y;

  if (final_rect.width != GDK_SURFACE (self)->width ||
      final_rect.height != GDK_SURFACE (self)->height)
    _gdk_macos_surface_move_resize (GDK_MACOS_SURFACE (self),
                                    x,
                                    y,
                                    final_rect.width,
                                    final_rect.height);
  else if (x != GDK_MACOS_SURFACE (self)->root_x ||
           y != GDK_MACOS_SURFACE (self)->root_y)
    _gdk_macos_surface_move (GDK_MACOS_SURFACE (self), x, y);
  else
    return;

  gdk_surface_invalidate_rect (GDK_SURFACE (self), NULL);
}

static void
show_popup (GdkMacosPopupSurface *self)
{
  NSWindow *nswindow = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (self));

  [(GdkMacosWindow *)nswindow showAndMakeKey:YES];

  _gdk_macos_surface_show (GDK_MACOS_SURFACE (self));
}

static void
show_grabbing_popup (GdkSeat    *seat,
                     GdkSurface *surface,
                     gpointer    user_data)
{
  show_popup (GDK_MACOS_POPUP_SURFACE (surface));
}

static gboolean
gdk_macos_popup_surface_present (GdkPopup       *popup,
                                 int             width,
                                 int             height,
                                 GdkPopupLayout *layout)
{
  GdkMacosPopupSurface *self = (GdkMacosPopupSurface *)popup;

  g_assert (GDK_IS_MACOS_POPUP_SURFACE (self));

  gdk_macos_popup_surface_layout (self, width, height, layout);

  GDK_MACOS_SURFACE (self)->did_initial_present = TRUE;

  if (GDK_SURFACE_IS_MAPPED (GDK_SURFACE (self)))
    return TRUE;

  if (GDK_SURFACE (self)->autohide)
    {
      GdkDisplay *display = gdk_surface_get_display (GDK_SURFACE (popup));
      GdkSeat *seat = gdk_display_get_default_seat (display);

      gdk_seat_grab (seat,
                     GDK_SURFACE (self),
                     GDK_SEAT_CAPABILITY_ALL,
                     TRUE,
                     NULL, NULL,
                     show_grabbing_popup,
                     NULL);
    }
  else
    {
      show_popup (GDK_MACOS_POPUP_SURFACE (self));
    }

  return GDK_SURFACE_IS_MAPPED (GDK_SURFACE (self));
}

static GdkGravity
gdk_macos_popup_surface_get_surface_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.surface_anchor;
}

static GdkGravity
gdk_macos_popup_surface_get_rect_anchor (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->popup.rect_anchor;
}

static int
gdk_macos_popup_surface_get_position_x (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->x;
}

static int
gdk_macos_popup_surface_get_position_y (GdkPopup *popup)
{
  return GDK_SURFACE (popup)->y;
}

static void
popup_interface_init (GdkPopupInterface *iface)
{
  iface->present = gdk_macos_popup_surface_present;
  iface->get_surface_anchor = gdk_macos_popup_surface_get_surface_anchor;
  iface->get_rect_anchor = gdk_macos_popup_surface_get_rect_anchor;
  iface->get_position_x = gdk_macos_popup_surface_get_position_x;
  iface->get_position_y = gdk_macos_popup_surface_get_position_y;
}

G_DEFINE_TYPE_WITH_CODE (GdkMacosPopupSurface, _gdk_macos_popup_surface, GDK_TYPE_MACOS_SURFACE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_POPUP, popup_interface_init))

enum {
  PROP_0,
  LAST_PROP,
};

static void
_gdk_macos_popup_surface_finalize (GObject *object)
{
  GdkMacosPopupSurface *self = (GdkMacosPopupSurface *)object;

  g_clear_object (&GDK_SURFACE (self)->parent);

  G_OBJECT_CLASS (_gdk_macos_popup_surface_parent_class)->finalize (object);
}

static void
_gdk_macos_popup_surface_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_POPUP_PROP_PARENT:
      g_value_set_object (value, surface->parent);
      break;

    case LAST_PROP + GDK_POPUP_PROP_AUTOHIDE:
      g_value_set_boolean (value, surface->autohide);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
_gdk_macos_popup_surface_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case LAST_PROP + GDK_POPUP_PROP_PARENT:
      surface->parent = g_value_dup_object (value);
      if (surface->parent)
        surface->parent->children = g_list_prepend (surface->parent->children, surface);
      break;

    case LAST_PROP + GDK_POPUP_PROP_AUTOHIDE:
      surface->autohide = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
_gdk_macos_popup_surface_class_init (GdkMacosPopupSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = _gdk_macos_popup_surface_finalize;
  object_class->get_property = _gdk_macos_popup_surface_get_property;
  object_class->set_property = _gdk_macos_popup_surface_set_property;

  gdk_popup_install_properties (object_class, 1);
}

static void
_gdk_macos_popup_surface_init (GdkMacosPopupSurface *self)
{
}

GdkMacosSurface *
_gdk_macos_popup_surface_new (GdkMacosDisplay *display,
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

  style_mask = NSWindowStyleMaskBorderless;

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

  [window setOpaque:NO];
  [window setBackgroundColor:[NSColor clearColor]];

#if 0
  /* NOTE: We could set these to be popup level, but then
   * [NSApp orderedWindows] would not give us the windows
   * back with the stacking order applied.
   */
  [window setLevel:NSPopUpMenuWindowLevel];
#endif

  self = g_object_new (GDK_TYPE_MACOS_POPUP_SURFACE,
                       "display", display,
                       "frame-clock", frame_clock,
                       "native", window,
                       "parent", parent,
                       NULL);

  GDK_END_MACOS_ALLOC_POOL;

  return g_steal_pointer (&self);
}

void
_gdk_macos_popup_surface_attach_to_parent (GdkMacosPopupSurface *self)
{
  GdkSurface *surface = (GdkSurface *)self;

  g_return_if_fail (GDK_IS_MACOS_POPUP_SURFACE (self));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (surface->parent != NULL && !GDK_SURFACE_DESTROYED (surface->parent))
    {
      NSWindow *parent = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (surface->parent));
      NSWindow *window = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (self));

      [parent addChildWindow:window ordered:NSWindowAbove];

      _gdk_macos_display_clear_sorting (GDK_MACOS_DISPLAY (surface->display));
    }
}

void
_gdk_macos_popup_surface_detach_from_parent (GdkMacosPopupSurface *self)
{
  GdkSurface *surface = (GdkSurface *)self;

  g_return_if_fail (GDK_IS_MACOS_POPUP_SURFACE (self));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (surface->parent != NULL && !GDK_SURFACE_DESTROYED (surface->parent))
    {
      NSWindow *parent = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (surface->parent));
      NSWindow *window = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (self));

      [parent removeChildWindow:window];

      _gdk_macos_display_clear_sorting (GDK_MACOS_DISPLAY (surface->display));
    }
}
