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

/**
 * SECTION:gtkpopover
 * @Short_description: Context dependent bubbles
 * @Title: GtkPopover
 *
 * GtkPopover is a bubble-like context window, primarily meant to
 * provide context-dependent information or options. Popovers are
 * attached to a widget, passed at construction time on gtk_popover_new(),
 * or updated afterwards through gtk_popover_set_relative_to(), by
 * default they will point to the whole widget area, although this
 * behavior can be changed through gtk_popover_set_pointing_to().
 *
 * The position of a popover relative to the widget it is attached to
 * can also be changed through gtk_popover_set_position().
 *
 * By default, #GtkPopover performs a GTK+ grab, in order to ensure
 * input events get redirected to it while it is shown, and also so
 * the popover is dismissed on the expected situations (clicks outside
 * the popover, or the Esc key being pressed). If no such modal behavior
 * is desired on a popover, gtk_popover_set_modal() may be called on it
 * to tweak its behavior.
 *
 * Since: 3.12
 */

#include "config.h"
#include <gdk/gdk.h>
#include <cairo-gobject.h>
#include "gtkpopover.h"
#include "gtktypebuiltins.h"
#include "gtkmain.h"
#include "gtkwindowprivate.h"
#include "gtkprivate.h"
#include "gtkintl.h"

#define TAIL_GAP_WIDTH 24
#define TAIL_HEIGHT    12

#define POS_IS_VERTICAL(p) ((p) == GTK_POS_TOP || (p) == GTK_POS_BOTTOM)

typedef struct _GtkPopoverPrivate GtkPopoverPrivate;

enum {
  PROP_RELATIVE_TO = 1,
  PROP_POINTING_TO,
  PROP_POSITION,
  PROP_MODAL
};

struct _GtkPopoverPrivate
{
  GtkWidget *widget;
  GtkWindow *window;
  GtkWidget *prev_focus_widget;
  cairo_rectangle_int_t pointing_to;
  guint hierarchy_changed_id;
  guint size_allocate_id;
  guint unmap_id;
  guint has_pointing_to    : 1;
  guint preferred_position : 2;
  guint final_position     : 2;
  guint current_position   : 2;
  guint modal              : 1;
};

static GQuark quark_widget_popovers = 0;

static void gtk_popover_update_position    (GtkPopover *popover);
static void gtk_popover_update_relative_to (GtkPopover *popover,
                                            GtkWidget  *relative_to);

G_DEFINE_TYPE_WITH_PRIVATE (GtkPopover, gtk_popover, GTK_TYPE_BIN)

static void
gtk_popover_init (GtkPopover *popover)
{
  GtkPopoverPrivate *priv;
  GtkWidget *widget;

  widget = GTK_WIDGET (popover);
  gtk_widget_set_has_window (widget, TRUE);
  popover->priv = priv = gtk_popover_get_instance_private (popover);
  priv->modal = TRUE;
}

static void
gtk_popover_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_RELATIVE_TO:
      gtk_popover_set_relative_to (GTK_POPOVER (object),
                                   g_value_get_object (value));
      break;
    case PROP_POINTING_TO:
      gtk_popover_set_pointing_to (GTK_POPOVER (object),
                                   g_value_get_boxed (value));
      break;
    case PROP_POSITION:
      gtk_popover_set_position (GTK_POPOVER (object),
                                g_value_get_enum (value));
      break;
    case PROP_MODAL:
      gtk_popover_set_modal (GTK_POPOVER (object),
                             g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_popover_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GtkPopoverPrivate *priv = GTK_POPOVER (object)->priv;

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
    case PROP_MODAL:
      g_value_set_boolean (value, priv->modal);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_popover_finalize (GObject *object)
{
  GtkPopover *popover = GTK_POPOVER (object);
  GtkPopoverPrivate *priv = popover->priv;

  if (priv->widget)
    gtk_popover_update_relative_to (popover, NULL);

  G_OBJECT_CLASS (gtk_popover_parent_class)->finalize (object);
}

static void
gtk_popover_dispose (GObject *object)
{
  GtkPopover *popover = GTK_POPOVER (object);
  GtkPopoverPrivate *priv = popover->priv;

  if (priv->window)
    _gtk_window_remove_popover (priv->window, GTK_WIDGET (object));

  priv->window = NULL;

  if (priv->widget)
    gtk_popover_update_relative_to (popover, NULL);

  if (priv->prev_focus_widget)
    {
      g_object_unref (priv->prev_focus_widget);
      priv->prev_focus_widget = NULL;
    }

  gtk_widget_set_visible (GTK_WIDGET (object), FALSE);
  G_OBJECT_CLASS (gtk_popover_parent_class)->dispose (object);
}

static void
gtk_popover_realize (GtkWidget *widget)
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
gtk_popover_apply_modality (GtkPopover *popover,
                            gboolean    modal)
{
  GtkPopoverPrivate *priv;

  priv = popover->priv;

  if (modal)
    {
      GtkWidget *prev_focus;

      prev_focus = gtk_window_get_focus (priv->window);
      priv->prev_focus_widget = g_object_ref (prev_focus);
      gtk_grab_add (GTK_WIDGET (popover));
      gtk_widget_grab_focus (GTK_WIDGET (popover));
    }
  else
    {
      gtk_grab_remove (GTK_WIDGET (popover));

      /* Let prev_focus_widget regain focus */
      if (priv->prev_focus_widget)
        {
          gtk_widget_grab_focus (priv->prev_focus_widget);
          g_object_unref (priv->prev_focus_widget);
          priv->prev_focus_widget = NULL;
        }
    }
}

static void
gtk_popover_map (GtkWidget *widget)
{
  GtkPopoverPrivate *priv;

  priv = GTK_POPOVER (widget)->priv;
  GTK_WIDGET_CLASS (gtk_popover_parent_class)->map (widget);
  gdk_window_show (gtk_widget_get_window (widget));
  gtk_popover_update_position (GTK_POPOVER (widget));

  if (priv->modal)
    gtk_popover_apply_modality (GTK_POPOVER (widget), TRUE);
}

static void
gtk_popover_unmap (GtkWidget *widget)
{
  GtkPopoverPrivate *priv;

  priv = GTK_POPOVER (widget)->priv;

  if (priv->modal)
    gtk_popover_apply_modality (GTK_POPOVER (widget), FALSE);

  gdk_window_hide (gtk_widget_get_window (widget));
  GTK_WIDGET_CLASS (gtk_popover_parent_class)->unmap (widget);
}

static void
gtk_popover_get_pointed_to_coords (GtkPopover            *popover,
                                   cairo_rectangle_int_t *rect_out)
{
  GtkPopoverPrivate *priv = popover->priv;
  cairo_rectangle_int_t rect;

  if (!rect_out)
    return;

  gtk_popover_get_pointing_to (popover, &rect);
  gtk_widget_translate_coordinates (priv->widget, GTK_WIDGET (priv->window),
                                    rect.x, rect.y, &rect.x, &rect.y);
  *rect_out = rect;
}

static GtkPositionType
get_effective_position (GtkPopover      *popover,
                        GtkPositionType  pos)
{
  if (gtk_widget_get_direction (GTK_WIDGET (popover)) == GTK_TEXT_DIR_RTL)
    {
      if (pos == GTK_POS_LEFT)
        pos = GTK_POS_RIGHT;
      else if (pos == GTK_POS_RIGHT)
        pos = GTK_POS_LEFT;
    }

  return pos;
}

static void
get_margin (GtkWidget *widget,
            GtkBorder *border)
{
  GtkStyleContext *context;
  GtkStateFlags state;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);
  gtk_style_context_get_margin (context, state, border);
}

static void
gtk_popover_get_gap_coords (GtkPopover      *popover,
                            gint            *initial_x_out,
                            gint            *initial_y_out,
                            gint            *tip_x_out,
                            gint            *tip_y_out,
                            gint            *final_x_out,
                            gint            *final_y_out,
                            GtkPositionType *gap_side_out)
{
  GtkWidget *widget = GTK_WIDGET (popover);
  GtkPopoverPrivate *priv = popover->priv;
  cairo_rectangle_int_t rect;
  gint base, tip, tip_pos;
  gint initial_x, initial_y;
  gint tip_x, tip_y;
  gint final_x, final_y;
  GtkPositionType gap_side, pos;
  GtkAllocation allocation;
  gint border_radius;
  GtkBorder margin;

  gtk_popover_get_pointing_to (popover, &rect);
  gtk_widget_get_allocation (widget, &allocation);
  gtk_widget_translate_coordinates (priv->widget, widget,
                                    rect.x, rect.y, &rect.x, &rect.y);
  get_margin (widget, &margin);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    rect.x += gtk_widget_get_margin_start (widget);
  else
    rect.x += gtk_widget_get_margin_end (widget);

  rect.y += gtk_widget_get_margin_top (widget);

  gtk_style_context_get (gtk_widget_get_style_context (GTK_WIDGET (popover)),
                         gtk_widget_get_state_flags (GTK_WIDGET (popover)),
                         GTK_STYLE_PROPERTY_BORDER_RADIUS, &border_radius,
                         NULL);
  pos = get_effective_position (popover, priv->final_position);

  if (pos == GTK_POS_BOTTOM || pos == GTK_POS_RIGHT)
    {
      base = TAIL_HEIGHT;
      tip = 0;
      gap_side = (priv->final_position == GTK_POS_BOTTOM) ? GTK_POS_TOP : GTK_POS_LEFT;
    }
  else if (pos == GTK_POS_TOP)
    {
      base = allocation.height - TAIL_HEIGHT;
      tip = allocation.height;
      gap_side = GTK_POS_BOTTOM;
    }
  else if (pos == GTK_POS_LEFT)
    {
      base = allocation.width - TAIL_HEIGHT;
      tip = allocation.width;
      gap_side = GTK_POS_RIGHT;
    }
  else
    g_assert_not_reached ();

  if (POS_IS_VERTICAL (pos))
    {
      tip_pos = rect.x + (rect.width / 2);
      initial_x = CLAMP (tip_pos - TAIL_GAP_WIDTH / 2,
                         border_radius + margin.left,
                         allocation.width - TAIL_GAP_WIDTH - margin.right - border_radius);
      initial_y = base;

      tip_x = CLAMP (tip_pos, 0, allocation.width);
      tip_y = tip;

      final_x = CLAMP (tip_pos + TAIL_GAP_WIDTH / 2,
                       border_radius + margin.left + TAIL_GAP_WIDTH,
                       allocation.width - margin.right - border_radius);
      final_y = base;
    }
  else
    {
      tip_pos = rect.y + (rect.height / 2);

      initial_x = base;
      initial_y = CLAMP (tip_pos - TAIL_GAP_WIDTH / 2,
                         border_radius + margin.top,
                         allocation.height - TAIL_GAP_WIDTH - margin.bottom - border_radius);

      tip_x = tip;
      tip_y = CLAMP (tip_pos, 0, allocation.height);

      final_x = base;
      final_y = CLAMP (tip_pos + TAIL_GAP_WIDTH / 2,
                       border_radius + margin.top + TAIL_GAP_WIDTH,
                       allocation.height - margin.right - border_radius);
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
gtk_popover_get_rect_coords (GtkPopover *popover,
                             gint       *x1_out,
                             gint       *y1_out,
                             gint       *x2_out,
                             gint       *y2_out)
{
  GtkWidget *widget = GTK_WIDGET (popover);
  GtkPopoverPrivate *priv = popover->priv;
  GtkAllocation allocation;
  GtkPositionType pos;
  gint x1, x2, y1, y2;
  GtkBorder margin;

  gtk_widget_get_allocation (widget, &allocation);
  get_margin (widget, &margin);

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    x1 = gtk_widget_get_margin_start (widget);
  else
    x1 = gtk_widget_get_margin_end (widget);

  y1 = gtk_widget_get_margin_top (widget);
  x2 = allocation.width -
    gtk_widget_get_margin_end (widget) + x1;
  y2 = allocation.height -
    gtk_widget_get_margin_bottom (widget) + y1;

  pos = get_effective_position (popover, priv->final_position);

  if (pos == GTK_POS_TOP)
    y2 -= MAX (TAIL_HEIGHT, margin.bottom);
  else if (pos == GTK_POS_BOTTOM)
    y1 += MAX (TAIL_HEIGHT, margin.top);
  else if (pos == GTK_POS_LEFT)
    x2 -= MAX (TAIL_HEIGHT, margin.right);
  else if (pos == GTK_POS_RIGHT)
    x1 += MAX (TAIL_HEIGHT, margin.left);

  if (pos != GTK_POS_BOTTOM)
    y1 += margin.top;
  if (pos != GTK_POS_TOP)
    y2 -= margin.bottom;
  if (pos != GTK_POS_RIGHT)
    x1 += margin.left;
  if (pos != GTK_POS_LEFT)
    x2 -= margin.right;

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
gtk_popover_apply_tail_path (GtkPopover *popover,
                             cairo_t    *cr)
{
  gint initial_x, initial_y;
  gint tip_x, tip_y;
  gint final_x, final_y;

  gtk_popover_get_gap_coords (popover,
                              &initial_x, &initial_y,
                              &tip_x, &tip_y,
                              &final_x, &final_y,
                              NULL);

  cairo_move_to (cr, initial_x, initial_y);
  cairo_line_to (cr, tip_x, tip_y);
  cairo_line_to (cr, final_x, final_y);
}

static void
gtk_popover_apply_border_path (GtkPopover *popover,
                               cairo_t    *cr)
{
  GtkPopoverPrivate *priv;
  GtkAllocation allocation;
  gint x1, y1, x2, y2;

  priv = popover->priv;
  gtk_widget_get_allocation (GTK_WIDGET (popover), &allocation);

  gtk_popover_apply_tail_path (popover, cr);
  gtk_popover_get_rect_coords (popover, &x1, &y1, &x2, &y2);

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
gtk_popover_update_shape (GtkPopover *popover)
{
  cairo_surface_t *surface;
  cairo_region_t *region;
  GdkWindow *win;
  cairo_t *cr;

  win = gtk_widget_get_window (GTK_WIDGET (popover));
  surface =
    gdk_window_create_similar_surface (win,
                                       CAIRO_CONTENT_COLOR_ALPHA,
                                       gdk_window_get_width (win),
                                       gdk_window_get_height (win));

  cr = cairo_create (surface);
  gtk_popover_apply_border_path (popover, cr);
  cairo_fill (cr);
  cairo_destroy (cr);

  region = gdk_cairo_region_create_from_surface (surface);
  cairo_surface_destroy (surface);

  gtk_widget_shape_combine_region (GTK_WIDGET (popover), region);
  cairo_region_destroy (region);
}

static void
gtk_popover_update_position (GtkPopover *popover)
{
  GtkAllocation window_alloc;
  cairo_rectangle_int_t rect;
  GtkPopoverPrivate *priv;
  GtkPositionType pos;
  GtkRequisition req;

  priv = popover->priv;

  if (!priv->window)
    return;

  gtk_widget_get_preferred_size (GTK_WIDGET (popover), NULL, &req);
  gtk_widget_get_allocation (GTK_WIDGET (priv->window), &window_alloc);
  priv->final_position = priv->preferred_position;

  gtk_popover_get_pointed_to_coords (popover, &rect);
  pos = get_effective_position (popover, priv->preferred_position);

  /* Check whether there's enough room on the
   * preferred side, move to the opposite one if not.
   */
  if (pos == GTK_POS_TOP && rect.y < req.height)
    priv->final_position = GTK_POS_BOTTOM;
  else if (pos == GTK_POS_BOTTOM && rect.y > window_alloc.height - req.height)
    priv->final_position = GTK_POS_TOP;
  else if (pos == GTK_POS_LEFT && rect.x < req.width)
    priv->final_position = get_effective_position (popover, GTK_POS_RIGHT);
  else if (pos == GTK_POS_RIGHT && rect.x > window_alloc.width - req.width)
    priv->final_position = get_effective_position (popover, GTK_POS_LEFT);

  _gtk_window_set_popover_position (priv->window, GTK_WIDGET (popover),
                                    priv->final_position, &rect);

  if (priv->final_position != priv->current_position)
    {
      if (gtk_widget_is_drawable (GTK_WIDGET (popover)))
        gtk_popover_update_shape (popover);

      priv->current_position = priv->final_position;
    }
}

static gboolean
gtk_popover_draw (GtkWidget *widget,
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

  gtk_popover_get_rect_coords (GTK_POPOVER (widget),
                               &rect_x1, &rect_y1,
                               &rect_x2, &rect_y2);

  /* Render the rect background */
  gtk_render_background (context, cr,
                         rect_x1, rect_y1,
                         rect_x2 - rect_x1, rect_y2 - rect_y1);

  gtk_popover_get_gap_coords (GTK_POPOVER (widget),
                              &initial_x, &initial_y,
                              NULL, NULL,
                              &final_x, &final_y,
                              &gap_side);

  if (POS_IS_VERTICAL (gap_side))
    {
      gap_start = initial_x - rect_x1;
      gap_end = final_x - rect_x1;
    }
  else
    {
      gap_start = initial_y - rect_y1;
      gap_end = final_y - rect_y1;
    }

  /* Now render the frame, without the gap for the arrow tip */
  gtk_render_frame_gap (context, cr,
                        rect_x1, rect_y1,
                        rect_x2 - rect_x1, rect_y2 - rect_y1,
                        gap_side,
                        gap_start, gap_end);

  /* Clip to the arrow shape */
  cairo_save (cr);

  gtk_popover_apply_tail_path (GTK_POPOVER (widget), cr);
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
      gtk_popover_apply_tail_path (GTK_POPOVER (widget), cr);
      gdk_cairo_set_source_rgba (cr, &border_color);

      cairo_set_line_width (cr, border.bottom + 1);
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
  gint border_width;
  GtkBorder tmp;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  gtk_style_context_get_padding (context, state, border);
  gtk_style_context_get_border (context, state, &tmp);
  border->top += tmp.top + border_width;
  border->right += tmp.right + border_width;
  border->bottom += tmp.bottom + border_width;
  border->left += tmp.left + border_width;
}

static void
gtk_popover_get_preferred_width (GtkWidget *widget,
                                 gint      *minimum_width,
                                 gint      *natural_width)
{
  GtkPopoverPrivate *priv;
  GtkWidget *child;
  GtkPositionType pos;
  gint min, nat, extra;
  GtkBorder border, margin;

  priv = GTK_POPOVER (widget)->priv;
  child = gtk_bin_get_child (GTK_BIN (widget));
  min = nat = 0;

  if (child)
    gtk_widget_get_preferred_width (child, &min, &nat);

  get_padding_and_border (widget, &border);
  get_margin (widget, &margin);

  min += border.left + border.right;
  nat += border.left + border.right;

  pos = get_effective_position (GTK_POPOVER (widget), priv->preferred_position);

  if (pos == GTK_POS_LEFT)
    extra = margin.left + MAX (TAIL_HEIGHT, margin.right);
  else if (pos == GTK_POS_RIGHT)
    extra = MAX (TAIL_HEIGHT, margin.left) + margin.right;
  else
    extra = margin.left + margin.right;

  min += extra;
  nat += extra;

  if (minimum_width)
    *minimum_width = MAX (min, TAIL_GAP_WIDTH);

  if (natural_width)
    *natural_width = MAX (nat, TAIL_GAP_WIDTH);
}

static void
gtk_popover_get_preferred_width_for_height (GtkWidget *widget,
                                            gint       height,
                                            gint      *minimum_width,
                                            gint      *natural_width)
{
  GtkPopoverPrivate *priv;
  GtkWidget *child;
  GtkPositionType pos;
  gint min, nat, extra;
  gint child_height;
  GtkBorder border, margin;

  priv = GTK_POPOVER (widget)->priv;
  child = gtk_bin_get_child (GTK_BIN (widget));
  min = nat = 0;

  child_height = height;

  if (POS_IS_VERTICAL (priv->preferred_position))
    child_height -= TAIL_HEIGHT;

  get_padding_and_border (widget, &border);
  get_margin (widget, &margin);
  child_height -= border.top + border.bottom;

  if (child)
    gtk_widget_get_preferred_width_for_height (child, child_height, &min, &nat);

  min += border.left + border.right;
  nat += border.left + border.right;

  pos = get_effective_position (GTK_POPOVER (widget), priv->preferred_position);

  if (pos == GTK_POS_LEFT)
    extra = margin.left + MAX (TAIL_HEIGHT, margin.right);
  else if (pos == GTK_POS_RIGHT)
    extra = MAX (TAIL_HEIGHT, margin.left) + margin.right;
  else
    extra = margin.left + margin.right;

  min += extra;
  nat += extra;

  if (minimum_width)
    *minimum_width = MAX (min, TAIL_GAP_WIDTH);

  if (natural_width)
    *natural_width = MAX (nat, TAIL_GAP_WIDTH);
}

static void
gtk_popover_get_preferred_height (GtkWidget *widget,
                                  gint      *minimum_height,
                                  gint      *natural_height)
{
  GtkPopoverPrivate *priv;
  GtkWidget *child;
  GtkPositionType pos;
  gint min, nat, extra;
  GtkBorder border, margin;

  priv = GTK_POPOVER (widget)->priv;
  child = gtk_bin_get_child (GTK_BIN (widget));
  min = nat = 0;

  if (child)
    gtk_widget_get_preferred_height (child, &min, &nat);

  get_padding_and_border (widget, &border);
  get_margin (widget, &margin);
  min += border.top + border.bottom;
  nat += border.top + border.bottom;

  pos = get_effective_position (GTK_POPOVER (widget), priv->preferred_position);

  if (pos == GTK_POS_TOP)
    extra = margin.top + MAX (TAIL_HEIGHT, margin.bottom);
  else if (pos == GTK_POS_BOTTOM)
    extra = MAX (TAIL_HEIGHT, margin.top) + margin.bottom;
  else
    extra = margin.top + margin.bottom;

  min += extra;
  nat += extra;

  if (minimum_height)
    *minimum_height = MAX (min, TAIL_GAP_WIDTH);

  if (natural_height)
    *natural_height = MAX (nat, TAIL_GAP_WIDTH);
}

static void
gtk_popover_get_preferred_height_for_width (GtkWidget *widget,
                                            gint       width,
                                            gint      *minimum_height,
                                            gint      *natural_height)
{
  GtkPopoverPrivate *priv;
  GtkWidget *child;
  GtkPositionType pos;
  gint min, nat, extra;
  gint child_width;
  GtkBorder border, margin;

  priv = GTK_POPOVER (widget)->priv;
  child = gtk_bin_get_child (GTK_BIN (widget));
  min = nat = 0;

  child_width = width;

  if (!POS_IS_VERTICAL (priv->preferred_position))
    child_width -= TAIL_HEIGHT;

  get_padding_and_border (widget, &border);
  get_margin (widget, &margin);
  child_width -= border.left + border.right;

  if (child)
    gtk_widget_get_preferred_height_for_width (child, child_width, &min, &nat);

  min += border.top + border.bottom;
  nat += border.top + border.bottom;

  pos = get_effective_position (GTK_POPOVER (widget), priv->preferred_position);

  if (pos == GTK_POS_TOP)
    extra = margin.top + MAX (TAIL_HEIGHT, margin.bottom);
  else if (pos == GTK_POS_BOTTOM)
    extra = MAX (TAIL_HEIGHT, margin.top) + margin.bottom;
  else
    extra = margin.top + margin.bottom;

  min += extra;
  nat += extra;

  if (minimum_height)
    *minimum_height = MAX (min, TAIL_GAP_WIDTH);

  if (natural_height)
    *natural_height = MAX (nat, TAIL_GAP_WIDTH);
}

static void
gtk_popover_size_allocate (GtkWidget     *widget,
                           GtkAllocation *allocation)
{
  GtkWidget *child;

  gtk_widget_set_allocation (widget, allocation);
  child = gtk_bin_get_child (GTK_BIN (widget));
  if (child)
    {
      GtkAllocation child_alloc;
      gint x1, y1, x2, y2;
      GtkBorder border;

      gtk_popover_get_rect_coords (GTK_POPOVER (widget),
                                   &x1, &y1, &x2, &y2);
      get_padding_and_border (widget, &border);

      child_alloc.x = x1 + border.left;
      child_alloc.y = y1 + border.top;
      child_alloc.width = (x2 - x1) - border.left - border.right;
      child_alloc.height = (y2 - y1) - border.top - border.bottom;
      gtk_widget_size_allocate (child, &child_alloc);
    }

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (gtk_widget_get_window (widget),
                              0, 0, allocation->width, allocation->height);
      gtk_popover_update_shape (GTK_POPOVER (widget));
    }
}

static gboolean
gtk_popover_button_press (GtkWidget      *widget,
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
        gtk_widget_hide (widget);
    }
  else
    gtk_widget_hide (widget);

  return GDK_EVENT_PROPAGATE;
}

static gboolean
gtk_popover_key_press (GtkWidget   *widget,
                       GdkEventKey *event)
{
  if (event->keyval == GDK_KEY_Escape)
    {
      gtk_widget_hide (widget);
      return GDK_EVENT_STOP;
    }

  return GDK_EVENT_PROPAGATE;
}

static void
gtk_popover_grab_focus (GtkWidget *widget)
{
  /* Focus the first natural child */
  gtk_widget_child_focus (gtk_bin_get_child (GTK_BIN (widget)),
                          GTK_DIR_TAB_FORWARD);
}

static gboolean
gtk_popover_focus (GtkWidget        *widget,
                   GtkDirectionType  direction)
{
  GtkPopoverPrivate *priv;

  priv = gtk_popover_get_instance_private (GTK_POPOVER (widget));

  if (!GTK_WIDGET_CLASS (gtk_popover_parent_class)->focus (widget, direction))
    {
      GtkWidget *focus;

      focus = gtk_window_get_focus (priv->window);
      focus = gtk_widget_get_parent (focus);

      /* Unset focus child through children, so it is next stepped from
       * scratch.
       */
      while (focus && focus != widget)
        {
          gtk_container_set_focus_child (GTK_CONTAINER (focus), NULL);
          focus = gtk_widget_get_parent (focus);
        }

      return gtk_widget_child_focus (gtk_bin_get_child (GTK_BIN (widget)),
                                     direction);
    }

  return TRUE;
}


static void
gtk_popover_class_init (GtkPopoverClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gtk_popover_set_property;
  object_class->get_property = gtk_popover_get_property;
  object_class->finalize = gtk_popover_finalize;
  object_class->dispose = gtk_popover_dispose;

  widget_class->realize = gtk_popover_realize;
  widget_class->map = gtk_popover_map;
  widget_class->unmap = gtk_popover_unmap;
  widget_class->get_preferred_width = gtk_popover_get_preferred_width;
  widget_class->get_preferred_height = gtk_popover_get_preferred_height;
  widget_class->get_preferred_width_for_height = gtk_popover_get_preferred_width_for_height;
  widget_class->get_preferred_height_for_width = gtk_popover_get_preferred_height_for_width;
  widget_class->size_allocate = gtk_popover_size_allocate;
  widget_class->draw = gtk_popover_draw;
  widget_class->button_press_event = gtk_popover_button_press;
  widget_class->key_press_event = gtk_popover_key_press;
  widget_class->grab_focus = gtk_popover_grab_focus;
  widget_class->focus = gtk_popover_focus;

  /**
   * GtkPopover:relative-to:
   *
   * Sets the attached widget.
   *
   * Since: 3.12
   */
  g_object_class_install_property (object_class,
                                   PROP_RELATIVE_TO,
                                   g_param_spec_object ("relative-to",
                                                        P_("Relative to"),
                                                        P_("Widget the bubble window points to"),
                                                        GTK_TYPE_WIDGET,
                                                        GTK_PARAM_READWRITE));
  /**
   * GtkPopover:pointing-to:
   *
   * Marks a specific rectangle to be pointed.
   *
   * Since: 3.12
   */
  g_object_class_install_property (object_class,
                                   PROP_POINTING_TO,
                                   g_param_spec_boxed ("pointing-to",
                                                       P_("Pointing to"),
                                                       P_("Rectangle the bubble window points to"),
                                                       CAIRO_GOBJECT_TYPE_RECTANGLE_INT,
                                                       GTK_PARAM_READWRITE));
  /**
   * GtkPopover:position
   *
   * Sets the preferred position of the popover.
   *
   * Since: 3.12
   */
  g_object_class_install_property (object_class,
                                   PROP_POSITION,
                                   g_param_spec_enum ("position",
                                                      P_("Position"),
                                                      P_("Position to place the bubble window"),
                                                      GTK_TYPE_POSITION_TYPE, GTK_POS_TOP,
                                                      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  /**
   * GtkPopover:modal
   *
   * Sets whether the popover is modal (so other elements in the window are
   * not usable while the popover is visible).
   *
   * Since: 3.12
   */
  g_object_class_install_property (object_class,
                                   PROP_MODAL,
                                   g_param_spec_boolean ("modal",
                                                         P_("Modal"),
                                                         P_("Whether the popover is modal"),
                                                         TRUE,
                                                         GTK_PARAM_READWRITE));

  quark_widget_popovers = g_quark_from_static_string ("gtk-quark-widget-popovers");
}

static void
_gtk_popover_update_context_parent (GtkPopover *popover)
{
  GtkStyleContext *context, *parent_context = NULL;
  GtkPopoverPrivate *priv = popover->priv;

  context = gtk_widget_get_style_context (GTK_WIDGET (popover));

  if (priv->widget)
    parent_context = gtk_widget_get_style_context (priv->widget);

  gtk_style_context_set_parent (context, parent_context);
}

static void
_gtk_popover_parent_hierarchy_changed (GtkWidget  *widget,
                                       GtkWidget  *previous_toplevel,
                                       GtkPopover *popover)
{
  GtkPopoverPrivate *priv;
  GtkWindow *new_window;

  priv = popover->priv;
  new_window = GTK_WINDOW (gtk_widget_get_ancestor (widget, GTK_TYPE_WINDOW));

  if (priv->window == new_window)
    return;

  g_object_ref (popover);

  if (priv->window)
    _gtk_window_remove_popover (priv->window, GTK_WIDGET (popover));

  if (new_window)
    _gtk_window_add_popover (new_window, GTK_WIDGET (popover));

  priv->window = new_window;

  if (new_window)
    gtk_popover_update_position (popover);

  _gtk_popover_update_context_parent (popover);

  if (gtk_widget_is_visible (GTK_WIDGET (popover)))
    gtk_widget_queue_resize (GTK_WIDGET (popover));

  g_object_unref (popover);
}

static void
_gtk_popover_parent_unmap (GtkWidget *widget,
                           GtkPopover *popover)
{
  gtk_widget_unmap (GTK_WIDGET (popover));
}

static void
_gtk_popover_parent_size_allocate (GtkWidget     *widget,
                                   GtkAllocation *allocation,
                                   GtkPopover    *popover)
{
  gtk_popover_update_position (popover);
}

static void
widget_manage_popover (GtkWidget  *widget,
                       GtkPopover *popover)
{
  GHashTable *popovers;

  popovers = g_object_get_qdata (G_OBJECT (widget), quark_widget_popovers);

  if (G_UNLIKELY (!popovers))
    {
      popovers = g_hash_table_new_full (NULL, NULL,
                                        (GDestroyNotify) g_object_unref, NULL);
      g_object_set_qdata_full (G_OBJECT (widget),
                               quark_widget_popovers, popovers,
                               (GDestroyNotify) g_hash_table_unref);
    }

  g_hash_table_add (popovers, g_object_ref_sink (popover));
}

static void
widget_unmanage_popover (GtkWidget  *widget,
                         GtkPopover *popover)
{
  GHashTable *popovers;

  popovers = g_object_get_qdata (G_OBJECT (widget), quark_widget_popovers);

  if (G_UNLIKELY (!popovers))
    return;

  g_hash_table_remove (popovers, popover);
}

static void
gtk_popover_update_relative_to (GtkPopover *popover,
                                GtkWidget  *relative_to)
{
  GtkPopoverPrivate *priv;

  priv = popover->priv;

  if (priv->widget == relative_to)
    return;

  g_object_ref (popover);

  if (priv->window)
    {
      _gtk_window_remove_popover (priv->window, GTK_WIDGET (popover));
      priv->window = NULL;
    }

  if (priv->widget)
    {
      if (g_signal_handler_is_connected (priv->widget, priv->hierarchy_changed_id))
        g_signal_handler_disconnect (priv->widget, priv->hierarchy_changed_id);
      if (g_signal_handler_is_connected (priv->widget, priv->size_allocate_id))
        g_signal_handler_disconnect (priv->widget, priv->size_allocate_id);
      if (g_signal_handler_is_connected (priv->widget, priv->unmap_id))
        g_signal_handler_disconnect (priv->widget, priv->unmap_id);

      widget_unmanage_popover (priv->widget, popover);
    }

  priv->widget = relative_to;
  g_object_notify (G_OBJECT (popover), "relative-to");

  if (priv->widget)
    {
      priv->window =
        GTK_WINDOW (gtk_widget_get_ancestor (priv->widget, GTK_TYPE_WINDOW));

      priv->hierarchy_changed_id =
        g_signal_connect (priv->widget, "hierarchy-changed",
                          G_CALLBACK (_gtk_popover_parent_hierarchy_changed),
                          popover);
      priv->size_allocate_id =
        g_signal_connect (priv->widget, "size-allocate",
                          G_CALLBACK (_gtk_popover_parent_size_allocate),
                          popover);
      priv->unmap_id =
        g_signal_connect (priv->widget, "unmap",
                          G_CALLBACK (_gtk_popover_parent_unmap),
                          popover);

      /* Give ownership of the popover to widget */
      widget_manage_popover (priv->widget, popover);
    }

  if (priv->window)
    _gtk_window_add_popover (priv->window, GTK_WIDGET (popover));

  _gtk_popover_update_context_parent (popover);
  g_object_unref (popover);
}

static void
gtk_popover_update_pointing_to (GtkPopover            *popover,
                                cairo_rectangle_int_t *pointing_to)
{
  GtkPopoverPrivate *priv;

  priv = popover->priv;

  if (pointing_to)
    {
      priv->pointing_to = *pointing_to;
      priv->has_pointing_to = TRUE;
    }
  else
    priv->has_pointing_to = FALSE;

  g_object_notify (G_OBJECT (popover), "pointing-to");
}

static void
gtk_popover_update_preferred_position (GtkPopover      *popover,
                                       GtkPositionType  position)
{
  GtkPopoverPrivate *priv;

  priv = popover->priv;
  priv->preferred_position = position;
  g_object_notify (G_OBJECT (popover), "position");
}

/**
 * gtk_popover_new:
 * @relative_to: #GtkWidget the popover is related to
 *
 * Creates a new popover to point to @relative_to
 *
 * Returns: a new #GtkPopover
 *
 * Since: 3.12
 **/
GtkWidget *
gtk_popover_new (GtkWidget *relative_to)
{
  return g_object_new (GTK_TYPE_POPOVER,
                       "relative-to", relative_to,
                       NULL);
}

/**
 * gtk_popover_set_relative_to:
 * @popover: a #GtkPopover
 * @relative_to: a #GtkWidget
 *
 * Sets a new widget to be attached to @popover. If @popover is
 * visible, the position will be updated.
 *
 * Since: 3.12
 **/
void
gtk_popover_set_relative_to (GtkPopover *popover,
                             GtkWidget  *relative_to)
{
  g_return_if_fail (GTK_IS_POPOVER (popover));
  g_return_if_fail (GTK_IS_WIDGET (relative_to));

  gtk_popover_update_relative_to (popover, relative_to);
  gtk_popover_update_position (popover);
}

/**
 * gtk_popover_get_relative_to:
 * @popover: a #GtkPopover
 *
 * Returns the wigdet @popover is currently attached to
 *
 * Returns: (transfer none): a #GtkWidget
 *
 * Since: 3.12
 **/
GtkWidget *
gtk_popover_get_relative_to (GtkPopover *popover)
{
  GtkPopoverPrivate *priv;

  g_return_val_if_fail (GTK_IS_POPOVER (popover), NULL);

  priv = popover->priv;

  return priv->widget;
}

/**
 * gtk_popover_set_pointing_to:
 * @popover: a #GtkPopover
 * @rect: rectangle to point to
 *
 * Sets the rectangle that @popover will point to, in the coordinate
 * space of the widget @popover is attached to, see
 * gtk_popover_set_relative_to()
 *
 * Since: 3.12
 **/
void
gtk_popover_set_pointing_to (GtkPopover            *popover,
                             cairo_rectangle_int_t *rect)
{
  g_return_if_fail (GTK_IS_POPOVER (popover));
  g_return_if_fail (rect != NULL);

  gtk_popover_update_pointing_to (popover, rect);
  gtk_popover_update_position (popover);
}

/**
 * gtk_popover_get_pointing_to:
 * @popover: a #GtkPopover
 * @rect: (out): location to store the rectangle
 *
 * If a rectangle to point to has been set, this function will
 * return %TRUE and fill in @rect with such rectangle, otherwise
 * it will return %FALSE and fill in @rect with the attached
 * widget coordinates.
 *
 * Returns: %TRUE if a rectangle to point to was set.
 **/
gboolean
gtk_popover_get_pointing_to (GtkPopover            *popover,
                             cairo_rectangle_int_t *rect)
{
  GtkPopoverPrivate *priv;

  g_return_val_if_fail (GTK_IS_POPOVER (popover), FALSE);

  priv = popover->priv;

  if (rect)
    {
      if (priv->has_pointing_to)
        *rect = priv->pointing_to;
      else if (priv->widget)
        {
          gtk_widget_get_allocation (priv->widget, rect);
          rect->x = rect->y = 0;
        }
    }

  return priv->has_pointing_to;
}

/**
 * gtk_popover_set_position:
 * @popover: a #GtkPopover
 * @position: preferred popover position
 *
 * Sets the preferred position for @popover to appear. If the @popover
 * is currently visible, it will be immediately updated.
 *
 * <note>
 *   This preference will be respected where possible, although
 *   on lack of space (eg. if close to the window edges), the
 *   #GtkPopover may choose to appear on the opposite side
 * </note>
 *
 * Since: 3.12
 **/
void
gtk_popover_set_position (GtkPopover      *popover,
                          GtkPositionType  position)
{
  g_return_if_fail (GTK_IS_POPOVER (popover));
  g_return_if_fail (position >= GTK_POS_LEFT && position <= GTK_POS_BOTTOM);

  gtk_popover_update_preferred_position (popover, position);
  gtk_popover_update_position (popover);
}

/**
 * gtk_popover_get_position:
 * @popover: a #GtkPopover
 *
 * Returns the preferred position of @popover.
 *
 * Returns: The preferred position.
 **/
GtkPositionType
gtk_popover_get_position (GtkPopover *popover)
{
  GtkPopoverPrivate *priv;

  g_return_val_if_fail (GTK_IS_POPOVER (popover), GTK_POS_TOP);

  priv = popover->priv;

  return priv->preferred_position;
}

/**
 * gtk_popover_set_modal:
 * @popover: a #GtkPopover
 * @modal: #TRUE to make popover claim all input within the toplevel
 *
 * Sets whether @popover is modal, a modal popover will grab all input
 * within the toplevel and grab the keyboard focus on it when being
 * displayed. Clicking outside the popover area or pressing Esc will
 * dismiss the popover and ungrab input.
 *
 * Since: 3.12
 **/
void
gtk_popover_set_modal (GtkPopover *popover,
                       gboolean    modal)
{
  GtkPopoverPrivate *priv;

  g_return_if_fail (GTK_IS_POPOVER (popover));

  priv = popover->priv;

  if ((priv->modal == TRUE) == (modal == TRUE))
    return;

  priv->modal = (modal != FALSE);

  if (gtk_widget_is_visible (GTK_WIDGET (popover)))
    gtk_popover_apply_modality (popover, priv->modal);

  g_object_notify (G_OBJECT (popover), "modal");
}

/**
 * gtk_popover_get_modal:
 * @popover: a #GtkPopover
 *
 * Returns whether the popover is modal, see gtk_popover_set_modal to
 * see the implications of this.
 *
 * Returns: #TRUE if @popover is modal
 *
 * Since: 3.12
 **/
gboolean
gtk_popover_get_modal (GtkPopover *popover)
{
  GtkPopoverPrivate *priv;

  g_return_val_if_fail (GTK_IS_POPOVER (popover), FALSE);

  priv = popover->priv;

  return priv->modal;
}
