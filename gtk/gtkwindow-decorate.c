/* GTK - The GIMP Toolkit
 * Copyright (C) 2001 Red Hat, Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Authors: Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"
#include "gtkprivate.h"
#include "gtkwindow.h"
#include "gtklabel.h"
#include "gtkhbox.h"
#include "gtkbutton.h"
#include "gtkmain.h"
#include "gtkwindow-decorate.h"
#include "gtkintl.h"
#include "gtkalias.h"

#include <math.h>

typedef enum
{
  GTK_WINDOW_REGION_TITLE,
  GTK_WINDOW_REGION_MAXIMIZE,
  GTK_WINDOW_REGION_MINIMIZE,
  GTK_WINDOW_REGION_CLOSE,
  GTK_WINDOW_REGION_BR_RESIZE
} GtkWindowRegionType;

typedef struct _GtkWindowRegion GtkWindowRegion;
typedef struct _GtkWindowDecoration GtkWindowDecoration;

struct _GtkWindowRegion
{
  GdkRectangle rect;
  GtkWindowRegionType type;
};

typedef enum
{
  RESIZE_TOP_LEFT,
  RESIZE_TOP,
  RESIZE_TOP_RIGHT,
  RESIZE_RIGHT,
  RESIZE_BOTTOM_RIGHT,
  RESIZE_BOTTOM,
  RESIZE_BOTTOM_LEFT,
  RESIZE_LEFT,
  RESIZE_NONE,
} GtkWindowResizeType;

struct _GtkWindowDecoration
{
  gint n_regions;
  GtkWindowRegion *regions;

  gint last_x, last_y;
  gint last_w, last_h;

  gint radius;
  gboolean round_corners;

  PangoLayout *title_layout;

  GtkWindowResizeType resize;

  GtkWidget *hbox;
  GtkWidget *label_widget;
  GtkWidget *close_button;
  GtkWidget *max_button;
  GtkWidget *min_button;

  guint minimizing : 1;
  guint moving : 1;
  guint closing : 1;
  guint maximizing : 1;
  guint maximized : 1;
  guint maximizable : 1;
  guint minimizable : 1;
  guint decorated : 1;
  guint real_inner_move : 1;
  guint focused : 1;
};

#define DECORATION_TITLE_FONT "Sans 9"

static void gtk_decorated_window_recalculate_regions      (GtkWindow      *window);
static GtkWindowRegionType gtk_decorated_window_region_type    (GtkWindow      *window,
						 gint            x,
						 gint            y);
static gint gtk_decorated_window_frame_event    (GtkWindow *window,
						 GdkEvent *event);
static gint gtk_decorated_window_button_press   (GtkWidget      *widget,
						 GdkEventButton *event);
static gint gtk_decorated_window_button_release (GtkWidget      *widget,
						 GdkEventButton *event);
static gint gtk_decorated_window_motion_notify  (GtkWidget      *widget,
						 GdkEventMotion *event);
static gint gtk_decorated_window_window_state   (GtkWidget           *widget,
						 GdkEventWindowState *event);
static void gtk_decorated_window_paint          (GtkWidget      *widget,
						 GdkRectangle   *area);
static gint gtk_decorated_window_focus_change   (GtkWidget         *widget,
						 GdkEventFocus     *event);
static void gtk_decorated_window_realize        (GtkWindow   *window);
static void gtk_decorated_window_unrealize      (GtkWindow   *window);

static void
gtk_decoration_free (GtkWindowDecoration *deco)
{
  g_free (deco->regions);
  deco->regions = NULL;
  deco->n_regions = 0;

  g_object_unref (deco->hbox);

  g_free (deco);
}

void
gtk_decorated_window_init (GtkWindow   *window)
{
  GtkWindowDecoration *deco;

  deco = g_new (GtkWindowDecoration, 1);

  deco->n_regions = 0;
  deco->regions = NULL;
  deco->title_layout = NULL;
  deco->resize = RESIZE_NONE;
  deco->moving = FALSE;
  deco->decorated = TRUE;
  deco->closing = FALSE;
  deco->maximizing = FALSE;
  deco->maximized = FALSE;
  deco->minimizing = FALSE;
  deco->maximizable = FALSE;
  deco->real_inner_move = FALSE;
  deco->round_corners = TRUE;
  deco->radius = 5;

  deco->label_widget = gtk_label_new ("This is a test");
  deco->close_button = gtk_button_new_from_stock ("stock-smiley-26");
  deco->hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (deco->hbox), deco->label_widget, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (deco->hbox), deco->close_button, FALSE, FALSE, 0);
  gtk_widget_show_all (deco->hbox);
  gtk_widget_set_parent_window (deco->hbox, window->frame);
  gtk_widget_set_parent (deco->hbox, GTK_WIDGET (window));

  g_object_set_data_full (G_OBJECT (window), I_("gtk-window-decoration"), deco,
			  (GDestroyNotify) gtk_decoration_free);

  gtk_window_set_has_frame (window, TRUE);

  g_signal_connect (window,
		    "frame-event",
		    G_CALLBACK (gtk_decorated_window_frame_event),
		    window);
  g_signal_connect (window,
		    "focus-in-event",
		    G_CALLBACK (gtk_decorated_window_focus_change),
		    window);
  g_signal_connect (window,
		    "focus-out-event",
		    G_CALLBACK (gtk_decorated_window_focus_change),
		    window);
  g_signal_connect (window,
		    "realize",
		    G_CALLBACK (gtk_decorated_window_realize),
		    window);
  g_signal_connect (window,
		    "unrealize",
		    G_CALLBACK (gtk_decorated_window_unrealize),
		    window);
}

static inline GtkWindowDecoration *
get_decoration (GtkWindow *window)
{
  return (GtkWindowDecoration *)g_object_get_data (G_OBJECT (window), "gtk-window-decoration");
}

GtkWidget *
gtk_decorated_window_get_box (GtkWindow *window)
{
  GtkWindowDecoration *deco;

  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  deco = get_decoration (window);

  if (!deco || !deco->hbox)
    return NULL;

  return deco->hbox;
}

void
gtk_decorated_window_set_title (GtkWindow   *window,
				const gchar *title)
{
  GtkWindowDecoration *deco;

  g_return_val_if_fail (GTK_IS_WINDOW (window), NULL);

  deco = get_decoration (window);

  if (deco->title_layout)
    pango_layout_set_text (deco->title_layout, title, -1);
}

void
gtk_decorated_window_calculate_frame_size (GtkWindow *window)
{
  GdkWMDecoration decorations;
  GtkWindowDecoration *deco = get_decoration (window);
  gint border_left, border_top, border_right, border_bottom;

  if (!deco)
    return;

  decorations = gtk_window_get_client_side_decorations (window);
  if (!decorations)
    {
      gdk_window_get_decorations (GTK_WIDGET (window)->window,
                                  &decorations);
    }

  if (decorations)
    {
      if ((decorations & GDK_DECOR_BORDER) &&
	  (decorations & GDK_DECOR_TITLE))
	{
	  deco->decorated = TRUE;

	  if ((decorations & GDK_DECOR_MAXIMIZE) &&
	      (gtk_window_get_type_hint (window) == GDK_WINDOW_TYPE_HINT_NORMAL))
            {
              deco->maximizable = TRUE;
            }

          if ((decorations & GDK_DECOR_MINIMIZE) &&
              (gtk_window_get_type_hint (window) == GDK_WINDOW_TYPE_HINT_NORMAL))
            {
              deco->minimizable = TRUE;
            }
	}
      else
        {
          deco->decorated = FALSE;
        }
    }
  else
    {
      deco->decorated = (window->type != GTK_WINDOW_POPUP);
      deco->maximizable = (gtk_window_get_type_hint (window) == GDK_WINDOW_TYPE_HINT_NORMAL);
    }

  if (deco->decorated)
    {
      gtk_widget_style_get (GTK_WIDGET (window),
                            "decoration-border-left",   &border_left,
                            "decoration-border-top",    &border_top,
                            "decoration-border-right",  &border_right,
                            "decoration-border-bottom", &border_bottom,
                            NULL);

      gtk_window_set_frame_dimensions (window,
                                       border_left,
                                       border_top,
                                       border_right,
                                       border_bottom);
    }
  else
    {
      gtk_window_set_frame_dimensions (window, 0, 0, 0, 0);
    }

  gtk_decorated_window_recalculate_regions (window);
}

static gboolean
gtk_decorated_window_inner_change (GdkWindow *win,
				   gint x, gint y,
				   gint width, gint height,
				   gpointer user_data)
{
  GtkWindow *window = (GtkWindow *)user_data;
  GtkWidget *widget = GTK_WIDGET (window);
  GtkWindowDecoration *deco = get_decoration (window);

  if (deco->real_inner_move)
    {
      deco->real_inner_move = FALSE;
      return FALSE;
    }

  deco->real_inner_move = TRUE;
  gdk_window_move_resize (widget->window,
			  window->frame_left, window->frame_top,
			  width, height);

  gdk_window_move_resize (window->frame,
			  x - window->frame_left, y - window->frame_top,
			  width + window->frame_left + window->frame_right,
			  height + window->frame_top + window->frame_bottom);
  return TRUE;
}

static void
gtk_decorated_window_inner_get_pos (GdkWindow *win,
				    gint *x, gint *y,
				    gpointer user_data)
{
  GtkWindow *window = (GtkWindow *)user_data;

  gdk_window_get_position (window->frame, x, y);

  *x += window->frame_left;
  *y += window->frame_top;
}

static void
gtk_decorated_window_realize (GtkWindow   *window)
{
  GtkWindowDecoration *deco = get_decoration (window);
  GtkWidget *widget = GTK_WIDGET (window);
  PangoFontDescription *font_desc;

  deco->title_layout = gtk_widget_create_pango_layout (widget,
						       (window->title)?window->title:"");

  font_desc = pango_font_description_from_string(DECORATION_TITLE_FONT);
  pango_layout_set_font_description (deco->title_layout, font_desc);
  pango_font_description_free (font_desc);
}


static void
gtk_decorated_window_unrealize (GtkWindow   *window)
{
  GtkWindowDecoration *deco = get_decoration (window);

  if (deco->title_layout)
    {
      g_object_unref (deco->title_layout);
      deco->title_layout = NULL;
    }
}

static gint
gtk_decorated_window_frame_event (GtkWindow *window, GdkEvent *event)
{
  GtkWindowDecoration *deco = get_decoration (window);
  GtkWidget *widget = GTK_WIDGET (window);
  GdkEventExpose *expose_event;

  switch (event->type)
    {
    case GDK_EXPOSE:
      expose_event = (GdkEventExpose *)event;
      if (deco->decorated)
	gtk_decorated_window_paint (widget, &expose_event->area);

      gtk_container_propagate_expose (GTK_CONTAINER (window),
                                      deco->hbox,
                                      event);
      return TRUE;
      break;
    case GDK_CONFIGURE:
      gtk_decorated_window_recalculate_regions (window);
      break;
    case GDK_MOTION_NOTIFY:
      return gtk_decorated_window_motion_notify (widget, (GdkEventMotion *)event);
      break;
    case GDK_BUTTON_PRESS:
      return gtk_decorated_window_button_press (widget, (GdkEventButton *)event);
      break;
    case GDK_BUTTON_RELEASE:
      return gtk_decorated_window_button_release (widget, (GdkEventButton *)event);
    case GDK_WINDOW_STATE:
      return gtk_decorated_window_window_state (widget, (GdkEventWindowState *)event);
    default:
      break;
    }
  return FALSE;
}

static gint
gtk_decorated_window_focus_change (GtkWidget         *widget,
				   GdkEventFocus     *event)
{
  GtkWindow *window = GTK_WINDOW(widget);
  GtkWindowDecoration *deco = get_decoration (window);
  deco->focused = event->in;
  gdk_window_invalidate_rect (window->frame, NULL, FALSE);
  return FALSE;
}

static gint
gtk_decorated_window_motion_notify (GtkWidget       *widget,
				    GdkEventMotion  *event)
{
  GtkWindow *window;
  GtkWindowDecoration *deco;
  GdkModifierType mask;
  GdkWindow *win;
  gint x, y;
  gint win_x, win_y, win_w, win_h;
  gint border_top, border_left, border_right, border_bottom;

  window = GTK_WINDOW (widget);
  deco = get_decoration (window);

  if (!deco->decorated)
    return TRUE;

  win = widget->window;
  gdk_window_get_pointer (window->frame, &x, &y, &mask);

  gtk_widget_style_get (GTK_WIDGET (window),
                        "decoration-border-left",   &border_left,
                        "decoration-border-top",    &border_top,
                        "decoration-border-right",  &border_right,
                        "decoration-border-bottom", &border_bottom,
                        NULL);

  gdk_window_get_position (window->frame, &win_x, &win_y);
  win_x += border_left;
  win_y += border_top;

  gdk_window_get_geometry (win, NULL, NULL, &win_w, &win_h, NULL);

  if (deco->moving)
    {
      int dx, dy;
      dx = x - deco->last_x;
      dy = y - deco->last_y;

      _gtk_window_reposition (window, win_x + dx, win_y + dy);
    }

  if (deco->resize != RESIZE_NONE)
    {
      int w, h;
      
      w = win_w;
      h = win_h;
      
      switch(deco->resize) {
      case RESIZE_BOTTOM_RIGHT:
	w = x - border_left + border_right;
	h = y - border_top + border_bottom;
	break;
      case RESIZE_RIGHT:
	w = x - border_left + border_right;
	break;
      case RESIZE_BOTTOM:
	h = y - border_top + border_bottom;
	break;
      case RESIZE_TOP_LEFT:
      case RESIZE_TOP:
      case RESIZE_TOP_RIGHT:
      case RESIZE_BOTTOM_LEFT:
      case RESIZE_LEFT:
      default:
	g_warning ("Resize mode %d not handled yet.\n", deco->resize);
	break;
      }
      
      if ((w > 0) && (h > 0))
	{
	  _gtk_window_constrain_size (window, w,h, &w, &h);
	  
	  if ((w != win_w) || (h != win_h))
	    gdk_window_resize (widget->window, w, h);
	}
    }

  return TRUE;
}

static GtkWindowRegionType
gtk_decorated_window_region_type (GtkWindow *window, gint x, gint y)
{
  GtkWindowDecoration *deco = get_decoration (window);
  int i;

  for (i=0;i<deco->n_regions;i++)
    {
      if ((x > deco->regions[i].rect.x) &&
	  (x - deco->regions[i].rect.x < deco->regions[i].rect.width) &&
	  (y > deco->regions[i].rect.y) &&
	  (y - deco->regions[i].rect.y < deco->regions[i].rect.height))
        {
          return deco->regions[i].type;
        }
    }

  return -1;
}

static gint
gtk_decorated_window_button_press (GtkWidget       *widget,
				   GdkEventButton  *event)
{
  GtkWindow *window;
  GtkWindowRegionType type;
  GtkWindowDecoration *deco;
  gint x, y;

  window = GTK_WINDOW (widget);
  deco = get_decoration (window);

  if (!deco->decorated)
    return TRUE;

  x = event->x;
  y = event->y;

  type = gtk_decorated_window_region_type (window, x, y);

  switch (type)
    {
    case GTK_WINDOW_REGION_TITLE:
      if (!deco->maximized && event->button == 1)
	{
          gtk_window_begin_move_drag (window,
                                      event->button,
                                      event->x_root,
                                      event->y_root,
                                      event->time);
	}
      break;
#if 0
    case GTK_WINDOW_REGION_MAXIMIZE:
      if (event->button == 1)
	deco->maximizing = TRUE;
      break;
    case GTK_WINDOW_REGION_MINIMIZE:
      if (event->button == 1)
        deco->minimizing = TRUE;
      break;
    case GTK_WINDOW_REGION_CLOSE:
      if (event->button == 1)
	deco->closing = TRUE;
      break;
#endif
    case GTK_WINDOW_REGION_BR_RESIZE:
      if (!deco->maximized)
	{
          gtk_window_begin_resize_drag (window,
                                        GDK_WINDOW_EDGE_SOUTH_EAST,
                                        event->button,
                                        event->x_root,
                                        event->y_root,
                                        event->time);
	}
      break;
    default:
      break;
    }

  return TRUE;
}

static gint
gtk_decorated_window_button_release (GtkWidget	    *widget,
				     GdkEventButton *event)
{
  GtkWindow *window;
  GtkWindowRegionType type;
  GtkWindowDecoration *deco;

  window = GTK_WINDOW (widget);
  type = gtk_decorated_window_region_type (window, event->x, event->y);
  deco = get_decoration (window);

  if (deco->closing)
    {
      if (type == GTK_WINDOW_REGION_CLOSE)
	{
	  GdkEvent *event = gdk_event_new (GDK_DELETE);

	  event->any.type = GDK_DELETE;
	  event->any.window = g_object_ref (widget->window);
	  event->any.send_event = TRUE;

	  gtk_main_do_event (event);
	  gdk_event_free (event);
	}
    }
  else if (deco->maximizing)
    {
      if (type == GTK_WINDOW_REGION_MAXIMIZE)
        {
	  if (deco->maximized)
	    gtk_window_unmaximize (window);
	  else
	    gtk_window_maximize (window);
	}
    }
  else if (deco->minimizing)
    {
      if (type == GTK_WINDOW_REGION_MINIMIZE)
        {
          gtk_window_iconify (window);
        }
    }

  deco->closing = FALSE;
  deco->maximizing = FALSE;
  deco->minimizing = FALSE;
  deco->moving = FALSE;
  deco->resize = RESIZE_NONE;
  return TRUE;
}

static gint
gtk_decorated_window_window_state (GtkWidget	       *widget,
				   GdkEventWindowState *event)
{
  GtkWindow *window;
  GtkWindowDecoration *deco;
  GdkWindowObject *priv;
  gint border_left, border_right, border_top, border_bottom;

  window = GTK_WINDOW (widget);
  deco = get_decoration (window);
  priv = GDK_WINDOW_OBJECT (window->frame);

  gtk_widget_style_get (widget,
                        "decoration-border-left",   &border_left,
                        "decoration-border-top",    &border_top,
                        "decoration-border-right",  &border_right,
                        "decoration-border-bottom", &border_bottom,
                        NULL);

  if (event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED)
    {
      if (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED)
	{
	  int w, h;
	  gdk_window_get_geometry (widget->window, NULL, NULL,
				   &deco->last_w, &deco->last_h, NULL);
	  gdk_window_get_origin (widget->window, &deco->last_x, &deco->last_y);
	  w = gdk_screen_get_width(gdk_screen_get_default()) - border_left + border_right;
	  h = gdk_screen_get_height(gdk_screen_get_default()) - border_top + border_bottom;
	  _gtk_window_constrain_size (window, w, h, &w, &h);
	  if (w != deco->last_w || h != deco->last_h)
	    {
	      _gtk_window_reposition (window, border_left, border_top);
	      gdk_window_resize (widget->window, w, h);
	      deco->maximized = TRUE;
	    }
	}
      else
	{
	  _gtk_window_reposition (window, deco->last_x, deco->last_y);
	  _gtk_window_constrain_size (window, deco->last_w, deco->last_h,
				      &deco->last_w, &deco->last_h);
	  gdk_window_resize (widget->window, deco->last_w, deco->last_h);
	  deco->maximized = FALSE;
	}
    }
  return TRUE;
}

static void
gtk_decorated_window_paint (GtkWidget    *widget,
			    GdkRectangle *area)
{
  GtkWindow *window = GTK_WINDOW (widget);
  GtkWindowDecoration *deco = get_decoration (window);
  gint x1, y1, x2, y2;
  GtkStateType border_state;
  GdkRectangle rect;

  if (deco->decorated)
    {
      GdkWindow *frame;
      gint width, height;
      gint border_top, border_bottom, border_left, border_right;
      gint button_size;
      gint y_offset;

      frame = window->frame;
      gdk_drawable_get_size (frame, &width, &height);

      gtk_widget_style_get (GTK_WIDGET (window),
                            "decoration-border-left",     &border_left,
                            "decoration-border-top",      &border_top,
                            "decoration-border-right",    &border_right,
                            "decoration-border-bottom",   &border_bottom,
                            "decoration-button-size",     &button_size,
                            "decoration-button-y-offset", &y_offset,
                            NULL);

      height += border_top + border_bottom;
      width += border_left + border_right;

      rect.x = area->x - border_left;
      rect.y = area->y - border_top;
      rect.width = area->width + border_left + border_right;
      rect.height = area->height + border_top + border_bottom;

#if 1
      /* XXX - what is this for? */

      /* Top */
      gtk_paint_flat_box (widget->style, frame, GTK_STATE_NORMAL,
			  GTK_SHADOW_NONE, area, widget, "base",
			  0, 0,
			  width, border_top);
      /* Bottom */
      gtk_paint_flat_box (widget->style, frame, GTK_STATE_NORMAL,
			  GTK_SHADOW_NONE, area, widget, "base",
			  0, height - border_bottom,
			  width, border_bottom);
      /* Left */
      gtk_paint_flat_box (widget->style, frame, GTK_STATE_NORMAL,
			  GTK_SHADOW_NONE, area, widget, "base",
			  0, border_top,
			  border_left, height - border_top + border_bottom);
      /* Right */
      gtk_paint_flat_box (widget->style, frame, GTK_STATE_NORMAL,
			  GTK_SHADOW_NONE, area, widget, "base",
			  width - border_right, border_top,
			  border_right, height - border_top + border_bottom);
#endif

      /* Border: */
      if (deco->focused)
	border_state = GTK_STATE_SELECTED;
      else
	border_state = GTK_STATE_PRELIGHT;

      if (deco->round_corners)
        {
          cairo_pattern_t *gradient;
          cairo_t *cr;
          const int hmargin = 2, vmargin = 2, radius = 5;
          const int width = widget->allocation.width + border_left + border_right;
          const int height = widget->allocation.height + border_top + border_bottom;

          cr = gdk_cairo_create (frame);
          cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
          cairo_paint (cr);

          cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
          cairo_arc (cr, hmargin + radius, vmargin + radius,
                     radius, M_PI, 3 * M_PI / 2);
          cairo_line_to (cr, width - hmargin - radius, vmargin);
          cairo_arc (cr, width - hmargin - radius, vmargin + radius,
                     radius, 3 * M_PI / 2, 2 * M_PI);
          cairo_line_to (cr, width - hmargin, height - vmargin - radius);
          cairo_arc (cr, width - hmargin - radius, height - vmargin - radius,
                     radius, 0, M_PI / 2);
          cairo_line_to (cr, hmargin + radius, height - vmargin);
          cairo_arc (cr, hmargin + radius, height - vmargin - radius,
                     radius, M_PI / 2, M_PI);
          cairo_close_path (cr);

          gradient = cairo_pattern_create_linear (width / 2 - 1, vmargin,
                                                  width / 2 + 1, height);
          cairo_pattern_add_color_stop_rgba (gradient, 0, 1, 1, 1, 0.2);
          cairo_pattern_add_color_stop_rgba (gradient, 1, 1, 1, 1, 0.9);
          cairo_set_source (cr, gradient);
          cairo_fill_preserve (cr);

          cairo_set_source_rgba (cr, 1, 1, 1, 2);
          cairo_set_line_width (cr, 1);
          cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
          cairo_stroke (cr);

          cairo_destroy (cr);
        }
      else
        {
          gtk_paint_box (widget->style, frame, border_state,
                         GTK_SHADOW_OUT, area, widget, "base",
                         0, 0, width, height);

          gtk_paint_box (widget->style, frame, border_state,
                         GTK_SHADOW_IN, area, widget, "base",
                         border_left - 2, border_top - 2,
                         width - (border_left + border_right) + 3,
                         height - (border_top + border_bottom) + 3);
        }

#if 0
      if (deco->maximizable)
	{
	  /* Maximize button: */
	  x1 = width - (border_left * 2) - (button_size * 2);
	  y1 = y_offset;
	  x2 = x1 + button_size;
	  y2 = y1 + button_size;

	  if (area)
	    gdk_gc_set_clip_rectangle (widget->style->bg_gc[widget->state], area);

	  gdk_draw_rectangle (frame, widget->style->bg_gc[widget->state], TRUE,
			      x1, y1, x2 - x1, y2 - y1);

	  gdk_draw_line (frame, widget->style->black_gc, x1 + 1, y1 + 1, x2 - 2, y1 + 1);

	  gdk_draw_rectangle (frame, widget->style->black_gc, FALSE,
			      x1 + 1, y1 + 2,
			      button_size - 3, button_size - 4);

	  if (area)
	    gdk_gc_set_clip_rectangle (widget->style->black_gc, NULL);
	}

      if (deco->minimizable)
        {
          /* Minimize button */
          x1 = width - (border_left * 2) - (button_size * 3);
          y1 = y_offset;
          x2 = x1 + button_size;
          y2 = y1 + button_size;

          if (area)
            gdk_gc_set_clip_rectangle (widget->style->bg_gc[widget->state], area);

          gdk_draw_rectangle (frame, widget->style->bg_gc[widget->state], TRUE,
                              x1, y1, x2 - x1, y2 - y1);

          gdk_draw_line (frame, widget->style->black_gc, x1 + 1, y2 - 2, x2 - 2, y2 - 2);

          if (area)
            gdk_gc_set_clip_rectangle (widget->style->black_gc, NULL);
        }

      /* Close button: */
      x1 = width - border_left - button_size;
      y1 = y_offset;
      x2 = width - border_left;
      y2 = y_offset + button_size;

      if (area)
	gdk_gc_set_clip_rectangle (widget->style->bg_gc[widget->state], area);

      gdk_draw_rectangle (frame, widget->style->bg_gc[widget->state], TRUE,
			  x1, y1, x2 - x1, y2 - y1);

      if (area)
        {
          gdk_gc_set_clip_rectangle (widget->style->bg_gc[widget->state], NULL);
          gdk_gc_set_clip_rectangle (widget->style->black_gc, area);
        }

      gdk_draw_line (frame, widget->style->black_gc, x1, y1, x2-1, y2-1);

      gdk_draw_line (frame, widget->style->black_gc, x1, y2-1, x2-1, y1);

      if (area)
	gdk_gc_set_clip_rectangle (widget->style->black_gc, NULL);
#endif

#if 0
      /* Title */
      if (deco->title_layout)
	{
	  if (area)
	    gdk_gc_set_clip_rectangle (widget->style->fg_gc [border_state], area);

	  gdk_draw_layout (frame,
			   widget->style->fg_gc [border_state],
			   border_left, 1,
			   deco->title_layout);
	  if (area)
	    gdk_gc_set_clip_rectangle (widget->style->fg_gc [border_state], NULL);
	}
#endif
    }
}


static void
gtk_decorated_window_recalculate_regions (GtkWindow *window)
{
  gint n_regions;
  gint width, height;
  GtkWindowRegion *region;
  GtkWindowDecoration *deco = get_decoration (window);
  gint border_left, border_right, border_top, border_bottom;
  gint button_size;
  gint y_offset;

  n_regions = 0;

  if (!deco->decorated)
    return;

  n_regions += 2; /* close, Title */
  if (deco->maximizable)
    n_regions += 1;
  if (deco->minimizable)
    n_regions += 1;
  if (window->allow_shrink || window->allow_grow)
    n_regions += 2;

  if (deco->n_regions != n_regions)
    {
      g_free (deco->regions);
      deco->regions = g_new (GtkWindowRegion, n_regions);
      deco->n_regions = n_regions;
    }

  gtk_widget_style_get (GTK_WIDGET (window),
                        "decoration-border-left",     &border_left,
                        "decoration-border-top",      &border_top,
                        "decoration-border-right",    &border_right,
                        "decoration-border-bottom",   &border_bottom,
                        "decoration-button-size",     &button_size,
                        "decoration-button-y-offset", &y_offset,
                        NULL);

  width = GTK_WIDGET (window)->allocation.width + border_left + border_right;
  height = GTK_WIDGET (window)->allocation.height + border_top + border_bottom;

  region = deco->regions;

  if (deco->minimizable)
    {
      region->rect.x = width - (border_left * 2) - (button_size * 3);
      region->rect.y = y_offset;
      region->rect.width = button_size;
      region->rect.height = button_size;
      region->type = GTK_WINDOW_REGION_MINIMIZE;
      region++;
    }

  /* Maximize button */
  if (deco->maximizable)
    {
      region->rect.x = width - (border_left * 2) - (button_size * 2);
      region->rect.y = y_offset;
      region->rect.width = button_size;
      region->rect.height = button_size;
      region->type = GTK_WINDOW_REGION_MAXIMIZE;
      region++;
    }

  /* Close button */
  region->rect.x = width - border_left - button_size;
  region->rect.y = y_offset;
  region->rect.width = button_size;
  region->rect.height = button_size;
  region->type = GTK_WINDOW_REGION_CLOSE;
  region++;

  /* title bar */
  region->rect.x = 0;
  region->rect.y = 0;
  region->rect.width = width;
  region->rect.height = border_top;
  region->type = GTK_WINDOW_REGION_TITLE;
  region++;

  if (window->allow_shrink || window->allow_grow)
    {
      region->rect.x = width - (border_right + 10);
      region->rect.y = height - border_bottom;
      region->rect.width = border_right + 10;
      region->rect.height = border_bottom;
      region->type = GTK_WINDOW_REGION_BR_RESIZE;
      region++;

      region->rect.x = width - border_right;
      region->rect.y = height - (border_bottom + 10);
      region->rect.width = border_right;
      region->rect.height = border_bottom + 10;
      region->type = GTK_WINDOW_REGION_BR_RESIZE;
      region++;
    }
}

void
gtk_decorated_window_move_resize_window (GtkWindow   *window,
					 gint         x,
					 gint         y,
					 gint         width,
					 gint         height)
{
  GtkWidget *widget = GTK_WIDGET (window);
  GtkWindowDecoration *deco = get_decoration (window);

  deco->real_inner_move = TRUE;
  gdk_window_move_resize (widget->window,
			  x, y, width, height);
}


#define __GTK_WINDOW_DECORATE_C__
#include "gtkaliasdef.c"
