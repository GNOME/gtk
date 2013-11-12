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

typedef struct _GtkBubbleWindowPrivate GtkBubbleWindowPrivate;

enum {
  PROP_RELATIVE_TO = 1,
  PROP_POINTING_TO,
  PROP_POSITION
};

struct _GtkBubbleWindowPrivate
{
  GdkDevice *device;
  GtkWidget *widget;
  GtkWindow *window;
  cairo_rectangle_int_t pointing_to;
  gint win_x;
  gint win_y;
  guint has_pointing_to    : 1;
  guint preferred_position : 2;
  guint final_position     : 2;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkBubbleWindow, _gtk_bubble_window, GTK_TYPE_BIN)

static void
_gtk_bubble_window_init (GtkBubbleWindow *window)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (window);
  gtk_widget_set_has_window (widget, TRUE);
  window->priv = _gtk_bubble_window_get_instance_private (window);
  gtk_style_context_add_class (gtk_widget_get_style_context (widget),
                               GTK_STYLE_CLASS_OSD);
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
      g_value_set_object (value, priv->widget);
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

  _gtk_bubble_window_popdown (window);

  G_OBJECT_CLASS (_gtk_bubble_window_parent_class)->finalize (object);
}

static void
gtk_bubble_window_dispose (GObject *object)
{
  GtkBubbleWindow *window = GTK_BUBBLE_WINDOW (object);
  GtkBubbleWindowPrivate *priv = window->priv;

  if (priv->window)
    gtk_window_remove_popover (priv->window, GTK_WIDGET (object));

  priv->window = NULL;
  priv->widget = NULL;

  G_OBJECT_CLASS (_gtk_bubble_window_parent_class)->dispose (object);
}

static void
gtk_bubble_window_realize (GtkWidget *widget)
{
  GtkAllocation allocation;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GdkWindow *window;

  gtk_widget_get_allocation (widget, &allocation);

  attributes.x = 0;
  attributes.y = 0;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.event_mask =
    gtk_widget_get_events (widget) |
    GDK_BUTTON_MOTION_MASK |
    GDK_BUTTON_PRESS_MASK |
    GDK_BUTTON_RELEASE_MASK |
    GDK_EXPOSURE_MASK |
    GDK_ENTER_NOTIFY_MASK |
    GDK_LEAVE_NOTIFY_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;
  window = gdk_window_new (gtk_widget_get_parent_window (widget),
                           &attributes, attributes_mask);
  gtk_widget_set_window (widget, window);
  gtk_widget_register_window (widget, window);
  gtk_widget_set_realized (widget, TRUE);
}

static void
gtk_bubble_window_map (GtkWidget *widget)
{
  gdk_window_show (gtk_widget_get_window (widget));
  GTK_WIDGET_CLASS (_gtk_bubble_window_parent_class)->map (widget);
}

static void
gtk_bubble_window_unmap (GtkWidget *widget)
{
  gdk_window_hide (gtk_widget_get_window (widget));
  GTK_WIDGET_CLASS (_gtk_bubble_window_parent_class)->unmap (widget);
}

static void
gtk_bubble_window_get_pointed_to_coords (GtkBubbleWindow       *window,
                                         gint                  *x,
                                         gint                  *y,
                                         cairo_rectangle_int_t *rect_out)
{
  GtkBubbleWindowPrivate *priv = window->priv;
  cairo_rectangle_int_t rect;
  GtkAllocation window_alloc;

  rect = priv->pointing_to;
  gtk_widget_get_allocation (GTK_WIDGET (priv->window), &window_alloc);
  gtk_widget_translate_coordinates (priv->widget, GTK_WIDGET (priv->window),
                                    rect.x, rect.y, &rect.x, &rect.y);

  if (POS_IS_VERTICAL (priv->final_position))
    {
      *x = CLAMP (rect.x + (rect.width / 2), 0, window_alloc.width);
      *y = rect.y;

      if (priv->final_position == GTK_POS_BOTTOM)
        (*y) += rect.height;
    }
  else
    {
      *y = CLAMP (rect.y + (rect.height / 2), 0, window_alloc.height);
      *x = rect.x;

      if (priv->final_position == GTK_POS_RIGHT)
        (*x) += rect.width;
    }

  if (rect_out)
    *rect_out = rect;
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

  gtk_widget_shape_combine_region (GTK_WIDGET (window), region);
  cairo_region_destroy (region);
}

static void
gtk_bubble_window_update_position (GtkBubbleWindow *window)
{
  GtkAllocation allocation, window_alloc;
  GtkBubbleWindowPrivate *priv;
  cairo_rectangle_int_t rect;
  gint win_x, win_y, x, y;

  priv = window->priv;
  gtk_widget_get_allocation (GTK_WIDGET (window), &allocation);
  gtk_widget_get_allocation (GTK_WIDGET (priv->window), &window_alloc);
  priv->final_position = priv->preferred_position;
  rect = priv->pointing_to;

  gtk_bubble_window_get_pointed_to_coords (window, &x, &y, &rect);

  /* Check whether there's enough room on the
   * preferred side, move to the opposite one if not.
   */
  if (priv->preferred_position == GTK_POS_TOP && rect.y < allocation.height)
    priv->final_position = GTK_POS_BOTTOM;
  else if (priv->preferred_position == GTK_POS_BOTTOM &&
           rect.y > window_alloc.height - allocation.height)
    priv->final_position = GTK_POS_TOP;
  else if (priv->preferred_position == GTK_POS_LEFT && rect.x < allocation.width)
    priv->final_position = GTK_POS_RIGHT;
  else if (priv->preferred_position == GTK_POS_RIGHT &&
           rect.x > window_alloc.width - allocation.width)
    priv->final_position = GTK_POS_LEFT;

  if (POS_IS_VERTICAL (priv->final_position))
    {
      win_x = CLAMP (x - allocation.width / 2,
                     0, window_alloc.width - allocation.width);
      win_y = y;

      if (priv->final_position == GTK_POS_TOP)
        win_y -= allocation.height;
      else if (priv->final_position == GTK_POS_BOTTOM)
        win_y += rect.height;
    }
  else
    {
      win_y = CLAMP (y - allocation.height / 2,
		     0, window_alloc.height - allocation.height);
      win_x = x;

      if (priv->final_position == GTK_POS_LEFT)
        win_x -= allocation.width;
      else if (priv->final_position == GTK_POS_RIGHT)
        win_x += rect.width;
    }

  priv->win_x = win_x;
  priv->win_y = win_y;

  gtk_window_set_popover_position (priv->window,
                                   GTK_WIDGET (window),
                                   win_x, win_y);

  gtk_bubble_window_update_shape (window);
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

  if (gtk_widget_get_visible (widget))
    gtk_bubble_window_update_position (GTK_BUBBLE_WINDOW (widget));

  if (child)
    {
      GtkAllocation child_alloc;
      GtkBorder border;

      get_padding_and_border (widget, &border);

      child_alloc.x = allocation->x + border.left;
      child_alloc.y = allocation->y + border.top;
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
    {
      gdk_window_move_resize (gtk_widget_get_window (widget),
                              0, 0, allocation->width, allocation->height);
      gtk_bubble_window_update_shape (GTK_BUBBLE_WINDOW (widget));
    }
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

static void
_gtk_bubble_window_class_init (GtkBubbleWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gtk_bubble_window_set_property;
  object_class->get_property = gtk_bubble_window_get_property;
  object_class->finalize = gtk_bubble_window_finalize;
  object_class->dispose = gtk_bubble_window_dispose;

  widget_class->realize = gtk_bubble_window_realize;
  widget_class->map = gtk_bubble_window_map;
  widget_class->unmap = gtk_bubble_window_unmap;
  widget_class->get_preferred_width = gtk_bubble_window_get_preferred_width;
  widget_class->get_preferred_height = gtk_bubble_window_get_preferred_height;
  widget_class->size_allocate = gtk_bubble_window_size_allocate;
  widget_class->draw = gtk_bubble_window_draw;
  widget_class->button_press_event = gtk_bubble_window_button_press;
  widget_class->key_press_event = gtk_bubble_window_key_press;

  g_object_class_install_property (object_class,
                                   PROP_RELATIVE_TO,
                                   g_param_spec_object ("relative-to",
                                                        P_("Relative to"),
                                                        P_("Window the bubble window points to"),
                                                        GTK_TYPE_WIDGET,
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
                                      GtkWidget       *relative_to)
{
  GtkBubbleWindowPrivate *priv;

  priv = window->priv;

  if (priv->widget == relative_to)
    return;

  if (priv->window)
    {
      gtk_window_remove_popover (priv->window, GTK_WIDGET (window));
      priv->window = NULL;
    }

  priv->widget = relative_to;
  g_object_notify (G_OBJECT (window), "relative-to");

  if (priv->widget)
    priv->window =
      GTK_WINDOW (gtk_widget_get_ancestor (priv->widget, GTK_TYPE_WINDOW));

  if (priv->window)
    gtk_window_add_popover (priv->window, GTK_WIDGET (window));
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
_gtk_bubble_window_new (GtkWidget *relative_to)
{
  return g_object_new (GTK_TYPE_BUBBLE_WINDOW,
                       "relative-to", relative_to,
                       NULL);
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
                                    GtkWidget       *relative_to)
{
  g_return_if_fail (GTK_IS_BUBBLE_WINDOW (window));
  g_return_if_fail (GTK_IS_WIDGET (relative_to));

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
GtkWidget *
_gtk_bubble_window_get_relative_to (GtkBubbleWindow *window)
{
  GtkBubbleWindowPrivate *priv;

  g_return_val_if_fail (GTK_IS_BUBBLE_WINDOW (window), NULL);

  priv = window->priv;

  return priv->widget;
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
                          GtkWidget             *relative_to,
                          cairo_rectangle_int_t *pointing_to,
                          GtkPositionType        position)
{
  g_return_if_fail (GTK_IS_BUBBLE_WINDOW (window));
  g_return_if_fail (GTK_IS_WIDGET (relative_to));
  g_return_if_fail (position >= GTK_POS_LEFT && position <= GTK_POS_BOTTOM);
  g_return_if_fail (pointing_to != NULL);

  gtk_bubble_window_update_preferred_position (window, position);
  gtk_bubble_window_update_relative_to (window, relative_to);
  gtk_bubble_window_update_pointing_to (window, pointing_to);

  gtk_bubble_window_update_position (window);

  if (!gtk_widget_get_visible (GTK_WIDGET (window)))
    gtk_widget_show (GTK_WIDGET (window));
}

/*
 * gtk_bubble_window_popdown:
 * @window: a #GtkBubbleWindow
 *
 * Removes the window from the screen
 *
 * Since: 3.8
 */
void
_gtk_bubble_window_popdown (GtkBubbleWindow *window)
{
  g_return_if_fail (GTK_IS_BUBBLE_WINDOW (window));

  if (gtk_widget_get_visible (GTK_WIDGET (window)))
    gtk_widget_hide (GTK_WIDGET (window));
}
