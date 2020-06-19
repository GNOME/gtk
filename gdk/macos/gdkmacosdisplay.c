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

#import "GdkMacosWindow.h"

#include "gdkdisplayprivate.h"
#include "gdkeventsprivate.h"

#include "gdkdisplaylinksource.h"
#include "gdkmacoscairocontext-private.h"
#include "gdkmacoseventsource-private.h"
#include "gdkmacosdisplay-private.h"
#include "gdkmacoskeymap-private.h"
#include "gdkmacosmonitor-private.h"
#include "gdkmacosseat-private.h"
#include "gdkmacossurface-private.h"
#include "gdkmacosutils-private.h"

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

G_DEFINE_TYPE (GdkMacosDisplay, gdk_macos_display, GDK_TYPE_DISPLAY)

static GSource *event_source;

static GdkMacosMonitor *
get_monitor (GdkMacosDisplay *self,
             guint            position)
{
  GdkMacosMonitor *monitor;

  g_assert (GDK_IS_MACOS_DISPLAY (self));

  /* Get the monitor but return a borrowed reference */
  monitor = g_list_model_get_item (G_LIST_MODEL (self->monitors), position);
  if (monitor != NULL)
    g_object_unref (monitor);

  return monitor;
}

static gboolean
gdk_macos_display_get_setting (GdkDisplay  *display,
                               const gchar *setting,
                               GValue      *value)
{
  return _gdk_macos_display_get_setting (GDK_MACOS_DISPLAY (display), setting, value);
}

static GListModel *
gdk_macos_display_get_monitors (GdkDisplay *display)
{
  return G_LIST_MODEL (GDK_MACOS_DISPLAY (display)->monitors);
}

static GdkMonitor *
gdk_macos_display_get_monitor_at_surface (GdkDisplay *display,
                                          GdkSurface *surface)
{
  GdkMacosDisplay *self = (GdkMacosDisplay *)display;
  CGDirectDisplayID screen_id;
  guint n_monitors;

  g_assert (GDK_IS_MACOS_DISPLAY (self));
  g_assert (GDK_IS_MACOS_SURFACE (surface));

  screen_id = _gdk_macos_surface_get_screen_id (GDK_MACOS_SURFACE (surface));
  n_monitors = g_list_model_get_n_items (G_LIST_MODEL (self->monitors));

  for (guint i = 0; i < n_monitors; i++)
    {
      GdkMacosMonitor *monitor = get_monitor (self, i);

      if (screen_id == _gdk_macos_monitor_get_screen_id (monitor))
        return GDK_MONITOR (monitor);
    }

  return GDK_MONITOR (get_monitor (self, 0));
}

static GdkMacosMonitor *
gdk_macos_display_find_monitor (GdkMacosDisplay   *self,
                                CGDirectDisplayID  screen_id)
{
  guint n_monitors;

  g_assert (GDK_IS_MACOS_DISPLAY (self));

  n_monitors = g_list_model_get_n_items (G_LIST_MODEL (self->monitors));

  for (guint i = 0; i < n_monitors; i++)
    {
      GdkMacosMonitor *monitor = get_monitor (self, i);

      if (screen_id == _gdk_macos_monitor_get_screen_id (monitor))
        return monitor;
    }

  return NULL;
}

static void
gdk_macos_display_update_bounds (GdkMacosDisplay *self)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  g_assert (GDK_IS_MACOS_DISPLAY (self));

  self->min_x = G_MAXINT;
  self->min_y = G_MAXINT;

  self->max_x = G_MININT;
  self->max_y = G_MININT;

  for (id obj in [NSScreen screens])
    {
      NSRect geom = [(NSScreen *)obj frame];

      self->min_x = MIN (self->min_x, geom.origin.x);
      self->min_y = MIN (self->min_y, geom.origin.y);
      self->max_x = MAX (self->max_x, geom.origin.x + geom.size.width);
      self->max_y = MAX (self->max_y, geom.origin.y + geom.size.height);
    }

  self->width = self->max_x - self->min_x;
  self->height = self->max_y - self->min_y;

  GDK_END_MACOS_ALLOC_POOL;
}

static void
gdk_macos_display_monitors_changed_cb (CFNotificationCenterRef  center,
                                       void                    *observer,
                                       CFStringRef              name,
                                       const void              *object,
                                       CFDictionaryRef          userInfo)
{
  GdkMacosDisplay *self = observer;

  g_assert (GDK_IS_MACOS_DISPLAY (self));

  _gdk_macos_display_reload_monitors (self);

  /* Now we need to update all our surface positions since they
   * probably just changed origins. We ignore the popup surfaces
   * since we can rely on the toplevel surfaces to handle that.
   */
  for (const GList *iter = _gdk_macos_display_get_surfaces (self);
       iter != NULL;
       iter = iter->next)
    {
      GdkMacosSurface *surface = iter->data;

      g_assert (GDK_IS_MACOS_SURFACE (surface));

      if (GDK_IS_TOPLEVEL (surface))
        _gdk_macos_surface_update_position (surface);
    }
}

static void
gdk_macos_display_user_defaults_changed_cb (CFNotificationCenterRef  center,
                                            void                    *observer,
                                            CFStringRef              name,
                                            const void              *object,
                                            CFDictionaryRef          userInfo)
{
  GdkMacosDisplay *self = observer;

  g_assert (GDK_IS_MACOS_DISPLAY (self));

  _gdk_macos_display_reload_settings (self);
}

void
_gdk_macos_display_reload_monitors (GdkMacosDisplay *self)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  GArray *seen;
  guint n_monitors;

  g_assert (GDK_IS_MACOS_DISPLAY (self));

  gdk_macos_display_update_bounds (self);

  seen = g_array_new (FALSE, FALSE, sizeof (CGDirectDisplayID));

  for (id obj in [NSScreen screens])
    {
      CGDirectDisplayID screen_id;
      GdkMacosMonitor *monitor;

      screen_id = [[[obj deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue];
      g_array_append_val (seen, screen_id);

      if ((monitor = gdk_macos_display_find_monitor (self, screen_id)))
        {
          _gdk_macos_monitor_reconfigure (monitor);
        }
      else
        {
          monitor = _gdk_macos_monitor_new (self, screen_id);
          g_list_store_append (self->monitors, monitor);
          g_object_unref (monitor);
        }
    }

  n_monitors = g_list_model_get_n_items (G_LIST_MODEL (self->monitors));

  for (guint i = n_monitors; i > 0; i--)
    {
      GdkMacosMonitor *monitor = get_monitor (self, i - 1);
      CGDirectDisplayID screen_id = _gdk_macos_monitor_get_screen_id (monitor);
      gboolean found = FALSE;

      for (guint j = 0; j < seen->len; j++)
        {
          if (screen_id == g_array_index (seen, CGDirectDisplayID, j))
            {
              found = TRUE;
              break;
            }
        }

      if (!found)
        g_list_store_remove (self->monitors, i - 1);
    }

  g_array_unref (seen);

  GDK_END_MACOS_ALLOC_POOL;
}

static void
gdk_macos_display_load_seat (GdkMacosDisplay *self)
{
  GdkSeat *seat;

  g_assert (GDK_IS_MACOS_DISPLAY (self));

  seat = _gdk_macos_seat_new (self);
  gdk_display_add_seat (GDK_DISPLAY (self), seat);
  g_object_unref (seat);
}

static gboolean
gdk_macos_display_frame_cb (gpointer data)
{
  GdkMacosDisplay *self = data;
  GdkDisplayLinkSource *source;
  gint64 presentation_time;
  gint64 now;
  GList *iter;

  g_assert (GDK_IS_MACOS_DISPLAY (self));

  source = (GdkDisplayLinkSource *)self->frame_source;

  presentation_time = source->presentation_time;
  now = g_source_get_time ((GSource *)source);

  iter = self->awaiting_frames.head;

  while (iter != NULL)
    {
      GdkMacosSurface *surface = iter->data;

      g_assert (GDK_IS_MACOS_SURFACE (surface));

      iter = iter->next;

      _gdk_macos_display_remove_frame_callback (self, surface);
      _gdk_macos_surface_thaw (surface,
                               source->presentation_time,
                               source->refresh_interval);
    }

  return G_SOURCE_CONTINUE;
}

static void
gdk_macos_display_load_display_link (GdkMacosDisplay *self)
{
  self->frame_source = gdk_display_link_source_new ();
  g_source_set_callback (self->frame_source,
                         gdk_macos_display_frame_cb,
                         self,
                         NULL);
  g_source_attach (self->frame_source, NULL);
}

static const gchar *
gdk_macos_display_get_name (GdkDisplay *display)
{
  return GDK_MACOS_DISPLAY (display)->name;
}

static void
gdk_macos_display_beep (GdkDisplay *display)
{
  NSBeep ();
}

static void
gdk_macos_display_flush (GdkDisplay *display)
{
  /* Not Supported */
}

static void
gdk_macos_display_sync (GdkDisplay *display)
{
  /* Not Supported */
}

static gulong
gdk_macos_display_get_next_serial (GdkDisplay *display)
{
  return 0;
}

static gboolean
gdk_macos_display_has_pending (GdkDisplay *display)
{
  return _gdk_event_queue_find_first (display) ||
         _gdk_macos_event_source_check_pending ();
}

static void
gdk_macos_display_notify_startup_complete (GdkDisplay  *display,
                                           const gchar *startup_notification_id)
{
  /* Not Supported */
}

static void
gdk_macos_display_queue_events (GdkDisplay *display)
{
  GdkMacosDisplay *self = (GdkMacosDisplay *)display;
  NSEvent *nsevent;

  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));

  if ((nsevent = _gdk_macos_event_source_get_pending ()))
    {
      GdkEvent *event = _gdk_macos_display_translate (self, nsevent);

      if (event != NULL)
        _gdk_windowing_got_event (GDK_DISPLAY (self),
                                  _gdk_event_queue_append (GDK_DISPLAY (self), event),
                                  event,
                                  0);
      else
        [NSApp sendEvent:nsevent];

      [nsevent release];
    }
}

static void
_gdk_macos_display_surface_added (GdkMacosDisplay *self,
                                  GdkMacosSurface *surface)
{
  g_assert (GDK_IS_MACOS_DISPLAY (self));
  g_assert (GDK_IS_MACOS_SURFACE (surface));
  g_assert (!queue_contains (&self->sorted_surfaces, &surface->sorted));
  g_assert (!queue_contains (&self->main_surfaces, &surface->main));
  g_assert (!queue_contains (&self->awaiting_frames, &surface->frame));
  g_assert (surface->sorted.data == surface);
  g_assert (surface->main.data == surface);
  g_assert (surface->frame.data == surface);

  if (GDK_IS_TOPLEVEL (surface))
    g_queue_push_tail_link (&self->main_surfaces, &surface->main);

  _gdk_macos_display_clear_sorting (self);
}

void
_gdk_macos_display_surface_removed (GdkMacosDisplay *self,
                                    GdkMacosSurface *surface)
{
  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));
  g_return_if_fail (GDK_IS_MACOS_SURFACE (surface));

  if (self->keyboard_surface == surface)
    _gdk_macos_display_surface_resigned_key (self, surface);

  g_queue_unlink (&self->sorted_surfaces, &surface->sorted);

  if (queue_contains (&self->main_surfaces, &surface->main))
    _gdk_macos_display_surface_resigned_main (self, surface);

  if (queue_contains (&self->awaiting_frames, &surface->frame))
    g_queue_unlink (&self->awaiting_frames, &surface->frame);

  g_return_if_fail (self->keyboard_surface != surface);
}

void
_gdk_macos_display_surface_became_key (GdkMacosDisplay *self,
                                       GdkMacosSurface *surface)
{
  GdkDevice *keyboard;
  GdkEvent *event;
  GdkSeat *seat;

  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));
  g_return_if_fail (GDK_IS_MACOS_SURFACE (surface));
  g_return_if_fail (self->keyboard_surface == NULL);

  self->keyboard_surface = surface;

  seat = gdk_display_get_default_seat (GDK_DISPLAY (self));
  keyboard = gdk_seat_get_keyboard (seat);
  event = gdk_focus_event_new (GDK_SURFACE (surface), keyboard, NULL, TRUE);
  _gdk_event_queue_append (GDK_DISPLAY (self), event);

  /* We just became the active window.  Unlike X11, Mac OS X does
   * not send us motion events while the window does not have focus
   * ("is not key").  We send a dummy motion notify event now, so that
   * everything in the window is set to correct state.
   */
  _gdk_macos_display_synthesize_motion (self, surface);
}

void
_gdk_macos_display_surface_resigned_key (GdkMacosDisplay *self,
                                         GdkMacosSurface *surface)
{
  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));
  g_return_if_fail (GDK_IS_MACOS_SURFACE (surface));

  if (self->keyboard_surface == surface)
    {
      GdkDevice *keyboard;
      GdkEvent *event;
      GdkSeat *seat;

      seat = gdk_display_get_default_seat (GDK_DISPLAY (self));
      keyboard = gdk_seat_get_keyboard (seat);
      event = gdk_focus_event_new (GDK_SURFACE (surface), keyboard, NULL, FALSE);
      _gdk_event_queue_append (GDK_DISPLAY (self), event);
    }

  self->keyboard_surface = NULL;

  _gdk_macos_display_clear_sorting (self);
}

void
_gdk_macos_display_surface_became_main (GdkMacosDisplay *self,
                                        GdkMacosSurface *surface)
{
  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));
  g_return_if_fail (GDK_IS_MACOS_SURFACE (surface));

  if (queue_contains (&self->main_surfaces, &surface->main))
    g_queue_unlink (&self->main_surfaces, &surface->main);

  g_queue_push_head_link (&self->main_surfaces, &surface->main);

  _gdk_macos_display_clear_sorting (self);
}

void
_gdk_macos_display_surface_resigned_main (GdkMacosDisplay *self,
                                          GdkMacosSurface *surface)
{
  GdkMacosSurface *new_surface = NULL;

  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));
  g_return_if_fail (GDK_IS_MACOS_SURFACE (surface));

  if (queue_contains (&self->main_surfaces, &surface->main))
    g_queue_unlink (&self->main_surfaces, &surface->main);

  _gdk_macos_display_clear_sorting (self);

  if (GDK_SURFACE (surface)->transient_for &&
      gdk_surface_get_mapped (GDK_SURFACE (surface)->transient_for))
    {
      new_surface = GDK_MACOS_SURFACE (GDK_SURFACE (surface)->transient_for);
    }
  else
    {
      const GList *surfaces = _gdk_macos_display_get_surfaces (self);

      for (const GList *iter = surfaces; iter; iter = iter->next)
        {
          GdkMacosSurface *item = iter->data;

          g_assert (GDK_IS_MACOS_SURFACE (item));

          if (item == surface)
            continue;

          if (GDK_SURFACE_IS_MAPPED (GDK_SURFACE (item)))
            {
              new_surface = item;
              break;
            }
        }
    }

  if (new_surface != NULL)
    {
      NSWindow *nswindow = _gdk_macos_surface_get_native (new_surface);
      [nswindow makeKeyAndOrderFront:nswindow];
    }

  _gdk_macos_display_clear_sorting (self);
}

static GdkSurface *
gdk_macos_display_create_surface (GdkDisplay     *display,
                                  GdkSurfaceType  surface_type,
                                  GdkSurface     *parent,
                                  int             x,
                                  int             y,
                                  int             width,
                                  int             height)
{
  GdkMacosDisplay *self = (GdkMacosDisplay *)display;
  GdkMacosSurface *surface;

  g_assert (GDK_IS_MACOS_DISPLAY (self));
  g_assert (!parent || GDK_IS_MACOS_SURFACE (parent));

  surface = _gdk_macos_surface_new (self, surface_type, parent, x, y, width, height);

  if (surface != NULL)
    _gdk_macos_display_surface_added (self, surface);

  return GDK_SURFACE (surface);
}

static GdkKeymap *
gdk_macos_display_get_keymap (GdkDisplay *display)
{
  GdkMacosDisplay *self = (GdkMacosDisplay *)display;

  g_assert (GDK_IS_MACOS_DISPLAY (self));

  return GDK_KEYMAP (self->keymap);
}

static void
gdk_macos_display_finalize (GObject *object)
{
  GdkMacosDisplay *self = (GdkMacosDisplay *)object;

  CFNotificationCenterRemoveObserver (CFNotificationCenterGetDistributedCenter (),
                                      self,
                                      CFSTR ("NSApplicationDidChangeScreenParametersNotification"),
                                      NULL);

  CFNotificationCenterRemoveObserver (CFNotificationCenterGetDistributedCenter (),
                                      self,
                                      CFSTR ("NSUserDefaultsDidChangeNotification"),
                                      NULL);

  g_clear_pointer (&self->frame_source, g_source_unref);
  g_clear_object (&self->monitors);
  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (gdk_macos_display_parent_class)->finalize (object);
}

static void
gdk_macos_display_class_init (GdkMacosDisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (klass);

  object_class->finalize = gdk_macos_display_finalize;

  display_class->cairo_context_type = GDK_TYPE_MACOS_CAIRO_CONTEXT;

  display_class->beep = gdk_macos_display_beep;
  display_class->create_surface = gdk_macos_display_create_surface;
  display_class->flush = gdk_macos_display_flush;
  display_class->get_keymap = gdk_macos_display_get_keymap;
  display_class->get_monitors = gdk_macos_display_get_monitors;
  display_class->get_monitor_at_surface = gdk_macos_display_get_monitor_at_surface;
  display_class->get_next_serial = gdk_macos_display_get_next_serial;
  display_class->get_name = gdk_macos_display_get_name;
  display_class->get_setting = gdk_macos_display_get_setting;
  display_class->has_pending = gdk_macos_display_has_pending;
  display_class->notify_startup_complete = gdk_macos_display_notify_startup_complete;
  display_class->queue_events = gdk_macos_display_queue_events;
  display_class->sync = gdk_macos_display_sync;
}

static void
gdk_macos_display_init (GdkMacosDisplay *self)
{
  self->monitors = g_list_store_new (GDK_TYPE_MONITOR);

  gdk_display_set_composited (GDK_DISPLAY (self), TRUE);
  gdk_display_set_input_shapes (GDK_DISPLAY (self), FALSE);
  gdk_display_set_rgba (GDK_DISPLAY (self), TRUE);
}

GdkDisplay *
_gdk_macos_display_open (const gchar *display_name)
{
  static GdkMacosDisplay *self;
  ProcessSerialNumber psn = { 0, kCurrentProcess };

  /* Until we can have multiple GdkMacosEventSource instances
   * running concurrently, we can't exactly support multiple
   * display connections. So just short-circuit if we already
   * have one active.
   */
  if (self != NULL)
    return NULL;

  GDK_NOTE (MISC, g_message ("opening display %s", display_name ? display_name : ""));

  /* Make the current process a foreground application, i.e. an app
   * with a user interface, in case we're not running from a .app bundle
   */
  TransformProcessType (&psn, kProcessTransformToForegroundApplication);

  [NSApplication sharedApplication];

  self = g_object_new (GDK_TYPE_MACOS_DISPLAY, NULL);
  self->name = g_strdup (display_name);
  self->keymap = _gdk_macos_keymap_new (self);

  gdk_macos_display_load_seat (self);
  /* Load CVDisplayLink before monitors to access refresh rates */
  gdk_macos_display_load_display_link (self);
  _gdk_macos_display_reload_monitors (self);

  CFNotificationCenterAddObserver (CFNotificationCenterGetLocalCenter (),
                                   self,
                                   gdk_macos_display_monitors_changed_cb,
                                   CFSTR ("NSApplicationDidChangeScreenParametersNotification"),
                                   NULL,
                                   CFNotificationSuspensionBehaviorDeliverImmediately);

  CFNotificationCenterAddObserver (CFNotificationCenterGetDistributedCenter (),
                                   self,
                                   gdk_macos_display_user_defaults_changed_cb,
                                   CFSTR ("NSUserDefaultsDidChangeNotification"),
                                   NULL,
                                   CFNotificationSuspensionBehaviorDeliverImmediately);

  if (event_source == NULL)
    {
      event_source = _gdk_macos_event_source_new (self);
      g_source_attach (event_source, NULL);
    }

  g_object_add_weak_pointer (G_OBJECT (self), (gpointer *)&self);

  gdk_display_emit_opened (GDK_DISPLAY (self));

  return GDK_DISPLAY (self);
}

void
_gdk_macos_display_to_display_coords (GdkMacosDisplay *self,
                                      int              x,
                                      int              y,
                                      int             *out_x,
                                      int             *out_y)
{
  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));

  if (out_y)
    *out_y = self->height - y + self->min_y;

  if (out_x)
    *out_x = x + self->min_x;
}

void
_gdk_macos_display_from_display_coords (GdkMacosDisplay *self,
                                        int              x,
                                        int              y,
                                        int             *out_x,
                                        int             *out_y)
{
  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));

  if (out_y != NULL)
    *out_y = self->height - y + self->min_y;

  if (out_x != NULL)
    *out_x = x - self->min_x;
}

GdkMonitor *
_gdk_macos_display_get_monitor_at_coords (GdkMacosDisplay *self,
                                          int              x,
                                          int              y)
{
  guint n_monitors;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (self), NULL);

  n_monitors = g_list_model_get_n_items (G_LIST_MODEL (self->monitors));

  for (guint i = 0; i < n_monitors; i++)
    {
      GdkMacosMonitor *monitor = get_monitor (self, i);

      if (gdk_rectangle_contains_point (&GDK_MONITOR (monitor)->geometry, x, y))
        return GDK_MONITOR (monitor);
    }

  return NULL;
}

GdkMonitor *
_gdk_macos_display_get_monitor_at_display_coords (GdkMacosDisplay *self,
                                                  int              x,
                                                  int              y)
{
  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (self), NULL);

  _gdk_macos_display_from_display_coords (self, x, y, &x, &y);

  return _gdk_macos_display_get_monitor_at_coords (self, x, y);
}

NSScreen *
_gdk_macos_display_get_screen_at_display_coords (GdkMacosDisplay *self,
                                                 int              x,
                                                 int              y)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  NSArray *screens;
  NSScreen *screen = NULL;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (self), NULL);

  screens = [NSScreen screens];

  for (id obj in screens)
    {
      NSRect geom = [obj frame];

      if (x >= geom.origin.x && x <= geom.origin.x + geom.size.width &&
          y >= geom.origin.y && y <= geom.origin.y + geom.size.height)
        {
          screen = obj;
          break;
        }
    }

  GDK_END_MACOS_ALLOC_POOL;

  return screen;
}

void
_gdk_macos_display_break_all_grabs (GdkMacosDisplay *self,
                                    guint32          time)
{
  GdkDevice *devices[2];
  GdkSeat *seat;

  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));

  seat = gdk_display_get_default_seat (GDK_DISPLAY (self));
  devices[0] = gdk_seat_get_keyboard (seat);
  devices[1] = gdk_seat_get_pointer (seat);

  for (guint i = 0; i < G_N_ELEMENTS (devices); i++)
    {
      GdkDevice *device = devices[i];
      GdkDeviceGrabInfo *grab;

      grab = _gdk_display_get_last_device_grab (GDK_DISPLAY (self), device);

      if (grab != NULL)
        {
          GdkEvent *event;
          GList *node;

          event = gdk_grab_broken_event_new (grab->surface,
                                             device,
                                             NULL,
                                             grab->surface,
                                             TRUE);
          node = _gdk_event_queue_append (GDK_DISPLAY (self), event);
          _gdk_windowing_got_event (GDK_DISPLAY (self), node, event, 0);
        }
    }
}

void
_gdk_macos_display_queue_events (GdkMacosDisplay *self)
{
  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));

  gdk_macos_display_queue_events (GDK_DISPLAY (self));
}

static GdkMacosSurface *
_gdk_macos_display_get_surface_at_coords (GdkMacosDisplay *self,
                                          int              x,
                                          int              y,
                                          int             *surface_x,
                                          int             *surface_y)
{
  const GList *surfaces;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (self), NULL);
  g_return_val_if_fail (surface_x != NULL, NULL);
  g_return_val_if_fail (surface_y != NULL, NULL);

  surfaces = _gdk_macos_display_get_surfaces (self);

  for (const GList *iter = surfaces; iter; iter = iter->next)
    {
      GdkSurface *surface = iter->data;
      NSWindow *nswindow;

      g_assert (GDK_IS_MACOS_SURFACE (surface));

      if (!gdk_surface_get_mapped (surface))
        continue;

      nswindow = _gdk_macos_surface_get_native (GDK_MACOS_SURFACE (surface));

      if (x >= GDK_MACOS_SURFACE (surface)->root_x &&
          y >= GDK_MACOS_SURFACE (surface)->root_y &&
          x <= (GDK_MACOS_SURFACE (surface)->root_x + surface->width) &&
          y <= (GDK_MACOS_SURFACE (surface)->root_y + surface->height))
        {
          *surface_x = x - GDK_MACOS_SURFACE (surface)->root_x;
          *surface_y = y - GDK_MACOS_SURFACE (surface)->root_y;

          return GDK_MACOS_SURFACE (surface);
        }
    }

  *surface_x = 0;
  *surface_y = 0;

  return NULL;
}

GdkMacosSurface *
_gdk_macos_display_get_surface_at_display_coords (GdkMacosDisplay *self,
                                                  double           x,
                                                  double           y,
                                                  int             *surface_x,
                                                  int             *surface_y)
{
  int x_gdk;
  int y_gdk;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (self), NULL);
  g_return_val_if_fail (surface_x != NULL, NULL);
  g_return_val_if_fail (surface_y != NULL, NULL);

  _gdk_macos_display_from_display_coords (self, x, y, &x_gdk, &y_gdk);

  return _gdk_macos_display_get_surface_at_coords (self, x_gdk, y_gdk, surface_x, surface_y);
}

void
_gdk_macos_display_add_frame_callback (GdkMacosDisplay *self,
                                       GdkMacosSurface *surface)
{
  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));
  g_return_if_fail (GDK_IS_MACOS_SURFACE (surface));

  if (!queue_contains (&self->awaiting_frames, &surface->frame))
    {
      g_queue_push_tail_link (&self->awaiting_frames, &surface->frame);

      if (self->awaiting_frames.length == 1)
        gdk_display_link_source_unpause ((GdkDisplayLinkSource *)self->frame_source);
    }
}

void
_gdk_macos_display_remove_frame_callback (GdkMacosDisplay *self,
                                          GdkMacosSurface *surface)
{
  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));
  g_return_if_fail (GDK_IS_MACOS_SURFACE (surface));

  if (queue_contains (&self->awaiting_frames, &surface->frame))
    {
      g_queue_unlink (&self->awaiting_frames, &surface->frame);

      if (self->awaiting_frames.length == 0)
        gdk_display_link_source_pause ((GdkDisplayLinkSource *)self->frame_source);
    }
}

NSWindow *
_gdk_macos_display_find_native_under_pointer (GdkMacosDisplay *self,
                                              int             *x,
                                              int             *y)
{
  GdkMacosSurface *surface;
  NSPoint point;

  g_assert (GDK_IS_MACOS_DISPLAY (self));

  point = [NSEvent mouseLocation];

  surface = _gdk_macos_display_get_surface_at_display_coords (self, point.x, point.y, x, y);
  if (surface != NULL)
    return _gdk_macos_surface_get_native (surface);

  return NULL;
}

int
_gdk_macos_display_get_nominal_refresh_rate (GdkMacosDisplay *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (self), 60 * 1000);

  if (self->frame_source == NULL)
    return 60 * 1000;

  return ((GdkDisplayLinkSource *)self->frame_source)->refresh_rate;
}

void
_gdk_macos_display_clear_sorting (GdkMacosDisplay *self)
{
  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));

  self->sorted_surfaces.head = NULL;
  self->sorted_surfaces.tail = NULL;
  self->sorted_surfaces.length = 0;
}

const GList *
_gdk_macos_display_get_surfaces (GdkMacosDisplay *self)
{
  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (self), NULL);

  if (self->sorted_surfaces.length == 0)
    {
      GDK_BEGIN_MACOS_ALLOC_POOL;

      NSArray *array = [NSApp orderedWindows];
      GQueue sorted = G_QUEUE_INIT;

      for (id obj in array)
        {
          NSWindow *nswindow = (NSWindow *)obj;
          GdkMacosSurface *surface;

          if (!GDK_IS_MACOS_WINDOW (nswindow))
            continue;

          surface = [(GdkMacosWindow *)nswindow gdkSurface];

          surface->sorted.prev = NULL;
          surface->sorted.next = NULL;

          g_queue_push_tail_link (&sorted, &surface->sorted);
        }

      self->sorted_surfaces = sorted;

      GDK_END_MACOS_ALLOC_POOL;
    }

  return self->sorted_surfaces.head;
}
