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

#include "gtkprivate.h"
#include "gtksignal.h"
#include "gtkwindow.h"
#include "gtkmain.h"


#ifdef GDK_WINDOWING_FB
#define DECORATE_WINDOWS
#endif

#ifdef DECORATE_WINDOWS
#include "linux-fb/gdkfb.h"

typedef enum
{
  GTK_WINDOW_REGION_TITLE,
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
  
  PangoLayout *title_layout;

  GtkWindowResizeType resize;
  
  gboolean moving : 1;
  gboolean closing : 1;
  gboolean decorated : 1;
  gboolean real_inner_move : 1;
  gboolean focused : 1;
};

#define DECORATION_BORDER_TOP 15
#define DECORATION_BORDER_LEFT 3
#define DECORATION_BORDER_RIGHT 3
#define DECORATION_BORDER_BOTTOM 3
#define DECORATION_BORDER_TOT_X (DECORATION_BORDER_LEFT + DECORATION_BORDER_RIGHT)
#define DECORATION_BORDER_TOT_Y (DECORATION_BORDER_TOP + DECORATION_BORDER_BOTTOM)
#define DECORATION_BUTTON_SIZE 9
#define DECORATION_BUTTON_Y_OFFSET 2
#define DECORATION_TITLE_FONT "Sans 9"

static void gtk_decorated_window_recalculate_regions      (GtkWindow      *window);
static GtkWindowRegionType gtk_decorated_window_region_type    (GtkWindow      *window,
						 gint            x,
						 gint            y);
static gint gtk_decorated_window_frame_event    (GtkWidget *widget,
						 GdkEvent *event);
static gint gtk_decorated_window_button_press   (GtkWidget      *widget,
						 GdkEventButton *event);
static gint gtk_decorated_window_button_release (GtkWidget      *widget,
						 GdkEventButton *event);
static gint gtk_decorated_window_motion_notify  (GtkWidget      *widget,
						 GdkEventMotion *event);
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
  deco->real_inner_move = FALSE;
 
  g_object_set_data_full (G_OBJECT (window), "gtk-window-decoration", deco,
			  (GDestroyNotify) gtk_decoration_free);
  
  gtk_window_set_has_frame (window);

  gtk_signal_connect (GTK_OBJECT (window),
		      "frame_event",
		      GTK_SIGNAL_FUNC (gtk_decorated_window_frame_event),
		      window);
  gtk_signal_connect (GTK_OBJECT (window),
		      "focus_in_event",
		      GTK_SIGNAL_FUNC (gtk_decorated_window_focus_change),
		      window);
  gtk_signal_connect (GTK_OBJECT (window),
		      "focus_out_event",
		      GTK_SIGNAL_FUNC (gtk_decorated_window_focus_change),
		      window);
  gtk_signal_connect (GTK_OBJECT (window),
		      "realize",
		      GTK_SIGNAL_FUNC (gtk_decorated_window_realize),
		      window);
  gtk_signal_connect (GTK_OBJECT (window),
		      "unrealize",
		      GTK_SIGNAL_FUNC (gtk_decorated_window_unrealize),
		      window);
}

static inline GtkWindowDecoration *
get_decoration (GtkWindow *window)
{
  return (GtkWindowDecoration *)g_object_get_data (G_OBJECT (window), "gtk-window-decoration");
}

void
gtk_decorated_window_set_title (GtkWindow   *window,
				const gchar *title)
{
  GtkWindowDecoration *deco = get_decoration (window);
  
  if (deco->title_layout)
    pango_layout_set_text (deco->title_layout, title, -1);
}

void 
gtk_decorated_window_calculate_frame_size (GtkWindow *window)
{
  GdkWMDecoration decorations;
  GtkWindowDecoration *deco = get_decoration (window);
  
  if (gdk_window_get_decorations (GTK_WIDGET (window)->window,
				  &decorations))
    {
      if ((decorations & GDK_DECOR_BORDER) &&
	  (decorations & GDK_DECOR_TITLE))
	deco->decorated = TRUE;
      else
	deco->decorated = FALSE;
    }
  else
    deco->decorated = (window->type != GTK_WINDOW_POPUP);

  if (deco->decorated)
    gtk_window_set_frame_dimensions (window,
				     DECORATION_BORDER_LEFT,
				     DECORATION_BORDER_TOP,
				     DECORATION_BORDER_RIGHT,
				     DECORATION_BORDER_BOTTOM);
  else
    gtk_window_set_frame_dimensions (window, 0, 0, 0, 0);

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

  deco->title_layout = gtk_widget_create_pango_layout (widget,
						       (window->title)?window->title:"");
  
  pango_layout_set_font_description (deco->title_layout,
				     pango_font_description_from_string(DECORATION_TITLE_FONT));
  
  gdk_fb_window_set_child_handler (window->frame,
				   gtk_decorated_window_inner_change,
				   gtk_decorated_window_inner_get_pos,
				   window);
}

static void
gtk_decorated_window_unrealize (GtkWindow   *window)
{
  GtkWindowDecoration *deco = get_decoration (window);

  if (deco->title_layout)
    {
      g_object_unref (G_OBJECT (deco->title_layout));
      deco->title_layout = NULL;
    }
}

static gint
gtk_decorated_window_frame_event (GtkWidget *widget, GdkEvent *event)
{
  GtkWindowDecoration *deco = get_decoration (GTK_WINDOW (widget));
  GdkEventExpose *expose_event;

  switch (event->type)
    {
    case GDK_EXPOSE:
      expose_event = (GdkEventExpose *)event;
      if (deco->decorated)
	gtk_decorated_window_paint (widget, &expose_event->area);
      return TRUE;
      break;
    case GDK_CONFIGURE:
      gtk_decorated_window_recalculate_regions (GTK_WINDOW (widget));
      break;
    case GDK_MOTION_NOTIFY:
      return gtk_decorated_window_motion_notify (widget, (GdkEventMotion *)event);
      break;
    case GDK_BUTTON_PRESS:
      return gtk_decorated_window_button_press (widget, (GdkEventButton *)event);
      break;
    case GDK_BUTTON_RELEASE:
      return gtk_decorated_window_button_release (widget, (GdkEventButton *)event);
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
  return TRUE;
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
  
  window = GTK_WINDOW (widget);
  deco = get_decoration (window);
  
  if (!deco->decorated)
    return TRUE;
  
  win = widget->window;
  gdk_window_get_pointer (window->frame, &x, &y, &mask);
  
  gdk_window_get_position (window->frame, &win_x, &win_y);
  win_x += DECORATION_BORDER_LEFT;
  win_y += DECORATION_BORDER_TOP;
  
  gdk_window_get_geometry (win, NULL, NULL, &win_w, &win_h, NULL);

  if (deco->moving)
    {
      int dx, dy;
      dx = x - deco->last_x;
      dy = y - deco->last_y;

      gtk_window_reposition (window, win_x + dx, win_y + dy);
    }

  if (deco->resize != RESIZE_NONE)
    {
      int w, h;
      
      w = win_w;
      h = win_h;
      
      switch(deco->resize) {
      case RESIZE_BOTTOM_RIGHT:
	w = x - DECORATION_BORDER_TOT_X;
	h = y - DECORATION_BORDER_TOT_Y;
	break;
      case RESIZE_RIGHT:
	w = x - DECORATION_BORDER_TOT_X;
	break;
      case RESIZE_BOTTOM:
	h = y - DECORATION_BORDER_TOT_Y;
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
	return deco->regions[i].type;
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

  deco->last_x = x;
  deco->last_y = y;

  switch (type)
    {
    case GTK_WINDOW_REGION_TITLE:
      if (event->state & GDK_BUTTON1_MASK)
	deco->moving = TRUE;
      break;
    case GTK_WINDOW_REGION_CLOSE:
      if (event->state & GDK_BUTTON1_MASK)
	deco->closing = TRUE;
      break;
    case GTK_WINDOW_REGION_BR_RESIZE:
      if (event->state & GDK_BUTTON1_MASK)
	deco->resize = RESIZE_BOTTOM_RIGHT;
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
  deco = get_decoration (window);

  if (deco->closing)
    {
      type = gtk_decorated_window_region_type (window, event->x, event->y);
      if (type == GTK_WINDOW_REGION_CLOSE)
	{
	  GdkEventAny event;

	  event.type = GDK_DELETE;
	  event.window = widget->window;
	  event.send_event = TRUE;

	    /* Synthesize delete_event */
	  g_object_ref (G_OBJECT (event.window));
  
	  gtk_main_do_event ((GdkEvent*)&event);
	  
	  g_object_unref (G_OBJECT (event.window));
	}
    }
  
  deco->closing = FALSE;
  deco->moving = FALSE;
  deco->resize = RESIZE_NONE;
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

  if (deco->decorated)
    {
      GdkWindow *frame;
      gint width, height;

      frame = window->frame;
      gdk_window_get_size (frame, &width, &height);

      /* Top */
      gtk_paint_flat_box (widget->style, frame, GTK_STATE_NORMAL,
			  GTK_SHADOW_NONE, area, widget, "base",
			  0, 0,
			  width, DECORATION_BORDER_TOP);
      /* Bottom */
      gtk_paint_flat_box (widget->style, frame, GTK_STATE_NORMAL,
			  GTK_SHADOW_NONE, area, widget, "base",
			  0, height - DECORATION_BORDER_BOTTOM,
			  width, DECORATION_BORDER_BOTTOM);
      /* Left */
      gtk_paint_flat_box (widget->style, frame, GTK_STATE_NORMAL,
			  GTK_SHADOW_NONE, area, widget, "base",
			  0, DECORATION_BORDER_TOP,
			  DECORATION_BORDER_LEFT, height - DECORATION_BORDER_TOT_Y);
      /* Right */
      gtk_paint_flat_box (widget->style, frame, GTK_STATE_NORMAL,
			  GTK_SHADOW_NONE, area, widget, "base",
			  width - DECORATION_BORDER_RIGHT, DECORATION_BORDER_TOP,
			  DECORATION_BORDER_RIGHT, height - DECORATION_BORDER_TOT_Y);
      
      /* Border: */
      if (deco->focused)
	border_state = GTK_STATE_SELECTED;
      else 
	border_state = GTK_STATE_PRELIGHT;

      gtk_paint_box (widget->style, frame, border_state, 
		     GTK_SHADOW_OUT, area, widget, "base",
		     0, 0, width, height);
      
      gtk_paint_box (widget->style, frame, border_state, 
		     GTK_SHADOW_IN, area, widget, "base",
		     DECORATION_BORDER_LEFT - 2, DECORATION_BORDER_TOP - 2,
		     width - (DECORATION_BORDER_LEFT + DECORATION_BORDER_RIGHT) + 3,
		     height - (DECORATION_BORDER_TOP + DECORATION_BORDER_BOTTOM) + 3);
      
      /* Close button: */
      
      x1 = width - DECORATION_BORDER_LEFT - DECORATION_BUTTON_SIZE;
      y1 = DECORATION_BUTTON_Y_OFFSET;
      x2 = width - DECORATION_BORDER_LEFT;
      y2 = DECORATION_BUTTON_Y_OFFSET + DECORATION_BUTTON_SIZE;

      if (area)
	gdk_gc_set_clip_rectangle (widget->style->bg_gc[widget->state], area);

      gdk_draw_rectangle (frame, widget->style->bg_gc[widget->state], TRUE,
			  x1, y1, x2 - x1, y2 - y1);

      if (area)
	gdk_gc_set_clip_rectangle (widget->style->bg_gc[widget->state], NULL);
      
      if (area)
	gdk_gc_set_clip_rectangle (widget->style->black_gc, area);

      gdk_draw_line (frame, widget->style->black_gc, x1, y1, x2-1, y2-1);

      gdk_draw_line (frame, widget->style->black_gc, x1, y2-1, x2-1, y1);

      if (area)
	gdk_gc_set_clip_rectangle (widget->style->black_gc, NULL);
      
      

      /* Title */
      if (deco->title_layout)
	{
	  if (area)
	    gdk_gc_set_clip_rectangle (widget->style->fg_gc [widget->state], area);

	  gdk_draw_layout (frame,
			   widget->style->fg_gc [widget->state],
			   DECORATION_BORDER_LEFT, 1,
			   deco->title_layout);
	  if (area)
	    gdk_gc_set_clip_rectangle (widget->style->fg_gc [widget->state], NULL);
	}
      
    }
}


static void
gtk_decorated_window_recalculate_regions (GtkWindow *window)
{
  gint n_regions;
  gint width, height;
  GtkWindowRegion *region;
  GtkWindowDecoration *deco = get_decoration (window);
      
  n_regions = 0;

  if (!deco->decorated)
    return;
  
  n_regions += 2; /* close, Title */
  if (window->allow_shrink || window->allow_grow)
    n_regions += 2;

  if (deco->n_regions != n_regions)
    {
      g_free (deco->regions);
      deco->regions = g_new (GtkWindowRegion, n_regions);
      deco->n_regions = n_regions;
    }

  width = GTK_WIDGET (window)->allocation.width + DECORATION_BORDER_TOT_X;
  height = GTK_WIDGET (window)->allocation.height + DECORATION_BORDER_TOT_Y;

  region = deco->regions;

  /* Close button */
  region->rect.x = width - DECORATION_BORDER_LEFT - DECORATION_BUTTON_SIZE;
  region->rect.y = 2;
  region->rect.width = DECORATION_BUTTON_SIZE;
  region->rect.height = DECORATION_BUTTON_SIZE;
  region->type = GTK_WINDOW_REGION_CLOSE;
  region++;
    
  /* title bar */
  region->rect.x = 0;
  region->rect.y = 0;
  region->rect.width = width;
  region->rect.height = DECORATION_BORDER_TOP;
  region->type = GTK_WINDOW_REGION_TITLE;
  region++;
  
  if (window->allow_shrink || window->allow_grow)
    {
      region->rect.x = width - (DECORATION_BORDER_RIGHT + 10);
      region->rect.y = height - DECORATION_BORDER_BOTTOM;
      region->rect.width = DECORATION_BORDER_RIGHT + 10;
      region->rect.height = DECORATION_BORDER_BOTTOM;
      region->type = GTK_WINDOW_REGION_BR_RESIZE;
      region++;

      region->rect.x = width - DECORATION_BORDER_RIGHT;
      region->rect.y = height - (DECORATION_BORDER_BOTTOM + 10);
      region->rect.width = DECORATION_BORDER_RIGHT;
      region->rect.height = DECORATION_BORDER_BOTTOM + 10;
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
  gdk_window_move_resize (GTK_WIDGET (window)->window,
			  x, y, width, height);
}
#else

void
gtk_decorated_window_init (GtkWindow  *window)
{
}

void 
gtk_decorated_window_calculate_frame_size (GtkWindow *window)
{
}

void
gtk_decorated_window_set_title (GtkWindow   *window,
				const gchar *title)
{
}

void
gtk_decorated_window_move_resize_window (GtkWindow   *window,
					 gint         x,
					 gint         y,
					 gint         width,
					 gint         height)
{
  gdk_window_move_resize (GTK_WIDGET (window)->window,
			  x, y, width, height);
}
#endif



