/*
 * Copyright © 2014 Canonical Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkdisplayprivate.h"
#include "gdkinternals.h"

#include "gdkmir.h"
#include "gdkmir-private.h"

#define GDK_TYPE_DISPLAY_MIR              (gdk_mir_display_get_type ())
#define GDK_MIR_DISPLAY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DISPLAY_MIR, GdkMirDisplay))
#define GDK_MIR_DISPLAY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DISPLAY_MIR, GdkMirDisplayClass))
#define GDK_IS_MIR_DISPLAY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_MIR_DISPLAY))
#define GDK_MIR_DISPLAY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_MIR_DISPLAY, GdkMirDisplayImplClass))

typedef struct GdkMirDisplay
{
  GdkDisplay parent_instance;

  /* Connection to Mir server */
  MirConnection *connection;

  /* Event source */
  GdkMirEventSource *event_source;

  /* Serial number? */
  gulong serial;

  /* Screen information */
  GdkScreen *screen;

  GdkKeymap *keymap;

  MirPixelFormat sw_pixel_format;
  MirPixelFormat hw_pixel_format;
} GdkMirDisplay;

typedef struct GdkMirDisplayClass
{
  GdkDisplayClass parent_class;
} GdkMirDisplayClass;

static void initialize_pixel_formats (GdkMirDisplay *display);

/**
 * SECTION:mir_interaction
 * @Short_description: Mir backend-specific functions
 * @Title: Mir Interaction
 *
 * The functions in this section are specific to the GDK Mir backend.
 * To use them, you need to include the <literal>&lt;gdk/gdkmir.h&gt;</literal>
 * header and use the Mir-specific pkg-config files to build your
 * application (either <literal>gdk-mir-3.0</literal> or
 * <literal>gtk+-mir-3.0</literal>).
 *
 * To make your code compile with other GDK backends, guard backend-specific
 * calls by an ifdef as follows. Since GDK may be built with multiple
 * backends, you should also check for the backend that is in use (e.g. by
 * using the GDK_IS_MIR_DISPLAY() macro).
 * |[
 * #ifdef GDK_WINDOWING_MIR
 *   if (GDK_IS_MIR_DISPLAY (display))
 *     {
 *       /&ast; make Mir-specific calls here &ast;/
 *     }
 *   else
 * #endif
 * #ifdef GDK_WINDOWING_X11
 *   if (GDK_IS_X11_DISPLAY (display))
 *     {
 *       /&ast; make X11-specific calls here &ast;/
 *     }
 *   else
 * #endif
 *   g_error ("Unsupported GDK backend");
 * ]|
 */

G_DEFINE_TYPE (GdkMirDisplay, gdk_mir_display, GDK_TYPE_DISPLAY)

GdkDisplay *
_gdk_mir_display_open (const gchar *display_name)
{
  MirConnection *connection;
  GdkMirDisplay *display;

  g_printerr ("gdk_mir_display_open\n");

  connection = mir_connect_sync (NULL, "GDK-Mir");
  if (!connection)
     return NULL;
  if (!mir_connection_is_valid (connection))
    {
      g_printerr ("Failed to connect to Mir: %s\n", mir_connection_get_error_message (connection));
      mir_connection_release (connection);
      return NULL;
    }

  display = g_object_new (GDK_TYPE_MIR_DISPLAY, NULL);

  display->connection = connection;
  GDK_DISPLAY (display)->device_manager = _gdk_mir_device_manager_new (GDK_DISPLAY (display));
  display->screen = _gdk_mir_screen_new (GDK_DISPLAY (display));
  initialize_pixel_formats (display);

  g_signal_emit_by_name (display, "opened");

  return GDK_DISPLAY (display);
}

/**
 * gdk_mir_display_get_mir_connection
 * @display: (type GdkMirDisplay): a #GdkDisplay
 *
 * Returns the #MirConnection for a #GdkDisplay
 *
 * Returns: (transfer none): a #MirConnection
 *
 * Since: 3.14
 */
struct MirConnection *
gdk_mir_display_get_mir_connection (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_MIR_DISPLAY (display), NULL);
  return GDK_MIR_DISPLAY (display)->connection;
}

GdkMirEventSource *
_gdk_mir_display_get_event_source (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_MIR_DISPLAY (display), NULL);

  return GDK_MIR_DISPLAY (display)->event_source;
}

static void
gdk_mir_display_dispose (GObject *object)
{
  GdkMirDisplay *display = GDK_MIR_DISPLAY (object);

  g_object_unref (display->screen);
  display->screen = NULL;

  G_OBJECT_CLASS (gdk_mir_display_parent_class)->dispose (object);
}

static void
gdk_mir_display_finalize (GObject *object)
{
  GdkMirDisplay *display = GDK_MIR_DISPLAY (object);

  mir_connection_release (display->connection);

  G_OBJECT_CLASS (gdk_mir_display_parent_class)->finalize (object);
}

static const gchar *
gdk_mir_display_get_name (GdkDisplay *display)
{
  //g_printerr ("gdk_mir_display_get_name\n");
  return "Mir";
}

static GdkScreen *
gdk_mir_display_get_default_screen (GdkDisplay *display)
{
  //g_printerr ("gdk_mir_display_get_default_screen\n");
  return GDK_MIR_DISPLAY (display)->screen;
}

static void
gdk_mir_display_beep (GdkDisplay *display)
{
  g_printerr ("gdk_mir_display_beep\n");
  /* No system level beep... */
}

static void
gdk_mir_display_sync (GdkDisplay *display)
{
  g_printerr ("gdk_mir_display_sync\n");
}

static void
gdk_mir_display_flush (GdkDisplay *display)
{
  g_printerr ("gdk_mir_display_flush\n");
}

static gboolean
gdk_mir_display_has_pending (GdkDisplay *display)
{
  g_printerr ("gdk_mir_display_has_pending\n");
  /* We don't need to poll for events - so nothing pending */
  return FALSE;
}

static void
gdk_mir_display_queue_events (GdkDisplay *display)
{
  //g_printerr ("gdk_mir_display_queue_events\n");
  /* We don't need to poll for events - so don't do anything*/
}

static void
gdk_mir_display_make_default (GdkDisplay *display)
{
  //g_printerr ("gdk_mir_display_make_default\n");
}

static GdkWindow *
gdk_mir_display_get_default_group (GdkDisplay *display)
{
  g_printerr ("gdk_mir_display_get_default_group\n");
  return NULL;
}

static gboolean
gdk_mir_display_supports_shapes (GdkDisplay *display)
{
  g_printerr ("gdk_mir_display_supports_shapes\n");
  /* Mir doesn't support shaped windows */
  return FALSE;
}

static gboolean
gdk_mir_display_supports_input_shapes (GdkDisplay *display)
{
  g_printerr ("gdk_mir_display_supports_input_shapes\n");
  return FALSE;
}

static gboolean
gdk_mir_display_supports_composite (GdkDisplay *display)
{
  g_printerr ("gdk_mir_display_supports_composite\n");
  return FALSE;
}

static gboolean
gdk_mir_display_supports_clipboard_persistence (GdkDisplay *display)
{
  g_printerr ("gdk_mir_display_supports_clipboard_persistence\n");
  return FALSE;
}

static gboolean
gdk_mir_display_supports_cursor_alpha (GdkDisplay *display)
{
  g_printerr ("gdk_mir_display_supports_cursor_alpha\n");
  return FALSE;
}

static gboolean
gdk_mir_display_supports_cursor_color (GdkDisplay *display)
{
  g_printerr ("gdk_mir_display_supports_cursor_color\n");
  return FALSE;
}

static gboolean
gdk_mir_display_supports_selection_notification (GdkDisplay *display)
{
  g_printerr ("gdk_mir_display_supports_selection_notification\n");
  return FALSE;
}

static gboolean
gdk_mir_display_request_selection_notification (GdkDisplay *display,
                                                GdkAtom     selection)
{
  g_printerr ("gdk_mir_display_request_selection_notification\n");
  return FALSE;
}

static void
gdk_mir_display_store_clipboard (GdkDisplay    *display,
                                 GdkWindow     *clipboard_window,
                                 guint32        time_,
                                 const GdkAtom *targets,
                                 gint           n_targets)
{
  g_printerr ("gdk_mir_display_store_clipboard\n");
}

static void
gdk_mir_display_get_default_cursor_size (GdkDisplay *display,
                                         guint      *width,
                                         guint      *height)
{
  g_printerr ("gdk_mir_display_get_default_cursor_size\n");
  *width = *height = 32; // FIXME: Random value
}

static void
gdk_mir_display_get_maximal_cursor_size (GdkDisplay *display,
                                         guint      *width,
                                         guint      *height)
{
  g_printerr ("gdk_mir_display_get_maximal_cursor_size\n");
  *width = *height = 32; // FIXME: Random value
}

static GdkCursor *
gdk_mir_display_get_cursor_for_type (GdkDisplay    *display,
                                     GdkCursorType  cursor_type)
{
  return _gdk_mir_cursor_new_for_type (display, cursor_type);
}

static GdkCursor *
gdk_mir_display_get_cursor_for_name (GdkDisplay  *display,
                                     const gchar *name)
{
  return _gdk_mir_cursor_new_for_name (display, name);
}

static GdkCursor *
gdk_mir_display_get_cursor_for_surface (GdkDisplay      *display,
                                        cairo_surface_t *surface,
                                        gdouble          x,
                                        gdouble          y)
{
  g_printerr ("gdk_mir_display_get_cursor_for_surface (%f, %f)\n", x, y);
  return NULL;
}

static GList *
gdk_mir_display_list_devices (GdkDisplay *display)
{
  g_printerr ("gdk_mir_display_list_devices\n");
  // FIXME: Should this access the device manager?
  return NULL;
}

static GdkAppLaunchContext *
gdk_mir_display_get_app_launch_context (GdkDisplay *display)
{
  g_printerr ("gdk_mir_display_get_app_launch_context\n");
  return NULL;
}

static void
gdk_mir_display_before_process_all_updates (GdkDisplay *display)
{
  g_printerr ("gdk_mir_display_before_process_all_updates\n");
}

static void
gdk_mir_display_after_process_all_updates (GdkDisplay *display)
{
  g_printerr ("gdk_mir_display_after_process_all_updates\n");
}

static gulong
gdk_mir_display_get_next_serial (GdkDisplay *display)
{
  //g_printerr ("gdk_mir_display_get_next_serial\n");
  return GDK_MIR_DISPLAY (display)->serial++;
}

static void
gdk_mir_display_notify_startup_complete (GdkDisplay  *display,
                                         const gchar *startup_id)
{
  //g_printerr ("gdk_mir_display_notify_startup_complete\n");
}

static void
gdk_mir_display_create_window_impl (GdkDisplay    *display,
                                    GdkWindow     *window,
                                    GdkWindow     *real_parent,
                                    GdkScreen     *screen,
                                    GdkEventMask   event_mask,
                                    GdkWindowAttr *attributes,
                                    gint           attributes_mask)
{
  g_printerr ("gdk_mir_display_create_window_impl");
  g_printerr (" window=%p", window);
  g_printerr (" location=(%d, %d)", window->x, window->y);
  g_printerr (" size=(%d, %d)", window->width, window->height);
  g_printerr ("\n");
  if (attributes->wclass != GDK_INPUT_OUTPUT)
    return;
  window->impl = _gdk_mir_window_impl_new ();
}

static GdkKeymap *
gdk_mir_display_get_keymap (GdkDisplay *display)
{
  //g_printerr ("gdk_mir_display_get_keymap\n");
  return GDK_MIR_DISPLAY (display)->keymap;
}

static void
gdk_mir_display_push_error_trap (GdkDisplay *display)
{
  g_printerr ("gdk_mir_display_push_error_trap\n");
}

static gint
gdk_mir_display_pop_error_trap (GdkDisplay *display,
                                gboolean    ignored)
{
  g_printerr ("gdk_mir_display_pop_error_trap\n");
  return 0;
}

static GdkWindow *
gdk_mir_display_get_selection_owner (GdkDisplay *display,
                                     GdkAtom     selection)
{
  g_printerr ("gdk_mir_display_get_selection_owner\n");
  return NULL;
}

static gboolean
gdk_mir_display_set_selection_owner (GdkDisplay *display,
                                     GdkWindow  *owner,
                                     GdkAtom     selection,
                                     guint32     time,
                                     gboolean    send_event)
{
  g_printerr ("gdk_mir_display_set_selection_owner\n");
  return FALSE;
}

static void
gdk_mir_display_send_selection_notify (GdkDisplay *display,
                                       GdkWindow  *requestor,
                                       GdkAtom     selection,
                                       GdkAtom     target,
                                       GdkAtom     property,
                                       guint32     time)
{
  g_printerr ("gdk_mir_display_send_selection_notify\n");
}

static gint
gdk_mir_display_get_selection_property (GdkDisplay  *display,
                                        GdkWindow   *requestor,
                                        guchar     **data,
                                        GdkAtom     *ret_type,
                                        gint        *ret_format)
{
  g_printerr ("gdk_mir_display_get_selection_property\n");
  return 0;
}

static void
gdk_mir_display_convert_selection (GdkDisplay *display,
                                   GdkWindow  *requestor,
                                   GdkAtom     selection,
                                   GdkAtom     target,
                                   guint32     time)
{
  g_printerr ("gdk_mir_display_convert_selection\n");
}

static gint
gdk_mir_display_text_property_to_utf8_list (GdkDisplay    *display,
                                            GdkAtom        encoding,
                                            gint           format,
                                            const guchar  *text,
                                            gint           length,
                                            gchar       ***list)
{
  g_printerr ("gdk_mir_display_text_property_to_utf8_list\n");
  return 0;
}

static gchar *
gdk_mir_display_utf8_to_string_target (GdkDisplay  *display,
                                       const gchar *str)
{
  g_printerr ("gdk_mir_display_utf8_to_string_target\n");
  return NULL;
}

static void
initialize_pixel_formats (GdkMirDisplay *display)
{
  MirPixelFormat formats[mir_pixel_formats];
  unsigned int n_formats, i;

  mir_connection_get_available_surface_formats (display->connection, formats,
                                                mir_pixel_formats, &n_formats);

  display->sw_pixel_format = mir_pixel_format_invalid;
  display->hw_pixel_format = mir_pixel_format_invalid;

  for (i = 0; i < n_formats; i++)
    {
      switch (formats[i])
      {
        case mir_pixel_format_abgr_8888:
        case mir_pixel_format_xbgr_8888:
        case mir_pixel_format_argb_8888:
        case mir_pixel_format_xrgb_8888:
          display->hw_pixel_format = formats[i];
          break;
        default:
          continue;
      }

      if (display->hw_pixel_format != mir_pixel_format_invalid)
        break;
    }

  for (i = 0; i < n_formats; i++)
    {
      if (formats[i] == mir_pixel_format_argb_8888)
        {
          display->sw_pixel_format = formats[i];
          break;
        }
    }
}

gdk_mir_display_init (GdkMirDisplay *display)
{
  display->event_source = _gdk_mir_event_source_new (GDK_DISPLAY (display));
  display->keymap = _gdk_mir_keymap_new ();
}

static void
gdk_mir_display_class_init (GdkMirDisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (klass);

  object_class->dispose = gdk_mir_display_dispose;
  object_class->finalize = gdk_mir_display_finalize;

  display_class->window_type = gdk_mir_window_get_type ();

  display_class->get_name = gdk_mir_display_get_name;
  display_class->get_default_screen = gdk_mir_display_get_default_screen;
  display_class->beep = gdk_mir_display_beep;
  display_class->sync = gdk_mir_display_sync;
  display_class->flush = gdk_mir_display_flush;
  display_class->has_pending = gdk_mir_display_has_pending;
  display_class->queue_events = gdk_mir_display_queue_events;
  display_class->make_default = gdk_mir_display_make_default;
  display_class->get_default_group = gdk_mir_display_get_default_group;
  display_class->supports_shapes = gdk_mir_display_supports_shapes;
  display_class->supports_input_shapes = gdk_mir_display_supports_input_shapes;
  display_class->supports_composite = gdk_mir_display_supports_composite;
  display_class->supports_clipboard_persistence = gdk_mir_display_supports_clipboard_persistence;
  display_class->supports_cursor_alpha = gdk_mir_display_supports_cursor_alpha;
  display_class->supports_cursor_color = gdk_mir_display_supports_cursor_color;
  display_class->supports_selection_notification = gdk_mir_display_supports_selection_notification;
  display_class->request_selection_notification = gdk_mir_display_request_selection_notification;
  display_class->store_clipboard = gdk_mir_display_store_clipboard;
  display_class->get_default_cursor_size = gdk_mir_display_get_default_cursor_size;
  display_class->get_maximal_cursor_size = gdk_mir_display_get_maximal_cursor_size;
  display_class->get_cursor_for_type = gdk_mir_display_get_cursor_for_type;
  display_class->get_cursor_for_name = gdk_mir_display_get_cursor_for_name;
  display_class->get_cursor_for_surface = gdk_mir_display_get_cursor_for_surface;
  display_class->list_devices = gdk_mir_display_list_devices;
  display_class->get_app_launch_context = gdk_mir_display_get_app_launch_context;
  display_class->before_process_all_updates = gdk_mir_display_before_process_all_updates;
  display_class->after_process_all_updates = gdk_mir_display_after_process_all_updates;
  display_class->get_next_serial = gdk_mir_display_get_next_serial;
  display_class->notify_startup_complete = gdk_mir_display_notify_startup_complete;
  display_class->create_window_impl = gdk_mir_display_create_window_impl;
  display_class->get_keymap = gdk_mir_display_get_keymap;
  display_class->push_error_trap = gdk_mir_display_push_error_trap;
  display_class->pop_error_trap = gdk_mir_display_pop_error_trap;
  display_class->get_selection_owner = gdk_mir_display_get_selection_owner;
  display_class->set_selection_owner = gdk_mir_display_set_selection_owner;
  display_class->send_selection_notify = gdk_mir_display_send_selection_notify;
  display_class->get_selection_property = gdk_mir_display_get_selection_property;
  display_class->convert_selection = gdk_mir_display_convert_selection;
  display_class->text_property_to_utf8_list = gdk_mir_display_text_property_to_utf8_list;
  display_class->utf8_to_string_target = gdk_mir_display_utf8_to_string_target;
}
