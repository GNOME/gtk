/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include <gdk/gdk.h>
#include <gdk/gdkinternals.h>
#include "gdkinputprivate.h"
#include "gdkkeysyms.h"
#include "gdkprivate-fb.h"
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/vt.h>
#include <sys/time.h>
#include <sys/kd.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <linux/fb.h>

/* Two minutes */
#define BLANKING_TIMEOUT 120*1000

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

typedef struct {
  gint fd, fd_tag;

  gdouble x, y;
  GdkWindow *prev_window;
  gboolean button1_pressed, button2_pressed, button3_pressed;
  gboolean click_grab;

  guchar mouse_packet[5];
  int packet_nbytes;
} MouseDevice;

typedef struct {
  gint fd, fd_tag, consfd;

  int vtnum, prev_vtnum;
  guint modifier_state;
  gboolean caps_lock : 1;
} Keyboard;

static guint blanking_timer = 0;

static Keyboard * tty_keyboard_open(void);

static MouseDevice *gdk_fb_mouse = NULL;
static Keyboard *keyboard = NULL;

#ifndef VESA_NO_BLANKING
#define VESA_NO_BLANKING        0
#define VESA_VSYNC_SUSPEND      1
#define VESA_HSYNC_SUSPEND      2
#define VESA_POWERDOWN          3
#endif

#if 0
static gboolean
input_activity_timeout(gpointer p)
{
  blanking_timer = 0;
  ioctl(gdk_display->fd, FBIOBLANK, VESA_POWERDOWN);
  return FALSE;
}
#endif

/* This is all very broken :( */
static void
input_activity (void)
{
#if 0
  if (blanking_timer)
    g_source_remove (blanking_timer);
  else
    gdk_fb_redraw_all ();

  blanking_timer = g_timeout_add (BLANKING_TIMEOUT, input_activity_timeout, NULL);
#endif
}

static void
send_button_event (MouseDevice *mouse,
		   guint button,
		   gboolean press_event,
		   guint32 the_time)
{
  GdkEvent *event;
  gint x, y;
  GdkWindow *window;
  int nbuttons = 0;

  if (_gdk_fb_pointer_grab_window_events)
    window = _gdk_fb_pointer_grab_window_events;
  else
    window = gdk_window_at_pointer(NULL, NULL);

  event = gdk_event_make (window, press_event ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE, FALSE);

  if (event)
    {
      gdk_window_get_origin (window, &x, &y);
      x = mouse->x - x;
      y = mouse->y - y;
      
      event->button.x = x;
      event->button.y = y;
      event->button.button = button;
      event->button.state = (mouse->button1_pressed?GDK_BUTTON1_MASK:0) |
	(mouse->button2_pressed ? GDK_BUTTON2_MASK : 0) |
	(mouse->button3_pressed ? GDK_BUTTON3_MASK : 0) |
	(1 << (button + 8)) /* badhack */ |
	keyboard->modifier_state;
      event->button.device = gdk_core_pointer;
      event->button.x_root = mouse->x;
      event->button.y_root = mouse->y;
      
      event->button.time = the_time;
      
#if 0
      g_message ("Button #%d %s [%d, %d] in %p",
		 button, press_event?"pressed":"released",
		 x, y, window);
      
      /* Debugging aid */
      if (window && window != gdk_parent_root)
	{
	  GdkGC *tmp_gc;
	  
	  tmp_gc = gdk_gc_new (window);
	  GDK_GC_FBDATA (tmp_gc)->values.foreground.pixel = 0;
	  gdk_fb_draw_rectangle (GDK_DRAWABLE_IMPL(window), tmp_gc, TRUE, 0, 0,
				 GDK_DRAWABLE_IMPL_FBDATA(window)->width, GDK_DRAWABLE_IMPL_FBDATA(window)->height);
	  gdk_gc_unref(tmp_gc);
	}
#endif
      
      gdk_event_queue_append (event);
      
      /* For double-clicks */
      if (press_event)
	gdk_event_button_generate (event);
      
    }
  
  if (mouse->button1_pressed)
    nbuttons++;
  if (mouse->button2_pressed)
    nbuttons++;
  if (mouse->button3_pressed)
    nbuttons++;

  /* Handle implicit button grabs: */
  if (press_event && nbuttons == 1)
    {
      gdk_fb_pointer_grab (window, FALSE,
			   gdk_window_get_events (window),
			   NULL, NULL,
			   GDK_CURRENT_TIME, TRUE);
      mouse->click_grab = TRUE;
    }
  else if (!press_event && nbuttons == 0 && mouse->click_grab)
    {
      gdk_fb_pointer_ungrab (GDK_CURRENT_TIME, TRUE);
      mouse->click_grab = FALSE;
    }
}

static GdkPixmap *last_contents = NULL;
static GdkPoint last_location, last_contents_size;
static GdkCursor *last_cursor = NULL;
static GdkFBDrawingContext *gdk_fb_cursor_dc = NULL;
static GdkFBDrawingContext cursor_dc_dat;
static GdkGC *cursor_gc;
static gint cursor_visibility_count = 1;

static GdkFBDrawingContext *
gdk_fb_cursor_dc_reset (void)
{
  if (gdk_fb_cursor_dc)
    gdk_fb_drawing_context_finalize (gdk_fb_cursor_dc);

  gdk_fb_cursor_dc = &cursor_dc_dat;
  gdk_fb_drawing_context_init (gdk_fb_cursor_dc,
			       GDK_DRAWABLE_IMPL(gdk_parent_root),
			       cursor_gc,
			       TRUE,
			       FALSE);

  return gdk_fb_cursor_dc;
}

void
gdk_fb_cursor_hide (void)
{
  GdkFBDrawingContext *mydc = gdk_fb_cursor_dc;

  cursor_visibility_count--;
  g_assert (cursor_visibility_count <= 0);
  
  if (cursor_visibility_count < 0)
    return;

  if (!mydc)
    mydc = gdk_fb_cursor_dc_reset ();

  if (last_contents)
    {
      gdk_gc_set_clip_mask (cursor_gc, NULL);
      /* Restore old picture */
      gdk_fb_draw_drawable_3 (GDK_DRAWABLE_IMPL(gdk_parent_root),
			      cursor_gc,
			      GDK_DRAWABLE_IMPL(last_contents),
			      mydc,
			      0, 0,
			      last_location.x,
			      last_location.y,
			      last_contents_size.x,
			      last_contents_size.y);
    }
}

void
gdk_fb_cursor_invalidate (void)
{
  if (last_contents)
    {
      gdk_pixmap_unref (last_contents);
      last_contents = NULL;
    }
}

void
gdk_fb_cursor_unhide()
{
  GdkFBDrawingContext *mydc = gdk_fb_cursor_dc;
  GdkCursorPrivateFB *last_private;
  GdkDrawableFBData *pixmap_last;
  
  last_private = GDK_CURSOR_FB (last_cursor);
  pixmap_last = GDK_DRAWABLE_IMPL_FBDATA (last_private->cursor);
  cursor_visibility_count++;
  g_assert (cursor_visibility_count <= 1);
  if (cursor_visibility_count < 1)
    return;

  if (!mydc)
    mydc = gdk_fb_cursor_dc_reset ();

  if (last_cursor)
    {
      if (!last_contents ||
	  pixmap_last->width > GDK_DRAWABLE_IMPL_FBDATA (last_contents)->width ||
	  pixmap_last->height > GDK_DRAWABLE_IMPL_FBDATA (last_contents)->height)
	{
	  if (last_contents)
	    gdk_pixmap_unref (last_contents);

	  last_contents = gdk_pixmap_new (gdk_parent_root,
					  pixmap_last->width,
					  pixmap_last->height,
					  GDK_DRAWABLE_IMPL_FBDATA (gdk_parent_root)->depth);
	}

      gdk_gc_set_clip_mask (cursor_gc, NULL);
      gdk_fb_draw_drawable_2 (GDK_DRAWABLE_IMPL (last_contents),
			      cursor_gc,
			      GDK_DRAWABLE_IMPL (gdk_parent_root),
			      last_location.x,
			      last_location.y,
			      0, 0,
			      pixmap_last->width,
			      pixmap_last->height,
			      TRUE, FALSE);
      last_contents_size.x = pixmap_last->width;
      last_contents_size.y = pixmap_last->height;
      
      gdk_gc_set_clip_mask (cursor_gc, last_private->mask);
      gdk_gc_set_clip_origin (cursor_gc,
			      last_location.x,
			      last_location.y);

      gdk_fb_cursor_dc_reset ();
      gdk_fb_draw_drawable_3 (GDK_DRAWABLE_IMPL (gdk_parent_root),
			      cursor_gc,
			      GDK_DRAWABLE_IMPL (last_private->cursor),
			      mydc,
			      0, 0,
			      last_location.x, last_location.y,
			      pixmap_last->width,
			      pixmap_last->height);
    }
  else
    gdk_fb_cursor_invalidate ();
}

gboolean
gdk_fb_cursor_region_need_hide (GdkRegion *region)
{
  GdkRectangle testme;

  if (!last_cursor)
    return FALSE;

  testme.x = last_location.x;
  testme.y = last_location.y;
  testme.width = GDK_DRAWABLE_IMPL_FBDATA (GDK_CURSOR_FB (last_cursor)->cursor)->width;
  testme.height = GDK_DRAWABLE_IMPL_FBDATA (GDK_CURSOR_FB (last_cursor)->cursor)->height;

  return (gdk_region_rect_in (region, &testme) != GDK_OVERLAP_RECTANGLE_OUT);
}

gboolean
gdk_fb_cursor_need_hide (GdkRectangle *rect)
{
  GdkRectangle testme;

  if (!last_cursor)
    return FALSE;

  testme.x = last_location.x;
  testme.y = last_location.y;
  testme.width = GDK_DRAWABLE_IMPL_FBDATA (GDK_CURSOR_FB (last_cursor)->cursor)->width;
  testme.height = GDK_DRAWABLE_IMPL_FBDATA (GDK_CURSOR_FB (last_cursor)->cursor)->height;

  return gdk_rectangle_intersect (rect, &testme, &testme);
}

void
gdk_fb_get_cursor_rect (GdkRectangle *rect)
{
  if (last_cursor)
    {
      rect->x = last_location.x;
      rect->y = last_location.y;
      rect->width = GDK_DRAWABLE_IMPL_FBDATA (GDK_CURSOR_FB (last_cursor)->cursor)->width;
      rect->height = GDK_DRAWABLE_IMPL_FBDATA (GDK_CURSOR_FB (last_cursor)->cursor)->height;
    }
  else
    {
      rect->x = rect->y = -1;
      rect->width = rect->height = 0;
    }
}

static void
move_pointer (MouseDevice *mouse, GdkWindow *in_window)
{
  GdkCursor *the_cursor;

  if (!cursor_gc)
    {
      GdkColor white, black;
      cursor_gc = gdk_gc_new (gdk_parent_root);
      gdk_color_black (gdk_colormap_get_system (), &black);
      gdk_color_white (gdk_colormap_get_system (), &white);
      gdk_gc_set_foreground (cursor_gc, &black);
      gdk_gc_set_background (cursor_gc, &white);
    }

  gdk_fb_cursor_hide ();

  if (_gdk_fb_pointer_grab_window)
    {
      if (_gdk_fb_pointer_grab_cursor)
	the_cursor = _gdk_fb_pointer_grab_cursor;
      else
	{
	  GdkWindow *win = _gdk_fb_pointer_grab_window;
	  while (!GDK_WINDOW_IMPL_FBDATA (win)->cursor && GDK_WINDOW_OBJECT (win)->parent)
	    win = (GdkWindow *)GDK_WINDOW_OBJECT (win)->parent;
	  the_cursor = GDK_WINDOW_IMPL_FBDATA (win)->cursor;
	}
    }
  else
    {
      while (!GDK_WINDOW_IMPL_FBDATA (in_window)->cursor && GDK_WINDOW_P (in_window)->parent)
	in_window = (GdkWindow *)GDK_WINDOW_P (in_window)->parent;
      the_cursor = GDK_WINDOW_IMPL_FBDATA (in_window)->cursor;
    }

  last_location.x = mouse->x - GDK_CURSOR_FB (the_cursor)->hot_x;
  last_location.y = mouse->y - GDK_CURSOR_FB (the_cursor)->hot_y;

  if (the_cursor)
    gdk_cursor_ref (the_cursor);
  if (last_cursor)
    gdk_cursor_unref (last_cursor);
  last_cursor = the_cursor;

  gdk_fb_cursor_unhide ();
}

void
gdk_fb_cursor_reset(void)
{
  GdkWindow *win = gdk_window_at_pointer (NULL, NULL);

  move_pointer (gdk_fb_mouse, win);
}

void
gdk_fb_window_send_crossing_events (GdkWindow *dest,
				    GdkCrossingMode mode)
{
  GdkWindow *c;
  GdkWindow *win, *last, *next;
  GdkEvent *event;
  gint x, y, x_int, y_int;
  GdkModifierType my_mask;
  GList *path, *list;
  gboolean non_linear;
  gboolean only_grabbed_window;
  GdkWindow *a;
  GdkWindow *b;

  if (gdk_fb_mouse->prev_window == NULL)
    gdk_fb_mouse->prev_window = gdk_window_ref (gdk_parent_root);

  if (mode == GDK_CROSSING_UNGRAB)
    a = _gdk_fb_pointer_grab_window;
  else
    a = gdk_fb_mouse->prev_window;
  b = dest;

  /* When grab in progress only send normal crossing events about
   * the grabbed window.
   */
  only_grabbed_window = (_gdk_fb_pointer_grab_window_events != NULL) &&
                        (mode == GDK_CROSSING_NORMAL);
  
  if (a==b)
    return;

  gdk_input_get_mouseinfo (&x, &y, &my_mask);

  c = gdk_fb_find_common_ancestor (a, b);

  non_linear = (c != a) && (c != b);

  if (!only_grabbed_window || (a == _gdk_fb_pointer_grab_window))
    event = gdk_event_make (a, GDK_LEAVE_NOTIFY, TRUE);
  else
    event = NULL;
  if (event)
    {
      event->crossing.subwindow = NULL;
      gdk_window_get_root_origin (a, &x_int, &y_int);
      event->crossing.x = x - x_int;
      event->crossing.y = y - y_int;
      event->crossing.x_root = x;
      event->crossing.y_root = y;
      event->crossing.mode = mode;
      if (non_linear)
	event->crossing.detail = GDK_NOTIFY_NONLINEAR;
      else if (c==a)
	event->crossing.detail = GDK_NOTIFY_INFERIOR;
      else
	event->crossing.detail = GDK_NOTIFY_ANCESTOR;
      event->crossing.focus = FALSE;
      event->crossing.state = my_mask;
    }
  
  /* Traverse up from a to (excluding) c */
  if (c != a)
    {
      last = a;
      win = GDK_WINDOW (GDK_WINDOW_OBJECT (a)->parent);
      while (win != c)
	{
	  if (!only_grabbed_window || (win == _gdk_fb_pointer_grab_window))
	    event = gdk_event_make (win, GDK_LEAVE_NOTIFY, TRUE);
	  else
	    event = NULL;
	  if (event)
	    {
	      event->crossing.subwindow = gdk_window_ref (last);
	      gdk_window_get_root_origin (win, &x_int, &y_int);
	      event->crossing.x = x - x_int;
	      event->crossing.y = y - y_int;
	      event->crossing.x_root = x;
	      event->crossing.y_root = y;
	      event->crossing.mode = mode;
	      if (non_linear)
		event->crossing.detail = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	      else
		event->crossing.detail = GDK_NOTIFY_VIRTUAL;
	      event->crossing.focus = FALSE;
	      event->crossing.state = my_mask;
	    }
	  last = win;
	  win = GDK_WINDOW (GDK_WINDOW_OBJECT (win)->parent);
	}
    }
  
  /* Traverse down from c to b */
  if (c != b) 
    {
      path = NULL;
      win = GDK_WINDOW( GDK_WINDOW_OBJECT (b)->parent);
      while (win != c)
	{
	  path = g_list_prepend (path, win);
	  win = GDK_WINDOW( GDK_WINDOW_OBJECT (win)->parent);
	}
      
      list = path;
      while (list) 
	{
	  win = (GdkWindow *)list->data;
	  list = g_list_next (list);
	  if (list)
	    next = (GdkWindow *)list->data;
	  else 
	    next = b;
	  
	  if (!only_grabbed_window || (win == _gdk_fb_pointer_grab_window))
	    event = gdk_event_make (win, GDK_ENTER_NOTIFY, TRUE);
	  else
	    event = NULL;
	  if (event)
	    {
	      event->crossing.subwindow = gdk_window_ref (next);
	      gdk_window_get_root_origin (win, &x_int, &y_int);
	      event->crossing.x = x - x_int;
	      event->crossing.y = y - y_int;
	      event->crossing.x_root = x;
	      event->crossing.y_root = y;
	      event->crossing.mode = mode;
	      if (non_linear)
		event->crossing.detail = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	      else
		event->crossing.detail = GDK_NOTIFY_VIRTUAL;
	      event->crossing.focus = FALSE;
	      event->crossing.state = my_mask;
	    }
	}
      g_list_free (path);
    }

  if (!only_grabbed_window || (b == _gdk_fb_pointer_grab_window))
    event = gdk_event_make (b, GDK_ENTER_NOTIFY, TRUE);
  else
    event = NULL;
  if (event)
    {
      event->crossing.subwindow = NULL;
      gdk_window_get_root_origin (b, &x_int, &y_int);
      event->crossing.x = x - x_int;
      event->crossing.y = y - y_int;
      event->crossing.x_root = x;
      event->crossing.y_root = y;
      event->crossing.mode = mode;
      if (non_linear)
	event->crossing.detail = GDK_NOTIFY_NONLINEAR;
      else if (c==a)
	event->crossing.detail = GDK_NOTIFY_ANCESTOR;
      else
	event->crossing.detail = GDK_NOTIFY_INFERIOR;
      event->crossing.focus = FALSE;
      event->crossing.state = my_mask;
    }

  if ((mode != GDK_CROSSING_GRAB) &&
      (b != gdk_fb_mouse->prev_window))
    {
      gdk_window_unref (gdk_fb_mouse->prev_window);
      gdk_fb_mouse->prev_window = gdk_window_ref (b);
    }
}

static void
handle_mouse_input(MouseDevice *mouse,
		   gboolean got_motion)
{
  GdkWindow *mousewin;
  GdkEvent *event;
  gint x, y;
  GdkWindow *win, *grabwin;
  guint state;

  if (_gdk_fb_pointer_grab_confine)
    mousewin = _gdk_fb_pointer_grab_confine;
  else
    mousewin = gdk_parent_root;

  if (mouse->x < GDK_DRAWABLE_IMPL_FBDATA (mousewin)->llim_x)
    mouse->x = GDK_DRAWABLE_IMPL_FBDATA (mousewin)->llim_x;
  else if (mouse->x > (GDK_DRAWABLE_IMPL_FBDATA (mousewin)->lim_x - 1))
    mouse->x = GDK_DRAWABLE_IMPL_FBDATA (mousewin)->lim_x - 1;

  if (mouse->y < GDK_DRAWABLE_IMPL_FBDATA (mousewin)->llim_y)
    mouse->y = GDK_DRAWABLE_IMPL_FBDATA (mousewin)->llim_y;
  else if (mouse->y > (GDK_DRAWABLE_IMPL_FBDATA (mousewin)->lim_y - 1))
    mouse->y = GDK_DRAWABLE_IMPL_FBDATA (mousewin)->lim_y - 1;

  if (!got_motion)
    return;

  win = gdk_window_at_pointer (NULL, NULL);
  if (_gdk_fb_pointer_grab_window_events)
    grabwin = _gdk_fb_pointer_grab_window_events;
  else
    grabwin = win;

  move_pointer (mouse, grabwin);
  
  gdk_window_get_origin (grabwin, &x, &y);
  x = mouse->x - x;
  y = mouse->y - y;


  state = (mouse->button1_pressed?GDK_BUTTON1_MASK:0) |
    (mouse->button2_pressed?GDK_BUTTON2_MASK:0) |
    (mouse->button3_pressed?GDK_BUTTON3_MASK:0) |
    keyboard->modifier_state;

  event = gdk_event_make (grabwin, GDK_MOTION_NOTIFY, TRUE);
  if (event)
    {
      event->motion.x = x;
      event->motion.y = y;
      event->motion.state = state;
      event->motion.is_hint = FALSE;
      event->motion.device = gdk_core_pointer;
      event->motion.x_root = mouse->x;
      event->motion.y_root = mouse->y;
      event->motion.time = gdk_fb_get_time ();
    }

  if (win != mouse->prev_window)
    gdk_fb_window_send_crossing_events (win,
					GDK_CROSSING_NORMAL);

  input_activity ();
}

static gboolean
pull_fidmour_packet (MouseDevice *mouse,
		     gboolean *btn_down,
		     gdouble *x,
		     gdouble *y)
{
  gboolean keep_reading = TRUE;

  while (keep_reading)
    {
      int n;

      n = read (mouse->fd, mouse->mouse_packet + mouse->packet_nbytes, 5 - mouse->packet_nbytes);
      if (n < 0)
	return FALSE;
      else if (n == 0)
	{
	  g_error ("EOF on mouse device!");
	  g_source_remove (mouse->fd_tag);
	  return FALSE;
	}

      mouse->packet_nbytes += n;

      n = 0;
      if (!(mouse->mouse_packet[0] & 0x80))
	{
	  int i;
	  /* We haven't received any of the packet yet but there is no header at the beginning */
	  for (i = 1; i < mouse->packet_nbytes; i++)
	    {
	      if (mouse->mouse_packet[i] & 0x80)
		{
		  n = i;
		  break;
		}
	    }
	}
      else if (mouse->packet_nbytes > 1 &&
	       ((mouse->mouse_packet[0] & 0x90) == 0x90))
	{
	  /* eat the 0x90 and following byte, no clue what it's for */
	  n = 2;
	}
      else if (mouse->packet_nbytes == 5)
	{
	  switch (mouse->mouse_packet[0] & 0xF)
	    {
	    case 2:
	      *btn_down = 0;
	      break;
	    case 1:
	    case 0:
	      *btn_down = 1;
	      break;
	    default:
	      g_assert_not_reached ();
	      break;
	    }

	  *x = mouse->mouse_packet[1] + (mouse->mouse_packet[2] << 7);
	  if (*x > 8192)
	    *x -= 16384;
	  *y = mouse->mouse_packet[3] + (mouse->mouse_packet[4] << 7);
	  if (*y > 8192)
	    *y -= 16384;
	  /* Now map touchscreen coords to screen coords */
	  *x *= ((double)gdk_display->modeinfo.xres)/4096.0;
	  *y *= ((double)gdk_display->modeinfo.yres)/4096.0;
	  n = 5;
	  keep_reading = FALSE;
	}

      if (n)
	{
	  memmove (mouse->mouse_packet, mouse->mouse_packet+n, mouse->packet_nbytes-n);
	  mouse->packet_nbytes -= n;
	}
    }

  return TRUE;
}

static gboolean
handle_input_fidmour (GIOChannel *gioc,
		      GIOCondition cond,
		      gpointer data)
{
  MouseDevice *mouse = data;
  gdouble x, y, oldx, oldy;
  gboolean got_motion = FALSE;
  gboolean btn_down;
  guint32 the_time;

  the_time = gdk_fb_get_time ();
  
  oldx = mouse->x;
  oldy = mouse->y;
  while (pull_fidmour_packet (mouse, &btn_down, &x, &y))
    {
      if (fabs(x - mouse->x) >= 1.0 || fabs(x - mouse->y) >= 1.0)
	{
	  got_motion = TRUE;
	  mouse->x = x;
	  mouse->y = y;
	}

      if (btn_down != mouse->button1_pressed)
	{
	  if (got_motion)
	    {
	      handle_mouse_input (mouse, TRUE);
	      got_motion = FALSE;
	    }

	  mouse->button1_pressed = btn_down;
	  send_button_event (mouse, 1, btn_down, the_time);
	}
    }

  if (got_motion)
    handle_mouse_input (mouse, TRUE);

  return TRUE;
}

static gboolean
handle_input_ps2 (GIOChannel *gioc,
		  GIOCondition cond,
		  gpointer data)
{
  MouseDevice *mouse = data;
  int n, dx=0, dy=0;
  gboolean new_button1, new_button2, new_button3;
  guint32 the_time;
  gboolean got_motion = FALSE;
  guchar *buf;

  the_time = gdk_fb_get_time ();

  while (1) /* Go through as many mouse events as we can */
    {
      n = read (mouse->fd, mouse->mouse_packet + mouse->packet_nbytes, 3 - mouse->packet_nbytes);
      if (n<=0) /* error or nothing to read */
	break;
      
      mouse->packet_nbytes += n;
      
      if (mouse->packet_nbytes < 3) /* Mouse packet not finished */
	break;

      mouse->packet_nbytes = 0;

      /* Finished reading a packet */
      buf = mouse->mouse_packet;
      
      new_button1 = (buf[0] & 1) && 1;
      new_button3 = (buf[0] & 2) && 1;
      new_button2 = (buf[0] & 4) && 1;

      if (got_motion &&
	  (new_button1 != mouse->button1_pressed ||
	   new_button2 != mouse->button2_pressed ||
	   new_button3 != mouse->button3_pressed))
	{
	  /* If a mouse button state changes we need to get correct ordering with enter/leave events,
	     so push those out via handle_mouse_input */
	  got_motion = FALSE;
	  handle_mouse_input (mouse, TRUE);
	}

      if (new_button1 != mouse->button1_pressed)
	{
	  mouse->button1_pressed = new_button1; 
	  send_button_event (mouse, 1, new_button1, the_time);
	}

      if (new_button2 != mouse->button2_pressed)
	{
	  mouse->button2_pressed = new_button2;
	  send_button_event (mouse, 2, new_button2, the_time);
	}

      if (new_button3 != mouse->button3_pressed)
	{
	  mouse->button3_pressed = new_button3; 
	  send_button_event (mouse, 3, new_button3, the_time);
	}

      if (buf[1] != 0)
	dx = ((buf[0] & 0x10) ? ((gint)buf[1])-256 : buf[1]);
      else
	dx = 0;
      if (buf[2] != 0)
	dy = -((buf[0] & 0x20) ? ((gint)buf[2])-256 : buf[2]);
      else
	dy = 0;

      mouse->x += dx;
      mouse->y += dy;
      if (dx || dy)
	got_motion = TRUE;
    }

  if (got_motion)
    handle_mouse_input (mouse, TRUE);

  return TRUE;
}

static gboolean
handle_input_ms (GIOChannel *gioc,
		 GIOCondition cond,
		 gpointer data)
{
  MouseDevice *mouse = data;
  guchar byte1, byte2, byte3;
  int n, dx=0, dy=0;
  gboolean new_button1, new_button2, new_button3;
  guint32 the_time;

  the_time = gdk_fb_get_time ();

  n = read (mouse->fd, &byte1, 1);
  if ( (n!=1) || (byte1 & 0x40) != 0x40)
    return TRUE;
  
  n = read (mouse->fd, &byte2, 1);
  if ( (n!=1) || (byte2 & 0x40) != 0x00)
    return TRUE;
  
  n = read (mouse->fd, &byte3, 1);
  if (n!=1)
    return TRUE;
  
  new_button1 = (byte1 & 0x20) && 1;
  new_button2 = (byte1 & 0x10) && 1;
  new_button3 = 0;
  
  if (new_button1 != mouse->button1_pressed)
    {
      mouse->button1_pressed = new_button1; 
      send_button_event (mouse, 1, new_button1, the_time);
    }
  
  if (new_button2 != mouse->button2_pressed)
    {
      mouse->button2_pressed = new_button2;
      send_button_event (mouse, 2, new_button2, the_time);
    }
  
  if (new_button3 != mouse->button3_pressed)
    {
      mouse->button3_pressed = new_button3; 
      send_button_event (mouse, 3, new_button3, the_time);
    }
  
  dx = (signed char)(((byte1 & 0x03) << 6) | (byte2 & 0x3F));
  dy = (signed char)(((byte1 & 0x0C) << 4) | (byte3 & 0x3F));
  
  mouse->x += dx;
  mouse->y += dy;
  
  if (dx || dy)
    handle_mouse_input (mouse, TRUE);
  
  return TRUE;
}

static MouseDevice *
mouse_open(void)
{
  MouseDevice *retval;
  guchar buf[7];
  int i = 0;
  GIOChannel *gioc;
  char *mousedev, *ctmp;
  int mode;
  struct termios tty;
  enum { PS2_MOUSE, FIDMOUR_MOUSE, MS_MOUSE, UNKNOWN_MOUSE } type;
  int flags;
  fd_set fds;
  char c;
  struct timeval tv;
      
  retval = g_new0 (MouseDevice, 1);
  retval->fd = -1;
  mode = O_RDWR;
  ctmp = getenv ("GDK_MOUSETYPE");
  if (ctmp)
    {
      if (!strcmp (ctmp, "fidmour"))
	type = FIDMOUR_MOUSE;
      else if (!strcmp (ctmp, "ps2"))
	type = PS2_MOUSE;
      else if (!strcmp (ctmp, "ms"))
	type = MS_MOUSE;
      else
	{
	  g_print ("Unknown mouse type %s\n", ctmp);
	  type = UNKNOWN_MOUSE;
	}
    }
  else
    type = PS2_MOUSE;

  switch (type)
    {
    case PS2_MOUSE:
      mousedev = "/dev/psaux";
      mode = O_RDWR;
      break;
    case MS_MOUSE:
      mousedev = "/dev/ttyS0";
      mode = O_RDWR;
      break;
    case FIDMOUR_MOUSE:
      mousedev = "/dev/fidmour";
      mode = O_RDONLY;
      break;
    default:
      goto error;
      break;
    }

  ctmp = getenv ("GDK_MOUSEDEV");
  if (ctmp)
    mousedev = ctmp;

  /* Use nonblocking mode to open, to not hang on device */
  retval->fd = open (mousedev, mode | O_NONBLOCK);
  if (retval->fd < 0)
    goto error;

  flags = fcntl (retval->fd, F_GETFL);
  fcntl (retval->fd, F_SETFL, flags & ~O_NONBLOCK);

  switch (type)
    {
    case PS2_MOUSE:
      /* From xf86_Mouse.c */
      buf[i++] = 230; /* 1:1 scaling */
      buf[i++] = 244; /* enable mouse */
      buf[i++] = 243; /* Sample rate */
      buf[i++] = 200;
      buf[i++] = 232; /* device resolution */
      buf[i++] = 1;
      write (retval->fd, buf, i);
      fcntl (retval->fd, F_SETFL, O_RDWR|O_NONBLOCK);

      usleep (10000); /* sleep 10 ms, then read whatever junk we can get from the mouse, in a vain attempt
			to get synchronized with the event stream */
      while ((i = read (retval->fd, buf, sizeof(buf))) > 0)
	g_print ("Got %d bytes of junk from psaux\n", i);

      gioc = g_io_channel_unix_new (retval->fd);
      retval->fd_tag = g_io_add_watch (gioc, G_IO_IN|G_IO_ERR|G_IO_HUP|G_IO_NVAL, handle_input_ps2, retval);
      break;

    case MS_MOUSE:
      /* Read all data from fd: */
      FD_ZERO (&fds);
      FD_SET (retval->fd, &fds);
      tv.tv_sec = 0;
      tv.tv_usec = 0;
      while (select (retval->fd+1, &fds, NULL, NULL, &tv) > 0) {
	FD_ZERO (&fds);
	FD_SET (retval->fd, &fds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	read (retval->fd, &c, 1);
      } 

      tcgetattr (retval->fd, &tty);
      tty.c_iflag = IGNBRK | IGNPAR;
      tty.c_cflag = CREAD|CLOCAL|HUPCL|CS7|B1200;
      tty.c_oflag = 0;
      tty.c_lflag = 0;
      tty.c_line = 0;
      tty.c_cc[VTIME] = 0;
      tty.c_cc[VMIN] = 1;
      tcsetattr (retval->fd, TCSAFLUSH, &tty);

      write (retval->fd, "*n", 2);
      
      gioc = g_io_channel_unix_new (retval->fd);
      retval->fd_tag = g_io_add_watch (gioc, G_IO_IN|G_IO_ERR|G_IO_HUP|G_IO_NVAL, handle_input_ms, retval);
      break;

    case FIDMOUR_MOUSE:
      fcntl (retval->fd, F_SETFL, O_RDONLY|O_NONBLOCK);
      gioc = g_io_channel_unix_new (retval->fd);
      /* We set the priority lower here because otherwise it will flood out all the other stuff */
      retval->fd_tag = g_io_add_watch_full (gioc, G_PRIORITY_DEFAULT, G_IO_IN|G_IO_ERR|G_IO_HUP|G_IO_NVAL,
					   handle_input_fidmour, retval, NULL);
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  retval->x = gdk_display->modeinfo.xres >> 1;
  retval->y = gdk_display->modeinfo.yres >> 1;

  return retval;

 error:
  /* No errors allowed once fd_tag is added */
  g_warning ("Failed to open mouse device\n");
  if (retval->fd >= 0)
    close (retval->fd);
  g_free (retval);
  return NULL;
}

void
gdk_input_init (void)
{
  gdk_input_devices = g_list_append (NULL, gdk_core_pointer);

  gdk_input_ignore_core = FALSE;

  gdk_fb_mouse = mouse_open ();
}

void
gdk_input_get_mouseinfo (gint *x,
			 gint *y,
			 GdkModifierType *mask)
{
  *x = gdk_fb_mouse->x;
  *y = gdk_fb_mouse->y;
  *mask =
    (gdk_fb_mouse->button1_pressed?GDK_BUTTON1_MASK:0) |
    (gdk_fb_mouse->button2_pressed?GDK_BUTTON2_MASK:0) |
    (gdk_fb_mouse->button3_pressed?GDK_BUTTON3_MASK:0) |
    keyboard->modifier_state;
}

GdkWindow *
gdk_window_find_focus (void)
{
  if (_gdk_fb_keyboard_grab_window)
    return _gdk_fb_keyboard_grab_window;
  else if (GDK_WINDOW_P (gdk_parent_root)->children)
    {
      GList *item;
      for (item = GDK_WINDOW_P (gdk_parent_root)->children; item; item = item->next)
	{
	  GdkWindowObject *priv = item->data;

	  if (priv->mapped)
	    return item->data;
	}
    }

  return gdk_parent_root;
}

static const guint trans_table[256][3] = {
  /* 0x00 */
  {0, 0, 0},
  {GDK_Escape, 0, 0},
  {'1', '!', 0},
  {'2', '@', 0},
  {'3', '#', 0},
  {'4', '$', 0},
  {'5', '%', 0},
  {'6', '^', 0},
  {'7', '&', 0},
  {'8', '*', 0},
  {'9', '(', 0},
  {'0', ')', 0},
  {'-', '_', 0},
  {'=', '+', 0},
  {GDK_BackSpace, 0, 0},
  {GDK_Tab, 0, 0},

  /* 0x10 */
  {'q', 'Q', 0},
  {'w', 'W', 0},
  {'e', 'E', 0},
  {'r', 'R', 0},
  {'t', 'T', 0},
  {'y', 'Y', 0},
  {'u', 'U', 0},
  {'i', 'I', 0},
  {'o', 'O', 0},
  {'p', 'P', 0},
  {'[', '{', 0},
  {']', '}', 0},
  {GDK_Return, 0, 0},
  {GDK_Control_L, 0, 0}, /* mod */
  {'a', 'A', 0},
  {'s', 'S', 0},

	/* 0x20 */
  {'d', 'D', 0},
  {'f', 'F', 0},
  {'g', 'G', 0},
  {'h', 'H', 0},
  {'j', 'J', 0},
  {'k', 'K', 0},
  {'l', 'L', 0},
  {';', ':', 0},
  {'\'', '"', 0},
  {'`', '~', 0},
  {GDK_Shift_L, 0, 0}, /* mod */
  {'\\', 0, 0},
  {'z', 0, 0},
  {'x', 0, 0},
  {'c', 0, 0},

  {'v', 'V', 0},

	/* 0x30 */
  {'b', 'B', 0},
  {'n', 'N', 0},
  {'m', 'M', 0},
  {',', 0, 0},
  {'.', 0, 0},
  {'/', 0, 0},
  {GDK_Shift_R, 0, 0}, /* mod */
  {GDK_KP_Multiply, 0, 0},
  {0, 0, 0},
  {GDK_space, 0, 0},
  {0, 0, 0},
  {GDK_F1, 0, 0},
  {GDK_F2, 0, 0},
  {GDK_F3, 0, 0},
  {GDK_F4, 0, 0},
  {GDK_F5, 0, 0},

	/* 0x40 */
  {GDK_F6, 0, 0},
  {GDK_F7, 0, 0},
  {GDK_F8, 0, 0},
  {GDK_F9, 0, 0},
  {GDK_F10, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {'7', 0, 0},
  {'8', 0, 0},
  {'9', 0, 0},
  {'-', 0, 0},
  {'4', 0, 0},
  {'5', 0, 0},
  {'6', 0, 0},
  {'+', 0, 0},
  {'1', 0, 0},

	/* 0x50 */
  {'2', 0, 0},
  {'3', 0, 0},
  {'0', 0, 0},
  {'.', 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {GDK_F11, 0, 0},
  {GDK_F12, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0x60 */
  {GDK_Return, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0x70 */
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0x80 */
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0x90 */
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0xA0 */
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0xB0 */
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0xC0 */
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {GDK_Up, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {GDK_Left, 0, 0},
  {0, 0, 0},
  {GDK_Right, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0xD0 */
  {GDK_Down, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0xE0 */
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0xF0 */
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
};

#define TRANS_TABLE_SIZE (sizeof(trans_table)/sizeof(trans_table[0]))

static gboolean
handle_mediumraw_keyboard_input (GIOChannel *gioc,
				 GIOCondition cond,
				 gpointer data)
{
  guchar buf[128];
  int i, n;
  Keyboard *k = data;
  guint32 now;

  n = read (k->fd, buf, sizeof(buf));
  if (n <= 0)
    g_error("Nothing from keyboard!");

  /* Now turn this into a keyboard event */
  now = gdk_fb_get_time ();

  for (i = 0; i < n; i++)
    {
      guchar keycode;
      gboolean key_up;
      GdkEvent *event;
      GdkWindow *win;
      char dummy[2];
      int mod;
      guint keyval;

      keycode = buf[i] & 0x7F;
      key_up = buf[i] & 0x80;

      if (keycode > TRANS_TABLE_SIZE)
	{
	  g_warning ("Unknown keycode\n");
	  continue;
	}

      if ( (keycode == 0x1D) /* left Ctrl */
	   || (keycode == 0x9D) /* right Ctrl */
	   || (keycode == 0x38) /* left Alt */
	   || (keycode == 0xB8) /* right Alt */
	   || (keycode == 0x2A) /* left Shift */
	   || (keycode == 0x36) /* right Shift */)
	{
	  switch (keycode)
	    {
	    case 0x1D: /* Left Ctrl */
	    case 0x9D: /* Right Ctrl */
	      if (key_up)
		k->modifier_state &= ~GDK_CONTROL_MASK;
	      else
		k->modifier_state |= GDK_CONTROL_MASK;
	      break;
	    case 0x38: /* Left Alt */
	    case 0xB8: /* Right Alt */
	      if (key_up)
		k->modifier_state &= ~GDK_MOD1_MASK;
	      else
		k->modifier_state |= GDK_MOD1_MASK;
	      break;
	    case 0x2A: /* Left Shift */
	    case 0x36: /* Right Shift */
	      if (key_up)
		k->modifier_state &= ~GDK_SHIFT_MASK;
	      else
		k->modifier_state |= GDK_SHIFT_MASK;
	      break;
	    }
	  continue; /* Don't generate events for modifiers */
	}

      if (keycode == 0x3A /* Caps lock */)
	{
	  if (!key_up)
	    k->caps_lock = !k->caps_lock;
	  
	  ioctl (k->fd, KDSETLED, k->caps_lock ? LED_CAP : 0);
	  continue;
	}

      if (trans_table[keycode][0] >= GDK_F1 &&
	  trans_table[keycode][0] <= GDK_F35 &&
	  (k->modifier_state & GDK_MOD1_MASK))
	{
	  if (key_up) /* Only switch on release */
	    {
	      gint vtnum = trans_table[keycode][0] - GDK_F1 + 1;

	      /* Do the whole funky VT switch thing */
	      ioctl (k->consfd, VT_ACTIVATE, vtnum);
	      ioctl (k->consfd, VT_WAITACTIVE, k->vtnum);
	      gdk_fb_redraw_all ();
	    }

	  continue;
	}

      keyval = 0;
      mod = 0;
      if (k->modifier_state & GDK_CONTROL_MASK)
	mod = 2;
      else if (k->modifier_state & GDK_SHIFT_MASK)
	mod = 1;
      do {
	keyval = trans_table[keycode][mod--];
      } while (!keyval && (mod >= 0));

      if (k->caps_lock && (keyval >= 'a') && (keyval <= 'z'))
	keyval = toupper (keyval);

      /* handle some magic keys */
      if (k->modifier_state & (GDK_CONTROL_MASK|GDK_MOD1_MASK))
	{
	  if (key_up)
	    {
	      if (keyval == GDK_BackSpace)
		exit (1);

	      if (keyval == GDK_Return)
		gdk_fb_redraw_all ();
	    }

	  keyval = 0;
	}

      if (!keyval)
	continue;

      win = gdk_window_find_focus ();
      event = gdk_event_make (win,
			      key_up ? GDK_KEY_RELEASE : GDK_KEY_PRESS,
			      TRUE);
      if (event)
	{
	  /* Find focused window */
	  event->key.time = now;
	  event->key.state = k->modifier_state;
	  event->key.keyval = keyval;
	  event->key.length = isprint (event->key.keyval) ? 1 : 0;
	  dummy[0] = event->key.keyval;
	  dummy[1] = 0;
	  event->key.string = event->key.length ? g_strdup(dummy) : NULL;
	}
    }

  input_activity ();

  return TRUE;
}

static gboolean
handle_xlate_keyboard_input (GIOChannel *gioc,
			     GIOCondition cond,
			     gpointer data)
{
  guchar buf[128];
  int i, n;
  Keyboard *k = data;
  guint32 now;

  n = read (k->fd, buf, sizeof(buf));
  if (n <= 0)
    g_error ("Nothing from keyboard!");

  /* Now turn this into a keyboard event */
  now = gdk_fb_get_time ();

  for (i = 0; i < n; i++)
    {
      GdkEvent *event;
      GdkWindow *win;
      char dummy[2];
      guint keyval;

      keyval = buf[i];

      switch (keyval) {
      case '\n':
	keyval = GDK_Return;
	break;
      case '\t':
	keyval = GDK_Tab;
	break;
      case 127:
	keyval = GDK_BackSpace;
	break;
      case 27:
	keyval = GDK_Escape;
	break;
      }

      win = gdk_window_find_focus ();
      
      /* Send key down: */
      event = gdk_event_make (win, GDK_KEY_PRESS, TRUE);
      if (event)
	{
	  /* Find focused window */
	  event->key.time = now;
	  event->key.state = k->modifier_state;
	  event->key.keyval = keyval;
	  event->key.length = isprint (event->key.keyval) ? 1 : 0;
	  dummy[0] = event->key.keyval;
	  dummy[1] = 0;
	  event->key.string = event->key.length ? g_strdup(dummy) : NULL;
	}

      /* Send key up: */
      event = gdk_event_make (win, GDK_KEY_RELEASE, TRUE);
      if (event)
	{
	  /* Find focused window */
	  event->key.time = now;
	  event->key.state = k->modifier_state;
	  event->key.keyval = keyval;
	  event->key.length = isprint (event->key.keyval) ? 1 : 0;
	  dummy[0] = event->key.keyval;
	  dummy[1] = 0;
	  event->key.string = event->key.length ? g_strdup(dummy) : NULL;
	}
    }

  input_activity ();

  return TRUE;
}


static Keyboard *
tty_keyboard_open (void)
{
  Keyboard *retval = g_new0 (Keyboard, 1);
  GIOChannel *gioc;
  const char cursoroff_str[] = "\033[?1;0;0c";
  int n;
  struct vt_stat vs;
  char buf[32];
  struct termios ts;
  gboolean raw_keyboard;

  retval->modifier_state = 0;
  retval->caps_lock = 0;
  
  setsid();
  retval->consfd = open ("/dev/console", O_RDWR);
  ioctl (retval->consfd, VT_GETSTATE, &vs);
  retval->prev_vtnum = vs.v_active;
  g_snprintf (buf, sizeof(buf), "/dev/tty%d", retval->prev_vtnum);
  ioctl (retval->consfd, KDSKBMODE, K_XLATE);

  n = ioctl (retval->consfd, VT_OPENQRY, &retval->vtnum);
  if (n < 0 || retval->vtnum == -1)
    g_error("Cannot allocate VT");

  ioctl (retval->consfd, VT_ACTIVATE, retval->vtnum);
  ioctl (retval->consfd, VT_WAITACTIVE, retval->vtnum);

#if 0
  close (0);
  close (1);
  close (2);
#endif
  
  g_snprintf (buf, sizeof(buf), "/dev/tty%d", retval->vtnum);
  retval->fd = open (buf, O_RDWR|O_NONBLOCK);
  if (retval->fd < 0)
    return NULL;
  raw_keyboard = TRUE;
  if (ioctl (retval->fd, KDSKBMODE, K_MEDIUMRAW) < 0)
    {
      raw_keyboard = FALSE;
      g_warning ("K_MEDIUMRAW failed, using broken XLATE keyboard driver");
    }

  /* Disable normal text on the console */
  ioctl (retval->fd, KDSETMODE, KD_GRAPHICS);

  /* Set controlling tty */
  ioctl (0, TIOCNOTTY, 0);
  ioctl (retval->fd, TIOCSCTTY, 0);
  tcgetattr (retval->fd, &ts);
  ts.c_cc[VTIME] = 0;
  ts.c_cc[VMIN] = 1;
  ts.c_lflag &= ~(ICANON|ECHO|ISIG);
  ts.c_iflag = 0;
  tcsetattr (retval->fd, TCSAFLUSH, &ts);

  tcsetpgrp (retval->fd, getpgrp());

  write (retval->fd, cursoroff_str, strlen (cursoroff_str));

#if 0
  if (retval->fd != 0)
    dup2 (retval->fd, 0);
  if (retval->fd != 1)
    dup2 (retval->fd, 1);
  if (retval->fd != 2)
    dup2 (retval->fd, 2);
#endif

  gioc = g_io_channel_unix_new (retval->fd);
  retval->fd_tag = g_io_add_watch (gioc,
				   G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL,
				   (raw_keyboard) ? handle_mediumraw_keyboard_input : handle_xlate_keyboard_input,
				   retval);

  return retval;
}

void
gdk_beep (void)
{
  static int pitch = 600, duration = 100;
  gulong arg;

  if (!keyboard)
    return;

  /* Thank you XFree86 */
  arg = ((1193190 / pitch) & 0xffff) |
    (((unsigned long)duration) << 16);

  ioctl (keyboard->fd, KDMKTONE, arg);
}

void
keyboard_init (void)
{
  keyboard = tty_keyboard_open ();
}

void
keyboard_shutdown (void)
{
  int tmpfd;

  ioctl (keyboard->fd, KDSETMODE, KD_TEXT);
  ioctl (keyboard->fd, KDSKBMODE, K_XLATE);
  close (keyboard->fd);
  g_source_remove (keyboard->fd_tag);

  tmpfd = keyboard->consfd;
  ioctl (tmpfd, VT_ACTIVATE, keyboard->prev_vtnum);
  ioctl (tmpfd, VT_WAITACTIVE, keyboard->prev_vtnum);
  ioctl (tmpfd, VT_DISALLOCATE, keyboard->vtnum);
  close (tmpfd);

  g_free (keyboard);
  keyboard = NULL;
}


