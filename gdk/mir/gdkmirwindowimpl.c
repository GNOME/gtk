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

#include <inttypes.h>
#include <math.h>

#include "config.h"

#include "gdk.h"
#include "gdkmir.h"
#include "gdkmir-private.h"

#include "gdkwindowimpl.h"
#include "gdkinternals.h"
#include "gdkintl.h"
#include "gdkdisplayprivate.h"
#include "gdkdeviceprivate.h"

#define GDK_MIR_WINDOW_IMPL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WINDOW_IMPL_MIR, GdkMirWindowImplClass))
#define GDK_IS_WINDOW_IMPL_MIR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WINDOW_IMPL_MIR))
#define GDK_MIR_WINDOW_IMPL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WINDOW_IMPL_MIR, GdkMirWindowImplClass))

#define MAX_EGL_ATTRS 30

typedef struct
{
  GdkAtom type;
  GArray *array;
} GdkMirProperty;

static GdkMirProperty *
gdk_mir_property_new (GdkAtom type,
                      guint   format,
                      guint   capacity)
{
  GdkMirProperty *property = g_slice_new (GdkMirProperty);

  property->type = type;
  property->array = g_array_sized_new (TRUE, FALSE, format, capacity);

  return property;
}

static void
gdk_mir_property_free (gpointer data)
{
  GdkMirProperty *property = data;

  if (!property)
    return;

  g_array_unref (property->array);
  g_slice_free (GdkMirProperty, property);
}

typedef struct _GdkMirWindowImplClass GdkMirWindowImplClass;

struct _GdkMirWindowImpl
{
  GdkWindowImpl parent_instance;

  GHashTable *properties;

  /* Window we are temporary for */
  GdkWindow *transient_for;
  gint transient_x;
  gint transient_y;

  /* gdk_window_move_to_rect */
  gboolean            has_rect;
  GdkRectangle        rect;
  MirRectangle        mir_rect;
  MirPlacementGravity rect_anchor;
  MirPlacementGravity window_anchor;
  MirPlacementHints   anchor_hints;
  gint                rect_anchor_dx;
  gint                rect_anchor_dy;

  /* Desired window attributes */
  GdkWindowTypeHint type_hint;
  MirWindowState window_state;
  gboolean modal;

  /* Current button state for checking which buttons are being pressed / released */
  gdouble x;
  gdouble y;
  guint button_state;

  GdkDisplay *display;

  /* Window being rendered to (only exists when visible) */
  MirWindow *mir_window;
  MirBufferStream *buffer_stream;
  MirBufferUsage buffer_usage;

  /* Cairo context for current frame */
  cairo_surface_t *cairo_surface;

  gchar *title;

  GdkGeometry geometry_hints;
  GdkWindowHints geometry_mask;

  /* Egl surface for the current mir window */
  EGLSurface egl_surface;

  /* Dummy MIR and EGL surfaces */
  EGLSurface dummy_egl_surface;

  /* TRUE if the window can be seen */
  gboolean visible;

  /* TRUE if cursor is inside this window */
  gboolean cursor_inside;

  gboolean pending_spec_update;
  gint output_scale;
};

struct _GdkMirWindowImplClass
{
  GdkWindowImplClass parent_class;
};

G_DEFINE_TYPE (GdkMirWindowImpl, gdk_mir_window_impl, GDK_TYPE_WINDOW_IMPL)

static cairo_surface_t *gdk_mir_window_impl_ref_cairo_surface (GdkWindow *window);
static void ensure_mir_window (GdkWindow *window);

static gboolean
type_hint_differs (GdkWindowTypeHint lhs, GdkWindowTypeHint rhs)
{
    if (lhs == rhs)
      return FALSE;

    switch (lhs)
      {
      case GDK_WINDOW_TYPE_HINT_DIALOG:
      case GDK_WINDOW_TYPE_HINT_DOCK:
        return rhs != GDK_WINDOW_TYPE_HINT_DIALOG &&
            rhs != GDK_WINDOW_TYPE_HINT_DOCK;
      case GDK_WINDOW_TYPE_HINT_MENU:
      case GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU:
      case GDK_WINDOW_TYPE_HINT_POPUP_MENU:
      case GDK_WINDOW_TYPE_HINT_TOOLBAR:
      case GDK_WINDOW_TYPE_HINT_COMBO:
      case GDK_WINDOW_TYPE_HINT_DND:
      case GDK_WINDOW_TYPE_HINT_TOOLTIP:
      case GDK_WINDOW_TYPE_HINT_NOTIFICATION:
        return rhs != GDK_WINDOW_TYPE_HINT_MENU &&
            rhs != GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU &&
            rhs != GDK_WINDOW_TYPE_HINT_POPUP_MENU &&
            rhs != GDK_WINDOW_TYPE_HINT_TOOLBAR &&
            rhs != GDK_WINDOW_TYPE_HINT_COMBO &&
            rhs != GDK_WINDOW_TYPE_HINT_DND &&
            rhs != GDK_WINDOW_TYPE_HINT_TOOLTIP &&
            rhs != GDK_WINDOW_TYPE_HINT_NOTIFICATION;
      case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
      case GDK_WINDOW_TYPE_HINT_UTILITY:
        return rhs != GDK_WINDOW_TYPE_HINT_SPLASHSCREEN &&
            rhs != GDK_WINDOW_TYPE_HINT_UTILITY;
      case GDK_WINDOW_TYPE_HINT_NORMAL:
      case GDK_WINDOW_TYPE_HINT_DESKTOP:
      default:
        return rhs != GDK_WINDOW_TYPE_HINT_NORMAL &&
            rhs != GDK_WINDOW_TYPE_HINT_DESKTOP;
      }
}

static void
drop_cairo_surface (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  g_clear_pointer (&impl->cairo_surface, cairo_surface_destroy);
}

static const gchar *
get_default_title (void)
{
  const char *title;

  title = g_get_application_name ();
  if (!title)
    title = g_get_prgname ();
  if (!title)
    title = "";

  return title;
}

GdkWindowImpl *
_gdk_mir_window_impl_new (GdkDisplay *display, GdkWindow *window)
{
  GdkMirWindowImpl *impl = g_object_new (GDK_TYPE_MIR_WINDOW_IMPL, NULL);

  impl->display = display;

  impl->title = g_strdup (get_default_title ());

  impl->pending_spec_update = TRUE;

  return (GdkWindowImpl *) impl;
}

void
_gdk_mir_window_impl_set_window_state (GdkMirWindowImpl *impl,
                                       MirWindowState    state)
{
  impl->window_state = state;
}

void
_gdk_mir_window_impl_set_window_type (GdkMirWindowImpl *impl,
                                      MirWindowType     type)
{
}

void
_gdk_mir_window_impl_set_cursor_state (GdkMirWindowImpl *impl,
                                       gdouble x,
                                       gdouble y,
                                       gboolean cursor_inside,
                                       guint button_state)
{
  impl->x = x;
  impl->y = y;
  impl->cursor_inside = cursor_inside;
  impl->button_state = button_state;
}

void
_gdk_mir_window_impl_get_cursor_state (GdkMirWindowImpl *impl,
                                       gdouble *x,
                                       gdouble *y,
                                       gboolean *cursor_inside,
                                       guint *button_state)
{
  if (x)
    *x = impl->x;
  if (y)
    *y = impl->y;
  if (cursor_inside)
    *cursor_inside = impl->cursor_inside;
  if (button_state)
    *button_state = impl->button_state;
}

static void
gdk_mir_window_impl_init (GdkMirWindowImpl *impl)
{
  impl->properties = g_hash_table_new_full (NULL, NULL, NULL, gdk_mir_property_free);
  impl->type_hint = GDK_WINDOW_TYPE_HINT_NORMAL;
  impl->window_state = mir_window_state_unknown;
  impl->output_scale = 1;
}

static void
set_window_state (GdkMirWindowImpl *impl,
                  MirWindowState    state)
{
  MirConnection *connection = gdk_mir_display_get_mir_connection (impl->display);
  MirWindowSpec *spec;

  if (state == impl->window_state)
    return;

  impl->window_state = state;

  if (impl->mir_window && !impl->pending_spec_update)
    {
      spec = mir_create_window_spec (connection);
      mir_window_spec_set_state (spec, state);
      mir_window_apply_spec (impl->mir_window, spec);
      mir_window_spec_release (spec);
    }
}

static void
event_cb (MirWindow      *mir_window,
          const MirEvent *event,
          void           *context)
{
  _gdk_mir_event_source_queue (context, event);
}

static MirWindowSpec *
create_window_type_spec (GdkDisplay *display,
                         GdkWindow *parent,
                         gint x,
                         gint y,
                         gint width,
                         gint height,
                         gboolean modal,
                         GdkWindowTypeHint type,
                         MirBufferUsage buffer_usage)
{
  MirConnection *connection = gdk_mir_display_get_mir_connection (display);
  MirWindow *parent_mir_window = NULL;
  MirPixelFormat format;
  MirRectangle rect;
  MirWindowSpec *spec;

  if (parent && parent->impl)
    {
      ensure_mir_window (parent);
      parent_mir_window = GDK_MIR_WINDOW_IMPL (parent->impl)->mir_window;
    }

  if (!parent_mir_window)
    {
      switch (type)
        {
          case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
          case GDK_WINDOW_TYPE_HINT_UTILITY:
            type = GDK_WINDOW_TYPE_HINT_DIALOG;
            break;
          default:
            break;
        }
    }

  format = _gdk_mir_display_get_pixel_format (display, buffer_usage);

  rect.left = x;
  rect.top = y;
  rect.width = 1;
  rect.height = 1;

  switch (type)
    {
      case GDK_WINDOW_TYPE_HINT_DIALOG:
        if (modal)
          spec = mir_create_modal_dialog_window_spec (connection,
                                                      width,
                                                      height,
                                                      parent_mir_window);
        else
          spec = mir_create_dialog_window_spec (connection,
                                                width,
                                                height);
        break;
      case GDK_WINDOW_TYPE_HINT_DOCK:
        spec = mir_create_dialog_window_spec (connection,
                                              width,
                                              height);
        break;
      case GDK_WINDOW_TYPE_HINT_MENU:
      case GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU:
      case GDK_WINDOW_TYPE_HINT_POPUP_MENU:
      case GDK_WINDOW_TYPE_HINT_TOOLBAR:
      case GDK_WINDOW_TYPE_HINT_COMBO:
      case GDK_WINDOW_TYPE_HINT_DND:
      case GDK_WINDOW_TYPE_HINT_TOOLTIP:
      case GDK_WINDOW_TYPE_HINT_NOTIFICATION:
        spec = mir_create_menu_window_spec (connection,
                                            width,
                                            height,
                                            parent_mir_window,
                                            &rect,
                                            0);
        break;
      case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
      case GDK_WINDOW_TYPE_HINT_UTILITY:
        spec = mir_create_modal_dialog_window_spec (connection,
                                                    width,
                                                    height,
                                                    parent_mir_window);
        break;
      case GDK_WINDOW_TYPE_HINT_NORMAL:
      case GDK_WINDOW_TYPE_HINT_DESKTOP:
      default:
        spec = mir_create_normal_window_spec (connection,
                                              width,
                                              height);
        break;
    }

  mir_window_spec_set_pixel_format (spec, format);

  return spec;
}

static void
apply_geometry_hints (MirWindowSpec    *spec,
                      GdkMirWindowImpl *impl)
{
  if (impl->geometry_mask & GDK_HINT_RESIZE_INC)
    {
      mir_window_spec_set_width_increment (spec, impl->geometry_hints.width_inc);
      mir_window_spec_set_height_increment (spec, impl->geometry_hints.height_inc);
    }
  if (impl->geometry_mask & GDK_HINT_MIN_SIZE)
    {
      mir_window_spec_set_min_width (spec, impl->geometry_hints.min_width);
      mir_window_spec_set_min_height (spec, impl->geometry_hints.min_height);
    }
  if (impl->geometry_mask & GDK_HINT_MAX_SIZE)
    {
      mir_window_spec_set_max_width (spec, impl->geometry_hints.max_width);
      mir_window_spec_set_max_height (spec, impl->geometry_hints.max_height);
    }
  if (impl->geometry_mask & GDK_HINT_ASPECT)
    {
      mir_window_spec_set_min_aspect_ratio (spec, (guint) 1000 * impl->geometry_hints.min_aspect, 1000);
      mir_window_spec_set_max_aspect_ratio (spec, (guint) 1000 * impl->geometry_hints.max_aspect, 1000);
    }
}

static MirWindowSpec *
create_spec (GdkWindow        *window,
             GdkMirWindowImpl *impl)
{
  MirWindowSpec *spec = NULL;
  GdkWindow *parent;
  MirRectangle rect;

  spec = create_window_type_spec (impl->display,
                                  impl->transient_for,
                                  impl->transient_x,
                                  impl->transient_y,
                                  window->width,
                                  window->height,
                                  impl->modal,
                                  impl->type_hint,
                                  impl->buffer_usage);

  mir_window_spec_set_name (spec, impl->title);
  mir_window_spec_set_buffer_usage (spec, impl->buffer_usage);

  apply_geometry_hints (spec, impl);

  if (impl->has_rect)
    {
      impl->mir_rect.left = impl->rect.x;
      impl->mir_rect.top = impl->rect.y;
      impl->mir_rect.width = impl->rect.width;
      impl->mir_rect.height = impl->rect.height;

      parent = impl->transient_for;

      while (parent && !gdk_window_has_native (parent))
        {
          impl->mir_rect.left += parent->x;
          impl->mir_rect.top += parent->y;

          parent = gdk_window_get_parent (parent);
        }

      mir_window_spec_set_placement (spec,
                                     &impl->mir_rect,
                                     impl->rect_anchor,
                                     impl->window_anchor,
                                     impl->anchor_hints,
                                     impl->rect_anchor_dx,
                                     impl->rect_anchor_dy);
    }
  else
    {
      switch (impl->type_hint)
        {
        case GDK_WINDOW_TYPE_HINT_MENU:
        case GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU:
        case GDK_WINDOW_TYPE_HINT_POPUP_MENU:
        case GDK_WINDOW_TYPE_HINT_TOOLBAR:
        case GDK_WINDOW_TYPE_HINT_COMBO:
        case GDK_WINDOW_TYPE_HINT_DND:
        case GDK_WINDOW_TYPE_HINT_TOOLTIP:
        case GDK_WINDOW_TYPE_HINT_NOTIFICATION:
          rect.left = impl->transient_x;
          rect.top = impl->transient_y;
          rect.width = 1;
          rect.height = 1;

          mir_window_spec_set_placement (spec,
                                         &rect,
                                         mir_placement_gravity_southeast,
                                         mir_placement_gravity_northwest,
                                         (mir_placement_hints_flip_x |
                                          mir_placement_hints_flip_y |
                                          mir_placement_hints_slide_x |
                                          mir_placement_hints_slide_y |
                                          mir_placement_hints_resize_x |
                                          mir_placement_hints_resize_y),
                                         -window->shadow_left,
                                         -window->shadow_top);

          break;
        default:
          break;
        }
    }

  return spec;
}

static void
update_window_spec (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  MirWindowSpec *spec;

  if (!impl->mir_window)
    return;

  spec = create_spec (window, impl);

  mir_window_apply_spec (impl->mir_window, spec);
  mir_window_spec_release (spec);

  impl->pending_spec_update = FALSE;
  impl->buffer_stream = mir_window_get_buffer_stream (impl->mir_window);
}

static GdkDevice *
get_pointer (GdkWindow *window)
{
  GdkDisplay *display;
  GdkSeat *seat;
  GdkDevice *pointer;

  display = gdk_window_get_display (window);
  seat = gdk_display_get_default_seat (display);
  pointer = gdk_seat_get_pointer (seat);

  return pointer;
}

static void
send_event (GdkWindow *window, GdkDevice *device, GdkEvent *event)
{
  GdkDisplay *display;
  GList *node;

  display = gdk_window_get_display (window);
  gdk_event_set_device (event, device);
  gdk_event_set_source_device (event, device);
  gdk_event_set_screen (event, gdk_display_get_default_screen (display));
  event->any.window = g_object_ref (window);

  node = _gdk_event_queue_append (display, event);
  _gdk_windowing_got_event (display, node, event, _gdk_display_get_next_serial (display));
}

static void
generate_configure_event (GdkWindow *window,
                          gint       width,
                          gint       height)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_CONFIGURE);
  event->configure.send_event = FALSE;
  event->configure.width = width;
  event->configure.height = height;

  send_event (window, get_pointer (window), event);
}

static void
synthesize_resize (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  MirWindowParameters params;

  if (!impl->mir_window)
    return;

  mir_window_get_parameters (impl->mir_window, &params);

  window->width = params.width;
  window->height = params.height;

  _gdk_window_update_size (window);

  generate_configure_event (window, window->width, window->height);
}

static void
maybe_synthesize_resize (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  MirWindowParameters params;

  if (!impl->mir_window)
    return;

  mir_window_get_parameters (impl->mir_window, &params);

  if (params.width != window->width || params.height != window->height)
    {
      window->width = params.width;
      window->height = params.height;

      _gdk_window_update_size (window);

      generate_configure_event (window, window->width, window->height);
    }
}

static void
ensure_mir_window_full (GdkWindow      *window,
                        MirBufferUsage  buffer_usage)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  GdkMirWindowReference *window_ref;
  MirWindowSpec *spec;

  if (window->input_only)
    return;

  if (impl->mir_window)
    {
      if (impl->pending_spec_update)
        update_window_spec (window);
      return;
    }

  /* no destroy notify -- we must leak for now
   * https://bugs.launchpad.net/mir/+bug/1324100
   */
  window_ref = _gdk_mir_event_source_get_window_reference (window);
  impl->buffer_usage = buffer_usage;

  spec = create_spec (window, impl);

  impl->mir_window = mir_create_window_sync (spec);

  mir_window_spec_release (spec);

  impl->pending_spec_update = FALSE;
  impl->buffer_stream = mir_window_get_buffer_stream (impl->mir_window);

  synthesize_resize (window);

  /* FIXME: Ignore some events until shown */
  mir_window_set_event_handler (impl->mir_window, event_cb, window_ref);
}

static void
ensure_mir_window (GdkWindow *window)
{
  ensure_mir_window_full (window,
                          window->gl_paint_context ?
                          mir_buffer_usage_hardware :
                          mir_buffer_usage_software);
}

static void
ensure_no_mir_window (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  if (impl->cairo_surface)
    {
      cairo_surface_finish (impl->cairo_surface);
      g_clear_pointer (&impl->cairo_surface, cairo_surface_destroy);
    }

  if (window->gl_paint_context)
    {
      GdkDisplay *display = gdk_window_get_display (window);
      EGLDisplay egl_display = _gdk_mir_display_get_egl_display (display);

      if (impl->egl_surface)
        {
          eglDestroySurface (egl_display, impl->egl_surface);
          impl->egl_surface = NULL;
        }

      if (impl->dummy_egl_surface)
        {
          eglDestroySurface (egl_display, impl->dummy_egl_surface);
          impl->dummy_egl_surface = NULL;
        }
    }

  g_clear_pointer (&impl->mir_window, mir_window_release_sync);
}

static void
send_buffer (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  /* Send the completed buffer to Mir */
  if (impl->mir_window)
    mir_buffer_stream_swap_buffers_sync (mir_window_get_buffer_stream (impl->mir_window));

  /* The Cairo context is no longer valid */
  g_clear_pointer (&impl->cairo_surface, cairo_surface_destroy);
  if (impl->pending_spec_update)
    update_window_spec (window);

  impl->pending_spec_update = FALSE;

  maybe_synthesize_resize (window);
}

static cairo_surface_t *
gdk_mir_window_impl_ref_cairo_surface (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  MirGraphicsRegion region;
  cairo_format_t pixel_format = CAIRO_FORMAT_ARGB32;
  cairo_surface_t *cairo_surface;

  if (impl->cairo_surface)
    {
      cairo_surface_reference (impl->cairo_surface);
      return impl->cairo_surface;
    }

  ensure_mir_window (window);

  if (!impl->mir_window)
    return NULL;

  if (window->gl_paint_context)
    {
      cairo_surface = cairo_image_surface_create (pixel_format, window->width, window->height);
      cairo_surface_set_device_scale (cairo_surface, (double) impl->output_scale, (double) impl->output_scale);
    }
  else if (impl->visible)
    {
      mir_buffer_stream_get_graphics_region (mir_window_get_buffer_stream (impl->mir_window), &region);

      switch (region.pixel_format)
        {
        case mir_pixel_format_abgr_8888:
          g_warning ("pixel format ABGR 8888 not supported, using ARGB 8888");
          pixel_format = CAIRO_FORMAT_ARGB32;
          break;
        case mir_pixel_format_xbgr_8888:
          g_warning ("pixel format XBGR 8888 not supported, using XRGB 8888");
          pixel_format = CAIRO_FORMAT_RGB24;
          break;
        case mir_pixel_format_argb_8888:
          pixel_format = CAIRO_FORMAT_ARGB32;
          break;
        case mir_pixel_format_xrgb_8888:
          pixel_format = CAIRO_FORMAT_RGB24;
          break;
        case mir_pixel_format_bgr_888:
          g_error ("pixel format BGR 888 not supported");
          break;
        case mir_pixel_format_rgb_888:
          g_error ("pixel format RGB 888 not supported");
          break;
        case mir_pixel_format_rgb_565:
          pixel_format = CAIRO_FORMAT_RGB16_565;
          break;
        case mir_pixel_format_rgba_5551:
          g_error ("pixel format RGBA 5551 not supported");
          break;
        case mir_pixel_format_rgba_4444:
          g_error ("pixel format RGBA 4444 not supported");
          break;
        default:
          g_error ("unknown pixel format");
          break;
        }

      cairo_surface = cairo_image_surface_create_for_data ((unsigned char *) region.vaddr,
                                                           pixel_format,
                                                           region.width,
                                                           region.height,
                                                           region.stride);
      cairo_surface_set_device_scale (cairo_surface, (double) impl->output_scale, (double) impl->output_scale);
    }
  else
    cairo_surface = cairo_image_surface_create (pixel_format, 0, 0);

  impl->cairo_surface = cairo_surface_reference (cairo_surface);

  return cairo_surface;
}

static cairo_surface_t *
gdk_mir_window_impl_create_similar_image_surface (GdkWindow      *window,
                                                  cairo_format_t  format,
                                                  int             width,
                                                  int             height)
{
  return cairo_image_surface_create (format, width, height);
}

static void
gdk_mir_window_impl_finalize (GObject *object)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (object);

  g_free (impl->title);

  g_clear_pointer (&impl->mir_window, mir_window_release_sync);
  g_clear_pointer (&impl->cairo_surface, cairo_surface_destroy);
  g_clear_pointer (&impl->properties, g_hash_table_unref);

  G_OBJECT_CLASS (gdk_mir_window_impl_parent_class)->finalize (object);
}

static void
gdk_mir_window_impl_show (GdkWindow *window,
                          gboolean   already_mapped)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  cairo_surface_t *s;

  impl->visible = TRUE;
  set_window_state (impl, mir_window_state_restored);

  /* Make sure there's a window to see */
  ensure_mir_window (window);

  if (!window->gl_paint_context)
    {
      /* Make sure something is rendered and then show first frame */
      s = gdk_mir_window_impl_ref_cairo_surface (window);
      send_buffer (window);
      cairo_surface_destroy (s);
    }
}

static void
gdk_mir_window_impl_hide (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  impl->cursor_inside = FALSE;
  impl->visible = FALSE;

  set_window_state (impl, mir_window_state_hidden);
}

static void
gdk_mir_window_impl_withdraw (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  impl->cursor_inside = FALSE;
  impl->visible = FALSE;

  set_window_state (impl, mir_window_state_hidden);
}

static void
gdk_mir_window_impl_raise (GdkWindow *window)
{
  /* We don't support client window stacking */
}

static void
gdk_mir_window_impl_lower (GdkWindow *window)
{
  /* We don't support client window stacking */
}

static void
gdk_mir_window_impl_restack_toplevel (GdkWindow *window,
                                      GdkWindow *sibling,
                                      gboolean   above)
{
  /* We don't support client window stacking */
}

static void
gdk_mir_window_impl_move_resize (GdkWindow *window,
                                 gboolean   with_move,
                                 gint       x,
                                 gint       y,
                                 gint       width,
                                 gint       height)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  /* If resize requested then rebuild window */
  if (width >= 0 && (window->width != width || window->height != height))
  {
    /* We accept any resize */
    window->width = width;
    window->height = height;
    impl->pending_spec_update = TRUE;
  }

  /* Transient windows can move wherever they want */
  if (with_move)
    {
      if (impl->has_rect || x != impl->transient_x || y != impl->transient_y)
        {
          impl->has_rect = FALSE;
          impl->transient_x = x;
          impl->transient_y = y;
          if (!impl->pending_spec_update && impl->mir_window)
            update_window_spec (window);
        }
    }
}

static MirPlacementGravity
get_mir_placement_gravity (GdkGravity gravity)
{
  switch (gravity)
    {
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
      return mir_placement_gravity_northwest;
    case GDK_GRAVITY_NORTH:
      return mir_placement_gravity_north;
    case GDK_GRAVITY_NORTH_EAST:
      return mir_placement_gravity_northeast;
    case GDK_GRAVITY_WEST:
      return mir_placement_gravity_west;
    case GDK_GRAVITY_CENTER:
      return mir_placement_gravity_center;
    case GDK_GRAVITY_EAST:
      return mir_placement_gravity_east;
    case GDK_GRAVITY_SOUTH_WEST:
      return mir_placement_gravity_southwest;
    case GDK_GRAVITY_SOUTH:
      return mir_placement_gravity_south;
    case GDK_GRAVITY_SOUTH_EAST:
      return mir_placement_gravity_southeast;
    }

  g_warn_if_reached ();

  return mir_placement_gravity_center;
}

static MirPlacementHints
get_mir_placement_hints (GdkAnchorHints hints)
{
  MirPlacementHints mir_hints = 0;

  if (hints & GDK_ANCHOR_FLIP_X)
    mir_hints |= mir_placement_hints_flip_x;

  if (hints & GDK_ANCHOR_FLIP_Y)
    mir_hints |= mir_placement_hints_flip_y;

  if (hints & GDK_ANCHOR_SLIDE_X)
    mir_hints |= mir_placement_hints_slide_x;

  if (hints & GDK_ANCHOR_SLIDE_Y)
    mir_hints |= mir_placement_hints_slide_y;

  if (hints & GDK_ANCHOR_RESIZE_X)
    mir_hints |= mir_placement_hints_resize_x;

  if (hints & GDK_ANCHOR_RESIZE_Y)
    mir_hints |= mir_placement_hints_resize_y;

  return mir_hints;
}

static gint
get_window_shadow_dx (GdkWindow  *window,
                      GdkGravity  window_anchor)
{
  switch (window_anchor)
    {
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_WEST:
    case GDK_GRAVITY_SOUTH_WEST:
      return -window->shadow_left;

    case GDK_GRAVITY_NORTH:
    case GDK_GRAVITY_CENTER:
    case GDK_GRAVITY_SOUTH:
      return (window->shadow_right - window->shadow_left) / 2;

    case GDK_GRAVITY_NORTH_EAST:
    case GDK_GRAVITY_EAST:
    case GDK_GRAVITY_SOUTH_EAST:
      return window->shadow_right;
    }

  g_warn_if_reached ();

  return 0;
}

static gint
get_window_shadow_dy (GdkWindow  *window,
                      GdkGravity  window_anchor)
{
  switch (window_anchor)
    {
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_NORTH:
    case GDK_GRAVITY_NORTH_EAST:
      return -window->shadow_top;

    case GDK_GRAVITY_WEST:
    case GDK_GRAVITY_CENTER:
    case GDK_GRAVITY_EAST:
      return (window->shadow_bottom - window->shadow_top) / 2;

    case GDK_GRAVITY_SOUTH_WEST:
    case GDK_GRAVITY_SOUTH:
    case GDK_GRAVITY_SOUTH_EAST:
      return window->shadow_bottom;
    }

  g_warn_if_reached ();

  return 0;
}

static void
gdk_mir_window_impl_move_to_rect (GdkWindow          *window,
                                  const GdkRectangle *rect,
                                  GdkGravity          rect_anchor,
                                  GdkGravity          window_anchor,
                                  GdkAnchorHints      anchor_hints,
                                  gint                rect_anchor_dx,
                                  gint                rect_anchor_dy)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  impl->has_rect = TRUE;
  impl->rect = *rect;
  impl->rect_anchor = get_mir_placement_gravity (rect_anchor);
  impl->window_anchor = get_mir_placement_gravity (window_anchor);
  impl->anchor_hints = get_mir_placement_hints (anchor_hints);
  impl->rect_anchor_dx = rect_anchor_dx + get_window_shadow_dx (window, window_anchor);
  impl->rect_anchor_dy = rect_anchor_dy + get_window_shadow_dy (window, window_anchor);

  if (impl->mir_window && !impl->pending_spec_update)
    update_window_spec (window);
}

static gint
get_mir_placement_gravity_x (MirPlacementGravity gravity)
{
  switch (gravity)
    {
    case mir_placement_gravity_west:
    case mir_placement_gravity_northwest:
    case mir_placement_gravity_southwest:
      return 0;

    case mir_placement_gravity_center:
    case mir_placement_gravity_north:
    case mir_placement_gravity_south:
      return 1;

    case mir_placement_gravity_east:
    case mir_placement_gravity_northeast:
    case mir_placement_gravity_southeast:
      return 2;
    }

  g_warn_if_reached ();

  return 1;
}

static gint
get_mir_placement_gravity_y (MirPlacementGravity gravity)
{
  switch (gravity)
    {
    case mir_placement_gravity_north:
    case mir_placement_gravity_northwest:
    case mir_placement_gravity_northeast:
      return 0;

    case mir_placement_gravity_center:
    case mir_placement_gravity_west:
    case mir_placement_gravity_east:
      return 1;

    case mir_placement_gravity_south:
    case mir_placement_gravity_southwest:
    case mir_placement_gravity_southeast:
      return 2;
    }

  g_warn_if_reached ();

  return 1;
}

static GdkRectangle
get_unflipped_rect (const GdkRectangle  *rect,
                    gint                 width,
                    gint                 height,
                    MirPlacementGravity  rect_anchor,
                    MirPlacementGravity  window_anchor,
                    gint                 rect_anchor_dx,
                    gint                 rect_anchor_dy)
{
  GdkRectangle unflipped_rect;

  unflipped_rect.x = rect->x;
  unflipped_rect.x += rect->width * get_mir_placement_gravity_x (rect_anchor) / 2;
  unflipped_rect.x -= width * get_mir_placement_gravity_x (window_anchor) / 2;
  unflipped_rect.x += rect_anchor_dx;
  unflipped_rect.y = rect->y;
  unflipped_rect.y += rect->height * get_mir_placement_gravity_y (rect_anchor) / 2;
  unflipped_rect.y -= height * get_mir_placement_gravity_y (window_anchor) / 2;
  unflipped_rect.y += rect_anchor_dy;
  unflipped_rect.width = width;
  unflipped_rect.height = height;

  return unflipped_rect;
}

static MirPlacementGravity
get_opposite_mir_placement_gravity (MirPlacementGravity gravity)
{
  switch (gravity)
    {
    case mir_placement_gravity_center:
      return mir_placement_gravity_center;
    case mir_placement_gravity_west:
      return mir_placement_gravity_east;
    case mir_placement_gravity_east:
      return mir_placement_gravity_west;
    case mir_placement_gravity_north:
      return mir_placement_gravity_south;
    case mir_placement_gravity_south:
      return mir_placement_gravity_north;
    case mir_placement_gravity_northwest:
      return mir_placement_gravity_southeast;
    case mir_placement_gravity_northeast:
      return mir_placement_gravity_southwest;
    case mir_placement_gravity_southwest:
      return mir_placement_gravity_northeast;
    case mir_placement_gravity_southeast:
      return mir_placement_gravity_northwest;
    }

  g_warn_if_reached ();

  return gravity;
}

static gint
get_anchor_x (const GdkRectangle  *rect,
              MirPlacementGravity  anchor)
{
  return rect->x + rect->width * get_mir_placement_gravity_x (anchor) / 2;
}

static gint
get_anchor_y (const GdkRectangle  *rect,
              MirPlacementGravity  anchor)
{
  return rect->y + rect->height * get_mir_placement_gravity_y (anchor) / 2;
}

void
_gdk_mir_window_set_final_rect (GdkWindow    *window,
                                MirRectangle  rect)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  GdkRectangle best_rect;
  GdkRectangle worst_rect;
  GdkRectangle flipped_rect;
  GdkRectangle final_rect;
  gboolean flipped_x = FALSE;
  gboolean flipped_y = FALSE;
  gint test_position;
  gint final_position;
  gint unflipped_offset;
  gint flipped_offset;

  if (!impl->has_rect)
    return;

  best_rect = get_unflipped_rect (&impl->rect,
                                  window->width,
                                  window->height,
                                  impl->rect_anchor,
                                  impl->window_anchor,
                                  impl->rect_anchor_dx,
                                  impl->rect_anchor_dy);

  worst_rect = get_unflipped_rect (&impl->rect,
                                   window->width,
                                   window->height,
                                   get_opposite_mir_placement_gravity (impl->rect_anchor),
                                   get_opposite_mir_placement_gravity (impl->window_anchor),
                                   -impl->rect_anchor_dx,
                                   -impl->rect_anchor_dy);

  flipped_rect.x = best_rect.x;
  flipped_rect.y = best_rect.y;
  flipped_rect.width = window->width;
  flipped_rect.height = window->height;

  final_rect.x = rect.left - (impl->mir_rect.left - impl->rect.x);
  final_rect.y = rect.top - (impl->mir_rect.top - impl->rect.y);
  final_rect.width = rect.width;
  final_rect.height = rect.height;

  if (impl->anchor_hints & mir_placement_hints_flip_x)
    {
      test_position = get_anchor_x (&best_rect, impl->window_anchor);
      final_position = get_anchor_x (&final_rect, impl->window_anchor);
      unflipped_offset = final_position - test_position;

      test_position = get_anchor_x (&worst_rect, get_opposite_mir_placement_gravity (impl->window_anchor));
      final_position = get_anchor_x (&final_rect, get_opposite_mir_placement_gravity (impl->window_anchor));
      flipped_offset = final_position - test_position;

      if (ABS (flipped_offset) < ABS (unflipped_offset))
        {
          flipped_rect.x = worst_rect.x;
          flipped_x = TRUE;
        }
    }

  if (impl->anchor_hints & mir_placement_hints_flip_y)
    {
      test_position = get_anchor_y (&best_rect, impl->window_anchor);
      final_position = get_anchor_y (&final_rect, impl->window_anchor);
      unflipped_offset = final_position - test_position;

      test_position = get_anchor_y (&worst_rect, get_opposite_mir_placement_gravity (impl->window_anchor));
      final_position = get_anchor_y (&final_rect, get_opposite_mir_placement_gravity (impl->window_anchor));
      flipped_offset = final_position - test_position;

      if (ABS (flipped_offset) < ABS (unflipped_offset))
        {
          flipped_rect.y = worst_rect.y;
          flipped_y = TRUE;
        }
    }

  g_signal_emit_by_name (window,
                         "moved-to-rect",
                         &flipped_rect,
                         &final_rect,
                         flipped_x,
                         flipped_y);
}

static GdkEventMask
gdk_mir_window_impl_get_events (GdkWindow *window)
{
  return window->event_mask;
}

static void
gdk_mir_window_impl_set_events (GdkWindow    *window,
                                GdkEventMask  event_mask)
{
  /* We send all events and let GDK decide */
}

static void
gdk_mir_window_impl_set_device_cursor (GdkWindow *window,
                                       GdkDevice *device,
                                       GdkCursor *cursor)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  MirConnection *connection = gdk_mir_display_get_mir_connection (impl->display);
  MirWindowSpec *spec;
  const gchar *cursor_name;

  if (cursor)
    cursor_name = _gdk_mir_cursor_get_name (cursor);
  else
    cursor_name = mir_default_cursor_name;

  spec = mir_create_window_spec (connection);
  mir_window_spec_set_cursor_name (spec, cursor_name);
  mir_window_apply_spec (impl->mir_window, spec);
  mir_window_spec_release (spec);
}

static void
gdk_mir_window_impl_get_geometry (GdkWindow *window,
                                  gint      *x,
                                  gint      *y,
                                  gint      *width,
                                  gint      *height)
{
  if (x)
    *x = 0; // FIXME
  if (y)
    *y = 0; // FIXME
  if (width)
    *width = window->width;
  if (height)
    *height = window->height;
}

static void
gdk_mir_window_impl_get_root_coords (GdkWindow *window,
                                     gint       x,
                                     gint       y,
                                     gint      *root_x,
                                     gint      *root_y)
{
  if (root_x)
    *root_x = x; // FIXME
  if (root_y)
    *root_y = y; // FIXME
}

static gboolean
gdk_mir_window_impl_get_device_state (GdkWindow       *window,
                                      GdkDevice       *device,
                                      gdouble         *x,
                                      gdouble         *y,
                                      GdkModifierType *mask)
{
  GdkWindow *child;

  _gdk_device_query_state (device, window, NULL, &child, NULL, NULL, x, y, mask);

  return child != NULL;
}

static gboolean
gdk_mir_window_impl_begin_paint (GdkWindow *window)
{
  /* Indicate we are ready to be drawn onto directly? */
  return FALSE;
}

static void
gdk_mir_window_impl_end_paint (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  if (impl->visible)
    send_buffer (window);
}

static void
gdk_mir_window_impl_shape_combine_region (GdkWindow            *window,
                                          const cairo_region_t *shape_region,
                                          gint                  offset_x,
                                          gint                  offset_y)
{
}

static void
gdk_mir_window_impl_input_shape_combine_region (GdkWindow            *window,
                                                const cairo_region_t *shape_region,
                                                gint                  offset_x,
                                                gint                  offset_y)
{
}

static void
gdk_mir_window_impl_destroy (GdkWindow *window,
                             gboolean   recursing,
                             gboolean   foreign_destroy)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  impl->visible = FALSE;
  ensure_no_mir_window (window);
}

static void
gdk_mir_window_impl_focus (GdkWindow *window,
                           guint32    timestamp)
{
}

static void
gdk_mir_window_impl_set_type_hint (GdkWindow         *window,
                                   GdkWindowTypeHint  hint)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  if (type_hint_differs (hint, impl->type_hint))
    {
      impl->type_hint = hint;

      if (impl->mir_window && !impl->pending_spec_update)
        update_window_spec (window);
    }
}

static GdkWindowTypeHint
gdk_mir_window_impl_get_type_hint (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  return impl->type_hint;
}

void
gdk_mir_window_impl_set_modal_hint (GdkWindow *window,
                                    gboolean   modal)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  if (modal != impl->modal)
    {
      impl->modal = modal;

      if (impl->mir_window && !impl->pending_spec_update)
        update_window_spec (window);
    }
}

static void
gdk_mir_window_impl_set_skip_taskbar_hint (GdkWindow *window,
                                           gboolean   skips_taskbar)
{
}

static void
gdk_mir_window_impl_set_skip_pager_hint (GdkWindow *window,
                                         gboolean   skips_pager)
{
}

static void
gdk_mir_window_impl_set_urgency_hint (GdkWindow *window,
                                      gboolean   urgent)
{
}

static void
gdk_mir_window_impl_set_geometry_hints (GdkWindow         *window,
                                        const GdkGeometry *geometry,
                                        GdkWindowHints     geom_mask)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  MirConnection *connection = gdk_mir_display_get_mir_connection (impl->display);
  MirWindowSpec *spec;

  impl->geometry_hints = *geometry;
  impl->geometry_mask = geom_mask;

  if (impl->mir_window && !impl->pending_spec_update)
    {
       spec = mir_create_window_spec (connection);
       apply_geometry_hints (spec, impl);
       mir_window_apply_spec (impl->mir_window, spec);
       mir_window_spec_release (spec);
    }
}

static void
gdk_mir_window_impl_set_title (GdkWindow   *window,
                               const gchar *title)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  MirConnection *connection = gdk_mir_display_get_mir_connection (impl->display);
  MirWindowSpec *spec;

  g_free (impl->title);
  impl->title = g_strdup (title);

  if (impl->mir_window && !impl->pending_spec_update)
    {
       spec = mir_create_window_spec (connection);
       mir_window_spec_set_name (spec, impl->title);
       mir_window_apply_spec (impl->mir_window, spec);
       mir_window_spec_release (spec);
    }
}

static void
gdk_mir_window_impl_set_role (GdkWindow   *window,
                              const gchar *role)
{
}

static void
gdk_mir_window_impl_set_startup_id (GdkWindow   *window,
                                    const gchar *startup_id)
{
}

static void
gdk_mir_window_impl_set_transient_for (GdkWindow *window,
                                       GdkWindow *parent)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  if (impl->transient_for == parent)
    return;

  /* Link this window to the parent */
  impl->transient_for = parent;

  if (impl->mir_window && !impl->pending_spec_update)
    update_window_spec (window);
}

static void
gdk_mir_window_impl_get_frame_extents (GdkWindow    *window,
                                       GdkRectangle *rect)
{
}

static void
gdk_mir_window_impl_set_accept_focus (GdkWindow *window,
                                      gboolean   accept_focus)
{
  /* Mir clients cannot control focus */
}

static void
gdk_mir_window_impl_set_focus_on_map (GdkWindow *window,
                                      gboolean focus_on_map)
{
  /* Mir clients cannot control focus */
}

static void
gdk_mir_window_impl_set_icon_list (GdkWindow *window,
                                   GList     *pixbufs)
{
  // ??
}

static void
gdk_mir_window_impl_set_icon_name (GdkWindow   *window,
                                   const gchar *name)
{
}

static void
gdk_mir_window_impl_iconify (GdkWindow *window)
{
  /* We don't support iconification */
}

static void
gdk_mir_window_impl_deiconify (GdkWindow *window)
{
  /* We don't support iconification */
}

static void
gdk_mir_window_impl_stick (GdkWindow *window)
{
  /* We do not support stick/unstick in Mir */
}

static void
gdk_mir_window_impl_unstick (GdkWindow *window)
{
  /* We do not support stick/unstick in Mir */
}

static void
gdk_mir_window_impl_maximize (GdkWindow *window)
{
  set_window_state (GDK_MIR_WINDOW_IMPL (window->impl), mir_window_state_maximized);
}

static void
gdk_mir_window_impl_unmaximize (GdkWindow *window)
{
  set_window_state (GDK_MIR_WINDOW_IMPL (window->impl), mir_window_state_restored);
}

static void
gdk_mir_window_impl_fullscreen (GdkWindow *window)
{
  set_window_state (GDK_MIR_WINDOW_IMPL (window->impl), mir_window_state_fullscreen);
}

static void
gdk_mir_window_impl_apply_fullscreen_mode (GdkWindow *window)
{
}

static void
gdk_mir_window_impl_unfullscreen (GdkWindow *window)
{
  set_window_state (GDK_MIR_WINDOW_IMPL (window->impl), mir_window_state_restored);
}

static void
gdk_mir_window_impl_set_keep_above (GdkWindow *window,
                                    gboolean   setting)
{
  /* We do not support keep above/below in Mir */
}

static void
gdk_mir_window_impl_set_keep_below (GdkWindow *window,
                                    gboolean setting)
{
  /* We do not support keep above/below in Mir */
}

static GdkWindow *
gdk_mir_window_impl_get_group (GdkWindow *window)
{
  return NULL;
}

static void
gdk_mir_window_impl_set_group (GdkWindow *window,
                               GdkWindow *leader)
{
}

static void
gdk_mir_window_impl_set_decorations (GdkWindow       *window,
                                     GdkWMDecoration  decorations)
{
}

static gboolean
gdk_mir_window_impl_get_decorations (GdkWindow       *window,
                                     GdkWMDecoration *decorations)
{
  return FALSE;
}

static void
gdk_mir_window_impl_set_functions (GdkWindow     *window,
                                   GdkWMFunction  functions)
{
}

static void
gdk_mir_window_impl_begin_resize_drag (GdkWindow     *window,
                                       GdkWindowEdge  edge,
                                       GdkDevice     *device,
                                       gint           button,
                                       gint           root_x,
                                       gint           root_y,
                                       guint32        timestamp)
{
}

static void
gdk_mir_window_impl_begin_move_drag (GdkWindow *window,
                                     GdkDevice *device,
                                     gint       button,
                                     gint       root_x,
                                     gint       root_y,
                                     guint32    timestamp)
{
}

static void
gdk_mir_window_impl_enable_synchronized_configure (GdkWindow *window)
{
}

static void
gdk_mir_window_impl_configure_finished (GdkWindow *window)
{
}

static void
gdk_mir_window_impl_set_opacity (GdkWindow *window,
                                 gdouble    opacity)
{
  // FIXME
}

static void
gdk_mir_window_impl_destroy_notify (GdkWindow *window)
{
}

static GdkDragProtocol
gdk_mir_window_impl_get_drag_protocol (GdkWindow *window,
                                       GdkWindow **target)
{
  return 0;
}

static void
gdk_mir_window_impl_register_dnd (GdkWindow *window)
{
}

static GdkDragContext *
gdk_mir_window_impl_drag_begin (GdkWindow *window,
                                GdkDevice *device,
                                GList     *targets,
                                gint       x_root,
                                gint       y_root)
{
  return NULL;
}

static void
gdk_mir_window_impl_process_updates_recurse (GdkWindow      *window,
                                             cairo_region_t *region)
{
  cairo_rectangle_int_t rectangle;

  /* We redraw the whole region, but we should track the buffers and only redraw what has changed since we sent this buffer */
  rectangle.x = 0;
  rectangle.y = 0;
  rectangle.width = window->width;
  rectangle.height = window->height;
  cairo_region_union_rectangle (region, &rectangle);

  _gdk_window_process_updates_recurse (window, region);
}

static gboolean
gdk_mir_window_impl_get_property (GdkWindow  *window,
                                  GdkAtom     property,
                                  GdkAtom     type,
                                  gulong      offset,
                                  gulong      length,
                                  gint        pdelete,
                                  GdkAtom    *actual_type,
                                  gint       *actual_format,
                                  gint       *actual_length,
                                  guchar    **data)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  GdkMirProperty *mir_property;
  GdkAtom dummy_actual_type;
  gint dummy_actual_format;
  gint dummy_actual_length;
  guint width;

  if (!actual_type)
    actual_type = &dummy_actual_type;
  if (!actual_format)
    actual_format = &dummy_actual_format;
  if (!actual_length)
    actual_length = &dummy_actual_length;

  *actual_type = GDK_NONE;
  *actual_format = 0;
  *actual_length = 0;

  if (data)
    *data = NULL;

  mir_property = g_hash_table_lookup (impl->properties, property);

  if (!mir_property)
    return FALSE;

  width = g_array_get_element_size (mir_property->array);
  *actual_type = mir_property->type;
  *actual_format = 8 * width;

  /* ICCCM 2.7: GdkAtoms can be 64-bit, but ATOMs and ATOM_PAIRs have format 32  */
  if (*actual_type == GDK_SELECTION_TYPE_ATOM || *actual_type == gdk_atom_intern_static_string ("ATOM_PAIR"))
    *actual_format = 32;

  if (type != GDK_NONE && type != mir_property->type)
    return FALSE;

  offset *= 4;

  /* round up to next nearest multiple of width */
  if (length < G_MAXULONG - width + 1)
    length = (length - 1 + width) / width * width;
  else
    length = G_MAXULONG / width * width;

  /* we're skipping the first offset bytes */
  if (length > mir_property->array->len * width - offset)
    length = mir_property->array->len * width - offset;

  /* leave room for null terminator */
  if (length > G_MAXULONG - width)
    length -= width;

  *actual_length = length;

  if (data)
    {
      *data = g_memdup (mir_property->array->data + offset, length + width);
      memset (*data + length, 0, width);
    }

  return TRUE;
}

static void
request_targets (GdkWindow     *window,
                 const GdkAtom *available_targets,
                 gint           n_available_targets)
{
  GArray *requested_targets;
  GdkAtom target_pair[2];
  gchar *target_location;
  GdkEvent *event;
  gint i;

  requested_targets = g_array_sized_new (TRUE, FALSE, sizeof (GdkAtom), 2 * n_available_targets);

  for (i = 0; i < n_available_targets; i++)
    {
      target_pair[0] = available_targets[i];

      if (target_pair[0] == gdk_atom_intern_static_string ("TIMESTAMP") ||
          target_pair[0] == gdk_atom_intern_static_string ("TARGETS") ||
          target_pair[0] == gdk_atom_intern_static_string ("MULTIPLE") ||
          target_pair[0] == gdk_atom_intern_static_string ("SAVE_TARGETS"))
        continue;

      target_location = g_strdup_printf ("REQUESTED_TARGET_U%u", requested_targets->len / 2);
      target_pair[1] = gdk_atom_intern (target_location, FALSE);
      g_free (target_location);

      g_array_append_vals (requested_targets, target_pair, 2);
    }

  gdk_property_delete (window, gdk_atom_intern_static_string ("AVAILABLE_TARGETS"));
  gdk_property_delete (window, gdk_atom_intern_static_string ("REQUESTED_TARGETS"));

  gdk_property_change (window,
                       gdk_atom_intern_static_string ("REQUESTED_TARGETS"),
                       GDK_SELECTION_TYPE_ATOM,
                       8 * sizeof (GdkAtom),
                       GDK_PROP_MODE_REPLACE,
                       (const guchar *) requested_targets->data,
                       requested_targets->len);

  g_array_unref (requested_targets);

  event = gdk_event_new (GDK_SELECTION_REQUEST);
  event->selection.window = g_object_ref (window);
  event->selection.send_event = FALSE;
  event->selection.selection = GDK_SELECTION_CLIPBOARD;
  event->selection.target = gdk_atom_intern_static_string ("MULTIPLE");
  event->selection.property = gdk_atom_intern_static_string ("REQUESTED_TARGETS");
  event->selection.time = GDK_CURRENT_TIME;
  event->selection.requestor = g_object_ref (window);

  gdk_event_put (event);
  gdk_event_free (event);
}

static void
create_paste (GdkWindow     *window,
              const GdkAtom *requested_targets,
              gint           n_requested_targets)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  GPtrArray *paste_formats;
  GArray *paste_header;
  GByteArray *paste_data;
  gint sizes[4];
  GdkMirProperty *mir_property;
  const gchar *paste_format;
  gint i;

  paste_formats = g_ptr_array_new_full (n_requested_targets, g_free);
  paste_header = g_array_sized_new (FALSE, FALSE, sizeof (gint), 1 + 4 * n_requested_targets);
  paste_data = g_byte_array_new ();

  g_array_append_val (paste_header, sizes[0]);

  for (i = 0; i < n_requested_targets; i++)
    {
      if (requested_targets[i] == GDK_NONE)
        continue;

      mir_property = g_hash_table_lookup (impl->properties, requested_targets[i]);

      if (!mir_property)
        continue;

      paste_format = _gdk_atom_name_const (mir_property->type);

      /* skip non-MIME targets */
      if (!strchr (paste_format, '/'))
        {
          g_hash_table_remove (impl->properties, requested_targets[i]);
          continue;
        }

      sizes[0] = paste_data->len;
      sizes[1] = strlen (paste_format);
      sizes[2] = sizes[0] + sizes[1];
      sizes[3] = mir_property->array->len * g_array_get_element_size (mir_property->array);

      g_ptr_array_add (paste_formats, g_strdup (paste_format));
      g_array_append_vals (paste_header, sizes, 4);
      g_byte_array_append (paste_data, (const guint8 *) paste_format, sizes[1]);
      g_byte_array_append (paste_data, (const guint8 *) mir_property->array->data, sizes[3]);

      g_hash_table_remove (impl->properties, requested_targets[i]);
    }

  gdk_property_delete (window, gdk_atom_intern_static_string ("REQUESTED_TARGETS"));

  g_array_index (paste_header, gint, 0) = paste_formats->len;

  for (i = 0; i < paste_formats->len; i++)
    {
      g_array_index (paste_header, gint, 1 + 4 * i) += paste_header->len * sizeof (gint);
      g_array_index (paste_header, gint, 3 + 4 * i) += paste_header->len * sizeof (gint);
    }

  g_byte_array_prepend (paste_data,
                        (const guint8 *) paste_header->data,
                        paste_header->len * g_array_get_element_size (paste_header));

  g_ptr_array_add (paste_formats, NULL);

  _gdk_mir_display_create_paste (gdk_window_get_display (window),
                                 (const gchar * const *) paste_formats->pdata,
                                 paste_data->data,
                                 paste_data->len);

  g_byte_array_unref (paste_data);
  g_array_unref (paste_header);
  g_ptr_array_unref (paste_formats);
}

static void
gdk_mir_window_impl_change_property (GdkWindow    *window,
                                     GdkAtom       property,
                                     GdkAtom       type,
                                     gint          format,
                                     GdkPropMode   mode,
                                     const guchar *data,
                                     gint          n_elements)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  GdkMirProperty *mir_property;
  gboolean existed;
  GdkEvent *event;

  /* ICCCM 2.7: ATOMs and ATOM_PAIRs have format 32, but GdkAtoms can be 64-bit */
  if (type == GDK_SELECTION_TYPE_ATOM || type == gdk_atom_intern_static_string ("ATOM_PAIR"))
    format = 8 * sizeof (GdkAtom);

  if (mode != GDK_PROP_MODE_REPLACE)
    {
      mir_property = g_hash_table_lookup (impl->properties, property);
      existed = mir_property != NULL;
    }
  else
    {
      mir_property = NULL;
      existed = g_hash_table_contains (impl->properties, property);
    }

  if (!mir_property)
    {
      /* format is measured in bits, but we need to know this in bytes */
      mir_property = gdk_mir_property_new (type, format / 8, n_elements);
      g_hash_table_insert (impl->properties, property, mir_property);
    }

  /* format is measured in bits, but we need to know this in bytes */
  if (type != mir_property->type || format / 8 != g_array_get_element_size (mir_property->array))
    return;

  if (mode == GDK_PROP_MODE_PREPEND)
    g_array_prepend_vals (mir_property->array, data, n_elements);
  else
    g_array_append_vals (mir_property->array, data, n_elements);

  event = gdk_event_new (GDK_PROPERTY_NOTIFY);
  event->property.window = g_object_ref (window);
  event->property.send_event = FALSE;
  event->property.atom = property;
  event->property.time = GDK_CURRENT_TIME;
  event->property.state = GDK_PROPERTY_NEW_VALUE;

  gdk_event_put (event);
  gdk_event_free (event);

  if (property == gdk_atom_intern_static_string ("AVAILABLE_TARGETS"))
    request_targets (window, (const GdkAtom *) data, n_elements);
  else if (property == gdk_atom_intern_static_string ("REQUESTED_TARGETS") && existed)
    create_paste (window, (const GdkAtom *) data, n_elements);
}

static void
gdk_mir_window_impl_delete_property (GdkWindow *window,
                                     GdkAtom    property)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  GdkEvent *event;

  if (g_hash_table_remove (impl->properties, property))
    {
      event = gdk_event_new (GDK_PROPERTY_NOTIFY);
      event->property.window = g_object_ref (window);
      event->property.send_event = FALSE;
      event->property.atom = property;
      event->property.time = GDK_CURRENT_TIME;
      event->property.state = GDK_PROPERTY_DELETE;

      gdk_event_put (event);
      gdk_event_free (event);
    }
}

static gint
gdk_mir_window_impl_get_scale_factor (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  return impl->output_scale;
}

static void
gdk_mir_window_impl_set_opaque_region (GdkWindow      *window,
                                       cairo_region_t *region)
{
  /* FIXME: An optimisation to tell the compositor which regions of the window are fully transparent */
}

static void
gdk_mir_window_impl_set_shadow_width (GdkWindow *window,
                                      gint       left,
                                      gint       right,
                                      gint       top,
                                      gint       bottom)
{
}

static gboolean
find_eglconfig_for_window (GdkWindow  *window,
                           EGLConfig  *egl_config_out,
                           GError    **error)
{
  GdkDisplay *display = gdk_window_get_display (window);
  EGLDisplay *egl_display = _gdk_mir_display_get_egl_display (display);
  EGLint attrs[MAX_EGL_ATTRS];
  EGLint count;
  EGLConfig *configs;

  int i = 0;

  attrs[i++] = EGL_SURFACE_TYPE;
  attrs[i++] = EGL_WINDOW_BIT;

  attrs[i++] = EGL_COLOR_BUFFER_TYPE;
  attrs[i++] = EGL_RGB_BUFFER;

  attrs[i++] = EGL_RED_SIZE;
  attrs[i++] = 1;
  attrs[i++] = EGL_GREEN_SIZE;
  attrs[i++] = 1;
  attrs[i++] = EGL_BLUE_SIZE;
  attrs[i++] = 1;
  attrs[i++] = EGL_ALPHA_SIZE;
  attrs[i++] = 1;

  attrs[i++] = EGL_NONE;
  g_assert (i < MAX_EGL_ATTRS);

  if (!eglChooseConfig (egl_display, attrs, NULL, 0, &count) || count < 1)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                           _("No available configurations for the given pixel format"));
      return FALSE;
    }

  configs = g_new (EGLConfig, count);

  if (!eglChooseConfig (egl_display, attrs, configs, count, &count) || count < 1)
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_FORMAT,
                           _("No available configurations for the given pixel format"));
      return FALSE;
    }

  /* Pick first valid configuration i guess? */

  if (egl_config_out != NULL)
    *egl_config_out = configs[0];

  g_free (configs);

  return TRUE;
}

static GdkGLContext *
gdk_mir_window_impl_create_gl_context (GdkWindow     *window,
                                       gboolean       attached,
                                       GdkGLContext  *share,
                                       GError       **error)
{
  GdkDisplay *display = gdk_window_get_display (window);
  GdkMirGLContext *context;
  EGLConfig config;

  if (!_gdk_mir_display_init_egl_display (display))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("No GL implementation is available"));
      return NULL;
    }

  if (!_gdk_mir_display_have_egl_khr_create_context (display))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_UNSUPPORTED_PROFILE,
                           _("3.2 core GL profile is not available on EGL implementation"));
      return NULL;
    }

  if (!find_eglconfig_for_window (window, &config, error))
    return NULL;

  context = g_object_new (GDK_TYPE_MIR_GL_CONTEXT,
                          "window", window,
                          "shared-context", share,
                          NULL);

  context->egl_config = config;
  context->is_attached = attached;

  return GDK_GL_CONTEXT (context);
}

EGLSurface
_gdk_mir_window_get_egl_surface (GdkWindow *window,
                                 EGLConfig config)
{
  GdkMirWindowImpl *impl;

  impl = GDK_MIR_WINDOW_IMPL (window->impl);

  if (!impl->egl_surface)
    {
      EGLDisplay egl_display;
      EGLNativeWindowType egl_window;

      ensure_no_mir_window (window);
      ensure_mir_window_full (window, mir_buffer_usage_hardware);

      egl_display = _gdk_mir_display_get_egl_display (gdk_window_get_display (window));
      egl_window = (EGLNativeWindowType) mir_buffer_stream_get_egl_native_window (impl->buffer_stream);

      impl->egl_surface =
        eglCreateWindowSurface (egl_display, config, egl_window, NULL);
    }

  return impl->egl_surface;
}

EGLSurface
_gdk_mir_window_get_dummy_egl_surface (GdkWindow *window,
                                       EGLConfig config)
{
  GdkMirWindowImpl *impl;

  impl = GDK_MIR_WINDOW_IMPL (window->impl);

  if (!impl->dummy_egl_surface)
    {
      GdkDisplay *display;
      EGLDisplay egl_display;
      EGLNativeWindowType egl_window;

      display = gdk_window_get_display (window);
      egl_display = _gdk_mir_display_get_egl_display (display);
      egl_window = (EGLNativeWindowType) mir_buffer_stream_get_egl_native_window (impl->buffer_stream);

      impl->dummy_egl_surface =
        eglCreateWindowSurface (egl_display, config, egl_window, NULL);
    }

  return impl->dummy_egl_surface;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

MirSurface *
gdk_mir_window_get_mir_surface (GdkWindow *window)
{
  return _gdk_mir_window_get_mir_window (window);
}

#pragma GCC diagnostic pop

MirWindow *
_gdk_mir_window_get_mir_window (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_MIR_WINDOW (window), NULL);

  return GDK_MIR_WINDOW_IMPL (window->impl)->mir_window;
}

void
_gdk_mir_window_set_scale (GdkWindow *window,
                           gdouble    scale)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  GdkRectangle area = {0, 0, window->width, window->height};
  cairo_region_t *region;
  gint new_scale = (gint) round (scale);

  if (impl->output_scale != new_scale)
    {
      impl->output_scale = new_scale;

      drop_cairo_surface (window);

      if (impl->buffer_stream)
        mir_buffer_stream_set_scale (impl->buffer_stream, (float) new_scale);

      region = cairo_region_create_rectangle (&area);
      _gdk_window_invalidate_for_expose (window, region);
      cairo_region_destroy (region);
    }
}

static void
gdk_mir_window_impl_class_init (GdkMirWindowImplClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkWindowImplClass *impl_class = GDK_WINDOW_IMPL_CLASS (klass);

  object_class->finalize = gdk_mir_window_impl_finalize;

  impl_class->ref_cairo_surface = gdk_mir_window_impl_ref_cairo_surface;
  impl_class->create_similar_image_surface = gdk_mir_window_impl_create_similar_image_surface;
  impl_class->show = gdk_mir_window_impl_show;
  impl_class->hide = gdk_mir_window_impl_hide;
  impl_class->withdraw = gdk_mir_window_impl_withdraw;
  impl_class->raise = gdk_mir_window_impl_raise;
  impl_class->lower = gdk_mir_window_impl_lower;
  impl_class->restack_toplevel = gdk_mir_window_impl_restack_toplevel;
  impl_class->move_resize = gdk_mir_window_impl_move_resize;
  impl_class->move_to_rect = gdk_mir_window_impl_move_to_rect;
  impl_class->get_events = gdk_mir_window_impl_get_events;
  impl_class->set_events = gdk_mir_window_impl_set_events;
  impl_class->set_device_cursor = gdk_mir_window_impl_set_device_cursor;
  impl_class->get_geometry = gdk_mir_window_impl_get_geometry;
  impl_class->get_root_coords = gdk_mir_window_impl_get_root_coords;
  impl_class->get_device_state = gdk_mir_window_impl_get_device_state;
  impl_class->begin_paint = gdk_mir_window_impl_begin_paint;
  impl_class->end_paint = gdk_mir_window_impl_end_paint;
  impl_class->shape_combine_region = gdk_mir_window_impl_shape_combine_region;
  impl_class->input_shape_combine_region = gdk_mir_window_impl_input_shape_combine_region;
  impl_class->destroy = gdk_mir_window_impl_destroy;
  impl_class->focus = gdk_mir_window_impl_focus;
  impl_class->set_type_hint = gdk_mir_window_impl_set_type_hint;
  impl_class->get_type_hint = gdk_mir_window_impl_get_type_hint;
  impl_class->set_modal_hint = gdk_mir_window_impl_set_modal_hint;
  impl_class->set_skip_taskbar_hint = gdk_mir_window_impl_set_skip_taskbar_hint;
  impl_class->set_skip_pager_hint = gdk_mir_window_impl_set_skip_pager_hint;
  impl_class->set_urgency_hint = gdk_mir_window_impl_set_urgency_hint;
  impl_class->set_geometry_hints = gdk_mir_window_impl_set_geometry_hints;
  impl_class->set_title = gdk_mir_window_impl_set_title;
  impl_class->set_role = gdk_mir_window_impl_set_role;
  impl_class->set_startup_id = gdk_mir_window_impl_set_startup_id;
  impl_class->set_transient_for = gdk_mir_window_impl_set_transient_for;
  impl_class->get_frame_extents = gdk_mir_window_impl_get_frame_extents;
  impl_class->set_accept_focus = gdk_mir_window_impl_set_accept_focus;
  impl_class->set_focus_on_map = gdk_mir_window_impl_set_focus_on_map;
  impl_class->set_icon_list = gdk_mir_window_impl_set_icon_list;
  impl_class->set_icon_name = gdk_mir_window_impl_set_icon_name;
  impl_class->iconify = gdk_mir_window_impl_iconify;
  impl_class->deiconify = gdk_mir_window_impl_deiconify;
  impl_class->stick = gdk_mir_window_impl_stick;
  impl_class->unstick = gdk_mir_window_impl_unstick;
  impl_class->maximize = gdk_mir_window_impl_maximize;
  impl_class->unmaximize = gdk_mir_window_impl_unmaximize;
  impl_class->fullscreen = gdk_mir_window_impl_fullscreen;
  impl_class->apply_fullscreen_mode = gdk_mir_window_impl_apply_fullscreen_mode;
  impl_class->unfullscreen = gdk_mir_window_impl_unfullscreen;
  impl_class->set_keep_above = gdk_mir_window_impl_set_keep_above;
  impl_class->set_keep_below = gdk_mir_window_impl_set_keep_below;
  impl_class->get_group = gdk_mir_window_impl_get_group;
  impl_class->set_group = gdk_mir_window_impl_set_group;
  impl_class->set_decorations = gdk_mir_window_impl_set_decorations;
  impl_class->get_decorations = gdk_mir_window_impl_get_decorations;
  impl_class->set_functions = gdk_mir_window_impl_set_functions;
  impl_class->begin_resize_drag = gdk_mir_window_impl_begin_resize_drag;
  impl_class->begin_move_drag = gdk_mir_window_impl_begin_move_drag;
  impl_class->enable_synchronized_configure = gdk_mir_window_impl_enable_synchronized_configure;
  impl_class->configure_finished = gdk_mir_window_impl_configure_finished;
  impl_class->set_opacity = gdk_mir_window_impl_set_opacity;
  impl_class->destroy_notify = gdk_mir_window_impl_destroy_notify;
  impl_class->get_drag_protocol = gdk_mir_window_impl_get_drag_protocol;
  impl_class->register_dnd = gdk_mir_window_impl_register_dnd;
  impl_class->drag_begin = gdk_mir_window_impl_drag_begin;
  impl_class->process_updates_recurse = gdk_mir_window_impl_process_updates_recurse;
  impl_class->get_property = gdk_mir_window_impl_get_property;
  impl_class->change_property = gdk_mir_window_impl_change_property;
  impl_class->delete_property = gdk_mir_window_impl_delete_property;
  impl_class->get_scale_factor = gdk_mir_window_impl_get_scale_factor;
  impl_class->set_opaque_region = gdk_mir_window_impl_set_opaque_region;
  impl_class->set_shadow_width = gdk_mir_window_impl_set_shadow_width;
  impl_class->create_gl_context = gdk_mir_window_impl_create_gl_context;
}
