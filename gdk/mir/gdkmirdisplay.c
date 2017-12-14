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
  GError *error = NULL;

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

  session = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

  if (session == NULL)
    {
      g_error ("Error connecting to D-Bus session bus: %s", error->message);
      g_clear_error (&error);
      mir_connection_release (connection);
      return NULL;
    }

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
gdk_mir_display_supports_cursor_alpha (GdkDisplay *display)
{
  return FALSE;
}

static gboolean
gdk_mir_display_supports_cursor_color (GdkDisplay *display)
{
  return FALSE;
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

static gboolean
gdk_mir_display_get_setting (GdkDisplay *display,
                             const char *name,
                             GValue     *value)
{
  return gdk_mir_screen_get_setting (GDK_MIR_DISPLAY (display)->screen, name, value);
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
  display_class->supports_cursor_alpha = gdk_mir_display_supports_cursor_alpha;
  display_class->supports_cursor_color = gdk_mir_display_supports_cursor_color;
  display_class->get_default_cursor_size = gdk_mir_display_get_default_cursor_size;
  display_class->get_maximal_cursor_size = gdk_mir_display_get_maximal_cursor_size;
  display_class->get_app_launch_context = gdk_mir_display_get_app_launch_context;
  display_class->get_next_serial = gdk_mir_display_get_next_serial;
  display_class->notify_startup_complete = gdk_mir_display_notify_startup_complete;
  display_class->create_window_impl = gdk_mir_display_create_window_impl;
  display_class->get_keymap = gdk_mir_display_get_keymap;
  display_class->push_error_trap = gdk_mir_display_push_error_trap;
  display_class->pop_error_trap = gdk_mir_display_pop_error_trap;
  display_class->text_property_to_utf8_list = gdk_mir_display_text_property_to_utf8_list;
  display_class->utf8_to_string_target = gdk_mir_display_utf8_to_string_target;
  display_class->make_gl_context_current = gdk_mir_display_make_gl_context_current;
  display_class->get_n_monitors = gdk_mir_display_get_n_monitors;
  display_class->get_monitor = gdk_mir_display_get_monitor;
  display_class->get_setting = gdk_mir_display_get_setting;
}
