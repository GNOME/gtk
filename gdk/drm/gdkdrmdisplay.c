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

#include "gdkdisplayprivate.h"
#include "gdkeventsprivate.h"

#include "gdkdrmcairocontext-private.h"
#include "gdkdrmclipboard-private.h"
#include "gdkdrmdisplay-private.h"
#include "gdkdrmglcontext-private.h"
#include "gdkdrmkeymap-private.h"
#include "gdkdrmmonitor-private.h"
#include "gdkdrmpopupsurface-private.h"
#include "gdkdrmseat-private.h"
#include "gdkdrmsurface-private.h"
#include "gdkdrmtoplevelsurface-private.h"

G_DEFINE_FINAL_TYPE (GdkDrmDisplay, gdk_drm_display, GDK_TYPE_DISPLAY)

static GdkDrmMonitor *
get_monitor_unowned (GdkDrmDisplay *self,
                     guint          position)
{
  GdkDrmMonitor *monitor;

  g_assert (GDK_IS_DRM_DISPLAY (self));

  /* Get the monitor but return a borrowed reference */
  monitor = g_list_model_get_item (G_LIST_MODEL (self->monitors), position);
  if (monitor != NULL)
    g_object_unref (monitor);

  return monitor;
}

static gboolean
gdk_drm_display_get_setting (GdkDisplay *display,
                             const char *setting,
                             GValue     *value)
{
  return FALSE;
}

static GListModel *
gdk_drm_display_get_monitors (GdkDisplay *display)
{
  return G_LIST_MODEL (GDK_DRM_DISPLAY (display)->monitors);
}

static GdkMonitor *
gdk_drm_display_get_monitor_at_surface (GdkDisplay *display,
                                        GdkSurface *surface)
{
  GdkDrmDisplay *self = (GdkDrmDisplay *)display;
  guint n_monitors;

  g_assert (GDK_IS_DRM_DISPLAY (self));
  g_assert (GDK_IS_DRM_SURFACE (surface));

  n_monitors = g_list_model_get_n_items (G_LIST_MODEL (self->monitors));

  for (guint i = 0; i < n_monitors; i++)
    {
      G_GNUC_UNUSED GdkDrmMonitor *monitor = get_monitor_unowned (self, i);

    }

  return GDK_MONITOR (get_monitor_unowned (self, 0));
}

static void
gdk_drm_display_load_seat (GdkDrmDisplay *self)
{
  GdkSeat *seat;

  g_assert (GDK_IS_DRM_DISPLAY (self));

  seat = _gdk_drm_seat_new (self);
  gdk_display_add_seat (GDK_DISPLAY (self), seat);
  g_object_unref (seat);
}

static const char *
gdk_drm_display_get_name (GdkDisplay *display)
{
  return GDK_DRM_DISPLAY (display)->name;
}

static void
gdk_drm_display_beep (GdkDisplay *display)
{
  /* Not Supported */
}

static void
gdk_drm_display_flush (GdkDisplay *display)
{
  /* Not Supported */
}

static void
gdk_drm_display_sync (GdkDisplay *display)
{
  /* Not Supported */
}

static gulong
gdk_drm_display_get_next_serial (GdkDisplay *display)
{
  static gulong serial = 0;
  return ++serial;
}

static void
gdk_drm_display_notify_startup_complete (GdkDisplay  *display,
                                           const char *startup_notification_id)
{
  /* Not Supported */
}

static GdkKeymap *
gdk_drm_display_get_keymap (GdkDisplay *display)
{
  GdkDrmDisplay *self = (GdkDrmDisplay *)display;

  g_assert (GDK_IS_DRM_DISPLAY (self));

  return GDK_KEYMAP (self->keymap);
}

static void
gdk_drm_display_load_clipboard (GdkDrmDisplay *self)
{
  g_assert (GDK_IS_DRM_DISPLAY (self));

  GDK_DISPLAY (self)->clipboard = _gdk_drm_clipboard_new (self);
}

static GdkGLContext *
gdk_drm_display_init_gl (GdkDisplay  *display,
                         GError     **error)
{
  return _gdk_drm_gl_context_new (GDK_DRM_DISPLAY (display), error);
}

static void
gdk_drm_display_queue_events (GdkDisplay *display)
{
  g_assert (GDK_IS_DRM_DISPLAY (display));

}

static void
gdk_drm_display_finalize (GObject *object)
{
  GdkDrmDisplay *self = (GdkDrmDisplay *)object;

  g_clear_object (&GDK_DISPLAY (self)->clipboard);
  g_clear_object (&self->monitors);
  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (gdk_drm_display_parent_class)->finalize (object);
}

static void
gdk_drm_display_class_init (GdkDrmDisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (klass);

  object_class->finalize = gdk_drm_display_finalize;

  display_class->toplevel_type = GDK_TYPE_DRM_TOPLEVEL_SURFACE;
  display_class->popup_type = GDK_TYPE_DRM_POPUP_SURFACE;
  display_class->cairo_context_type = GDK_TYPE_DRM_CAIRO_CONTEXT;

  display_class->beep = gdk_drm_display_beep;
  display_class->flush = gdk_drm_display_flush;
  display_class->get_keymap = gdk_drm_display_get_keymap;
  display_class->get_monitors = gdk_drm_display_get_monitors;
  display_class->get_monitor_at_surface = gdk_drm_display_get_monitor_at_surface;
  display_class->get_next_serial = gdk_drm_display_get_next_serial;
  display_class->get_name = gdk_drm_display_get_name;
  display_class->get_setting = gdk_drm_display_get_setting;
  display_class->init_gl = gdk_drm_display_init_gl;
  display_class->notify_startup_complete = gdk_drm_display_notify_startup_complete;
  display_class->queue_events = gdk_drm_display_queue_events;
  display_class->sync = gdk_drm_display_sync;
}

static void
gdk_drm_display_init (GdkDrmDisplay *self)
{
  self->monitors = g_list_store_new (GDK_TYPE_MONITOR);

  gdk_display_set_composited (GDK_DISPLAY (self), FALSE);
  gdk_display_set_input_shapes (GDK_DISPLAY (self), FALSE);
  gdk_display_set_rgba (GDK_DISPLAY (self), FALSE);
  gdk_display_set_shadow_width (GDK_DISPLAY (self), FALSE);
}

GdkDisplay *
_gdk_drm_display_open (const char *display_name)
{
  static GdkDrmDisplay *self;

  display_name = display_name ? display_name : "";
  GDK_DEBUG (MISC, "opening display %s", display_name);

  self = g_object_new (GDK_TYPE_DRM_DISPLAY, NULL);
  self->name = g_strdup (display_name);

  gdk_drm_display_load_seat (self);
  gdk_drm_display_load_clipboard (self);

  return GDK_DISPLAY (self);
}

GdkModifierType
_gdk_drm_display_get_current_keyboard_modifiers (GdkDrmDisplay *display)
{
  return 0;
}

GdkModifierType
_gdk_drm_display_get_current_mouse_modifiers (GdkDrmDisplay *display)
{
  return 0;
}

void
_gdk_drm_display_from_display_coords (GdkDrmDisplay *self,
                                      int            x,
                                      int            y,
                                      int           *out_x,
                                      int           *out_y)
{
  *out_x = x;
  *out_y = y;
}
