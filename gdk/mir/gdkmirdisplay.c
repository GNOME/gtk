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
#include "gdkmonitorprivate.h"
#include "gdkinternals.h"

#include "gdkmir.h"
#include "gdkmir-private.h"

#include <string.h>

#include <com/ubuntu/content/glib/content-hub-glib.h>

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

  const MirDisplayConfig *config;
  GList *monitors;

  /* Event source */
  GdkMirEventSource *event_source;

  /* Serial number? */
  gulong serial;

  /* Screen information */
  GdkScreen *screen;

  GdkKeymap *keymap;

  GdkWindow *focused_window;

  MirPixelFormat sw_pixel_format;
  MirPixelFormat hw_pixel_format;

  EGLDisplay egl_display;
  guint have_egl_khr_create_context : 1;
  guint have_egl_buffer_age : 1;
  guint have_egl_swap_buffers_with_damage : 1;
  guint have_egl_surfaceless_context : 1;

  ContentHubService *content_service;
  ContentHubHandler *content_handler;
  GVariant *paste_data;
} GdkMirDisplay;

typedef struct GdkMirDisplayClass
{
  GdkDisplayClass parent_class;
} GdkMirDisplayClass;

static void get_pixel_formats (MirConnection *, MirPixelFormat *sw, MirPixelFormat *hw);

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

static void
config_changed_cb (MirConnection *connection,
                   void          *context)
{
  GdkMirDisplay *display = context;
  GdkMonitor *monitor;
  const MirOutput *output;
  const MirOutputMode *mode;
  gint i;

  g_list_free_full (display->monitors, g_object_unref);
  g_clear_pointer (&display->config, mir_display_config_release);

  display->config = mir_connection_create_display_configuration (display->connection);
  display->monitors = NULL;

  for (i = mir_display_config_get_num_outputs (display->config) - 1; i >= 0; i--)
    {
      output = mir_display_config_get_output (display->config, i);

      if (!mir_output_is_enabled (output))
        continue;

      mode = mir_output_get_current_mode (output);
      monitor = gdk_monitor_new (GDK_DISPLAY (display));

      gdk_monitor_set_position (monitor,
                                mir_output_get_position_x (output),
                                mir_output_get_position_y (output));

      gdk_monitor_set_size (monitor,
                            mir_output_mode_get_width (mode),
                            mir_output_mode_get_height (mode));

      gdk_monitor_set_physical_size (monitor,
                                     mir_output_get_physical_width_mm (output),
                                     mir_output_get_physical_height_mm (output));

      gdk_monitor_set_scale_factor (monitor,
                                    (gint) mir_output_get_scale_factor (output));

      gdk_monitor_set_refresh_rate (monitor,
                                    (gint) mir_output_mode_get_refresh_rate (mode));

      display->monitors = g_list_prepend (display->monitors, monitor);
    }
}

static void
pasteboard_changed_cb (GdkMirDisplay *display,
                       gpointer       user_data)
{
  g_clear_pointer (&display->paste_data, g_variant_unref);
}

GdkDisplay *
_gdk_mir_display_open (const gchar *display_name)
{
  MirConnection *connection;
  MirPixelFormat sw_pixel_format, hw_pixel_format;
  GdkMirDisplay *display;
  GDBusConnection *session;

  connection = mir_connect_sync (NULL, g_get_prgname ());
  if (!connection)
     return NULL;

  if (!mir_connection_is_valid (connection))
    {
      mir_connection_release (connection);
      return NULL;
    }

  get_pixel_formats (connection, &sw_pixel_format, &hw_pixel_format);

  if (sw_pixel_format == mir_pixel_format_invalid ||
      hw_pixel_format == mir_pixel_format_invalid)
    {
      g_printerr ("Mir display does not support required pixel formats\n");
      mir_connection_release (connection);
      return NULL;
    }

  display = g_object_new (GDK_TYPE_MIR_DISPLAY, NULL);

  display->connection = connection;
  config_changed_cb (display->connection, display);
  mir_connection_set_display_config_change_callback (display->connection, config_changed_cb, display);

  GDK_DISPLAY (display)->device_manager = _gdk_mir_device_manager_new (GDK_DISPLAY (display));
  display->screen = _gdk_mir_screen_new (GDK_DISPLAY (display));
  display->sw_pixel_format = sw_pixel_format;
  display->hw_pixel_format = hw_pixel_format;

  session = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  display->content_service = content_hub_service_proxy_new_sync (
    session,
    G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
    "com.ubuntu.content.dbus.Service",
    "/",
    NULL,
    NULL);

  g_signal_connect_swapped (
    display->content_service,
    "pasteboard-changed",
    G_CALLBACK (pasteboard_changed_cb),
    display);

  display->content_handler = content_hub_handler_skeleton_new ();

  g_dbus_interface_skeleton_export (
    G_DBUS_INTERFACE_SKELETON (display->content_handler),
    session,
    "/org/gnome/gtk/content/handler",
    NULL);

  g_object_unref (session);

  content_hub_service_call_register_import_export_handler_sync (
    display->content_service,
    g_application_get_application_id (g_application_get_default ()),
    "/org/gnome/gtk/content/handler",
    NULL,
    NULL);

  content_hub_service_call_handler_active_sync (
    display->content_service,
    g_application_get_application_id (g_application_get_default ()),
    NULL,
    NULL);

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

  g_clear_pointer (&display->paste_data, g_variant_unref);
  g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (display->content_handler));
  g_clear_object (&display->content_handler);
  g_clear_object (&display->content_service);
  g_clear_object (&display->screen);
  g_clear_object (&display->keymap);
  g_clear_pointer (&display->event_source, g_source_unref);

  g_list_free_full (display->monitors, g_object_unref);
  display->monitors = NULL;

  G_OBJECT_CLASS (gdk_mir_display_parent_class)->dispose (object);
}

static void
gdk_mir_display_finalize (GObject *object)
{
  GdkMirDisplay *display = GDK_MIR_DISPLAY (object);

  g_clear_pointer (&display->config, mir_display_config_release);

  mir_connection_release (display->connection);

  G_OBJECT_CLASS (gdk_mir_display_parent_class)->finalize (object);
}

static const gchar *
gdk_mir_display_get_name (GdkDisplay *display)
{
  return "Mir";
}

static GdkScreen *
gdk_mir_display_get_default_screen (GdkDisplay *display)
{
  return GDK_MIR_DISPLAY (display)->screen;
}

static void
gdk_mir_display_beep (GdkDisplay *display)
{
  /* No system level beep... */
}

static void
gdk_mir_display_sync (GdkDisplay *display)
{
}

static void
gdk_mir_display_flush (GdkDisplay *display)
{
}

static gboolean
gdk_mir_display_has_pending (GdkDisplay *display)
{
  /* We don't need to poll for events - so nothing pending */
  return FALSE;
}

static void
gdk_mir_display_queue_events (GdkDisplay *display)
{
  /* We don't need to poll for events - so don't do anything*/
}

static void
gdk_mir_display_make_default (GdkDisplay *display)
{
}

static GdkWindow *
gdk_mir_display_get_default_group (GdkDisplay *display)
{
  return NULL;
}

static gboolean
gdk_mir_display_supports_shapes (GdkDisplay *display)
{
  /* Mir doesn't support shaped windows */
  return FALSE;
}

static gboolean
gdk_mir_display_supports_input_shapes (GdkDisplay *display)
{
  return FALSE;
}

static gboolean
gdk_mir_display_supports_clipboard_persistence (GdkDisplay *display)
{
  return FALSE;
}

static gboolean
gdk_mir_display_supports_cursor_alpha (GdkDisplay *display)
{
  return FALSE;
}

static gboolean
gdk_mir_display_supports_cursor_color (GdkDisplay *display)
{
  return FALSE;
}

static gboolean
gdk_mir_display_supports_selection_notification (GdkDisplay *display)
{
  return FALSE;
}

static gboolean
gdk_mir_display_request_selection_notification (GdkDisplay *display,
                                                GdkAtom     selection)
{
  return FALSE;
}

static void
gdk_mir_display_store_clipboard (GdkDisplay    *display,
                                 GdkWindow     *clipboard_window,
                                 guint32        time_,
                                 const GdkAtom *targets,
                                 gint           n_targets)
{
}

static void
gdk_mir_display_get_default_cursor_size (GdkDisplay *display,
                                         guint      *width,
                                         guint      *height)
{
  *width = *height = 32; // FIXME: Random value
}

static void
gdk_mir_display_get_maximal_cursor_size (GdkDisplay *display,
                                         guint      *width,
                                         guint      *height)
{
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
  return NULL;
}

static GdkAppLaunchContext *
gdk_mir_display_get_app_launch_context (GdkDisplay *display)
{
  return NULL;
}

static gulong
gdk_mir_display_get_next_serial (GdkDisplay *display)
{
  return GDK_MIR_DISPLAY (display)->serial++;
}

static void
gdk_mir_display_notify_startup_complete (GdkDisplay  *display,
                                         const gchar *startup_id)
{
}

static void
gdk_mir_display_create_window_impl (GdkDisplay    *display,
                                    GdkWindow     *window,
                                    GdkWindow     *real_parent,
                                    GdkScreen     *screen,
                                    GdkEventMask   event_mask,
                                    GdkWindowAttr *attributes)
{
  if (attributes->wclass == GDK_INPUT_OUTPUT)
    {
      window->impl = _gdk_mir_window_impl_new (display, window);
      window->impl_window = window;
    }
  else /* attributes->wclass == GDK_INPUT_ONLY */
    {
      window->impl = g_object_ref (real_parent->impl);
      window->impl_window = real_parent;

      /* FIXME: this is called in gdk_window_new, which sets window->impl_window
       * back to window after this function returns. */
    }
}

static GdkKeymap *
gdk_mir_display_get_keymap (GdkDisplay *display)
{
  return GDK_MIR_DISPLAY (display)->keymap;
}

static void
gdk_mir_display_push_error_trap (GdkDisplay *display)
{
}

static gint
gdk_mir_display_pop_error_trap (GdkDisplay *display,
                                gboolean    ignored)
{
  return 0;
}

static GdkWindow *
gdk_mir_display_get_selection_owner (GdkDisplay *display,
                                     GdkAtom     selection)
{
  return NULL;
}

static gboolean
gdk_mir_display_set_selection_owner (GdkDisplay *display,
                                     GdkWindow  *owner,
                                     GdkAtom     selection,
                                     guint32     time,
                                     gboolean    send_event)
{
  GdkEvent *event;

  if (selection == GDK_SELECTION_CLIPBOARD)
    {
      if (owner)
        {
          event = gdk_event_new (GDK_SELECTION_REQUEST);
          event->selection.window = g_object_ref (owner);
          event->selection.send_event = FALSE;
          event->selection.selection = selection;
          event->selection.target = gdk_atom_intern_static_string ("TARGETS");
          event->selection.property = gdk_atom_intern_static_string ("AVAILABLE_TARGETS");
          event->selection.time = GDK_CURRENT_TIME;
          event->selection.requestor = g_object_ref (owner);

          gdk_event_put (event);
          gdk_event_free (event);

          return TRUE;
        }
    }

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
}

static gint
gdk_mir_display_get_selection_property (GdkDisplay  *display,
                                        GdkWindow   *requestor,
                                        guchar     **data,
                                        GdkAtom     *ret_type,
                                        gint        *ret_format)
{
  gint length;

  gdk_property_get (requestor,
                    gdk_atom_intern_static_string ("GDK_SELECTION"),
                    GDK_NONE,
                    0,
                    G_MAXULONG,
                    FALSE,
                    ret_type,
                    ret_format,
                    &length,
                    data);

  return length;
}

static gint
get_format_score (const gchar *format,
                  GdkAtom      target,
                  GdkAtom     *out_type,
                  gint        *out_size)
{
  const gchar *target_string;
  GdkAtom dummy_type;
  gint dummy_size;

  target_string = _gdk_atom_name_const (target);

  if (!out_type)
    out_type = &dummy_type;

  if (!out_size)
    out_size = &dummy_size;

  if (!g_ascii_strcasecmp (format, target_string))
    {
      *out_type = GDK_SELECTION_TYPE_STRING;
      *out_size = sizeof (guchar);

      return G_MAXINT;
    }

  if (target == gdk_atom_intern_static_string ("UTF8_STRING"))
    return get_format_score (format, gdk_atom_intern_static_string ("text/plain;charset=utf-8"), out_type, out_size);

  /* TODO: use best media type for COMPOUND_TEXT target */
  if (target == gdk_atom_intern_static_string ("COMPOUND_TEXT"))
    return get_format_score (format, gdk_atom_intern_static_string ("text/plain;charset=utf-8"), out_type, out_size);

  if (target == GDK_TARGET_STRING)
    return get_format_score (format, gdk_atom_intern_static_string ("text/plain;charset=iso-8859-1"), out_type, out_size);

  if (target == gdk_atom_intern_static_string ("GTK_TEXT_BUFFER_CONTENTS"))
    return get_format_score (format, gdk_atom_intern_static_string ("text/plain;charset=utf-8"), out_type, out_size);

  if (g_content_type_is_a (format, target_string))
    {
      *out_type = GDK_SELECTION_TYPE_STRING;
      *out_size = sizeof (guchar);

      return 2;
    }

  if (g_content_type_is_a (target_string, format))
    {
      *out_type = GDK_SELECTION_TYPE_STRING;
      *out_size = sizeof (guchar);

      return 1;
    }

  return 0;
}

static gint
get_best_format_index (const gchar * const *formats,
                       guint                n_formats,
                       GdkAtom              target,
                       GdkAtom             *out_type,
                       gint                *out_size)
{
  gint best_i = -1;
  gint best_score = 0;
  GdkAtom best_type;
  gint best_size;
  gint score;
  GdkAtom type;
  gint size;
  gint i;

  if (!out_type)
    out_type = &best_type;

  if (!out_size)
    out_size = &best_size;

  *out_type = GDK_NONE;
  *out_size = 0;

  for (i = 0; i < n_formats; i++)
    {
      score = get_format_score (formats[i], target, &type, &size);

      if (score > best_score)
        {
          best_i = i;
          best_score = score;
          *out_type = type;
          *out_size = size;
        }
    }

  return best_i;
}

static void
gdk_mir_display_real_convert_selection (GdkDisplay *display,
                                        GdkWindow  *requestor,
                                        GdkAtom     selection,
                                        GdkAtom     target,
                                        guint32     time)
{
  GdkMirDisplay *mir_display = GDK_MIR_DISPLAY (display);
  const gchar *paste_data;
  gsize paste_size;
  const gint *paste_header;
  GPtrArray *paste_formats;
  GArray *paste_targets;
  GdkAtom paste_target;
  GdkEvent *event;
  gint best_i;
  GdkAtom best_type;
  gint best_size;
  gint i;

  g_return_if_fail (mir_display->paste_data);

  paste_data = g_variant_get_fixed_array (mir_display->paste_data, &paste_size, sizeof (guchar));
  paste_header = (const gint *) paste_data;

  if (paste_data)
    {
      paste_formats = g_ptr_array_new_full (paste_header[0], g_free);

      for (i = 0; i < paste_header[0]; i++)
        g_ptr_array_add (paste_formats, g_strndup (paste_data + paste_header[1 + 4 * i], paste_header[2 + 4 * i]));
    }
  else
    paste_formats = g_ptr_array_new_with_free_func (g_free);

  if (target == gdk_atom_intern_static_string ("TARGETS"))
    {
      paste_targets = g_array_sized_new (TRUE, FALSE, sizeof (GdkAtom), paste_formats->len);

      for (i = 0; i < paste_formats->len; i++)
        {
          paste_target = gdk_atom_intern (g_ptr_array_index (paste_formats, i), FALSE);
          g_array_append_val (paste_targets, paste_target);
        }

      gdk_property_change (requestor,
                           gdk_atom_intern_static_string ("GDK_SELECTION"),
                           GDK_SELECTION_TYPE_ATOM,
                           8 * sizeof (GdkAtom),
                           GDK_PROP_MODE_REPLACE,
                           (const guchar *) paste_targets->data,
                           paste_targets->len);

      g_array_unref (paste_targets);

      event = gdk_event_new (GDK_SELECTION_NOTIFY);
      event->selection.window = g_object_ref (requestor);
      event->selection.send_event = FALSE;
      event->selection.selection = selection;
      event->selection.target = target;
      event->selection.property = gdk_atom_intern_static_string ("GDK_SELECTION");
      event->selection.time = time;
      event->selection.requestor = g_object_ref (requestor);

      gdk_event_put (event);
      gdk_event_free (event);
    }
  else
    {
      best_i = get_best_format_index ((const gchar * const *) paste_formats->pdata,
                                      paste_formats->len,
                                      target,
                                      &best_type,
                                      &best_size);

      if (best_i >= 0)
        {
          gdk_property_change (requestor,
                               gdk_atom_intern_static_string ("GDK_SELECTION"),
                               best_type,
                               8 * best_size,
                               GDK_PROP_MODE_REPLACE,
                               (const guchar *) paste_data + paste_header[3 + 4 * best_i],
                               paste_header[4 + 4 * best_i] / best_size);

          event = gdk_event_new (GDK_SELECTION_NOTIFY);
          event->selection.window = g_object_ref (requestor);
          event->selection.send_event = FALSE;
          event->selection.selection = selection;
          event->selection.target = target;
          event->selection.property = gdk_atom_intern_static_string ("GDK_SELECTION");
          event->selection.time = time;
          event->selection.requestor = g_object_ref (requestor);

          gdk_event_put (event);
          gdk_event_free (event);
        }
    }

  g_ptr_array_unref (paste_formats);
}

typedef struct
{
  GdkDisplay *display;
  GdkWindow  *requestor;
  GdkAtom     selection;
  GdkAtom     target;
  guint32     time;
} ConvertInfo;

static void
paste_data_ready_cb (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  ContentHubService *content_service = CONTENT_HUB_SERVICE (source_object);
  ConvertInfo *info = user_data;
  GdkMirDisplay *mir_display = GDK_MIR_DISPLAY (info->display);
  gboolean result;

  g_clear_pointer (&mir_display->paste_data, g_variant_unref);

  result = content_hub_service_call_get_latest_paste_data_finish (content_service,
                                                                  &mir_display->paste_data,
                                                                  res,
                                                                  NULL);

  if (result)
    gdk_mir_display_real_convert_selection (info->display,
                                            info->requestor,
                                            info->selection,
                                            info->target,
                                            info->time);

  g_object_unref (info->requestor);
  g_object_unref (info->display);
  g_free (info);
}

static void
gdk_mir_display_convert_selection (GdkDisplay *display,
                                   GdkWindow  *requestor,
                                   GdkAtom     selection,
                                   GdkAtom     target,
                                   guint32     time)
{
  GdkMirDisplay *mir_display = GDK_MIR_DISPLAY (display);
  MirWindow *mir_window;
  MirWindowId *mir_window_id;
  ConvertInfo *info;

  if (selection != GDK_SELECTION_CLIPBOARD)
    return;
  else if (mir_display->paste_data)
    gdk_mir_display_real_convert_selection (display, requestor, selection, target, time);
  else if (mir_display->focused_window)
    {
      mir_window = _gdk_mir_window_get_mir_window (mir_display->focused_window);

      if (!mir_window)
        return;

      mir_window_id = mir_window_request_window_id_sync (mir_window);

      if (!mir_window_id)
        return;

      if (mir_window_id_is_valid (mir_window_id))
        {
          info = g_new (ConvertInfo, 1);
          info->display = g_object_ref (display);
          info->requestor = g_object_ref (requestor);
          info->selection = selection;
          info->target = target;
          info->time = time;

          content_hub_service_call_get_latest_paste_data (
            mir_display->content_service,
            mir_window_id_as_string (mir_window_id),
            NULL,
            paste_data_ready_cb,
            info);
        }

      mir_window_id_release (mir_window_id);
    }
}

static gint
gdk_mir_display_text_property_to_utf8_list (GdkDisplay    *display,
                                            GdkAtom        encoding,
                                            gint           format,
                                            const guchar  *text,
                                            gint           length,
                                            gchar       ***list)
{
  GPtrArray *array;
  const gchar *ptr;
  gsize chunk_len;
  gchar *copy;
  guint nitems;

  ptr = (const gchar *) text;
  array = g_ptr_array_new ();

  /* split text into utf-8 strings */
  while (ptr < (const gchar *) &text[length])
    {
      chunk_len = strlen (ptr);

      if (g_utf8_validate (ptr, chunk_len, NULL))
        {
          copy = g_strndup (ptr, chunk_len);
          g_ptr_array_add (array, copy);
        }

      ptr = &ptr[chunk_len + 1];
    }

  nitems = array->len;
  g_ptr_array_add (array, NULL);

  if (list)
    *list = (gchar **) g_ptr_array_free (array, FALSE);
  else
    g_ptr_array_free (array, TRUE);

  return nitems;
}

static gchar *
gdk_mir_display_utf8_to_string_target (GdkDisplay  *display,
                                       const gchar *str)
{
  return NULL;
}

static void
get_pixel_formats (MirConnection *connection,
                   MirPixelFormat *sw_pixel_format,
                   MirPixelFormat *hw_pixel_format)
{
  MirPixelFormat formats[mir_pixel_formats];
  unsigned int n_formats, i;

  mir_connection_get_available_surface_formats (connection, formats,
                                                mir_pixel_formats, &n_formats);

  if (sw_pixel_format)
    {
      *sw_pixel_format = mir_pixel_format_invalid;

      for (i = 0; i < n_formats && *sw_pixel_format == mir_pixel_format_invalid; i++)
        {
          switch (formats[i])
            {
            case mir_pixel_format_abgr_8888:
            case mir_pixel_format_xbgr_8888:
            case mir_pixel_format_argb_8888:
            case mir_pixel_format_xrgb_8888:
            case mir_pixel_format_rgb_565:
              *sw_pixel_format = formats[i];
              break;
            default:
              break;
            }
        }
    }

  if (hw_pixel_format)
    {
      *hw_pixel_format = mir_pixel_format_invalid;

      for (i = 0; i < n_formats && *hw_pixel_format == mir_pixel_format_invalid; i++)
        {
          switch (formats[i])
            {
            case mir_pixel_format_abgr_8888:
            case mir_pixel_format_xbgr_8888:
            case mir_pixel_format_argb_8888:
            case mir_pixel_format_xrgb_8888:
            case mir_pixel_format_rgb_565:
              *hw_pixel_format = formats[i];
              break;
            default:
              break;
            }
        }
    }
}

MirPixelFormat
_gdk_mir_display_get_pixel_format (GdkDisplay *display,
                                   MirBufferUsage usage)
{
  GdkMirDisplay *mir_dpy = GDK_MIR_DISPLAY (display);

  if (usage == mir_buffer_usage_hardware)
    return mir_dpy->hw_pixel_format;

  return mir_dpy->sw_pixel_format;
}

void
_gdk_mir_display_focus_window (GdkDisplay *display,
                               GdkWindow  *window)
{
  GdkMirDisplay *mir_display = GDK_MIR_DISPLAY (display);

  g_set_object (&mir_display->focused_window, window);
}

void
_gdk_mir_display_unfocus_window (GdkDisplay *display,
                                 GdkWindow  *window)
{
  GdkMirDisplay *mir_display = GDK_MIR_DISPLAY (display);

  if (window == mir_display->focused_window)
    g_clear_object (&mir_display->focused_window);
}

void
_gdk_mir_display_create_paste (GdkDisplay          *display,
                               const gchar * const *paste_formats,
                               gconstpointer        paste_data,
                               gsize                paste_size)
{
  GdkMirDisplay *mir_display = GDK_MIR_DISPLAY (display);
  MirWindow *mir_window;
  MirWindowId *mir_window_id;

  if (!mir_display->focused_window)
    return;

  mir_window = _gdk_mir_window_get_mir_window (mir_display->focused_window);

  if (!mir_window)
    return;

  mir_window_id = mir_window_request_window_id_sync (mir_window);

  if (!mir_window_id)
    return;

  if (mir_window_id_is_valid (mir_window_id))
    content_hub_service_call_create_paste_sync (
      mir_display->content_service,
      g_application_get_application_id (g_application_get_default ()),
      mir_window_id_as_string (mir_window_id),
      g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, paste_data, paste_size, sizeof (guchar)),
      paste_formats,
      NULL,
      NULL,
      NULL);

  mir_window_id_release (mir_window_id);
}

gboolean
_gdk_mir_display_init_egl_display (GdkDisplay *display)
{
  GdkMirDisplay *mir_dpy = GDK_MIR_DISPLAY (display);
  EGLint major_version, minor_version;
  EGLDisplay *dpy;

  if (mir_dpy->egl_display)
    return TRUE;

  dpy = eglGetDisplay (mir_connection_get_egl_native_display (mir_dpy->connection));
  if (dpy == NULL)
    return FALSE;

  if (!eglInitialize (dpy, &major_version, &minor_version))
    return FALSE;

  if (!eglBindAPI (EGL_OPENGL_API))
    return FALSE;

  mir_dpy->egl_display = dpy;

  mir_dpy->have_egl_khr_create_context =
    epoxy_has_egl_extension (dpy, "EGL_KHR_create_context");

  mir_dpy->have_egl_buffer_age =
    epoxy_has_egl_extension (dpy, "EGL_EXT_buffer_age");

  mir_dpy->have_egl_swap_buffers_with_damage =
    epoxy_has_egl_extension (dpy, "EGL_EXT_swap_buffers_with_damage");

  mir_dpy->have_egl_surfaceless_context =
    epoxy_has_egl_extension (dpy, "EGL_KHR_surfaceless_context");

  GDK_NOTE (OPENGL,
            g_print ("EGL API version %d.%d found\n"
                     " - Vendor: %s\n"
                     " - Version: %s\n"
                     " - Client APIs: %s\n"
                     " - Extensions:\n"
                     "\t%s\n",
                     major_version,
                     minor_version,
                     eglQueryString (dpy, EGL_VENDOR),
                     eglQueryString (dpy, EGL_VERSION),
                     eglQueryString (dpy, EGL_CLIENT_APIS),
                     eglQueryString (dpy, EGL_EXTENSIONS)));

  return TRUE;
}

static gboolean
gdk_mir_display_make_gl_context_current (GdkDisplay   *display,
                                         GdkGLContext *context)
{
  EGLDisplay egl_display = _gdk_mir_display_get_egl_display (display);
  GdkMirGLContext *mir_context;
  GdkWindow *window;
  EGLSurface egl_surface;

  if (context == NULL)
    {
      eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      return TRUE;
    }

  mir_context = GDK_MIR_GL_CONTEXT (context);
  window = gdk_gl_context_get_window (context);

  if (mir_context->is_attached || gdk_draw_context_is_drawing (GDK_DRAW_CONTEXT (context)))
    {
      egl_surface = _gdk_mir_window_get_egl_surface (window,
                                                     mir_context->egl_config);
    }
  else
    {
      if (_gdk_mir_display_have_egl_surfaceless_context (display))
        egl_surface = EGL_NO_SURFACE;
      else
        egl_surface = _gdk_mir_window_get_dummy_egl_surface (window,
                                                             mir_context->egl_config);
    }

  if (!eglMakeCurrent (egl_display, egl_surface, egl_surface, mir_context->egl_context))
    {
      g_warning ("eglMakeCurrent failed");
      return FALSE;
    }

  return TRUE;
}

EGLDisplay _gdk_mir_display_get_egl_display (GdkDisplay *display)
{
  return GDK_MIR_DISPLAY (display)->egl_display;
}

gboolean _gdk_mir_display_have_egl_khr_create_context (GdkDisplay *display)
{
  return GDK_MIR_DISPLAY (display)->have_egl_khr_create_context;
}

gboolean _gdk_mir_display_have_egl_buffer_age (GdkDisplay *display)
{
  /* FIXME: this is not really supported by mir yet (despite is advertised) */
  // return GDK_MIR_DISPLAY (display)->have_egl_buffer_age;
  return FALSE;
}

gboolean _gdk_mir_display_have_egl_swap_buffers_with_damage (GdkDisplay *display)
{
  /* FIXME: this is not really supported by mir yet (despite is advertised) */
  // return GDK_MIR_DISPLAY (display)->have_egl_swap_buffers_with_damage;
  return FALSE;
}

gboolean _gdk_mir_display_have_egl_surfaceless_context (GdkDisplay *display)
{
  return GDK_MIR_DISPLAY (display)->have_egl_surfaceless_context;
}

static int
gdk_mir_display_get_n_monitors (GdkDisplay *display)
{
  return g_list_length (GDK_MIR_DISPLAY (display)->monitors);
}

static GdkMonitor *
gdk_mir_display_get_monitor (GdkDisplay *display,
                             int         index)
{
  g_return_val_if_fail (0 <= index && index < gdk_display_get_n_monitors (display), NULL);

  return g_list_nth_data (GDK_MIR_DISPLAY (display)->monitors, index);
}

static void
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
  display_class->get_app_launch_context = gdk_mir_display_get_app_launch_context;
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
  display_class->make_gl_context_current = gdk_mir_display_make_gl_context_current;
  display_class->get_n_monitors = gdk_mir_display_get_n_monitors;
  display_class->get_monitor = gdk_mir_display_get_monitor;
}
