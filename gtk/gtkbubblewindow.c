/* GTK - The GIMP Toolkit
 * Copyright © 2013 Carlos Garnacho <carlosg@gnome.org>
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
 * GtkBubbleWindow is a bubble-like context window, primarily mean for
 * context-dependent helpers on touch interfaces.
 *
 * In order to place a GtkBubbleWindow to point to some other area,
 * use gtk_bubble_window_set_relative_to(), gtk_bubble_window_set_pointing_to()
 * and gtk_bubble_window_set_position(). Although it is usually  more
 * convenient to use gtk_bubble_window_popup() which handles all of those
 * at once.
 *
 * By default, no grabs are performed on the GtkBubbleWindow, leaving
 * the popup/popdown semantics up to the caller, gtk_bubble_window_grab()
 * can be used to grab the window for a device pair, bringing #GtkMenu alike
 * popdown behavior by default on keyboard/pointer interaction. Grabs need
 * to be undone through gtk_bubble_window_ungrab().
 */

#include "config.h"
#include <gdk/gdk.h>
#include <cairo-gobject.h>
#include "gtkbubblewindowprivate.h"
#include "gtktypebuiltins.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtkintl.h"

#define TAIL_GAP_WIDTH 24
#define TAIL_HEIGHT    12

#define POS_IS_VERTICAL(p) ((p) == GTK_POS_TOP || (p) == GTK_POS_BOTTOM)

#define GRAB_EVENT_MASK                             \
  GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | \
  GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |       \
  GDK_POINTER_MOTION_MASK

typedef struct _GtkBubbleWindowPrivate GtkBubbleWindowPrivate;

enum {
  PROP_RELATIVE_TO = 1,
  PROP_POINTING_TO,
  PROP_POSITION
};

struct _GtkBubbleWindowPrivate
{
  GdkDevice *device;
  GdkWindow *relative_to;
  cairo_rectangle_int_t pointing_to;
  gint win_x;
  gint win_y;
  guint has_pointing_to    : 1;
  guint grabbed            : 1;
  guint preferred_position : 2;
  guint final_position     : 2;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkBubbleWindow, _gtk_bubble_window, GTK_TYPE_WINDOW)

static void
_gtk_bubble_window_init (GtkBubbleWindow *window)
{
  GtkWidget *widget;
  GdkScreen *screen;
  GdkVisual *visual;

  widget = GTK_WIDGET (window);
  window->priv = _gtk_bubble_window_get_instance_private (window);
  gtk_window_set_default_size (GTK_WINDOW (window),
                               TAIL_GAP_WIDTH, TAIL_GAP_WIDTH);
  gtk_widget_set_app_paintable (widget, TRUE);

  screen = gtk_widget_get_screen (widget);
  visual = gdk_screen_get_rgba_visual (screen);

  if (visual)
    gtk_widget_set_visual (widget, visual);

  gtk_style_context_add_class (gtk_widget_get_style_context (widget),
                               GTK_STYLE_CLASS_OSD);
}

static GObject *
gtk_bubble_window_constructor (GType                  type,
                               guint                  n_construct_properties,
                               GObjectConstructParam *construct_properties)
{
  GObject *object;

  object =
    G_OBJECT_CLASS (_gtk_bubble_window_parent_class)->constructor (type,
                                                                  n_construct_properties,
                                                                  construct_properties);
  g_object_set (object, "type", GTK_WINDOW_POPUP, NULL);

  return object;
}

static void
gtk_bubble_window_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_RELATIVE_TO:
      _gtk_bubble_window_set_relative_to (GTK_BUBBLE_WINDOW (object),
                                          g_value_get_object (value));
      break;
    case PROP_POINTING_TO:
      _gtk_bubble_window_set_pointing_to (GTK_BUBBLE_WINDOW (object),
                                          g_value_get_boxed (value));
      break;
    case PROP_POSITION:
      _gtk_bubble_window_set_position (GTK_BUBBLE_WINDOW (object),
                                       g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_bubble_window_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkBubbleWindowPrivate *priv = GTK_BUBBLE_WINDOW (object)->priv;

  switch (prop_id)
    {
    case PROP_RELATIVE_TO:
      g_value_set_object (value, priv->relative_to);
      break;
    case PROP_POINTING_TO:
      g_value_set_boxed (value, &priv->pointing_to);
      break;
    case PROP_POSITION:
      g_value_set_enum (value, priv->preferred_position);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_bubble_window_finalize (GObject *object)
{
  GtkBubbleWindow *window = GTK_BUBBLE_WINDOW (object);
  GtkBubbleWindowPrivate *priv = window->priv;

  _gtk_bubble_window_popdown (window);

  if (priv->relative_to)
    g_object_unref (priv->relative_to);

  G_OBJECT_CLASS (_gtk_bubble_window_parent_class)->finalize (object);
}

static void
gtk_bubble_window_get_pointed_to_coords (GtkBubbleWindow       *window,
                                         gint                  *x,
                                         gint                  *y,
                                         cairo_rectangle_int_t *root_rect)
{
  GtkBubbleWindowPrivate *priv = window->priv;
  cairo_rectangle_int_t rect;
  GdkScreen *screen;

  rect = priv->pointing_to;
  screen = gtk_widget_get_screen (GTK_WIDGET (window));

  if (priv->relative_to)
    gdk_window_get_root_coords (priv->relative_to,
                                rect.x, rect.y, &rect.x, &rect.y);

  if (POS_IS_VERTICAL (priv->final_position))
    {
      *x = CLAMP (rect.x + (rect.width / 2),
                  0, gdk_screen_get_width (screen));
      *y = rect.y;

      if (priv->final_position == GTK_POS_BOTTOM)
        (*y) += rect.height;
    }
  else
    {
      *y = CLAMP (rect.y + (rect.height / 2),
                  0, gdk_screen_get_height (screen));
      *x = rect.x;

      if (priv->final_position == GTK_POS_RIGHT)
        (*x) += rect.width;
    }

  if (root_rect)
    *root_rect = rect;
}

static void
gtk_bubble_window_get_gap_coords (GtkBubbleWindow *window,
                                  gint            *initial_x_out,
                                  gint            *initial_y_out,
                                  gint            *tip_x_out,
                                  gint            *tip_y_out,
                                  gint            *final_x_out,
                                  gint            *final_y_out,
                                  GtkPositionType *gap_side_out)
{
  GtkBubbleWindowPrivate *priv = window->priv;
  gint base, tip, x, y;
  gint initial_x, initial_y;
  gint tip_x, tip_y;
  gint final_x, final_y;
  GtkPositionType gap_side;
  GtkAllocation allocation;

  gtk_bubble_window_get_pointed_to_coords (window, &x, &y, NULL);
  gtk_widget_get_allocation (GTK_WIDGET (window), &allocation);

  base = tip = 0;
  gap_side = GTK_POS_LEFT;

  if (priv->final_position == GTK_POS_BOTTOM ||
      priv->final_position == GTK_POS_RIGHT)
    {
      base = TAIL_HEIGHT;
      tip = 0;

      gap_side = (priv->final_position == GTK_POS_BOTTOM) ? GTK_POS_TOP : GTK_POS_LEFT;
    }
  else if (priv->final_position == GTK_POS_TOP)
    {
      base = allocation.height - TAIL_HEIGHT;
      tip = allocation.height;
      gap_side = GTK_POS_BOTTOM;
    }
  else if (priv->final_position == GTK_POS_LEFT)
    {
      base = allocation.width - TAIL_HEIGHT;
      tip = allocation.width;
      gap_side = GTK_POS_RIGHT;
    }

  if (POS_IS_VERTICAL (priv->final_position))
    {
      initial_x = CLAMP (x - priv->win_x - TAIL_GAP_WIDTH / 2,
                         0, allocation.width - TAIL_GAP_WIDTH);
      initial_y = base;

      tip_x = CLAMP (x - priv->win_x, 0, allocation.width);
      tip_y = tip;

      final_x = CLAMP (x - priv->win_x + TAIL_GAP_WIDTH / 2,
                       TAIL_GAP_WIDTH, allocation.width);
      final_y = base;
    }
  else
    {
      initial_x = base;
      initial_y = CLAMP (y - priv->win_y - TAIL_GAP_WIDTH / 2,
                         0, allocation.height - TAIL_GAP_WIDTH);

      tip_x = tip;
      tip_y = CLAMP (y - priv->win_y, 0, allocation.height);

      final_x = base;
      final_y = CLAMP (y - priv->win_y + TAIL_GAP_WIDTH / 2,
                       TAIL_GAP_WIDTH, allocation.height);
    }

  if (initial_x_out)
    *initial_x_out = initial_x;
  if (initial_y_out)
    *initial_y_out = initial_y;

  if (tip_x_out)
    *tip_x_out = tip_x;
  if (tip_y_out)
    *tip_y_out = tip_y;

  if (final_x_out)
    *final_x_out = final_x;
  if (final_y_out)
    *final_y_out = final_y;

  if (gap_side_out)
    *gap_side_out = gap_side;
}

static void
gtk_bubble_window_get_rect_coords (GtkBubbleWindow *window,
                                   gint            *x1_out,
                                   gint            *y1_out,
                                   gint            *x2_out,
                                   gint            *y2_out)
{
  GtkBubbleWindowPrivate *priv = window->priv;
  gint x1, x2, y1, y2;
  GtkAllocation allocation;

  x1 = y1 = x2 = y2 = 0;
  gtk_widget_get_allocation (GTK_WIDGET (window), &allocation);

  if (priv->final_position == GTK_POS_TOP)
    {
      x1 = 0;
      y1 = 0;
      x2 = allocation.width;
      y2 = allocation.height - TAIL_HEIGHT;
    }
  else if (priv->final_position == GTK_POS_BOTTOM)
    {
      x1 = 0;
      y1 = TAIL_HEIGHT;
      x2 = allocation.width;
      y2 = allocation.height;
    }
  else if (priv->final_position == GTK_POS_LEFT)
    {
      x1 = 0;
      y1 = 0;
      x2 = allocation.width - TAIL_HEIGHT;
      y2 = allocation.height;
    }
  else if (priv->final_position == GTK_POS_RIGHT)
    {
      x1 = TAIL_HEIGHT;
      y1 = 0;
      x2 = allocation.width;
      y2 = allocation.height;
    }

  if (x1_out)
    *x1_out = x1;
  if (y1_out)
    *y1_out = y1;
  if (x2_out)
    *x2_out = x2;
  if (y2_out)
    *y2_out = y2;
}

static void
gtk_bubble_window_apply_tail_path (GtkBubbleWindow *window,
                                   cairo_t         *cr)
{
  gint initial_x, initial_y;
  gint tip_x, tip_y;
  gint final_x, final_y;

  gtk_bubble_window_get_gap_coords (window,
                                    &initial_x, &initial_y,
                                    &tip_x, &tip_y,
                                    &final_x, &final_y,
                                    NULL);

  cairo_move_to (cr, initial_x, initial_y);
  cairo_line_to (cr, tip_x, tip_y);
  cairo_line_to (cr, final_x, final_y);
}

static void
gtk_bubble_window_apply_border_path (GtkBubbleWindow *window,
                                     cairo_t         *cr)
{
  GtkBubbleWindowPrivate *priv;
  GtkAllocation allocation;
  gint x1, y1, x2, y2;

  priv = window->priv;
  gtk_widget_get_allocation (GTK_WIDGET (window), &allocation);

  gtk_bubble_window_apply_tail_path (window, cr);
  gtk_bubble_window_get_rect_coords (window, &x1, &y1, &x2, &y2);

  if (priv->final_position == GTK_POS_TOP)
    {
      cairo_line_to (cr, x2, y2);
      cairo_line_to (cr, x2, y1);
      cairo_line_to (cr, x1, y1);
      cairo_line_to (cr, x1, y2);
    }
  else if (priv->final_position == GTK_POS_BOTTOM)
    {
      cairo_line_to (cr, x2, y1);
      cairo_line_to (cr, x2, y2);
      cairo_line_to (cr, x1, y2);
      cairo_line_to (cr, x1, y1);
    }
  else if (priv->final_position == GTK_POS_LEFT)
    {
      cairo_line_to (cr, x2, y2);
      cairo_line_to (cr, x1, y2);
      cairo_line_to (cr, x1, y1);
      cairo_line_to (cr, x2, y1);
    }
  else if (priv->final_position == GTK_POS_RIGHT)
    {
      cairo_line_to (cr, x1, y1);
      cairo_line_to (cr, x2, y1);
      cairo_line_to (cr, x2, y2);
      cairo_line_to (cr, x1, y2);
    }

  cairo_close_path (cr);
}

static void
gtk_bubble_window_update_shape (GtkBubbleWindow *window)
{
  cairo_surface_t *surface;
  cairo_region_t *region;
  GdkWindow *win;
  cairo_t *cr;

  win = gtk_widget_get_window (GTK_WIDGET (window));
  surface =
    gdk_window_create_similar_surface (win,
                                       CAIRO_CONTENT_COLOR_ALPHA,
                                       gdk_window_get_width (win),
                                       gdk_window_get_height (win));

  cr = cairo_create (surface);
  gtk_bubble_window_apply_border_path (window, cr);
  cairo_fill (cr);
  cairo_destroy (cr);

  region = gdk_cairo_region_create_from_surface (surface);
  cairo_surface_destroy (surface);

  if (!gtk_widget_is_composited (GTK_WIDGET (window)))
    gtk_widget_shape_combine_region (GTK_WIDGET (window), region);

  gtk_widget_input_shape_combine_region (GTK_WIDGET (window), region);
  cairo_region_destroy (region);
}

static void
gtk_bubble_window_update_position (GtkBubbleWindow *window)
{
  GtkBubbleWindowPrivate *priv;
  cairo_rectangle_int_t rect;
  GtkAllocation allocation;
  gint win_x, win_y, x, y;
  GdkScreen *screen;

  priv = window->priv;
  screen = gtk_widget_get_screen (GTK_WIDGET (window));
  gtk_widget_get_allocation (GTK_WIDGET (window), &allocation);
  priv->final_position = priv->preferred_position;
  rect = priv->pointing_to;

  gtk_bubble_window_get_pointed_to_coords (window, &x, &y, &rect);

  /* Check whether there's enough room on the
   * preferred side, move to the opposite one if not.
   */
  if (priv->preferred_position == GTK_POS_TOP && rect.y < allocation.height)
    priv->final_position = GTK_POS_BOTTOM;
  else if (priv->preferred_position == GTK_POS_BOTTOM &&
           rect.y > gdk_screen_get_height (screen) - allocation.height)
    priv->final_position = GTK_POS_TOP;
  else if (priv->preferred_position == GTK_POS_LEFT && rect.x < allocation.width)
    priv->final_position = GTK_POS_RIGHT;
  else if (priv->preferred_position == GTK_POS_RIGHT &&
           rect.x > gdk_screen_get_width (screen) - allocation.width)
    priv->final_position = GTK_POS_LEFT;

  if (POS_IS_VERTICAL (priv->final_position))
    {
      win_x = CLAMP (x - allocation.width / 2,
                     0, gdk_screen_get_width (screen) - allocation.width);
      win_y = y;

      if (priv->final_position == GTK_POS_TOP)
        win_y -= allocation.height;
    }
  else
    {
      win_y = CLAMP (y - allocation.height / 2,
                     0, gdk_screen_get_height (screen) - allocation.height);
      win_x = x;

      if (priv->final_position == GTK_POS_LEFT)
        win_x -= allocation.width;

    }

  priv->win_x = win_x;
  priv->win_y = win_y;
  gtk_window_move (GTK_WINDOW (window), win_x, win_y);

  gtk_widget_queue_resize (GTK_WIDGET (window));
}

static gboolean
gtk_bubble_window_draw (GtkWidget *widget,
                        cairo_t   *cr)
{
  GtkStyleContext *context;
  GtkAllocation allocation;
  GtkWidget *child;
  GtkBorder border;
  GdkRGBA border_color;
  gint rect_x1, rect_x2, rect_y1, rect_y2;
  gint initial_x, initial_y, final_x, final_y;
  gint gap_start, gap_end;
  GtkPositionType gap_side;
  GtkStateFlags state;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  gtk_widget_get_allocation (widget, &allocation);

  if (gtk_widget_is_composited (widget))
    {
      cairo_save (cr);
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_set_source_rgba (cr, 0, 0, 0, 0);
      cairo_paint (cr);
      cairo_restore (cr);
    }

  gtk_bubble_window_get_rect_coords (GTK_BUBBLE_WINDOW (widget),
                                     &rect_x1, &rect_y1,
                                     &rect_x2, &rect_y2);

  /* Render the rect background */
  gtk_render_background (context, cr,
                         rect_x1, rect_y1,
                         rect_x2 - rect_x1, rect_y2 - rect_y1);

  gtk_bubble_window_get_gap_coords (GTK_BUBBLE_WINDOW (widget),
                                    &initial_x, &initial_y,
                                    NULL, NULL,
                                    &final_x, &final_y,
                                    &gap_side);

  if (POS_IS_VERTICAL (gap_side))
    {
      gap_start = initial_x;
      gap_end = final_x;
    }
  else
    {
      gap_start = initial_y;
      gap_end = final_y;
    }

  /* Now render the frame, without the gap for the arrow tip */
  gtk_render_frame_gap (context, cr,
                        rect_x1, rect_y1,
                        rect_x2 - rect_x1, rect_y2 - rect_y1,
                        gap_side,
                        gap_start, gap_end);

  /* Clip to the arrow shape */
  cairo_save (cr);

  gtk_bubble_window_apply_tail_path (GTK_BUBBLE_WINDOW (widget), cr);
  cairo_clip (cr);

  /* Render the arrow background */
  gtk_render_background (context, cr,
                         0, 0,
                         allocation.width, allocation.height);

  /* Render the border of the arrow tip */
  gtk_style_context_get_border (context, state, &border);

  if (border.bottom > 0)
    {
      gtk_style_context_get_border_color (context, state, &border_color);
      gtk_bubble_window_apply_tail_path (GTK_BUBBLE_WINDOW (widget), cr);
      gdk_cairo_set_source_rgba (cr, &border_color);

      cairo_set_line_width (cr, border.bottom);
      cairo_stroke (cr);
    }

  /* We're done */
  cairo_restore (cr);

  child = gtk_bin_get_child (GTK_BIN (widget));

  if (child)
    gtk_container_propagate_draw (GTK_CONTAINER (widget), child, cr);

  return TRUE;
}

static void
get_padding_and_border (GtkWidget *widget,
                        GtkBorder *border)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder tmp;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, border);
  gtk_style_context_get_border (context, state, &tmp);
  border->top += tmp.top;
  border->right += tmp.right;
  border->bottom += tmp.bottom;
  border->left += tmp.left;
}

static void
gtk_bubble_window_get_preferred_width (GtkWidget *widget,
                                       gint      *minimum_width,
                                       gint      *natural_width)
{
  GtkBubbleWindowPrivate *priv;
  GtkWidget *child;
  gint min, nat;
  GtkBorder border;

  priv = GTK_BUBBLE_WINDOW (widget)->priv;
  child = gtk_bin_get_child (GTK_BIN (widget));
  min = nat = 0;

  if (child)
    gtk_widget_get_preferred_width (child, &min, &nat);

  get_padding_and_border (widget, &border);
  min += border.left + border.right;
  nat += border.left + border.right;

  if (!POS_IS_VERTICAL (priv->final_position))
    {
      min += TAIL_HEIGHT;
      nat += TAIL_HEIGHT;
    }

  if (minimum_width)
    *minimum_width = MAX (min, TAIL_GAP_WIDTH);

  if (natural_width)
    *natural_width = MAX (nat, TAIL_GAP_WIDTH);
}

static void
gtk_bubble_window_get_preferred_height (GtkWidget *widget,
                                        gint      *minimum_height,
                                        gint      *natural_height)
{
  GtkBubbleWindowPrivate *priv;
  GtkWidget *child;
  gint min, nat;
  GtkBorder border;

  priv = GTK_BUBBLE_WINDOW (widget)->priv;
  child = gtk_bin_get_child (GTK_BIN (widget));
  min = nat = 0;

  if (child)
    gtk_widget_get_preferred_height (child, &min, &nat);

  get_padding_and_border (widget, &border);
  min += border.top + border.bottom;
  nat += border.top + border.bottom;

  if (POS_IS_VERTICAL (priv->final_position))
    {
      min += TAIL_HEIGHT;
      nat += TAIL_HEIGHT;
    }

  if (minimum_height)
    *minimum_height = MAX (min, TAIL_GAP_WIDTH);

  if (natural_height)
    *natural_height = MAX (nat, TAIL_GAP_WIDTH);
}

static void
gtk_bubble_window_size_allocate (GtkWidget     *widget,
                                 GtkAllocation *allocation)
{
  GtkBubbleWindowPrivate *priv;
  GtkWidget *child;

  priv = GTK_BUBBLE_WINDOW (widget)->priv;
  gtk_widget_set_allocation (widget, allocation);
  child = gtk_bin_get_child (GTK_BIN (widget));

  if (child)
    {
      GtkAllocation child_alloc;
      GtkBorder border;

      get_padding_and_border (widget, &border);

      child_alloc.x = border.left;
      child_alloc.y = border.top;
      child_alloc.width = allocation->width - border.left - border.right;
      child_alloc.height = allocation->height - border.top - border.bottom;

      if (POS_IS_VERTICAL (priv->final_position))
        child_alloc.height -= TAIL_HEIGHT;
      else
        child_alloc.width -= TAIL_HEIGHT;

      if (priv->final_position == GTK_POS_BOTTOM)
        child_alloc.y += TAIL_HEIGHT;
      else if (priv->final_position == GTK_POS_RIGHT)
        child_alloc.x += TAIL_HEIGHT;

      gtk_widget_size_allocate (child, &child_alloc);
    }

  if (gtk_widget_get_realized (widget))
    gtk_bubble_window_update_shape (GTK_BUBBLE_WINDOW (widget));

  if (gtk_widget_get_visible (widget))
    gtk_bubble_window_update_position (GTK_BUBBLE_WINDOW (widget));
}

static gboolean
gtk_bubble_window_button_press (GtkWidget      *widget,
                                GdkEventButton *event)
{
  GtkWidget *child;

  child = gtk_bin_get_child (GTK_BIN (widget));

  if (child && event->window == gtk_widget_get_window (widget))
    {
      GtkAllocation child_alloc;

      gtk_widget_get_allocation (child, &child_alloc);

      if (event->x < child_alloc.x ||
          event->x > child_alloc.x + child_alloc.width ||
          event->y < child_alloc.y ||
          event->y > child_alloc.y + child_alloc.height)
        _gtk_bubble_window_popdown (GTK_BUBBLE_WINDOW (widget));
    }
  else
    _gtk_bubble_window_popdown (GTK_BUBBLE_WINDOW (widget));

  return GDK_EVENT_PROPAGATE;
}

static gboolean
gtk_bubble_window_key_press (GtkWidget   *widget,
                             GdkEventKey *event)
{
  if (event->keyval == GDK_KEY_Escape)
    {
      _gtk_bubble_window_popdown (GTK_BUBBLE_WINDOW (widget));
      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static gboolean
gtk_bubble_window_grab_broken (GtkWidget          *widget,
                               GdkEventGrabBroken *grab_broken)
{
  GtkBubbleWindow *window = GTK_BUBBLE_WINDOW (widget);
  GtkBubbleWindowPrivate *priv;
  GdkDevice *event_device;

  priv = window->priv;
  event_device = gdk_event_get_device ((GdkEvent *) grab_broken);

  if (event_device == priv->device ||
      event_device == gdk_device_get_associated_device (priv->device))
    _gtk_bubble_window_ungrab (window);

  return FALSE;
}

static void
gtk_bubble_window_grab_notify (GtkWidget *widget,
                               gboolean   was_grabbed)
{
  GtkBubbleWindow *window = GTK_BUBBLE_WINDOW (widget);
  GtkBubbleWindowPrivate *priv;

  priv = window->priv;

  if (priv->device && gtk_widget_device_is_shadowed (widget, priv->device))
    _gtk_bubble_window_ungrab (window);
}

static void
gtk_bubble_window_screen_changed (GtkWidget *widget,
                                  GdkScreen *previous_screen)
{
  GdkScreen *screen;
  GdkVisual *visual;

  screen = gtk_widget_get_screen (widget);
  visual = gdk_screen_get_rgba_visual (screen);

  if (visual)
    gtk_widget_set_visual (widget, visual);
}

static void
_gtk_bubble_window_class_init (GtkBubbleWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructor = gtk_bubble_window_constructor;
  object_class->set_property = gtk_bubble_window_set_property;
  object_class->get_property = gtk_bubble_window_get_property;
  object_class->finalize = gtk_bubble_window_finalize;

  widget_class->get_preferred_width = gtk_bubble_window_get_preferred_width;
  widget_class->get_preferred_height = gtk_bubble_window_get_preferred_height;
  widget_class->size_allocate = gtk_bubble_window_size_allocate;
  widget_class->draw = gtk_bubble_window_draw;
  widget_class->button_press_event = gtk_bubble_window_button_press;
  widget_class->key_press_event = gtk_bubble_window_key_press;
  widget_class->grab_broken_event = gtk_bubble_window_grab_broken;
  widget_class->grab_notify = gtk_bubble_window_grab_notify;
  widget_class->screen_changed = gtk_bubble_window_screen_changed;

  g_object_class_install_property (object_class,
                                   PROP_RELATIVE_TO,
                                   g_param_spec_object ("relative-to",
                                                        P_("Relative to"),
                                                        P_("Window the bubble window points to"),
                                                        GDK_TYPE_WINDOW,
                                                        GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_POINTING_TO,
                                   g_param_spec_boxed ("pointing-to",
                                                       P_("Pointing to"),
                                                       P_("Rectangle the bubble window points to"),
                                                       CAIRO_GOBJECT_TYPE_RECTANGLE_INT,
                                                       GTK_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_POSITION,
                                   g_param_spec_enum ("position",
                                                      P_("Position"),
                                                      P_("Position to place the bubble window"),
                                                      GTK_TYPE_POSITION_TYPE, GTK_POS_TOP,
                                                      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
gtk_bubble_window_update_relative_to (GtkBubbleWindow *window,
                                      GdkWindow       *relative_to)
{
  GtkBubbleWindowPrivate *priv;

  priv = window->priv;

  if (priv->relative_to == relative_to)
    return;

  if (priv->relative_to)
    g_object_unref (priv->relative_to);

  priv->relative_to = (relative_to) ? g_object_ref (relative_to) : NULL;
  g_object_notify (G_OBJECT (window), "relative-to");
}

static void
gtk_bubble_window_update_pointing_to (GtkBubbleWindow       *window,
                                      cairo_rectangle_int_t *pointing_to)
{
  GtkBubbleWindowPrivate *priv;

  priv = window->priv;
  priv->pointing_to = *pointing_to;
  priv->has_pointing_to = TRUE;
  g_object_notify (G_OBJECT (window), "pointing-to");
}

static void
gtk_bubble_window_update_preferred_position (GtkBubbleWindow *window,
                                             GtkPositionType  position)
{
  GtkBubbleWindowPrivate *priv;

  priv = window->priv;
  priv->preferred_position = position;
  g_object_notify (G_OBJECT (window), "position");
}

/*
 * gtk_bubble_window_new:
 *
 * Returns a newly created #GtkBubblewindow
 *
 * Returns: a new #GtkBubbleWindow
 *
 * Since: 3.8
 */
GtkWidget *
_gtk_bubble_window_new (void)
{
  return g_object_new (GTK_TYPE_BUBBLE_WINDOW, NULL);
}

/*
 * gtk_bubble_window_set_relative_to:
 * @window: a #GtkBubbleWindow
 * @relative_to: a #GdkWindow
 *
 * Sets the #GdkWindow to act as the origin of coordinates of
 * @window, or %NULL to use the root window. See
 * gtk_bubble_window_set_pointing_to()
 *
 * If @window is currently visible, it will be moved to reflect
 * this change.
 *
 * Since: 3.8
 */
void
_gtk_bubble_window_set_relative_to (GtkBubbleWindow *window,
                                    GdkWindow       *relative_to)
{
  g_return_if_fail (GTK_IS_BUBBLE_WINDOW (window));
  g_return_if_fail (!relative_to || GDK_IS_WINDOW (relative_to));

  gtk_bubble_window_update_relative_to (window, relative_to);

  if (gtk_widget_get_visible (GTK_WIDGET (window)))
    gtk_bubble_window_update_position (window);
}

/*
 * gtk_bubble_window_get_relative_to:
 * @window: a #GtkBubbleWindow
 *
 * Returns the #GdkWindow used as the origin of coordinates.
 * If @window is currently visible, it will be moved to reflect
 * this change.
 *
 * Returns: the #GdkWindow @window is placed upon
 *
 * Since: 3.8
 */
GdkWindow *
_gtk_bubble_window_get_relative_to (GtkBubbleWindow *window)
{
  GtkBubbleWindowPrivate *priv;

  g_return_val_if_fail (GTK_IS_BUBBLE_WINDOW (window), NULL);

  priv = window->priv;

  return priv->relative_to;
}

/*
 * gtk_bubble_window_set_pointing_to:
 * @window: a #GtkBubbleWindow
 * @rect: rectangle to point to
 *
 * Sets the rectangle that @window will point to, the coordinates
 * of this rectangle are relative to the #GdkWindow set through
 * gtk_bubble_window_set_relative_to().
 *
 * Since: 3.8
 */
void
_gtk_bubble_window_set_pointing_to (GtkBubbleWindow       *window,
                                    cairo_rectangle_int_t *rect)
{
  g_return_if_fail (GTK_IS_BUBBLE_WINDOW (window));
  g_return_if_fail (rect != NULL);

  gtk_bubble_window_update_pointing_to (window, rect);

  if (gtk_widget_get_visible (GTK_WIDGET (window)))
    gtk_bubble_window_update_position (window);
}

/*
 * gtk_bubble_window_get_pointing_to:
 * @window: a #GtkBubbleWindow
 * @rect: (out): location to store the rectangle
 *
 * If a rectangle to point to is set, this function will return
 * %TRUE and fill in @rect with the rectangle @window is currently
 * pointing to.
 *
 * Returns: %TRUE if a rectangle is set
 *
 * Since: 3.8
 */
gboolean
_gtk_bubble_window_get_pointing_to (GtkBubbleWindow       *window,
                                    cairo_rectangle_int_t *rect)
{
  GtkBubbleWindowPrivate *priv;

  g_return_val_if_fail (GTK_IS_BUBBLE_WINDOW (window), FALSE);

  priv = window->priv;

  if (rect)
    *rect = priv->pointing_to;

  return priv->has_pointing_to;
}

/*
 * gtk_bubble_window_set_position:
 * @window: a #GtkBubbleWindow
 * @position: preferred bubble position
 *
 * Sets the preferred position for @window to appear.
 * If @window is currently visible, it will be moved to reflect
 * this change.
 *
 * <note>
 *   This preference will be respected where possible, although
 *   on lack of space (eg. if close to the screen edges), the
 *   #GtkBubbleWindow may choose to appear on the opposite side
 * </note>
 *
 * Since: 3.8
 */
void
_gtk_bubble_window_set_position (GtkBubbleWindow *window,
                                 GtkPositionType  position)
{
  g_return_if_fail (GTK_IS_BUBBLE_WINDOW (window));
  g_return_if_fail (position >= GTK_POS_LEFT && position <= GTK_POS_BOTTOM);

  gtk_bubble_window_update_preferred_position (window, position);

  if (gtk_widget_get_visible (GTK_WIDGET (window)))
    gtk_bubble_window_update_position (window);
}

/*
 * gtk_bubble_window_get_position:
 * @window: a #GtkBubbleWindow
 *
 * Returns the preferred position to place @window
 *
 * Returns: The preferred position
 *
 * Since: 3.8
 */
GtkPositionType
_gtk_bubble_window_get_position (GtkBubbleWindow *window)
{
  GtkBubbleWindowPrivate *priv;

  g_return_val_if_fail (GTK_IS_BUBBLE_WINDOW (window), GTK_POS_TOP);

  priv = window->priv;

  return priv->preferred_position;
}

/*
 * gtk_bubble_window_popup:
 * @window: a #GtkBubbleWindow
 * @relative_to: #GdkWindow to position upon
 * @pointing_to: rectangle to point to, in @relative_to coordinates
 * @position: preferred position for @window
 *
 * This function sets atomically all #GtkBubbleWindow position
 * parameters, and shows/updates @window
 *
 * Since: 3.8
 */
void
_gtk_bubble_window_popup (GtkBubbleWindow       *window,
                          GdkWindow             *relative_to,
                          cairo_rectangle_int_t *pointing_to,
                          GtkPositionType        position)
{
  g_return_if_fail (GTK_IS_BUBBLE_WINDOW (window));
  g_return_if_fail (!relative_to || GDK_IS_WINDOW (relative_to));
  g_return_if_fail (position >= GTK_POS_LEFT && position <= GTK_POS_BOTTOM);
  g_return_if_fail (pointing_to != NULL);

  gtk_bubble_window_update_preferred_position (window, position);
  gtk_bubble_window_update_relative_to (window, relative_to);
  gtk_bubble_window_update_pointing_to (window, pointing_to);

  if (!gtk_widget_get_visible (GTK_WIDGET (window)))
    gtk_widget_show (GTK_WIDGET (window));

  gtk_bubble_window_update_position (window);
}

/*
 * gtk_bubble_window_popdown:
 * @window: a #GtkBubbleWindow
 *
 * Removes the window from the screen
 *
 * <note>
 *   If a grab was previously added through gtk_bubble_window_grab(),
 *   the grab will be removed by this function.
 * </note>
 *
 * Since: 3.8
 */
void
_gtk_bubble_window_popdown (GtkBubbleWindow *window)
{
  GtkBubbleWindowPrivate *priv = window->priv;

  g_return_if_fail (GTK_IS_BUBBLE_WINDOW (window));

  if (priv->grabbed)
    _gtk_bubble_window_ungrab (window);

  if (gtk_widget_get_visible (GTK_WIDGET (window)))
    gtk_widget_hide (GTK_WIDGET (window));
}

/*
 * gtk_bubble_window_grab:
 * @window: a #GtkBubbleWindow
 * @device: a master #GdkDevice
 * @activate_time: timestamp to perform the grab
 *
 * This function performs GDK and GTK+ grabs on @device and
 * its paired #GdkDevice. After this call all pointer/keyboard
 * events will be handled by @window.
 *
 * Calling this also brings in a #GtkMenu alike behavior, clicking
 * outside the #GtkBubbleWindow or pressing the Escape key will
 * popdown the menu by default.
 *
 * <note>
 *   If there was a previous grab, it will be undone before doing
 *   the requested grab.
 * </note>
 *
 * Returns: %TRUE if the grab was successful
 *
 * Since: 3.8
 */
gboolean
_gtk_bubble_window_grab (GtkBubbleWindow *window,
                         GdkDevice       *device,
                         guint32          activate_time)
{
  GtkBubbleWindowPrivate *priv;
  GdkDevice *other_device;
  GdkWindow *grab_window;
  GdkGrabStatus status;

  g_return_val_if_fail (GTK_IS_BUBBLE_WINDOW (window), FALSE);
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);
  g_return_val_if_fail (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_MASTER, FALSE);

  priv = window->priv;

  if (!priv->has_pointing_to ||
      gdk_window_is_destroyed (priv->relative_to))
    return FALSE;

  if (priv->device)
    _gtk_bubble_window_ungrab (window);

  gtk_widget_realize (GTK_WIDGET (window));
  grab_window = gtk_widget_get_window (GTK_WIDGET (window));
  other_device = gdk_device_get_associated_device (device);

  status = gdk_device_grab (device, grab_window,
                            GDK_OWNERSHIP_WINDOW, TRUE, GRAB_EVENT_MASK,
                            NULL, activate_time);

  if (status == GDK_GRAB_SUCCESS)
    {
      status = gdk_device_grab (other_device, grab_window,
                                GDK_OWNERSHIP_WINDOW, TRUE, GRAB_EVENT_MASK,
                                NULL, activate_time);

      /* Ungrab the first device on error */
      if (status != GDK_GRAB_SUCCESS)
        gdk_device_ungrab (device, activate_time);
    }

  if (status == GDK_GRAB_SUCCESS)
    {
      gtk_device_grab_add (GTK_WIDGET (window), device, TRUE);
      priv->device = device;
    }

  return status == GDK_GRAB_SUCCESS;
}

/*
 * gtk_bubble_window_ungrab:
 * @window: a #GtkBubbleWindow
 *
 * This functions undoes a grab added through gtk_bubble_window_grab()
 * in this @window,
 *
 * Since: 3.8
 */
void
_gtk_bubble_window_ungrab (GtkBubbleWindow *window)
{
  GtkBubbleWindowPrivate *priv;

  g_return_if_fail (GTK_IS_BUBBLE_WINDOW (window));

  priv = window->priv;

  if (!priv->device)
    return;

  gdk_device_ungrab (priv->device, GDK_CURRENT_TIME);
  gdk_device_ungrab (gdk_device_get_associated_device (priv->device),
                     GDK_CURRENT_TIME);
  gtk_device_grab_remove (GTK_WIDGET (window), priv->device);
  priv->device = NULL;
}
