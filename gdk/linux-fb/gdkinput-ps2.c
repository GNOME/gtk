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
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/vt.h>
#include <sys/kd.h>
#include <ctype.h>

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

typedef struct {
  gint fd, fd_tag;

  gint x, y;
  GdkWindow *prev_window;
  gboolean button1_pressed, button2_pressed, button3_pressed;
  gboolean click_grab;
} PS2Mouse;

typedef struct {
  gint fd, fd_tag, consfd;

  int vtnum, prev_vtnum;
  guchar states[256];
  gboolean is_ext : 1;
  gboolean caps_lock : 1;
} Keyboard;

static Keyboard * tty_keyboard_open(void);
static guint keyboard_get_state(Keyboard *k);

PS2Mouse *gdk_fb_ps2mouse = NULL;
static Keyboard *keyboard = NULL;
FILE *debug_out;

static guint multiclick_tag;
static GdkEvent *multiclick_event = NULL;

static gboolean
click_event_timeout(gpointer x)
{
  switch(multiclick_event->type)
    {
    case GDK_BUTTON_RELEASE:
      gdk_event_free(multiclick_event);
      break;
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
      gdk_event_queue_append(multiclick_event);
      break;
    default:
      break;
    }

  multiclick_event = NULL;
  multiclick_tag = 0;

  return FALSE;
}

static void
send_button_event(PS2Mouse *mouse, guint button, gboolean press_event, time_t the_time)
{
  GdkEvent *event;
  gint x, y;
  GdkWindow *window;
  int nbuttons = 0;

  if(_gdk_fb_pointer_grab_window)
    window = _gdk_fb_pointer_grab_window;
  else
    window = gdk_window_get_pointer(NULL, NULL, NULL, NULL);

  gdk_window_get_origin(window, &x, &y);
  x = mouse->x - x;
  y = mouse->y - y;

  if(!press_event
     && multiclick_event
     && multiclick_event->button.button == button
     && multiclick_event->button.window == window
     && ABS(multiclick_event->button.x - x) < 3
     && ABS(multiclick_event->button.y - y) < 3)
    {
      multiclick_event->button.time = the_time;

      /* Just change multiclick_event into a different event */
      switch(multiclick_event->button.type)
	{
	default:
	  g_assert_not_reached();

	case GDK_BUTTON_RELEASE:
	  multiclick_event->button.type = GDK_2BUTTON_PRESS;
	  return;

	case GDK_2BUTTON_PRESS:
	  multiclick_event->button.type = GDK_3BUTTON_PRESS;
	  return;

	case GDK_3BUTTON_PRESS:
	  gdk_event_queue_append(multiclick_event); multiclick_event = NULL;
	  g_source_remove(multiclick_tag); multiclick_tag = 0;
	}
    }

  event = gdk_event_make(window, press_event?GDK_BUTTON_PRESS:GDK_BUTTON_RELEASE, FALSE);

  if(!event)
    return;

  event->button.x = x;
  event->button.y = y;
  event->button.button = button;
  event->button.state = (mouse->button1_pressed?GDK_BUTTON1_MASK:0)
    | (mouse->button2_pressed?GDK_BUTTON2_MASK:0)
    | (mouse->button3_pressed?GDK_BUTTON3_MASK:0)
    | (1 << (button + 8)) /* badhack */
    | keyboard_get_state(keyboard);
  event->button.device = gdk_core_pointer;
  event->button.x_root = mouse->x;
  event->button.y_root = mouse->y;

  if(mouse->button1_pressed)
    nbuttons++;
  if(mouse->button2_pressed)
    nbuttons++;
  if(mouse->button3_pressed)
    nbuttons++;

  if(press_event && nbuttons == 1 && !_gdk_fb_pointer_grab_window)
    {
      gdk_pointer_grab(window, FALSE, gdk_window_get_events(window), NULL, NULL, GDK_CURRENT_TIME);
      mouse->click_grab = TRUE;
    }
  else if(!press_event && nbuttons == 0 && mouse->click_grab)
    {
      gdk_pointer_ungrab(GDK_CURRENT_TIME);
      mouse->click_grab = FALSE;
    }

#if 0
  g_message("Button #%d %s [%d, %d] in %p", button, press_event?"pressed":"released",
	    x, y, window);

  /* Debugging aid */
  if(window && window != gdk_parent_root)
    {
      GdkGC *tmp_gc;

      tmp_gc = gdk_gc_new(window);
      GDK_GC_FBDATA(tmp_gc)->values.foreground.pixel = 0;
      gdk_fb_draw_rectangle(GDK_DRAWABLE_IMPL(window), tmp_gc, TRUE, 0, 0,
			    GDK_DRAWABLE_IMPL_FBDATA(window)->width, GDK_DRAWABLE_IMPL_FBDATA(window)->height);
      gdk_gc_unref(tmp_gc);
    }
#endif

  if(!press_event && !multiclick_tag)
    {
      multiclick_tag = g_timeout_add(250, click_event_timeout, NULL);
      multiclick_event = gdk_event_copy(event);
    }

  gdk_event_queue_append(event);
}

static GdkPixmap *last_contents = NULL;
static GdkPoint last_location, last_contents_size;
static GdkCursor *last_cursor = NULL;
GdkFBDrawingContext *gdk_fb_cursor_dc = NULL;
static GdkFBDrawingContext cursor_dc_dat;
static GdkGC *cursor_gc;
static gint cursor_visibility_count = 1;

static GdkFBDrawingContext *
gdk_fb_cursor_dc_reset(void)
{
  if(gdk_fb_cursor_dc)
    gdk_fb_drawing_context_finalize(gdk_fb_cursor_dc);

  gdk_fb_cursor_dc = &cursor_dc_dat;
  gdk_fb_drawing_context_init(gdk_fb_cursor_dc, GDK_DRAWABLE_IMPL(gdk_parent_root), cursor_gc, TRUE, FALSE);

  return gdk_fb_cursor_dc;
}

void
gdk_fb_cursor_hide(void)
{
  GdkFBDrawingContext *mydc = gdk_fb_cursor_dc;

  cursor_visibility_count--;
  g_assert(cursor_visibility_count <= 0);
  if(cursor_visibility_count < 0)
    return;

  if(!mydc)
    mydc = gdk_fb_cursor_dc_reset();

  if(last_contents)
    {
      gdk_gc_set_clip_mask(cursor_gc, NULL);
      /* Restore old picture */
      gdk_fb_draw_drawable_3(GDK_DRAWABLE_IMPL(gdk_parent_root), cursor_gc, GDK_DRAWABLE_IMPL(last_contents),
			     mydc,
			     0, 0,
			     last_location.x,
			     last_location.y,
			     last_contents_size.x,
			     last_contents_size.y);
    }
}

void
gdk_fb_cursor_invalidate(void)
{
  if(last_contents)
    {
      gdk_pixmap_unref(last_contents);
      last_contents = NULL;
    }
}

void
gdk_fb_cursor_unhide()
{
  GdkFBDrawingContext *mydc = gdk_fb_cursor_dc;

  cursor_visibility_count++;
  g_assert(cursor_visibility_count <= 1);
  if(cursor_visibility_count < 1)
    return;

  if(!mydc)
    mydc = gdk_fb_cursor_dc_reset();

  if(last_cursor)
    {
      if(!last_contents
	 || GDK_DRAWABLE_IMPL_FBDATA(GDK_CURSOR_FB(last_cursor)->cursor)->width > GDK_DRAWABLE_IMPL_FBDATA(last_contents)->width
	 || GDK_DRAWABLE_IMPL_FBDATA(GDK_CURSOR_FB(last_cursor)->cursor)->height > GDK_DRAWABLE_IMPL_FBDATA(last_contents)->height)
	{
	  if(last_contents)
	    gdk_pixmap_unref(last_contents);

	  last_contents = gdk_pixmap_new(gdk_parent_root,
					 GDK_DRAWABLE_IMPL_FBDATA(GDK_CURSOR_FB(last_cursor)->cursor)->width,
					 GDK_DRAWABLE_IMPL_FBDATA(GDK_CURSOR_FB(last_cursor)->cursor)->height,
					 GDK_DRAWABLE_IMPL_FBDATA(gdk_parent_root)->depth);
	}

      gdk_gc_set_clip_mask(cursor_gc, NULL);
      gdk_fb_draw_drawable_2(GDK_DRAWABLE_IMPL(last_contents), cursor_gc, GDK_DRAWABLE_IMPL(gdk_parent_root), last_location.x,
			     last_location.y, 0, 0,
			     GDK_DRAWABLE_IMPL_FBDATA(GDK_CURSOR_FB(last_cursor)->cursor)->width,
			     GDK_DRAWABLE_IMPL_FBDATA(GDK_CURSOR_FB(last_cursor)->cursor)->height, TRUE, FALSE);
      last_contents_size.x = GDK_DRAWABLE_IMPL_FBDATA(GDK_CURSOR_FB(last_cursor)->cursor)->width;
      last_contents_size.y = GDK_DRAWABLE_IMPL_FBDATA(GDK_CURSOR_FB(last_cursor)->cursor)->height;
      gdk_gc_set_clip_mask(cursor_gc, GDK_CURSOR_FB(last_cursor)->mask);
      gdk_gc_set_clip_origin(cursor_gc, last_location.x + GDK_CURSOR_FB(last_cursor)->mask_off_x,
			     last_location.y + GDK_CURSOR_FB(last_cursor)->mask_off_y);

      gdk_fb_cursor_dc_reset();
      gdk_fb_draw_drawable_3(GDK_DRAWABLE_IMPL(gdk_parent_root), cursor_gc, GDK_DRAWABLE_IMPL(GDK_CURSOR_FB(last_cursor)->cursor),
			     mydc, 0, 0, last_location.x, last_location.y,
			     GDK_DRAWABLE_IMPL_FBDATA(GDK_CURSOR_FB(last_cursor)->cursor)->width,
			     GDK_DRAWABLE_IMPL_FBDATA(GDK_CURSOR_FB(last_cursor)->cursor)->height);
    }
  else
    gdk_fb_cursor_invalidate();
}

gboolean
gdk_fb_cursor_region_need_hide(GdkRegion *region)
{
  GdkRectangle testme;

  if(!last_cursor)
    return FALSE;

  testme.x = last_location.x;
  testme.y = last_location.y;
  testme.width = GDK_DRAWABLE_IMPL_FBDATA(GDK_CURSOR_FB(last_cursor)->cursor)->width;
  testme.height = GDK_DRAWABLE_IMPL_FBDATA(GDK_CURSOR_FB(last_cursor)->cursor)->height;

  return (gdk_region_rect_in(region, &testme)!=GDK_OVERLAP_RECTANGLE_OUT);
}

gboolean
gdk_fb_cursor_need_hide(GdkRectangle *rect)
{
  GdkRectangle testme;

  if(!last_cursor)
    return FALSE;

  testme.x = last_location.x;
  testme.y = last_location.y;
  testme.width = GDK_DRAWABLE_IMPL_FBDATA(GDK_CURSOR_FB(last_cursor)->cursor)->width;
  testme.height = GDK_DRAWABLE_IMPL_FBDATA(GDK_CURSOR_FB(last_cursor)->cursor)->height;

  return gdk_rectangle_intersect(rect, &testme, &testme);
}

void
gdk_fb_get_cursor_rect(GdkRectangle *rect)
{
  if(last_cursor)
    {
      rect->x = last_location.x;
      rect->y = last_location.y;
      rect->width = GDK_DRAWABLE_IMPL_FBDATA(GDK_CURSOR_FB(last_cursor)->cursor)->width;
      rect->height = GDK_DRAWABLE_IMPL_FBDATA(GDK_CURSOR_FB(last_cursor)->cursor)->height;
    }
  else
    {
      rect->x = rect->y = -1;
      rect->width = rect->height = 0;
    }
}

static void
move_pointer(PS2Mouse *mouse, GdkWindow *in_window)
{
  GdkCursor *the_cursor;

  if(!cursor_gc)
    {
      GdkColor white, black;
      cursor_gc = gdk_gc_new(gdk_parent_root);
      gdk_color_black(gdk_colormap_get_system(), &black);
      gdk_color_white(gdk_colormap_get_system(), &white);
      gdk_gc_set_foreground(cursor_gc, &black);
      gdk_gc_set_background(cursor_gc, &white);
    }

  gdk_fb_cursor_hide();

  if(_gdk_fb_pointer_grab_cursor)
    the_cursor = _gdk_fb_pointer_grab_cursor;
  else
    {
      while(!GDK_WINDOW_IMPL_FBDATA(in_window)->cursor && GDK_WINDOW_P(in_window)->parent)
	in_window = (GdkWindow *)GDK_WINDOW_P(in_window)->parent;
      the_cursor = GDK_WINDOW_IMPL_FBDATA(in_window)->cursor;
    }

  last_location.x = mouse->x - GDK_CURSOR_FB(the_cursor)->hot_x;
  last_location.y = mouse->y - GDK_CURSOR_FB(the_cursor)->hot_y;

  if(the_cursor)
    gdk_cursor_ref(the_cursor);
  if(last_cursor)
    gdk_cursor_unref(last_cursor);
  last_cursor = the_cursor;

  gdk_fb_cursor_unhide();
}

void
gdk_fb_cursor_reset(void)
{
  GdkWindow *win = gdk_window_get_pointer(NULL, NULL, NULL, NULL);

  move_pointer(gdk_fb_ps2mouse, win);
}

void gdk_fb_window_visibility_crossing(GdkWindow *window, gboolean is_show)
{
  gint winx, winy;
  GdkModifierType my_mask;

  gdk_input_ps2_get_mouseinfo(&winx, &winy, &my_mask);

  if(winx >= GDK_DRAWABLE_IMPL_FBDATA(window)->llim_x
     && winx < GDK_DRAWABLE_IMPL_FBDATA(window)->lim_x
     && winy >= GDK_DRAWABLE_IMPL_FBDATA(window)->llim_y
     && winy < GDK_DRAWABLE_IMPL_FBDATA(window)->lim_y)
    {
      GdkWindow *oldwin, *newwin, *curwin;
      GdkEvent *event;

      curwin = gdk_window_get_pointer(NULL, NULL, NULL, NULL);

      if(is_show)
	{
	  /* Window is about to be shown */
	  oldwin = curwin;
	  newwin = window;
	}
      else
	{
	  /* Window is about to be hidden */
	  oldwin = window;
	  newwin = curwin;
	}

      event = gdk_event_make(oldwin, GDK_LEAVE_NOTIFY, TRUE);
      if(event)
	{
	  guint x_int, y_int;
	  event->crossing.subwindow = gdk_window_ref(newwin);
	  gdk_window_get_root_origin(oldwin, &x_int, &y_int);
	  event->crossing.x = winx - x_int;
	  event->crossing.y = winy - y_int;
	  event->crossing.x_root = winx;
	  event->crossing.y_root = winy;
	  event->crossing.mode = GDK_CROSSING_NORMAL;
	  event->crossing.detail = GDK_NOTIFY_UNKNOWN;
	  event->crossing.focus = FALSE;
	  event->crossing.state = my_mask;
	}

      event = gdk_event_make(newwin, GDK_ENTER_NOTIFY, TRUE);
      if(event)
	{
	  guint x_int, y_int;
	  event->crossing.subwindow = gdk_window_ref(oldwin);
	  gdk_window_get_root_origin(newwin, &x_int, &y_int);
	  event->crossing.x = winx - x_int;
	  event->crossing.y = winy - y_int;
	  event->crossing.x_root = winx;
	  event->crossing.y_root = winy;
	  event->crossing.mode = GDK_CROSSING_NORMAL;
	  event->crossing.detail = GDK_NOTIFY_UNKNOWN;
	  event->crossing.focus = FALSE;
	  event->crossing.state = my_mask;
	}

      if(gdk_fb_ps2mouse->prev_window)
	gdk_window_unref(gdk_fb_ps2mouse->prev_window);
      gdk_fb_ps2mouse->prev_window = gdk_window_ref(newwin);
    }
}

static gboolean
handle_input(GIOChannel *gioc, GIOCondition cond, gpointer data)
{
  guchar buf[3];
  int n, left, dx=0, dy=0;
  PS2Mouse *mouse = data;
  gboolean new_button1, new_button2, new_button3;
  time_t the_time = g_latest_time.tv_sec;
  GdkWindow *mousewin;
  gboolean got_motion = FALSE;

  while(1) /* Go through as many mouse events as we can */
    {
      for(left = sizeof(buf); left > 0; )
	{
	  n = read(mouse->fd, buf+sizeof(buf)-left, left);

	  if(n <= 0)
	    {
	      if(left != sizeof(buf))
		continue; /* XXX FIXME - this will be slow compared to turning on blocking mode, etc. */

	      goto done_reading_mouse_events;
	    }

	  left -= n;
	}

      new_button1 = (buf[0] & 1) && 1;
      new_button3 = (buf[0] & 2) && 1;
      new_button2 = (buf[0] & 4) && 1;

      if(new_button1 != mouse->button1_pressed)
	{
	  mouse->button1_pressed = new_button1; 
	  send_button_event(mouse, 1, new_button1, the_time);
	}

      if(new_button2 != mouse->button2_pressed)
	{
	  mouse->button2_pressed = new_button2;
	  send_button_event(mouse, 2, new_button2, the_time);
	}

      if(new_button3 != mouse->button3_pressed)
	{
	  mouse->button3_pressed = new_button3; 
	  send_button_event(mouse, 3, new_button3, the_time);
	}

      if(buf[1] != 0)
	dx = ((buf[0] & 0x10) ? ((gint)buf[1])-256 : buf[1]);
      else
	dx = 0;
      if(buf[2] != 0)
	dy = -((buf[0] & 0x20) ? ((gint)buf[2])-256 : buf[2]);
      else
	dy = 0;

      mouse->x += dx;
      mouse->y += dy;
      if(dx || dy)
	got_motion = TRUE;
    }
 done_reading_mouse_events:

  if(_gdk_fb_pointer_grab_confine)
    mousewin = _gdk_fb_pointer_grab_confine;
  else
    mousewin = gdk_parent_root;

  if(mouse->x < 0)
    mouse->x = 0;
  else if(mouse->x > (GDK_DRAWABLE_IMPL_FBDATA(mousewin)->lim_x - 1))
    mouse->x = GDK_DRAWABLE_IMPL_FBDATA(mousewin)->lim_x - 1;
  if(mouse->y < 0)
    mouse->y = 0;
  else if(mouse->y > (GDK_DRAWABLE_IMPL_FBDATA(mousewin)->lim_y - 1))
    mouse->y = GDK_DRAWABLE_IMPL_FBDATA(mousewin)->lim_y - 1;

  if(got_motion) {
    GdkEvent *event;
    gint x, y;
    GdkWindow *win;
    guint state;

    win = gdk_window_get_pointer(NULL, NULL, NULL, NULL);
    move_pointer(mouse, win);
    if(_gdk_fb_pointer_grab_window)
      win = _gdk_fb_pointer_grab_window;

    gdk_window_get_origin(win, &x, &y);
    x = mouse->x - x;
    y = mouse->y - y;


    state = (mouse->button1_pressed?GDK_BUTTON1_MASK:0)
      | (mouse->button2_pressed?GDK_BUTTON2_MASK:0)
      | (mouse->button3_pressed?GDK_BUTTON3_MASK:0)
      | keyboard_get_state(keyboard);

    event = gdk_event_make (win, GDK_MOTION_NOTIFY, TRUE);
    if(event)
      {
	event->motion.x = x;
	event->motion.y = y;
	event->motion.state = state;
	event->motion.is_hint = FALSE;
	event->motion.device = gdk_core_pointer;
	event->motion.x_root = mouse->x;
	event->motion.y_root = mouse->y;
      }

    if(win != mouse->prev_window)
      {
	GdkEvent *evel;

	if(mouse->prev_window && (evel = gdk_event_make(mouse->prev_window, GDK_LEAVE_NOTIFY, TRUE)))
	  {
	    evel->crossing.subwindow = gdk_window_ref(win);
	    evel->crossing.x = x;
	    evel->crossing.y = y;
	    evel->crossing.x_root = mouse->x;
	    evel->crossing.y_root = mouse->y;
	    evel->crossing.mode = GDK_CROSSING_NORMAL;
	    evel->crossing.detail = GDK_NOTIFY_UNKNOWN;
	    evel->crossing.focus = FALSE;
	    evel->crossing.state = state;
	  }

	evel = gdk_event_make(win, GDK_ENTER_NOTIFY, TRUE);
	if(evel)
	  {
	    evel->crossing.subwindow = gdk_window_ref(mouse->prev_window?mouse->prev_window:gdk_parent_root);
	    evel->crossing.x = x;
	    evel->crossing.y = y;
	    evel->crossing.x_root = mouse->x;
	    evel->crossing.y_root = mouse->y;
	    evel->crossing.mode = GDK_CROSSING_NORMAL;
	    evel->crossing.detail = GDK_NOTIFY_UNKNOWN;
	    evel->crossing.focus = FALSE;
	    evel->crossing.state = state;
	  }

	if(mouse->prev_window)
	  gdk_window_unref(mouse->prev_window);
	mouse->prev_window = gdk_window_ref(win);
      }
  }

  return TRUE;
}

static PS2Mouse *
mouse_open(void)
{
  PS2Mouse *retval = g_new0(PS2Mouse, 1);
  guchar buf[7];
  int i = 0;
  GIOChannel *gioc;

  retval->fd = open("/dev/psaux", O_RDWR);
  if(retval->fd < 0)
    {
      g_free(retval);
      return NULL;
    }

  /* From xf86_Mouse.c */
  buf[i++] = 230; /* 1:1 scaling */
  buf[i++] = 244; /* enable mouse */
  buf[i++] = 243; /* Sample rate */
  buf[i++] = 200;
  buf[i++] = 232; /* device resolution */
  buf[i++] = 1;
  write(retval->fd, buf, i);
  read(retval->fd, buf, 3); /* Get rid of misc garbage whatever stuff from mouse */

  fcntl(retval->fd, F_SETFL, O_RDWR|O_NONBLOCK);

  gioc = g_io_channel_unix_new(retval->fd);
  retval->fd_tag = g_io_add_watch(gioc, G_IO_IN|G_IO_ERR|G_IO_HUP|G_IO_NVAL, handle_input, retval);

  retval->x = gdk_display->modeinfo.xres >> 1;
  retval->y = gdk_display->modeinfo.yres >> 1;

  return retval;
}

void
gdk_input_init (void)
{
  gdk_input_devices = g_list_append (NULL, gdk_core_pointer);

  gdk_input_ignore_core = FALSE;

  gdk_fb_ps2mouse = mouse_open();
}

void
gdk_input_ps2_get_mouseinfo(gint *x, gint *y, GdkModifierType *mask)
{
  *x = gdk_fb_ps2mouse->x;
  *y = gdk_fb_ps2mouse->y;
  *mask =
    (gdk_fb_ps2mouse->button1_pressed?GDK_BUTTON1_MASK:0)
    | (gdk_fb_ps2mouse->button2_pressed?GDK_BUTTON2_MASK:0)
    | (gdk_fb_ps2mouse->button3_pressed?GDK_BUTTON3_MASK:0)
    | keyboard_get_state(keyboard);
}

/* Returns the modifier mask for the keyboard */
static guint
keyboard_get_state(Keyboard *k)
{
  guint retval = 0;
  struct {
    guchar from;
    guint to;
  } statetrans[] = {
    {0x1D, GDK_CONTROL_MASK},
    {0x9D, GDK_CONTROL_MASK},
    {0x38, GDK_MOD1_MASK},
    {0xB8, GDK_MOD1_MASK},
    {0x2A, GDK_SHIFT_MASK},
    {0x36, GDK_SHIFT_MASK}
  };
  int i;

  for(i = 0; i < sizeof(statetrans)/sizeof(statetrans[0]); i++)
    if(k->states[statetrans[i].from])
      retval |= statetrans[i].to;

  return retval;
}

static GdkWindow *
gdk_window_find_focus(void)
{
  if(_gdk_fb_keyboard_grab_window)
    return _gdk_fb_keyboard_grab_window;
  else if(GDK_WINDOW_P(gdk_parent_root)->children)
    {
      GList *item;
      for(item = GDK_WINDOW_P(gdk_parent_root)->children; item; item = item->next)
	{
	  GdkWindowPrivate *priv = item->data;

	  if(priv->mapped)
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
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},

	/* 0x50 */
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
  {0, 0, 0},
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
  {0, 0, 0},
  {GDK_Left, 0, 0},
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
static gboolean
handle_keyboard_input(GIOChannel *gioc, GIOCondition cond, gpointer data)
{
  guchar buf[128];
  int i, n;
  Keyboard *k = data;
  time_t now;

  n = read(k->fd, buf, sizeof(buf));
  if(n <= 0)
    g_error("Nothing from keyboard!");

  /* Now turn this into a keyboard event */
  now = g_latest_time.tv_sec;

  for(i = 0; i < n; i++)
    {
      guchar base_char;
      GdkEvent *event;
      GdkWindow *win;
      char dummy[2];
      int mod;
      guint keyval, state;

      if(buf[i] == 0xE0 || k->is_ext) /* extended char */
	{
	  int l;

	  l = k->is_ext?0:1;
	  k->is_ext = TRUE;

	  if((i+l) >= n)
	    continue;

	  if(buf[i+l] == 0x2A
	     || buf[i+l] == 0xAA)
	    {
	      i++;
	      continue;
	    }

	  base_char = 0x80 + (buf[i+l] & 0x7F);
	  k->is_ext = FALSE;
	  i += l;
	}
      else
	base_char = buf[i] & 0x7F;

      if(base_char > sizeof(trans_table)/sizeof(trans_table[0]))
	continue;

      {
	gboolean new_state = (buf[i] & 0x80)?FALSE:TRUE;

	k->states[base_char] = new_state;
      }

      if((base_char == 0x1D) /* left Ctrl */
	 || (base_char == 0x9D) /* right Ctrl */
	 || (base_char == 0x38) /* left Alt */
	 || (base_char == 0xB8) /* right Alt */
	 || (base_char == 0x2A) /* left Shift */
	 || (base_char == 0x36) /* right Shift */)
	{
	  continue; /* Don't generate events for modifiers */
	}

      if(base_char == 0x3A /* Caps lock */)
	{
	  if(k->states[base_char])
	    k->caps_lock = !k->caps_lock;
	  ioctl(k->fd, KDSETLED, k->caps_lock?LED_CAP:0);

	  continue;
	}

      if(trans_table[base_char][0] >= GDK_F1
	 && trans_table[base_char][0] <= GDK_F35
	 && (keyboard_get_state(k) & GDK_MOD1_MASK))
	{
	  if(!k->states[base_char]) /* Only switch on release */
	    {
	      gint vtnum = trans_table[base_char][0] - GDK_F1 + 1;

	      fprintf(debug_out, "Switching VTs\n");

	      /* Do the whole funky VT switch thing */
	      ioctl(k->consfd, VT_ACTIVATE, vtnum);
	      ioctl(k->consfd, VT_WAITACTIVE, k->vtnum);
	      gdk_fb_redraw_all();
	    }

	  continue;
	}

      keyval = 0;
      state = keyboard_get_state(k);
      mod = 0;
      if(state & GDK_CONTROL_MASK)
	mod = 2;
      else if(state & GDK_SHIFT_MASK)
	mod = 1;
      do {
	keyval = trans_table[base_char][mod--];
      } while(!keyval && (mod >= 0));

      if(k->caps_lock && (keyval >= 'a')
	 && (keyval <= 'z'))
	keyval = toupper(keyval);

      /* handle some magic keys */
      if(state & (GDK_CONTROL_MASK|GDK_MOD1_MASK))
	{
	  if(!k->states[base_char])
	    {
	      if(keyval == GDK_BackSpace)
		exit(1);

	      if(keyval == GDK_Return)
		gdk_fb_redraw_all();

	    }

	  keyval = 0;
	}

      if(!keyval)
	continue;

      win = gdk_window_find_focus();
      event = gdk_event_make(win, k->states[base_char]?GDK_KEY_PRESS:GDK_KEY_RELEASE, TRUE);
      if(event)
	{
	  /* Find focused window */
	  event->key.time = now;
	  event->key.state = state;
	  event->key.keyval = keyval;
	  event->key.length = isprint(event->key.keyval)?1:0;
	  dummy[0] = event->key.keyval;
	  dummy[1] = 0;
	  event->key.string = event->key.length?g_strdup(dummy):NULL;
	}
    }

  return TRUE;
}

static Keyboard *
tty_keyboard_open(void)
{
  Keyboard *retval = g_new0(Keyboard, 1);
  GIOChannel *gioc;
  const char cursoroff_str[] = "\033[?1;0;0c";
  int n;
  struct vt_stat vs;
  char buf[32];
  struct termios ts;

  setsid();
  retval->consfd = open("/dev/console", O_RDWR);
  ioctl(retval->consfd, VT_GETSTATE, &vs);
  retval->prev_vtnum = vs.v_active;
  g_snprintf(buf, sizeof(buf), "/dev/tty%d", retval->prev_vtnum);
  ioctl(retval->consfd, KDSKBMODE, K_XLATE);

  n = ioctl(retval->consfd, VT_OPENQRY, &retval->vtnum);
  if(n < 0 || retval->vtnum == -1)
    g_error("Cannot allocate VT");

  ioctl(retval->consfd, VT_ACTIVATE, retval->vtnum);
  ioctl(retval->consfd, VT_WAITACTIVE, retval->vtnum);

  debug_out = fdopen(dup(2), "w");

#if 0
  close(0);
  close(1);
  close(2);
#endif
  g_snprintf(buf, sizeof(buf), "/dev/tty%d", retval->vtnum);
  retval->fd = open(buf, O_RDWR|O_NONBLOCK);
  if(retval->fd < 0)
    return NULL;
  if(ioctl(retval->fd, KDSKBMODE, K_RAW) < 0)
    g_warning("K_RAW failed");

  ioctl(0, TIOCNOTTY, 0);
  ioctl(retval->fd, TIOCSCTTY, 0);
  tcgetattr(retval->fd, &ts);
  ts.c_cc[VTIME] = 0;
  ts.c_cc[VMIN] = 1;
  ts.c_lflag &= ~(ICANON|ECHO|ISIG);
  ts.c_iflag = 0;
  tcsetattr(retval->fd, TCSAFLUSH, &ts);

  tcsetpgrp(retval->fd, getpgrp());

  write(retval->fd, cursoroff_str, strlen(cursoroff_str));

#if 0
  if(retval->fd != 0)
    dup2(retval->fd, 0);
  if(retval->fd != 1)
    dup2(retval->fd, 1);
  if(retval->fd != 2)
    dup2(retval->fd, 2);
#endif

  gioc = g_io_channel_unix_new(retval->fd);
  retval->fd_tag = g_io_add_watch(gioc, G_IO_IN|G_IO_ERR|G_IO_HUP|G_IO_NVAL, handle_keyboard_input, retval);

  return retval;
}

void
keyboard_init(void)
{
  keyboard = tty_keyboard_open();
}

void
keyboard_shutdown(void)
{
  int tmpfd;

  ioctl(keyboard->fd, KDSKBMODE, K_XLATE);
  close(keyboard->fd);
  g_source_remove(keyboard->fd_tag);

  tmpfd = keyboard->consfd;
  ioctl(tmpfd, VT_ACTIVATE, keyboard->prev_vtnum);
  ioctl(tmpfd, VT_WAITACTIVE, keyboard->prev_vtnum);
  ioctl(tmpfd, VT_DISALLOCATE, keyboard->vtnum);
  close(tmpfd);

  g_free(keyboard);
  keyboard = NULL;
}
