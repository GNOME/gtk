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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gdk.h"
#include "gdkprivate-x11.h"
#include "gdkinternals.h"
#include "gdkx.h"

#include "gdkkeysyms.h"

#include "xsettings-client.h"

#if HAVE_CONFIG_H
#  include <config.h>
#  if STDC_HEADERS
#    include <string.h>
#  endif
#endif

#include "gdkinputprivate.h"

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif

#include <X11/Xatom.h>

typedef struct _GdkIOClosure GdkIOClosure;
typedef struct _GdkEventPrivate GdkEventPrivate;

#define DOUBLE_CLICK_TIME      250
#define TRIPLE_CLICK_TIME      500
#define DOUBLE_CLICK_DIST      5
#define TRIPLE_CLICK_DIST      5

typedef enum
{
  /* Following flag is set for events on the event queue during
   * translation and cleared afterwards.
   */
  GDK_EVENT_PENDING = 1 << 0
} GdkEventFlags;

struct _GdkIOClosure
{
  GdkInputFunction function;
  GdkInputCondition condition;
  GdkDestroyNotify notify;
  gpointer data;
};

struct _GdkEventPrivate
{
  GdkEvent event;
  guint    flags;
};

/* 
 * Private function declarations
 */

static gint	 gdk_event_apply_filters (XEvent   *xevent,
					  GdkEvent *event,
					  GList    *filters);
static gint	 gdk_event_translate	 (GdkEvent *event, 
					  XEvent   *xevent,
					  gboolean  return_exposes);
#if 0
static Bool	 gdk_event_get_type	(Display   *display, 
					 XEvent	   *xevent, 
					 XPointer   arg);
#endif

static gboolean gdk_event_prepare  (GSource     *source,
				    gint        *timeout);
static gboolean gdk_event_check    (GSource     *source);
static gboolean gdk_event_dispatch (GSource     *source,
				    GSourceFunc  callback,
				    gpointer     user_data);

GdkFilterReturn gdk_wm_protocols_filter (GdkXEvent *xev,
					 GdkEvent  *event,
					 gpointer   data);

static void gdk_xsettings_watch_cb  (Window            window,
				     Bool              is_start,
				     long              mask,
				     void             *cb_data);
static void gdk_xsettings_notify_cb (const char       *name,
				     XSettingsAction   action,
				     XSettingsSetting *setting,
				     void             *data);

/* Private variable declarations
 */

static int connection_number = 0;	    /* The file descriptor number of our
					     *	connection to the X server. This
					     *	is used so that we may determine
					     *	when events are pending by using
					     *	the "select" system call.
					     */
static GList *client_filters;	            /* Filters for client messages */

static GSourceFuncs event_funcs = {
  gdk_event_prepare,
  gdk_event_check,
  gdk_event_dispatch,
  NULL
};

static GPollFD event_poll_fd;

static Window wmspec_check_window = None;

static XSettingsClient *xsettings_client;

/*********************************************
 * Functions for maintaining the event queue *
 *********************************************/

void 
gdk_events_init (void)
{
  GSource *source;
  
  connection_number = ConnectionNumber (gdk_display);
  GDK_NOTE (MISC,
	    g_message ("connection number: %d", connection_number));


  source = g_source_new (&event_funcs, sizeof (GSource));
  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  
  event_poll_fd.fd = connection_number;
  event_poll_fd.events = G_IO_IN;
  
  g_source_add_poll (source, &event_poll_fd);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  gdk_add_client_message_filter (gdk_wm_protocols, 
				 gdk_wm_protocols_filter, NULL);

  xsettings_client = xsettings_client_new (gdk_display, DefaultScreen (gdk_display),
					   gdk_xsettings_notify_cb,
					   gdk_xsettings_watch_cb,
					   NULL);
}

/*
 *--------------------------------------------------------------
 * gdk_events_pending
 *
 *   Returns if events are pending on the queue.
 *
 * Arguments:
 *
 * Results:
 *   Returns TRUE if events are pending
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

gboolean
gdk_events_pending (void)
{
  return (gdk_event_queue_find_first() || XPending (gdk_display));
}

/*
 *--------------------------------------------------------------
 * gdk_event_get_graphics_expose
 *
 *   Waits for a GraphicsExpose or NoExpose event
 *
 * Arguments:
 *
 * Results: 
 *   For GraphicsExpose events, returns a pointer to the event
 *   converted into a GdkEvent Otherwise, returns NULL.
 *
 * Side effects:
 *
 *-------------------------------------------------------------- */

static Bool
graphics_expose_predicate (Display  *display,
			   XEvent   *xevent,
			   XPointer  arg)
{
  if (xevent->xany.window == GDK_DRAWABLE_XID (arg) &&
      (xevent->xany.type == GraphicsExpose ||
       xevent->xany.type == NoExpose))
    return True;
  else
    return False;
}

GdkEvent*
gdk_event_get_graphics_expose (GdkWindow *window)
{
  XEvent xevent;
  GdkEvent *event;
  
  g_return_val_if_fail (window != NULL, NULL);
  
  XIfEvent (gdk_display, &xevent, graphics_expose_predicate, (XPointer) window);
  
  if (xevent.xany.type == GraphicsExpose)
    {
      event = gdk_event_new ();
      
      if (gdk_event_translate (event, &xevent, TRUE))
	return event;
      else
	gdk_event_free (event);
    }
  
  return NULL;	
}

static gint
gdk_event_apply_filters (XEvent *xevent,
			 GdkEvent *event,
			 GList *filters)
{
  GList *tmp_list;
  GdkFilterReturn result;
  
  tmp_list = filters;
  
  while (tmp_list)
    {
      GdkEventFilter *filter = (GdkEventFilter*) tmp_list->data;
      
      tmp_list = tmp_list->next;
      result = filter->function (xevent, event, filter->data);
      if (result !=  GDK_FILTER_CONTINUE)
	return result;
    }
  
  return GDK_FILTER_CONTINUE;
}

void 
gdk_add_client_message_filter (GdkAtom       message_type,
			       GdkFilterFunc func,
			       gpointer      data)
{
  GdkClientFilter *filter = g_new (GdkClientFilter, 1);

  filter->type = message_type;
  filter->function = func;
  filter->data = data;
  
  client_filters = g_list_prepend (client_filters, filter);
}

static GdkAtom wm_state_atom = 0;
static GdkAtom wm_desktop_atom = 0;

static void
gdk_check_wm_state_changed (GdkWindow *window)
{  
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  GdkAtom *atoms = NULL;
  gulong i;
  GdkAtom sticky_atom;
  GdkAtom maxvert_atom;
  GdkAtom maxhorz_atom;
  gboolean found_sticky, found_maxvert, found_maxhorz;
  GdkWindowState old_state;
  
  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  if (wm_state_atom == 0)
    wm_state_atom = gdk_atom_intern ("_NET_WM_STATE", FALSE);

  if (wm_desktop_atom == 0)
    wm_desktop_atom = gdk_atom_intern ("_NET_WM_DESKTOP", FALSE);
  
  XGetWindowProperty (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window),
		      wm_state_atom, 0, G_MAXLONG,
		      False, XA_ATOM, &type, &format, &nitems,
		      &bytes_after, (guchar **)&atoms);

  if (type != None)
    {

      sticky_atom = gdk_atom_intern ("_NET_WM_STATE_STICKY", FALSE);
      maxvert_atom = gdk_atom_intern ("_NET_WM_STATE_MAXIMIZED_VERT", FALSE);
      maxhorz_atom = gdk_atom_intern ("_NET_WM_STATE_MAXIMIZED_HORZ", FALSE);    

      found_sticky = FALSE;
      found_maxvert = FALSE;
      found_maxhorz = FALSE;
  
      i = 0;
      while (i < nitems)
        {
          if (atoms[i] == sticky_atom)
            found_sticky = TRUE;
          else if (atoms[i] == maxvert_atom)
            found_maxvert = TRUE;
          else if (atoms[i] == maxhorz_atom)
            found_maxhorz = TRUE;

          ++i;
        }

      XFree (atoms);
    }
  else
    {
      found_sticky = FALSE;
      found_maxvert = FALSE;
      found_maxhorz = FALSE;
    }

  /* For found_sticky to remain TRUE, we have to also be on desktop
   * 0xFFFFFFFF
   */

  if (found_sticky)
    {
      gulong *desktop;
      
      XGetWindowProperty (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window),
                          wm_desktop_atom, 0, G_MAXLONG,
                          False, XA_CARDINAL, &type, &format, &nitems,
                          &bytes_after, (guchar **)&desktop);

      if (type != None)
        {
          if (*desktop != 0xFFFFFFFF)
            found_sticky = FALSE;
          XFree (desktop);
        }
    }
          
  old_state = gdk_window_get_state (window);

  if (old_state & GDK_WINDOW_STATE_STICKY)
    {
      if (!found_sticky)
        gdk_synthesize_window_state (window,
                                     GDK_WINDOW_STATE_STICKY,
                                     0);
    }
  else
    {
      if (found_sticky)
        gdk_synthesize_window_state (window,
                                     0,
                                     GDK_WINDOW_STATE_STICKY);
    }

  /* Our "maximized" means both vertical and horizontal; if only one,
   * we don't expose that via GDK
   */
  if (old_state & GDK_WINDOW_STATE_MAXIMIZED)
    {
      if (!(found_maxvert && found_maxhorz))
        gdk_synthesize_window_state (window,
                                     GDK_WINDOW_STATE_MAXIMIZED,
                                     0);
    }
  else
    {
      if (found_maxvert && found_maxhorz)
        gdk_synthesize_window_state (window,
                                     0,
                                     GDK_WINDOW_STATE_MAXIMIZED);
    }
}

#define HAS_FOCUS(window_impl)                           \
  ((window_impl)->has_focus || (window_impl)->has_pointer_focus)

static void
generate_focus_event (GdkWindow *window,
		      gboolean   in)
{
  GdkEvent event;
  
  event.type = GDK_FOCUS_CHANGE;
  event.focus_change.window = window;
  event.focus_change.send_event = FALSE;
  event.focus_change.in = in;
  
  gdk_event_put (&event);
}

static gint
gdk_event_translate (GdkEvent *event,
		     XEvent   *xevent,
		     gboolean  return_exposes)
{
  
  GdkWindow *window;
  GdkWindowObject *window_private;
  GdkWindowImplX11 *window_impl = NULL;
  static XComposeStatus compose;
  KeySym keysym;
  int charcount;
  char buf[16];
  gint return_val;
  gint xoffset, yoffset;
  
  return_val = FALSE;
  
  /* Find the GdkWindow that this event occurred in.
   * 
   * We handle events with window=None
   *  specially - they are generated by XFree86's XInput under
   *  some circumstances.
   */
  
  if (xevent->xany.window == None)
    {
      return_val = _gdk_input_window_none_event (event, xevent);
      
      if (return_val >= 0)	/* was handled */
	return return_val;
      else
	return_val = FALSE;
    }
  
  window = gdk_window_lookup (xevent->xany.window);
  window_private = (GdkWindowObject *) window;

  if (_gdk_moveresize_window &&
      (xevent->xany.type == MotionNotify ||
       xevent->xany.type == ButtonRelease))
    {
      _gdk_moveresize_handle_event (xevent);
      gdk_window_unref (window);
      return FALSE;
    }
    
  if (wmspec_check_window != None &&
      xevent->xany.window == wmspec_check_window)
    {
      if (xevent->type == DestroyNotify)
        wmspec_check_window = None;
      
      /* Eat events on this window unless someone had wrapped
       * it as a foreign window
       */
      if (window == NULL)
        return FALSE;
    }
  
  /* FIXME: window might be a GdkPixmap!!! */
  if (window != NULL)
    {
      window_impl = GDK_WINDOW_IMPL_X11 (window_private->impl);
	  
      if (xevent->xany.window != GDK_WINDOW_XID (window))
	{
	  g_assert (xevent->xany.window == window_impl->focus_window);

	  switch (xevent->type)
	    {
	    case KeyPress:
	    case KeyRelease:
	      xevent->xany.window = GDK_WINDOW_XID (window);
	      break;
	    default:
	      return False;
	    }
	}

      gdk_window_ref (window);
    }

  event->any.window = window;
  event->any.send_event = xevent->xany.send_event ? TRUE : FALSE;
  
  if (window_private && GDK_WINDOW_DESTROYED (window))
    {
      if (xevent->type != DestroyNotify)
        return FALSE;
    }
  else
    {
      /* Check for filters for this window
       */
      GdkFilterReturn result;
      result = gdk_event_apply_filters (xevent, event,
					window_private
					?window_private->filters
					:gdk_default_filters);
      
      if (result != GDK_FILTER_CONTINUE)
	{
	  return_val = (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
	  goto done;
	}
    }

  /* We do a "manual" conversion of the XEvent to a
   *  GdkEvent. The structures are mostly the same so
   *  the conversion is fairly straightforward. We also
   *  optionally print debugging info regarding events
   *  received.
   */

  return_val = TRUE;

  if (window)
    {
      _gdk_windowing_window_get_offsets (window, &xoffset, &yoffset);
    }
  else
    {
      xoffset = 0;
      yoffset = 0;
    }

  switch (xevent->type)
    {
    case KeyPress:
      /* Lookup the string corresponding to the given keysym.
       */

      charcount = XLookupString (&xevent->xkey, buf, 16,
				 &keysym, &compose);
      event->key.keyval = keysym;
      event->key.hardware_keycode = xevent->xkey.keycode;
      
      if (charcount > 0 && buf[charcount-1] == '\0')
	charcount --;
      else
	buf[charcount] = '\0';
      
#ifdef G_ENABLE_DEBUG
      if (gdk_debug_flags & GDK_DEBUG_EVENTS)
	{
	  g_message ("key press:\twindow: %ld  key: %12s  %d",
		     xevent->xkey.window,
		     event->key.keyval ? XKeysymToString (event->key.keyval) : "(none)",
		     event->key.keyval);
	  if (charcount > 0)
	    g_message ("\t\tlength: %4d string: \"%s\"",
		       charcount, buf);
	}
#endif /* G_ENABLE_DEBUG */

      /* bits 13 and 14 in the "state" field are the keyboard group */
#define KEYBOARD_GROUP_SHIFT 13
#define KEYBOARD_GROUP_MASK ((1 << 13) | (1 << 14))
      
      event->key.type = GDK_KEY_PRESS;
      event->key.window = window;
      event->key.time = xevent->xkey.time;
      event->key.state = (GdkModifierType) xevent->xkey.state;
      event->key.string = g_strdup (buf);
      event->key.length = charcount;

      event->key.group = (xevent->xkey.state & KEYBOARD_GROUP_MASK) >> KEYBOARD_GROUP_SHIFT;
      
      break;
      
    case KeyRelease:
      /* Lookup the string corresponding to the given keysym.
       */

      /* Emulate detectable auto-repeat by checking to see
       * if the next event is a key press with the same
       * keycode and timestamp, and if so, ignoring the event.
       */

      if (!_gdk_have_xkb_autorepeat && XPending (gdk_display))
	{
	  XEvent next_event;

	  XPeekEvent (gdk_display, &next_event);

	  if (next_event.type == KeyPress &&
	      next_event.xkey.keycode == xevent->xkey.keycode &&
	      next_event.xkey.time == xevent->xkey.time)
	    break;
	}
      
      keysym = GDK_VoidSymbol;
      charcount = XLookupString (&xevent->xkey, buf, 16,
				 &keysym, &compose);
      event->key.keyval = keysym;      
      
      GDK_NOTE (EVENTS, 
		g_message ("key release:\t\twindow: %ld	 key: %12s  %d",
			   xevent->xkey.window,
			   XKeysymToString (event->key.keyval),
			   event->key.keyval));
      
      event->key.type = GDK_KEY_RELEASE;
      event->key.window = window;
      event->key.time = xevent->xkey.time;
      event->key.state = (GdkModifierType) xevent->xkey.state;
      event->key.length = 0;
      event->key.string = NULL;
      
      break;
      
    case ButtonPress:
      GDK_NOTE (EVENTS, 
		g_message ("button press:\t\twindow: %ld  x,y: %d %d  button: %d",
			   xevent->xbutton.window,
			   xevent->xbutton.x, xevent->xbutton.y,
			   xevent->xbutton.button));
      
      if (window_private &&
	  (window_private->extension_events != 0) &&
	  gdk_input_ignore_core)
	{
	  return_val = FALSE;
	  break;
	}
      
      /* If we get a ButtonPress event where the button is 4 or 5,
	 it's a Scroll event */
      if (xevent->xbutton.button == 4 || xevent->xbutton.button == 5)
	{
	  event->scroll.type = GDK_SCROLL;
	  event->scroll.direction = (xevent->xbutton.button == 4) ? 
	    GDK_SCROLL_UP : GDK_SCROLL_DOWN;
	  event->scroll.window = window;
	  event->scroll.time = xevent->xbutton.x;
	  event->scroll.x = xevent->xbutton.x + xoffset;
	  event->scroll.y = xevent->xbutton.y + yoffset;
	  event->scroll.x_root = (gfloat)xevent->xbutton.x_root;
	  event->scroll.y_root = (gfloat)xevent->xbutton.y_root;
	  event->scroll.state = (GdkModifierType) xevent->xbutton.state;
	  event->scroll.device = gdk_core_pointer;
	}
      else
	{
	  event->button.type = GDK_BUTTON_PRESS;
	  event->button.window = window;
	  event->button.time = xevent->xbutton.time;
	  event->button.x = xevent->xbutton.x + xoffset;
	  event->button.y = xevent->xbutton.y + yoffset;
	  event->button.x_root = (gfloat)xevent->xbutton.x_root;
	  event->button.y_root = (gfloat)xevent->xbutton.y_root;
	  event->button.axes = NULL;
	  event->button.state = (GdkModifierType) xevent->xbutton.state;
	  event->button.button = xevent->xbutton.button;
	  event->button.device = gdk_core_pointer;
	  
	  gdk_event_button_generate (event);
	}

      break;
      
    case ButtonRelease:
      GDK_NOTE (EVENTS, 
		g_message ("button release:\twindow: %ld  x,y: %d %d  button: %d",
			   xevent->xbutton.window,
			   xevent->xbutton.x, xevent->xbutton.y,
			   xevent->xbutton.button));
      
      if (window_private &&
	  (window_private->extension_events != 0) &&
	  gdk_input_ignore_core)
	{
	  return_val = FALSE;
	  break;
	}
      
      /* We treat button presses as scroll wheel events, so ignore the release */
      if (xevent->xbutton.button == 4 || xevent->xbutton.button == 5)
	{
	  return_val = FALSE;
	  break;
	}

      event->button.type = GDK_BUTTON_RELEASE;
      event->button.window = window;
      event->button.time = xevent->xbutton.time;
      event->button.x = xevent->xbutton.x + xoffset;
      event->button.y = xevent->xbutton.y + yoffset;
      event->button.x_root = (gfloat)xevent->xbutton.x_root;
      event->button.y_root = (gfloat)xevent->xbutton.y_root;
      event->button.axes = NULL;
      event->button.state = (GdkModifierType) xevent->xbutton.state;
      event->button.button = xevent->xbutton.button;
      event->button.device = gdk_core_pointer;
      
      break;
      
    case MotionNotify:
      GDK_NOTE (EVENTS,
		g_message ("motion notify:\t\twindow: %ld  x,y: %d %d  hint: %s", 
			   xevent->xmotion.window,
			   xevent->xmotion.x, xevent->xmotion.y,
			   (xevent->xmotion.is_hint) ? "true" : "false"));
      
      if (window_private &&
	  (window_private->extension_events != 0) &&
	  gdk_input_ignore_core)
	{
	  return_val = FALSE;
	  break;
	}
      
      event->motion.type = GDK_MOTION_NOTIFY;
      event->motion.window = window;
      event->motion.time = xevent->xmotion.time;
      event->motion.x = xevent->xmotion.x + xoffset;
      event->motion.y = xevent->xmotion.y + yoffset;
      event->motion.x_root = (gfloat)xevent->xmotion.x_root;
      event->motion.y_root = (gfloat)xevent->xmotion.y_root;
      event->motion.axes = NULL;
      event->motion.state = (GdkModifierType) xevent->xmotion.state;
      event->motion.is_hint = xevent->xmotion.is_hint;
      event->motion.device = gdk_core_pointer;
      
      break;
      
    case EnterNotify:
      GDK_NOTE (EVENTS,
		g_message ("enter notify:\t\twindow: %ld  detail: %d subwin: %ld",
			   xevent->xcrossing.window,
			   xevent->xcrossing.detail,
			   xevent->xcrossing.subwindow));

      /* Handle focusing (in the case where no window manager is running */
      if (window &&
	  GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD &&
	  xevent->xcrossing.detail != NotifyInferior &&
	  xevent->xcrossing.focus && !window_impl->has_focus)
	{
	  gboolean had_focus = HAS_FOCUS (window_impl);
	  
	  window_impl->has_pointer_focus = TRUE;

	  if (HAS_FOCUS (window_impl) != had_focus)
	    generate_focus_event (window, TRUE);
	}

      /* Tell XInput stuff about it if appropriate */
      if (window_private &&
	  !GDK_WINDOW_DESTROYED (window) &&
	  window_private->extension_events != 0)
	_gdk_input_enter_event (&xevent->xcrossing, window);
      
      event->crossing.type = GDK_ENTER_NOTIFY;
      event->crossing.window = window;
      
      /* If the subwindow field of the XEvent is non-NULL, then
       *  lookup the corresponding GdkWindow.
       */
      if (xevent->xcrossing.subwindow != None)
	event->crossing.subwindow = gdk_window_lookup (xevent->xcrossing.subwindow);
      else
	event->crossing.subwindow = NULL;
      
      event->crossing.time = xevent->xcrossing.time;
      event->crossing.x = xevent->xcrossing.x + xoffset;
      event->crossing.y = xevent->xcrossing.y + yoffset;
      event->crossing.x_root = xevent->xcrossing.x_root;
      event->crossing.y_root = xevent->xcrossing.y_root;
      
      /* Translate the crossing mode into Gdk terms.
       */
      switch (xevent->xcrossing.mode)
	{
	case NotifyNormal:
	  event->crossing.mode = GDK_CROSSING_NORMAL;
	  break;
	case NotifyGrab:
	  event->crossing.mode = GDK_CROSSING_GRAB;
	  break;
	case NotifyUngrab:
	  event->crossing.mode = GDK_CROSSING_UNGRAB;
	  break;
	};
      
      /* Translate the crossing detail into Gdk terms.
       */
      switch (xevent->xcrossing.detail)
	{
	case NotifyInferior:
	  event->crossing.detail = GDK_NOTIFY_INFERIOR;
	  break;
	case NotifyAncestor:
	  event->crossing.detail = GDK_NOTIFY_ANCESTOR;
	  break;
	case NotifyVirtual:
	  event->crossing.detail = GDK_NOTIFY_VIRTUAL;
	  break;
	case NotifyNonlinear:
	  event->crossing.detail = GDK_NOTIFY_NONLINEAR;
	  break;
	case NotifyNonlinearVirtual:
	  event->crossing.detail = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	  break;
	default:
	  event->crossing.detail = GDK_NOTIFY_UNKNOWN;
	  break;
	}
      
      event->crossing.focus = xevent->xcrossing.focus;
      event->crossing.state = xevent->xcrossing.state;
  
      break;
      
    case LeaveNotify:
      GDK_NOTE (EVENTS, 
		g_message ("leave notify:\t\twindow: %ld  detail: %d subwin: %ld",
			   xevent->xcrossing.window,
			   xevent->xcrossing.detail, xevent->xcrossing.subwindow));
      
      /* Handle focusing (in the case where no window manager is running */
      if (window &&
	  GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD &&
	  xevent->xcrossing.detail != NotifyInferior &&
	  xevent->xcrossing.focus && !window_impl->has_focus)
	{
	  gboolean had_focus = HAS_FOCUS (window_impl);
	  
	  window_impl->has_pointer_focus = FALSE;

	  if (HAS_FOCUS (window_impl) != had_focus)
	    generate_focus_event (window, FALSE);
	}

      event->crossing.type = GDK_LEAVE_NOTIFY;
      event->crossing.window = window;
      
      /* If the subwindow field of the XEvent is non-NULL, then
       *  lookup the corresponding GdkWindow.
       */
      if (xevent->xcrossing.subwindow != None)
	event->crossing.subwindow = gdk_window_lookup (xevent->xcrossing.subwindow);
      else
	event->crossing.subwindow = NULL;
      
      event->crossing.time = xevent->xcrossing.time;
      event->crossing.x = xevent->xcrossing.x + xoffset;
      event->crossing.y = xevent->xcrossing.y + yoffset;
      event->crossing.x_root = xevent->xcrossing.x_root;
      event->crossing.y_root = xevent->xcrossing.y_root;
      
      /* Translate the crossing mode into Gdk terms.
       */
      switch (xevent->xcrossing.mode)
	{
	case NotifyNormal:
	  event->crossing.mode = GDK_CROSSING_NORMAL;
	  break;
	case NotifyGrab:
	  event->crossing.mode = GDK_CROSSING_GRAB;
	  break;
	case NotifyUngrab:
	  event->crossing.mode = GDK_CROSSING_UNGRAB;
	  break;
	};
      
      /* Translate the crossing detail into Gdk terms.
       */
      switch (xevent->xcrossing.detail)
	{
	case NotifyInferior:
	  event->crossing.detail = GDK_NOTIFY_INFERIOR;
	  break;
	case NotifyAncestor:
	  event->crossing.detail = GDK_NOTIFY_ANCESTOR;
	  break;
	case NotifyVirtual:
	  event->crossing.detail = GDK_NOTIFY_VIRTUAL;
	  break;
	case NotifyNonlinear:
	  event->crossing.detail = GDK_NOTIFY_NONLINEAR;
	  break;
	case NotifyNonlinearVirtual:
	  event->crossing.detail = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	  break;
	default:
	  event->crossing.detail = GDK_NOTIFY_UNKNOWN;
	  break;
	}
      
      event->crossing.focus = xevent->xcrossing.focus;
      event->crossing.state = xevent->xcrossing.state;
      
      break;
      
      /* We only care about focus events that indicate that _this_
       * window (not a ancestor or child) got or lost the focus
       */
    case FocusIn:
      GDK_NOTE (EVENTS,
		g_message ("focus in:\t\twindow: %ld", xevent->xfocus.window));
      
      if (window && GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD)
	{
	  gboolean had_focus = HAS_FOCUS (window_impl);
	  
	  switch (xevent->xfocus.detail)
	    {
	    case NotifyAncestor:
	    case NotifyNonlinear:
	    case NotifyVirtual:
	    case NotifyNonlinearVirtual:
	      window_impl->has_focus = TRUE;
	      break;
	    case NotifyPointer:
	      window_impl->has_pointer_focus = TRUE;
	      break;
	    case NotifyInferior:
	    case NotifyPointerRoot:
	    case NotifyDetailNone:
	      break;
	    }

	  if (HAS_FOCUS (window_impl) != had_focus)
	    generate_focus_event (window, TRUE);
	}
      break;
    case FocusOut:
      GDK_NOTE (EVENTS,
		g_message ("focus out:\t\twindow: %ld", xevent->xfocus.window));

      if (window && GDK_WINDOW_TYPE (window) != GDK_WINDOW_CHILD)
	{
	  gboolean had_focus = HAS_FOCUS (window_impl);
	    
	  switch (xevent->xfocus.detail)
	    {
	    case NotifyAncestor:
	    case NotifyNonlinear:
	    case NotifyVirtual:
	    case NotifyNonlinearVirtual:
	      window_impl->has_focus = FALSE;
	      break;
	    case NotifyPointer:
	      window_impl->has_pointer_focus = FALSE;
	    break;
	    case NotifyInferior:
	    case NotifyPointerRoot:
	    case NotifyDetailNone:
	      break;
	    }

	  if (HAS_FOCUS (window_impl) != had_focus)
	    generate_focus_event (window, FALSE);
	}
      break;

#if 0      
 	  /* gdk_keyboard_grab() causes following events. These events confuse
 	   * the XIM focus, so ignore them.
 	   */
 	  if (xevent->xfocus.mode == NotifyGrab ||
 	      xevent->xfocus.mode == NotifyUngrab)
 	    break;
#endif

    case KeymapNotify:
      GDK_NOTE (EVENTS,
		g_message ("keymap notify"));

      /* Not currently handled */
      return_val = FALSE;
      break;
      
    case Expose:
      GDK_NOTE (EVENTS,
		g_message ("expose:\t\twindow: %ld  %d	x,y: %d %d  w,h: %d %d%s",
			   xevent->xexpose.window, xevent->xexpose.count,
			   xevent->xexpose.x, xevent->xexpose.y,
			   xevent->xexpose.width, xevent->xexpose.height,
			   event->any.send_event ? " (send)" : ""));
      {
	GdkRectangle expose_rect;

	expose_rect.x = xevent->xexpose.x + xoffset;
	expose_rect.y = xevent->xexpose.y + yoffset;
	expose_rect.width = xevent->xexpose.width;
	expose_rect.height = xevent->xexpose.height;

	if (return_exposes)
	  {
	    event->expose.type = GDK_EXPOSE;
	    event->expose.area = expose_rect;
	    event->expose.region = gdk_region_rectangle (&expose_rect);
	    event->expose.window = window;
	    event->expose.count = xevent->xexpose.count;

	    return_val = TRUE;
	  }
	else
	  {
	    _gdk_window_process_expose (window, xevent->xexpose.serial, &expose_rect);

	    return_val = FALSE;
	  }
	
	return_val = FALSE;
      }
	
      break;
      
    case GraphicsExpose:
      {
	GdkRectangle expose_rect;

        GDK_NOTE (EVENTS,
		  g_message ("graphics expose:\tdrawable: %ld",
			     xevent->xgraphicsexpose.drawable));

	expose_rect.x = xevent->xgraphicsexpose.x + xoffset;
	expose_rect.y = xevent->xgraphicsexpose.y + yoffset;
	expose_rect.width = xevent->xgraphicsexpose.width;
	expose_rect.height = xevent->xgraphicsexpose.height;
	    
	if (return_exposes)
	  {
	    event->expose.type = GDK_EXPOSE;
	    event->expose.area = expose_rect;
	    event->expose.region = gdk_region_rectangle (&expose_rect);
	    event->expose.window = window;
	    event->expose.count = xevent->xgraphicsexpose.count;

	    return_val = TRUE;
	  }
	else
	  {
	    _gdk_window_process_expose (window, xevent->xgraphicsexpose.serial, &expose_rect);
	    
	    return_val = FALSE;
	  }
	
      }
      break;
      
    case NoExpose:
      GDK_NOTE (EVENTS,
		g_message ("no expose:\t\tdrawable: %ld",
			   xevent->xnoexpose.drawable));
      
      event->no_expose.type = GDK_NO_EXPOSE;
      event->no_expose.window = window;
      
      break;
      
    case VisibilityNotify:
#ifdef G_ENABLE_DEBUG
      if (gdk_debug_flags & GDK_DEBUG_EVENTS)
	switch (xevent->xvisibility.state)
	  {
	  case VisibilityFullyObscured:
	    g_message ("visibility notify:\twindow: %ld	 none",
		       xevent->xvisibility.window);
	    break;
	  case VisibilityPartiallyObscured:
	    g_message ("visibility notify:\twindow: %ld	 partial",
		       xevent->xvisibility.window);
	    break;
	  case VisibilityUnobscured:
	    g_message ("visibility notify:\twindow: %ld	 full",
		       xevent->xvisibility.window);
	    break;
	  }
#endif /* G_ENABLE_DEBUG */
      
      event->visibility.type = GDK_VISIBILITY_NOTIFY;
      event->visibility.window = window;
      
      switch (xevent->xvisibility.state)
	{
	case VisibilityFullyObscured:
	  event->visibility.state = GDK_VISIBILITY_FULLY_OBSCURED;
	  break;
	  
	case VisibilityPartiallyObscured:
	  event->visibility.state = GDK_VISIBILITY_PARTIAL;
	  break;
	  
	case VisibilityUnobscured:
	  event->visibility.state = GDK_VISIBILITY_UNOBSCURED;
	  break;
	}
      
      break;
      
    case CreateNotify:
      GDK_NOTE (EVENTS,
		g_message ("create notify:\twindow: %ld  x,y: %d %d	w,h: %d %d  b-w: %d  parent: %ld	 ovr: %d",
			   xevent->xcreatewindow.window,
			   xevent->xcreatewindow.x,
			   xevent->xcreatewindow.y,
			   xevent->xcreatewindow.width,
			   xevent->xcreatewindow.height,
			   xevent->xcreatewindow.border_width,
			   xevent->xcreatewindow.parent,
			   xevent->xcreatewindow.override_redirect));
      /* not really handled */
      break;
      
    case DestroyNotify:
      GDK_NOTE (EVENTS,
		g_message ("destroy notify:\twindow: %ld",
			   xevent->xdestroywindow.window));
      
      event->any.type = GDK_DESTROY;
      event->any.window = window;
      
      return_val = window_private && !GDK_WINDOW_DESTROYED (window);
      
      if (window && GDK_WINDOW_XID (window) != GDK_ROOT_WINDOW())
	gdk_window_destroy_notify (window);
      break;
      
    case UnmapNotify:
      GDK_NOTE (EVENTS,
		g_message ("unmap notify:\t\twindow: %ld",
			   xevent->xmap.window));
      
      event->any.type = GDK_UNMAP;
      event->any.window = window;      

      /* If we are shown (not withdrawn) and get an unmap, it means we
       * were iconified in the X sense. If we are withdrawn, and get
       * an unmap, it means we hid the window ourselves, so we
       * will have already flipped the iconified bit off.
       */
      if (window && GDK_WINDOW_IS_MAPPED (window))
        gdk_synthesize_window_state (window,
                                     0,
                                     GDK_WINDOW_STATE_ICONIFIED);
      
      if (gdk_xgrab_window == window_private)
	gdk_xgrab_window = NULL;
      
      break;
      
    case MapNotify:
      GDK_NOTE (EVENTS,
		g_message ("map notify:\t\twindow: %ld",
			   xevent->xmap.window));
      
      event->any.type = GDK_MAP;
      event->any.window = window;

      /* Unset iconified if it was set */
      if (window && (((GdkWindowObject*)window)->state & GDK_WINDOW_STATE_ICONIFIED))
        gdk_synthesize_window_state (window,
                                     GDK_WINDOW_STATE_ICONIFIED,
                                     0);
      
      break;
      
    case ReparentNotify:
      GDK_NOTE (EVENTS,
		g_message ("reparent notify:\twindow: %ld  x,y: %d %d  parent: %ld	ovr: %d",
			   xevent->xreparent.window,
			   xevent->xreparent.x,
			   xevent->xreparent.y,
			   xevent->xreparent.parent,
			   xevent->xreparent.override_redirect));

      /* Not currently handled */
      return_val = FALSE;
      break;
      
    case ConfigureNotify:
      GDK_NOTE (EVENTS,
		g_message ("configure notify:\twindow: %ld  x,y: %d %d	w,h: %d %d  b-w: %d  above: %ld	 ovr: %d%s",
			   xevent->xconfigure.window,
			   xevent->xconfigure.x,
			   xevent->xconfigure.y,
			   xevent->xconfigure.width,
			   xevent->xconfigure.height,
			   xevent->xconfigure.border_width,
			   xevent->xconfigure.above,
			   xevent->xconfigure.override_redirect,
			   !window
			   ? " (discarding)"
			   : GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD
			   ? " (discarding child)"
			   : ""));
      if (window &&
	  !GDK_WINDOW_DESTROYED (window) &&
	  (window_private->extension_events != 0))
	_gdk_input_configure_event (&xevent->xconfigure, window);

      if (!window || GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD)
	return_val = FALSE;
      else
	{
	  event->configure.type = GDK_CONFIGURE;
	  event->configure.window = window;
	  event->configure.width = xevent->xconfigure.width;
	  event->configure.height = xevent->xconfigure.height;
	  
	  if (!xevent->xconfigure.x &&
	      !xevent->xconfigure.y &&
	      !GDK_WINDOW_DESTROYED (window))
	    {
	      gint tx = 0;
	      gint ty = 0;
	      Window child_window = 0;

	      gdk_error_trap_push ();
	      if (XTranslateCoordinates (GDK_DRAWABLE_XDISPLAY (window),
					 GDK_DRAWABLE_XID (window),
					 gdk_root_window,
					 0, 0,
					 &tx, &ty,
					 &child_window))
		{
		  if (!gdk_error_trap_pop ())
		    {
		      event->configure.x = tx;
		      event->configure.y = ty;
		    }
		}
	      else
		gdk_error_trap_pop ();
	    }
	  else
	    {
	      event->configure.x = xevent->xconfigure.x;
	      event->configure.y = xevent->xconfigure.y;
	    }
	  window_private->x = event->configure.x;
	  window_private->y = event->configure.y;
	  GDK_WINDOW_IMPL_X11 (window_private->impl)->width = xevent->xconfigure.width;
	  GDK_WINDOW_IMPL_X11 (window_private->impl)->height = xevent->xconfigure.height;
	  if (window_private->resize_count >= 1)
	    {
	      window_private->resize_count -= 1;

	      if (window_private->resize_count == 0 &&
		  window == _gdk_moveresize_window)
		_gdk_moveresize_configure_done ();
	    }
	}
      break;
      
    case PropertyNotify:
      GDK_NOTE (EVENTS,
		gchar *atom = gdk_atom_name (xevent->xproperty.atom);
		g_message ("property notify:\twindow: %ld, atom(%ld): %s%s%s",
			   xevent->xproperty.window,
			   xevent->xproperty.atom,
			   atom ? "\"" : "",
			   atom ? atom : "unknown",
			   atom ? "\"" : "");
		g_free (atom);
		);
      
      event->property.type = GDK_PROPERTY_NOTIFY;
      event->property.window = window;
      event->property.atom = xevent->xproperty.atom;
      event->property.time = xevent->xproperty.time;
      event->property.state = xevent->xproperty.state;

      if (wm_state_atom == 0)
        wm_state_atom = gdk_atom_intern ("_NET_WM_STATE", FALSE);

      if (wm_desktop_atom == 0)
        wm_desktop_atom = gdk_atom_intern ("_NET_WM_DESKTOP", FALSE);
      
      if (event->property.atom == wm_state_atom ||
          event->property.atom == wm_desktop_atom)
        {
          /* If window state changed, then synthesize those events. */
          gdk_check_wm_state_changed (event->property.window);
        }
      
      break;
      
    case SelectionClear:
      GDK_NOTE (EVENTS,
		g_message ("selection clear:\twindow: %ld",
			   xevent->xproperty.window));

      if (_gdk_selection_filter_clear_event (&xevent->xselectionclear))
	{
	  event->selection.type = GDK_SELECTION_CLEAR;
	  event->selection.window = window;
	  event->selection.selection = xevent->xselectionclear.selection;
	  event->selection.time = xevent->xselectionclear.time;
	}
      else
	return_val = FALSE;
	  
      break;
      
    case SelectionRequest:
      GDK_NOTE (EVENTS,
		g_message ("selection request:\twindow: %ld",
			   xevent->xproperty.window));
      
      event->selection.type = GDK_SELECTION_REQUEST;
      event->selection.window = window;
      event->selection.selection = xevent->xselectionrequest.selection;
      event->selection.target = xevent->xselectionrequest.target;
      event->selection.property = xevent->xselectionrequest.property;
      event->selection.requestor = xevent->xselectionrequest.requestor;
      event->selection.time = xevent->xselectionrequest.time;
      
      break;
      
    case SelectionNotify:
      GDK_NOTE (EVENTS,
		g_message ("selection notify:\twindow: %ld",
			   xevent->xproperty.window));
      
      
      event->selection.type = GDK_SELECTION_NOTIFY;
      event->selection.window = window;
      event->selection.selection = xevent->xselection.selection;
      event->selection.target = xevent->xselection.target;
      event->selection.property = xevent->xselection.property;
      event->selection.time = xevent->xselection.time;
      
      break;
      
    case ColormapNotify:
      GDK_NOTE (EVENTS,
		g_message ("colormap notify:\twindow: %ld",
			   xevent->xcolormap.window));
      
      /* Not currently handled */
      return_val = FALSE;
      break;
      
    case ClientMessage:
      {
	GList *tmp_list;
	GdkFilterReturn result = GDK_FILTER_CONTINUE;

	GDK_NOTE (EVENTS,
		  g_message ("client message:\twindow: %ld",
			     xevent->xclient.window));
	
	tmp_list = client_filters;
	while (tmp_list)
	  {
	    GdkClientFilter *filter = tmp_list->data;
	    if (filter->type == xevent->xclient.message_type)
	      {
		result = (*filter->function) (xevent, event, filter->data);
		break;
	      }
	    
	    tmp_list = tmp_list->next;
	  }

	switch (result)
	  {
	  case GDK_FILTER_REMOVE:
	    return_val = FALSE;
	    break;
	  case GDK_FILTER_TRANSLATE:
	    return_val = TRUE;
	    break;
	  case GDK_FILTER_CONTINUE:
	    /* Send unknown ClientMessage's on to Gtk for it to use */
	    event->client.type = GDK_CLIENT_EVENT;
	    event->client.window = window;
	    event->client.message_type = xevent->xclient.message_type;
	    event->client.data_format = xevent->xclient.format;
	    memcpy(&event->client.data, &xevent->xclient.data,
		   sizeof(event->client.data));
	  }
      }
      
      break;
      
    case MappingNotify:
      GDK_NOTE (EVENTS,
		g_message ("mapping notify"));
      
      /* Let XLib know that there is a new keyboard mapping.
       */
      XRefreshKeyboardMapping (&xevent->xmapping);
      ++_gdk_keymap_serial;
      return_val = FALSE;
      break;

#ifdef HAVE_XKB
    case XkbMapNotify:
      ++_gdk_keymap_serial;
      return_val = FALSE;
      break;
#endif
      
    default:
      /* something else - (e.g., a Xinput event) */
      
      if (window_private &&
	  !GDK_WINDOW_DESTROYED (window_private) &&
	  (window_private->extension_events != 0))
	return_val = _gdk_input_other_event(event, xevent, window);
      else
	return_val = FALSE;
      
      break;
    }

 done:
  if (return_val)
    {
      if (event->any.window)
	gdk_window_ref (event->any.window);
      if (((event->any.type == GDK_ENTER_NOTIFY) ||
	   (event->any.type == GDK_LEAVE_NOTIFY)) &&
	  (event->crossing.subwindow != NULL))
	gdk_window_ref (event->crossing.subwindow);
    }
  else
    {
      /* Mark this event as having no resources to be freed */
      event->any.window = NULL;
      event->any.type = GDK_NOTHING;
    }
  
  if (window)
    gdk_window_unref (window);
  
  return return_val;
}

GdkFilterReturn
gdk_wm_protocols_filter (GdkXEvent *xev,
			 GdkEvent  *event,
			 gpointer data)
{
  XEvent *xevent = (XEvent *)xev;

  if ((Atom) xevent->xclient.data.l[0] == gdk_wm_delete_window)
    {
  /* The delete window request specifies a window
   *  to delete. We don't actually destroy the
   *  window because "it is only a request". (The
   *  window might contain vital data that the
   *  program does not want destroyed). Instead
   *  the event is passed along to the program,
   *  which should then destroy the window.
   */
      GDK_NOTE (EVENTS,
		g_message ("delete window:\t\twindow: %ld",
			   xevent->xclient.window));
      
      event->any.type = GDK_DELETE;

      return GDK_FILTER_TRANSLATE;
    }
  else if ((Atom) xevent->xclient.data.l[0] == gdk_wm_take_focus)
    {
      GdkWindow *win = event->any.window;
      Window focus_win = GDK_WINDOW_IMPL_X11(((GdkWindowObject *)win)->impl)->focus_window;

      XSetInputFocus (GDK_WINDOW_XDISPLAY (win), focus_win,
		      RevertToParent, xevent->xclient.data.l[1]);
    }
  else if ((Atom) xevent->xclient.data.l[0] == gdk_atom_intern ("_NET_WM_PING", FALSE))
    {
      XEvent xev = *xevent;
      
      xev.xclient.window = gdk_root_window;
      XSendEvent (gdk_display, gdk_root_window, False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    }

  return GDK_FILTER_REMOVE;
}

#if 0
static Bool
gdk_event_get_type (Display  *display,
		    XEvent   *xevent,
		    XPointer  arg)
{
  GdkEvent event;
  GdkPredicate *pred;
  
  if (gdk_event_translate (&event, xevent, FALSE))
    {
      pred = (GdkPredicate*) arg;
      return (* pred->func) (&event, pred->data);
    }
  
  return FALSE;
}
#endif

void
gdk_events_queue (void)
{
  GList *node;
  GdkEvent *event;
  XEvent xevent;

  while (!gdk_event_queue_find_first() && XPending (gdk_display))
    {
      XNextEvent (gdk_display, &xevent);

      switch (xevent.type)
	{
	case KeyPress:
	case KeyRelease:
	  break;
	default:
	  if (XFilterEvent (&xevent, None))
	    continue;
	}
      
      event = gdk_event_new ();
      
      event->any.type = GDK_NOTHING;
      event->any.window = NULL;
      event->any.send_event = xevent.xany.send_event ? TRUE : FALSE;

      ((GdkEventPrivate *)event)->flags |= GDK_EVENT_PENDING;

      gdk_event_queue_append (event);
      node = gdk_queued_tail;

      if (gdk_event_translate (event, &xevent, FALSE))
	{
	  ((GdkEventPrivate *)event)->flags &= ~GDK_EVENT_PENDING;
	}
      else
	{
	  gdk_event_queue_remove_link (node);
	  g_list_free_1 (node);
	  gdk_event_free (event);
	}
    }
}

static gboolean  
gdk_event_prepare (GSource  *source,
		   gint     *timeout)
{
  gboolean retval;
  
  GDK_THREADS_ENTER ();

  *timeout = -1;

  retval = (gdk_event_queue_find_first () != NULL) || XPending (gdk_display);

  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean  
gdk_event_check (GSource  *source) 
{
  gboolean retval;
  
  GDK_THREADS_ENTER ();

  if (event_poll_fd.revents & G_IO_IN)
    retval = (gdk_event_queue_find_first () != NULL) || XPending (gdk_display);
  else
    retval = FALSE;

  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean  
gdk_event_dispatch (GSource    *source,
		    GSourceFunc callback,
		    gpointer    user_data)
{
  GdkEvent *event;
 
  GDK_THREADS_ENTER ();

  gdk_events_queue();
  event = gdk_event_unqueue();

  if (event)
    {
      if (gdk_event_func)
	(*gdk_event_func) (event, gdk_event_data);
      
      gdk_event_free (event);
    }
  
  GDK_THREADS_LEAVE ();

  return TRUE;
}

/* Sends a ClientMessage to all toplevel client windows */
gboolean
gdk_event_send_client_message (GdkEvent *event, guint32 xid)
{
  XEvent sev;
  
  g_return_val_if_fail(event != NULL, FALSE);
  
  /* Set up our event to send, with the exception of its target window */
  sev.xclient.type = ClientMessage;
  sev.xclient.display = gdk_display;
  sev.xclient.format = event->client.data_format;
  sev.xclient.window = xid;
  memcpy(&sev.xclient.data, &event->client.data, sizeof(sev.xclient.data));
  sev.xclient.message_type = event->client.message_type;
  
  return gdk_send_xevent (xid, False, NoEventMask, &sev);
}

/* Sends a ClientMessage to all toplevel client windows */
gboolean
gdk_event_send_client_message_to_all_recurse (XEvent  *xev, 
					      guint32  xid,
					      guint    level)
{
  static GdkAtom wm_state_atom = GDK_NONE;
  Atom type = None;
  int format;
  unsigned long nitems, after;
  unsigned char *data;
  Window *ret_children, ret_root, ret_parent;
  unsigned int ret_nchildren;
  gint old_warnings = gdk_error_warnings;
  gboolean send = FALSE;
  gboolean found = FALSE;
  int i;

  if (!wm_state_atom)
    wm_state_atom = gdk_atom_intern ("WM_STATE", FALSE);

  gdk_error_warnings = FALSE;
  gdk_error_code = 0;
  XGetWindowProperty (gdk_display, xid, wm_state_atom, 0, 0, False, AnyPropertyType,
		      &type, &format, &nitems, &after, &data);

  if (gdk_error_code)
    {
      gdk_error_warnings = old_warnings;

      return FALSE;
    }

  if (type)
    {
      send = TRUE;
      XFree (data);
    }
  else
    {
      /* OK, we're all set, now let's find some windows to send this to */
      if (XQueryTree (gdk_display, xid, &ret_root, &ret_parent,
		      &ret_children, &ret_nchildren) != True ||
	  gdk_error_code)
	{
	  gdk_error_warnings = old_warnings;

	  return FALSE;
	}

      for(i = 0; i < ret_nchildren; i++)
	if (gdk_event_send_client_message_to_all_recurse (xev, ret_children[i], level + 1))
	  found = TRUE;

      XFree (ret_children);
    }

  if (send || (!found && (level == 1)))
    {
      xev->xclient.window = xid;
      gdk_send_xevent (xid, False, NoEventMask, xev);
    }

  gdk_error_warnings = old_warnings;

  return (send || found);
}

void
gdk_event_send_clientmessage_toall (GdkEvent *event)
{
  XEvent sev;
  gint old_warnings = gdk_error_warnings;

  g_return_if_fail(event != NULL);
  
  /* Set up our event to send, with the exception of its target window */
  sev.xclient.type = ClientMessage;
  sev.xclient.display = gdk_display;
  sev.xclient.format = event->client.data_format;
  memcpy(&sev.xclient.data, &event->client.data, sizeof(sev.xclient.data));
  sev.xclient.message_type = event->client.message_type;

  gdk_event_send_client_message_to_all_recurse(&sev, gdk_root_window, 0);

  gdk_error_warnings = old_warnings;
}

/*
 *--------------------------------------------------------------
 * gdk_flush
 *
 *   Flushes the Xlib output buffer and then waits
 *   until all requests have been received and processed
 *   by the X server. The only real use for this function
 *   is in dealing with XShm.
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
gdk_flush (void)
{
  XSync (gdk_display, False);
}

static GdkAtom timestamp_prop_atom = 0;

static Bool
timestamp_predicate (Display *display,
		     XEvent  *xevent,
		     XPointer arg)
{
  Window xwindow = GPOINTER_TO_UINT (arg);

  if (xevent->type == PropertyNotify &&
      xevent->xproperty.window == xwindow &&
      xevent->xproperty.atom == timestamp_prop_atom)
    return True;

  return False;
}

/**
 * gdk_x11_get_server_time:
 * @window: a #GdkWindow, used for communication with the server.
 *          The window must have GDK_PROPERTY_CHANGE_MASK in its
 *          events mask or a hang will result.
 * 
 * Routine to get the current X server time stamp. 
 * 
 * Return value: the time stamp.
 **/
guint32
gdk_x11_get_server_time (GdkWindow *window)
{
  Display *xdisplay;
  Window   xwindow;
  guchar c = 'a';
  XEvent xevent;

  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  g_return_val_if_fail (!GDK_WINDOW_DESTROYED (window), 0);

  if (!timestamp_prop_atom)
    timestamp_prop_atom = gdk_atom_intern ("GDK_TIMESTAMP_PROP", FALSE);

  xdisplay = GDK_WINDOW_XDISPLAY (window);
  xwindow = GDK_WINDOW_XWINDOW (window);
  
  XChangeProperty (xdisplay, xwindow,
		   timestamp_prop_atom, timestamp_prop_atom,
		   8, PropModeReplace, &c, 1);

  XIfEvent (xdisplay, &xevent,
	    timestamp_predicate, GUINT_TO_POINTER(xwindow));

  return xevent.xproperty.time;
}


gboolean
gdk_net_wm_supports (GdkAtom property)
{
  static GdkAtom wmspec_check_atom = 0;
  static GdkAtom wmspec_supported_atom = 0;
  static GdkAtom *atoms = NULL;
  static gulong n_atoms = 0;
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  Window *xwindow;
  gulong i;

  if (wmspec_check_window != None)
    {
      if (atoms == NULL)
        return FALSE;

      i = 0;
      while (i < n_atoms)
        {
          if (atoms[i] == property)
            return TRUE;
          
          ++i;
        }

      return FALSE;
    }

  if (atoms)
    XFree (atoms);

  atoms = NULL;
  n_atoms = 0;
  
  /* This function is very slow on every call if you are not running a
   * spec-supporting WM. For now not optimized, because it isn't in
   * any critical code paths, but if you used it somewhere that had to
   * be fast you want to avoid "GTK is slow with old WMs" complaints.
   * Probably at that point the function should be changed to query
   * _NET_SUPPORTING_WM_CHECK only once every 10 seconds or something.
   */
  
  if (wmspec_check_atom == 0)
    wmspec_check_atom = gdk_atom_intern ("_NET_SUPPORTING_WM_CHECK", FALSE);
      
  if (wmspec_supported_atom == 0)
    wmspec_supported_atom = gdk_atom_intern ("_NET_SUPPORTED", FALSE);
  
  XGetWindowProperty (gdk_display, gdk_root_window,
		      wmspec_check_atom, 0, G_MAXLONG,
		      False, XA_WINDOW, &type, &format, &nitems,
		      &bytes_after, (guchar **)&xwindow);

  if (type != XA_WINDOW)
    return FALSE;

  gdk_error_trap_push ();

  /* Find out if this WM goes away, so we can reset everything. */
  XSelectInput (gdk_display, *xwindow,
                StructureNotifyMask);
  
  gdk_flush ();
  
  if (gdk_error_trap_pop ())
    {
      XFree (xwindow);
      return FALSE;
    }

  XGetWindowProperty (gdk_display, gdk_root_window,
		      wmspec_supported_atom, 0, G_MAXLONG,
		      False, XA_ATOM, &type, &format, &n_atoms,
		      &bytes_after, (guchar **)&atoms);
  
  if (type != XA_ATOM)
    return FALSE;
  
  wmspec_check_window = *xwindow;
  XFree (xwindow);
  
  /* since wmspec_check_window != None this isn't infinite. ;-) */
  return gdk_net_wm_supports (property);
}

static struct
{
  const char *xsettings_name;
  const char *gdk_name;
} settings_map[] = {
  { "Net/DoubleClickTime", "double-click-timeout" },
  { "Net/DragThreshold", "drag-threshold" }
};

static void
gdk_xsettings_notify_cb (const char       *name,
			 XSettingsAction   action,
			 XSettingsSetting *setting,
			 void             *data)
{
  GdkEvent new_event;
  int i;

  new_event.type = GDK_SETTING;
  new_event.setting.window = NULL;
  new_event.setting.send_event = FALSE;
  new_event.setting.name = NULL;

  for (i = 0; i < G_N_ELEMENTS (settings_map) ; i++)
    if (strcmp (settings_map[i].xsettings_name, name) == 0)
      {
	new_event.setting.name = g_strdup (settings_map[i].gdk_name);
	break;
      }

  if (!new_event.setting.name)
    return;
  
  switch (action)
    {
    case XSETTINGS_ACTION_NEW:
      new_event.setting.action = GDK_SETTING_ACTION_NEW;
      break;
    case XSETTINGS_ACTION_CHANGED:
      new_event.setting.action = GDK_SETTING_ACTION_CHANGED;
      break;
    case XSETTINGS_ACTION_DELETED:
      new_event.setting.action = GDK_SETTING_ACTION_DELETED;
      break;
    }

  gdk_event_put (&new_event);
}

static gboolean
check_transform (const gchar *xsettings_name,
		 GType        src_type,
		 GType        dest_type)
{
  if (!g_value_type_transformable (src_type, dest_type))
    {
      g_warning ("Cannot tranform xsetting %s of type %s to type %s\n",
		 xsettings_name,
		 g_type_name (src_type),
		 g_type_name (dest_type));
      return FALSE;
    }
  else
    return TRUE;
}

gboolean
gdk_setting_get (const gchar *name,
		 GValue      *value)
{
  const char *xsettings_name = NULL;
  XSettingsResult result;
  XSettingsSetting *setting;
  gboolean success = FALSE;
  gint i;

  for (i = 0; i < G_N_ELEMENTS (settings_map) ; i++)
    if (strcmp (settings_map[i].gdk_name, name) == 0)
      {
	xsettings_name = settings_map[i].xsettings_name;
	break;
      }

  if (!xsettings_name)
    return FALSE;

  result = xsettings_client_get_setting (xsettings_client, xsettings_name, &setting);
  if (result != XSETTINGS_SUCCESS)
    return FALSE;

  switch (setting->type)
    {
    case XSETTINGS_TYPE_INT:
      if (check_transform (xsettings_name, G_TYPE_INT, G_VALUE_TYPE (value)))
	{
	  g_value_set_int (value, setting->data.v_int);
	  success = TRUE;
	}
      break;
    case XSETTINGS_TYPE_STRING:
      if (check_transform (xsettings_name, G_TYPE_STRING, G_VALUE_TYPE (value)))
	{
	  g_value_set_string (value, setting->data.v_string);
	  success = TRUE;
	}
      break;
    case XSETTINGS_TYPE_COLOR:
      if (!check_transform (xsettings_name, GDK_TYPE_COLOR, G_VALUE_TYPE (value)))
	{
	  GdkColor color;
	  
	  color.pixel = 0;
	  color.red = setting->data.v_color.red;
	  color.green = setting->data.v_color.green;
	  color.blue = setting->data.v_color.blue;
	  
	  g_value_set_boxed (value, &color);
	  
	  success = TRUE;
	}
      break;
    }

  xsettings_setting_free (setting);

  return success;
}

GdkFilterReturn 
gdk_xsettings_client_event_filter (GdkXEvent *xevent,
				   GdkEvent  *event,
				   gpointer   data)
{
  if (xsettings_client_process_event (xsettings_client, (XEvent *)xevent))
    return GDK_FILTER_REMOVE;
  else
    return GDK_FILTER_CONTINUE;
}

static void 
gdk_xsettings_watch_cb (Window window,
			Bool   is_start,
			long   mask,
			void  *cb_data)
{
  GdkWindow *gdkwin;

  gdkwin = gdk_window_lookup (window);
  if (is_start)
    gdk_window_add_filter (gdkwin, gdk_xsettings_client_event_filter, NULL);
  else
    gdk_window_remove_filter (gdkwin, gdk_xsettings_client_event_filter, NULL);
}
