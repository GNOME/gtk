/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-2007 Peter Mattis, Spencer Kimball,
 * Josh MacDonald, Ryan Lortie
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <cairo-gobject.h>

#include "gdksurface.h"

#include "gdkeventsprivate.h"
#include "gdkrectangle.h"
#include "gdkinternals.h"
#include "gdkintl.h"
#include "gdkdisplayprivate.h"
#include "gdkdeviceprivate.h"
#include "gdkframeclockidleprivate.h"
#include "gdkmarshalers.h"
#include "gdkglcontextprivate.h"
#include "gdk-private.h"

#include <math.h>

#include <epoxy/gl.h>

/* for the use of round() */
#include "fallback-c89.c"

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#endif

/**
 * SECTION:gdksurface
 * @Short_description: Onscreen display areas in the target window system
 * @Title: Surfaces
 *
 * A #GdkSurface is a (usually) rectangular region on the screen.
 * It’s a low-level object, used to implement high-level objects such as
 * #GtkWindow on the GTK level.
 */

/**
 * GdkSurface:
 *
 * The GdkSurface struct contains only private fields and
 * should not be accessed directly.
 */

enum {
  MOVED_TO_RECT,
  SIZE_CHANGED,
  RENDER,
  EVENT,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_SURFACE_TYPE,
  PROP_CURSOR,
  PROP_DISPLAY,
  PROP_PARENT,
  PROP_FRAME_CLOCK,
  PROP_STATE,
  PROP_MAPPED,
  PROP_AUTOHIDE,
  LAST_PROP
};

/* Global info */

static void gdk_surface_finalize     (GObject      *object);

static void gdk_surface_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec);
static void gdk_surface_get_property (GObject      *object,
                                      guint         prop_id,
                                      GValue       *value,
                                      GParamSpec   *pspec);

static void update_cursor               (GdkDisplay *display,
                                         GdkDevice  *device);

static void gdk_surface_set_frame_clock (GdkSurface      *surface,
                                         GdkFrameClock  *clock);

static void gdk_surface_move_resize_internal (GdkSurface *surface,
                                              gboolean   with_move,
                                              gint       x,
                                              gint       y,
                                              gint       width,
                                              gint       height);


static guint signals[LAST_SIGNAL] = { 0 };
static GParamSpec *properties[LAST_PROP] = { NULL, };

G_DEFINE_ABSTRACT_TYPE (GdkSurface, gdk_surface, G_TYPE_OBJECT)

static gboolean
gdk_surface_real_beep (GdkSurface *surface)
{
  return FALSE;
}

static GdkDisplay *
get_display_for_surface (GdkSurface *primary,
                         GdkSurface *secondary)
{
  GdkDisplay *display = primary->display;

  if (display)
    return display;

  display = secondary->display;

  if (display)
    return display;

  g_warning ("no display for surface, using default");
  return gdk_display_get_default ();
}

static GdkMonitor *
get_monitor_for_rect (GdkDisplay         *display,
                      const GdkRectangle *rect)
{
  gint biggest_area = G_MININT;
  GdkMonitor *best_monitor = NULL;
  GdkMonitor *monitor;
  GdkRectangle workarea;
  GdkRectangle intersection;
  gint i;

  for (i = 0; i < gdk_display_get_n_monitors (display); i++)
    {
      monitor = gdk_display_get_monitor (display, i);
      gdk_monitor_get_workarea (monitor, &workarea);

      if (gdk_rectangle_intersect (&workarea, rect, &intersection))
        {
          if (intersection.width * intersection.height > biggest_area)
            {
              biggest_area = intersection.width * intersection.height;
              best_monitor = monitor;
            }
        }
    }

  return best_monitor;
}

static gint
get_anchor_x_sign (GdkGravity anchor)
{
  switch (anchor)
    {
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_WEST:
    case GDK_GRAVITY_SOUTH_WEST:
      return -1;

    default:
    case GDK_GRAVITY_NORTH:
    case GDK_GRAVITY_CENTER:
    case GDK_GRAVITY_SOUTH:
      return 0;

    case GDK_GRAVITY_NORTH_EAST:
    case GDK_GRAVITY_EAST:
    case GDK_GRAVITY_SOUTH_EAST:
      return 1;
    }
}

static gint
get_anchor_y_sign (GdkGravity anchor)
{
  switch (anchor)
    {
    case GDK_GRAVITY_STATIC:
    case GDK_GRAVITY_NORTH_WEST:
    case GDK_GRAVITY_NORTH:
    case GDK_GRAVITY_NORTH_EAST:
      return -1;

    default:
    case GDK_GRAVITY_WEST:
    case GDK_GRAVITY_CENTER:
    case GDK_GRAVITY_EAST:
      return 0;

    case GDK_GRAVITY_SOUTH_WEST:
    case GDK_GRAVITY_SOUTH:
    case GDK_GRAVITY_SOUTH_EAST:
      return 1;
    }
}

static gint
maybe_flip_position (gint      bounds_pos,
                     gint      bounds_size,
                     gint      rect_pos,
                     gint      rect_size,
                     gint      surface_size,
                     gint      rect_sign,
                     gint      surface_sign,
                     gint      offset,
                     gboolean  flip,
                     gboolean *flipped)
{
  gint primary;
  gint secondary;

  *flipped = FALSE;
  primary = rect_pos + (1 + rect_sign) * rect_size / 2 + offset - (1 + surface_sign) * surface_size / 2;

  if (!flip || (primary >= bounds_pos && primary + surface_size <= bounds_pos + bounds_size))
    return primary;

  *flipped = TRUE;
  secondary = rect_pos + (1 - rect_sign) * rect_size / 2 - offset - (1 - surface_sign) * surface_size / 2;

  if (secondary >= bounds_pos && secondary + surface_size <= bounds_pos + bounds_size)
    return secondary;

  *flipped = FALSE;
  return primary;
}

void
gdk_surface_move_to_rect_helper (GdkSurface            *surface,
                                 const GdkRectangle    *rect,
                                 GdkGravity             rect_anchor,
                                 GdkGravity             surface_anchor,
                                 GdkAnchorHints         anchor_hints,
                                 gint                   rect_anchor_dx,
                                 gint                   rect_anchor_dy,
                                 GdkSurfaceMovedToRect  moved_to_rect)
{
  GdkSurface *toplevel;
  GdkDisplay *display;
  GdkMonitor *monitor;
  GdkRectangle bounds;
  GdkRectangle root_rect = *rect;
  GdkRectangle flipped_rect;
  GdkRectangle final_rect;
  gboolean flipped_x;
  gboolean flipped_y;
  int x, y;

  /* This implementation only works for backends that
   * can provide root coordinates via get_root_coords.
   * Other backends need to implement move_to_rect.
   */
  if (surface->surface_type == GDK_SURFACE_POPUP)
    toplevel = surface->parent;
  else
    toplevel = surface->transient_for;

  gdk_surface_get_root_coords (toplevel,
                               root_rect.x,
                               root_rect.y,
                               &root_rect.x,
                               &root_rect.y);

  display = get_display_for_surface (surface, surface->transient_for);
  monitor = get_monitor_for_rect (display, &root_rect);
  gdk_monitor_get_workarea (monitor, &bounds);

  flipped_rect.width = surface->width - surface->shadow_left - surface->shadow_right;
  flipped_rect.height = surface->height - surface->shadow_top - surface->shadow_bottom;
  flipped_rect.x = maybe_flip_position (bounds.x,
                                        bounds.width,
                                        root_rect.x,
                                        root_rect.width,
                                        flipped_rect.width,
                                        get_anchor_x_sign (rect_anchor),
                                        get_anchor_x_sign (surface_anchor),
                                        rect_anchor_dx,
                                        anchor_hints & GDK_ANCHOR_FLIP_X,
                                        &flipped_x);
  flipped_rect.y = maybe_flip_position (bounds.y,
                                        bounds.height,
                                        root_rect.y,
                                        root_rect.height,
                                        flipped_rect.height,
                                        get_anchor_y_sign (rect_anchor),
                                        get_anchor_y_sign (surface_anchor),
                                        rect_anchor_dy,
                                        anchor_hints & GDK_ANCHOR_FLIP_Y,
                                        &flipped_y);

  final_rect = flipped_rect;

  if (anchor_hints & GDK_ANCHOR_SLIDE_X)
    {
      if (final_rect.x + final_rect.width > bounds.x + bounds.width)
        final_rect.x = bounds.x + bounds.width - final_rect.width;

      if (final_rect.x < bounds.x)
        final_rect.x = bounds.x;
    }

  if (anchor_hints & GDK_ANCHOR_SLIDE_Y)
    {
      if (final_rect.y + final_rect.height > bounds.y + bounds.height)
        final_rect.y = bounds.y + bounds.height - final_rect.height;

      if (final_rect.y < bounds.y)
        final_rect.y = bounds.y;
    }

  if (anchor_hints & GDK_ANCHOR_RESIZE_X)
    {
      if (final_rect.x < bounds.x)
        {
          final_rect.width -= bounds.x - final_rect.x;
          final_rect.x = bounds.x;
        }

      if (final_rect.x + final_rect.width > bounds.x + bounds.width)
        final_rect.width = bounds.x + bounds.width - final_rect.x;
    }

  if (anchor_hints & GDK_ANCHOR_RESIZE_Y)
    {
      if (final_rect.y < bounds.y)
        {
          final_rect.height -= bounds.y - final_rect.y;
          final_rect.y = bounds.y;
        }

      if (final_rect.y + final_rect.height > bounds.y + bounds.height)
        final_rect.height = bounds.y + bounds.height - final_rect.y;
    }

  flipped_rect.x -= surface->shadow_left;
  flipped_rect.y -= surface->shadow_top;
  flipped_rect.width += surface->shadow_left + surface->shadow_right;
  flipped_rect.height += surface->shadow_top + surface->shadow_bottom;

  final_rect.x -= surface->shadow_left;
  final_rect.y -= surface->shadow_top;
  final_rect.width += surface->shadow_left + surface->shadow_right;
  final_rect.height += surface->shadow_top + surface->shadow_bottom;

  gdk_surface_get_origin (toplevel, &x, &y);
  final_rect.x -= x;
  final_rect.y -= y;
  flipped_rect.x -= x;
  flipped_rect.y -= y;

  moved_to_rect (surface, final_rect);

  g_signal_emit_by_name (surface,
                         "moved-to-rect",
                         &flipped_rect,
                         &final_rect,
                         flipped_x,
                         flipped_y);
}

static void
gdk_surface_init (GdkSurface *surface)
{
  /* 0-initialization is good for all other fields. */

  surface->surface_type = GDK_SURFACE_TOPLEVEL;

  surface->state = GDK_SURFACE_STATE_WITHDRAWN;
  surface->fullscreen_mode = GDK_FULLSCREEN_ON_CURRENT_MONITOR;
  surface->width = 1;
  surface->height = 1;

  surface->accept_focus = TRUE;
  surface->focus_on_map = TRUE;
  surface->alpha = 255;

  surface->device_cursor = g_hash_table_new_full (NULL, NULL,
                                                 NULL, g_object_unref);
}

static void
gdk_surface_class_init (GdkSurfaceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_surface_finalize;
  object_class->set_property = gdk_surface_set_property;
  object_class->get_property = gdk_surface_get_property;

  klass->beep = gdk_surface_real_beep;

  /**
   * GdkSurface:cursor:
   *
   * The mouse pointer for a #GdkSurface. See gdk_surface_set_cursor() and
   * gdk_surface_get_cursor() for details.
   */
  properties[PROP_CURSOR] =
      g_param_spec_object ("cursor",
                           P_("Cursor"),
                           P_("Cursor"),
                           GDK_TYPE_CURSOR,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GdkSurface:display:
   *
   * The #GdkDisplay connection of the surface. See gdk_surface_get_display()
   * for details.
   */
  properties[PROP_DISPLAY] =
      g_param_spec_object ("display",
                           P_("Display"),
                           P_("Display"),
                           GDK_TYPE_DISPLAY,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_PARENT] =
      g_param_spec_object ("parent",
                           P_("Parent"),
                           P_("Parent surface"),
                           GDK_TYPE_SURFACE,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_FRAME_CLOCK] =
      g_param_spec_object ("frame-clock",
                           P_("Frame Clock"),
                           P_("Frame Clock"),
                           GDK_TYPE_FRAME_CLOCK,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_STATE] =
      g_param_spec_flags ("state",
                          P_("State"),
                          P_("State"),
                          GDK_TYPE_SURFACE_STATE, GDK_SURFACE_STATE_WITHDRAWN,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_MAPPED] =
      g_param_spec_boolean ("mapped",
                            P_("Mapped"),
                            P_("Mapped"),
                            FALSE,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  properties[PROP_AUTOHIDE] =
      g_param_spec_boolean ("autohide",
                            P_("Autohide"),
                            P_("Whether to dismiss the surface on outside clicks"),
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_SURFACE_TYPE] =
      g_param_spec_enum ("surface-type",
                          P_("Surface type"),
                          P_("Surface type"),
                          GDK_TYPE_SURFACE_TYPE, GDK_SURFACE_TOPLEVEL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  /**
   * GdkSurface::moved-to-rect:
   * @surface: the #GdkSurface that moved
   * @flipped_rect: (nullable): the position of @surface after any possible
   *                flipping or %NULL if the backend can't obtain it
   * @final_rect: (nullable): the final position of @surface or %NULL if the
   *              backend can't obtain it
   * @flipped_x: %TRUE if the anchors were flipped horizontally
   * @flipped_y: %TRUE if the anchors were flipped vertically
   *
   * Emitted when the position of @surface is finalized after being moved to a
   * destination rectangle.
   *
   * @surface might be flipped over the destination rectangle in order to keep
   * it on-screen, in which case @flipped_x and @flipped_y will be set to %TRUE
   * accordingly.
   *
   * @flipped_rect is the ideal position of @surface after any possible
   * flipping, but before any possible sliding. @final_rect is @flipped_rect,
   * but possibly translated in the case that flipping is still ineffective in
   * keeping @surface on-screen.
   * Stability: Private
   */
  signals[MOVED_TO_RECT] =
    g_signal_new (g_intern_static_string ("moved-to-rect"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL,
                  NULL,
                  _gdk_marshal_VOID__POINTER_POINTER_BOOLEAN_BOOLEAN,
                  G_TYPE_NONE,
                  4,
                  G_TYPE_POINTER,
                  G_TYPE_POINTER,
                  G_TYPE_BOOLEAN,
                  G_TYPE_BOOLEAN);

  /**
   * GdkSurface::size-changed:
   * @surface: the #GdkSurface
   * @width: the new width
   * @height: the new height
   *
   * Emitted when the size of @surface is changed.
   */
  signals[SIZE_CHANGED] =
    g_signal_new (g_intern_static_string ("size-changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  2,
                  G_TYPE_INT,
                  G_TYPE_INT);

  /**
   * GdkSurface::render:
   * @surface: the #GdkSurface
   * @region: the region that needs to be redrawn
   *
   * Emitted when part of the surface needs to be redrawn.
   *
   * Returns: %TRUE to indicate that the signal has been handled
   */ 
  signals[RENDER] =
    g_signal_new (g_intern_static_string ("render"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_true_handled,
                  NULL,
                  _gdk_marshal_BOOLEAN__BOXED,
                  G_TYPE_BOOLEAN,
                  1,
                  CAIRO_GOBJECT_TYPE_REGION);
  g_signal_set_va_marshaller (signals[RENDER],
                              G_OBJECT_CLASS_TYPE (object_class),
                              _gdk_marshal_BOOLEAN__BOXEDv);

  /**
   * GdkSurface::event:
   * @surface: the #GdkSurface
   * @event: an input event
   *
   * Emitted when GDK receives an input event for @surface.
   *
   * Returns: %TRUE to indicate that the event has been handled
   */ 
  signals[EVENT] =
    g_signal_new (g_intern_static_string ("event"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_true_handled,
                  NULL,
                  _gdk_marshal_BOOLEAN__OBJECT,
                  G_TYPE_BOOLEAN,
                  1,
                  GDK_TYPE_EVENT);
  g_signal_set_va_marshaller (signals[EVENT],
                              G_OBJECT_CLASS_TYPE (object_class),
                              _gdk_marshal_BOOLEAN__OBJECTv);
}

static void
seat_removed_cb (GdkDisplay *display,
                 GdkSeat    *seat,
                 GdkSurface  *surface)
{
  GdkDevice *device = gdk_seat_get_pointer (seat);

  surface->devices_inside = g_list_remove (surface->devices_inside, device);
  g_hash_table_remove (surface->device_cursor, device);
}

static void
gdk_surface_finalize (GObject *object)
{
  GdkSurface *surface = GDK_SURFACE (object);

  g_signal_handlers_disconnect_by_func (surface->display,
                                        seat_removed_cb, surface);

  if (!GDK_SURFACE_DESTROYED (surface))
    {
      g_warning ("losing last reference to undestroyed surface");
      _gdk_surface_destroy (surface, FALSE);
    }

  if (surface->input_shape)
    cairo_region_destroy (surface->input_shape);

  if (surface->cursor)
    g_object_unref (surface->cursor);

  if (surface->device_cursor)
    g_hash_table_destroy (surface->device_cursor);

  if (surface->devices_inside)
    g_list_free (surface->devices_inside);

  g_clear_object (&surface->display);

  if (surface->opaque_region)
    cairo_region_destroy (surface->opaque_region);

  if (surface->parent)
    surface->parent->children = g_list_remove (surface->parent->children, surface);

  G_OBJECT_CLASS (gdk_surface_parent_class)->finalize (object);
}

static void
gdk_surface_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case PROP_CURSOR:
      gdk_surface_set_cursor (surface, g_value_get_object (value));
      break;

    case PROP_DISPLAY:
      surface->display = g_value_dup_object (value);
      g_assert (surface->display != NULL);
      g_signal_connect (surface->display, "seat-removed",
                        G_CALLBACK (seat_removed_cb), surface);
      break;

    case PROP_PARENT:
      surface->parent = g_value_dup_object (value);
      if (surface->parent != NULL)
        surface->parent->children = g_list_prepend (surface->parent->children, surface);
      break;

    case PROP_FRAME_CLOCK:
      gdk_surface_set_frame_clock (surface, GDK_FRAME_CLOCK (g_value_get_object (value)));
      break;

    case PROP_AUTOHIDE:
      surface->autohide = g_value_get_boolean (value);
      break;

    case PROP_SURFACE_TYPE:
      surface->surface_type = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_surface_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GdkSurface *surface = GDK_SURFACE (object);

  switch (prop_id)
    {
    case PROP_CURSOR:
      g_value_set_object (value, gdk_surface_get_cursor (surface));
      break;

    case PROP_DISPLAY:
      g_value_set_object (value, surface->display);
      break;

    case PROP_PARENT:
      g_value_set_object (value, surface->parent);
      break;

    case PROP_FRAME_CLOCK:
      g_value_set_object (value, surface->frame_clock);
      break;

    case PROP_STATE:
      g_value_set_flags (value, surface->state);
      break;

    case PROP_MAPPED:
      g_value_set_boolean (value, GDK_SURFACE_IS_MAPPED (surface));
      break;

    case PROP_AUTOHIDE:
      g_value_set_boolean (value, surface->autohide);
      break;

    case PROP_SURFACE_TYPE:
      g_value_set_enum (value, surface->surface_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
_gdk_surface_update_size (GdkSurface *surface)
{
  GSList *l;

  for (l = surface->draw_contexts; l; l = l->next)
    gdk_draw_context_surface_resized (l->data);
}

static GdkSurface *
gdk_surface_new (GdkDisplay     *display,
                 GdkSurfaceType  surface_type,
                 GdkSurface     *parent,
                 int             x,
                 int             y,
                 int             width,
                 int             height)
{
  return gdk_display_create_surface (display,
                                     surface_type,
                                     parent,
                                     x, y, width, height);
}

/**
 * gdk_surface_new_toplevel: (constructor)
 * @display: the display to create the surface on
 * @width: width of new surface
 * @height: height of new surface
 *
 * Creates a new toplevel surface.
 *
 * Returns: (transfer full): the new #GdkSurface
 **/
GdkSurface *
gdk_surface_new_toplevel (GdkDisplay *display,
                          gint        width,
                          gint        height)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return gdk_surface_new (display, GDK_SURFACE_TOPLEVEL,
                          NULL, 0, 0, width, height);
}

/**
 * gdk_surface_new_temp: (constructor)
 * @display: the display to create the surface on
 * @position: position of the surface on screen
 *
 * Creates a new temporary surface.
 * The surface will bypass surface management.
 *
 * Returns: (transfer full): the new #GdkSurface
 **/
GdkSurface *
gdk_surface_new_temp (GdkDisplay         *display,
                      const GdkRectangle *position)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (position != NULL, NULL);

  return gdk_surface_new (display, GDK_SURFACE_TEMP,
                          NULL,
                          position->x, position->y,
                          position->width, position->height);
}

/**
 * gdk_surface_new_popup: (constructor)
 * @display: the display to create the surface on
 * @parent: the parent surface to attach the surface to
 * @autohide: whether to hide the surface on outside clicks
 *
 * Create a new popup surface.
 *
 * The surface will be attached to @parent and can
 * be positioned relative to it using
 * gdk_surface_move_to_rect().
 *
 * Returns: (transfer full): a new #GdkSurface
 */
GdkSurface *
gdk_surface_new_popup (GdkDisplay *display,
                       GdkSurface *parent,
                       gboolean    autohide)
{
  GdkSurface *surface;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (GDK_IS_SURFACE (parent), NULL);

  surface = gdk_surface_new (display, GDK_SURFACE_POPUP,
                             parent, 0, 0, 100, 100);

  surface->autohide = autohide;

  return surface;
}

/**
 * gdk_surface_get_parent:
 * @surface: a #GtkSurface
 *
 * Returns the parent surface of a surface, or
 * %NULL if the surface does not have a parent.
 *
 * Only popup surfaces have parents.
 *
 * Returns: (transfer none) (nullable): the parent of
 *   @surface, or %NULL
 */
GdkSurface *
gdk_surface_get_parent (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);
  
  return surface->parent;
}

static void
update_pointer_info_foreach (GdkDisplay           *display,
                             GdkDevice            *device,
                             GdkPointerSurfaceInfo *pointer_info,
                             gpointer              user_data)
{
  GdkSurface *surface = user_data;

  if (pointer_info->surface_under_pointer == surface)
    {
      g_object_unref (pointer_info->surface_under_pointer);
      pointer_info->surface_under_pointer = NULL;
    }
}

static void
surface_remove_from_pointer_info (GdkSurface  *surface,
                                  GdkDisplay *display)
{
  _gdk_display_pointer_info_foreach (display,
                                     update_pointer_info_foreach,
                                     surface);
}

/**
 * _gdk_surface_destroy_hierarchy:
 * @surface: a #GdkSurface
 * @recursing_native: If %TRUE, then this is being called because a native parent
 *            was destroyed. This generally means that the call to the
 *            windowing system to destroy the surface can be omitted, since
 *            it will be destroyed as a result of the parent being destroyed.
 *            Unless @foreign_destroy.
 * @foreign_destroy: If %TRUE, the surface or a parent was destroyed by some
 *            external agency. The surface has already been destroyed and no
 *            windowing system calls should be made. (This may never happen
 *            for some windowing systems.)
 *
 * Internal function to destroy a surface. Like gdk_surface_destroy(),
 * but does not drop the reference count created by gdk_surface_new().
 **/
static void
_gdk_surface_destroy_hierarchy (GdkSurface *surface,
                                gboolean   foreign_destroy)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  GDK_SURFACE_GET_CLASS (surface)->destroy (surface, foreign_destroy);

  if (surface->gl_paint_context)
    {
      /* Make sure to destroy if current */
      g_object_run_dispose (G_OBJECT (surface->gl_paint_context));
      g_object_unref (surface->gl_paint_context);
      surface->gl_paint_context = NULL;
    }

  if (surface->frame_clock)
    {
      if (surface->parent == NULL)
        g_object_run_dispose (G_OBJECT (surface->frame_clock));
      gdk_surface_set_frame_clock (surface, NULL);
    }

  _gdk_surface_clear_update_area (surface);

  surface->state |= GDK_SURFACE_STATE_WITHDRAWN;
  surface->destroyed = TRUE;

  surface_remove_from_pointer_info (surface, surface->display);

  g_object_notify_by_pspec (G_OBJECT (surface), properties[PROP_STATE]);
  g_object_notify_by_pspec (G_OBJECT (surface), properties[PROP_MAPPED]);
}

/**
 * _gdk_surface_destroy:
 * @surface: a #GdkSurface
 * @foreign_destroy: If %TRUE, the surface or a parent was destroyed by some
 *            external agency. The surface has already been destroyed and no
 *            windowing system calls should be made. (This may never happen
 *            for some windowing systems.)
 *
 * Internal function to destroy a surface. Like gdk_surface_destroy(),
 * but does not drop the reference count created by gdk_surface_new().
 **/
void
_gdk_surface_destroy (GdkSurface *surface,
                      gboolean   foreign_destroy)
{
  _gdk_surface_destroy_hierarchy (surface, foreign_destroy);
}

/**
 * gdk_surface_destroy:
 * @surface: a #GdkSurface
 *
 * Destroys the window system resources associated with @surface and decrements @surface's
 * reference count. The window system resources for all children of @surface are also
 * destroyed, but the children’s reference counts are not decremented.
 *
 * Note that a surface will not be destroyed automatically when its reference count
 * reaches zero. You must call this function yourself before that happens.
 *
 **/
void
gdk_surface_destroy (GdkSurface *surface)
{
  _gdk_surface_destroy_hierarchy (surface, FALSE);
  g_object_unref (surface);
}

void
gdk_surface_set_widget (GdkSurface *surface,
                        gpointer    widget)
{
  surface->widget = widget;
}

gpointer
gdk_surface_get_widget (GdkSurface *surface)
{
  return surface->widget;
}

/**
 * gdk_surface_get_surface_type:
 * @surface: a #GdkSurface
 *
 * Gets the type of the surface. See #GdkSurfaceType.
 *
 * Returns: type of surface
 **/
GdkSurfaceType
gdk_surface_get_surface_type (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), (GdkSurfaceType) -1);

  return GDK_SURFACE_TYPE (surface);
}

/**
 * gdk_surface_get_display:
 * @surface: a #GdkSurface
 * 
 * Gets the #GdkDisplay associated with a #GdkSurface.
 * 
 * Returns: (transfer none): the #GdkDisplay associated with @surface
 **/
GdkDisplay *
gdk_surface_get_display (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);

  return surface->display;
}
/**
 * gdk_surface_is_destroyed:
 * @surface: a #GdkSurface
 *
 * Check to see if a surface is destroyed..
 *
 * Returns: %TRUE if the surface is destroyed
 **/
gboolean
gdk_surface_is_destroyed (GdkSurface *surface)
{
  return GDK_SURFACE_DESTROYED (surface);
}

/**
 * gdk_surface_get_position:
 * @surface: a #GdkSurface
 * @x: (out): X coordinate of surface
 * @y: (out): Y coordinate of surface
 *
 * Obtains the position of the surface relative to its parent.
 **/
void
gdk_surface_get_position (GdkSurface *surface,
                          int        *x,
                          int        *y)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (surface->parent)
    {
      *x = surface->x;
      *y = surface->y;
    }
  else
    {
      *x = 0;
      *y = 0;
    }
}

/**
 * gdk_surface_is_visible:
 * @surface: a #GdkSurface
 *
 * Checks whether the surface has been mapped (with gdk_surface_show() or
 * gdk_surface_show_unraised()).
 *
 * Returns: %TRUE if the surface is mapped
 **/
gboolean
gdk_surface_is_visible (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), FALSE);

  return GDK_SURFACE_IS_MAPPED (surface);
}

/**
 * gdk_surface_is_viewable:
 * @surface: a #GdkSurface
 *
 * Check if the surface and all ancestors of the surface are
 * mapped. (This is not necessarily "viewable" in the X sense, since
 * we only check as far as we have GDK surface parents, not to the root
 * surface.)
 *
 * Returns: %TRUE if the surface is viewable
 **/
gboolean
gdk_surface_is_viewable (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), FALSE);

  if (surface->destroyed)
    return FALSE;

  return surface->viewable;
}

/**
 * gdk_surface_get_state:
 * @surface: a #GdkSurface
 *
 * Gets the bitwise OR of the currently active surface state flags,
 * from the #GdkSurfaceState enumeration.
 *
 * Returns: surface state bitfield
 **/
GdkSurfaceState
gdk_surface_get_state (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), FALSE);

  return surface->state;
}

GdkGLContext *
gdk_surface_get_shared_data_gl_context (GdkSurface *surface)
{
  static int in_shared_data_creation;
  GdkDisplay *display;
  GdkGLContext *context;

  if (in_shared_data_creation)
    return NULL;

  in_shared_data_creation = 1;

  display = gdk_surface_get_display (surface);
  context = (GdkGLContext *)g_object_get_data (G_OBJECT (display), "gdk-gl-shared-data-context");
  if (context == NULL)
    {
      GError *error = NULL;
      context = GDK_SURFACE_GET_CLASS (surface)->create_gl_context (surface, FALSE, NULL, &error);
      if (context == NULL)
        {
          g_warning ("Failed to create shared context: %s", error->message);
          g_clear_error (&error);
        }

      gdk_gl_context_realize (context, &error);
      if (context == NULL)
        {
          g_warning ("Failed to realize shared context: %s", error->message);
          g_clear_error (&error);
        }


      g_object_set_data (G_OBJECT (display), "gdk-gl-shared-data-context", context);
    }

  in_shared_data_creation = 0;

  return context;
}

GdkGLContext *
gdk_surface_get_paint_gl_context (GdkSurface  *surface,
                                  GError    **error)
{
  GError *internal_error = NULL;

  if (GDK_DISPLAY_DEBUG_CHECK (surface->display, GL_DISABLE))
    {
      g_set_error_literal (error, GDK_GL_ERROR,
                           GDK_GL_ERROR_NOT_AVAILABLE,
                           _("GL support disabled via GDK_DEBUG"));
      return NULL;
    }

  if (surface->gl_paint_context == NULL)
    {
      GdkSurfaceClass *class = GDK_SURFACE_GET_CLASS (surface);

      if (class->create_gl_context == NULL)
        {
          g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                               _("The current backend does not support OpenGL"));
          return NULL;
        }

      surface->gl_paint_context =
        class->create_gl_context (surface, TRUE, NULL, &internal_error);
    }

  if (internal_error != NULL)
    {
      g_propagate_error (error, internal_error);
      g_clear_object (&(surface->gl_paint_context));
      return NULL;
    }

  gdk_gl_context_realize (surface->gl_paint_context, &internal_error);
  if (internal_error != NULL)
    {
      g_propagate_error (error, internal_error);
      g_clear_object (&(surface->gl_paint_context));
      return NULL;
    }

  return surface->gl_paint_context;
}

/**
 * gdk_surface_create_gl_context:
 * @surface: a #GdkSurface
 * @error: return location for an error
 *
 * Creates a new #GdkGLContext matching the
 * framebuffer format to the visual of the #GdkSurface. The context
 * is disconnected from any particular surface or surface.
 *
 * If the creation of the #GdkGLContext failed, @error will be set.
 *
 * Before using the returned #GdkGLContext, you will need to
 * call gdk_gl_context_make_current() or gdk_gl_context_realize().
 *
 * Returns: (transfer full): the newly created #GdkGLContext, or
 * %NULL on error
 **/
GdkGLContext *
gdk_surface_create_gl_context (GdkSurface   *surface,
                               GError      **error)
{
  GdkGLContext *paint_context;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  paint_context = gdk_surface_get_paint_gl_context (surface, error);
  if (paint_context == NULL)
    return NULL;

  return GDK_SURFACE_GET_CLASS (surface)->create_gl_context (surface,
                                                             FALSE,
                                                             paint_context,
                                                             error);
}

/**
 * gdk_surface_create_cairo_context:
 * @surface: a #GdkSurface
 *
 * Creates a new #GdkCairoContext for rendering on @surface.
 *
 * Returns: (transfer full): the newly created #GdkCairoContext
 **/
GdkCairoContext *
gdk_surface_create_cairo_context (GdkSurface *surface)
{
  GdkDisplay *display;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);

  display = surface->display;

  return g_object_new (GDK_DISPLAY_GET_CLASS (display)->cairo_context_type,
                       "surface", surface,
                       NULL);
}

/**
 * gdk_surface_create_vulkan_context:
 * @surface: a #GdkSurface
 * @error: return location for an error
 *
 * Creates a new #GdkVulkanContext for rendering on @surface.
 *
 * If the creation of the #GdkVulkanContext failed, @error will be set.
 *
 * Returns: (transfer full): the newly created #GdkVulkanContext, or
 * %NULL on error
 **/
GdkVulkanContext *
gdk_surface_create_vulkan_context (GdkSurface  *surface,
                                   GError    **error)
{
  GdkDisplay *display;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (GDK_DISPLAY_DEBUG_CHECK (surface->display, VULKAN_DISABLE))
    {
      g_set_error_literal (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_NOT_AVAILABLE,
                           _("Vulkan support disabled via GDK_DEBUG"));
      return NULL;
    }

  display = surface->display;

  if (GDK_DISPLAY_GET_CLASS (display)->vk_extension_name == NULL)
    {
      g_set_error (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_UNSUPPORTED,
                   "The %s backend has no Vulkan support.", G_OBJECT_TYPE_NAME (display));
      return FALSE;
    }

  return g_initable_new (GDK_DISPLAY_GET_CLASS (display)->vk_context_type,
                         NULL,
                         error,
                         "surface", surface,
                         NULL);
}

/* Code for dirty-region queueing
 */
static GSList *update_surfaces = NULL;

static void
gdk_surface_add_update_surface (GdkSurface *surface)
{
  GSList *tmp;

  /*  Check whether "surface" is already in "update_surfaces" list.
   *  It could be added during execution of gtk_widget_destroy() when
   *  setting focus widget to NULL and redrawing old focus widget.
   *  See bug 711552.
   */
  tmp = g_slist_find (update_surfaces, surface);
  if (tmp != NULL)
    return;

  update_surfaces = g_slist_prepend (update_surfaces, g_object_ref (surface));
}

static void
gdk_surface_remove_update_surface (GdkSurface *surface)
{
  GSList *link;

  link = g_slist_find (update_surfaces, surface);
  if (link != NULL)
    {
      update_surfaces = g_slist_delete_link (update_surfaces, link);
      g_object_unref (surface);
    }
}

static gboolean
gdk_surface_is_toplevel_frozen (GdkSurface *surface)
{
  return surface->update_and_descendants_freeze_count > 0;
}

static void
gdk_surface_schedule_update (GdkSurface *surface)
{
  GdkFrameClock *frame_clock;

  g_return_if_fail (surface);

  if (surface->update_freeze_count ||
      gdk_surface_is_toplevel_frozen (surface))
    {
      surface->pending_schedule_update = TRUE;
      return;
    }

  /* If there's no frame clock (a foreign surface), then the invalid
   * region will just stick around unless gdk_surface_process_updates()
   * is called. */
  frame_clock = gdk_surface_get_frame_clock (surface);
  if (frame_clock)
    gdk_frame_clock_request_phase (gdk_surface_get_frame_clock (surface),
                                   GDK_FRAME_CLOCK_PHASE_PAINT);
}

static void
gdk_surface_process_updates_internal (GdkSurface *surface)
{
  /* Ensure the surface lives while updating it */
  g_object_ref (surface);

  surface->in_update = TRUE;

  /* If an update got queued during update processing, we can get a
   * surface in the update queue that has an empty update_area.
   * just ignore it.
   */
  if (surface->update_area)
    {
      g_assert (surface->active_update_area == NULL); /* No reentrancy */

      surface->active_update_area = surface->update_area;
      surface->update_area = NULL;

      if (gdk_surface_is_viewable (surface))
        {
          cairo_region_t *expose_region;
          gboolean handled;

          expose_region = cairo_region_copy (surface->active_update_area);

          g_signal_emit (surface, signals[RENDER], 0, expose_region, &handled);

          cairo_region_destroy (expose_region);
        }

      cairo_region_destroy (surface->active_update_area);
      surface->active_update_area = NULL;
    }

  surface->in_update = FALSE;

  g_object_unref (surface);
}

static void
gdk_surface_paint_on_clock (GdkFrameClock *clock,
                            void          *data)
{
  GdkSurface *surface = GDK_SURFACE (data);

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  g_object_ref (surface);

  if (surface->update_area &&
      !surface->update_freeze_count &&
      !gdk_surface_is_toplevel_frozen (surface) &&

      /* Don't recurse into process_updates_internal, we'll
       * do the update later when idle instead. */
      !surface->in_update)
    {
      gdk_surface_process_updates_internal (surface);
      gdk_surface_remove_update_surface (surface);
    }

  g_object_unref (surface);
}

/**
 * gdk_surface_invalidate_rect:
 * @surface: a #GdkSurface
 * @rect: (allow-none): rectangle to invalidate or %NULL to invalidate the whole
 *      surface
 *
 * A convenience wrapper around gdk_surface_invalidate_region() which
 * invalidates a rectangular region. See
 * gdk_surface_invalidate_region() for details.
 **/
void
gdk_surface_invalidate_rect (GdkSurface        *surface,
                             const GdkRectangle *rect)
{
  GdkRectangle surface_rect;
  cairo_region_t *region;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (!surface->viewable)
    return;

  if (!rect)
    {
      surface_rect.x = 0;
      surface_rect.y = 0;
      surface_rect.width = surface->width;
      surface_rect.height = surface->height;
      rect = &surface_rect;
    }

  region = cairo_region_create_rectangle (rect);
  gdk_surface_invalidate_region (surface, region);
  cairo_region_destroy (region);
}

static void
impl_surface_add_update_area (GdkSurface     *impl_surface,
                              cairo_region_t *region)
{
  if (impl_surface->update_area)
    cairo_region_union (impl_surface->update_area, region);
  else
    {
      gdk_surface_add_update_surface (impl_surface);
      impl_surface->update_area = cairo_region_copy (region);
      gdk_surface_schedule_update (impl_surface);
    }
}

/**
 * gdk_surface_queue_expose:
 * @surface: a #GdkSurface
 *
 * Forces an expose event for @surface to be scheduled.
 *
 * If the invalid area of @surface is empty, an expose event will
 * still be emitted. Its invalid region will be empty.
 *
 * This function is useful for implementations that track invalid
 * regions on their own.
 **/
void
gdk_surface_queue_expose (GdkSurface *surface)
{
  cairo_region_t *region;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  region = cairo_region_create ();
  impl_surface_add_update_area (surface, region);
  cairo_region_destroy (region);
}

/**
 * gdk_surface_invalidate_region:
 * @surface: a #GdkSurface
 * @region: a #cairo_region_t
 *
 * Adds @region to the update area for @surface. The update area is the
 * region that needs to be redrawn, or “dirty region.”
 *
 * GDK will process all updates whenever the frame clock schedules a redraw,
 * so there’s no need to do forces redraws manually, you just need to
 * invalidate regions that you know should be redrawn.
 **/
void
gdk_surface_invalidate_region (GdkSurface          *surface,
                               const cairo_region_t *region)
{
  cairo_region_t *visible_region;
  cairo_rectangle_int_t r;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (!surface->viewable || cairo_region_is_empty (region))
    return;

  r.x = 0;
  r.y = 0;
  r.width = surface->width;
  r.height = surface->height;

  visible_region = cairo_region_copy (region);

  cairo_region_intersect_rectangle (visible_region, &r);
  impl_surface_add_update_area (surface, visible_region);

  cairo_region_destroy (visible_region);
}

/**
 * _gdk_surface_clear_update_area:
 * @surface: a #GdkSurface.
 *
 * Internal function to clear the update area for a surface. This
 * is called when the surface is hidden or destroyed.
 **/
void
_gdk_surface_clear_update_area (GdkSurface *surface)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (surface->update_area)
    {
      gdk_surface_remove_update_surface (surface);

      cairo_region_destroy (surface->update_area);
      surface->update_area = NULL;
    }
}

/**
 * gdk_surface_freeze_updates:
 * @surface: a #GdkSurface
 *
 * Temporarily freezes a surface such that it won’t receive expose
 * events.  The surface will begin receiving expose events again when
 * gdk_surface_thaw_updates() is called. If gdk_surface_freeze_updates()
 * has been called more than once, gdk_surface_thaw_updates() must be called
 * an equal number of times to begin processing exposes.
 **/
void
gdk_surface_freeze_updates (GdkSurface *surface)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  surface->update_freeze_count++;
  if (surface->update_freeze_count == 1)
    _gdk_frame_clock_uninhibit_freeze (surface->frame_clock);
}

/**
 * gdk_surface_thaw_updates:
 * @surface: a #GdkSurface
 *
 * Thaws a surface frozen with gdk_surface_freeze_updates(). Note that this
 * will not necessarily schedule updates if the surface freeze count reaches
 * zero.
 **/
void
gdk_surface_thaw_updates (GdkSurface *surface)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  g_return_if_fail (surface->update_freeze_count > 0);

  if (--surface->update_freeze_count == 0)
    {
      _gdk_frame_clock_inhibit_freeze (surface->frame_clock);

      if (surface->pending_schedule_update)
        {
          surface->pending_schedule_update = FALSE;
          gdk_surface_schedule_update (surface);
        }
    }
}

void
gdk_surface_freeze_toplevel_updates (GdkSurface *surface)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  surface->update_and_descendants_freeze_count++;
  gdk_surface_freeze_updates (surface);
}

void
gdk_surface_thaw_toplevel_updates (GdkSurface *surface)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (surface->update_and_descendants_freeze_count > 0);

  surface->update_and_descendants_freeze_count--;
  gdk_surface_schedule_update (surface);
  gdk_surface_thaw_updates (surface);

}

/**
 * gdk_surface_constrain_size:
 * @geometry: a #GdkGeometry structure
 * @flags: a mask indicating what portions of @geometry are set
 * @width: desired width of surface
 * @height: desired height of the surface
 * @new_width: (out): location to store resulting width
 * @new_height: (out): location to store resulting height
 *
 * Constrains a desired width and height according to a
 * set of geometry hints (such as minimum and maximum size).
 */
void
gdk_surface_constrain_size (GdkGeometry    *geometry,
                            GdkSurfaceHints  flags,
                            gint            width,
                            gint            height,
                            gint           *new_width,
                            gint           *new_height)
{
  /* This routine is partially borrowed from fvwm.
   *
   * Copyright 1993, Robert Nation
   *     You may use this code for any purpose, as long as the original
   *     copyright remains in the source code and all documentation
   *
   * which in turn borrows parts of the algorithm from uwm
   */
  gint min_width = 0;
  gint min_height = 0;
  gint base_width = 0;
  gint base_height = 0;
  gint xinc = 1;
  gint yinc = 1;
  gint max_width = G_MAXINT;
  gint max_height = G_MAXINT;

#define FLOOR(value, base)      ( ((gint) ((value) / (base))) * (base) )

  if ((flags & GDK_HINT_BASE_SIZE) && (flags & GDK_HINT_MIN_SIZE))
    {
      base_width = geometry->base_width;
      base_height = geometry->base_height;
      min_width = geometry->min_width;
      min_height = geometry->min_height;
    }
  else if (flags & GDK_HINT_BASE_SIZE)
    {
      base_width = geometry->base_width;
      base_height = geometry->base_height;
      min_width = geometry->base_width;
      min_height = geometry->base_height;
    }
  else if (flags & GDK_HINT_MIN_SIZE)
    {
      base_width = geometry->min_width;
      base_height = geometry->min_height;
      min_width = geometry->min_width;
      min_height = geometry->min_height;
    }

  if (flags & GDK_HINT_MAX_SIZE)
    {
      max_width = geometry->max_width ;
      max_height = geometry->max_height;
    }

  if (flags & GDK_HINT_RESIZE_INC)
    {
      xinc = MAX (xinc, geometry->width_inc);
      yinc = MAX (yinc, geometry->height_inc);
    }

  /* clamp width and height to min and max values
   */
  width = CLAMP (width, min_width, max_width);
  height = CLAMP (height, min_height, max_height);

  /* shrink to base + N * inc
   */
  width = base_width + FLOOR (width - base_width, xinc);
  height = base_height + FLOOR (height - base_height, yinc);

  /* constrain aspect ratio, according to:
   *
   *                width
   * min_aspect <= -------- <= max_aspect
   *                height
   */

  if (flags & GDK_HINT_ASPECT &&
      geometry->min_aspect > 0 &&
      geometry->max_aspect > 0)
    {
      gint delta;

      if (geometry->min_aspect * height > width)
        {
          delta = FLOOR (height - width / geometry->min_aspect, yinc);
          if (height - delta >= min_height)
            height -= delta;
          else
            {
              delta = FLOOR (height * geometry->min_aspect - width, xinc);
              if (width + delta <= max_width)
                width += delta;
            }
        }

      if (geometry->max_aspect * height < width)
        {
          delta = FLOOR (width - height * geometry->max_aspect, xinc);
          if (width - delta >= min_width)
            width -= delta;
          else
            {
              delta = FLOOR (width / geometry->max_aspect - height, yinc);
              if (height + delta <= max_height)
                height += delta;
            }
        }
    }

#undef FLOOR

  *new_width = width;
  *new_height = height;
}

/**
 * gdk_surface_get_device_position:
 * @surface: a #GdkSurface.
 * @device: pointer #GdkDevice to query to.
 * @x: (out) (allow-none): return location for the X coordinate of @device, or %NULL.
 * @y: (out) (allow-none): return location for the Y coordinate of @device, or %NULL.
 * @mask: (out) (allow-none): return location for the modifier mask, or %NULL.
 *
 * Obtains the current device position in doubles and modifier state.
 * The position is given in coordinates relative to the upper left
 * corner of @surface.
 **/
void
gdk_surface_get_device_position (GdkSurface       *surface,
                                 GdkDevice       *device,
                                 double          *x,
                                 double          *y,
                                 GdkModifierType *mask)
{
  gdouble tmp_x, tmp_y;
  GdkModifierType tmp_mask;

  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD);

  tmp_x = tmp_y = 0;
  tmp_mask = 0;
  GDK_SURFACE_GET_CLASS (surface)->get_device_state (surface,
                                                     device,
                                                     &tmp_x, &tmp_y,
                                                     &tmp_mask);

  if (x)
    *x = tmp_x;
  if (y)
    *y = tmp_y;
  if (mask)
    *mask = tmp_mask;
}

static void
gdk_surface_raise_internal (GdkSurface *surface)
{
  GDK_SURFACE_GET_CLASS (surface)->raise (surface);
}

/* Returns TRUE If the native surface was mapped or unmapped */
static gboolean
set_viewable (GdkSurface *w,
              gboolean val)
{
  if (w->viewable == val)
    return FALSE;

  w->viewable = val;

  return FALSE;
}

gboolean
_gdk_surface_update_viewable (GdkSurface *surface)
{
  return set_viewable (surface, GDK_SURFACE_IS_MAPPED (surface));
}

static void
gdk_surface_show_internal (GdkSurface *surface, gboolean raise)
{
  gboolean was_mapped;
  gboolean did_show;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (surface->destroyed)
    return;

  was_mapped = GDK_SURFACE_IS_MAPPED (surface);

  if (raise)
    gdk_surface_raise_internal (surface);

  if (!was_mapped)
    gdk_synthesize_surface_state (surface, GDK_SURFACE_STATE_WITHDRAWN, 0);

  did_show = _gdk_surface_update_viewable (surface);

  GDK_SURFACE_GET_CLASS (surface)->show (surface, !did_show ? was_mapped : TRUE);

  if (!was_mapped)
    {
      if (gdk_surface_is_viewable (surface))
        gdk_surface_invalidate_rect (surface, NULL);
    }
}

/**
 * gdk_surface_show_unraised:
 * @surface: a #GdkSurface
 *
 * Shows a #GdkSurface onscreen, but does not modify its stacking
 * order. In contrast, gdk_surface_show() will raise the surface
 * to the top of the surface stack.
 *
 * On the X11 platform, in Xlib terms, this function calls
 * XMapWindow() (it also updates some internal GDK state, which means
 * that you can’t really use XMapWindow() directly on a GDK surface).
 */
void
gdk_surface_show_unraised (GdkSurface *surface)
{
  gdk_surface_show_internal (surface, FALSE);
}

/**
 * gdk_surface_raise:
 * @surface: a #GdkSurface
 *
 * Raises @surface to the top of the Z-order (stacking order), so that
 * other surfaces with the same parent surface appear below @surface.
 * This is true whether or not the surfaces are visible.
 *
 * If @surface is a toplevel, the window manager may choose to deny the
 * request to move the surface in the Z-order, gdk_surface_raise() only
 * requests the restack, does not guarantee it.
 */
void
gdk_surface_raise (GdkSurface *surface)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (surface->destroyed)
    return;

  gdk_surface_raise_internal (surface);
}

static void
gdk_surface_lower_internal (GdkSurface *surface)
{
  GDK_SURFACE_GET_CLASS (surface)->lower (surface);
}

/**
 * gdk_surface_lower:
 * @surface: a #GdkSurface
 *
 * Lowers @surface to the bottom of the Z-order (stacking order), so that
 * other surfaces with the same parent surface appear above @surface.
 * This is true whether or not the other surfaces are visible.
 *
 * If @surface is a toplevel, the window manager may choose to deny the
 * request to move the surface in the Z-order, gdk_surface_lower() only
 * requests the restack, does not guarantee it.
 *
 * Note that gdk_surface_show() raises the surface again, so don’t call this
 * function before gdk_surface_show(). (Try gdk_surface_show_unraised().)
 */
void
gdk_surface_lower (GdkSurface *surface)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (surface->destroyed)
    return;

  /* Keep children in (reverse) stacking order */
  gdk_surface_lower_internal (surface);
}

/**
 * gdk_surface_restack:
 * @surface: a #GdkSurface
 * @sibling: (allow-none): a #GdkSurface that is a sibling of @surface, or %NULL
 * @above: a boolean
 *
 * Changes the position of  @surface in the Z-order (stacking order), so that
 * it is above @sibling (if @above is %TRUE) or below @sibling (if @above is
 * %FALSE).
 *
 * If @sibling is %NULL, then this either raises (if @above is %TRUE) or
 * lowers the surface.
 *
 * If @surface is a toplevel, the window manager may choose to deny the
 * request to move the surface in the Z-order, gdk_surface_restack() only
 * requests the restack, does not guarantee it.
 */
void
gdk_surface_restack (GdkSurface     *surface,
                     GdkSurface     *sibling,
                     gboolean       above)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (sibling == NULL || GDK_IS_SURFACE (sibling));

  if (surface->destroyed)
    return;

  if (sibling == NULL)
    {
      if (above)
        gdk_surface_raise (surface);
      else
        gdk_surface_lower (surface);
      return;
    }

  GDK_SURFACE_GET_CLASS (surface)->restack_toplevel (surface, sibling, above);
}

static void
grab_prepare_func (GdkSeat    *seat,
                   GdkSurface *surface,
                   gpointer    data)
{
  gdk_surface_show_internal (surface, TRUE);
}

/**
 * gdk_surface_show:
 * @surface: a #GdkSurface
 *
 * Like gdk_surface_show_unraised(), but also raises the surface to the
 * top of the surface stack (moves the surface to the front of the
 * Z-order).
 *
 * This function maps a surface so it’s visible onscreen. Its opposite
 * is gdk_surface_hide().
 *
 * When implementing a #GtkWidget, you should call this function on the widget's
 * #GdkSurface as part of the “map” method.
 */
void
gdk_surface_show (GdkSurface *surface)
{
  if (surface->autohide)
    {
      gdk_seat_grab (gdk_display_get_default_seat (surface->display),
                     surface,
                     GDK_SEAT_CAPABILITY_ALL,
                     TRUE,
                     NULL, NULL,
                     grab_prepare_func, NULL);
    }
  else
    {
      gdk_surface_show_internal (surface, TRUE);
    }
}

/**
 * gdk_surface_hide:
 * @surface: a #GdkSurface
 *
 * For toplevel surfaces, withdraws them, so they will no longer be
 * known to the window manager; for all surfaces, unmaps them, so
 * they won’t be displayed. Normally done automatically as
 * part of gtk_widget_hide().
 */
void
gdk_surface_hide (GdkSurface *surface)
{
  gboolean was_mapped;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (surface->destroyed)
    return;

  was_mapped = GDK_SURFACE_IS_MAPPED (surface);

  if (GDK_SURFACE_IS_MAPPED (surface))
    gdk_synthesize_surface_state (surface, 0, GDK_SURFACE_STATE_WITHDRAWN);

  if (was_mapped)
    {
      GdkDisplay *display;
      GdkSeat *seat;
      GList *devices, *d;

      /* May need to break grabs on children */
      display = surface->display;
      seat = gdk_display_get_default_seat (display);

      devices = gdk_seat_get_slaves (seat, GDK_SEAT_CAPABILITY_ALL);
      devices = g_list_prepend (devices, gdk_seat_get_keyboard (seat));
      devices = g_list_prepend (devices, gdk_seat_get_pointer (seat));

      for (d = devices; d; d = d->next)
        {
          GdkDevice *device = d->data;

          if (_gdk_display_end_device_grab (display,
                                            device,
                                            _gdk_display_get_next_serial (display),
                                            surface,
                                            TRUE))
            {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
              gdk_device_ungrab (device, GDK_CURRENT_TIME);
G_GNUC_END_IGNORE_DEPRECATIONS
            }
        }

      g_list_free (devices);
    }

  GDK_SURFACE_GET_CLASS (surface)->hide (surface);
}

static void
gdk_surface_move_resize_toplevel (GdkSurface *surface,
                                  gboolean   with_move,
                                  gint       x,
                                  gint       y,
                                  gint       width,
                                  gint       height)
{
  GDK_SURFACE_GET_CLASS (surface)->move_resize (surface, with_move, x, y, width, height);
}


static void
gdk_surface_move_resize_internal (GdkSurface *surface,
                                  gboolean   with_move,
                                  gint       x,
                                  gint       y,
                                  gint       width,
                                  gint       height)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (surface->destroyed)
    return;

  gdk_surface_move_resize_toplevel (surface, with_move, x, y, width, height);
}

/**
 * gdk_surface_resize:
 * @surface: a #GdkSurface
 * @width: new width of the surface
 * @height: new height of the surface
 *
 * Resizes @surface; for toplevel surfaces, asks the window manager to resize
 * the surface. The window manager may not allow the resize. When using GTK,
 * use gtk_window_resize() instead of this low-level GDK function.
 *
 * Surfaces may not be resized below 1x1.
 */
void
gdk_surface_resize (GdkSurface *surface,
                    gint       width,
                    gint       height)
{
  GDK_SURFACE_GET_CLASS (surface)->toplevel_resize (surface, width, height);
}


/*
 * gdk_surface_move_resize:
 * @surface: a #GdkSurface
 * @x: new X position relative to surface’s parent
 * @y: new Y position relative to surface’s parent
 * @width: new width
 * @height: new height
 *
 * Equivalent to calling gdk_surface_move() and gdk_surface_resize(),
 * except that both operations are performed at once, avoiding strange
 * visual effects. (i.e. the user may be able to see the surface first
 * move, then resize, if you don’t use gdk_surface_move_resize().)
 **/
void
gdk_surface_move_resize (GdkSurface *surface,
                         gint       x,
                         gint       y,
                         gint       width,
                         gint       height)
{
  gdk_surface_move_resize_internal (surface, TRUE, x, y, width, height);
}

/**
 * gdk_surface_move_to_rect:
 * @surface: the #GdkSurface to move
 * @rect: (not nullable): the destination #GdkRectangle to align @surface with
 * @rect_anchor: the point on @rect to align with @surface's anchor point
 * @surface_anchor: the point on @surface to align with @rect's anchor point
 * @anchor_hints: positioning hints to use when limited on space
 * @rect_anchor_dx: horizontal offset to shift @surface, i.e. @rect's anchor
 *                  point
 * @rect_anchor_dy: vertical offset to shift @surface, i.e. @rect's anchor point
 *
 * Moves @surface to @rect, aligning their anchor points.
 *
 * @rect is relative to the top-left corner of the surface that @surface is
 * transient for. @rect_anchor and @surface_anchor determine anchor points on
 * @rect and @surface to pin together. @rect's anchor point can optionally be
 * offset by @rect_anchor_dx and @rect_anchor_dy, which is equivalent to
 * offsetting the position of @surface.
 *
 * @anchor_hints determines how @surface will be moved if the anchor points cause
 * it to move off-screen. For example, %GDK_ANCHOR_FLIP_X will replace
 * %GDK_GRAVITY_NORTH_WEST with %GDK_GRAVITY_NORTH_EAST and vice versa if
 * @surface extends beyond the left or right edges of the monitor.
 *
 * Connect to the #GdkSurface::moved-to-rect signal to find out how it was
 * actually positioned.
 */
void
gdk_surface_move_to_rect (GdkSurface          *surface,
                          const GdkRectangle *rect,
                          GdkGravity          rect_anchor,
                          GdkGravity          surface_anchor,
                          GdkAnchorHints      anchor_hints,
                          gint                rect_anchor_dx,
                          gint                rect_anchor_dy)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (surface->parent || surface->transient_for);
  g_return_if_fail (rect);

  GDK_SURFACE_GET_CLASS (surface)->move_to_rect (surface,
                                                 rect,
                                                 rect_anchor,
                                                 surface_anchor,
                                                 anchor_hints,
                                                 rect_anchor_dx,
                                                 rect_anchor_dy);
}

static void
gdk_surface_set_cursor_internal (GdkSurface *surface,
                                 GdkDevice *device,
                                 GdkCursor *cursor)
{
  GdkPointerSurfaceInfo *pointer_info;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  g_assert (surface->display == gdk_device_get_display (device));

  pointer_info = _gdk_display_get_pointer_info (surface->display, device);

  if (surface == pointer_info->surface_under_pointer)
    update_cursor (surface->display, device);
}

/**
 * gdk_surface_get_cursor:
 * @surface: a #GdkSurface
 *
 * Retrieves a #GdkCursor pointer for the cursor currently set on the
 * specified #GdkSurface, or %NULL.  If the return value is %NULL then
 * there is no custom cursor set on the specified surface, and it is
 * using the cursor for its parent surface.
 *
 * Returns: (nullable) (transfer none): a #GdkCursor, or %NULL. The
 *   returned object is owned by the #GdkSurface and should not be
 *   unreferenced directly. Use gdk_surface_set_cursor() to unset the
 *   cursor of the surface
 */
GdkCursor *
gdk_surface_get_cursor (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);

  return surface->cursor;
}

/**
 * gdk_surface_set_cursor:
 * @surface: a #GdkSurface
 * @cursor: (allow-none): a cursor
 *
 * Sets the default mouse pointer for a #GdkSurface.
 *
 * Note that @cursor must be for the same display as @surface.
 *
 * Use gdk_cursor_new_from_name() or gdk_cursor_new_from_texture() to
 * create the cursor. To make the cursor invisible, use %GDK_BLANK_CURSOR.
 * Passing %NULL for the @cursor argument to gdk_surface_set_cursor() means
 * that @surface will use the cursor of its parent surface. Most surfaces
 * should use this default.
 */
void
gdk_surface_set_cursor (GdkSurface *surface,
                        GdkCursor *cursor)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (surface->cursor)
    {
      g_object_unref (surface->cursor);
      surface->cursor = NULL;
    }

  if (!GDK_SURFACE_DESTROYED (surface))
    {
      GdkDevice *device;
      GList *seats, *s;

      if (cursor)
        surface->cursor = g_object_ref (cursor);

      seats = gdk_display_list_seats (surface->display);

      for (s = seats; s; s = s->next)
        {
          GList *devices, *d;

          device = gdk_seat_get_pointer (s->data);
          gdk_surface_set_cursor_internal (surface, device, surface->cursor);

          devices = gdk_seat_get_slaves (s->data, GDK_SEAT_CAPABILITY_TABLET_STYLUS);
          for (d = devices; d; d = d->next)
            {
              device = gdk_device_get_associated_device (d->data);
              gdk_surface_set_cursor_internal (surface, device, surface->cursor);
            }
          g_list_free (devices);
        }

      g_list_free (seats);
      g_object_notify_by_pspec (G_OBJECT (surface), properties[PROP_CURSOR]);
    }
}

/**
 * gdk_surface_get_device_cursor:
 * @surface: a #GdkSurface.
 * @device: a master, pointer #GdkDevice.
 *
 * Retrieves a #GdkCursor pointer for the @device currently set on the
 * specified #GdkSurface, or %NULL.  If the return value is %NULL then
 * there is no custom cursor set on the specified surface, and it is
 * using the cursor for its parent surface.
 *
 * Returns: (nullable) (transfer none): a #GdkCursor, or %NULL. The
 *   returned object is owned by the #GdkSurface and should not be
 *   unreferenced directly. Use gdk_surface_set_cursor() to unset the
 *   cursor of the surface
 **/
GdkCursor *
gdk_surface_get_device_cursor (GdkSurface *surface,
                               GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);
  g_return_val_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD, NULL);
  g_return_val_if_fail (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_MASTER, NULL);

  return g_hash_table_lookup (surface->device_cursor, device);
}

/**
 * gdk_surface_set_device_cursor:
 * @surface: a #GdkSurface
 * @device: a master, pointer #GdkDevice
 * @cursor: a #GdkCursor
 *
 * Sets a specific #GdkCursor for a given device when it gets inside @surface.
 * Use gdk_cursor_new_fromm_name() or gdk_cursor_new_from_texture() to create
 * the cursor. To make the cursor invisible, use %GDK_BLANK_CURSOR. Passing
 * %NULL for the @cursor argument to gdk_surface_set_cursor() means that
 * @surface will use the cursor of its parent surface. Most surfaces should
 * use this default.
 **/
void
gdk_surface_set_device_cursor (GdkSurface *surface,
                               GdkDevice *device,
                               GdkCursor *cursor)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD);
  g_return_if_fail (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_MASTER);

  if (!cursor)
    g_hash_table_remove (surface->device_cursor, device);
  else
    g_hash_table_replace (surface->device_cursor, device, g_object_ref (cursor));

  gdk_surface_set_cursor_internal (surface, device, cursor);
}

/*
 * gdk_surface_get_geometry:
 * @surface: a #GdkSurface
 * @x: (out) (allow-none): return location for X coordinate of surface (relative to its parent)
 * @y: (out) (allow-none): return location for Y coordinate of surface (relative to its parent)
 * @width: (out) (allow-none): return location for width of surface
 * @height: (out) (allow-none): return location for height of surface
 *
 * Any of the return location arguments to this function may be %NULL,
 * if you aren’t interested in getting the value of that field.
 *
 * The X and Y coordinates returned are relative to the parent surface
 * of @surface, which for toplevels usually means relative to the
 * surface decorations (titlebar, etc.) rather than relative to the
 * root window (screen-size background window).
 *
 * On the X11 platform, the geometry is obtained from the X server,
 * so reflects the latest position of @surface; this may be out-of-sync
 * with the position of @surface delivered in the most-recently-processed
 * #GdkEventConfigure. gdk_surface_get_position() in contrast gets the
 * position from the most recent configure event.
 *
 * Note: If @surface is not a toplevel, it is much better
 * to call gdk_surface_get_position(), gdk_surface_get_width() and
 * gdk_surface_get_height() instead, because it avoids the roundtrip to
 * the X server and because these functions support the full 32-bit
 * coordinate space, whereas gdk_surface_get_geometry() is restricted to
 * the 16-bit coordinates of X11.
 */
void
gdk_surface_get_geometry (GdkSurface *surface,
                          gint      *x,
                          gint      *y,
                          gint      *width,
                          gint      *height)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  GDK_SURFACE_GET_CLASS (surface)->get_geometry (surface, x, y, width, height);
}

/**
 * gdk_surface_get_width:
 * @surface: a #GdkSurface
 *
 * Returns the width of the given @surface.
 *
 * On the X11 platform the returned size is the size reported in the
 * most-recently-processed configure event, rather than the current
 * size on the X server.
 *
 * Returns: The width of @surface
 */
int
gdk_surface_get_width (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), 0);

  return surface->width;
}

/**
 * gdk_surface_get_height:
 * @surface: a #GdkSurface
 *
 * Returns the height of the given @surface.
 *
 * On the X11 platform the returned size is the size reported in the
 * most-recently-processed configure event, rather than the current
 * size on the X server.
 *
 * Returns: The height of @surface
 */
int
gdk_surface_get_height (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), 0);

  return surface->height;
}

/*
 * gdk_surface_get_origin:
 * @surface: a #GdkSurface
 * @x: (out): return location for X coordinate
 * @y: (out): return location for Y coordinate
 *
 * Obtains the position of a surface in root window coordinates.
 * (Compare with gdk_surface_get_position() and
 * gdk_surface_get_geometry() which return the position
 * of a surface relative to its parent surface.)
 */
void
gdk_surface_get_origin (GdkSurface *surface,
                        gint       *x,
                        gint       *y)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  gdk_surface_get_root_coords (surface, 0, 0, x, y);
}

/*
 * gdk_surface_get_root_coords:
 * @surface: a #GdkSurface
 * @x: X coordinate in surface
 * @y: Y coordinate in surface
 * @root_x: (out): return location for X coordinate
 * @root_y: (out): return location for Y coordinate
 *
 * Obtains the position of a surface position in root
 * window coordinates. This is similar to
 * gdk_surface_get_origin() but allows you to pass
 * in any position in the surface, not just the origin.
 */
void
gdk_surface_get_root_coords (GdkSurface *surface,
                             gint       x,
                             gint       y,
                             gint      *root_x,
                             gint      *root_y)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    {
      *root_x = 0;
      *root_y = 0;
      return;
    }
  
  GDK_SURFACE_GET_CLASS (surface)->get_root_coords (surface, x, y, root_x, root_y);
}

/**
 * gdk_surface_input_shape_combine_region:
 * @surface: a #GdkSurface
 * @shape_region: region of surface to be non-transparent
 * @offset_x: X position of @shape_region in @surface coordinates
 * @offset_y: Y position of @shape_region in @surface coordinates
 *
 * Like gdk_surface_shape_combine_region(), but the shape applies
 * only to event handling. Mouse events which happen while
 * the pointer position corresponds to an unset bit in the
 * mask will be passed on the surface below @surface.
 *
 * An input shape is typically used with RGBA surfaces.
 * The alpha channel of the surface defines which pixels are
 * invisible and allows for nicely antialiased borders,
 * and the input shape controls where the surface is
 * “clickable”.
 *
 * On the X11 platform, this requires version 1.1 of the
 * shape extension.
 *
 * On the Win32 platform, this functionality is not present and the
 * function does nothing.
 */
void
gdk_surface_input_shape_combine_region (GdkSurface       *surface,
                                        const cairo_region_t *shape_region,
                                        gint             offset_x,
                                        gint             offset_y)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (surface->input_shape)
    cairo_region_destroy (surface->input_shape);

  if (shape_region)
    {
      surface->input_shape = cairo_region_copy (shape_region);
      cairo_region_translate (surface->input_shape, offset_x, offset_y);
    }
  else
    surface->input_shape = NULL;

  GDK_SURFACE_GET_CLASS (surface)->input_shape_combine_region (surface, surface->input_shape, 0, 0);
}

/**
 * gdk_surface_get_modal_hint:
 * @surface: A toplevel #GdkSurface.
 *
 * Determines whether or not the window manager is hinted that @surface
 * has modal behaviour.
 *
 * Returns: whether or not the surface has the modal hint set.
 */
gboolean
gdk_surface_get_modal_hint (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), FALSE);

  return surface->modal_hint;
}

/**
 * gdk_surface_get_accept_focus:
 * @surface: a toplevel #GdkSurface.
 *
 * Determines whether or not the desktop environment shuld be hinted that
 * the surface does not want to receive input focus.
 *
 * Returns: whether or not the surface should receive input focus.
 */
gboolean
gdk_surface_get_accept_focus (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), FALSE);

  return surface->accept_focus;
}

/**
 * gdk_surface_get_focus_on_map:
 * @surface: a toplevel #GdkSurface.
 *
 * Determines whether or not the desktop environment should be hinted that the
 * surface does not want to receive input focus when it is mapped.
 *
 * Returns: whether or not the surface wants to receive input focus when
 * it is mapped.
 */
gboolean
gdk_surface_get_focus_on_map (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), FALSE);

  return surface->focus_on_map;
}

static void
update_cursor (GdkDisplay *display,
               GdkDevice  *device)
{
  GdkSurface *cursor_surface;
  GdkSurface *pointer_surface;
  GdkPointerSurfaceInfo *pointer_info;
  GdkDeviceGrabInfo *grab;
  GdkCursor *cursor;

  pointer_info = _gdk_display_get_pointer_info (display, device);
  pointer_surface = pointer_info->surface_under_pointer;

  /* We ignore the serials here and just pick the last grab
     we've sent, as that would shortly be used anyway. */
  grab = _gdk_display_get_last_device_grab (display, device);
  if (grab != NULL)
    {
      /* use the cursor from the grab surface */
      cursor_surface = grab->surface;
    }
  else
    {
      /* otherwise use the cursor from the pointer surface */
      cursor_surface = pointer_surface;
    }

  cursor = g_hash_table_lookup (cursor_surface->device_cursor, device);

  if (!cursor)
    cursor = cursor_surface->cursor;

  GDK_DEVICE_GET_CLASS (device)->set_surface_cursor (device, pointer_surface, cursor);
}

/**
 * gdk_surface_beep:
 * @surface: a toplevel #GdkSurface
 *
 * Emits a short beep associated to @surface in the appropriate
 * display, if supported. Otherwise, emits a short beep on
 * the display just as gdk_display_beep().
 **/
void
gdk_surface_beep (GdkSurface *surface)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (GDK_SURFACE_GET_CLASS (surface)->beep (surface))
    return;

  gdk_display_beep (surface->display);
}

/**
 * gdk_surface_set_support_multidevice:
 * @surface: a #GdkSurface.
 * @support_multidevice: %TRUE to enable multidevice support in @surface.
 *
 * This function will enable multidevice features in @surface.
 *
 * Multidevice aware surfaces will need to handle properly multiple,
 * per device enter/leave events, device grabs and grab ownerships.
 **/
void
gdk_surface_set_support_multidevice (GdkSurface *surface,
                                     gboolean   support_multidevice)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  if (surface->support_multidevice == support_multidevice)
    return;

  surface->support_multidevice = support_multidevice;

  /* FIXME: What to do if called when some pointers are inside the surface ? */
}

/**
 * gdk_surface_get_support_multidevice:
 * @surface: a #GdkSurface.
 *
 * Returns %TRUE if the surface is aware of the existence of multiple
 * devices.
 *
 * Returns: %TRUE if the surface handles multidevice features.
 **/
gboolean
gdk_surface_get_support_multidevice (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), FALSE);

  if (GDK_SURFACE_DESTROYED (surface))
    return FALSE;

  return surface->support_multidevice;
}

void
_gdk_display_set_surface_under_pointer (GdkDisplay *display,
                                        GdkDevice  *device,
                                        GdkSurface  *surface)
{
  GdkPointerSurfaceInfo *device_info;

  device_info = _gdk_display_get_pointer_info (display, device);

  if (device_info->surface_under_pointer)
    g_object_unref (device_info->surface_under_pointer);
  device_info->surface_under_pointer = surface;

  if (surface)
    {
      g_object_ref (surface);
      update_cursor (display, device);
    }
}

#define GDK_ANY_BUTTON_MASK (GDK_BUTTON1_MASK | \
                             GDK_BUTTON2_MASK | \
                             GDK_BUTTON3_MASK | \
                             GDK_BUTTON4_MASK | \
                             GDK_BUTTON5_MASK)

void
_gdk_windowing_got_event (GdkDisplay *display,
                          GList      *event_link,
                          GdkEvent   *event,
                          gulong      serial)
{
  GdkSurface *event_surface;
  gboolean unlink_event = FALSE;
  GdkDeviceGrabInfo *button_release_grab;
  GdkPointerSurfaceInfo *pointer_info = NULL;
  GdkDevice *device, *source_device;

  _gdk_display_update_last_event (display, event);

  device = gdk_event_get_device (event);
  source_device = gdk_event_get_source_device (event);

  if (device)
    {
      if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD &&
          gdk_device_get_source (device) != GDK_SOURCE_TABLET_PAD)
        {
          pointer_info = _gdk_display_get_pointer_info (display, device);

          if (source_device != pointer_info->last_slave &&
              gdk_device_get_device_type (source_device) == GDK_DEVICE_TYPE_SLAVE)
            pointer_info->last_slave = source_device;
          else if (pointer_info->last_slave)
            source_device = pointer_info->last_slave;
        }

      _gdk_display_device_grab_update (display, device, source_device, serial);

      if (gdk_device_get_input_mode (device) == GDK_MODE_DISABLED ||
          !_gdk_display_check_grab_ownership (display, device, serial))
        {
          /* Device events are blocked by another
           * device grab, or the device is disabled
           */
          unlink_event = TRUE;
          goto out;
        }
    }

  event_surface = event->any.surface;
  if (!event_surface)
    goto out;

  if (event->any.type == GDK_ENTER_NOTIFY)
    _gdk_display_set_surface_under_pointer (display, device, event_surface);
  else if (event->any.type == GDK_LEAVE_NOTIFY)
    _gdk_display_set_surface_under_pointer (display, device, NULL);

  if ((event->any.type == GDK_BUTTON_RELEASE ||
       event->any.type == GDK_TOUCH_CANCEL ||
       event->any.type == GDK_TOUCH_END) &&
      !event->any.send_event)
    {
      if (event->any.type == GDK_BUTTON_RELEASE ||
          gdk_event_get_pointer_emulated (event))
        {
          button_release_grab =
            _gdk_display_has_device_grab (display, device, serial);

          if (button_release_grab &&
              button_release_grab->implicit &&
              (event->button.state & GDK_ANY_BUTTON_MASK & ~(GDK_BUTTON1_MASK << (event->button.button - 1))) == 0)
            {
              button_release_grab->serial_end = serial;
              button_release_grab->implicit_ungrab = FALSE;
              _gdk_display_device_grab_update (display, device, source_device, serial);
            }
        }
    }

 out:
  if (unlink_event)
    {
      _gdk_event_queue_remove_link (display, event_link);
      g_list_free_1 (event_link);
      g_object_unref (event);
    }

  /* This does two things - first it sees if there are motions at the
   * end of the queue that can be compressed. Second, if there is just
   * a single motion that won't be dispatched because it is a compression
   * candidate it queues up flushing the event queue.
   */
  _gdk_event_queue_handle_motion_compression (display);
}

/**
 * gdk_surface_create_similar_surface:
 * @surface: surface to make new surface similar to
 * @content: the content for the new surface
 * @width: width of the new surface
 * @height: height of the new surface
 *
 * Create a new surface that is as compatible as possible with the
 * given @surface. For example the new surface will have the same
 * fallback resolution and font options as @surface. Generally, the new
 * surface will also use the same backend as @surface, unless that is
 * not possible for some reason. The type of the returned surface may
 * be examined with cairo_surface_get_type().
 *
 * Initially the surface contents are all 0 (transparent if contents
 * have transparency, black otherwise.)
 *
 * Returns: a pointer to the newly allocated surface. The caller
 * owns the surface and should call cairo_surface_destroy() when done
 * with it.
 *
 * This function always returns a valid pointer, but it will return a
 * pointer to a “nil” surface if @other is already in an error state
 * or any other error occurs.
 **/
cairo_surface_t *
gdk_surface_create_similar_surface (GdkSurface *     surface,
                                    cairo_content_t content,
                                    int             width,
                                    int             height)
{
  cairo_surface_t *similar_surface;
  int scale;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);

  scale = gdk_surface_get_scale_factor (surface);

  similar_surface = cairo_image_surface_create (content == CAIRO_CONTENT_COLOR ? CAIRO_FORMAT_RGB24 :
                                                content == CAIRO_CONTENT_ALPHA ? CAIRO_FORMAT_A8 : CAIRO_FORMAT_ARGB32,
                                                width * scale, height * scale);
  cairo_surface_set_device_scale (similar_surface, scale, scale);

  return similar_surface;
}

/**
 * gdk_surface_focus:
 * @surface: a #GdkSurface
 * @timestamp: timestamp of the event triggering the surface focus
 *
 * Sets keyboard focus to @surface. In most cases, gtk_window_present_with_time()
 * should be used on a #GtkWindow, rather than calling this function.
 *
 **/
void
gdk_surface_focus (GdkSurface *surface,
                   guint32    timestamp)
{
  GDK_SURFACE_GET_CLASS (surface)->focus (surface, timestamp);
}

/**
 * gdk_surface_set_type_hint:
 * @surface: A toplevel #GdkSurface
 * @hint: A hint of the function this surface will have
 *
 * The application can use this call to provide a hint to the surface
 * manager about the functionality of a surface. The window manager
 * can use this information when determining the decoration and behaviour
 * of the surface.
 *
 * The hint must be set before the surface is mapped.
 **/
void
gdk_surface_set_type_hint (GdkSurface        *surface,
                           GdkSurfaceTypeHint hint)
{
  GDK_SURFACE_GET_CLASS (surface)->set_type_hint (surface, hint);
}

/**
 * gdk_surface_get_type_hint:
 * @surface: A toplevel #GdkSurface
 *
 * This function returns the type hint set for a surface.
 *
 * Returns: The type hint set for @surface
 **/
GdkSurfaceTypeHint
gdk_surface_get_type_hint (GdkSurface *surface)
{
  return GDK_SURFACE_GET_CLASS (surface)->get_type_hint (surface);
}

/**
 * gdk_surface_set_modal_hint:
 * @surface: A toplevel #GdkSurface
 * @modal: %TRUE if the surface is modal, %FALSE otherwise.
 *
 * The application can use this hint to tell the window manager
 * that a certain surface has modal behaviour. The window manager
 * can use this information to handle modal surfaces in a special
 * way.
 *
 * You should only use this on surfaces for which you have
 * previously called gdk_surface_set_transient_for()
 **/
void
gdk_surface_set_modal_hint (GdkSurface *surface,
                            gboolean   modal)
{
  GDK_SURFACE_GET_CLASS (surface)->set_modal_hint (surface, modal);
}

/**
 * gdk_surface_set_geometry_hints:
 * @surface: a toplevel #GdkSurface
 * @geometry: geometry hints
 * @geom_mask: bitmask indicating fields of @geometry to pay attention to
 *
 * Sets the geometry hints for @surface. Hints flagged in @geom_mask
 * are set, hints not flagged in @geom_mask are unset.
 * To unset all hints, use a @geom_mask of 0 and a @geometry of %NULL.
 *
 * This function provides hints to the surfaceing system about
 * acceptable sizes for a toplevel surface. The purpose of
 * this is to constrain user resizing, but the windowing system
 * will typically  (but is not required to) also constrain the
 * current size of the surface to the provided values and
 * constrain programatic resizing via gdk_surface_resize() or
 * gdk_surface_move_resize().
 *
 * Note that on X11, this effect has no effect on surfaces
 * of type %GDK_SURFACE_TEMP since these surfaces are not resizable
 * by the user.
 *
 * Since you can’t count on the windowing system doing the
 * constraints for programmatic resizes, you should generally
 * call gdk_surface_constrain_size() yourself to determine
 * appropriate sizes.
 *
 **/
void
gdk_surface_set_geometry_hints (GdkSurface         *surface,
                                const GdkGeometry *geometry,
                                GdkSurfaceHints     geom_mask)
{
  g_return_if_fail (geometry != NULL || geom_mask == 0);

  GDK_SURFACE_GET_CLASS (surface)->set_geometry_hints (surface, geometry, geom_mask);
}

/**
 * gdk_surface_set_title:
 * @surface: a toplevel #GdkSurface
 * @title: title of @surface
 *
 * Sets the title of a toplevel surface, to be displayed in the titlebar.
 * If you haven’t explicitly set the icon name for the surface
 * (using gdk_surface_set_icon_name()), the icon name will be set to
 * @title as well. @title must be in UTF-8 encoding (as with all
 * user-readable strings in GDK and GTK). @title may not be %NULL.
 **/
void
gdk_surface_set_title (GdkSurface   *surface,
                       const gchar *title)
{
  GDK_SURFACE_GET_CLASS (surface)->set_title (surface, title);
}

/**
 * gdk_surface_set_startup_id:
 * @surface: a toplevel #GdkSurface
 * @startup_id: a string with startup-notification identifier
 *
 * When using GTK, typically you should use gtk_window_set_startup_id()
 * instead of this low-level function.
 **/
void
gdk_surface_set_startup_id (GdkSurface   *surface,
                            const gchar *startup_id)
{
  GDK_SURFACE_GET_CLASS (surface)->set_startup_id (surface, startup_id);
}

/**
 * gdk_surface_set_transient_for:
 * @surface: a toplevel #GdkSurface
 * @parent: another toplevel #GdkSurface
 *
 * Indicates to the window manager that @surface is a transient dialog
 * associated with the application surface @parent. This allows the
 * window manager to do things like center @surface on @parent and
 * keep @surface above @parent.
 *
 * See gtk_window_set_transient_for() if you’re using #GtkWindow or
 * #GtkDialog.
 **/
void
gdk_surface_set_transient_for (GdkSurface *surface,
                               GdkSurface *parent)
{
  surface->transient_for = parent;

  GDK_SURFACE_GET_CLASS (surface)->set_transient_for (surface, parent);
}

/**
 * gdk_surface_set_accept_focus:
 * @surface: a toplevel #GdkSurface
 * @accept_focus: %TRUE if the surface should receive input focus
 *
 * Setting @accept_focus to %FALSE hints the desktop environment that the
 * surface doesn’t want to receive input focus.
 *
 * On X, it is the responsibility of the window manager to interpret this
 * hint. ICCCM-compliant window manager usually respect it.
 **/
void
gdk_surface_set_accept_focus (GdkSurface *surface,
                              gboolean accept_focus)
{
  GDK_SURFACE_GET_CLASS (surface)->set_accept_focus (surface, accept_focus);
}

/**
 * gdk_surface_set_focus_on_map:
 * @surface: a toplevel #GdkSurface
 * @focus_on_map: %TRUE if the surface should receive input focus when mapped
 *
 * Setting @focus_on_map to %FALSE hints the desktop environment that the
 * surface doesn’t want to receive input focus when it is mapped.
 * focus_on_map should be turned off for surfaces that aren’t triggered
 * interactively (such as popups from network activity).
 *
 * On X, it is the responsibility of the window manager to interpret
 * this hint. Window managers following the freedesktop.org window
 * manager extension specification should respect it.
 **/
void
gdk_surface_set_focus_on_map (GdkSurface *surface,
                              gboolean focus_on_map)
{
  GDK_SURFACE_GET_CLASS (surface)->set_focus_on_map (surface, focus_on_map);
}

/**
 * gdk_surface_set_icon_list:
 * @surface: The #GdkSurface toplevel surface to set the icon of.
 * @surfaces: (transfer none) (element-type GdkTexture):
 *     A list of image surfaces, of different sizes.
 *
 * Sets a list of icons for the surface. One of these will be used
 * to represent the surface when it has been iconified. The icon is
 * usually shown in an icon box or some sort of task bar. Which icon
 * size is shown depends on the window manager. The window manager
 * can scale the icon  but setting several size icons can give better
 * image quality since the window manager may only need to scale the
 * icon by a small amount or not at all.
 *
 * Note that some platforms don't support surface icons.
 */
void
gdk_surface_set_icon_list (GdkSurface *surface,
                           GList     *textures)
{
  GDK_SURFACE_GET_CLASS (surface)->set_icon_list (surface, textures);
}

/**
 * gdk_surface_set_icon_name:
 * @surface: a toplevel #GdkSurface
 * @name: (allow-none): name of surface while iconified (minimized)
 *
 * Surfaces may have a name used while minimized, distinct from the
 * name they display in their titlebar. Most of the time this is a bad
 * idea from a user interface standpoint. But you can set such a name
 * with this function, if you like.
 *
 * After calling this with a non-%NULL @name, calls to gdk_surface_set_title()
 * will not update the icon title.
 *
 * Using %NULL for @name unsets the icon title; further calls to
 * gdk_surface_set_title() will again update the icon title as well.
 *
 * Note that some platforms don't support surface icons.
 **/
void
gdk_surface_set_icon_name (GdkSurface   *surface,
                           const gchar *name)
{
  GDK_SURFACE_GET_CLASS (surface)->set_icon_name (surface, name);
}

/**
 * gdk_surface_iconify:
 * @surface: a toplevel #GdkSurface
 *
 * Asks to iconify (minimize) @surface. The window manager may choose
 * to ignore the request, but normally will honor it. Using
 * gtk_window_iconify() is preferred, if you have a #GtkWindow widget.
 *
 * This function only makes sense when @surface is a toplevel surface.
 *
 **/
void
gdk_surface_iconify (GdkSurface *surface)
{
  GDK_SURFACE_GET_CLASS (surface)->iconify (surface);
}

/**
 * gdk_surface_deiconify:
 * @surface: a toplevel #GdkSurface
 *
 * Attempt to deiconify (unminimize) @surface. On X11 the window manager may
 * choose to ignore the request to deiconify. When using GTK,
 * use gtk_window_deiconify() instead of the #GdkSurface variant. Or better yet,
 * you probably want to use gtk_window_present_with_time(), which raises the surface, focuses it,
 * unminimizes it, and puts it on the current desktop.
 *
 **/
void
gdk_surface_deiconify (GdkSurface *surface)
{
  GDK_SURFACE_GET_CLASS (surface)->deiconify (surface);
}

/**
 * gdk_surface_stick:
 * @surface: a toplevel #GdkSurface
 *
 * “Pins” a surface such that it’s on all workspaces and does not scroll
 * with viewports, for window managers that have scrollable viewports.
 * (When using #GtkWindow, gtk_window_stick() may be more useful.)
 *
 * On the X11 platform, this function depends on window manager
 * support, so may have no effect with many window managers. However,
 * GDK will do the best it can to convince the window manager to stick
 * the surface. For window managers that don’t support this operation,
 * there’s nothing you can do to force it to happen.
 *
 **/
void
gdk_surface_stick (GdkSurface *surface)
{
  GDK_SURFACE_GET_CLASS (surface)->stick (surface);
}

/**
 * gdk_surface_unstick:
 * @surface: a toplevel #GdkSurface
 *
 * Reverse operation for gdk_surface_stick(); see gdk_surface_stick(),
 * and gtk_window_unstick().
 *
 **/
void
gdk_surface_unstick (GdkSurface *surface)
{
  GDK_SURFACE_GET_CLASS (surface)->unstick (surface);
}

/**
 * gdk_surface_maximize:
 * @surface: a toplevel #GdkSurface
 *
 * Maximizes the surface. If the surface was already maximized, then
 * this function does nothing.
 *
 * On X11, asks the window manager to maximize @surface, if the window
 * manager supports this operation. Not all window managers support
 * this, and some deliberately ignore it or don’t have a concept of
 * “maximized”; so you can’t rely on the maximization actually
 * happening. But it will happen with most standard window managers,
 * and GDK makes a best effort to get it to happen.
 *
 * On Windows, reliably maximizes the surface.
 *
 **/
void
gdk_surface_maximize (GdkSurface *surface)
{
  GDK_SURFACE_GET_CLASS (surface)->maximize (surface);
}

/**
 * gdk_surface_unmaximize:
 * @surface: a toplevel #GdkSurface
 *
 * Unmaximizes the surface. If the surface wasn’t maximized, then this
 * function does nothing.
 *
 * On X11, asks the window manager to unmaximize @surface, if the
 * window manager supports this operation. Not all window managers
 * support this, and some deliberately ignore it or don’t have a
 * concept of “maximized”; so you can’t rely on the unmaximization
 * actually happening. But it will happen with most standard window
 * managers, and GDK makes a best effort to get it to happen.
 *
 * On Windows, reliably unmaximizes the surface.
 *
 **/
void
gdk_surface_unmaximize (GdkSurface *surface)
{
  GDK_SURFACE_GET_CLASS (surface)->unmaximize (surface);
}

/**
 * gdk_surface_fullscreen:
 * @surface: a toplevel #GdkSurface
 *
 * Moves the surface into fullscreen mode. This means the
 * surface covers the entire screen and is above any panels
 * or task bars.
 *
 * If the surface was already fullscreen, then this function does nothing.
 *
 * On X11, asks the window manager to put @surface in a fullscreen
 * state, if the window manager supports this operation. Not all
 * window managers support this, and some deliberately ignore it or
 * don’t have a concept of “fullscreen”; so you can’t rely on the
 * fullscreenification actually happening. But it will happen with
 * most standard window managers, and GDK makes a best effort to get
 * it to happen.
 **/
void
gdk_surface_fullscreen (GdkSurface *surface)
{
  GDK_SURFACE_GET_CLASS (surface)->fullscreen (surface);
}

/**
 * gdk_surface_fullscreen_on_monitor:
 * @surface: a toplevel #GdkSurface
 * @monitor: Which monitor to display fullscreen on.
 *
 * Moves the surface into fullscreen mode on the given monitor. This means
 * the surface covers the entire screen and is above any panels or task bars.
 *
 * If the surface was already fullscreen, then this function does nothing.
 **/
void
gdk_surface_fullscreen_on_monitor (GdkSurface  *surface,
                                   GdkMonitor *monitor)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (GDK_IS_MONITOR (monitor));
  g_return_if_fail (gdk_monitor_get_display (monitor) == surface->display);
  g_return_if_fail (gdk_monitor_is_valid (monitor));

  if (GDK_SURFACE_GET_CLASS (surface)->fullscreen_on_monitor != NULL)
    GDK_SURFACE_GET_CLASS (surface)->fullscreen_on_monitor (surface, monitor);
  else
    GDK_SURFACE_GET_CLASS (surface)->fullscreen (surface);
}

/**
 * gdk_surface_set_fullscreen_mode:
 * @surface: a toplevel #GdkSurface
 * @mode: fullscreen mode
 *
 * Specifies whether the @surface should span over all monitors (in a multi-head
 * setup) or only the current monitor when in fullscreen mode.
 *
 * The @mode argument is from the #GdkFullscreenMode enumeration.
 * If #GDK_FULLSCREEN_ON_ALL_MONITORS is specified, the fullscreen @surface will
 * span over all monitors of the display.
 *
 * On X11, searches through the list of monitors display the ones
 * which delimit the 4 edges of the entire display and will ask the window
 * manager to span the @surface over these monitors.
 *
 * If the XINERAMA extension is not available or not usable, this function
 * has no effect.
 *
 * Not all window managers support this, so you can’t rely on the fullscreen
 * surface to span over the multiple monitors when #GDK_FULLSCREEN_ON_ALL_MONITORS
 * is specified.
 **/
void
gdk_surface_set_fullscreen_mode (GdkSurface        *surface,
                                 GdkFullscreenMode mode)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (surface->fullscreen_mode != mode)
    {
      surface->fullscreen_mode = mode;

      if (GDK_SURFACE_GET_CLASS (surface)->apply_fullscreen_mode != NULL)
        GDK_SURFACE_GET_CLASS (surface)->apply_fullscreen_mode (surface);
    }
}

/**
 * gdk_surface_get_fullscreen_mode:
 * @surface: a toplevel #GdkSurface
 *
 * Obtains the #GdkFullscreenMode of the @surface.
 *
 * Returns: The #GdkFullscreenMode applied to the surface when fullscreen.
 **/
GdkFullscreenMode
gdk_surface_get_fullscreen_mode (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), GDK_FULLSCREEN_ON_CURRENT_MONITOR);

  return surface->fullscreen_mode;
}

/**
 * gdk_surface_unfullscreen:
 * @surface: a toplevel #GdkSurface
 *
 * Moves the surface out of fullscreen mode. If the surface was not
 * fullscreen, does nothing.
 *
 * On X11, asks the window manager to move @surface out of the fullscreen
 * state, if the window manager supports this operation. Not all
 * window managers support this, and some deliberately ignore it or
 * don’t have a concept of “fullscreen”; so you can’t rely on the
 * unfullscreenification actually happening. But it will happen with
 * most standard window managers, and GDK makes a best effort to get
 * it to happen.
 **/
void
gdk_surface_unfullscreen (GdkSurface *surface)
{
  GDK_SURFACE_GET_CLASS (surface)->unfullscreen (surface);
}

/**
 * gdk_surface_set_keep_above:
 * @surface: a toplevel #GdkSurface
 * @setting: whether to keep @surface above other surfaces
 *
 * Set if @surface must be kept above other surfaces. If the
 * surface was already above, then this function does nothing.
 *
 * On X11, asks the window manager to keep @surface above, if the window
 * manager supports this operation. Not all window managers support
 * this, and some deliberately ignore it or don’t have a concept of
 * “keep above”; so you can’t rely on the surface being kept above.
 * But it will happen with most standard window managers,
 * and GDK makes a best effort to get it to happen.
 **/
void
gdk_surface_set_keep_above (GdkSurface *surface,
                            gboolean   setting)
{
  GDK_SURFACE_GET_CLASS (surface)->set_keep_above (surface, setting);
}

/**
 * gdk_surface_set_keep_below:
 * @surface: a toplevel #GdkSurface
 * @setting: whether to keep @surface below other surfaces
 *
 * Set if @surface must be kept below other surfaces. If the
 * surface was already below, then this function does nothing.
 *
 * On X11, asks the window manager to keep @surface below, if the window
 * manager supports this operation. Not all window managers support
 * this, and some deliberately ignore it or don’t have a concept of
 * “keep below”; so you can’t rely on the surface being kept below.
 * But it will happen with most standard window managers,
 * and GDK makes a best effort to get it to happen.
 **/
void
gdk_surface_set_keep_below (GdkSurface *surface,
                            gboolean setting)
{
  GDK_SURFACE_GET_CLASS (surface)->set_keep_below (surface, setting);
}

/**
 * gdk_surface_set_decorations:
 * @surface: a toplevel #GdkSurface
 * @decorations: decoration hint mask
 *
 * “Decorations” are the features the window manager adds to a toplevel #GdkSurface.
 * This function sets the traditional Motif window manager hints that tell the
 * window manager which decorations you would like your surface to have.
 * Usually you should use gtk_window_set_decorated() on a #GtkWindow instead of
 * using the GDK function directly.
 *
 * The @decorations argument is the logical OR of the fields in
 * the #GdkWMDecoration enumeration. If #GDK_DECOR_ALL is included in the
 * mask, the other bits indicate which decorations should be turned off.
 * If #GDK_DECOR_ALL is not included, then the other bits indicate
 * which decorations should be turned on.
 *
 * Most window managers honor a decorations hint of 0 to disable all decorations,
 * but very few honor all possible combinations of bits.
 *
 **/
void
gdk_surface_set_decorations (GdkSurface      *surface,
                             GdkWMDecoration decorations)
{
  GDK_SURFACE_GET_CLASS (surface)->set_decorations (surface, decorations);
}

/**
 * gdk_surface_get_decorations:
 * @surface: The toplevel #GdkSurface to get the decorations from
 * @decorations: (out): The surface decorations will be written here
 *
 * Returns the decorations set on the GdkSurface with
 * gdk_surface_set_decorations().
 *
 * Returns: %TRUE if the surface has decorations set, %FALSE otherwise.
 **/
gboolean
gdk_surface_get_decorations (GdkSurface       *surface,
                             GdkWMDecoration *decorations)
{
  return GDK_SURFACE_GET_CLASS (surface)->get_decorations (surface, decorations);
}

/**
 * gdk_surface_set_functions:
 * @surface: a toplevel #GdkSurface
 * @functions: bitmask of operations to allow on @surface
 *
 * Sets hints about the window management functions to make available
 * via buttons on the window frame.
 *
 * On the X backend, this function sets the traditional Motif window
 * manager hint for this purpose. However, few window managers do
 * anything reliable or interesting with this hint. Many ignore it
 * entirely.
 *
 * The @functions argument is the logical OR of values from the
 * #GdkWMFunction enumeration. If the bitmask includes #GDK_FUNC_ALL,
 * then the other bits indicate which functions to disable; if
 * it doesn’t include #GDK_FUNC_ALL, it indicates which functions to
 * enable.
 *
 **/
void
gdk_surface_set_functions (GdkSurface    *surface,
                           GdkWMFunction functions)
{
  GDK_SURFACE_GET_CLASS (surface)->set_functions (surface, functions);
}

/**
 * gdk_surface_begin_resize_drag_for_device:
 * @surface: a toplevel #GdkSurface
 * @edge: the edge or corner from which the drag is started
 * @device: the device used for the operation
 * @button: the button being used to drag, or 0 for a keyboard-initiated drag
 * @x: surface X coordinate of mouse click that began the drag
 * @y: surface Y coordinate of mouse click that began the drag
 * @timestamp: timestamp of mouse click that began the drag (use gdk_event_get_time())
 *
 * Begins a surface resize operation (for a toplevel surface).
 * You might use this function to implement a “window resize grip,”
 */
void
gdk_surface_begin_resize_drag_for_device (GdkSurface     *surface,
                                          GdkSurfaceEdge  edge,
                                          GdkDevice      *device,
                                          gint            button,
                                          gint            x,
                                          gint            y,
                                          guint32         timestamp)
{
  GDK_SURFACE_GET_CLASS (surface)->begin_resize_drag (surface, edge, device, button, x, y, timestamp);
}

/**
 * gdk_surface_begin_resize_drag:
 * @surface: a toplevel #GdkSurface
 * @edge: the edge or corner from which the drag is started
 * @button: the button being used to drag, or 0 for a keyboard-initiated drag
 * @x: surface X coordinate of mouse click that began the drag
 * @y: surface Y coordinate of mouse click that began the drag
 * @timestamp: timestamp of mouse click that began the drag (use gdk_event_get_time())
 *
 * Begins a surface resize operation (for a toplevel surface).
 *
 * This function assumes that the drag is controlled by the
 * client pointer device, use gdk_surface_begin_resize_drag_for_device()
 * to begin a drag with a different device.
 */
void
gdk_surface_begin_resize_drag (GdkSurface     *surface,
                               GdkSurfaceEdge  edge,
                               gint            button,
                               gint            x,
                               gint            y,
                               guint32         timestamp)
{
  GdkDevice *device;

  device = gdk_seat_get_pointer (gdk_display_get_default_seat (surface->display));
  gdk_surface_begin_resize_drag_for_device (surface, edge,
                                            device, button, x, y, timestamp);
}

/**
 * gdk_surface_begin_move_drag_for_device:
 * @surface: a toplevel #GdkSurface
 * @device: the device used for the operation
 * @button: the button being used to drag, or 0 for a keyboard-initiated drag
 * @x: surface X coordinate of mouse click that began the drag
 * @y: surface Y coordinate of mouse click that began the drag
 * @timestamp: timestamp of mouse click that began the drag
 *
 * Begins a surface move operation (for a toplevel surface).
 */
void
gdk_surface_begin_move_drag_for_device (GdkSurface *surface,
                                        GdkDevice  *device,
                                        gint        button,
                                        gint        x,
                                        gint        y,
                                        guint32     timestamp)
{
  GDK_SURFACE_GET_CLASS (surface)->begin_move_drag (surface,
                                                    device, button, x, y, timestamp);
}

/**
 * gdk_surface_begin_move_drag:
 * @surface: a toplevel #GdkSurface
 * @button: the button being used to drag, or 0 for a keyboard-initiated drag
 * @x: surface X coordinate of mouse click that began the drag
 * @y: surface Y coordinate of mouse click that began the drag
 * @timestamp: timestamp of mouse click that began the drag
 *
 * Begins a surface move operation (for a toplevel surface).
 *
 * This function assumes that the drag is controlled by the
 * client pointer device, use gdk_surface_begin_move_drag_for_device()
 * to begin a drag with a different device.
 */
void
gdk_surface_begin_move_drag (GdkSurface *surface,
                             gint       button,
                             gint       x,
                             gint       y,
                             guint32    timestamp)
{
  GdkDevice *device;

  device = gdk_seat_get_pointer (gdk_display_get_default_seat (surface->display));
  gdk_surface_begin_move_drag_for_device (surface, device, button, x, y, timestamp);
}

/**
 * gdk_surface_set_opacity:
 * @surface: a top-level or non-native #GdkSurface
 * @opacity: opacity
 *
 * Set @surface to render as partially transparent,
 * with opacity 0 being fully transparent and 1 fully opaque. (Values
 * of the opacity parameter are clamped to the [0,1] range.) 
 *
 * For toplevel surfaces this depends on support from the windowing system
 * that may not always be there. For instance, On X11, this works only on
 * X screens with a compositing manager running. On Wayland, there is no
 * per-surface opacity value that the compositor would apply. Instead, use
 * `gdk_surface_set_opaque_region (surface, NULL)` to tell the compositor
 * that the entire surface is (potentially) non-opaque, and draw your content
 * with alpha, or use gtk_widget_set_opacity() to set an overall opacity
 * for your widgets.
 *
 * Support for non-toplevel surfaces was added in 3.8.
 */
void
gdk_surface_set_opacity (GdkSurface *surface,
                         gdouble    opacity)
{
  if (opacity < 0)
    opacity = 0;
  else if (opacity > 1)
    opacity = 1;

  surface->alpha = round (opacity * 255);

  if (surface->destroyed)
    return;

  GDK_SURFACE_GET_CLASS (surface)->set_opacity (surface, opacity);
}

/* This function is called when the XWindow is really gone.
 */
void
gdk_surface_destroy_notify (GdkSurface *surface)
{
  GDK_SURFACE_GET_CLASS (surface)->destroy_notify (surface);
}

/**
 * gdk_surface_register_dnd:
 * @surface: a #GdkSurface.
 *
 * Registers a surface as a potential drop destination.
 */
void
gdk_surface_register_dnd (GdkSurface *surface)
{
  GDK_SURFACE_GET_CLASS (surface)->register_dnd (surface);
}

/**
 * gdk_drag_begin:
 * @surface: the source surface for this drag
 * @device: the device that controls this drag
 * @content: (transfer none): the offered content
 * @actions: the actions supported by this drag
 * @dx: the x offset to @device's position where the drag nominally started
 * @dy: the y offset to @device's position where the drag nominally started
 *
 * Starts a drag and creates a new drag context for it.
 *
 * This function is called by the drag source.
 *
 * Returns: (transfer full) (nullable): a newly created #GdkDrag or
 *     %NULL on error.
 */
GdkDrag *
gdk_drag_begin (GdkSurface          *surface,
                GdkDevice          *device,
                GdkContentProvider *content,
                GdkDragAction       actions,
                gint                dx,
                gint                dy)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);
  g_return_val_if_fail (surface->display == gdk_device_get_display (device), NULL);
  g_return_val_if_fail (GDK_IS_CONTENT_PROVIDER (content), NULL);

  return GDK_SURFACE_GET_CLASS (surface)->drag_begin (surface, device, content, actions, dx, dy);
}

static void
gdk_surface_flush_events (GdkFrameClock *clock,
                          void          *data)
{
  GdkSurface *surface = GDK_SURFACE (data);

  _gdk_event_queue_flush (surface->display);
  _gdk_display_pause_events (surface->display);

  gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_RESUME_EVENTS);

  surface->frame_clock_events_paused = TRUE;
}

static void
gdk_surface_resume_events (GdkFrameClock *clock,
                           void          *data)
{
  GdkSurface *surface = GDK_SURFACE (data);

  _gdk_display_unpause_events (surface->display);

  surface->frame_clock_events_paused = FALSE;
}

static void
gdk_surface_set_frame_clock (GdkSurface     *surface,
                             GdkFrameClock *clock)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (clock == NULL || GDK_IS_FRAME_CLOCK (clock));

  if (clock == surface->frame_clock)
    return;

  if (clock)
    {
      g_object_ref (clock);

      g_signal_connect (G_OBJECT (clock),
                        "flush-events",
                        G_CALLBACK (gdk_surface_flush_events),
                        surface);
      g_signal_connect (G_OBJECT (clock),
                        "resume-events",
                        G_CALLBACK (gdk_surface_resume_events),
                        surface);
      g_signal_connect (G_OBJECT (clock),
                        "paint",
                        G_CALLBACK (gdk_surface_paint_on_clock),
                        surface);

      if (surface->update_freeze_count == 0)
        _gdk_frame_clock_inhibit_freeze (clock);
    }

  if (surface->frame_clock)
    {
      if (surface->frame_clock_events_paused)
        gdk_surface_resume_events (surface->frame_clock, G_OBJECT (surface));

      g_signal_handlers_disconnect_by_func (G_OBJECT (surface->frame_clock),
                                            G_CALLBACK (gdk_surface_flush_events),
                                            surface);
      g_signal_handlers_disconnect_by_func (G_OBJECT (surface->frame_clock),
                                            G_CALLBACK (gdk_surface_resume_events),
                                            surface);
      g_signal_handlers_disconnect_by_func (G_OBJECT (surface->frame_clock),
                                            G_CALLBACK (gdk_surface_paint_on_clock),
                                            surface);

      if (surface->update_freeze_count == 0)
        _gdk_frame_clock_uninhibit_freeze (surface->frame_clock);

      g_object_unref (surface->frame_clock);
    }

  surface->frame_clock = clock;
}

/**
 * gdk_surface_get_frame_clock:
 * @surface: surface to get frame clock for
 *
 * Gets the frame clock for the surface. The frame clock for a surface
 * never changes unless the surface is reparented to a new toplevel
 * surface.
 *
 * Returns: (transfer none): the frame clock
 */
GdkFrameClock*
gdk_surface_get_frame_clock (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);

  return surface->frame_clock;
}

/**
 * gdk_surface_get_scale_factor:
 * @surface: surface to get scale factor for
 *
 * Returns the internal scale factor that maps from surface coordiantes
 * to the actual device pixels. On traditional systems this is 1, but
 * on very high density outputs this can be a higher value (often 2).
 *
 * A higher value means that drawing is automatically scaled up to
 * a higher resolution, so any code doing drawing will automatically look
 * nicer. However, if you are supplying pixel-based data the scale
 * value can be used to determine whether to use a pixel resource
 * with higher resolution data.
 *
 * The scale of a surface may change during runtime, if this happens
 * a configure event will be sent to the toplevel surface.
 *
 * Returns: the scale factor
 */
gint
gdk_surface_get_scale_factor (GdkSurface *surface)
{
  GdkSurfaceClass *class;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), 1);

  if (GDK_SURFACE_DESTROYED (surface))
    return 1;

  class = GDK_SURFACE_GET_CLASS (surface);
  if (class->get_scale_factor)
    return class->get_scale_factor (surface);

  return 1;
}

/* Returns the *real* unscaled size, which may be a fractional size
   in surface scale coordinates. We need this to properly handle GL
   coordinates which are y-flipped in the real coordinates. */
void
gdk_surface_get_unscaled_size (GdkSurface *surface,
                               int *unscaled_width,
                               int *unscaled_height)
{
  GdkSurfaceClass *class;
  gint scale;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  class = GDK_SURFACE_GET_CLASS (surface);

  if (class->get_unscaled_size)
    {
      class->get_unscaled_size (surface, unscaled_width, unscaled_height);
      return;
    }

  scale = gdk_surface_get_scale_factor (surface);

  if (unscaled_width)
    *unscaled_width = surface->width * scale;

  if (unscaled_height)
    *unscaled_height = surface->height * scale;
}


/**
 * gdk_surface_set_opaque_region:
 * @surface: a top-level or non-native #GdkSurface
 * @region: (allow-none):  a region, or %NULL
 *
 * For optimisation purposes, compositing window managers may
 * like to not draw obscured regions of surfaces, or turn off blending
 * during for these regions. With RGB windows with no transparency,
 * this is just the shape of the window, but with ARGB32 windows, the
 * compositor does not know what regions of the window are transparent
 * or not.
 *
 * This function only works for toplevel surfaces.
 *
 * GTK will update this property automatically if
 * the @surface background is opaque, as we know where the opaque regions
 * are. If your surface background is not opaque, please update this
 * property in your #GtkWidget::style-updated handler.
 */
void
gdk_surface_set_opaque_region (GdkSurface      *surface,
                               cairo_region_t *region)
{
  GdkSurfaceClass *class;

  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (!GDK_SURFACE_DESTROYED (surface));

  if (cairo_region_equal (surface->opaque_region, region))
    return;

  g_clear_pointer (&surface->opaque_region, cairo_region_destroy);

  if (region != NULL)
    surface->opaque_region = cairo_region_reference (region);

  class = GDK_SURFACE_GET_CLASS (surface);
  if (class->set_opaque_region)
    class->set_opaque_region (surface, region);
}

/**
 * gdk_surface_set_shadow_width:
 * @surface: a #GdkSurface
 * @left: The left extent
 * @right: The right extent
 * @top: The top extent
 * @bottom: The bottom extent
 *
 * Newer GTK windows using client-side decorations use extra geometry
 * around their frames for effects like shadows and invisible borders.
 * Window managers that want to maximize windows or snap to edges need
 * to know where the extents of the actual frame lie, so that users
 * don’t feel like windows are snapping against random invisible edges.
 *
 * Note that this property is automatically updated by GTK, so this
 * function should only be used by applications which do not use GTK
 * to create toplevel surfaces.
 */
void
gdk_surface_set_shadow_width (GdkSurface *surface,
                              gint       left,
                              gint       right,
                              gint       top,
                              gint       bottom)
{
  GdkSurfaceClass *class;

  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (!GDK_SURFACE_DESTROYED (surface));
  g_return_if_fail (left >= 0 && right >= 0 && top >= 0 && bottom >= 0);

  surface->shadow_top = top;
  surface->shadow_left = left;
  surface->shadow_right = right;
  surface->shadow_bottom = bottom;

  class = GDK_SURFACE_GET_CLASS (surface);
  if (class->set_shadow_width)
    class->set_shadow_width (surface, left, right, top, bottom);
}

/**
 * gdk_surface_show_window_menu:
 * @surface: a #GdkSurface
 * @event: a #GdkEvent to show the menu for
 *
 * Asks the windowing system to show the window menu. The window menu
 * is the menu shown when right-clicking the titlebar on traditional
 * windows managed by the window manager. This is useful for windows
 * using client-side decorations, activating it with a right-click
 * on the window decorations.
 *
 * Returns: %TRUE if the window menu was shown and %FALSE otherwise.
 */
gboolean
gdk_surface_show_window_menu (GdkSurface *surface,
                              GdkEvent  *event)
{
  GdkSurfaceClass *class;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), FALSE);
  g_return_val_if_fail (!GDK_SURFACE_DESTROYED (surface), FALSE);

  class = GDK_SURFACE_GET_CLASS (surface);
  if (class->show_window_menu)
    return class->show_window_menu (surface, event);
  else
    return FALSE;
}

gboolean
gdk_surface_supports_edge_constraints (GdkSurface *surface)
{
  GdkSurfaceClass *class;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), FALSE);
  g_return_val_if_fail (!GDK_SURFACE_DESTROYED (surface), FALSE);

  class = GDK_SURFACE_GET_CLASS (surface);
  if (class->supports_edge_constraints)
    return class->supports_edge_constraints (surface);
  else
    return FALSE;
}

void
gdk_surface_set_state (GdkSurface      *surface,
                       GdkSurfaceState  new_state)
{
  gboolean was_mapped, mapped;
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (new_state == surface->state)
    return; /* No actual work to do, nothing changed. */

  /* Actually update the field in GdkSurface, this is sort of an odd
   * place to do it, but seems like the safest since it ensures we expose no
   * inconsistent state to the user.
   */

  was_mapped = GDK_SURFACE_IS_MAPPED (surface);

  surface->state = new_state;

  mapped = GDK_SURFACE_IS_MAPPED (surface);

  _gdk_surface_update_viewable (surface);

  g_object_notify_by_pspec (G_OBJECT (surface), properties[PROP_STATE]);

  if (was_mapped != mapped)
    g_object_notify_by_pspec (G_OBJECT (surface), properties[PROP_MAPPED]);
}

void
gdk_synthesize_surface_state (GdkSurface     *surface,
                              GdkSurfaceState unset_flags,
                              GdkSurfaceState set_flags)
{
  gdk_surface_set_state (surface, (surface->state | set_flags) & ~unset_flags);
}

static gboolean
check_autohide (GdkEvent *event)
{
  GdkDisplay *display;
  GdkDevice *device;
  GdkSurface *grab_surface;

 switch ((guint) gdk_event_get_event_type (event))
    {
    case GDK_BUTTON_PRESS:
#if 0
    // FIXME: we need to ignore the release that is paired
    // with the press starting the grab - due to implicit
    // grabs, it will be delivered to the same place as the
    // press, and will cause the auto dismissal to be triggered.
    case GDK_BUTTON_RELEASE:
#endif
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
    case GDK_TOUCHPAD_SWIPE:
    case GDK_TOUCHPAD_PINCH:
      display = gdk_event_get_display (event);
      device = gdk_event_get_device (event);
      if (gdk_device_grab_info (display, device, &grab_surface, NULL))
        {
          if (grab_surface != gdk_event_get_surface (event) &&
              grab_surface->autohide)
            {
              gdk_surface_hide (grab_surface);
              return TRUE;
            }
        }
      break;
    default:;
    }

  return FALSE;
}

static gboolean
is_key_event (GdkEvent *event)
{
  switch ((guint) gdk_event_get_event_type (event))
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      return TRUE;
    default:;
    }

  return FALSE;
}

static void
rewrite_event_for_toplevel (GdkEvent *event)
{
  GdkSurface *surface;

  surface = gdk_event_get_surface (event);
  while (surface->parent)
    surface = surface->parent;

  g_set_object (&event->any.surface, surface);
}

#ifdef G_ENABLE_DEBUG
static void
add_event_mark (GdkEvent *event,
                gint64    time,
                guint64   duration)
{
  gchar *message = NULL;
  const gchar *kind;
  GEnumValue *value;
  GdkEventType event_type;

  event_type = gdk_event_get_event_type (event);
  value = g_enum_get_value (g_type_class_peek_static (GDK_TYPE_EVENT_TYPE), event_type);
  kind = value ? value->value_nick : NULL;

  switch (event_type)
    {
    case GDK_MOTION_NOTIFY:
      {
        GdkEventMotion *motion = (GdkEventMotion *)event;
        message = g_strdup_printf ("{x=%lf, y=%lf, state=0x%x}",
                                   motion->x, motion->y, motion->state);
        break;
      }

    case GDK_BUTTON_PRESS:
      {
        GdkEventButton *button = (GdkEventButton *)event;
        message = g_strdup_printf ("{button=%u, x=%lf, y=%lf, state=0x%x}",
                                   button->button, button->x, button->y, button->state);
        break;
      }

    case GDK_BUTTON_RELEASE:
      {
        GdkEventButton *button = (GdkEventButton *)event;
        message = g_strdup_printf ("{button=%u, x=%lf, y=%lf, state=0x%x}",
                                   button->button, button->x, button->y, button->state);
        break;
      }

    case GDK_KEY_PRESS:
      {
        GdkEventKey *key = (GdkEventKey *)event;
        message = g_strdup_printf ("{keyval=%u, state=0x%x, hardware_keycode=%u key_scancode=%u group=%u is_modifier=%u}",
                                   key->keyval, key->state, key->hardware_keycode, key->key_scancode, key->group, key->is_modifier);
        break;
      }

    case GDK_KEY_RELEASE:
      {
        GdkEventKey *key = (GdkEventKey *)event;
        message = g_strdup_printf ("{keyval=%u, state=0x%x, hardware_keycode=%u key_scancode=%u group=%u is_modifier=%u}",
                                   key->keyval, key->state, key->hardware_keycode, key->key_scancode, key->group, key->is_modifier);
        break;
      }

    case GDK_CONFIGURE:
      {
        GdkEventConfigure *config = (GdkEventConfigure *)event;
        message = g_strdup_printf ("{x=%d, y=%d, width=%d, height=%d}",
                                   config->x, config->y, config->width, config->height);
        break;
      }

    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
    case GDK_TOUCHPAD_SWIPE:
    case GDK_TOUCHPAD_PINCH:
    case GDK_SCROLL:
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
    case GDK_PAD_BUTTON_PRESS:
    case GDK_PAD_BUTTON_RELEASE:
    case GDK_PAD_RING:
    case GDK_PAD_STRIP:
    case GDK_PAD_GROUP_MODE:
    case GDK_GRAB_BROKEN:
    case GDK_DELETE:
    case GDK_DESTROY:
    case GDK_FOCUS_CHANGE:
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
    case GDK_NOTHING:
    case GDK_EVENT_LAST:
    default:
      break;
    }

  if (kind != NULL && message != NULL)
    {
      gchar *full_message = g_strdup_printf ("%s %s", kind, message);
      gdk_profiler_add_mark (time * 1000L, duration * 1000L, "event", full_message);
      g_free (full_message);
    }
  else
    {
      gdk_profiler_add_mark (time * 1000L, duration * 1000L, "event", message);
    }

  g_free (message);
}
#endif

gboolean
gdk_surface_handle_event (GdkEvent *event)
{
#ifdef G_ENABLE_DEBUG
  gint64 begin_time = g_get_monotonic_time ();
#endif
  gboolean handled = FALSE;

  if (check_autohide (event))
    return TRUE;

  if (gdk_event_get_event_type (event) == GDK_CONFIGURE)
    {
      g_signal_emit (gdk_event_get_surface (event), signals[SIZE_CHANGED], 0,
                     event->configure.width, event->configure.height);
      handled = TRUE;
    }
  else
    {
      if (is_key_event (event))
        rewrite_event_for_toplevel (event);
      g_signal_emit (gdk_event_get_surface (event), signals[EVENT], 0, event, &handled);
    }

#ifdef G_ENABLE_DEBUG
  if (gdk_profiler_is_running ())
    add_event_mark (event, begin_time, g_get_monotonic_time () - begin_time);
#endif

  return handled;
}

gboolean
gdk_surface_translate_coordinates (GdkSurface *from,
                                   GdkSurface *to,
                                   double     *x,
                                   double     *y)
{
  int x1, y1, x2, y2;
  GdkSurface *f, *t;

  x1 = 0;
  y1 = 0;
  f = from;
  while (f->parent)
    {
      x1 += f->x;
      y1 += f->y;
      f = f->parent;
    }

  x2 = 0;
  y2 = 0;
  t = to;
  while (t->parent)
    {
      x2 += t->x;
      y2 += t->y;
      t = t->parent;
    }

  if (f != t)
    return FALSE;

  *x += x1 - x2;
  *y += y1 - y2;

  return TRUE;
}
