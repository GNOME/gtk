/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <t1lib.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#include "gdk.h"

#include "gdkprivate-fb.h"
#include "gdkinternals.h"

/* 
 * Private function declarations
 */

#ifndef HAVE_XCONVERTCASE
static void	 gdkx_XConvertCase	(KeySym	       symbol,
					 KeySym	      *lower,
					 KeySym	      *upper);
#define XConvertCase gdkx_XConvertCase
#endif

/* Private variable declarations
 */
static int gdk_initialized = 0;			    /* 1 if the library is initialized,
						     * 0 otherwise.
						     */

#ifdef G_ENABLE_DEBUG
static const GDebugKey gdk_debug_keys[] = {
  {"misc",	    GDK_DEBUG_MISC},
  {"events",	    GDK_DEBUG_EVENTS},
};

static const int gdk_ndebug_keys = sizeof(gdk_debug_keys)/sizeof(GDebugKey);

#endif /* G_ENABLE_DEBUG */

GdkArgDesc _gdk_windowing_args[] = {
  { NULL }
};

static GdkFBDisplay *
gdk_fb_display_new(const char *filename)
{
  int fd, n;
  GdkFBDisplay *retval;
  guint16 red[256], green[256], blue[256];
  struct fb_cmap cmap;

  fd = open(filename, O_RDWR);
  if(fd < 0)
    return NULL;

  retval = g_new0(GdkFBDisplay, 1);
  retval->fd = fd;
  n = ioctl(fd, FBIOGET_FSCREENINFO, &retval->sinfo);
  n |= ioctl(fd, FBIOGET_VSCREENINFO, &retval->modeinfo);
  g_assert(!n);

  retval->fbmem = mmap(NULL, retval->sinfo.smem_len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  g_assert(retval->fbmem != MAP_FAILED);

  for(n = 0; n < 16; n++)
    red[n] = green[n] = blue[n] = n << 12;
  for(n = 16; n < 256; n++)
    red[n] = green[n] = blue[n] = n << 8;
  cmap.red = red; cmap.green = green; cmap.blue = blue; cmap.len = 256; cmap.start = 0;
  ioctl(fd, FBIOPUTCMAP, &cmap);

  return retval;
}

static void
gdk_fb_display_destroy(GdkFBDisplay *fbd)
{
  munmap(fbd->fbmem, fbd->sinfo.smem_len);
  g_free(fbd);
}

extern void keyboard_init(void);

gboolean
_gdk_windowing_init_check (int argc, char **argv)
{
  if(gdk_initialized)
    return TRUE;

  keyboard_init();
  gdk_display = gdk_fb_display_new("/dev/fb");

  if(!gdk_display)
    return FALSE;

  T1_InitLib(NO_LOGFILE|IGNORE_FONTDATABASE);
  T1_AASetBitsPerPixel(gdk_display->modeinfo.bits_per_pixel);

  gdk_initialized = TRUE;

  return TRUE;
}

/*
 *--------------------------------------------------------------
 * gdk_pointer_grab
 *
 *   Grabs the pointer to a specific window
 *
 * Arguments:
 *   "window" is the window which will receive the grab
 *   "owner_events" specifies whether events will be reported as is,
 *     or relative to "window"
 *   "event_mask" masks only interesting events
 *   "confine_to" limits the cursor movement to the specified window
 *   "cursor" changes the cursor for the duration of the grab
 *   "time" specifies the time
 *
 * Results:
 *
 * Side effects:
 *   requires a corresponding call to gdk_pointer_ungrab
 *
 *--------------------------------------------------------------
 */

gint
gdk_pointer_grab (GdkWindow *	  window,
		  gint		  owner_events,
		  GdkEventMask	  event_mask,
		  GdkWindow *	  confine_to,
		  GdkCursor *	  cursor,
		  guint32	  time)
{
  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  g_return_val_if_fail (confine_to == NULL || GDK_IS_WINDOW (confine_to), 0);

  if(_gdk_fb_pointer_grab_window)
    return 1;

  if(!owner_events)
    _gdk_fb_pointer_grab_window = gdk_window_ref(window);

  _gdk_fb_pointer_grab_confine = confine_to?gdk_window_ref(confine_to):NULL;
  _gdk_fb_pointer_grab_events = event_mask;
  _gdk_fb_pointer_grab_cursor = cursor?gdk_cursor_ref(cursor):NULL;
  
  return 0;
}

/*
 *--------------------------------------------------------------
 * gdk_pointer_ungrab
 *
 *   Releases any pointer grab
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void
gdk_pointer_ungrab (guint32 time)
{
  if(_gdk_fb_pointer_grab_window)
    gdk_window_unref(_gdk_fb_pointer_grab_window);
  _gdk_fb_pointer_grab_window = NULL;

  if(_gdk_fb_pointer_grab_confine)
    gdk_window_unref(_gdk_fb_pointer_grab_confine);
  _gdk_fb_pointer_grab_confine = NULL;

  if(_gdk_fb_pointer_grab_cursor)
    gdk_cursor_unref(_gdk_fb_pointer_grab_cursor);
  _gdk_fb_pointer_grab_cursor = NULL;
}

/*
 *--------------------------------------------------------------
 * gdk_pointer_is_grabbed
 *
 *   Tell wether there is an active x pointer grab in effect
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_pointer_is_grabbed (void)
{
  return _gdk_fb_pointer_grab_window != NULL;
}

/*
 *--------------------------------------------------------------
 * gdk_keyboard_grab
 *
 *   Grabs the keyboard to a specific window
 *
 * Arguments:
 *   "window" is the window which will receive the grab
 *   "owner_events" specifies whether events will be reported as is,
 *     or relative to "window"
 *   "time" specifies the time
 *
 * Results:
 *
 * Side effects:
 *   requires a corresponding call to gdk_keyboard_ungrab
 *
 *--------------------------------------------------------------
 */

gint
gdk_keyboard_grab (GdkWindow *	   window,
		   gint		   owner_events,
		   guint32	   time)
{
  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if(_gdk_fb_pointer_grab_window)
    gdk_keyboard_ungrab(time);

  if(!owner_events)
    _gdk_fb_keyboard_grab_window = gdk_window_ref(window);
  
  return 0;
}

/*
 *--------------------------------------------------------------
 * gdk_keyboard_ungrab
 *
 *   Releases any keyboard grab
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void
gdk_keyboard_ungrab (guint32 time)
{
  if(_gdk_fb_keyboard_grab_window)
    gdk_window_unref(_gdk_fb_keyboard_grab_window);
  _gdk_fb_keyboard_grab_window = NULL;
}

/*
 *--------------------------------------------------------------
 * gdk_screen_width
 *
 *   Return the width of the screen.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_screen_width (void)
{
  return gdk_display->modeinfo.xres;
}

/*
 *--------------------------------------------------------------
 * gdk_screen_height
 *
 *   Return the height of the screen.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_screen_height (void)
{
  return gdk_display->modeinfo.yres;
}

/*
 *--------------------------------------------------------------
 * gdk_screen_width_mm
 *
 *   Return the width of the screen in millimeters.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_screen_width_mm (void)
{
  return 0.5 + gdk_screen_width () * (25.4 / 72.);
}

/*
 *--------------------------------------------------------------
 * gdk_screen_height
 *
 *   Return the height of the screen in millimeters.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gint
gdk_screen_height_mm (void)
{
  return 0.5 + gdk_screen_height () * (25.4 / 72.);
}

/*
 *--------------------------------------------------------------
 * gdk_set_sm_client_id
 *
 *   Set the SM_CLIENT_ID property on the WM_CLIENT_LEADER window
 *   so that the window manager can save our state using the
 *   X11R6 ICCCM session management protocol. A NULL value should 
 *   be set following disconnection from the session manager to
 *   remove the SM_CLIENT_ID property.
 *
 * Arguments:
 * 
 *   "sm_client_id" specifies the client id assigned to us by the
 *   session manager or NULL to remove the property.
 *
 * Results:
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void
gdk_set_sm_client_id (const gchar* sm_client_id)
{
}

void
gdk_key_repeat_disable (void)
{
}

void
gdk_key_repeat_restore (void)
{
}


void
gdk_beep (void)
{
}

extern void keyboard_shutdown(void);

void
gdk_windowing_exit (void)
{
  gdk_fb_display_destroy(gdk_display); gdk_display = NULL;

  T1_CloseLib();

  keyboard_shutdown();
}

gchar*
gdk_keyval_name (guint	      keyval)
{
  return NULL;
}

guint
gdk_keyval_from_name (const gchar *keyval_name)
{
  return 0;
}

void     gdk_keyval_convert_case (guint        symbol,
                                  guint       *lower,
                                  guint       *upper)
{
  if(symbol >= 'a' && symbol <= 'z')
    symbol = toupper(symbol);

  if(upper)
    *upper = symbol;

  if(lower)
    *lower = symbol;
}

gchar *
gdk_get_display(void)
{
  return g_strdup("/dev/fb0");
}

/* utils */
GdkEvent *
gdk_event_make(GdkWindow *window, GdkEventType type, gboolean append_to_queue)
{
  static const guint type_masks[] = {
    GDK_SUBSTRUCTURE_MASK, /* GDK_DELETE		= 0, */
    GDK_STRUCTURE_MASK, /* GDK_DESTROY		= 1, */
    GDK_EXPOSURE_MASK, /* GDK_EXPOSE		= 2, */
    GDK_POINTER_MOTION_MASK|GDK_BUTTON_MOTION_MASK, /* GDK_MOTION_NOTIFY	= 3, */
    GDK_BUTTON_PRESS_MASK, /* GDK_BUTTON_PRESS	= 4, */
    GDK_BUTTON_PRESS_MASK, /* GDK_2BUTTON_PRESS	= 5, */
    GDK_BUTTON_PRESS_MASK, /* GDK_3BUTTON_PRESS	= 6, */
    GDK_BUTTON_RELEASE_MASK, /* GDK_BUTTON_RELEASE	= 7, */
    GDK_KEY_PRESS_MASK, /* GDK_KEY_PRESS	= 8, */
    GDK_KEY_RELEASE_MASK, /* GDK_KEY_RELEASE	= 9, */
    GDK_ENTER_NOTIFY_MASK, /* GDK_ENTER_NOTIFY	= 10, */
    GDK_LEAVE_NOTIFY_MASK, /* GDK_LEAVE_NOTIFY	= 11, */
    GDK_FOCUS_CHANGE_MASK, /* GDK_FOCUS_CHANGE	= 12, */
    GDK_STRUCTURE_MASK, /* GDK_CONFIGURE		= 13, */
    GDK_VISIBILITY_NOTIFY_MASK, /* GDK_MAP		= 14, */
    GDK_VISIBILITY_NOTIFY_MASK, /* GDK_UNMAP		= 15, */
    GDK_PROPERTY_CHANGE_MASK, /* GDK_PROPERTY_NOTIFY	= 16, */
    GDK_PROPERTY_CHANGE_MASK, /* GDK_SELECTION_CLEAR	= 17, */
    GDK_PROPERTY_CHANGE_MASK, /* GDK_SELECTION_REQUEST = 18, */
    GDK_PROPERTY_CHANGE_MASK, /* GDK_SELECTION_NOTIFY	= 19, */
    GDK_PROXIMITY_IN_MASK, /* GDK_PROXIMITY_IN	= 20, */
    GDK_PROXIMITY_OUT_MASK, /* GDK_PROXIMITY_OUT	= 21, */
    GDK_ALL_EVENTS_MASK, /* GDK_DRAG_ENTER        = 22, */
    GDK_ALL_EVENTS_MASK, /* GDK_DRAG_LEAVE        = 23, */
    GDK_ALL_EVENTS_MASK, /* GDK_DRAG_MOTION       = 24, */
    GDK_ALL_EVENTS_MASK, /* GDK_DRAG_STATUS       = 25, */
    GDK_ALL_EVENTS_MASK, /* GDK_DROP_START        = 26, */
    GDK_ALL_EVENTS_MASK, /* GDK_DROP_FINISHED     = 27, */
    GDK_ALL_EVENTS_MASK, /* GDK_CLIENT_EVENT	= 28, */
    GDK_VISIBILITY_NOTIFY_MASK, /* GDK_VISIBILITY_NOTIFY = 29, */
    GDK_EXPOSURE_MASK, /* GDK_NO_EXPOSE		= 30, */
    GDK_SCROLL_MASK /* GDK_SCROLL            = 31 */
  };

  if(GDK_WINDOW_FBDATA(window)->event_mask & type_masks[type])
    {
      GdkEvent *event = gdk_event_new();
      guint32 the_time = g_latest_time.tv_sec;

      event->any.type = type;
      event->any.window = gdk_window_ref(window);
      switch(type)
	{
	case GDK_MOTION_NOTIFY:
	  event->motion.time = the_time;
	  break;
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
	  event->button.time = the_time;
	  break;
	case GDK_KEY_PRESS:
	case GDK_KEY_RELEASE:
	  event->key.time = the_time;
	  break;
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY:
	  event->crossing.time = the_time;
	  break;

	case GDK_PROPERTY_NOTIFY:
	  event->property.time = the_time;
	  break;

	case GDK_SELECTION_CLEAR:
	case GDK_SELECTION_REQUEST:
	case GDK_SELECTION_NOTIFY:
	  event->selection.time = the_time;
	  break;
	case GDK_PROXIMITY_IN:
	case GDK_PROXIMITY_OUT:
	  event->proximity.time = the_time;
	  break;
	case GDK_DRAG_ENTER:
	case GDK_DRAG_LEAVE:
	case GDK_DRAG_MOTION:
	case GDK_DRAG_STATUS:
	case GDK_DROP_START:
	case GDK_DROP_FINISHED:
	  event->dnd.time = the_time;
	  break;

	case GDK_FOCUS_CHANGE:
	case GDK_CONFIGURE:
	case GDK_MAP:
	case GDK_UNMAP:
	case GDK_CLIENT_EVENT:
	case GDK_VISIBILITY_NOTIFY:
	case GDK_NO_EXPOSE:
	case GDK_SCROLL:
	case GDK_DELETE:
	case GDK_DESTROY:
	case GDK_EXPOSE:
	default:
	  break;
	}

      if(append_to_queue)
	gdk_event_queue_append(event);

      return event;
    }

  return NULL;
}
