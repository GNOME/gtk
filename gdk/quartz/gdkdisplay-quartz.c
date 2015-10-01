/* gdkdisplay-quartz.c
 *
 * Copyright (C) 2005 Imendio AB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gdk/gdk.h>
#include <gdk/gdkdisplayprivate.h>
#include <gdk/gdkframeclockprivate.h>

#include "gdkprivate-quartz.h"
#include "gdkquartzscreen.h"
#include "gdkquartzwindow.h"
#include "gdkquartzdisplay.h"
#include "gdkquartzdevicemanager-core.h"
#include "gdkdisplaylinksource.h"


struct _GdkQuartzDisplay
{
  GdkDisplay display;

  GList *input_devices;

  /* This structure is not allocated. It points to an embedded
   * GList in the GdkWindow. */
  GList   *windows_awaiting_frame;
  GSource *frame_source;
};

struct _GdkQuartzDisplayClass
{
  GdkDisplayClass display_class;
};

static GdkWindow *
gdk_quartz_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  /* FIXME: Implement */

  return NULL;
}

GdkDeviceManager *
_gdk_device_manager_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_QUARTZ_DEVICE_MANAGER_CORE,
                       "display", display,
                       NULL);
}

static void
gdk_quartz_display_init_input (GdkDisplay *display)
{
  GdkQuartzDisplay *display_quartz;
  GdkDeviceManager *device_manager;
  GList *list, *l;

  display_quartz = GDK_QUARTZ_DISPLAY (display);
  device_manager = gdk_display_get_device_manager (_gdk_display);

  /* For backwards compabitility, just add floating devices that are
   * not keyboards.
   */
  list = gdk_device_manager_list_devices (device_manager,
                                          GDK_DEVICE_TYPE_FLOATING);
  for (l = list; l; l = l->next)
    {
      GdkDevice *device = l->data;

      if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
        continue;

      display_quartz->input_devices = g_list_prepend (display_quartz->input_devices,
                                                      g_object_ref (l->data));
    }

  g_list_free (list);

  /* Now set "core" pointer to the first master device that is a pointer. */
  list = gdk_device_manager_list_devices (device_manager,
                                          GDK_DEVICE_TYPE_MASTER);

  for (l = list; l; l = l->next)
    {
      GdkDevice *device = l->data;

      if (gdk_device_get_source (device) != GDK_SOURCE_MOUSE)
        continue;

      display->core_pointer = device;
      break;
    }

  /* Add the core pointer to the devices list */
  display_quartz->input_devices = g_list_prepend (display_quartz->input_devices,
                                                  g_object_ref (display->core_pointer));

  g_list_free (list);
}

void
_gdk_quartz_display_add_frame_callback (GdkDisplay             *display,
                                        GdkWindow              *window)
{
  GdkQuartzDisplay *display_quartz;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (window);

  display_quartz = GDK_QUARTZ_DISPLAY (display);

  impl->frame_link.data = window;
  impl->frame_link.prev = NULL;
  impl->frame_link.next = display_quartz->windows_awaiting_frame;

  display_quartz->windows_awaiting_frame = &impl->frame_link;

  if (impl->frame_link.next == NULL)
    gdk_display_link_source_unpause ((GdkDisplayLinkSource *)display_quartz->frame_source);
}

void
_gdk_quartz_display_remove_frame_callback (GdkDisplay             *display,
                                           GdkWindow              *window)
{
  GdkQuartzDisplay *display_quartz = GDK_QUARTZ_DISPLAY (display);
  GList *link;

  link = g_list_find (display_quartz->windows_awaiting_frame, window);

  if (link != NULL)
    {
      display_quartz->windows_awaiting_frame =
        g_list_remove_link (display_quartz->windows_awaiting_frame, link);
    }

  if (display_quartz->windows_awaiting_frame == NULL)
    gdk_display_link_source_pause ((GdkDisplayLinkSource *)display_quartz->frame_source);
}

static gboolean
gdk_quartz_display_frame_cb (gpointer data)
{
  GdkDisplayLinkSource *source;
  GdkQuartzDisplay *display_quartz = data;
  GList *iter;
  gint64 presentation_time;
  gint64 now;

  source = (GdkDisplayLinkSource *)display_quartz->frame_source;

  iter = display_quartz->windows_awaiting_frame;
  display_quartz->windows_awaiting_frame = NULL;

  if (iter == NULL)
    {
      gdk_display_link_source_pause (source);
      return G_SOURCE_CONTINUE;
    }

  presentation_time = source->presentation_time;
  now = g_source_get_time (display_quartz->frame_source);

  for (; iter != NULL; iter = iter->next)
    {
      GdkWindow *window = iter->data;
      GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (window->impl);
      GdkFrameClock *frame_clock = gdk_window_get_frame_clock (window);
      GdkFrameTimings *timings;

      if (frame_clock == NULL)
        continue;

      _gdk_frame_clock_thaw (frame_clock);

      if (impl->pending_frame_counter)
        {
          timings = gdk_frame_clock_get_timings (frame_clock, impl->pending_frame_counter);
          if (timings != NULL)
            timings->presentation_time = presentation_time - source->refresh_interval;
          impl->pending_frame_counter = 0;
        }

      timings = gdk_frame_clock_get_current_timings (frame_clock);

      if (timings != NULL)
        {
          timings->refresh_interval = source->refresh_interval;
          timings->predicted_presentation_time = source->presentation_time;
        }
    }

  return G_SOURCE_CONTINUE;
}

static void
gdk_quartz_display_init_display_link (GdkDisplay *display)
{
  GdkQuartzDisplay *display_quartz = GDK_QUARTZ_DISPLAY (display);

  display_quartz->frame_source = gdk_display_link_source_new ();
  g_source_set_callback (display_quartz->frame_source,
                         gdk_quartz_display_frame_cb,
                         display,
                         NULL);
  g_source_attach (display_quartz->frame_source, NULL);
}

GdkDisplay *
_gdk_quartz_display_open (const gchar *display_name)
{
  if (_gdk_display != NULL)
    return NULL;

  /* Initialize application */
  [NSApplication sharedApplication];

  _gdk_display = g_object_new (gdk_quartz_display_get_type (), NULL);
  _gdk_display->device_manager = _gdk_device_manager_new (_gdk_display);

  _gdk_screen = g_object_new (gdk_quartz_screen_get_type (), NULL);
  _gdk_quartz_screen_init_visuals (_gdk_screen);

  _gdk_quartz_window_init_windowing (_gdk_display, _gdk_screen);

  _gdk_quartz_events_init ();

  gdk_quartz_display_init_input (_gdk_display);

  gdk_quartz_display_init_display_link (_gdk_display);

#if 0
  /* FIXME: Remove the #if 0 when we have these functions */
  _gdk_quartz_dnd_init ();
#endif

  g_signal_emit_by_name (_gdk_display, "opened");

  return _gdk_display;
}

static const gchar *
gdk_quartz_display_get_name (GdkDisplay *display)
{
  static gchar *display_name = NULL;

  if (!display_name)
    {
      GDK_QUARTZ_ALLOC_POOL;
      display_name = g_strdup ([[[NSHost currentHost] name] UTF8String]);
      GDK_QUARTZ_RELEASE_POOL;
    }

  return display_name;
}

static GdkScreen *
gdk_quartz_display_get_default_screen (GdkDisplay *display)
{
  return _gdk_screen;
}

static void
gdk_quartz_display_beep (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  NSBeep();
}

static void
gdk_quartz_display_sync (GdkDisplay *display)
{
  /* Not supported. */
}

static void
gdk_quartz_display_flush (GdkDisplay *display)
{
  /* Not supported. */
}

static gboolean
gdk_quartz_display_supports_selection_notification (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  /* FIXME: Implement */
  return FALSE;
}

static gboolean
gdk_quartz_display_request_selection_notification (GdkDisplay *display,
                                                   GdkAtom     selection)

{
  /* FIXME: Implement */
  return FALSE;
}

static gboolean
gdk_quartz_display_supports_clipboard_persistence (GdkDisplay *display)
{
  /* FIXME: Implement */
  return FALSE;
}

static gboolean
gdk_quartz_display_supports_shapes (GdkDisplay *display)
{
  /* FIXME: Implement */
  return FALSE;
}

static gboolean
gdk_quartz_display_supports_input_shapes (GdkDisplay *display)
{
  /* FIXME: Implement */
  return FALSE;
}

static void
gdk_quartz_display_store_clipboard (GdkDisplay    *display,
                                    GdkWindow     *clipboard_window,
                                    guint32        time_,
                                    const GdkAtom *targets,
                                    gint           n_targets)
{
  /* FIXME: Implement */
}


static gboolean
gdk_quartz_display_supports_composite (GdkDisplay *display)
{
  /* FIXME: Implement */
  return FALSE;
}

static GList *
gdk_quartz_display_list_devices (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_QUARTZ_DISPLAY (display)->input_devices;
}

static gulong
gdk_quartz_display_get_next_serial (GdkDisplay *display)
{
  return 0;
}

static void
gdk_quartz_display_notify_startup_complete (GdkDisplay  *display,
                                            const gchar *startup_id)
{
  /* FIXME: Implement? */
}


G_DEFINE_TYPE (GdkQuartzDisplay, gdk_quartz_display, GDK_TYPE_DISPLAY)

static void
gdk_quartz_display_init (GdkQuartzDisplay *display)
{
}

static void
gdk_quartz_display_dispose (GObject *object)
{
  GdkQuartzDisplay *display_quartz = GDK_QUARTZ_DISPLAY (object);

  g_list_foreach (display_quartz->input_devices,
                  (GFunc) g_object_run_dispose, NULL);

  G_OBJECT_CLASS (gdk_quartz_display_parent_class)->dispose (object);
}

static void
gdk_quartz_display_finalize (GObject *object)
{
  GdkQuartzDisplay *display_quartz = GDK_QUARTZ_DISPLAY (object);

  g_list_free_full (display_quartz->input_devices, g_object_unref);

  g_source_unref (display_quartz->frame_source);
  display_quartz->windows_awaiting_frame = NULL;

  G_OBJECT_CLASS (gdk_quartz_display_parent_class)->finalize (object);
}

static void
gdk_quartz_display_class_init (GdkQuartzDisplayClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (class);

  object_class->finalize = gdk_quartz_display_finalize;
  object_class->dispose = gdk_quartz_display_dispose;

  display_class->window_type = GDK_TYPE_QUARTZ_WINDOW;

  display_class->get_name = gdk_quartz_display_get_name;
  display_class->get_default_screen = gdk_quartz_display_get_default_screen;
  display_class->beep = gdk_quartz_display_beep;
  display_class->sync = gdk_quartz_display_sync;
  display_class->flush = gdk_quartz_display_flush;
  display_class->queue_events = _gdk_quartz_display_queue_events;
  display_class->has_pending = _gdk_quartz_display_has_pending;
  display_class->get_default_group = gdk_quartz_display_get_default_group;
  display_class->supports_selection_notification = gdk_quartz_display_supports_selection_notification;
  display_class->request_selection_notification = gdk_quartz_display_request_selection_notification;
  display_class->supports_clipboard_persistence = gdk_quartz_display_supports_clipboard_persistence;
  display_class->store_clipboard = gdk_quartz_display_store_clipboard;
  display_class->supports_shapes = gdk_quartz_display_supports_shapes;
  display_class->supports_input_shapes = gdk_quartz_display_supports_input_shapes;
  display_class->supports_composite = gdk_quartz_display_supports_composite;
  display_class->list_devices = gdk_quartz_display_list_devices;
  display_class->get_cursor_for_type = _gdk_quartz_display_get_cursor_for_type;
  display_class->get_cursor_for_name = _gdk_quartz_display_get_cursor_for_name;
  display_class->get_cursor_for_surface = _gdk_quartz_display_get_cursor_for_surface;
  display_class->get_default_cursor_size = _gdk_quartz_display_get_default_cursor_size;
  display_class->get_maximal_cursor_size = _gdk_quartz_display_get_maximal_cursor_size;
  display_class->supports_cursor_alpha = _gdk_quartz_display_supports_cursor_alpha;
  display_class->supports_cursor_color = _gdk_quartz_display_supports_cursor_color;

  display_class->before_process_all_updates = _gdk_quartz_display_before_process_all_updates;
  display_class->after_process_all_updates = _gdk_quartz_display_after_process_all_updates;
  display_class->get_next_serial = gdk_quartz_display_get_next_serial;
  display_class->notify_startup_complete = gdk_quartz_display_notify_startup_complete;
  display_class->event_data_copy = _gdk_quartz_display_event_data_copy;
  display_class->event_data_free = _gdk_quartz_display_event_data_free;
  display_class->create_window_impl = _gdk_quartz_display_create_window_impl;
  display_class->get_keymap = _gdk_quartz_display_get_keymap;
  display_class->get_selection_owner = _gdk_quartz_display_get_selection_owner;
  display_class->set_selection_owner = _gdk_quartz_display_set_selection_owner;
  display_class->get_selection_property = _gdk_quartz_display_get_selection_property;
  display_class->convert_selection = _gdk_quartz_display_convert_selection;
  display_class->text_property_to_utf8_list = _gdk_quartz_display_text_property_to_utf8_list;
  display_class->utf8_to_string_target = _gdk_quartz_display_utf8_to_string_target;

  ProcessSerialNumber psn = { 0, kCurrentProcess };

  /* Make the current process a foreground application, i.e. an app
   * with a user interface, in case we're not running from a .app bundle
   */
  TransformProcessType (&psn, kProcessTransformToForegroundApplication);
}
