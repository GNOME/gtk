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

#include "gdkrectangle.h"
#include "gdkinternals.h"
#include "gdkintl.h"
#include "gdkdisplayprivate.h"
#include "gdkdeviceprivate.h"
#include "gdkframeclockidleprivate.h"
#include "gdkmarshalers.h"
#include "gdksurfaceimpl.h"
#include "gdkglcontextprivate.h"
#include "gdk-private.h"

#include <math.h>

#include <epoxy/gl.h>

/* for the use of round() */
#include "fallback-c89.c"

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#endif

#undef DEBUG_SURFACE_PRINTING


/**
 * SECTION:gdksurface
 * @Short_description: Onscreen display areas in the target window system
 * @Title: Surfaces
 *
 * A #GdkSurface is a (usually) rectangular region on the screen.
 * It’s a low-level object, used to implement high-level objects such as
 * #GtkWidget and #GtkWindow on the GTK level. A #GtkWindow is a toplevel
 * surface, the thing a user might think of as a “window” with a titlebar
 * and so on; a #GtkWindow may contain many sub-GdkSurfaces.
 */

/**
 * GdkSurface:
 *
 * The GdkSurface struct contains only private fields and
 * should not be accessed directly.
 */

/* Historically a GdkSurface always matches a platform native window,
 * be it a toplevel window or a child window. In this setup the
 * GdkSurface (and other GdkDrawables) were platform independent classes,
 * and the actual platform specific implementation was in a delegate
 * object available as “impl” in the surface object.
 *
 * With the addition of client side windows this changes a bit. The
 * application-visible GdkSurface object behaves as it did before, but
 * such surfaces now don't a corresponding native window. Instead subwindows
 * surfaces are “client side”, i.e. emulated by the gdk code such
 * that clipping, drawing, moving, events etc work as expected.
 *
 * GdkSurfaces have a pointer to the “impl surface” they are in, i.e.
 * the topmost GdkSurface which have the same “impl” value. This is stored
 * in impl_surface, which is different from the surface itself only for client
 * side surfaces.
 * All GdkSurfaces (native or not) track the position of the surface in the parent
 * (x, y), the size of the surface (width, height), the position of the surface
 * with respect to the impl surface (abs_x, abs_y). We also track the clip
 * region of the surface wrt parent surfaces, in surface-relative coordinates (clip_region).
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
  PROP_CURSOR,
  PROP_DISPLAY,
  PROP_FRAME_CLOCK,
  PROP_STATE,
  PROP_MAPPED,
  LAST_PROP
};

/* Global info */

static void gdk_surface_finalize   (GObject              *object);

static void gdk_surface_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec);
static void gdk_surface_get_property (GObject      *object,
                                      guint         prop_id,
                                      GValue       *value,
                                      GParamSpec   *pspec);

static void recompute_visible_regions   (GdkSurface *private,
                                         gboolean recalculate_children);
static void gdk_surface_invalidate_in_parent (GdkSurface *private);
static void update_cursor               (GdkDisplay *display,
                                         GdkDevice  *device);

static void gdk_surface_set_frame_clock (GdkSurface      *surface,
                                         GdkFrameClock  *clock);


static guint signals[LAST_SIGNAL] = { 0 };
static GParamSpec *properties[LAST_PROP] = { NULL, };

G_DEFINE_ABSTRACT_TYPE (GdkSurface, gdk_surface, G_TYPE_OBJECT)

#ifdef DEBUG_SURFACE_PRINTING
char *
print_region (cairo_region_t *region)
{
  GString *s = g_string_new ("{");
  if (cairo_region_is_empty (region))
    {
      g_string_append (s, "empty");
    }
  else
    {
      int num = cairo_region_num_rectangles (region);
      cairo_rectangle_int_t r;

      if (num == 1)
        {
          cairo_region_get_rectangle (region, 0, &r);
          g_string_append_printf (s, "%dx%d @%d,%d", r.width, r.height, r.x, r.y);
        }
      else
        {
          int i;
          cairo_region_get_extents (region, &r);
          g_string_append_printf (s, "extent: %dx%d @%d,%d, details: ", r.width, r.height, r.x, r.y);
          for (i = 0; i < num; i++)
            {
              cairo_region_get_rectangle (region, i, &r);
              g_string_append_printf (s, "[%dx%d @%d,%d]", r.width, r.height, r.x, r.y);
              if (i != num -1)
                g_string_append (s, ", ");
            }
        }
    }
  g_string_append (s, "}");
  return g_string_free (s, FALSE);
}
#endif

static GList *
list_insert_link_before (GList *list,
                         GList *sibling,
                         GList *link)
{
  if (list == NULL || sibling == list)
    {
      link->prev = NULL;
      link->next = list;
      if (list)
        list->prev = link;
      return link;
    }
  else if (sibling == NULL)
    {
      GList *last = g_list_last (list);

      last->next = link;
      link->prev = last;
      link->next = NULL;

      return list;
    }
  else
    {
      link->next = sibling;
      link->prev = sibling->prev;
      sibling->prev = link;

      if (link->prev)
        link->prev->next = link;

      return list;
    }
}

static void
gdk_surface_init (GdkSurface *surface)
{
  /* 0-initialization is good for all other fields. */

  surface->surface_type = GDK_SURFACE_CHILD;

  surface->state = GDK_SURFACE_STATE_WITHDRAWN;
  surface->fullscreen_mode = GDK_FULLSCREEN_ON_CURRENT_MONITOR;
  surface->width = 1;
  surface->height = 1;
  surface->children_list_node.data = surface;

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

  /* Properties */

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

  g_signal_handlers_disconnect_by_func (gdk_surface_get_display (surface),
                                        seat_removed_cb, surface);

  if (!GDK_SURFACE_DESTROYED (surface))
    {
      g_warning ("losing last reference to undestroyed surface");
      _gdk_surface_destroy (surface, FALSE);
    }

  if (surface->impl)
    {
      g_object_unref (surface->impl);
      surface->impl = NULL;
    }

  if (surface->impl_surface != surface)
    {
      g_object_unref (surface->impl_surface);
      surface->impl_surface = NULL;
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
      break;

    case PROP_FRAME_CLOCK:
      gdk_surface_set_frame_clock (surface, GDK_FRAME_CLOCK (g_value_get_object (value)));
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

    case PROP_FRAME_CLOCK:
      g_value_set_object (value, surface->frame_clock);
      break;

    case PROP_STATE:
      g_value_set_flags (value, surface->state);
      break;

    case PROP_MAPPED:
      g_value_set_boolean (value, GDK_SURFACE_IS_MAPPED (surface));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GdkSurface *
gdk_surface_get_impl_surface (GdkSurface *surface)
{
  return surface->impl_surface;
}

GdkSurface *
_gdk_surface_get_impl_surface (GdkSurface *surface)
{
  return gdk_surface_get_impl_surface (surface);
}

static gboolean
gdk_surface_has_impl (GdkSurface *surface)
{
  return surface->impl_surface == surface;
}

static gboolean
gdk_surface_is_toplevel (GdkSurface *surface)
{
  return surface->parent == NULL;
}

gboolean
_gdk_surface_has_impl (GdkSurface *surface)
{
  return gdk_surface_has_impl (surface);
}

static void
remove_child_area (GdkSurface *surface,
                   gboolean for_input,
                   cairo_region_t *region)
{
  GdkSurface *child;
  cairo_region_t *child_region;
  GdkRectangle r;
  GList *l;

  for (l = surface->children; l; l = l->next)
    {
      child = l->data;

      /* If region is empty already, no need to do
         anything potentially costly */
      if (cairo_region_is_empty (region))
        break;

      if (!GDK_SURFACE_IS_MAPPED (child) || child->input_only)
        continue;

      r.x = child->x;
      r.y = child->y;
      r.width = child->width;
      r.height = child->height;

      /* Bail early if child totally outside region */
      if (cairo_region_contains_rectangle (region, &r) == CAIRO_REGION_OVERLAP_OUT)
        continue;

      child_region = cairo_region_create_rectangle (&r);

      if (for_input)
        {
          if (child->input_shape)
            cairo_region_intersect (child_region, child->input_shape);
        }

      cairo_region_subtract (region, child_region);
      cairo_region_destroy (child_region);
    }
}

static void
recompute_visible_regions_internal (GdkSurface *private,
                                    gboolean   recalculate_clip,
                                    gboolean   recalculate_children)
{
  GList *l;
  GdkSurface *child;
  gboolean abs_pos_changed;
  int old_abs_x, old_abs_y;

  old_abs_x = private->abs_x;
  old_abs_y = private->abs_y;

  /* Update absolute position */
  if (gdk_surface_has_impl (private))
    {
      /* Native surfaces and toplevel subsurfaces start here */
      private->abs_x = 0;
      private->abs_y = 0;
    }
  else
    {
      private->abs_x = private->parent->abs_x + private->x;
      private->abs_y = private->parent->abs_y + private->y;
    }

  abs_pos_changed =
    private->abs_x != old_abs_x ||
    private->abs_y != old_abs_y;

  /* Update all children, recursively */
  if ((abs_pos_changed || recalculate_children))
    {
      for (l = private->children; l; l = l->next)
        {
          child = l->data;
          /* Only recalculate clip if the the clip region changed, otherwise
           * there is no way the child clip region could change (its has not e.g. moved)
           * Except if recalculate_children is set to force child updates
           */
          recompute_visible_regions_internal (child,
                                              recalculate_clip && recalculate_children,
                                              FALSE);
        }
    }
}

/* Call this when private has changed in one or more of these ways:
 *  size changed
 *  surface moved
 *  new surface added
 *  stacking order of surface changed
 *  child deleted
 *
 * It will recalculate abs_x/y and the clip regions
 *
 * Unless the surface didn’t change stacking order or size/pos, pass in TRUE
 * for recalculate_siblings. (Mostly used internally for the recursion)
 *
 * If a child surface was removed (and you can’t use that child for
 * recompute_visible_regions), pass in TRUE for recalculate_children on the parent
 */
static void
recompute_visible_regions (GdkSurface *private,
                           gboolean recalculate_children)
{
  recompute_visible_regions_internal (private,
                                      TRUE,
                                      recalculate_children);
}

void
_gdk_surface_update_size (GdkSurface *surface)
{
  GSList *l;

  for (l = surface->draw_contexts; l; l = l->next)
    gdk_draw_context_surface_resized (l->data);

  recompute_visible_regions (surface, FALSE);
}

GdkSurface*
gdk_surface_new (GdkDisplay    *display,
                 GdkSurface     *parent,
                 GdkSurfaceAttr *attributes)
{
  GdkSurface *surface;
  gboolean native;

  g_return_val_if_fail (attributes != NULL, NULL);

  if (parent != NULL && GDK_SURFACE_DESTROYED (parent))
    {
      g_warning ("gdk_surface_new(): parent is destroyed");
      return NULL;
    }

  surface = _gdk_display_create_surface (display);

  surface->parent = parent;

  surface->accept_focus = TRUE;
  surface->focus_on_map = TRUE;

  surface->x = attributes->x;
  surface->y = attributes->y;
  surface->width = (attributes->width > 1) ? (attributes->width) : (1);
  surface->height = (attributes->height > 1) ? (attributes->height) : (1);
  surface->alpha = 255;

  if (attributes->wclass == GDK_INPUT_ONLY)
    {
      /* Backwards compatiblity - we've always ignored
       * attributes->surface_type for input-only surfaces
       * before
       */
      if (parent == NULL)
        surface->surface_type = GDK_SURFACE_TEMP;
      else
        surface->surface_type = GDK_SURFACE_CHILD;
    }
  else
    surface->surface_type = attributes->surface_type;

  /* Sanity checks */
  switch (surface->surface_type)
    {
    case GDK_SURFACE_TOPLEVEL:
    case GDK_SURFACE_TEMP:
      if (parent != NULL)
        g_warning (G_STRLOC "Toplevel surfaces must be created without a parent");
      break;
    case GDK_SURFACE_CHILD:
      break;
    default:
      g_warning (G_STRLOC "cannot make surfaces of type %d", surface->surface_type);
      return NULL;
    }

  if (attributes->wclass == GDK_INPUT_OUTPUT)
    {
      surface->input_only = FALSE;
    }
  else
    {
      surface->input_only = TRUE;
    }

  native = FALSE;

  if (surface->parent != NULL)
    surface->parent->children = g_list_concat (&surface->children_list_node, surface->parent->children);
  else
    {
      GdkFrameClock *frame_clock = g_object_new (GDK_TYPE_FRAME_CLOCK_IDLE, NULL);
      gdk_surface_set_frame_clock (surface, frame_clock);
      g_object_unref (frame_clock);

      native = TRUE; /* Always use native surfaces for toplevels */
    }

  if (native)
    {
      /* Create the impl */
      gdk_display_create_surface_impl (display, surface, parent, attributes);
      surface->impl_surface = surface;
    }
  else
    {
      surface->impl_surface = g_object_ref (surface->parent->impl_surface);
      surface->impl = g_object_ref (surface->impl_surface->impl);
    }

  recompute_visible_regions (surface, FALSE);

  g_signal_connect (display, "seat-removed", G_CALLBACK (seat_removed_cb), surface);

  return surface;
}

/**
 * gdk_surface_new_toplevel: (constructor)
 * @display: the display to create the surface on
 * @width: width of new surface
 * @height: height of new surface
 *
 * Creates a new toplevel surface. The surface will be managed by the surface
 * manager.
 *
 * Returns: (transfer full): the new #GdkSurface
 **/
GdkSurface *
gdk_surface_new_toplevel (GdkDisplay *display,
                          gint        width,
                          gint        height)
{
  GdkSurfaceAttr attr;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  attr.wclass = GDK_INPUT_OUTPUT;
  attr.x = 0;
  attr.y = 0;
  attr.width = width;
  attr.height = height;
  attr.surface_type = GDK_SURFACE_TOPLEVEL;

  return gdk_surface_new (display, NULL, &attr);
}

/**
 * gdk_surface_new_popup: (constructor)
 * @display: the display to create the surface on
 * @position: position of the surface on screen
 *
 * Creates a new toplevel popup surface. The surface will bypass surface
 * management.
 *
 * Returns: (transfer full): the new #GdkSurface
 **/
GdkSurface *
gdk_surface_new_popup (GdkDisplay         *display,
                       const GdkRectangle *position)
{
  GdkSurfaceAttr attr;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (position != NULL, NULL);

  attr.wclass = GDK_INPUT_OUTPUT;
  attr.x = position->x;
  attr.y = position->y;
  attr.width = position->width;
  attr.height = position->height;
  attr.surface_type = GDK_SURFACE_TEMP;

  return gdk_surface_new (display, NULL, &attr);
}

/**
 * gdk_surface_new_popup_full: (constructor)
 * @display: the display to create the surface on
 * @parent: the parent surface to attach the surface to
 *
 * Create a new popup surface.
 * The surface will be attached to @parent and can
 * be positioned relative to it using
 * gdk_surface_move_to_rect().
 *
 * Returns: (transfer full): a new #GdkSurface
 */
GdkSurface *
gdk_surface_new_popup_full (GdkDisplay *display,
                            GdkSurface *parent)
{
  GdkSurface *surface;
  GdkSurfaceAttr attr;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (GDK_IS_SURFACE (parent), NULL);

  attr.wclass = GDK_INPUT_OUTPUT;
  attr.x = 0;
  attr.y = 0;
  attr.width = 100;
  attr.height = 100;
  attr.surface_type = GDK_SURFACE_TEMP;

  surface = gdk_surface_new (display, NULL, &attr);
  gdk_surface_set_transient_for (surface, parent);
  gdk_surface_set_type_hint (surface, GDK_SURFACE_TYPE_HINT_MENU);

  return surface;
}

/**
 * gdk_surface_new_temp: (constructor)
 * @display: the display to create the surface on
 *
 * Creates a new toplevel temporary surface. The surface will be
 * situated off-screen and not handle output.
 *
 * You most likely do not want to use this function.
 *
 * Returns: (transfer full): the new #GdkSurface
 **/
GdkSurface *
gdk_surface_new_temp (GdkDisplay *display)
{
  GdkSurfaceAttr attr;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  attr.wclass = GDK_INPUT_ONLY;
  attr.x = -100;
  attr.y = -100;
  attr.width = 10;
  attr.height = 10;
  attr.surface_type = GDK_SURFACE_TEMP;

  return gdk_surface_new (display, NULL, &attr);
}

/**
 * gdk_surface_new_child: (constructor)
 * @parent: the parent surface
 * @position: placement of the surface inside @parent
 *
 * Creates a new client-side child surface.
 *
 * Returns: (transfer full): the new #GdkSurface
 **/
GdkSurface *
gdk_surface_new_child (GdkSurface          *parent,
                       const GdkRectangle *position)
{
  GdkSurfaceAttr attr;

  g_return_val_if_fail (GDK_IS_SURFACE (parent), NULL);

  attr.wclass = GDK_INPUT_OUTPUT;
  attr.x = position->x;
  attr.y = position->y;
  attr.width = position->width;
  attr.height = position->height;
  attr.surface_type = GDK_SURFACE_CHILD;

  return gdk_surface_new (gdk_surface_get_display (parent), parent, &attr);
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
 * @recursing: If %TRUE, then this is being called because a parent
 *            was destroyed.
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
                                gboolean   recursing,
                                gboolean   recursing_native,
                                gboolean   foreign_destroy)
{
  GdkSurfaceImplClass *impl_class;
  GdkSurface *temp_surface;
  GdkDisplay *display;
  GList *tmp;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  display = gdk_surface_get_display (surface);

  switch (surface->surface_type)
    {
    default:
      g_assert_not_reached ();
      break;

    case GDK_SURFACE_TOPLEVEL:
    case GDK_SURFACE_CHILD:
    case GDK_SURFACE_TEMP:
      if (surface->parent)
        {
          if (surface->parent->children)
            surface->parent->children = g_list_remove_link (surface->parent->children, &surface->children_list_node);

          if (!recursing &&
              GDK_SURFACE_IS_MAPPED (surface))
            {
              recompute_visible_regions (surface, FALSE);
              gdk_surface_invalidate_in_parent (surface);
            }
        }

      if (surface->gl_paint_context)
        {
          /* Make sure to destroy if current */
          g_object_run_dispose (G_OBJECT (surface->gl_paint_context));
          g_object_unref (surface->gl_paint_context);
          surface->gl_paint_context = NULL;
        }

      if (surface->frame_clock)
        {
          g_object_run_dispose (G_OBJECT (surface->frame_clock));
          gdk_surface_set_frame_clock (surface, NULL);
        }

      tmp = surface->children;
      surface->children = NULL;
      /* No need to free children list, its all made up of in-struct nodes */

      while (tmp)
        {
          temp_surface = tmp->data;
          tmp = tmp->next;

          if (temp_surface)
            _gdk_surface_destroy_hierarchy (temp_surface,
                                           TRUE,
                                           recursing_native || gdk_surface_has_impl (surface),
                                           foreign_destroy);
        }

      _gdk_surface_clear_update_area (surface);

      impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);

      if (gdk_surface_has_impl (surface))
        impl_class->destroy (surface, recursing_native, foreign_destroy);
      else
        {
          /* hide to make sure we repaint and break grabs */
          gdk_surface_hide (surface);
        }

      surface->state |= GDK_SURFACE_STATE_WITHDRAWN;
      surface->parent = NULL;
      surface->destroyed = TRUE;

      surface_remove_from_pointer_info (surface, display);

      g_object_notify_by_pspec (G_OBJECT (surface), properties[PROP_STATE]);
      g_object_notify_by_pspec (G_OBJECT (surface), properties[PROP_MAPPED]);
      break;
    }
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
  _gdk_surface_destroy_hierarchy (surface, FALSE, FALSE, foreign_destroy);
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
  _gdk_surface_destroy_hierarchy (surface, FALSE, FALSE, FALSE);
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
 * gdk_surface_has_native:
 * @surface: a #GdkSurface
 *
 * Checks whether the surface has a native surface or not.
 *
 * Returns: %TRUE if the @surface has a native surface, %FALSE otherwise.
 */
gboolean
gdk_surface_has_native (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), FALSE);

  return surface->parent == NULL || surface->parent->impl != surface->impl;
}

/**
 * gdk_surface_get_position:
 * @surface: a #GdkSurface
 * @x: (out) (allow-none): X coordinate of surface
 * @y: (out) (allow-none): Y coordinate of surface
 *
 * Obtains the position of the surface as reported in the
 * most-recently-processed #GdkEventConfigure. Contrast with
 * gdk_surface_get_geometry() which queries the X server for the
 * current surface position, regardless of which events have been
 * received or processed.
 *
 * The position coordinates are relative to the surface’s parent surface.
 *
 **/
void
gdk_surface_get_position (GdkSurface *surface,
                          gint      *x,
                          gint      *y)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (x)
    *x = surface->x;
  if (y)
    *y = surface->y;
}

/**
 * gdk_surface_get_parent:
 * @surface: a #GdkSurface
 *
 * Obtains the parent of @surface, as known to GDK. Does not query the
 * X server; thus this returns the parent as passed to gdk_surface_new(),
 * not the actual parent. This should never matter unless you’re using
 * Xlib calls mixed with GDK calls on the X11 platform. It may also
 * matter for toplevel windows, because the window manager may choose
 * to reparent them.
 *
 * Returns: (transfer none): parent of @surface
 **/
GdkSurface*
gdk_surface_get_parent (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);

  return surface->parent;
}

/**
 * gdk_surface_get_toplevel:
 * @surface: a #GdkSurface
 *
 * Gets the toplevel surface that’s an ancestor of @surface.
 *
 * Any surface type but %GDK_SURFACE_CHILD is considered a
 * toplevel surface, as is a %GDK_SURFACE_CHILD surface that
 * has a root surface as parent.
 *
 * Returns: (transfer none): the toplevel surface containing @surface
 **/
GdkSurface *
gdk_surface_get_toplevel (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);

  while (surface->surface_type == GDK_SURFACE_CHILD)
    {
      if (gdk_surface_is_toplevel (surface))
        break;
      surface = surface->parent;
    }

  return surface;
}

/**
 * gdk_surface_get_children:
 * @surface: a #GdkSurface
 *
 * Gets the list of children of @surface known to GDK.
 * This function only returns children created via GDK,
 * so for example it’s useless when used with the root window;
 * it only returns surfaces an application created itself.
 *
 * The returned list must be freed, but the elements in the
 * list need not be.
 *
 * Returns: (transfer container) (element-type GdkSurface):
 *     list of child surfaces inside @surface
 **/
GList*
gdk_surface_get_children (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);

  if (GDK_SURFACE_DESTROYED (surface))
    return NULL;

  return g_list_copy (surface->children);
}

/**
 * gdk_surface_peek_children:
 * @surface: a #GdkSurface
 *
 * Like gdk_surface_get_children(), but does not copy the list of
 * children, so the list does not need to be freed.
 *
 * Returns: (transfer none) (element-type GdkSurface):
 *     a reference to the list of child surfaces in @surface
 **/
GList *
gdk_surface_peek_children (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);

  if (GDK_SURFACE_DESTROYED (surface))
    return NULL;

  return surface->children;
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

  if (surface->impl_surface->gl_paint_context == NULL)
    {
      GdkSurfaceImplClass *impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);

      if (impl_class->create_gl_context == NULL)
        {
          g_set_error_literal (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE,
                               _("The current backend does not support OpenGL"));
          return NULL;
        }

      surface->impl_surface->gl_paint_context =
        impl_class->create_gl_context (surface->impl_surface,
                                       TRUE,
                                       NULL,
                                       &internal_error);
    }

  if (internal_error != NULL)
    {
      g_propagate_error (error, internal_error);
      g_clear_object (&(surface->impl_surface->gl_paint_context));
      return NULL;
    }

  gdk_gl_context_realize (surface->impl_surface->gl_paint_context, &internal_error);
  if (internal_error != NULL)
    {
      g_propagate_error (error, internal_error);
      g_clear_object (&(surface->impl_surface->gl_paint_context));
      return NULL;
    }

  return surface->impl_surface->gl_paint_context;
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

  return GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->create_gl_context (surface->impl_surface,
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

  display = gdk_surface_get_display (surface);

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

  display = gdk_surface_get_display (surface);

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

static inline gboolean
gdk_surface_is_ancestor (GdkSurface *surface,
                         GdkSurface *ancestor)
{
  while (surface)
    {
      GdkSurface *parent = surface->parent;

      if (parent == ancestor)
        return TRUE;

      surface = parent;
    }

  return FALSE;
}

static void
gdk_surface_add_update_surface (GdkSurface *surface)
{
  GSList *tmp;
  GSList *prev = NULL;
  gboolean has_ancestor_in_list = FALSE;

  /*  Check whether "surface" is already in "update_surfaces" list.
   *  It could be added during execution of gtk_widget_destroy() when
   *  setting focus widget to NULL and redrawing old focus widget.
   *  See bug 711552.
   */
  tmp = g_slist_find (update_surfaces, surface);
  if (tmp != NULL)
    return;

  for (tmp = update_surfaces; tmp; tmp = tmp->next)
    {
      GdkSurface *parent = surface->parent;

      /*  check if tmp is an ancestor of "surface"; if it is, set a
       *  flag indicating that all following surfaces are either
       *  children of "surface" or from a differen hierarchy
       */
      if (!has_ancestor_in_list && gdk_surface_is_ancestor (surface, tmp->data))
        has_ancestor_in_list = TRUE;

      /* insert in reverse stacking order when adding around siblings,
       * so processing updates properly paints over lower stacked surfaces
       */
      if (parent == GDK_SURFACE (tmp->data)->parent)
        {
          if (parent != NULL)
            {
              gint index = g_list_index (parent->children, surface);
              for (; tmp && parent == GDK_SURFACE (tmp->data)->parent; tmp = tmp->next)
                {
                  gint sibling_index = g_list_index (parent->children, tmp->data);
                  if (index > sibling_index)
                    break;
                  prev = tmp;
                }
            }
          /* here, tmp got advanced past all lower stacked siblings */
          tmp = g_slist_prepend (tmp, g_object_ref (surface));
          if (prev)
            prev->next = tmp;
          else
            update_surfaces = tmp;
          return;
        }

      /*  if "surface" has an ancestor in the list and tmp is one of
       *  "surface's" children, insert "surface" before tmp
       */
      if (has_ancestor_in_list && gdk_surface_is_ancestor (tmp->data, surface))
        {
          tmp = g_slist_prepend (tmp, g_object_ref (surface));

          if (prev)
            prev->next = tmp;
          else
            update_surfaces = tmp;
          return;
        }

      /*  if we're at the end of the list and had an ancestor it it,
       *  append to the list
       */
      if (! tmp->next && has_ancestor_in_list)
        {
          tmp = g_slist_append (tmp, g_object_ref (surface));
          return;
        }

      prev = tmp;
    }

  /*  if all above checks failed ("surface" is from a different
   *  hierarchy than what is already in the list) or the list is
   *  empty, prepend
   */
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
  GdkSurface *toplevel;

  toplevel = gdk_surface_get_toplevel (surface);

  return toplevel->update_and_descendants_freeze_count > 0;
}

static void
gdk_surface_schedule_update (GdkSurface *surface)
{
  GdkFrameClock *frame_clock;

  if (surface &&
      (surface->update_freeze_count ||
       gdk_surface_is_toplevel_frozen (surface)))
    return;

  /* If there's no frame clock (a foreign surface), then the invalid
   * region will just stick around unless gdk_surface_process_updates()
   * is called. */
  frame_clock = gdk_surface_get_frame_clock (surface);
  if (frame_clock)
    gdk_frame_clock_request_phase (gdk_surface_get_frame_clock (surface),
                                   GDK_FRAME_CLOCK_PHASE_PAINT);
}

static void
gdk_surface_process_updates_recurse (GdkSurface *surface,
                                     cairo_region_t *expose_region)
{
  gboolean handled;

  if (surface->destroyed)
    return;

  /* Paint the surface before the children, clipped to the surface region */
  g_signal_emit (surface, signals[RENDER], 0, expose_region, &handled);
}

/* Process and remove any invalid area on the native surface by creating
 * expose events for the surface and all non-native descendants.
 */
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

          expose_region = cairo_region_copy (surface->active_update_area);

          gdk_surface_process_updates_recurse (surface, expose_region);

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
  GdkSurface *surface;

  surface = GDK_SURFACE (data);

  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (surface->impl_surface == surface);

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

  if (surface->input_only || !surface->viewable)
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

  while (!gdk_surface_has_impl (surface))
    surface = surface->parent;

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

  if (surface->input_only ||
      !surface->viewable ||
      cairo_region_is_empty (region))
    return;

  r.x = 0;
  r.y = 0;

  visible_region = cairo_region_copy (region);

  while (surface != NULL &&
         !cairo_region_is_empty (visible_region))
    {
      r.width = surface->width;
      r.height = surface->height;
      cairo_region_intersect_rectangle (visible_region, &r);

      if (gdk_surface_has_impl (surface))
        {
          impl_surface_add_update_area (surface, visible_region);
          break;
        }
      else
        {
          cairo_region_translate (visible_region,
                                  surface->x, surface->y);
          surface = surface->parent;
        }
    }

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
  GdkSurface *impl_surface;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  impl_surface = gdk_surface_get_impl_surface (surface);
  impl_surface->update_freeze_count++;
}

/**
 * gdk_surface_thaw_updates:
 * @surface: a #GdkSurface
 *
 * Thaws a surface frozen with gdk_surface_freeze_updates().
 **/
void
gdk_surface_thaw_updates (GdkSurface *surface)
{
  GdkSurface *impl_surface;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  impl_surface = gdk_surface_get_impl_surface (surface);

  g_return_if_fail (impl_surface->update_freeze_count > 0);

  if (--impl_surface->update_freeze_count == 0)
    gdk_surface_schedule_update (GDK_SURFACE (impl_surface));
}

void
gdk_surface_freeze_toplevel_updates (GdkSurface *surface)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (surface->surface_type != GDK_SURFACE_CHILD);

  surface->update_and_descendants_freeze_count++;
  _gdk_frame_clock_freeze (gdk_surface_get_frame_clock (surface));
}

void
gdk_surface_thaw_toplevel_updates (GdkSurface *surface)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (surface->surface_type != GDK_SURFACE_CHILD);
  g_return_if_fail (surface->update_and_descendants_freeze_count > 0);

  surface->update_and_descendants_freeze_count--;
  _gdk_frame_clock_thaw (gdk_surface_get_frame_clock (surface));

  gdk_surface_schedule_update (surface);
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
 *
 * Returns: (nullable) (transfer none): The surface underneath @device
 * (as with gdk_device_get_surface_at_position()), or %NULL if the
 * surface is not known to GDK.
 **/
GdkSurface *
gdk_surface_get_device_position (GdkSurface       *surface,
                                 GdkDevice       *device,
                                 double          *x,
                                 double          *y,
                                 GdkModifierType *mask)
{
  gdouble tmp_x, tmp_y;
  GdkModifierType tmp_mask;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);
  g_return_val_if_fail (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD, NULL);

  tmp_x = tmp_y = 0;
  tmp_mask = 0;
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->get_device_state (surface,
                                                                device,
                                                                &tmp_x, &tmp_y,
                                                                &tmp_mask);
  if (x)
    *x = tmp_x;
  if (y)
    *y = tmp_y;
  if (mask)
    *mask = tmp_mask;

  return NULL;
}

static gboolean
gdk_surface_raise_internal (GdkSurface *surface)
{
  GdkSurface *parent = surface->parent;
  GdkSurfaceImplClass *impl_class;
  gboolean did_raise = FALSE;

  if (parent && parent->children->data != surface)
    {
      parent->children = g_list_remove_link (parent->children, &surface->children_list_node);
      parent->children = g_list_concat (&surface->children_list_node, parent->children);
      did_raise = TRUE;
    }

  impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);

  /* Just do native raise for toplevels */
  if (gdk_surface_has_impl (surface))
      impl_class->raise (surface);

  return did_raise;
}

/* Returns TRUE If the native surface was mapped or unmapped */
static gboolean
set_viewable (GdkSurface *w,
              gboolean val)
{
  GdkSurface *child;
  GList *l;

  if (w->viewable == val)
    return FALSE;

  w->viewable = val;

  if (val)
    recompute_visible_regions (w, FALSE);

  for (l = w->children; l != NULL; l = l->next)
    {
      child = l->data;

      if (GDK_SURFACE_IS_MAPPED (child))
        set_viewable (child, val);
    }

  return FALSE;
}

/* Returns TRUE If the native surface was mapped or unmapped */
gboolean
_gdk_surface_update_viewable (GdkSurface *surface)
{
  gboolean viewable;

  if (gdk_surface_is_toplevel (surface) ||
      surface->parent->viewable)
    viewable = GDK_SURFACE_IS_MAPPED (surface);
  else
    viewable = FALSE;

  return set_viewable (surface, viewable);
}

static void
gdk_surface_show_internal (GdkSurface *surface, gboolean raise)
{
  GdkSurfaceImplClass *impl_class;
  gboolean was_mapped, was_viewable;
  gboolean did_show, did_raise = FALSE;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (surface->destroyed)
    return;

  was_mapped = GDK_SURFACE_IS_MAPPED (surface);
  was_viewable = surface->viewable;

  if (raise)
    {
      /* Keep children in (reverse) stacking order */
      did_raise = gdk_surface_raise_internal (surface);
    }

  if (gdk_surface_has_impl (surface))
    {
      if (!was_mapped)
        gdk_synthesize_surface_state (surface,
                                     GDK_SURFACE_STATE_WITHDRAWN,
                                     0);
    }
  else
    {
      surface->state = 0;
      g_object_notify_by_pspec (G_OBJECT (surface), properties[PROP_STATE]);
      g_object_notify_by_pspec (G_OBJECT (surface), properties[PROP_MAPPED]);
    }

  did_show = _gdk_surface_update_viewable (surface);

  /* If it was already viewable the backend show op won't be called, call it
     again to ensure things happen right if the mapped tracking was not right
     for e.g. a foreign surface.
     Dunno if this is strictly needed but its what happened pre-csw.
     Also show if not done by gdk_surface_update_viewable. */
  if (gdk_surface_has_impl (surface) && (was_viewable || !did_show))
    {
      impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);
      impl_class->show (surface, !did_show ? was_mapped : TRUE);
    }

  if (!was_mapped || did_raise)
    {
      recompute_visible_regions (surface, FALSE);

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
  gboolean did_raise;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (surface->destroyed)
    return;

  /* Keep children in (reverse) stacking order */
  did_raise = gdk_surface_raise_internal (surface);

  if (did_raise &&
      !gdk_surface_is_toplevel (surface) &&
      gdk_surface_is_viewable (surface) &&
      !surface->input_only)
    gdk_surface_invalidate_rect (surface, NULL);
}

static void
gdk_surface_lower_internal (GdkSurface *surface)
{
  GdkSurface *parent = surface->parent;
  GdkSurfaceImplClass *impl_class;

  if (parent)
    {
      parent->children = g_list_remove_link (parent->children, &surface->children_list_node);
      parent->children = g_list_concat (parent->children, &surface->children_list_node);
    }

  impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);

  /* Just do native lower for toplevels */
  if (gdk_surface_has_impl (surface))
    impl_class->lower (surface);
}

static void
gdk_surface_invalidate_in_parent (GdkSurface *private)
{
  GdkRectangle r, child;

  if (gdk_surface_is_toplevel (private))
    return;

  /* get the visible rectangle of the parent */
  r.x = r.y = 0;
  r.width = private->parent->width;
  r.height = private->parent->height;

  child.x = private->x;
  child.y = private->y;
  child.width = private->width;
  child.height = private->height;
  gdk_rectangle_intersect (&r, &child, &r);

  gdk_surface_invalidate_rect (private->parent, &r);
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

  gdk_surface_invalidate_in_parent (surface);
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
  GdkSurfaceImplClass *impl_class;
  GdkSurface *parent;
  GList *sibling_link;

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

  if (gdk_surface_is_toplevel (surface))
    {
      g_return_if_fail (gdk_surface_is_toplevel (sibling));
      impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);
      impl_class->restack_toplevel (surface, sibling, above);
      return;
    }

  parent = surface->parent;
  if (parent)
    {
      sibling_link = g_list_find (parent->children, sibling);
      g_return_if_fail (sibling_link != NULL);
      if (sibling_link == NULL)
        return;

      parent->children = g_list_remove_link (parent->children, &surface->children_list_node);
      if (above)
        parent->children = list_insert_link_before (parent->children,
                                                    sibling_link,
                                                    &surface->children_list_node);
      else
        parent->children = list_insert_link_before (parent->children,
                                                    sibling_link->next,
                                                    &surface->children_list_node);
    }

  gdk_surface_invalidate_in_parent (surface);
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
  gdk_surface_show_internal (surface, TRUE);
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
  GdkSurfaceImplClass *impl_class;
  gboolean was_mapped, did_hide;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (surface->destroyed)
    return;

  was_mapped = GDK_SURFACE_IS_MAPPED (surface);

  if (gdk_surface_has_impl (surface))
    {

      if (GDK_SURFACE_IS_MAPPED (surface))
        gdk_synthesize_surface_state (surface,
                                     0,
                                     GDK_SURFACE_STATE_WITHDRAWN);
    }
  else if (was_mapped)
    {
      surface->state = GDK_SURFACE_STATE_WITHDRAWN;
      g_object_notify_by_pspec (G_OBJECT (surface), properties[PROP_STATE]);
      g_object_notify_by_pspec (G_OBJECT (surface), properties[PROP_MAPPED]);
    }

  if (was_mapped)
    {
      GdkDisplay *display;
      GdkSeat *seat;
      GList *devices, *d;

      /* May need to break grabs on children */
      display = gdk_surface_get_display (surface);
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

  did_hide = _gdk_surface_update_viewable (surface);

  /* Hide foreign surface as those are not handled by update_viewable. */
  if (gdk_surface_has_impl (surface) && (!did_hide))
    {
      impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);
      impl_class->hide (surface);
    }

  recompute_visible_regions (surface, FALSE);

  /* Invalidate the rect */
  if (was_mapped)
    gdk_surface_invalidate_in_parent (surface);
}

static void
gdk_surface_move_resize_toplevel (GdkSurface *surface,
                                  gboolean   with_move,
                                  gint       x,
                                  gint       y,
                                  gint       width,
                                  gint       height)
{
  GdkSurfaceImplClass *impl_class;
  gboolean is_resize;

  is_resize = (width != -1) || (height != -1);

  impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);
  impl_class->move_resize (surface, with_move, x, y, width, height);

  /* Avoid recomputing for pure toplevel moves, for performance reasons */
  if (is_resize)
    recompute_visible_regions (surface, FALSE);
}


static void
gdk_surface_move_resize_internal (GdkSurface *surface,
                                  gboolean   with_move,
                                  gint       x,
                                  gint       y,
                                  gint       width,
                                  gint       height)
{
  cairo_region_t *old_region, *new_region;
  gboolean expose;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (surface->destroyed)
    return;

  if (gdk_surface_is_toplevel (surface))
    {
      gdk_surface_move_resize_toplevel (surface, with_move, x, y, width, height);
      return;
    }

  if (width == 0)
    width = 1;
  if (height == 0)
    height = 1;

  /* Bail early if no change */
  if (surface->width == width &&
      surface->height == height &&
      (!with_move ||
       (surface->x == x &&
        surface->y == y)))
    return;

  /* Handle child surfaces */

  expose = FALSE;
  old_region = NULL;

  if (gdk_surface_is_viewable (surface) &&
      !surface->input_only)
    {
      GdkRectangle r;

      expose = TRUE;

      r.x = surface->x;
      r.y = surface->y;
      r.width = surface->width;
      r.height = surface->height;

      old_region = cairo_region_create_rectangle (&r);
    }

  /* Set the new position and size */
  if (with_move)
    {
      surface->x = x;
      surface->y = y;
    }
  if (!(width < 0 && height < 0))
    {
      surface->width = width;
      surface->height = height;
    }

  recompute_visible_regions (surface, FALSE);

  if (expose)
    {
      GdkRectangle r;

      r.x = surface->x;
      r.y = surface->y;
      r.width = surface->width;
      r.height = surface->height;

      new_region = cairo_region_create_rectangle (&r);

      cairo_region_union (new_region, old_region);

      gdk_surface_invalidate_region (surface->parent, new_region);

      cairo_region_destroy (old_region);
      cairo_region_destroy (new_region);
    }
}



/**
 * gdk_surface_move:
 * @surface: a #GdkSurface
 * @x: X coordinate relative to surface’s parent
 * @y: Y coordinate relative to surface’s parent
 *
 * Repositions a surface relative to its parent surface.
 * For toplevel surfaces, window managers may ignore or modify the move;
 * you should probably use gtk_window_move() on a #GtkWindow widget
 * anyway, instead of using GDK functions. For child surfaces,
 * the move will reliably succeed.
 *
 * If you’re also planning to resize the surface, use gdk_surface_move_resize()
 * to both move and resize simultaneously, for a nicer visual effect.
 **/
void
gdk_surface_move (GdkSurface *surface,
                  gint       x,
                  gint       y)
{
  gdk_surface_move_resize_internal (surface, TRUE, x, y, -1, -1);
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
 *
 * If you’re also planning to move the surface, use gdk_surface_move_resize()
 * to both move and resize simultaneously, for a nicer visual effect.
 **/
void
gdk_surface_resize (GdkSurface *surface,
                    gint       width,
                    gint       height)
{
  gdk_surface_move_resize_internal (surface, FALSE, 0, 0, width, height);
}


/**
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
  GdkSurfaceImplClass *impl_class;

  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (surface->transient_for);
  g_return_if_fail (rect);

  impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);
  impl_class->move_to_rect (surface,
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
  GdkDisplay *display;

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  g_assert (gdk_surface_get_display (surface) == gdk_device_get_display (device));

  display = gdk_surface_get_display (surface);
  pointer_info = _gdk_display_get_pointer_info (display, device);

  if (_gdk_surface_event_parent_of (surface, pointer_info->surface_under_pointer))
    update_cursor (display, device);
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
  GdkDisplay *display;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  display = gdk_surface_get_display (surface);

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

      seats = gdk_display_list_seats (display);

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

/**
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
  GdkSurface *parent;
  GdkSurfaceImplClass *impl_class;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (!GDK_SURFACE_DESTROYED (surface))
    {
      if (gdk_surface_has_impl (surface))
        {
          impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);
          impl_class->get_geometry (surface, x, y,
                                    width, height);
          /* This reports the position wrt to the native parent, we need to convert
             it to be relative to the client side parent */
          parent = surface->parent;
          if (parent && !gdk_surface_has_impl (parent))
            {
              if (x)
                *x -= parent->abs_x;
              if (y)
                *y -= parent->abs_y;
            }
        }
      else
        {
          if (x)
            *x = surface->x;
          if (y)
            *y = surface->y;
          if (width)
            *width = surface->width;
          if (height)
            *height = surface->height;
        }
    }
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

/**
 * gdk_surface_get_origin:
 * @surface: a #GdkSurface
 * @x: (out) (allow-none): return location for X coordinate
 * @y: (out) (allow-none): return location for Y coordinate
 *
 * Obtains the position of a surface in root window coordinates.
 * (Compare with gdk_surface_get_position() and
 * gdk_surface_get_geometry() which return the position of a surface
 * relative to its parent surface.)
 *
 * Returns: not meaningful, ignore
 */
gint
gdk_surface_get_origin (GdkSurface *surface,
                        gint      *x,
                        gint      *y)
{
  gint dummy_x, dummy_y;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), 0);

  gdk_surface_get_root_coords (surface,
                              0, 0,
                              x ? x : &dummy_x,
                              y ? y : &dummy_y);

  return TRUE;
}

/**
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
  GdkSurfaceImplClass *impl_class;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    {
      *root_x = 0;
      *root_y = 0;
      return;
    }
  
  impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);
  impl_class->get_root_coords (surface->impl_surface,
                               x + surface->abs_x,
                               y + surface->abs_y,
                               root_x, root_y);
}

/**
 * gdk_surface_coords_to_parent:
 * @surface: a child surface
 * @x: X coordinate in child’s coordinate system
 * @y: Y coordinate in child’s coordinate system
 * @parent_x: (out) (allow-none): return location for X coordinate
 * in parent’s coordinate system, or %NULL
 * @parent_y: (out) (allow-none): return location for Y coordinate
 * in parent’s coordinate system, or %NULL
 *
 * Transforms surface coordinates from a child surface to its parent
 * surface. Calling this function is equivalent to adding the return
 * values of gdk_surface_get_position() to the child coordinates.
 *
 * See also: gdk_surface_coords_from_parent()
 **/
void
gdk_surface_coords_to_parent (GdkSurface *surface,
                              gdouble    x,
                              gdouble    y,
                              gdouble   *parent_x,
                              gdouble   *parent_y)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (parent_x)
    *parent_x = x + surface->x;

  if (parent_y)
    *parent_y = y + surface->y;
}

/**
 * gdk_surface_coords_from_parent:
 * @surface: a child surface
 * @parent_x: X coordinate in parent’s coordinate system
 * @parent_y: Y coordinate in parent’s coordinate system
 * @x: (out) (allow-none): return location for X coordinate in child’s coordinate system
 * @y: (out) (allow-none): return location for Y coordinate in child’s coordinate system
 *
 * Transforms surface coordinates from a parent surface to a child
 * surface.
 *
 * Calling this function is equivalent to subtracting the return
 * values of gdk_surface_get_position() from the parent coordinates.
 *
 * See also: gdk_surface_coords_to_parent()
 **/
void
gdk_surface_coords_from_parent (GdkSurface *surface,
                                gdouble    parent_x,
                                gdouble    parent_y,
                                gdouble   *x,
                                gdouble   *y)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (x)
    *x = parent_x - surface->x;

  if (y)
    *y = parent_y - surface->y;
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
  GdkSurfaceImplClass *impl_class;

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

  if (gdk_surface_has_impl (surface))
    {
      impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);
      impl_class->input_shape_combine_region (surface, surface->input_shape, 0, 0);
    }
}

static void
do_child_input_shapes (GdkSurface *surface,
                       gboolean merge)
{
  GdkRectangle r;
  cairo_region_t *region;

  r.x = 0;
  r.y = 0;
  r.width = surface->width;
  r.height = surface->height;

  region = cairo_region_create_rectangle (&r);
  remove_child_area (surface, TRUE, region);

  if (merge && surface->input_shape)
    cairo_region_subtract (region, surface->input_shape);

  cairo_region_xor_rectangle (region, &r);

  gdk_surface_input_shape_combine_region (surface, region, 0, 0);
}


/**
 * gdk_surface_set_child_input_shapes:
 * @surface: a #GdkSurface
 *
 * Sets the input shape mask of @surface to the union of input shape masks
 * for all children of @surface, ignoring the input shape mask of @surface
 * itself. Contrast with gdk_surface_merge_child_input_shapes() which includes
 * the input shape mask of @surface in the masks to be merged.
 **/
void
gdk_surface_set_child_input_shapes (GdkSurface *surface)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  do_child_input_shapes (surface, FALSE);
}

/**
 * gdk_surface_set_pass_through:
 * @surface: a #GdkSurface
 * @pass_through: a boolean
 *
 * Sets whether input to the surface is passed through to the surface
 * below.
 *
 * The default value of this is %FALSE, which means that pointer
 * events that happen inside the surface are send first to the surface,
 * but if the event is not selected by the event mask then the event
 * is sent to the parent surface, and so on up the hierarchy.
 *
 * If @pass_through is %TRUE then such pointer events happen as if the
 * surface wasn't there at all, and thus will be sent first to any
 * surfaces below @surface. This is useful if the surface is used in a
 * transparent fashion. In the terminology of the web this would be called
 * "pointer-events: none".
 *
 * Note that a surface with @pass_through %TRUE can still have a subsurface
 * without pass through, so you can get events on a subset of a surface. And in
 * that cases you would get the in-between related events such as the pointer
 * enter/leave events on its way to the destination surface.
 **/
void
gdk_surface_set_pass_through (GdkSurface *surface,
                              gboolean   pass_through)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  surface->pass_through = !!pass_through;
}

/**
 * gdk_surface_get_pass_through:
 * @surface: a #GdkSurface
 *
 * Returns whether input to the surface is passed through to the surface
 * below.
 *
 * See gdk_surface_set_pass_through() for details
 **/
gboolean
gdk_surface_get_pass_through (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), FALSE);

  return surface->pass_through;
}

/**
 * gdk_surface_merge_child_input_shapes:
 * @surface: a #GdkSurface
 *
 * Merges the input shape masks for any child surfaces into the
 * input shape mask for @surface. i.e. the union of all input masks
 * for @surface and its children will become the new input mask
 * for @surface. See gdk_surface_input_shape_combine_region().
 *
 * This function is distinct from gdk_surface_set_child_input_shapes()
 * because it includes @surface’s input shape mask in the set of
 * shapes to be merged.
 **/
void
gdk_surface_merge_child_input_shapes (GdkSurface *surface)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));

  do_child_input_shapes (surface, TRUE);
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

/**
 * gdk_surface_is_input_only:
 * @surface: a toplevel #GdkSurface
 *
 * Determines whether or not the surface is an input only surface.
 *
 * Returns: %TRUE if @surface is input only
 */
gboolean
gdk_surface_is_input_only (GdkSurface *surface)
{
  g_return_val_if_fail (GDK_IS_SURFACE (surface), FALSE);

  return surface->input_only;
}

/* Gets the toplevel for a surface as used for events,
   i.e. including offscreen parents going up to the native
   toplevel */
static GdkSurface *
get_event_toplevel (GdkSurface *surface)
{
  GdkSurface *parent;

  while ((parent = surface->parent) != NULL)
    surface = parent;

  return surface;
}

gboolean
_gdk_surface_event_parent_of (GdkSurface *parent,
                              GdkSurface *child)
{
  GdkSurface *w;

  w = child;
  while (w != NULL)
    {
      if (w == parent)
        return TRUE;

      w = w->parent;
    }

  return FALSE;
}

static void
update_cursor (GdkDisplay *display,
               GdkDevice  *device)
{
  GdkSurface *cursor_surface, *parent, *toplevel;
  GdkSurface *pointer_surface;
  GdkPointerSurfaceInfo *pointer_info;
  GdkDeviceGrabInfo *grab;
  GdkCursor *cursor;

  pointer_info = _gdk_display_get_pointer_info (display, device);
  pointer_surface = pointer_info->surface_under_pointer;

  /* We ignore the serials here and just pick the last grab
     we've sent, as that would shortly be used anyway. */
  grab = _gdk_display_get_last_device_grab (display, device);
  if (/* have grab */
      grab != NULL &&
      /* the pointer is not in a descendant of the grab surface */
      !_gdk_surface_event_parent_of (grab->surface, pointer_surface))
    {
      /* use the cursor from the grab surface */
      cursor_surface = grab->surface;
    }
  else
    {
      /* otherwise use the cursor from the pointer surface */
      cursor_surface = pointer_surface;
    }

  /* Find the first surface with the cursor actually set, as
     the cursor is inherited from the parent */
  while (cursor_surface->cursor == NULL &&
         !g_hash_table_contains (cursor_surface->device_cursor, device) &&
         (parent = cursor_surface->parent) != NULL)
    cursor_surface = parent;

  cursor = g_hash_table_lookup (cursor_surface->device_cursor, device);

  if (!cursor)
    cursor = cursor_surface->cursor;

  /* Set all cursors on toplevel, otherwise its tricky to keep track of
   * which native surface has what cursor set. */
  toplevel = get_event_toplevel (pointer_surface);
  GDK_DEVICE_GET_CLASS (device)->set_surface_cursor (device, toplevel, cursor);
}

static gboolean
point_in_surface (GdkSurface *surface,
                  gdouble    x,
                  gdouble    y)
{
  return
    x >= 0 && x < surface->width &&
    y >= 0 && y < surface->height &&
    (surface->input_shape == NULL ||
     cairo_region_contains_point (surface->input_shape,
                          x, y));
}

/* Same as point_in_surface, except it also takes pass_through and its
   interaction with child surfaces into account */
static gboolean
point_in_input_surface (GdkSurface *surface,
                        gdouble    x,
                        gdouble    y,
                        GdkSurface **input_surface,
                        gdouble   *input_surface_x,
                        gdouble   *input_surface_y)
{
  GdkSurface *sub;
  double child_x, child_y;
  GList *l;

  if (!point_in_surface (surface, x, y))
    return FALSE;

  if (!surface->pass_through)
    {
      if (input_surface)
        {
          *input_surface = surface;
          *input_surface_x = x;
          *input_surface_y = y;
        }
      return TRUE;
    }

  /* For pass-through, must be over a child input surface */

  /* Children is ordered in reverse stack order, i.e. first is topmost */
  for (l = surface->children; l != NULL; l = l->next)
    {
      sub = l->data;

      if (!GDK_SURFACE_IS_MAPPED (sub))
        continue;

      gdk_surface_coords_from_parent ((GdkSurface *)sub,
                                     x, y,
                                     &child_x, &child_y);
      if (point_in_input_surface (sub, child_x, child_y,
                                 input_surface, input_surface_x, input_surface_y))
        {
          if (input_surface)
            gdk_surface_coords_to_parent (sub,
                                         *input_surface_x,
                                         *input_surface_y,
                                         input_surface_x,
                                         input_surface_y);
          return TRUE;
        }
    }

  return FALSE;
}

GdkSurface *
_gdk_surface_find_child_at (GdkSurface *surface,
                            double     x,
                            double     y)
{
  GdkSurface *sub;
  double child_x, child_y;
  GList *l;

  if (point_in_surface (surface, x, y))
    {
      /* Children is ordered in reverse stack order, i.e. first is topmost */
      for (l = surface->children; l != NULL; l = l->next)
        {
          sub = l->data;

          if (!GDK_SURFACE_IS_MAPPED (sub))
            continue;

          gdk_surface_coords_from_parent ((GdkSurface *)sub,
                                         x, y,
                                         &child_x, &child_y);
          if (point_in_input_surface (sub, child_x, child_y,
                                     NULL, NULL, NULL))
            return (GdkSurface *)sub;
        }
    }

  return NULL;
}

GdkSurface *
_gdk_surface_find_descendant_at (GdkSurface *surface,
                                 gdouble    x,
                                 gdouble    y,
                                 gdouble   *found_x,
                                 gdouble   *found_y)
{
  GdkSurface *sub, *input_surface;
  gdouble child_x, child_y;
  GList *l;
  gboolean found;

  if (point_in_surface (surface, x, y))
    {
      do
        {
          found = FALSE;
          /* Children is ordered in reverse stack order, i.e. first is topmost */
          for (l = surface->children; l != NULL; l = l->next)
            {
              sub = l->data;

              if (!GDK_SURFACE_IS_MAPPED (sub))
                continue;

              gdk_surface_coords_from_parent ((GdkSurface *)sub,
                                             x, y,
                                             &child_x, &child_y);
              if (point_in_input_surface (sub, child_x, child_y,
                                         &input_surface, &child_x, &child_y))
                {
                  x = child_x;
                  y = child_y;
                  surface = input_surface;
                  found = TRUE;
                  break;
                }
            }
        }
      while (found);
    }
  else
    {
      /* Not in surface at all */
      surface = NULL;
    }

  if (found_x)
    *found_x = x;
  if (found_y)
    *found_y = y;

  return surface;
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
  GdkDisplay *display;
  GdkSurface *toplevel;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (GDK_SURFACE_DESTROYED (surface))
    return;

  toplevel = get_event_toplevel (surface);
  display = gdk_surface_get_display (surface);

  if (toplevel)
    {
      if (GDK_SURFACE_IMPL_GET_CLASS (toplevel->impl)->beep (toplevel))
        return;
    }

  /* If surfaces fail to beep, we beep the display. */
  gdk_display_beep (display);
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

#ifdef DEBUG_SURFACE_PRINTING

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#endif

static void
gdk_surface_print (GdkSurface *surface,
                  int indent)
{
  char *s;
  const char *surface_types[] = {
    "root",
    "toplevel",
    "child",
    "dialog",
    "temp",
    "foreign",
    "subsurface"
  };

  g_print ("%*s%p: [%s] %d,%d %dx%d", indent, "", surface,
           surface->user_data ? g_type_name_from_instance (surface->user_data) : "no widget",
           surface->x, surface->y,
           surface->width, surface->height
           );

  if (gdk_surface_has_impl (surface))
    {
#ifdef GDK_WINDOWING_X11
      g_print (" impl(0x%lx)", gdk_x11_surface_get_xid (window));
#endif
    }

  if (surface->surface_type != GDK_SURFACE_CHILD)
    g_print (" %s", surface_types[surface->surface_type]);

  if (surface->input_only)
    g_print (" input-only");

  if (!gdk_surface_is_visible ((GdkSurface *)surface))
    g_print (" hidden");

  g_print (" abs[%d,%d]",
           surface->abs_x, surface->abs_y);

  if (surface->alpha != 255)
    g_print (" alpha[%d]",
           surface->alpha);

  s = print_region (surface->clip_region);
  g_print (" clipbox[%s]", s);

  g_print ("\n");
}


static void
gdk_surface_print_tree (GdkSurface *surface,
                        int indent,
                        gboolean include_input_only)
{
  GList *l;

  if (surface->input_only && !include_input_only)
    return;

  gdk_surface_print (surface, indent);

  for (l = surface->children; l != NULL; l = l->next)
    gdk_surface_print_tree (l->data, indent + 4, include_input_only);
}

#endif /* DEBUG_SURFACE_PRINTING */

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

#ifdef DEBUG_SURFACE_PRINTING
  if (event->any.type == GDK_KEY_PRESS &&
      (event->key.keyval == 0xa7 ||
       event->key.keyval == 0xbd))
    {
      gdk_surface_print_tree (event_surface, 0, event->key.keyval == 0xbd);
    }
#endif

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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->focus (surface, timestamp);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->set_type_hint (surface, hint);
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
  return GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->get_type_hint (surface);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->set_modal_hint (surface, modal);
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

  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->set_geometry_hints (surface, geometry, geom_mask);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->set_title (surface, title);
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
  GdkSurfaceImplClass *klass = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);

  if (klass->set_startup_id)
    klass->set_startup_id (surface, startup_id);
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

  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->set_transient_for (surface, parent);
}

/**
 * gdk_surface_get_root_origin:
 * @surface: a toplevel #GdkSurface
 * @x: (out): return location for X position of surface frame
 * @y: (out): return location for Y position of surface frame
 *
 * Obtains the top-left corner of the window manager frame in root
 * surface coordinates.
 *
 **/
void
gdk_surface_get_root_origin (GdkSurface *surface,
                             gint      *x,
                             gint      *y)
{
  GdkRectangle rect;

  gdk_surface_get_frame_extents (surface, &rect);

  if (x)
    *x = rect.x;

  if (y)
    *y = rect.y;
}

/**
 * gdk_surface_get_frame_extents:
 * @surface: a toplevel #GdkSurface
 * @rect: (out): rectangle to fill with bounding box of the surface frame
 *
 * Obtains the bounding box of the surface, including window manager
 * titlebar/borders if any. The frame position is given in root window
 * coordinates. To get the position of the surface itself (rather than
 * the frame) in root window coordinates, use gdk_surface_get_origin().
 *
 **/
void
gdk_surface_get_frame_extents (GdkSurface    *surface,
                               GdkRectangle *rect)
{
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->get_frame_extents (surface, rect);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->set_accept_focus (surface, accept_focus);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->set_focus_on_map (surface, focus_on_map);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->set_icon_list (surface, textures);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->set_icon_name (surface, name);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->iconify (surface);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->deiconify (surface);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->stick (surface);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->unstick (surface);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->maximize (surface);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->unmaximize (surface);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->fullscreen (surface);
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
  g_return_if_fail (gdk_monitor_get_display (monitor) == gdk_surface_get_display (surface));
  g_return_if_fail (gdk_monitor_is_valid (monitor));

  if (GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->fullscreen_on_monitor != NULL)
    GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->fullscreen_on_monitor (surface, monitor);
  else
    GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->fullscreen (surface);
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
  GdkSurfaceImplClass *impl_class;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (surface->fullscreen_mode != mode)
    {
      surface->fullscreen_mode = mode;

      impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);
      if (impl_class->apply_fullscreen_mode != NULL)
        impl_class->apply_fullscreen_mode (surface);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->unfullscreen (surface);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->set_keep_above (surface, setting);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->set_keep_below (surface, setting);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->set_decorations (surface, decorations);
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
  return GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->get_decorations (surface, decorations);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->set_functions (surface, functions);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->begin_resize_drag (surface, edge, device, button, x, y, timestamp);
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
  GdkDisplay *display;
  GdkDevice *device;

  display = gdk_surface_get_display (surface);
  device = gdk_seat_get_pointer (gdk_display_get_default_seat (display));
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->begin_move_drag (surface,
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
  GdkDisplay *display;
  GdkDevice *device;

  display = gdk_surface_get_display (surface);
  device = gdk_seat_get_pointer (gdk_display_get_default_seat (display));
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

  if (gdk_surface_has_impl (surface))
    GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->set_opacity (surface, opacity);
  else
    {
      recompute_visible_regions (surface, FALSE);
      gdk_surface_invalidate_rect (surface, NULL);
    }
}

/* This function is called when the XWindow is really gone.
 */
void
gdk_surface_destroy_notify (GdkSurface *surface)
{
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->destroy_notify (surface);
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
  GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->register_dnd (surface);
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
  g_return_val_if_fail (gdk_surface_get_display (surface) == gdk_device_get_display (device), NULL);
  g_return_val_if_fail (GDK_IS_CONTENT_PROVIDER (content), NULL);

  return GDK_SURFACE_IMPL_GET_CLASS (surface->impl)->drag_begin (surface, device, content, actions, dx, dy);
}

static void
gdk_surface_flush_events (GdkFrameClock *clock,
                          void          *data)
{
  GdkSurface *surface;
  GdkDisplay *display;

  surface = GDK_SURFACE (data);

  display = gdk_surface_get_display (surface);
  _gdk_event_queue_flush (display);
  _gdk_display_pause_events (display);

  gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_RESUME_EVENTS);

  surface->frame_clock_events_paused = TRUE;
}

static void
gdk_surface_resume_events (GdkFrameClock *clock,
                           void          *data)
{
  GdkSurface *surface;
  GdkDisplay *display;

  surface = GDK_SURFACE (data);

  display = gdk_surface_get_display (surface);
  _gdk_display_unpause_events (display);

  surface->frame_clock_events_paused = FALSE;
}

static void
gdk_surface_set_frame_clock (GdkSurface     *surface,
                             GdkFrameClock *clock)
{
  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (clock == NULL || GDK_IS_FRAME_CLOCK (clock));
  g_return_if_fail (clock == NULL || gdk_surface_is_toplevel (surface));

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
                        "paint",
                        G_CALLBACK (gdk_surface_paint_on_clock),
                        surface);
      g_signal_connect (G_OBJECT (clock),
                        "resume-events",
                        G_CALLBACK (gdk_surface_resume_events),
                        surface);
    }

  if (surface->frame_clock)
    {
      if (surface->frame_clock_events_paused)
        gdk_surface_resume_events (surface->frame_clock, G_OBJECT (surface));

      g_signal_handlers_disconnect_by_func (G_OBJECT (surface->frame_clock),
                                            G_CALLBACK (gdk_surface_flush_events),
                                            surface);
      g_signal_handlers_disconnect_by_func (G_OBJECT (surface->frame_clock),
                                            G_CALLBACK (gdk_surface_paint_on_clock),
                                            surface);
      g_signal_handlers_disconnect_by_func (G_OBJECT (surface->frame_clock),
                                            G_CALLBACK (gdk_surface_resume_events),
                                            surface);
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
  GdkSurface *toplevel;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);

  toplevel = gdk_surface_get_toplevel (surface);

  return toplevel->frame_clock;
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
  GdkSurfaceImplClass *impl_class;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), 1);

  if (GDK_SURFACE_DESTROYED (surface))
    return 1;

  impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);

  if (impl_class->get_scale_factor)
    return impl_class->get_scale_factor (surface);

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
  GdkSurfaceImplClass *impl_class;
  gint scale;

  g_return_if_fail (GDK_IS_SURFACE (surface));

  if (surface->impl_surface == surface)
    {
      impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);

      if (impl_class->get_unscaled_size)
        {
          impl_class->get_unscaled_size (surface, unscaled_width, unscaled_height);
          return;
        }
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
  GdkSurfaceImplClass *impl_class;

  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (!GDK_SURFACE_DESTROYED (surface));

  if (cairo_region_equal (surface->opaque_region, region))
    return;

  g_clear_pointer (&surface->opaque_region, cairo_region_destroy);

  if (region != NULL)
    surface->opaque_region = cairo_region_reference (region);

  impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);

  if (impl_class->set_opaque_region)
    impl_class->set_opaque_region (surface, region);
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
  GdkSurfaceImplClass *impl_class;

  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (!GDK_SURFACE_DESTROYED (surface));
  g_return_if_fail (left >= 0 && right >= 0 && top >= 0 && bottom >= 0);

  surface->shadow_top = top;
  surface->shadow_left = left;
  surface->shadow_right = right;
  surface->shadow_bottom = bottom;

  impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);

  if (impl_class->set_shadow_width)
    impl_class->set_shadow_width (surface, left, right, top, bottom);
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
  GdkSurfaceImplClass *impl_class;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), FALSE);
  g_return_val_if_fail (!GDK_SURFACE_DESTROYED (surface), FALSE);

  impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);

  if (impl_class->show_window_menu)
    return impl_class->show_window_menu (surface, event);
  else
    return FALSE;
}

gboolean
gdk_surface_supports_edge_constraints (GdkSurface *surface)
{
  GdkSurfaceImplClass *impl_class;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), FALSE);
  g_return_val_if_fail (!GDK_SURFACE_DESTROYED (surface), FALSE);

  impl_class = GDK_SURFACE_IMPL_GET_CLASS (surface->impl);

  if (impl_class->supports_edge_constraints)
    return impl_class->supports_edge_constraints (surface);
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

  /* We only really send the event to toplevels, since
   * all the surface states don't apply to non-toplevels.
   * Non-toplevels do use the GDK_SURFACE_STATE_WITHDRAWN flag
   * internally so we needed to update surface->state.
   */
  switch (surface->surface_type)
    {
    case GDK_SURFACE_TOPLEVEL:
    case GDK_SURFACE_TEMP: /* ? */
      g_object_notify_by_pspec (G_OBJECT (surface), properties[PROP_STATE]);
      break;
    case GDK_SURFACE_CHILD:
    default:
      break;
    }

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

gboolean
gdk_surface_handle_event (GdkEvent *event)
{
  gboolean handled = FALSE;
  if (gdk_event_get_event_type (event) == GDK_CONFIGURE)
    {
      g_signal_emit (gdk_event_get_surface (event), signals[SIZE_CHANGED], 0,
                     event->configure.width, event->configure.height);
      handled = TRUE;
    }
  else
    {
      g_signal_emit (gdk_event_get_surface (event), signals[EVENT], 0, event, &handled);
    }

  return handled;
}
