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

#include "config.h"

#include "gdk.h"
#include "gdkmir.h"
#include "gdkmir-private.h"

#include "gdkwindowimpl.h"
#include "gdkinternals.h"
#include "gdkdisplayprivate.h"
#include "gdkdeviceprivate.h"

#define GDK_MIR_WINDOW_IMPL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WINDOW_IMPL_MIR, GdkMirWindowImplClass))
#define GDK_IS_WINDOW_IMPL_MIR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WINDOW_IMPL_MIR))
#define GDK_MIR_WINDOW_IMPL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WINDOW_IMPL_MIR, GdkMirWindowImplClass))

typedef struct _GdkMirWindowImplClass GdkMirWindowImplClass;

struct _GdkMirWindowImpl
{
  GdkWindowImpl parent_instance;

  /* Window we are temporary for */
  GdkWindow *transient_for;
  gint transient_x;
  gint transient_y;

  /* Child windows (e.g. tooltips) */
  GList *transient_children;

  /* Desired surface attributes */
  MirSurfaceType surface_type; // FIXME
  MirSurfaceState surface_state;

  /* Pattern for background */
  cairo_pattern_t *background;

  /* Current button state for checking which buttons are being pressed / released */
  gdouble x;
  gdouble y;
  MirMotionButton button_state;

  /* Surface being rendered to (only exists when window visible) */
  MirSurface *surface;

  /* Cairo context for current frame */
  cairo_surface_t *cairo_surface;

  /* TRUE if the window can be seen */
  gboolean visible;

  /* TRUE if cursor is inside this window */
  gboolean cursor_inside;
};

struct _GdkMirWindowImplClass
{
  GdkWindowImplClass parent_class;
};

G_DEFINE_TYPE (GdkMirWindowImpl, gdk_mir_window_impl, GDK_TYPE_WINDOW_IMPL)

static cairo_surface_t *gdk_mir_window_impl_ref_cairo_surface (GdkWindow *window);

GdkWindowImpl *
_gdk_mir_window_impl_new (void)
{
  return g_object_new (GDK_TYPE_MIR_WINDOW_IMPL, NULL);
}

void
_gdk_mir_window_impl_set_surface_state (GdkMirWindowImpl *impl, MirSurfaceState state)
{
  impl->surface_state = state;
}

void
_gdk_mir_window_impl_set_cursor_state (GdkMirWindowImpl *impl,
                                       gdouble x,
                                       gdouble y,
                                       gboolean cursor_inside,
                                       MirMotionButton button_state)
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
                                       MirMotionButton *button_state)
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
}

static MirConnection *
get_connection (GdkWindow *window)
{
  return gdk_mir_display_get_mir_connection (gdk_window_get_display (window));
}

static void
set_surface_state (GdkMirWindowImpl *impl,
                   MirSurfaceState state)
{
  impl->surface_state = state;
  if (impl->surface)
    mir_surface_set_state (impl->surface, state);
}

static void
event_cb (MirSurface     *surface,
          const MirEvent *event,
          void           *context)
{
  _gdk_mir_event_source_queue (context, event);
}

static void
ensure_surface (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  MirPixelFormat formats[100], pixel_format = mir_pixel_format_invalid;
  unsigned int n_formats, i;
  MirSurfaceParameters parameters;
  MirEventDelegate event_delegate = { event_cb, NULL };
  GdkMirWindowReference *window_ref;

  if (impl->surface)
    return;

  /* no destroy notify -- we must leak for now
   * https://bugs.launchpad.net/mir/+bug/1324100
   */
  window_ref = _gdk_mir_event_source_get_window_reference (window);

  event_delegate.context = window_ref;

  // Should probably calculate this once?
  // Should prefer certain formats over others
  mir_connection_get_available_surface_formats (get_connection (window), formats, 100, &n_formats);
  for (i = 0; i < n_formats; i++)
    if (formats[i] == mir_pixel_format_argb_8888)
      {
        pixel_format = formats[i];
        break;
      }

  parameters.name = "GTK+ Mir";
  parameters.width = window->width;
  parameters.height = window->height;
  parameters.pixel_format = pixel_format;
  parameters.buffer_usage = mir_buffer_usage_software;
  parameters.output_id = mir_display_output_id_invalid;
  impl->surface = mir_connection_create_surface_sync (get_connection (window), &parameters);

  MirGraphicsRegion region;
  MirEvent resize_event;

  mir_surface_get_graphics_region (impl->surface, &region);

  /* Send the initial configure with the size the server gave... */
  resize_event.resize.type = mir_event_type_resize;
  resize_event.resize.surface_id = 0;
  resize_event.resize.width = region.width;
  resize_event.resize.height = region.height;

  _gdk_mir_event_source_queue (window_ref, &resize_event);

  mir_surface_set_event_handler (impl->surface, &event_delegate); // FIXME: Ignore some events until shown
  set_surface_state (impl, impl->surface_state);
}

static void
ensure_no_surface (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  if (impl->cairo_surface)
    {
      cairo_surface_finish (impl->cairo_surface);
      cairo_surface_destroy (impl->cairo_surface);
      impl->cairo_surface = NULL;
    }

  if (impl->surface)
    {
      mir_surface_release_sync (impl->surface);
      impl->surface = NULL;
    }
}

static void
redraw_transient (GdkWindow *window)
{
  GdkRectangle r;
  r.x = window->x;
  r.y = window->y;
  r.width = window->width;
  r.height = window->height;
  gdk_window_invalidate_rect (GDK_MIR_WINDOW_IMPL (window->impl)->transient_for, &r, FALSE);
}

static void
send_buffer (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  /* Transient windows draw onto parent instead */
  if (impl->transient_for)
    {
      redraw_transient (window);
      return;
    }

  /* Composite transient windows over this one */
  if (impl->transient_children)
    {
      cairo_surface_t *surface;
      cairo_t *c;
      GList *link;

      surface = gdk_mir_window_impl_ref_cairo_surface (window);
      c = cairo_create (surface);

      for (link = impl->transient_children; link; link = link->next)
        {
          GdkWindow *child_window = link->data;
          GdkMirWindowImpl *child_impl = GDK_MIR_WINDOW_IMPL (child_window->impl);

          /* Skip children not yet drawn to */
          if (!child_impl->cairo_surface)
            continue;

          cairo_set_source_surface (c, child_impl->cairo_surface, child_window->x, child_window->y);
          cairo_rectangle (c, child_window->x, child_window->y, child_window->width, child_window->height);
          cairo_fill (c);
        }

      cairo_destroy (c);
      cairo_surface_destroy (surface);
    }

  /* Send the completed buffer to Mir */
  mir_surface_swap_buffers_sync (impl->surface);

  /* The Cairo context is no longer valid */
  if (impl->cairo_surface)
    {
      cairo_surface_destroy (impl->cairo_surface);
      impl->cairo_surface = NULL;
    }
}

static cairo_surface_t *
gdk_mir_window_impl_ref_cairo_surface (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_ref_cairo_surface window=%p\n", window);
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);
  MirGraphicsRegion region;
  cairo_format_t pixel_format = CAIRO_FORMAT_INVALID;
  cairo_surface_t *cairo_surface;
  cairo_t *c;

  if (impl->cairo_surface)
    {
      cairo_surface_reference (impl->cairo_surface);
      return impl->cairo_surface;
    }

  /* Transient windows get rendered into a buffer and copied onto their parent */
  if (impl->transient_for)
    {
      cairo_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, window->width, window->height);
    }
  else
    {
      ensure_surface (window);

      mir_surface_get_graphics_region (impl->surface, &region);

      // FIXME: Should calculate this once
      switch (region.pixel_format)
        {
        case mir_pixel_format_argb_8888:
          pixel_format = CAIRO_FORMAT_ARGB32;
          break;
        default:
        case mir_pixel_format_abgr_8888:
        case mir_pixel_format_xbgr_8888:
        case mir_pixel_format_xrgb_8888:
        case mir_pixel_format_bgr_888:
          // uh-oh...
          g_printerr ("Unsupported pixel format %d\n", region.pixel_format);
          break;
        }

      cairo_surface = cairo_image_surface_create_for_data ((unsigned char *) region.vaddr,
                                                           pixel_format,
                                                           region.width,
                                                           region.height,
                                                           region.stride);
    }

  impl->cairo_surface = cairo_surface_reference (cairo_surface);

  /* Draw background */
  c = cairo_create (impl->cairo_surface);
  if (impl->background)
    cairo_set_source (c, impl->background);
  else
    cairo_set_source_rgb (c, 1.0, 0.0, 0.0);
  cairo_paint (c);
  cairo_destroy (c);

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
  GList *link;

  for (link = impl->transient_children; link; link = link->next)
    {
      GdkWindow *window = link->data;
      gdk_window_destroy (window);
    }

  if (impl->background)
    cairo_pattern_destroy (impl->background);
  if (impl->surface)
    mir_surface_release_sync (impl->surface);
  if (impl->cairo_surface)
    cairo_surface_destroy (impl->cairo_surface);
  g_list_free (impl->transient_children);

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

  /* Make sure there's a surface to see */
  ensure_surface (window);

  /* Make sure something is rendered and then show first frame */
  s = gdk_mir_window_impl_ref_cairo_surface (window);
  send_buffer (window);
  cairo_surface_destroy (s);
}

static void
gdk_mir_window_impl_hide (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_hide window=%p\n", window);
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  impl->visible = FALSE;
  ensure_no_surface (window);

  if (impl->transient_for)
    redraw_transient (window);
}

static void
gdk_mir_window_impl_withdraw (GdkWindow *window)
{
  //g_printerr ("gdk_mir_window_impl_withdraw window=%p\n", window);
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  impl->visible = FALSE;
  ensure_no_surface (window);

  if (impl->transient_for)
    redraw_transient (window);
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
  g_printerr ("gdk_mir_window_impl_move_resize");
  g_printerr (" window=%p", window);
  if (with_move)
    g_printerr (" location=%d,%d", x, y);
  if (width > 0)
    g_printerr (" size=%dx%dpx", width, height);
  g_printerr ("\n");
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  /* Redraw parent where we moved from */
  if (impl->transient_for)
    redraw_transient (window);

  /* Transient windows can move wherever they want */
  if (with_move)
    {
      if (impl->transient_for)
        {
          window->x = x;
          window->y = y;
        }
      else
        {
          impl->transient_x = x;
          impl->transient_y = y;
        }
    }

  /* If resize requested then rebuild surface */
  if (width >= 0)
  {
    /* We accept any resize */
    window->width = width;
    window->height = height;

    if (impl->surface)
      {
        ensure_no_surface (window);
        ensure_surface (window);
      }
  }

  /* Redraw parent where we moved to */
  if (impl->transient_for)
    redraw_transient (window);
}

static void
gdk_mir_window_impl_set_background (GdkWindow       *window,
                                    cairo_pattern_t *pattern)
{
  //g_printerr ("gdk_mir_window_impl_set_background window=%p\n", window);
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  if (impl->background)
    cairo_pattern_destroy (impl->background);
  impl->background = cairo_pattern_reference (pattern);
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
  g_printerr ("gdk_mir_window_impl_reparent window=%p new-parent=%p\n", window, new_parent);
  return FALSE;
}

static void
gdk_mir_window_impl_set_device_cursor (GdkWindow *window,
                                       GdkDevice *device,
                                       GdkCursor *cursor)
{
  //g_printerr ("gdk_mir_window_impl_set_device_cursor window=%p\n", window);
  /* We don't support cursors yet... */
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

static gint
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

  return 1;
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
gdk_mir_window_impl_begin_paint_region (GdkWindow            *window,
                                        const cairo_region_t *region)
{
  //g_printerr ("gdk_mir_window_impl_begin_paint_region window=%p\n", window);
  /* Indicate we are ready to be drawn onto directly? */
  return FALSE;
}

static void
gdk_mir_window_impl_end_paint (GdkWindow *window)
{
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  //g_printerr ("gdk_mir_window_impl_end_paint window=%p\n", window);
  if (impl->visible)
    send_buffer (window);
}

static cairo_region_t *
gdk_mir_window_impl_get_shape (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_get_shape window=%p\n", window);
  return NULL;
}

static cairo_region_t *
gdk_mir_window_impl_get_input_shape (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_get_input_shape window=%p\n", window);
  return NULL;
}

static void
gdk_mir_window_impl_shape_combine_region (GdkWindow            *window,
                                          const cairo_region_t *shape_region,
                                          gint                  offset_x,
                                          gint                  offset_y)
{
  g_printerr ("gdk_mir_window_impl_shape_combine_region window=%p\n", window);
}

static void
gdk_mir_window_impl_input_shape_combine_region (GdkWindow            *window,
                                                const cairo_region_t *shape_region,
                                                gint                  offset_x,
                                                gint                  offset_y)
{
  g_printerr ("gdk_mir_window_impl_input_shape_combine_region window=%p\n", window);
}

static gboolean
gdk_mir_window_impl_set_static_gravities (GdkWindow *window,
                                          gboolean   use_static)
{
  g_printerr ("gdk_mir_window_impl_set_static_gravities window=%p\n", window);
  return FALSE;
}

static gboolean
gdk_mir_window_impl_queue_antiexpose (GdkWindow      *window,
                                      cairo_region_t *area)
{
  //g_printerr ("gdk_mir_window_impl_queue_antiexpose window=%p\n", window);
  // FIXME: ?
  return FALSE;
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

  if (impl->transient_for)
    {
      /* Redraw parent */
      redraw_transient (window);

      /* Remove from transient list */
      GdkMirWindowImpl *parent_impl = GDK_MIR_WINDOW_IMPL (impl->transient_for->impl);
      parent_impl->transient_children = g_list_remove (parent_impl->transient_children, window);
    }
}

static void
gdk_mir_window_impl_destroy_foreign (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_destroy_foreign window=%p\n", window);
}

static cairo_surface_t *
gdk_mir_window_impl_resize_cairo_surface (GdkWindow       *window,
                                          cairo_surface_t *surface,
                                          gint             width,
                                          gint             height)
{
  g_printerr ("gdk_mir_window_impl_resize_cairo_surface window=%p\n", window);
  return surface;
}

static void
gdk_mir_window_impl_focus (GdkWindow *window,
                      guint32    timestamp)
{
  g_printerr ("gdk_mir_window_impl_focus window=%p\n", window);
}

static void
gdk_mir_window_impl_set_type_hint (GdkWindow         *window,
                                   GdkWindowTypeHint  hint)
{
  //g_printerr ("gdk_mir_window_impl_set_type_hint window=%p\n", window);
  // FIXME: ?
}

static GdkWindowTypeHint
gdk_mir_window_impl_get_type_hint (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_get_type_hint window=%p\n", window);
  return GDK_WINDOW_TYPE_HINT_NORMAL;
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
  g_printerr ("gdk_mir_window_impl_set_skip_taskbar_hint window=%p\n", window);
}

static void
gdk_mir_window_impl_set_skip_pager_hint (GdkWindow *window,
                                         gboolean   skips_pager)
{
  g_printerr ("gdk_mir_window_impl_set_skip_pager_hint window=%p\n", window);
}

static void
gdk_mir_window_impl_set_urgency_hint (GdkWindow *window,
                                      gboolean   urgent)
{
  g_printerr ("gdk_mir_window_impl_set_urgency_hint window=%p\n", window);
}

static void
gdk_mir_window_impl_set_geometry_hints (GdkWindow         *window,
                                        const GdkGeometry *geometry,
                                        GdkWindowHints     geom_mask)
{
  //g_printerr ("gdk_mir_window_impl_set_geometry_hints window=%p\n", window);
  //FIXME: ?
}

static void
gdk_mir_window_impl_set_title (GdkWindow   *window,
                               const gchar *title)
{
  g_printerr ("gdk_mir_window_impl_set_title window=%p\n", window);
}

static void
gdk_mir_window_impl_set_role (GdkWindow   *window,
                              const gchar *role)
{
  g_printerr ("gdk_mir_window_impl_set_role window=%p\n", window);
}

static void
gdk_mir_window_impl_set_startup_id (GdkWindow   *window,
                                    const gchar *startup_id)
{
  g_printerr ("gdk_mir_window_impl_set_startup_id window=%p\n", window);
}

static void
gdk_mir_window_impl_set_transient_for (GdkWindow *window,
                                       GdkWindow *parent)
{
  g_printerr ("gdk_mir_window_impl_set_transient_for window=%p\n", window);
  GdkMirWindowImpl *impl = GDK_MIR_WINDOW_IMPL (window->impl);

  if (impl->transient_for == parent)
    return;

  g_return_if_fail (impl->transient_for == NULL);

  /* Link this window to the parent */
  impl->transient_for = parent;
  if (parent)
    {
      GdkMirWindowImpl *parent_impl = GDK_MIR_WINDOW_IMPL (parent->impl);
      parent_impl->transient_children = g_list_append (parent_impl->transient_children, window);

      /* Move to where the client requested */
      window->x = impl->transient_x;
      window->y = impl->transient_y;

      /* Redraw onto parent */
      redraw_transient (window);
    }

  /* Remove surface if we had made one before this was set */
  ensure_no_surface (window);
}

static void
gdk_mir_window_impl_get_root_origin (GdkWindow *window,
                                     gint      *x,
                                     gint      *y)
{
  g_printerr ("gdk_mir_window_impl_get_root_origin window=%p\n", window);
}

static void
gdk_mir_window_impl_get_frame_extents (GdkWindow    *window,
                                       GdkRectangle *rect)
{
  g_printerr ("gdk_mir_window_impl_get_frame_extents window=%p\n", window);
}

static void
gdk_mir_window_impl_set_override_redirect (GdkWindow *window,
                                           gboolean   override_redirect)
{
  g_printerr ("gdk_mir_window_impl_set_override_redirect window=%p\n", window);
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
  g_printerr ("gdk_mir_window_impl_set_icon_name window=%p\n", window);
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
  g_printerr ("gdk_mir_window_impl_get_group window=%p\n", window);
  return NULL;
}

static void
gdk_mir_window_impl_set_group (GdkWindow *window,
                               GdkWindow *leader)
{
  g_printerr ("gdk_mir_window_impl_set_group window=%p\n", window);
}

static void
gdk_mir_window_impl_set_decorations (GdkWindow       *window,
                                     GdkWMDecoration  decorations)
{
  g_printerr ("gdk_mir_window_impl_set_decorations window=%p decorations=%d\n", window, decorations);
}

static gboolean
gdk_mir_window_impl_get_decorations (GdkWindow       *window,
                                     GdkWMDecoration *decorations)
{
  g_printerr ("gdk_mir_window_impl_get_decorations window=%p\n", window);
  return FALSE;
}

static void
gdk_mir_window_impl_set_functions (GdkWindow     *window,
                                   GdkWMFunction  functions)
{
  g_printerr ("gdk_mir_window_impl_set_functions window=%p\n", window);
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
  g_printerr ("gdk_mir_window_impl_begin_resize_drag window=%p\n", window);
}

static void
gdk_mir_window_impl_begin_move_drag (GdkWindow *window,
                                     GdkDevice *device,
                                     gint       button,
                                     gint       root_x,
                                     gint       root_y,
                                     guint32    timestamp)
{
  g_printerr ("gdk_mir_window_impl_begin_move_drag window=%p\n", window);
}

static void
gdk_mir_window_impl_enable_synchronized_configure (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_enable_synchronized_configure window=%p\n", window);
}

static void
gdk_mir_window_impl_configure_finished (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_configure_finished window=%p\n", window);
}

static void
gdk_mir_window_impl_set_opacity (GdkWindow *window,
                                 gdouble    opacity)
{
  //g_printerr ("gdk_mir_window_impl_set_opacity window=%p\n", window);
  // FIXME
}

static void
gdk_mir_window_impl_set_composited (GdkWindow *window,
                                    gboolean   composited)
{
  g_printerr ("gdk_mir_window_impl_set_composited window=%p\n", window);
}

static void
gdk_mir_window_impl_destroy_notify (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_destroy_notify window=%p\n", window);
}

static GdkDragProtocol
gdk_mir_window_impl_get_drag_protocol (GdkWindow *window,
                                       GdkWindow **target)
{
  g_printerr ("gdk_mir_window_impl_get_drag_protocol window=%p\n", window);
  return 0;
}

static void
gdk_mir_window_impl_register_dnd (GdkWindow *window)
{
  g_printerr ("gdk_mir_window_impl_register_dnd window=%p\n", window);
}

static GdkDragContext *
gdk_mir_window_impl_drag_begin (GdkWindow *window,
                                GdkDevice *device,
                                GList     *targets)
{
  g_printerr ("gdk_mir_window_impl_drag_begin window=%p\n", window);
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
  g_printerr ("gdk_mir_window_impl_sync_rendering window=%p\n", window);
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
  g_printerr ("gdk_mir_window_impl_simulate_key window=%p\n", window);
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
  g_printerr ("gdk_mir_window_impl_simulate_button window=%p\n", window);
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
  g_printerr ("gdk_mir_window_impl_get_property window=%p\n", window);
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
  g_printerr ("gdk_mir_window_impl_change_property window=%p\n", window);
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
  /* Don't support monitor scaling */
  return 1;
}

static void
gdk_mir_window_impl_set_opaque_region (GdkWindow      *window,
                                       cairo_region_t *region)
{
  //g_printerr ("gdk_mir_window_impl_set_opaque_region window=%p\n", window);
  /* FIXME: An optimisation to tell the compositor which regions of the window are fully transparent */
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
  impl_class->set_background = gdk_mir_window_impl_set_background;
  impl_class->get_events = gdk_mir_window_impl_get_events;
  impl_class->set_events = gdk_mir_window_impl_set_events;
  impl_class->reparent = gdk_mir_window_impl_reparent;
  impl_class->set_device_cursor = gdk_mir_window_impl_set_device_cursor;
  impl_class->get_geometry = gdk_mir_window_impl_get_geometry;
  impl_class->get_root_coords = gdk_mir_window_impl_get_root_coords;
  impl_class->get_device_state = gdk_mir_window_impl_get_device_state;
  impl_class->begin_paint_region = gdk_mir_window_impl_begin_paint_region;
  impl_class->end_paint = gdk_mir_window_impl_end_paint;
  impl_class->get_shape = gdk_mir_window_impl_get_shape;
  impl_class->get_input_shape = gdk_mir_window_impl_get_input_shape;
  impl_class->shape_combine_region = gdk_mir_window_impl_shape_combine_region;
  impl_class->input_shape_combine_region = gdk_mir_window_impl_input_shape_combine_region;
  impl_class->set_static_gravities = gdk_mir_window_impl_set_static_gravities;
  impl_class->queue_antiexpose = gdk_mir_window_impl_queue_antiexpose;
  impl_class->destroy = gdk_mir_window_impl_destroy;
  impl_class->destroy_foreign = gdk_mir_window_impl_destroy_foreign;
  impl_class->resize_cairo_surface = gdk_mir_window_impl_resize_cairo_surface;
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
  impl_class->get_root_origin = gdk_mir_window_impl_get_root_origin;
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
  impl_class->set_composited = gdk_mir_window_impl_set_composited;
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
}
