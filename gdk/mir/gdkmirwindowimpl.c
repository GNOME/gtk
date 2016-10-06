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

typedef struct _GdkMirWindowImplClass GdkMirWindowImplClass;

struct _GdkMirWindowImpl
{
  GdkWindowImpl parent_instance;

  /* Window we are temporary for */
  GdkWindow *transient_for;
  gint transient_x;
  gint transient_y;

  /* Anchor rectangle */
  gboolean has_rect;
  MirRectangle rect;
  MirEdgeAttachment edge;

  /* Desired surface attributes */
  GdkWindowTypeHint type_hint;
  MirSurfaceState surface_state;

  /* Current button state for checking which buttons are being pressed / released */
  gdouble x;
  gdouble y;
  guint button_state;

  GdkDisplay *display;

  /* Surface being rendered to (only exists when window visible) */
  MirSurface *surface;
  MirBufferStream *buffer_stream;
  MirBufferUsage buffer_usage;

  /* Cairo context for current frame */
  cairo_surface_t *cairo_surface;

  gchar *title;

  GdkGeometry geometry_hints;
  GdkWindowHints geometry_mask;

  /* Egl surface for the current mir surface */
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
static void ensure_surface (GdkWindow *window);
static void apply_geometry_hints (MirSurfaceSpec *spec, GdkMirWindowImpl *impl);

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
_gdk_mir_window_impl_new (GdkDisplay *display, GdkWindow *window, GdkWindowAttr *attributes, gint attributes_mask)
{
  GdkMirWindowImpl *impl = g_object_new (GDK_TYPE_MIR_WINDOW_IMPL, NULL);

  impl->display = display;

  if (attributes && attributes_mask & GDK_WA_TITLE)
    impl->title = g_strdup (attributes->title);
  else
    impl->title = g_strdup (get_default_title ());

  if (attributes && attributes_mask & GDK_WA_TYPE_HINT)
    impl->type_hint = attributes->type_hint;

  impl->pending_spec_update = TRUE;

  return (GdkWindowImpl *) impl;
}

void
_gdk_mir_window_impl_set_surface_state (GdkMirWindowImpl *impl, MirSurfaceState state)
{
  impl->surface_state = state;
}

void
_gdk_mir_window_impl_set_surface_type (GdkMirWindowImpl *impl,
                                       MirSurfaceType    type)
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
  impl->type_hint = GDK_WINDOW_TYPE_HINT_NORMAL;
  impl->surface_state = mir_surface_state_unknown;
  impl->output_scale = 1;
}

static void
set_surface_state (GdkMirWindowImpl *impl,
                   MirSurfaceState state)
{
  MirConnection *connection = gdk_mir_display_get_mir_connection (impl->display);
  if (impl->surface_state == state)
    return;

  impl->surface_state = state;
  if (impl->surface && !impl->pending_spec_update)
    {
      MirSurfaceSpec *spec = mir_connection_create_spec_for_changes (connection);
      mir_surface_spec_set_state (spec, state);
      mir_surface_apply_spec (impl->surface, spec);
      mir_surface_spec_release (spec);
    }
}

static void
event_cb (MirSurface     *surface,
          const MirEvent *event,
          void           *context)
{
  _gdk_mir_event_source_queue (context, event);
}

static MirSurfaceSpec *
create_window_type_spec (GdkDisplay *display,
                         GdkWindow *parent,
                         gint x,
                         gint y,
                         gint width,
                         gint height,
                         GdkWindowTypeHint type,
                         const MirRectangle *rect,
                         MirEdgeAttachment edge,
                         MirBufferUsage buffer_usage)
{
  MirPixelFormat format;
  MirSurface *parent_surface = NULL;
  MirConnection *connection = gdk_mir_display_get_mir_connection (display);
  MirRectangle real_rect;

  format = _gdk_mir_display_get_pixel_format (display, buffer_usage);

  if (parent && parent->impl)
    {
      ensure_surface (parent);
      parent_surface = GDK_MIR_WINDOW_IMPL (parent->impl)->surface;
    }

  if (!parent_surface)
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

  if (rect)
    {
      real_rect = *rect;

      while (parent && !gdk_window_has_native (parent) && gdk_window_get_effective_parent (parent))
        {
          real_rect.left += parent->x;
          real_rect.top += parent->y;
          parent = gdk_window_get_effective_parent (parent);
        }
    }
  else
    {
      real_rect.left = x;
      real_rect.top = y;
      real_rect.width = 1;
      real_rect.height = 1;
    }

  switch (type)
    {
      case GDK_WINDOW_TYPE_HINT_DIALOG:
      case GDK_WINDOW_TYPE_HINT_DOCK:
        return mir_connection_create_spec_for_dialog (connection,
                                                      width,
                                                      height,
                                                      format);
      case GDK_WINDOW_TYPE_HINT_MENU:
      case GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU:
      case GDK_WINDOW_TYPE_HINT_POPUP_MENU:
      case GDK_WINDOW_TYPE_HINT_TOOLBAR:
      case GDK_WINDOW_TYPE_HINT_COMBO:
      case GDK_WINDOW_TYPE_HINT_DND:
      case GDK_WINDOW_TYPE_HINT_TOOLTIP:
      case GDK_WINDOW_TYPE_HINT_NOTIFICATION:
        return mir_connection_create_spec_for_menu (connection,
                                                    width,
                                                    height,
                                                    format,
                                                    parent_surface,
                                                    &real_rect,
                                                    edge);
        break;
      case GDK_WINDOW_TYPE_HINT_SPLASHSCREEN:
      case GDK_WINDOW_TYPE_HINT_UTILITY:
        return mir_connection_create_spec_for_modal_dialog (connection,
                                                            width,
                                                            height,
                                                            format,
                                                            parent_surface);
      case GDK_WINDOW_TYPE_HINT_NORMAL:
      case GDK_WINDOW_TYPE_HINT_DESKTOP:
      default:
        return mir_connection_create_spec_for_normal_surface (connection,
                                                              width,
                                                              height,
                                                              format);
    }
}

static void
apply_geometry_hints (MirSurfaceSpec *spec, GdkMirWindowImpl *impl)
{
  if (impl->geometry_mask & GDK_HINT_RESIZE_INC)
    {
      mir_surface_spec_set_width_increment (spec, impl->geometry_hints.width_inc);
      mir_surface_spec_set_height_increment (spec, impl->geometry_hints.height_inc);
    }
  if (impl->geometry_mask & GDK_HINT_MIN_SIZE)
    {
      mir_surface_spec_set_min_width (spec, impl->geometry_hints.min_width);
      mir_surface_spec_set_min_height (spec, impl->geometry_hints.min_height);
    }
  if (impl->geometry_mask & GDK_HINT_MAX_SIZE)
    {
      mir_surface_spec_set_max_width (spec, impl->geometry_hints.max_width);
      mir_surface_spec_set_max_height (spec, impl->geometry_hints.max_height);
    }
  if (impl->geometry_mask & GDK_HINT_ASPECT)
    {
      mir_surface_spec_set_min_aspect_ratio (spec, 1000, (unsigned)(1000.0/impl->geometry_hints.min_aspect));
      mir_surface_spec_set_max_aspect_ratio (spec, 1000, (unsigned)(1000.0/impl->geometry_hints.max_aspect));
    }
}

static MirSurfaceSpec*
create_spec (GdkWindow *window, GdkMirWindowImpl *impl)
{
  MirSurfaceSpec *spec = NULL;

  spec = create_window_type_spec (impl->display,
                                  impl->transient_for,
                                  impl->transient_x, impl->transient_y,
                                  window->width, window->height,
                                  impl->type_hint,
                                  impl->has_rect ? &impl->rect : NULL,
                                  impl->has_rect ? impl->edge : mir_edge_attachment_any,
                                  impl->buffer_usage);

  mir_surface_spec_set_name (spec, impl->title);
  mir_surface_spec_set_buffer_usage (spec, impl->buffer_usage);

  apply_geometry_hints (spec, impl);

  return spec;
}

static void
update_surface_spec (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  MirSurfaceSpec *spec;

  if (!impl->surface)
    return;

  spec = create_spec (window, impl);

  mir_surface_apply_spec (impl->surface, spec);
  mir_surface_spec_release (spec);
  impl->pending_spec_update = FALSE;
  impl->buffer_stream = mir_surface_get_buffer_stream (impl->surface);
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
ensure_surface_full (GdkWindow *window,
                     MirBufferUsage buffer_usage)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  GdkMirWindowReference *window_ref;
  MirSurfaceSpec *spec;

  if (impl->surface)
    {
      if (impl->pending_spec_update)
        update_surface_spec(window);
      return;
    }

  /* no destroy notify -- we must leak for now
   * https://bugs.launchpad.net/mir/+bug/1324100
   */
  window_ref = _gdk_mir_event_source_get_window_reference (window);
  impl->buffer_usage = buffer_usage;

  spec = create_spec (window, impl);

  impl->surface = mir_surface_create_sync (spec);

  mir_surface_spec_release(spec);

  impl->pending_spec_update = FALSE;
  impl->buffer_stream = mir_surface_get_buffer_stream (impl->surface);

  /* FIXME: can't make an initial resize event */
  // MirEvent *resize_event;

  /* Send the initial configure with the size the server gave... */
  /* FIXME: can't make an initial resize event */
  /*
  resize_event.resize.type = mir_event_type_resize;
  resize_event.resize.surface_id = 0;
  resize_event.resize.width = window->width;
  resize_event.resize.height = window->height;

  _gdk_mir_event_source_queue (window_ref, &resize_event);
  */

  generate_configure_event (window, window->width, window->height);

  mir_surface_set_event_handler (impl->surface, event_cb, window_ref); // FIXME: Ignore some events until shown
}

static void
ensure_surface (GdkWindow *window)
{
  ensure_surface_full (window,
                       window->gl_paint_context ?
                         mir_buffer_usage_hardware :
                         mir_buffer_usage_software);
}

static void
ensure_no_surface (GdkWindow *window)
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

  g_clear_pointer(&impl->surface, mir_surface_release_sync);
}

static void
send_buffer (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  /* Send the completed buffer to Mir */
  mir_buffer_stream_swap_buffers_sync (mir_surface_get_buffer_stream (impl->surface));

  /* The Cairo context is no longer valid */
  g_clear_pointer (&impl->cairo_surface, cairo_surface_destroy);
  if (impl->pending_spec_update)
    update_surface_spec (window);

  impl->pending_spec_update = FALSE;
}

static cairo_surface_t *
gdk_mir_window_impl_ref_cairo_surface (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_ref_cairo_surface window=%p\n", window);
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  MirGraphicsRegion region;
  cairo_format_t pixel_format = CAIRO_FORMAT_ARGB32;
  cairo_surface_t *cairo_surface;
  cairo_t *c;

  if (impl->cairo_surface)
    {
      cairo_surface_reference (impl->cairo_surface);
      return impl->cairo_surface;
    }

  ensure_surface (window);

  if (window->gl_paint_context)
    {
      cairo_surface = cairo_image_surface_create (pixel_format, window->width, window->height);
      cairo_surface_set_device_scale (cairo_surface, (double) impl->output_scale, (double) impl->output_scale);
    }
  else if (impl->visible)
    {
      mir_buffer_stream_get_graphics_region (mir_surface_get_buffer_stream (impl->surface), &region);

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
  //g_printerr ("gdk_mir_window_impl_create_similar_image_surface window=%p\n", window);
  return cairo_image_surface_create (format, width, height);
}

static void
gdk_mir_window_impl_finalize (GObject *object)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (object);

  g_free (impl->title);
  if (impl->surface)
    mir_surface_release_sync (impl->surface);
  if (impl->cairo_surface)
    cairo_surface_destroy (impl->cairo_surface);

  G_OBJECT_CLASS (gdk_mir_window_impl_parent_class)->finalize (object);
}

static void
gdk_mir_window_impl_show (GdkWindow *window,
                          gboolean   already_mapped)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  cairo_surface_t *s;

  //g_printerr ("gdk_mir_window_impl_show window=%p\n", window);

  impl->visible = TRUE;
  set_surface_state (impl, mir_surface_state_restored);

  /* Make sure there's a surface to see */
  ensure_surface (window);

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
  //g_printerr ("gdk_mir_window_impl_hide window=%p\n", window);
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  impl->cursor_inside = FALSE;
  impl->visible = FALSE;

  set_surface_state (impl, mir_surface_state_hidden);
}

static void
gdk_mir_window_impl_withdraw (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_withdraw window=%p\n", window);
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  impl->cursor_inside = FALSE;
  impl->visible = FALSE;

  set_surface_state (impl, mir_surface_state_hidden);
}

static void
gdk_mir_window_impl_raise (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_raise window=%p\n", window);
  /* We don't support client window stacking */
}

static void
gdk_mir_window_impl_lower (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_lower window=%p\n", window);
  /* We don't support client window stacking */
}

static void
gdk_mir_window_impl_restack_under (GdkWindow *window,
                                   GList     *native_siblings)
{
  //g_printerr ("gdk_mir_window_impl_restack_under window=%p\n", window);
  /* We don't support client window stacking */
}

static void
gdk_mir_window_impl_restack_toplevel (GdkWindow *window,
                                      GdkWindow *sibling,
                                      gboolean   above)
{
  //g_printerr ("gdk_mir_window_impl_restack_toplevel window=%p sibling=%p\n", window, sibling);
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
  /*
  g_printerr ("gdk_mir_window_impl_move_resize");
  g_printerr (" window=%p", window);
  if (with_move)
    g_printerr (" location=%d,%d", x, y);
  if (width > 0)
    g_printerr (" size=%dx%dpx", width, height);
  g_printerr ("\n");
  */
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  /* If resize requested then rebuild surface */
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
          if (!impl->pending_spec_update && impl->surface)
            update_surface_spec (window);
        }
    }
}

static MirEdgeAttachment
get_edge_for_anchors (GdkGravity     rect_anchor,
                      GdkGravity     window_anchor,
                      GdkAnchorHints anchor_hints)
{
  MirEdgeAttachment edge = 0;

  if (anchor_hints & GDK_ANCHOR_FLIP_X)
    edge |= mir_edge_attachment_vertical;

  if (anchor_hints & GDK_ANCHOR_FLIP_Y)
    edge |= mir_edge_attachment_horizontal;

  return edge;
}

static void
get_rect_for_edge (MirRectangle       *out_rect,
                   const GdkRectangle *in_rect,
                   MirEdgeAttachment   edge,
                   GdkWindow          *window)
{
  out_rect->left = in_rect->x;
  out_rect->top = in_rect->y;
  out_rect->width = in_rect->width;
  out_rect->height = in_rect->height;

  switch (edge)
    {
    case mir_edge_attachment_vertical:
      out_rect->left += window->shadow_right;
      out_rect->top -= window->shadow_top;
      out_rect->width -= window->shadow_left + window->shadow_right;
      out_rect->height += window->shadow_top + window->shadow_bottom;
      break;

    case mir_edge_attachment_horizontal:
      out_rect->left -= window->shadow_left;
      out_rect->top += window->shadow_bottom;
      out_rect->width += window->shadow_left + window->shadow_right;
      out_rect->height -= window->shadow_top + window->shadow_bottom;
      break;

    default:
      break;
    }
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

  impl->edge = get_edge_for_anchors (rect_anchor, window_anchor, anchor_hints);
  get_rect_for_edge (&impl->rect, rect, impl->edge, window);
  impl->has_rect = TRUE;

  ensure_no_surface (window);

  g_signal_emit_by_name (window,
                         "moved-to-rect",
                         NULL,
                         NULL,
                         FALSE,
                         FALSE);
}

static GdkEventMask
gdk_mir_window_impl_get_events (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_get_events window=%p\n", window);
  return window->event_mask;
}

static void
gdk_mir_window_impl_set_events (GdkWindow    *window,
                                GdkEventMask  event_mask)
{
  //g_printerr ("gdk_mir_window_impl_set_events window=%p\n", window);
  /* We send all events and let GDK decide */
}

static gboolean
gdk_mir_window_impl_reparent (GdkWindow *window,
                              GdkWindow *new_parent,
                              gint       x,
                              gint       y)
{
  //g_printerr ("gdk_mir_window_impl_reparent window=%p new-parent=%p\n", window, new_parent);
  return FALSE;
}

static void
gdk_mir_window_impl_set_device_cursor (GdkWindow *window,
                                       GdkDevice *device,
                                       GdkCursor *cursor)
{
  const gchar *cursor_name;
  MirCursorConfiguration *configuration;

  if (cursor)
    cursor_name = _gdk_mir_cursor_get_name (cursor);
  else
    cursor_name = mir_default_cursor_name;

  configuration = mir_cursor_configuration_from_name (cursor_name);

  if (configuration)
    {
      GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

      if (impl->surface)
        mir_surface_configure_cursor (impl->surface, configuration);

      mir_cursor_configuration_destroy (configuration);
    }
}

static void
gdk_mir_window_impl_get_geometry (GdkWindow *window,
                                  gint      *x,
                                  gint      *y,
                                  gint      *width,
                                  gint      *height)
{
  //g_printerr ("gdk_mir_window_impl_get_geometry window=%p\n", window);

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
  //g_printerr ("gdk_mir_window_impl_get_root_coords window=%p\n", window);

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
  //g_printerr ("gdk_mir_window_impl_get_device_state window=%p\n", window);
  GdkWindow *child;

  _gdk_device_query_state (device, window, NULL, &child, NULL, NULL, x, y, mask);

  return child != NULL;
}

static gboolean
gdk_mir_window_impl_begin_paint (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_begin_paint window=%p\n", window);
  /* Indicate we are ready to be drawn onto directly? */
  return FALSE;
}

static void
gdk_mir_window_impl_end_paint (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  //g_printerr ("gdk_mir_window_impl_end_paint window=%p\n", window);
  if (impl->visible && !window->current_paint.use_gl)
    send_buffer (window);
}

static cairo_region_t *
gdk_mir_window_impl_get_shape (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_get_shape window=%p\n", window);
  return NULL;
}

static cairo_region_t *
gdk_mir_window_impl_get_input_shape (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_get_input_shape window=%p\n", window);
  return NULL;
}

static void
gdk_mir_window_impl_shape_combine_region (GdkWindow            *window,
                                          const cairo_region_t *shape_region,
                                          gint                  offset_x,
                                          gint                  offset_y)
{
  //g_printerr ("gdk_mir_window_impl_shape_combine_region window=%p\n", window);
}

static void
gdk_mir_window_impl_input_shape_combine_region (GdkWindow            *window,
                                                const cairo_region_t *shape_region,
                                                gint                  offset_x,
                                                gint                  offset_y)
{
  // g_printerr ("gdk_mir_window_impl_input_shape_combine_region window=%p\n", window);
}

static void
gdk_mir_window_impl_destroy (GdkWindow *window,
                             gboolean   recursing,
                             gboolean   foreign_destroy)
{
  //g_printerr ("gdk_mir_window_impl_destroy window=%p\n", window);
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  impl->visible = FALSE;
  ensure_no_surface (window);
}

static void
gdk_mir_window_impl_destroy_foreign (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_destroy_foreign window=%p\n", window);
}

static void
gdk_mir_window_impl_focus (GdkWindow *window,
                      guint32    timestamp)
{
  //g_printerr ("gdk_mir_window_impl_focus window=%p\n", window);
}

static void
gdk_mir_window_impl_set_type_hint (GdkWindow         *window,
                                   GdkWindowTypeHint  hint)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  if (type_hint_differs (hint, impl->type_hint))
    {
      impl->type_hint = hint;
      if (impl->surface && !impl->pending_spec_update)
        update_surface_spec (window);
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
  //g_printerr ("gdk_mir_window_impl_set_modal_hint window=%p\n", window);
  /* Mir doesn't support modal windows */
}

static void
gdk_mir_window_impl_set_skip_taskbar_hint (GdkWindow *window,
                                           gboolean   skips_taskbar)
{
  //g_printerr ("gdk_mir_window_impl_set_skip_taskbar_hint window=%p\n", window);
}

static void
gdk_mir_window_impl_set_skip_pager_hint (GdkWindow *window,
                                         gboolean   skips_pager)
{
  //g_printerr ("gdk_mir_window_impl_set_skip_pager_hint window=%p\n", window);
}

static void
gdk_mir_window_impl_set_urgency_hint (GdkWindow *window,
                                      gboolean   urgent)
{
  //g_printerr ("gdk_mir_window_impl_set_urgency_hint window=%p\n", window);
}

static void
gdk_mir_window_impl_set_geometry_hints (GdkWindow         *window,
                                        const GdkGeometry *geometry,
                                        GdkWindowHints     geom_mask)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  MirConnection *connection = gdk_mir_display_get_mir_connection (impl->display);
  //g_printerr ("gdk_mir_window_impl_set_geometry_hints window=%p impl=%p\n", window, impl);

  impl->geometry_hints = *geometry;
  impl->geometry_mask = geom_mask;

  if (impl->surface && !impl->pending_spec_update)
    {
       MirSurfaceSpec* spec = mir_connection_create_spec_for_changes (connection);
       apply_geometry_hints (spec, impl);
       mir_surface_apply_spec (impl->surface, spec);
       mir_surface_spec_release (spec);
    }
}

static void
gdk_mir_window_impl_set_title (GdkWindow   *window,
                               const gchar *title)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  MirConnection *connection = gdk_mir_display_get_mir_connection (impl->display);
  //g_printerr ("gdk_mir_window_impl_set_title window=%p\n", window);

  g_free (impl->title);
  impl->title = g_strdup (title);
  if (impl->surface && !impl->pending_spec_update)
    {
       MirSurfaceSpec* spec = mir_connection_create_spec_for_changes (connection);
       mir_surface_spec_set_name (spec, impl->title);
       mir_surface_apply_spec (impl->surface, spec);
       mir_surface_spec_release (spec);
    }
}

static void
gdk_mir_window_impl_set_role (GdkWindow   *window,
                              const gchar *role)
{
  //g_printerr ("gdk_mir_window_impl_set_role window=%p\n", window);
}

static void
gdk_mir_window_impl_set_startup_id (GdkWindow   *window,
                                    const gchar *startup_id)
{
  //g_printerr ("gdk_mir_window_impl_set_startup_id window=%p\n", window);
}

static void
gdk_mir_window_impl_set_transient_for (GdkWindow *window,
                                       GdkWindow *parent)
{
  //g_printerr ("gdk_mir_window_impl_set_transient_for window=%p\n", window);
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  if (impl->transient_for == parent)
    return;

  /* Link this window to the parent */
  impl->transient_for = parent;

  if (impl->surface && !impl->pending_spec_update)
    update_surface_spec (window);
}

static void
gdk_mir_window_impl_get_frame_extents (GdkWindow    *window,
                                       GdkRectangle *rect)
{
  //g_printerr ("gdk_mir_window_impl_get_frame_extents window=%p\n", window);
}

static void
gdk_mir_window_impl_set_override_redirect (GdkWindow *window,
                                           gboolean   override_redirect)
{
  //g_printerr ("gdk_mir_window_impl_set_override_redirect window=%p\n", window);
}

static void
gdk_mir_window_impl_set_accept_focus (GdkWindow *window,
                                      gboolean   accept_focus)
{
  //g_printerr ("gdk_mir_window_impl_set_accept_focus window=%p\n", window);
  /* Mir clients cannot control focus */
}

static void
gdk_mir_window_impl_set_focus_on_map (GdkWindow *window,
                                      gboolean focus_on_map)
{
  //g_printerr ("gdk_mir_window_impl_set_focus_on_map window=%p\n", window);
  /* Mir clients cannot control focus */
}

static void
gdk_mir_window_impl_set_icon_list (GdkWindow *window,
                                   GList     *pixbufs)
{
  //g_printerr ("gdk_mir_window_impl_set_icon_list window=%p\n", window);
  // ??
}

static void
gdk_mir_window_impl_set_icon_name (GdkWindow   *window,
                                   const gchar *name)
{
  //g_printerr ("gdk_mir_window_impl_set_icon_name window=%p\n", window);
}

static void
gdk_mir_window_impl_iconify (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_iconify window=%p\n", window);
  /* We don't support iconification */
}

static void
gdk_mir_window_impl_deiconify (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_deiconify window=%p\n", window);
  /* We don't support iconification */
}

static void
gdk_mir_window_impl_stick (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_stick window=%p\n", window);
  /* We do not support stick/unstick in Mir */
}

static void
gdk_mir_window_impl_unstick (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_unstick window=%p\n", window);
  /* We do not support stick/unstick in Mir */
}

static void
gdk_mir_window_impl_maximize (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_maximize window=%p\n", window);
  set_surface_state (GDK_MIR_WINDOW_IMPL (window->impl), mir_surface_state_maximized);
}

static void
gdk_mir_window_impl_unmaximize (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_unmaximize window=%p\n", window);
  set_surface_state (GDK_MIR_WINDOW_IMPL (window->impl), mir_surface_state_restored);
}

static void
gdk_mir_window_impl_fullscreen (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_fullscreen window=%p\n", window);
  set_surface_state (GDK_MIR_WINDOW_IMPL (window->impl), mir_surface_state_fullscreen);
}

static void
gdk_mir_window_impl_apply_fullscreen_mode (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_apply_fullscreen_mode window=%p\n", window);
}

static void
gdk_mir_window_impl_unfullscreen (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_unfullscreen window=%p\n", window);
  set_surface_state (GDK_MIR_WINDOW_IMPL (window->impl), mir_surface_state_restored);
}

static void
gdk_mir_window_impl_set_keep_above (GdkWindow *window,
                                    gboolean   setting)
{
  //g_printerr ("gdk_mir_window_impl_set_keep_above window=%p\n", window);
  /* We do not support keep above/below in Mir */
}

static void
gdk_mir_window_impl_set_keep_below (GdkWindow *window,
                                    gboolean setting)
{
  //g_printerr ("gdk_mir_window_impl_set_keep_below window=%p\n", window);
  /* We do not support keep above/below in Mir */
}

static GdkWindow *
gdk_mir_window_impl_get_group (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_get_group window=%p\n", window);
  return NULL;
}

static void
gdk_mir_window_impl_set_group (GdkWindow *window,
                               GdkWindow *leader)
{
  //g_printerr ("gdk_mir_window_impl_set_group window=%p\n", window);
}

static void
gdk_mir_window_impl_set_decorations (GdkWindow       *window,
                                     GdkWMDecoration  decorations)
{
  //g_printerr ("gdk_mir_window_impl_set_decorations window=%p decorations=%d\n", window, decorations);
}

static gboolean
gdk_mir_window_impl_get_decorations (GdkWindow       *window,
                                     GdkWMDecoration *decorations)
{
  //g_printerr ("gdk_mir_window_impl_get_decorations window=%p\n", window);
  return FALSE;
}

static void
gdk_mir_window_impl_set_functions (GdkWindow     *window,
                                   GdkWMFunction  functions)
{
  //g_printerr ("gdk_mir_window_impl_set_functions window=%p\n", window);
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
  //g_printerr ("gdk_mir_window_impl_begin_resize_drag window=%p\n", window);
}

static void
gdk_mir_window_impl_begin_move_drag (GdkWindow *window,
                                     GdkDevice *device,
                                     gint       button,
                                     gint       root_x,
                                     gint       root_y,
                                     guint32    timestamp)
{
  //g_printerr ("gdk_mir_window_impl_begin_move_drag window=%p\n", window);
}

static void
gdk_mir_window_impl_enable_synchronized_configure (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_enable_synchronized_configure window=%p\n", window);
}

static void
gdk_mir_window_impl_configure_finished (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_configure_finished window=%p\n", window);
}

static void
gdk_mir_window_impl_set_opacity (GdkWindow *window,
                                 gdouble    opacity)
{
  //g_printerr ("gdk_mir_window_impl_set_opacity window=%p\n", window);
  // FIXME
}

static void
gdk_mir_window_impl_destroy_notify (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_destroy_notify window=%p\n", window);
}

static GdkDragProtocol
gdk_mir_window_impl_get_drag_protocol (GdkWindow *window,
                                       GdkWindow **target)
{
  //g_printerr ("gdk_mir_window_impl_get_drag_protocol window=%p\n", window);
  return 0;
}

static void
gdk_mir_window_impl_register_dnd (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_register_dnd window=%p\n", window);
}

static GdkDragContext *
gdk_mir_window_impl_drag_begin (GdkWindow *window,
                                GdkDevice *device,
                                GList     *targets,
                                gint       x_root,
                                gint       y_root)
{
  //g_printerr ("gdk_mir_window_impl_drag_begin window=%p\n", window);
  return NULL;
}

static void
gdk_mir_window_impl_process_updates_recurse (GdkWindow      *window,
                                             cairo_region_t *region)
{
  //g_printerr ("gdk_mir_window_impl_process_updates_recurse window=%p\n", window);
  cairo_rectangle_int_t rectangle;

  /* We redraw the whole region, but we should track the buffers and only redraw what has changed since we sent this buffer */
  rectangle.x = 0;
  rectangle.y = 0;
  rectangle.width = window->width;
  rectangle.height = window->height;
  cairo_region_union_rectangle (region, &rectangle);

  _gdk_window_process_updates_recurse (window, region);
}

static void
gdk_mir_window_impl_sync_rendering (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_sync_rendering window=%p\n", window);
  // FIXME: Only used for benchmarking
}

static gboolean
gdk_mir_window_impl_simulate_key (GdkWindow       *window,
                                  gint             x,
                                  gint             y,
                                  guint            keyval,
                                  GdkModifierType  modifiers,
                                  GdkEventType     key_pressrelease)
{
  //g_printerr ("gdk_mir_window_impl_simulate_key window=%p\n", window);
  return FALSE;
}

static gboolean
gdk_mir_window_impl_simulate_button (GdkWindow       *window,
                                     gint             x,
                                     gint             y,
                                     guint            button,
                                     GdkModifierType  modifiers,
                                     GdkEventType     button_pressrelease)
{
  //g_printerr ("gdk_mir_window_impl_simulate_button window=%p\n", window);
  return FALSE;
}

static gboolean
gdk_mir_window_impl_get_property (GdkWindow   *window,
                                  GdkAtom      property,
                                  GdkAtom      type,
                                  gulong       offset,
                                  gulong       length,
                                  gint         pdelete,
                                  GdkAtom     *actual_property_type,
                                  gint        *actual_format_type,
                                  gint        *actual_length,
                                  guchar     **data)
{
  //g_printerr ("gdk_mir_window_impl_get_property window=%p\n", window);
  return FALSE;
}

static void
gdk_mir_window_impl_change_property (GdkWindow    *window,
                                     GdkAtom       property,
                                     GdkAtom       type,
                                     gint          format,
                                     GdkPropMode   mode,
                                     const guchar *data,
                                     gint          nelements)
{
  //g_printerr ("gdk_mir_window_impl_change_property window=%p\n", window);
}

static void
gdk_mir_window_impl_delete_property (GdkWindow *window,
                                     GdkAtom    property)
{
  //g_printerr ("gdk_mir_window_impl_delete_property window=%p\n", window);
}

static gint
gdk_mir_window_impl_get_scale_factor (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_get_scale_factor window=%p\n", window);
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  return impl->output_scale;
}

static void
gdk_mir_window_impl_set_opaque_region (GdkWindow      *window,
                                       cairo_region_t *region)
{
  //g_printerr ("gdk_mir_window_impl_set_opaque_region window=%p\n", window);
  /* FIXME: An optimisation to tell the compositor which regions of the window are fully transparent */
}

static void
gdk_mir_window_impl_set_shadow_width (GdkWindow *window,
                                      gint       left,
                                      gint       right,
                                      gint       top,
                                      gint       bottom)
{
  // g_printerr ("gdk_mir_window_impl_set_shadow_width window=%p\n", window);
}

static gboolean
find_eglconfig_for_window (GdkWindow  *window,
                           EGLConfig  *egl_config_out,
                           GError    **error)
{
  GdkDisplay *display = gdk_window_get_display (window);
  EGLDisplay *egl_display = _gdk_mir_display_get_egl_display (display);
  GdkVisual *visual = gdk_window_get_visual (window);
  EGLint attrs[MAX_EGL_ATTRS];
  EGLint count;
  EGLConfig *configs;
  gboolean use_rgba;

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

  use_rgba = (visual == gdk_screen_get_rgba_visual (gdk_display_get_default_screen (display)));

  if (use_rgba)
    {
      attrs[i++] = EGL_ALPHA_SIZE;
      attrs[i++] = 1;
    }
  else
    {
      attrs[i++] = EGL_ALPHA_SIZE;
      attrs[i++] = 0;
    }

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
                          "display", display,
                          "window", window,
                          "shared-context", share,
                          NULL);

  context->egl_config = config;
  context->is_attached = attached;

  return GDK_GL_CONTEXT (context);
}

static void
gdk_mir_window_impl_invalidate_for_new_frame (GdkWindow *window,
                                              cairo_region_t *update_area)
{
  cairo_rectangle_int_t window_rect;
  GdkDisplay *display = gdk_window_get_display (window);
  GdkMirGLContext *context_mir;
  int buffer_age;
  gboolean invalidate_all;
  EGLSurface egl_surface;

  /* Minimal update is ok if we're not drawing with gl */
  if (window->gl_paint_context == NULL)
    return;

  context_mir = GDK_MIR_GL_CONTEXT (window->gl_paint_context);
  buffer_age = 0;

  egl_surface = _gdk_mir_window_get_egl_surface (window, context_mir->egl_config);

  if (_gdk_mir_display_have_egl_buffer_age (display))
    {
      gdk_gl_context_make_current (window->gl_paint_context);
      eglQuerySurface (_gdk_mir_display_get_egl_display (display), egl_surface,
                       EGL_BUFFER_AGE_EXT, &buffer_age);
    }

  invalidate_all = FALSE;
  if (buffer_age == 0 || buffer_age >= 4)
    invalidate_all = TRUE;
  else
    {
      if (buffer_age >= 2)
        {
          if (window->old_updated_area[0])
            cairo_region_union (update_area, window->old_updated_area[0]);
          else
            invalidate_all = TRUE;
        }
      if (buffer_age >= 3)
        {
          if (window->old_updated_area[1])
            cairo_region_union (update_area, window->old_updated_area[1]);
          else
            invalidate_all = TRUE;
        }
    }

  if (invalidate_all)
    {
      window_rect.x = 0;
      window_rect.y = 0;
      window_rect.width = gdk_window_get_width (window);
      window_rect.height = gdk_window_get_height (window);

      /* If nothing else is known, repaint everything so that the back
         buffer is fully up-to-date for the swapbuffer */
      cairo_region_union_rectangle (update_area, &window_rect);
    }
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

      ensure_no_surface (window);
      ensure_surface_full (window, mir_buffer_usage_hardware);

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

MirSurface *
gdk_mir_window_get_mir_surface (GdkWindow *window)
{
  GdkMirWindowImpl *impl;

  g_return_val_if_fail (GDK_IS_MIR_WINDOW (window), NULL);

  impl = GDK_MIR_WINDOW_IMPL (window->impl);

  return impl->surface;
}

void
_gdk_mir_window_set_surface_output (GdkWindow *window, gdouble scale)
{
  // g_printerr ("_gdk_mir_window_impl_set_surface_output impl=%p\n", impl);
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
  impl_class->restack_under = gdk_mir_window_impl_restack_under;
  impl_class->restack_toplevel = gdk_mir_window_impl_restack_toplevel;
  impl_class->move_resize = gdk_mir_window_impl_move_resize;
  impl_class->move_to_rect = gdk_mir_window_impl_move_to_rect;
  impl_class->set_background = gdk_mir_window_impl_set_background;
  impl_class->get_events = gdk_mir_window_impl_get_events;
  impl_class->set_events = gdk_mir_window_impl_set_events;
  impl_class->reparent = gdk_mir_window_impl_reparent;
  impl_class->set_device_cursor = gdk_mir_window_impl_set_device_cursor;
  impl_class->get_geometry = gdk_mir_window_impl_get_geometry;
  impl_class->get_root_coords = gdk_mir_window_impl_get_root_coords;
  impl_class->get_device_state = gdk_mir_window_impl_get_device_state;
  impl_class->begin_paint = gdk_mir_window_impl_begin_paint;
  impl_class->end_paint = gdk_mir_window_impl_end_paint;
  impl_class->get_shape = gdk_mir_window_impl_get_shape;
  impl_class->get_input_shape = gdk_mir_window_impl_get_input_shape;
  impl_class->shape_combine_region = gdk_mir_window_impl_shape_combine_region;
  impl_class->input_shape_combine_region = gdk_mir_window_impl_input_shape_combine_region;
  impl_class->destroy = gdk_mir_window_impl_destroy;
  impl_class->destroy_foreign = gdk_mir_window_impl_destroy_foreign;
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
  impl_class->set_override_redirect = gdk_mir_window_impl_set_override_redirect;
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
  impl_class->sync_rendering = gdk_mir_window_impl_sync_rendering;
  impl_class->simulate_key = gdk_mir_window_impl_simulate_key;
  impl_class->simulate_button = gdk_mir_window_impl_simulate_button;
  impl_class->get_property = gdk_mir_window_impl_get_property;
  impl_class->change_property = gdk_mir_window_impl_change_property;
  impl_class->delete_property = gdk_mir_window_impl_delete_property;
  impl_class->get_scale_factor = gdk_mir_window_impl_get_scale_factor;
  impl_class->set_opaque_region = gdk_mir_window_impl_set_opaque_region;
  impl_class->set_shadow_width = gdk_mir_window_impl_set_shadow_width;
  impl_class->create_gl_context = gdk_mir_window_impl_create_gl_context;
  impl_class->invalidate_for_new_frame = gdk_mir_window_impl_invalidate_for_new_frame;
}
