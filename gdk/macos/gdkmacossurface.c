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

#import "GdkMacosCairoView.h"

#include "gdkdisplay.h"
#include "gdkframeclockidleprivate.h"
#include "gdkinternals.h"
#include "gdksurfaceprivate.h"

#include "gdkmacosdevice.h"
#include "gdkmacosdisplay-private.h"
#include "gdkmacosdragsurface-private.h"
#include "gdkmacosmonitor-private.h"
#include "gdkmacospopupsurface-private.h"
#include "gdkmacossurface-private.h"
#include "gdkmacostoplevelsurface-private.h"
#include "gdkmacosutils-private.h"

G_DEFINE_TYPE (GdkMacosSurface, gdk_macos_surface, GDK_TYPE_SURFACE)

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

static void
gdk_macos_surface_set_input_region (GdkSurface     *surface,
                                    cairo_region_t *region)
{
}

static void
gdk_macos_surface_set_opaque_region (GdkSurface     *surface,
                                     cairo_region_t *region)
{
  /* TODO:
   *
   * For CSD, we have a NSWindow with isOpaque:NO. There is some performance
   * penalty for this because the compositor will have to composite every pixel
   * intead of simply taking the source pixel.
   *
   * To improve this when using Cairo, we could have a number of child NSView
   * which are opaque, and simply draw from the underlying Cairo surface for
   * each view. Then only the corners and shadow would have non-opaque regions.
   *
   * However, as we intend to move to a OpenGL (or Metal) renderer, the effort
   * here may not be worth the time.
   */
}

static void
gdk_macos_surface_hide (GdkSurface *surface)
{
  GdkMacosSurface *self = (GdkMacosSurface *)surface;
  GdkSeat *seat;

  g_assert (GDK_IS_MACOS_SURFACE (self));

  seat = gdk_display_get_default_seat (surface->display);
  gdk_seat_ungrab (seat);

  [self->window hide];

  _gdk_surface_clear_update_area (surface);
}

static gint
gdk_macos_surface_get_scale_factor (GdkSurface *surface)
{
  GdkMacosSurface *self = (GdkMacosSurface *)surface;

  g_assert (GDK_IS_MACOS_SURFACE (self));

  return [self->window backingScaleFactor];
}

static void
gdk_macos_surface_set_shadow_width (GdkSurface *surface,
                                    int         left,
                                    int         right,
                                    int         top,
                                    int         bottom)
{
  GdkMacosSurface *self = (GdkMacosSurface *)surface;

  g_assert (GDK_IS_MACOS_SURFACE (self));

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

  display = gdk_surface_get_display (GDK_SURFACE (self));
  frame_clock = gdk_surface_get_frame_clock (GDK_SURFACE (self));

  if ((timings = gdk_frame_clock_get_current_timings (frame_clock)))
    self->pending_frame_counter = timings->frame_counter;

  _gdk_macos_display_add_frame_callback (GDK_MACOS_DISPLAY (display), self);

  gdk_surface_freeze_updates (GDK_SURFACE (self));
}

static void
gdk_macos_surface_before_paint (GdkMacosSurface *self,
                                GdkFrameClock   *frame_clock)
{
  GdkSurface *surface = (GdkSurface *)self;

  g_assert (GDK_IS_MACOS_SURFACE (self));
  g_assert (GDK_IS_FRAME_CLOCK (frame_clock));

  if (surface->update_freeze_count > 0)
    return;

  gdk_macos_surface_begin_frame (self);
}

static void
gdk_macos_surface_after_paint (GdkMacosSurface *self,
                               GdkFrameClock   *frame_clock)
{
  GdkSurface *surface = (GdkSurface *)self;

  g_assert (GDK_IS_MACOS_SURFACE (self));
  g_assert (GDK_IS_FRAME_CLOCK (frame_clock));

  if (surface->update_freeze_count > 0)
    return;

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
  GdkDisplay *display;
  NSRect content_rect;
  int tmp_x = 0;
  int tmp_y = 0;

  g_assert (GDK_IS_MACOS_SURFACE (self));

  if (GDK_SURFACE_DESTROYED (surface))
    {
      if (root_x)
        *root_x = 0;
      if (root_y)
        *root_y = 0;

      return;
    }

  content_rect = [self->window contentRectForFrameRect:[self->window frame]];

  display = gdk_surface_get_display (surface);
  _gdk_macos_display_from_display_coords (GDK_MACOS_DISPLAY (display),
                                          content_rect.origin.x,
                                          content_rect.origin.y + content_rect.size.height,
                                          &tmp_x, &tmp_y);

  tmp_x += x;
  tmp_y += y;

  if (root_x)
    *root_x = tmp_x;

  if (root_y)
    *root_y = tmp_y;
}

static gboolean
gdk_macos_surface_get_device_state (GdkSurface      *surface,
                                    GdkDevice       *device,
                                    gdouble         *x,
                                    gdouble         *y,
                                    GdkModifierType *mask)
{
  GdkDisplay *display;
  NSWindow *nswindow;
  NSPoint point;
  int x_tmp;
  int y_tmp;

  g_assert (GDK_IS_MACOS_SURFACE (surface));
  g_assert (GDK_IS_MACOS_DEVICE (device));
  g_assert (x != NULL);
  g_assert (y != NULL);
  g_assert (mask != NULL);

  display = gdk_surface_get_display (surface);
  nswindow = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (surface));
  point = [nswindow mouseLocationOutsideOfEventStream];

  _gdk_macos_display_from_display_coords (GDK_MACOS_DISPLAY (display),
                                          point.x, point.y,
                                          &x_tmp, &y_tmp);

  *x = x_tmp;
  *y = x_tmp;

  return TRUE;
}

static void
gdk_macos_surface_destroy (GdkSurface *surface,
                           gboolean    foreign_destroy)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  GdkMacosSurface *self = (GdkMacosSurface *)surface;
  GdkMacosWindow *window = g_steal_pointer (&self->window);

  g_clear_pointer (&self->title, g_free);

  if (window != NULL)
    [window close];

  _gdk_macos_display_surface_removed (GDK_MACOS_DISPLAY (surface->display), self);

  g_clear_pointer (&self->monitors, g_ptr_array_unref);

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
  surface_class->get_device_state = gdk_macos_surface_get_device_state;
  surface_class->get_root_coords = gdk_macos_surface_get_root_coords;
  surface_class->get_scale_factor = gdk_macos_surface_get_scale_factor;
  surface_class->hide = gdk_macos_surface_hide;
  surface_class->set_input_region = gdk_macos_surface_set_input_region;
  surface_class->set_opaque_region = gdk_macos_surface_set_opaque_region;
  surface_class->set_shadow_width = gdk_macos_surface_set_shadow_width;

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

  if (ret != NULL)
    _gdk_macos_surface_monitor_changed (ret);

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

const char *
_gdk_macos_surface_get_title (GdkMacosSurface *self)
{

  return self->title;
}

void
_gdk_macos_surface_set_title (GdkMacosSurface *self,
                              const gchar     *title)
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

  if (geom_mask & GDK_HINT_POS) { /* TODO */ }
  if (geom_mask & GDK_HINT_USER_POS) { /* TODO */ }
  if (geom_mask & GDK_HINT_USER_SIZE) { /* TODO */ }

  if (geom_mask & GDK_HINT_MAX_SIZE)
    max_size = NSMakeSize (geometry->max_width, geometry->max_height);
  else
    max_size = NSMakeSize (FLT_MAX, FLT_MAX);
  [self->window setContentMaxSize:max_size];

  if (geom_mask & GDK_HINT_MIN_SIZE)
    min_size = NSMakeSize (geometry->min_width, geometry->min_height);
  else
    min_size = NSMakeSize (0, 0);
  [self->window setContentMinSize:min_size];

  if (geom_mask & GDK_HINT_BASE_SIZE) { /* TODO */ }

  if (geom_mask & GDK_HINT_RESIZE_INC)
    {
      NSSize size;

      size.width = geometry->width_inc;
      size.height = geometry->height_inc;

      [self->window setContentResizeIncrements:size];
    }

  if (geom_mask & GDK_HINT_ASPECT)
    {
      NSSize size;

      if (geometry->min_aspect != geometry->max_aspect)
        g_warning ("Only equal minimum and maximum aspect ratios are supported on Mac OS. Using minimum aspect ratio...");

      size.width = geometry->min_aspect;
      size.height = 1.0;

      [self->window setContentAspectRatio:size];
    }

  if (geom_mask & GDK_HINT_WIN_GRAVITY) { /* TODO */ }
}

void
_gdk_macos_surface_resize (GdkMacosSurface *self,
                           int              width,
                           int              height)
{
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
  frame_rect = [self->window frameRectForContentRect:content_rect];
  [self->window setFrame:frame_rect display:YES];
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

void
_gdk_macos_surface_update_position (GdkMacosSurface *self)
{
  GdkSurface *surface = GDK_SURFACE (self);
  GdkDisplay *display = gdk_surface_get_display (surface);
  NSRect frame_rect = [self->window frame];
  NSRect content_rect = [self->window contentRectForFrameRect:frame_rect];

  _gdk_macos_display_from_display_coords (GDK_MACOS_DISPLAY (display),
                                          content_rect.origin.x,
                                          content_rect.origin.y + content_rect.size.height,
                                          &surface->x, &surface->y);

  if (GDK_IS_POPUP (self) && self->did_initial_present)
    g_signal_emit_by_name (self, "popup-layout-changed");
}

void
_gdk_macos_surface_damage_cairo (GdkMacosSurface *self,
                                 cairo_surface_t *surface,
                                 cairo_region_t  *painted)
{
  NSView *view;

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));
  g_return_if_fail (surface != NULL);

  view = [self->window contentView];

  if (GDK_IS_MACOS_CAIRO_VIEW (view))
    [(GdkMacosCairoView *)view setCairoSurfaceWithRegion:surface
                                             cairoRegion:painted];
}

void
_gdk_macos_surface_thaw (GdkMacosSurface *self,
                         gint64           presentation_time,
                         gint64           refresh_interval)
{
  GdkFrameTimings *timings;
  GdkFrameClock *frame_clock;

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  gdk_surface_thaw_updates (GDK_SURFACE (self));

  frame_clock = gdk_surface_get_frame_clock (GDK_SURFACE (self));

  if (self->pending_frame_counter)
    {
      timings = gdk_frame_clock_get_timings (frame_clock, self->pending_frame_counter);

      if (timings != NULL)
        timings->presentation_time = presentation_time - refresh_interval;

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
    gdk_synthesize_surface_state (GDK_SURFACE (self), GDK_SURFACE_STATE_WITHDRAWN, 0);

  _gdk_macos_display_clear_sorting (GDK_MACOS_DISPLAY (GDK_SURFACE (self)->display));

  [self->window showAndMakeKey:YES];
  [[self->window contentView] setNeedsDisplay:YES];

  if (!was_mapped)
    {
      if (gdk_surface_get_mapped (GDK_SURFACE (self)))
        gdk_surface_invalidate_rect (GDK_SURFACE (self), NULL);
    }
}

CGContextRef
_gdk_macos_surface_acquire_context (GdkMacosSurface *self,
                                    gboolean         clear_scale,
                                    gboolean         antialias)
{
  CGContextRef cg_context;

  g_return_val_if_fail (GDK_IS_MACOS_SURFACE (self), NULL);

  if (GDK_SURFACE_DESTROYED (self))
    return NULL;

  if (!(cg_context = [[NSGraphicsContext currentContext] CGContext]))
    return NULL;

  CGContextSaveGState (cg_context);

  if (!antialias)
    CGContextSetAllowsAntialiasing (cg_context, antialias);

  if (clear_scale)
    {
      CGSize scale;

      scale = CGSizeMake (1.0, 1.0);
      scale = CGContextConvertSizeToDeviceSpace (cg_context, scale);

      CGContextScaleCTM (cg_context, 1.0 / scale.width, 1.0 / scale.height);
    }

  return cg_context;
}

void
_gdk_macos_surface_release_context (GdkMacosSurface *self,
                                    CGContextRef     cg_context)
{
  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  CGContextRestoreGState (cg_context);
  CGContextSetAllowsAntialiasing (cg_context, TRUE);
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
                             NULL,
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
  GdkEvent *event;
  GList *node;
  gboolean size_changed;

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  if ((x == -1 || (x == surface->x)) &&
      (y == -1 || (y == surface->y)) &&
      (width == -1 || (width == surface->width)) &&
      (height == -1 || (height == surface->height)))
    return;

  display = gdk_surface_get_display (surface);

  if (width == -1)
    width = surface->width;

  if (height == -1)
    height = surface->height;

  size_changed = height != surface->height || width != surface->width;

  surface->x = x;
  surface->y = y;

  _gdk_macos_display_to_display_coords (GDK_MACOS_DISPLAY (display),
                                        x, y, &x, &y);

  [self->window setFrame:NSMakeRect(x, y - height, width, height)
                 display:YES];

  if (size_changed)
    {
      gdk_surface_invalidate_rect (surface, NULL);

      event = gdk_configure_event_new (surface, width, height);
      node = _gdk_event_queue_append (display, event);
      _gdk_windowing_got_event (display, node, event,
                                _gdk_display_get_next_serial (display));
    }
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
  GdkRectangle rect;
  GdkRectangle intersect;
  GdkDisplay *display;
  GdkMonitor *monitor;
  guint n_monitors;

  g_return_if_fail (GDK_IS_MACOS_SURFACE (self));

  rect.x = GDK_SURFACE (self)->x;
  rect.y = GDK_SURFACE (self)->y;
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
}

GdkMonitor *
_gdk_macos_surface_get_best_monitor (GdkMacosSurface *self)
{
  GdkMonitor *best = NULL;
  GdkRectangle rect;
  int best_area = 0;

  g_return_val_if_fail (GDK_IS_MACOS_SURFACE (self), NULL);

  rect.x = GDK_SURFACE (self)->x;
  rect.y = GDK_SURFACE (self)->y;
  rect.width = GDK_SURFACE (self)->width;
  rect.height = GDK_SURFACE (self)->height;

  for (guint i = 0; i < self->monitors->len; i++)
    {
      GdkMonitor *monitor = g_ptr_array_index (self->monitors, i);
      GdkRectangle intersect;

      if (gdk_rectangle_intersect (&monitor->geometry, &rect, &intersect))
        {
          int area = intersect.width * intersect.height;

          if (area > best_area)
            {
              best = monitor;
              best_area = area;
            }
        }
    }

  return best;
}
