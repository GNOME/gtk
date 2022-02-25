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

#import "GdkMacosView.h"

#include "gdkmacossurface-private.h"

#include "gdkdeviceprivate.h"
#include "gdkdisplay.h"
#include "gdkeventsprivate.h"
#include "gdkframeclockidleprivate.h"
#include "gdkseatprivate.h"
#include "gdksurfaceprivate.h"

#include "gdkmacosdevice.h"
#include "gdkmacosdevice-private.h"
#include "gdkmacosdisplay-private.h"
#include "gdkmacosdrag-private.h"
#include "gdkmacosdragsurface-private.h"
#include "gdkmacosglcontext-private.h"
#include "gdkmacosmonitor-private.h"
#include "gdkmacospopupsurface-private.h"
#include "gdkmacostoplevelsurface-private.h"
#include "gdkmacosutils-private.h"

G_DEFINE_ABSTRACT_TYPE (GdkMacosSurface, gdk_macos_surface, GDK_TYPE_SURFACE)

enum {
  PROP_0,
  PROP_NATIVE,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static gboolean
window_is_fullscreen (GdkMacosSurface *self)
{
  g_assert (GDK_IS_MACOS_SURFACE (self));

  return ([self->window styleMask] & NSWindowStyleMaskFullScreen) != 0;
}


void
_gdk_macos_surface_reposition_children (GdkMacosSurface *self)
{
  g_assert (GDK_IS_MACOS_SURFACE (self));


  if (GDK_SURFACE_DESTROYED (self))
    return;

  for (const GList *iter = GDK_SURFACE (self)->children;
       iter != NULL;
       iter = iter->next)
    {
      GdkMacosSurface *child = iter->data;

      g_assert (GDK_IS_MACOS_SURFACE (child));

      if (GDK_IS_MACOS_POPUP_SURFACE (child))
        _gdk_macos_popup_surface_reposition (GDK_MACOS_POPUP_SURFACE (child));
    }

  if (GDK_IS_POPUP (self) && self->did_initial_present)
    gdk_surface_request_layout (GDK_SURFACE (self));
}

static void
gdk_macos_surface_set_input_region (GdkSurface     *surface,
                                    cairo_region_t *region)
{
  GdkMacosSurface *self = (GdkMacosSurface *)surface;
  cairo_rectangle_int_t rect;

  g_assert (GDK_IS_MACOS_SURFACE (self));

  if (self->window == NULL)
    return;

  cairo_region_get_extents (region, &rect);

  [(GdkMacosBaseView *)[self->window contentView] setInputArea:&rect];
}

static void
gdk_macos_surface_set_opaque_region (GdkSurface     *surface,
                                     cairo_region_t *region)
{
  GdkMacosSurface *self = (GdkMacosSurface *)surface;
  NSView *nsview;

  g_assert (GDK_IS_MACOS_SURFACE (self));

  if ((nsview = _gdk_macos_surface_get_view (GDK_MACOS_SURFACE (surface))))
    [(GdkMacosView *)nsview setOpaqueRegion:region];
}

static void
gdk_macos_surface_hide (GdkSurface *surface)
{
  GdkMacosSurface *self = (GdkMacosSurface *)surface;
  GdkSeat *seat;
  gboolean was_mapped;
  gboolean was_key;

  g_assert (GDK_IS_MACOS_SURFACE (self));

  self->show_on_next_swap = FALSE;

  _gdk_macos_display_remove_frame_callback (GDK_MACOS_DISPLAY (surface->display), self);

  was_mapped = GDK_SURFACE_IS_MAPPED (GDK_SURFACE (self));
  was_key = [self->window isKeyWindow];

  seat = gdk_display_get_default_seat (surface->display);
  gdk_seat_ungrab (seat);

  [self->window hide];

  _gdk_surface_clear_update_area (surface);

  g_clear_object (&self->buffer);
  g_clear_object (&self->front);

  if (was_key)
    {
      /* Return key input to the parent window if necessary */
      if (surface->parent != NULL && GDK_SURFACE_IS_MAPPED (surface->parent))
        {
          GdkMacosWindow *parentWindow = GDK_MACOS_SURFACE (surface->parent)->window;

          [parentWindow showAndMakeKey:YES];
        }
    }

  if (was_mapped)
    gdk_surface_freeze_updates (GDK_SURFACE (self));
}

static int
gdk_macos_surface_get_scale_factor (GdkSurface *surface)
{
  GdkMacosSurface *self = (GdkMacosSurface *)surface;

  g_assert (GDK_IS_MACOS_SURFACE (self));

  return [self->window backingScaleFactor];
}

void
_gdk_macos_surface_set_shadow (GdkMacosSurface *surface,
                               int              top,
                               int              right,
                               int              bottom,
                               int              left)
{
  GdkMacosSurface *self = (GdkMacosSurface *)surface;

  g_assert (GDK_IS_MACOS_SURFACE (self));

  if (self->shadow_top == top &&
      self->shadow_right == right &&
      self->shadow_bottom == bottom &&
      self->shadow_left == left)
    return;

  self->shadow_top = top;
  self->shadow_right = right;
  self->shadow_bottom = bottom;
  self->shadow_left = left;

  if (top || right || bottom || left)
    [self->window setHasShadow:NO];
}

static void
gdk_macos_surface_begin_frame (GdkMacosSurface *self)
{
  g_assert (GDK_IS_MACOS_SURFACE (self));

}

static void
gdk_macos_surface_end_frame (GdkMacosSurface *self)
{
  GdkFrameTimings *timings;
  GdkFrameClock *frame_clock;
  GdkDisplay *display;

  g_assert (GDK_IS_MACOS_SURFACE (self));

  if (GDK_SURFACE_DESTROYED (self))
    return;

  display = gdk_surface_get_display (GDK_SURFACE (self));
  frame_clock = gdk_surface_get_frame_clock (GDK_SURFACE (self));

  if ((timings = gdk_frame_clock_get_current_timings (frame_clock)))
    self->pending_frame_counter = timings->frame_counter;

  if (GDK_SURFACE_IS_MAPPED (GDK_SURFACE (self)))
    {
      _gdk_macos_display_add_frame_callback (GDK_MACOS_DISPLAY (display), self);
      gdk_surface_freeze_updates (GDK_SURFACE (self));
    }
}

static void
gdk_macos_surface_before_paint (GdkMacosSurface *self,
                                GdkFrameClock   *frame_clock)
{
  GdkSurface *surface = (GdkSurface *)self;

  g_assert (GDK_IS_MACOS_SURFACE (self));
  g_assert (GDK_IS_FRAME_CLOCK (frame_clock));

  if (GDK_SURFACE_DESTROYED (self))
    return;

  if (surface->update_freeze_count == 0)
    gdk_macos_surface_begin_frame (self);
}

static void
gdk_macos_surface_after_paint (GdkMacosSurface *self,
                               GdkFrameClock   *frame_clock)
{
  GdkSurface *surface = (GdkSurface *)self;

  g_assert (GDK_IS_MACOS_SURFACE (self));
  g_assert (GDK_IS_FRAME_CLOCK (frame_clock));

  if (GDK_SURFACE_DESTROYED (self))
    return;

  if (surface->update_freeze_count == 0)
    gdk_macos_surface_end_frame (self);
}

static void
gdk_macos_surface_get_root_coords (GdkSurface *surface,
                                   int         x,
                                   int         y,
                                   int        *root_x,
                                   int        *root_y)
{
  GdkMacosSurface *self = (GdkMacosSurface *)surface;

  g_assert (GDK_IS_MACOS_SURFACE (self));

  if (root_x)
    *root_x = self->root_x + x;

  if (root_y)
    *root_y = self->root_y + y;
}

static gboolean
gdk_macos_surface_get_device_state (GdkSurface      *surface,
                                    GdkDevice       *device,
                                    double          *x,
                                    double          *y,
                                    GdkModifierType *mask)
{
  GdkDisplay *display;
  NSWindow *nswindow;
  NSPoint point;

  g_assert (GDK_IS_MACOS_SURFACE (surface));
  g_assert (GDK_IS_MACOS_DEVICE (device));
  g_assert (x != NULL);
  g_assert (y != NULL);
  g_assert (mask != NULL);

  if (GDK_SURFACE_DESTROYED (surface))
    return FALSE;

  display = gdk_surface_get_display (surface);
  nswindow = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (surface));
  point = [nswindow mouseLocationOutsideOfEventStream];

  *mask = _gdk_macos_display_get_current_keyboard_modifiers (GDK_MACOS_DISPLAY (display))
        | _gdk_macos_display_get_current_mouse_modifiers (GDK_MACOS_DISPLAY (display));

  *x = point.x;
  *y = surface->height - point.y;

  return *x >= 0 && *y >= 0 && *x < surface->width && *y < surface->height;
}

static void
gdk_macos_surface_get_geometry (GdkSurface *surface,
                                int        *x,
                                int        *y,
                                int        *width,
                                int        *height)
{
  g_assert (GDK_IS_MACOS_SURFACE (surface));

  if (x != NULL)
    *x = surface->x;

  if (y != NULL)
    *y = surface->y;

  if (width != NULL)
    *width = surface->width;

  if (height != NULL)
    *height = surface->height;
}

static GdkDrag *
gdk_macos_surface_drag_begin (GdkSurface         *surface,
                              GdkDevice          *device,
                              GdkContentProvider *content,
                              GdkDragAction       actions,
                              double              dx,
                              double              dy)
{
  GdkMacosSurface *self = (GdkMacosSurface *)surface;
  GdkMacosSurface *drag_surface;
  GdkMacosDrag *drag;
  GdkCursor *cursor;
  GdkSeat *seat;
  double px;
  double py;
  int sx;
  int sy;

  g_assert (GDK_IS_MACOS_SURFACE (self));
  g_assert (GDK_IS_MACOS_TOPLEVEL_SURFACE (self) ||
            GDK_IS_MACOS_POPUP_SURFACE (self));
  g_assert (GDK_IS_MACOS_DEVICE (device));
  g_assert (GDK_IS_CONTENT_PROVIDER (content));

  seat = gdk_device_get_seat (device);
  gdk_macos_device_query_state (device, surface, NULL, &px, &py, NULL);
  _gdk_macos_surface_get_root_coords (GDK_MACOS_SURFACE (surface), &sx, &sy);
  drag_surface = _gdk_macos_surface_new (GDK_MACOS_DISPLAY (surface->display),
                                         GDK_SURFACE_TEMP,
                                         surface,
                                         sx, sy, 1, 1);
  drag = g_object_new (GDK_TYPE_MACOS_DRAG,
                       "drag-surface", drag_surface,
                       "surface", surface,
                       "device", device,
                       "content", content,
                       "actions", actions,
                       NULL);
  g_clear_object (&drag_surface);

  cursor = gdk_drag_get_cursor (GDK_DRAG (drag),
                                gdk_drag_get_selected_action (GDK_DRAG (drag)));
  gdk_drag_set_cursor (GDK_DRAG (drag), cursor);

  if (!_gdk_macos_drag_begin (drag))
    {
      g_object_unref (drag);
      return NULL;
    }

  /* Hold a reference until drop_done is called */
  g_object_ref (drag);

  return GDK_DRAG (g_steal_pointer (&drag));
}

static void
gdk_macos_surface_destroy (GdkSurface *surface,
                           gboolean    foreign_destroy)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  GdkMacosSurface *self = (GdkMacosSurface *)surface;
  GdkMacosWindow *window = g_steal_pointer (&self->window);
  GdkFrameClock *frame_clock;

  g_clear_object (&self->best_monitor);

  if ((frame_clock = gdk_surface_get_frame_clock (GDK_SURFACE (self))))
    {
      g_signal_handlers_disconnect_by_func (frame_clock,
                                            G_CALLBACK (gdk_macos_surface_before_paint),
                                            self);
      g_signal_handlers_disconnect_by_func (frame_clock,
                                            G_CALLBACK (gdk_macos_surface_after_paint),
                                            self);
    }

  g_clear_pointer (&self->title, g_free);

  if (window != NULL)
    [window close];

  _gdk_macos_display_surface_removed (GDK_MACOS_DISPLAY (surface->display), self);

  g_clear_pointer (&self->monitors, g_ptr_array_unref);

  g_clear_object (&self->buffer);
  g_clear_object (&self->front);

  g_assert (self->sorted.prev == NULL);
  g_assert (self->sorted.next == NULL);
  g_assert (self->frame.prev == NULL);
  g_assert (self->frame.next == NULL);
  g_assert (self->main.prev == NULL);
  g_assert (self->main.next == NULL);

  GDK_END_MACOS_ALLOC_POOL;
}

static void
gdk_macos_surface_constructed (GObject *object)
{
  GdkMacosSurface *self = (GdkMacosSurface *)object;
  GdkFrameClock *frame_clock;

  g_assert (GDK_IS_MACOS_SURFACE (self));

  G_OBJECT_CLASS (gdk_macos_surface_parent_class)->constructed (object);

  if ((frame_clock = gdk_surface_get_frame_clock (GDK_SURFACE (self))))
    {
      g_signal_connect_object (frame_clock,
                               "before-paint",
                               G_CALLBACK (gdk_macos_surface_before_paint),
                               self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (frame_clock,
                               "after-paint",
                               G_CALLBACK (gdk_macos_surface_after_paint),
                               self,
                               G_CONNECT_SWAPPED);
    }

  if (self->window != NULL)
    _gdk_macos_surface_configure (self);
}

static void
gdk_macos_surface_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GdkMacosSurface *self = GDK_MACOS_SURFACE (object);

  switch (prop_id)
    {
    case PROP_NATIVE:
      g_value_set_pointer (value, self->window);
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

  switch (prop_id)
    {
    case PROP_NATIVE:
      self->window = g_value_get_pointer (value);
      [self->window setGdkSurface:self];
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

  object_class->constructed = gdk_macos_surface_constructed;
  object_class->get_property = gdk_macos_surface_get_property;
  object_class->set_property = gdk_macos_surface_set_property;

  surface_class->destroy = gdk_macos_surface_destroy;
  surface_class->drag_begin = gdk_macos_surface_drag_begin;
  surface_class->get_device_state = gdk_macos_surface_get_device_state;
  surface_class->get_geometry = gdk_macos_surface_get_geometry;
  surface_class->get_root_coords = gdk_macos_surface_get_root_coords;
  surface_class->get_scale_factor = gdk_macos_surface_get_scale_factor;
  surface_class->hide = gdk_macos_surface_hide;
  surface_class->set_input_region = gdk_macos_surface_set_input_region;
  surface_class->set_opaque_region = gdk_macos_surface_set_opaque_region;

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
  self->frame.data = self;
  self->main.data = self;
  self->sorted.data = self;
  self->monitors = g_ptr_array_new_with_free_func (g_object_unref);
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
      ret = _gdk_macos_drag_surface_new (display, frame_clock, x, y, width, height);
      break;

    default:
      g_warn_if_reached ();
      ret = NULL;
    }

  if (ret != NULL)
    {
      gdk_surface_freeze_updates (GDK_SURFACE (ret));
      _gdk_macos_surface_monitor_changed (ret);
    }

  g_object_unref (frame_clock);

  return g_steal_pointer (&ret);
}

void
_gdk_macos_surface_get_shadow (GdkMacosSurface *self,
                               int             *top,
                               int             *right,
                               int             *bottom,
                               int             *left)
{

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  if (top)
    *top = self->shadow_top;

  if (left)
    *left = self->shadow_left;

  if (bottom)
    *bottom = self->shadow_bottom;

  if (right)
    *right = self->shadow_right;
}

gboolean
_gdk_macos_surface_is_opaque (GdkMacosSurface *self)
{
  GdkSurface *surface = (GdkSurface *)self;

  g_return_val_if_fail (GDK_IS_MACOS_SURFACE (self), FALSE);

  if (surface->opaque_region != NULL &&
      cairo_region_num_rectangles (surface->opaque_region) == 1)
    {
      cairo_rectangle_int_t extents;

      cairo_region_get_extents (surface->opaque_region, &extents);

      return (extents.x == 0 &&
              extents.y == 0 &&
              extents.width == GDK_SURFACE (self)->width &&
              extents.height == GDK_SURFACE (self)->height);
    }

  return FALSE;
}

const char *
_gdk_macos_surface_get_title (GdkMacosSurface *self)
{

  return self->title;
}

void
_gdk_macos_surface_set_title (GdkMacosSurface *self,
                              const char      *title)
{
  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  if (title == NULL)
    title = "";

  if (g_strcmp0 (self->title, title) != 0)
    {
      g_free (self->title);
      self->title = g_strdup (title);

      GDK_BEGIN_MACOS_ALLOC_POOL;
      [self->window setTitle:[NSString stringWithUTF8String:title]];
      GDK_END_MACOS_ALLOC_POOL;
    }
}

CGDirectDisplayID
_gdk_macos_surface_get_screen_id (GdkMacosSurface *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_SURFACE (self), (CGDirectDisplayID)-1);

  if (self->window != NULL)
    {
      NSScreen *screen = [self->window screen];
      return [[[screen deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue];
    }

  return (CGDirectDisplayID)-1;
}

NSWindow *
_gdk_macos_surface_get_native (GdkMacosSurface *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_SURFACE (self), NULL);

  return (NSWindow *)self->window;
}

void
_gdk_macos_surface_set_geometry_hints (GdkMacosSurface   *self,
                                       const GdkGeometry *geometry,
                                       GdkSurfaceHints    geom_mask)
{
  NSSize max_size;
  NSSize min_size;

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));
  g_return_if_fail (geometry != NULL);
  g_return_if_fail (self->window != NULL);

  if (geom_mask & GDK_HINT_MAX_SIZE)
    max_size = NSMakeSize (geometry->max_width, geometry->max_height);
  else
    max_size = NSMakeSize (FLT_MAX, FLT_MAX);
  [self->window setContentMaxSize:max_size];

  if (geom_mask & GDK_HINT_MIN_SIZE)
    min_size = NSMakeSize (geometry->min_width, geometry->min_height);
  else
    min_size = NSMakeSize (1, 1);
  [self->window setContentMinSize:min_size];
}

void
_gdk_macos_surface_resize (GdkMacosSurface *self,
                           int              width,
                           int              height)
{
  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  _gdk_macos_surface_move_resize (self, -1, -1, width, height);
}

void
_gdk_macos_surface_update_fullscreen_state (GdkMacosSurface *self)
{
  GdkToplevelState state;
  gboolean is_fullscreen;
  gboolean was_fullscreen;

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  state = GDK_SURFACE (self)->state;
  is_fullscreen = window_is_fullscreen (self);
  was_fullscreen = (state & GDK_TOPLEVEL_STATE_FULLSCREEN) != 0;

  if (is_fullscreen != was_fullscreen)
    {
      if (is_fullscreen)
        gdk_synthesize_surface_state (GDK_SURFACE (self), 0, GDK_TOPLEVEL_STATE_FULLSCREEN);
      else
        gdk_synthesize_surface_state (GDK_SURFACE (self), GDK_TOPLEVEL_STATE_FULLSCREEN, 0);
    }
}

void
_gdk_macos_surface_configure (GdkMacosSurface *self)
{
  GdkMacosDisplay *display;
  GdkSurface *surface = (GdkSurface *)self;
  NSRect frame_rect;
  NSRect content_rect;

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  if (GDK_SURFACE_DESTROYED (self))
    return;

  display = GDK_MACOS_DISPLAY (GDK_SURFACE (self)->display);
  frame_rect = [self->window frame];
  content_rect = [self->window contentRectForFrameRect:frame_rect];

  _gdk_macos_display_from_display_coords (GDK_MACOS_DISPLAY (display),
                                          content_rect.origin.x,
                                          content_rect.origin.y + content_rect.size.height,
                                          &self->root_x, &self->root_y);

  if (surface->parent != NULL)
    {
      surface->x = self->root_x - GDK_MACOS_SURFACE (surface->parent)->root_x;
      surface->y = self->root_y - GDK_MACOS_SURFACE (surface->parent)->root_y;
    }
  else
    {
      surface->x = self->root_x;
      surface->y = self->root_y;
    }

  if (surface->width != content_rect.size.width ||
      surface->height != content_rect.size.height)
    {
      surface->width = content_rect.size.width;
      surface->height = content_rect.size.height;

      g_clear_object (&self->buffer);
      g_clear_object (&self->front);

      _gdk_surface_update_size (surface);
      gdk_surface_request_layout (surface);
      gdk_surface_invalidate_rect (surface, NULL);
    }

  _gdk_macos_surface_reposition_children (self);
}

void
_gdk_macos_surface_publish_timings (GdkMacosSurface *self,
                                    gint64           presentation_time,
                                    gint64           refresh_interval)
{
  GdkFrameTimings *timings;
  GdkFrameClock *frame_clock;

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  if (!(frame_clock = gdk_surface_get_frame_clock (GDK_SURFACE (self))))
    return;

  if (self->pending_frame_counter)
    {
      timings = gdk_frame_clock_get_timings (frame_clock, self->pending_frame_counter);

      if (timings != NULL)
        {
          timings->presentation_time = presentation_time - refresh_interval;
          timings->complete = TRUE;
        }

      self->pending_frame_counter = 0;
    }

  timings = gdk_frame_clock_get_current_timings (frame_clock);

  if (timings != NULL)
    {
      timings->refresh_interval = refresh_interval;
      timings->predicted_presentation_time = presentation_time;
    }
}

void
_gdk_macos_surface_show (GdkMacosSurface *self)
{
  gboolean was_mapped;

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  if (GDK_SURFACE_DESTROYED (self))
    return;

  was_mapped = GDK_SURFACE_IS_MAPPED (GDK_SURFACE (self));

  if (!was_mapped)
    gdk_surface_set_is_mapped (GDK_SURFACE (self), TRUE);

  _gdk_macos_display_clear_sorting (GDK_MACOS_DISPLAY (GDK_SURFACE (self)->display));

  self->show_on_next_swap = TRUE;

  if (!was_mapped)
    {
      if (gdk_surface_get_mapped (GDK_SURFACE (self)))
        {
          _gdk_macos_surface_configure (self);
          gdk_surface_thaw_updates (GDK_SURFACE (self));
        }
    }
}

void
_gdk_macos_surface_synthesize_null_key (GdkMacosSurface *self)
{
  GdkTranslatedKey translated = {0};
  GdkTranslatedKey no_lock = {0};
  GdkDisplay *display;
  GdkEvent *event;
  GdkSeat *seat;

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  translated.keyval = GDK_KEY_VoidSymbol;
  no_lock.keyval = GDK_KEY_VoidSymbol;

  display = gdk_surface_get_display (GDK_SURFACE (self));
  seat = gdk_display_get_default_seat (display);
  event = gdk_key_event_new (GDK_KEY_PRESS,
                             GDK_SURFACE (self),
                             gdk_seat_get_keyboard (seat),
                             GDK_CURRENT_TIME,
                             0,
                             0,
                             FALSE,
                             &translated,
                             &no_lock);
  _gdk_event_queue_append (display, event);
}

void
_gdk_macos_surface_move (GdkMacosSurface *self,
                         int              x,
                         int              y)
{
  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  _gdk_macos_surface_move_resize (self, x, y, -1, -1);
}

void
_gdk_macos_surface_move_resize (GdkMacosSurface *self,
                                int              x,
                                int              y,
                                int              width,
                                int              height)
{
  GdkSurface *surface = (GdkSurface *)self;
  GdkDisplay *display;
  NSRect content_rect;
  NSRect frame_rect;

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  if ((x == -1 || (x == self->root_x)) &&
      (y == -1 || (y == self->root_y)) &&
      (width == -1 || (width == surface->width)) &&
      (height == -1 || (height == surface->height)))
    return;

  display = gdk_surface_get_display (surface);

  if (width == -1)
    width = surface->width;

  if (height == -1)
    height = surface->height;

  if (x == -1)
    x = self->root_x;

  if (y == -1)
    y = self->root_y;

  _gdk_macos_display_to_display_coords (GDK_MACOS_DISPLAY (display),
                                        x, y + height,
                                        &x, &y);

  content_rect = NSMakeRect (x, y, width, height);
  frame_rect = [self->window frameRectForContentRect:content_rect];
  [self->window setFrame:frame_rect display:YES];
}

void
_gdk_macos_surface_user_resize (GdkMacosSurface *self,
                                CGRect           new_frame)
{
  GdkMacosDisplay *display;
  CGRect content_rect;
  int root_x, root_y;

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));
  g_return_if_fail (GDK_IS_TOPLEVEL (self));

  if (GDK_SURFACE_DESTROYED (self))
    return;

  display = GDK_MACOS_DISPLAY (GDK_SURFACE (self)->display);
  content_rect = [self->window contentRectForFrameRect:new_frame];

  _gdk_macos_display_from_display_coords (display,
                                          new_frame.origin.x,
                                          new_frame.origin.y + new_frame.size.height,
                                          &root_x, &root_y);

  self->next_layout.root_x = root_x;
  self->next_layout.root_y = root_y;
  self->next_layout.width = content_rect.size.width;
  self->next_layout.height = content_rect.size.height;

  gdk_surface_request_layout (GDK_SURFACE (self));
}

gboolean
_gdk_macos_surface_is_tracking (GdkMacosSurface *self,
                                NSTrackingArea  *area)
{
  GdkMacosBaseView *view;

  g_return_val_if_fail (GDK_IS_MACOS_SURFACE (self), FALSE);

  if (self->window == NULL)
    return FALSE;

  view = (GdkMacosBaseView *)[self->window contentView];
  if (view == NULL)
    return FALSE;

  return [view trackingArea] == area;
}

void
_gdk_macos_surface_monitor_changed (GdkMacosSurface *self)
{
  GListModel *monitors;
  GdkMonitor *best = NULL;
  GdkRectangle rect;
  GdkRectangle intersect;
  GdkDisplay *display;
  GdkMonitor *monitor;
  guint n_monitors;
  int best_area = 0;

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  rect.x = self->root_x;
  rect.y = self->root_y;
  rect.width = GDK_SURFACE (self)->width;
  rect.height = GDK_SURFACE (self)->height;

  for (guint i = self->monitors->len; i > 0; i--)
    {
      monitor = g_ptr_array_index (self->monitors, i-1);

      if (!gdk_rectangle_intersect (&monitor->geometry, &rect, &intersect))
        {
          g_object_ref (monitor);
          g_ptr_array_remove_index (self->monitors, i-1);
          gdk_surface_leave_monitor (GDK_SURFACE (self), monitor);
          g_object_unref (monitor);
        }
    }

  display = gdk_surface_get_display (GDK_SURFACE (self));
  monitors = gdk_display_get_monitors (display);
  n_monitors = g_list_model_get_n_items (monitors);

  for (guint i = 0; i < n_monitors; i++)
    {
      monitor = g_list_model_get_item (monitors, i);

      if (!g_ptr_array_find (self->monitors, monitor, NULL))
        {
          gdk_surface_enter_monitor (GDK_SURFACE (self), monitor);
          g_ptr_array_add (self->monitors, g_object_ref (monitor));
        }

      g_object_unref (monitor);
    }

  /* We need to create a new IOSurface for this monitor */
  g_clear_object (&self->buffer);
  g_clear_object (&self->front);

  /* Determine the best-fit monitor */
  for (guint i = 0; i < self->monitors->len; i++)
    {
      monitor = g_ptr_array_index (self->monitors, i);

      if (gdk_rectangle_intersect (&monitor->geometry, &rect, &intersect))
        {
          int area = intersect.width * intersect.height;

          if (area > best_area)
            {
              best_area = area;
              best = monitor;
            }
        }
    }

  if (g_set_object (&self->best_monitor, best))
    {
      /* TODO: change frame clock to new monitor */
    }

  _gdk_macos_surface_configure (self);

  gdk_surface_invalidate_rect (GDK_SURFACE (self), NULL);
}

GdkMonitor *
_gdk_macos_surface_get_best_monitor (GdkMacosSurface *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_SURFACE (self), NULL);

  return self->best_monitor;
}

NSView *
_gdk_macos_surface_get_view (GdkMacosSurface *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_SURFACE (self), NULL);

  if (self->window == NULL)
    return NULL;

  return [self->window contentView];
}

void
_gdk_macos_surface_set_opacity (GdkMacosSurface *self,
                                double           opacity)
{
  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  if (self->window != NULL)
    [self->window setAlphaValue:opacity];
}

void
_gdk_macos_surface_get_root_coords (GdkMacosSurface *self,
                                    int             *x,
                                    int             *y)
{
  GdkSurface *surface;
  int out_x = 0;
  int out_y = 0;

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  for (surface = GDK_SURFACE (self); surface; surface = surface->parent)
    {
      out_x += surface->x;
      out_y += surface->y;
    }

  if (x)
    *x = out_x;

  if (y)
    *y = out_y;
}

GdkMacosBuffer *
_gdk_macos_surface_get_buffer (GdkMacosSurface *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_SURFACE (self), NULL);

  if (GDK_SURFACE_DESTROYED (self))
    return NULL;

  if (self->buffer == NULL)
    {
      /* Create replacement buffer. We always use 4-byte and 32-bit BGRA for
       * our surface as that can work with both Cairo and GL. The GdkMacosTile
       * handles opaque regions for the compositor, so using 3-byte/24-bit is
       * not a necessary optimization.
       */
      double scale = gdk_surface_get_scale_factor (GDK_SURFACE (self));
      guint width = GDK_SURFACE (self)->width * scale;
      guint height = GDK_SURFACE (self)->height * scale;

      self->buffer = _gdk_macos_buffer_new (width, height, scale, 4, 32);
    }

  return self->buffer;
}

static void
_gdk_macos_surface_do_delayed_show (GdkMacosSurface *self)
{
  g_assert (GDK_IS_MACOS_SURFACE (self));

  self->show_on_next_swap = FALSE;
  [self->window showAndMakeKey:YES];
  gdk_surface_request_motion (GDK_SURFACE (self));
}

void
_gdk_macos_surface_swap_buffers (GdkMacosSurface      *self,
                                 const cairo_region_t *damage)
{
  GdkMacosBuffer *swap;

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));
  g_return_if_fail (damage != NULL);

  swap = self->buffer;
  self->buffer = self->front;
  self->front = swap;

  /* This code looks like it swaps buffers, but since the IOSurfaceRef
   * appears to be retained on the other side, we really just ask all
   * of the GdkMacosTile CALayer's to update their contents.
   */
  [self->window swapBuffer:swap withDamage:damage];

  /* We might have delayed actually showing the window until the buffer
   * contents are ready to be displayed. Doing so ensures that we don't
   * get a point where we might have invalid buffer contents before we
   * have content to display to the user.
   */
  if G_UNLIKELY (self->show_on_next_swap)
    _gdk_macos_surface_do_delayed_show (self);
}
