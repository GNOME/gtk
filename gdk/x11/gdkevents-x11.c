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
#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"

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
typedef struct _GdkDisplaySource GdkDisplaySource;

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

struct _GdkDisplaySource
{
  GSource source;
  
  GdkDisplay *display;
  GPollFD event_poll_fd;
};

/* 
 * Private function declarations
 */

static gint	 gdk_event_apply_filters (XEvent   *xevent,
					  GdkEvent *event,
					  GList    *filters);
static gboolean	 gdk_event_translate	 (GdkDisplay *display,
					  GdkEvent   *event, 
					  XEvent     *xevent,
					  gboolean    return_exposes);

static gboolean gdk_event_prepare  (GSource     *source,
				    gint        *timeout);
static gboolean gdk_event_check    (GSource     *source);
static gboolean gdk_event_dispatch (GSource     *source,
				    GSourceFunc  callback,
				    gpointer     user_data);

static GdkFilterReturn gdk_wm_protocols_filter (GdkXEvent *xev,
						GdkEvent  *event,
						gpointer   data);

static GSource *gdk_display_source_new (GdkDisplay *display);
static gboolean gdk_check_xpending     (GdkDisplay *display);

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

static GList *display_sources;

static GSourceFuncs event_funcs = {
  gdk_event_prepare,
  gdk_event_check,
  gdk_event_dispatch,
  NULL
};

static GSource *
gdk_display_source_new (GdkDisplay *display)
{
  GSource *source = g_source_new (&event_funcs, sizeof (GdkDisplaySource));
  GdkDisplaySource *display_source = (GdkDisplaySource *)source;
  
  display_source->display = display;
  
  return source;
}

static gboolean
gdk_check_xpending (GdkDisplay *display)
{
  return XPending (GDK_DISPLAY_XDISPLAY (display));
}

/*********************************************
 * Functions for maintaining the event queue *
 *********************************************/

void
_gdk_x11_events_init_screen (GdkScreen *screen)
{
  GdkScreenX11 *screen_x11 = GDK_SCREEN_X11 (screen);
  
  screen_x11->xsettings_client = xsettings_client_new (screen_x11->xdisplay,
						       screen_x11->screen_num,
						       gdk_xsettings_notify_cb,
						       gdk_xsettings_watch_cb,
						       screen);
}

void 
_gdk_events_init (GdkDisplay *display)
{
  GSource *source;
  GdkDisplaySource *display_source;
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  
  int connection_number = ConnectionNumber (display_x11->xdisplay);
  GDK_NOTE (MISC, g_message ("connection number: %d", connection_number));


  source = gdk_display_source_new (display);
  display_source = (GdkDisplaySource*) source;
  g_source_set_priority (source, GDK_PRIORITY_EVENTS);
  
  display_source->event_poll_fd.fd = connection_number;
  display_source->event_poll_fd.events = G_IO_IN;
  
  g_source_add_poll (source, &display_source->event_poll_fd);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  display_sources = g_list_prepend (display_sources,display_source);

  gdk_display_add_client_message_filter (
	display,
	gdk_atom_intern ("WM_PROTOCOLS", FALSE), 
	gdk_wm_protocols_filter,   
	NULL);

  _gdk_x11_events_init_screen (display_x11->default_screen);
}


/**
 * gdk_events_pending:
 * 
 * Checks if any events are ready to be processed for any display.
 * 
 * Return value:  %TRUE if any events are pending.
 **/
gboolean
gdk_events_pending (void)
{
  GList *tmp_list;

  for (tmp_list = display_sources; tmp_list; tmp_list = tmp_list->next)
    {
      GdkDisplaySource *tmp_source = tmp_list->data;
      GdkDisplay *display = tmp_source->display;
      
      if (_gdk_event_queue_find_first (display))
	return TRUE;
    }

  for (tmp_list = display_sources; tmp_list; tmp_list = tmp_list->next)
    {
      GdkDisplaySource *tmp_source = tmp_list->data;
      GdkDisplay *display = tmp_source->display;
      
      if (gdk_check_xpending (display))
	return TRUE;
    }
  
  return FALSE;
}

static Bool
graphics_expose_predicate (Display  *display,
			   XEvent   *xevent,
			   XPointer  arg)
{
  if (xevent->xany.window == GDK_DRAWABLE_XID ((GdkDrawable *)arg) &&
      (xevent->xany.type == GraphicsExpose ||
       xevent->xany.type == NoExpose))
    return True;
  else
    return False;
}

/**
 * gdk_event_get_graphics_expose:
 * @window: the #GdkWindow to wait for the events for.
 * 
 * Waits for a GraphicsExpose or NoExpose event from the X server.
 * This is used in the #GtkText and #GtkCList widgets in GTK+ to make sure any
 * GraphicsExpose events are handled before the widget is scrolled.
 *
 * Return value:  a #GdkEventExpose if a GraphicsExpose was received, or %NULL if a
 * NoExpose event was received.
 **/
GdkEvent*
gdk_event_get_graphics_expose (GdkWindow *window)
{
  XEvent xevent;
  GdkEvent *event;
  
  g_return_val_if_fail (window != NULL, NULL);
  
  XIfEvent (GDK_WINDOW_XDISPLAY (window), &xevent, 
	    graphics_expose_predicate, (XPointer) window);
  
  if (xevent.xany.type == GraphicsExpose)
    {
      event = _gdk_event_new ();
      
      if (gdk_event_translate (GDK_WINDOW_DISPLAY (window), event,
			       &xevent, TRUE))
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

/**
 * gdk_display_add_client_message_filter:
 * @display: a #GdkDisplay for which this message filter applies
 * @message_type: the type of ClientMessage events to receive.
 *   This will be checked against the @message_type field 
 *   of the XClientMessage event struct.
 * @func: the function to call to process the event.
 * @data: user data to pass to @func.
 *
 * Adds a filter to be called when X ClientMessage events are received.
 *
 **/ 
void 
gdk_display_add_client_message_filter (GdkDisplay   *display,
				       GdkAtom       message_type,
				       GdkFilterFunc func,
				       gpointer      data)
{
  GdkClientFilter *filter;
  g_return_if_fail (GDK_IS_DISPLAY (display));
  filter = g_new (GdkClientFilter, 1);

  filter->type = message_type;
  filter->function = func;
  filter->data = data;
  
  GDK_DISPLAY_X11(display)->client_filters = 
    g_list_prepend (GDK_DISPLAY_X11 (display)->client_filters,
		    filter);
}

/**
 * gdk_add_client_message_filter:
 * @message_type: the type of ClientMessage events to receive. This will be
 *     checked against the <structfield>message_type</structfield> field of the
 *     XClientMessage event struct.
 * @func: the function to call to process the event.
 * @data: user data to pass to @func. 
 * 
 * Adds a filter to the default display to be called when X ClientMessage events
 * are received. See gdk_display_add_client_message_filter().
 **/
void 
gdk_add_client_message_filter (GdkAtom       message_type,
			       GdkFilterFunc func,
			       gpointer      data)
{
  gdk_display_add_client_message_filter (gdk_get_default_display (),
					 message_type, func, data);
}

static void
gdk_check_wm_state_changed (GdkWindow *window)
{  
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  Atom *atoms = NULL;
  gulong i;
  gboolean found_sticky, found_maxvert, found_maxhorz;
  GdkWindowState old_state;
  GdkDisplay *display = GDK_WINDOW_DISPLAY (window);
  
  if (GDK_WINDOW_DESTROYED (window) ||
      gdk_window_get_window_type (window) != GDK_WINDOW_TOPLEVEL)
    return;
  
  found_sticky = FALSE;
  found_maxvert = FALSE;
  found_maxhorz = FALSE;
  
  XGetWindowProperty (GDK_WINDOW_XDISPLAY (window), GDK_WINDOW_XID (window),
		      gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE"),
		      0, G_MAXLONG, False, XA_ATOM, &type, &format, &nitems,
		      &bytes_after, (guchar **)&atoms);

  if (type != None)
    {
      Atom sticky_atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE_STICKY");
      Atom maxvert_atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE_MAXIMIZED_VERT");
      Atom maxhorz_atom	= gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE_MAXIMIZED_HORZ");

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

  /* For found_sticky to remain TRUE, we have to also be on desktop
   * 0xFFFFFFFF
   */

  if (found_sticky)
    {
      gulong *desktop;
      
      XGetWindowProperty (GDK_WINDOW_XDISPLAY (window), 
			  GDK_WINDOW_XID (window),
                          gdk_x11_get_xatom_by_name_for_display 
			  (display, "_NET_WM_DESKTOP"),
			  0, G_MAXLONG, False, XA_CARDINAL, &type, 
			  &format, &nitems,
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

static gboolean
gdk_event_translate (GdkDisplay *display,
		     GdkEvent   *event,
		     XEvent     *xevent,
		     gboolean    return_exposes)
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
  GdkScreen *screen = NULL;
  GdkScreenX11 *screen_x11 = NULL;
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display);
  
  return_val = FALSE;

  /* init these, since the done: block uses them */
  window = NULL;
  window_private = NULL;
  event->any.window = NULL;

  if (_gdk_default_filters)
    {
      /* Apply global filters */
      GdkFilterReturn result;
      result = gdk_event_apply_filters (xevent, event,
                                        _gdk_default_filters);
      
      if (result != GDK_FILTER_CONTINUE)
        {
          return_val = (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
          goto done;
        }
    }  

   /* We handle events with window=None
    *  specially - they are generated by XFree86's XInput under
    *  some circumstances. This handling for obvious reasons
    * goes before we bother to lookup the event window.
    */
  
  if (xevent->xany.window == None)
    {
      return_val = _gdk_input_window_none_event (event, xevent);
      
      if (return_val >= 0)	/* was handled */
	return return_val;
      else
	return_val = FALSE;
    }

  /* Find the GdkWindow that this event occurred in. */
  
  window = gdk_window_lookup_for_display (display, xevent->xany.window);
  window_private = (GdkWindowObject *) window;

  if (window)
    {
      screen = GDK_WINDOW_SCREEN (window);
      screen_x11 = GDK_SCREEN_X11 (screen);
    }
    
  if (window != NULL)
    {
      /* Window may be a pixmap, so check its type.
       * (This check is probably too expensive unless
       *  GLib short-circuits an exact type match,
       *  which has been proposed)
       */
      if (GDK_IS_WINDOW (window))
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
                  return FALSE;
                }
            }
        }

      g_object_ref (G_OBJECT (window));
    }

  event->any.window = window;
  event->any.send_event = xevent->xany.send_event ? TRUE : FALSE;
  
  if (window_private && GDK_WINDOW_DESTROYED (window))
    {
      if (xevent->type != DestroyNotify)
	{
	  return_val = FALSE;
	  goto done;
	}
    }
  else if (window_private)
    {
      /* Apply per-window filters */
      GdkFilterReturn result;
      result = gdk_event_apply_filters (xevent, event,
					window_private->filters);
      
      if (result != GDK_FILTER_CONTINUE)
	{
	  return_val = (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
	  goto done;
	}
    }
      
  if (screen_x11 && screen_x11->wmspec_check_window != None &&
      xevent->xany.window == screen_x11->wmspec_check_window)
    {
      if (xevent->type == DestroyNotify)
        screen_x11->wmspec_check_window = None;
      
      /* Eat events on this window unless someone had wrapped
       * it as a foreign window
       */
      if (window == NULL)
	{
	  return_val = FALSE;
	  goto done;
	}
    }

  if (window &&
      (xevent->xany.type == MotionNotify ||
       xevent->xany.type == ButtonRelease))
    {
      if (_gdk_moveresize_handle_event (xevent))
	{
          return_val = FALSE;
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
      if (window_private == NULL)
        {
          return_val = FALSE;
          break;
        }
      
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
      if (_gdk_debug_flags & GDK_DEBUG_EVENTS)
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

      event->key.type = GDK_KEY_PRESS;
      event->key.window = window;
      event->key.time = xevent->xkey.time;
      event->key.state = (GdkModifierType) xevent->xkey.state;
      event->key.string = g_strdup (buf);
      event->key.length = charcount;

      /* bits 13 and 14 in the "state" field are the keyboard group */
#define KEYBOARD_GROUP_SHIFT 13
#define KEYBOARD_GROUP_MASK ((1 << 13) | (1 << 14))
      
      event->key.group = _gdk_x11_get_group_for_state (display, xevent->xkey.state);
      
      break;
      
    case KeyRelease:
      if (window_private == NULL)
        {
          return_val = FALSE;
          break;
        }
      
      /* Lookup the string corresponding to the given keysym.
       */

      /* Emulate detectable auto-repeat by checking to see
       * if the next event is a key press with the same
       * keycode and timestamp, and if so, ignoring the event.
       */

      if (!display_x11->have_xkb_autorepeat && XPending (xevent->xkey.display))
	{
	  XEvent next_event;

	  XPeekEvent (xevent->xkey.display, &next_event);

	  if (next_event.type == KeyPress &&
	      next_event.xkey.keycode == xevent->xkey.keycode &&
	      next_event.xkey.time == xevent->xkey.time)
	    break;
	}
      
      keysym = GDK_VoidSymbol;
      charcount = XLookupString (&xevent->xkey, buf, 16,
				 &keysym, &compose);
      event->key.keyval = keysym;      
      event->key.hardware_keycode = xevent->xkey.keycode;
      
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
      
      event->key.group = (xevent->xkey.state & KEYBOARD_GROUP_MASK) >> KEYBOARD_GROUP_SHIFT;

      break;
      
    case ButtonPress:
      GDK_NOTE (EVENTS, 
		g_message ("button press:\t\twindow: %ld  x,y: %d %d  button: %d",
			   xevent->xbutton.window,
			   xevent->xbutton.x, xevent->xbutton.y,
			   xevent->xbutton.button));
      
      if (window_private == NULL || 
	  ((window_private->extension_events != 0) &&
           display_x11->input_ignore_core))
	{
	  return_val = FALSE;
	  break;
	}
      
      /* If we get a ButtonPress event where the button is 4 or 5,
	 it's a Scroll event */
      switch (xevent->xbutton.button)
        {
        case 4: /* up */
        case 5: /* down */
        case 6: /* left */
        case 7: /* right */
	  event->scroll.type = GDK_SCROLL;

          if (xevent->xbutton.button == 4)
            event->scroll.direction = GDK_SCROLL_UP;
          else if (xevent->xbutton.button == 5)
            event->scroll.direction = GDK_SCROLL_DOWN;
          else if (xevent->xbutton.button == 6)
            event->scroll.direction = GDK_SCROLL_LEFT;
          else
            event->scroll.direction = GDK_SCROLL_RIGHT;

	  event->scroll.window = window;
	  event->scroll.time = xevent->xbutton.time;
	  event->scroll.x = xevent->xbutton.x + xoffset;
	  event->scroll.y = xevent->xbutton.y + yoffset;
	  event->scroll.x_root = (gfloat)xevent->xbutton.x_root;
	  event->scroll.y_root = (gfloat)xevent->xbutton.y_root;
	  event->scroll.state = (GdkModifierType) xevent->xbutton.state;
	  event->scroll.device = display->core_pointer;
          break;
          
        default:
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
	  event->button.device = display->core_pointer;
	  
	  _gdk_event_button_generate (display, event);
          break;
	}

      break;
      
    case ButtonRelease:
      GDK_NOTE (EVENTS, 
		g_message ("button release:\twindow: %ld  x,y: %d %d  button: %d",
			   xevent->xbutton.window,
			   xevent->xbutton.x, xevent->xbutton.y,
			   xevent->xbutton.button));
      
      if (window_private == NULL ||
	  ((window_private->extension_events != 0) &&
           display_x11->input_ignore_core))
	{
	  return_val = FALSE;
	  break;
	}
      
      /* We treat button presses as scroll wheel events, so ignore the release */
      if (xevent->xbutton.button == 4 || xevent->xbutton.button == 5 ||
          xevent->xbutton.button == 6 || xevent->xbutton.button ==7)
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
      event->button.device = display->core_pointer;
      
      break;
      
    case MotionNotify:
      GDK_NOTE (EVENTS,
		g_message ("motion notify:\t\twindow: %ld  x,y: %d %d  hint: %s", 
			   xevent->xmotion.window,
			   xevent->xmotion.x, xevent->xmotion.y,
			   (xevent->xmotion.is_hint) ? "true" : "false"));
      
      if (window_private == NULL ||
	  ((window_private->extension_events != 0) &&
           display_x11->input_ignore_core))
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
      event->motion.device = display->core_pointer;
      
      break;
      
    case EnterNotify:
      GDK_NOTE (EVENTS,
		g_message ("enter notify:\t\twindow: %ld  detail: %d subwin: %ld",
			   xevent->xcrossing.window,
			   xevent->xcrossing.detail,
			   xevent->xcrossing.subwindow));
 
      if (window_private == NULL)
        {
          return_val = FALSE;
          break;
        }
      
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
	event->crossing.subwindow = gdk_window_lookup_for_display (display, xevent->xcrossing.subwindow);
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

      if (window_private == NULL)
        {
          return_val = FALSE;
          break;
        }
      
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
	event->crossing.subwindow = gdk_window_lookup_for_display (display, xevent->xcrossing.subwindow);
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
      
      if (window_private == NULL)
        {
          return_val = FALSE;
          break;
        }
      
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
 
        if (window_private == NULL)
          {
            return_val = FALSE;
            break;
          }
        
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
      if (_gdk_debug_flags & GDK_DEBUG_EVENTS)
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
      
      if (window_private == NULL)
        {
          return_val = FALSE;
          break;
        }
      
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

      /* Ignore DestroyNotify from SubstructureNotifyMask */
      if (xevent->xdestroywindow.window == xevent->xdestroywindow.event)
	{
	  event->any.type = GDK_DESTROY;
	  event->any.window = window;
	  
	  return_val = window_private && !GDK_WINDOW_DESTROYED (window);
	  
	  if (window && GDK_WINDOW_XID (window) != screen_x11->xroot_window)
	    gdk_window_destroy_notify (window);
	}
      else
	return_val = FALSE;
      
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
      if (window)
	{
	  if (GDK_WINDOW_IS_MAPPED (window))
	    gdk_synthesize_window_state (window,
					 0,
					 GDK_WINDOW_STATE_ICONIFIED);

	  _gdk_xgrab_check_unmap (window, xevent->xany.serial);
	}
      
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
			   : xevent->xconfigure.event != xevent->xconfigure.window
			   ? " (discarding substructure)"
			   : ""));
      if (window &&
	  xevent->xconfigure.event == xevent->xconfigure.window &&
	  !GDK_WINDOW_DESTROYED (window) &&
	  (window_private->extension_events != 0))
	_gdk_input_configure_event (&xevent->xconfigure, window);

      if (!window ||
	  xevent->xconfigure.event != xevent->xconfigure.window ||
          GDK_WINDOW_TYPE (window) == GDK_WINDOW_CHILD ||
          GDK_WINDOW_TYPE (window) == GDK_WINDOW_ROOT)
	return_val = FALSE;
      else
	{
	  event->configure.type = GDK_CONFIGURE;
	  event->configure.window = window;
	  event->configure.width = xevent->xconfigure.width;
	  event->configure.height = xevent->xconfigure.height;
	  
	  if (!xevent->xconfigure.send_event && 
	      !GDK_WINDOW_DESTROYED (window))
	    {
	      gint tx = 0;
	      gint ty = 0;
	      Window child_window = 0;

	      gdk_error_trap_push ();
	      if (XTranslateCoordinates (GDK_DRAWABLE_XDISPLAY (window),
					 GDK_DRAWABLE_XID (window),
					 screen_x11->xroot_window,
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

	      if (window_private->resize_count == 0)
		_gdk_moveresize_configure_done (display, window);
	    }
	}
      break;
      
    case PropertyNotify:
      GDK_NOTE (EVENTS,
		g_message ("property notify:\twindow: %ld, atom(%ld): %s%s%s",
			   xevent->xproperty.window,
			   xevent->xproperty.atom,
			   "\"",
			   gdk_x11_get_xatom_name_for_display (display, xevent->xproperty.atom),
			   "\""));

      if (window_private == NULL)
        {
	  return_val = FALSE;
          break;
        }
      
      if (xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE") ||
	  xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_DESKTOP"))
        {
          /* If window state changed, then synthesize those events. */
          gdk_check_wm_state_changed (window);
        }
      
      if (window_private->event_mask & GDK_PROPERTY_CHANGE_MASK) 
	{
	  event->property.type = GDK_PROPERTY_NOTIFY;
	  event->property.window = window;
	  event->property.atom = gdk_x11_xatom_to_atom_for_display (display, xevent->xproperty.atom);
	  event->property.time = xevent->xproperty.time;
	  event->property.state = xevent->xproperty.state;
	}
      else
	return_val = FALSE;

      break;
      
    case SelectionClear:
      GDK_NOTE (EVENTS,
		g_message ("selection clear:\twindow: %ld",
			   xevent->xproperty.window));

      if (_gdk_selection_filter_clear_event (&xevent->xselectionclear))
	{
	  event->selection.type = GDK_SELECTION_CLEAR;
	  event->selection.window = window;
	  event->selection.selection = gdk_x11_xatom_to_atom_for_display (display, xevent->xselectionclear.selection);
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
      event->selection.selection = gdk_x11_xatom_to_atom_for_display (display, xevent->xselectionrequest.selection);
      event->selection.target = gdk_x11_xatom_to_atom_for_display (display, xevent->xselectionrequest.target);
      event->selection.property = gdk_x11_xatom_to_atom_for_display (display, xevent->xselectionrequest.property);
      event->selection.requestor = xevent->xselectionrequest.requestor;
      event->selection.time = xevent->xselectionrequest.time;
      
      break;
      
    case SelectionNotify:
      GDK_NOTE (EVENTS,
		g_message ("selection notify:\twindow: %ld",
			   xevent->xproperty.window));
      
      
      event->selection.type = GDK_SELECTION_NOTIFY;
      event->selection.window = window;
      event->selection.selection = gdk_x11_xatom_to_atom_for_display (display, xevent->xselection.selection);
      event->selection.target = gdk_x11_xatom_to_atom_for_display (display, xevent->xselection.target);
      event->selection.property = gdk_x11_xatom_to_atom_for_display (display, xevent->xselection.property);
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
	GdkAtom message_type = gdk_x11_xatom_to_atom_for_display (display, xevent->xclient.message_type);

	GDK_NOTE (EVENTS,
		  g_message ("client message:\twindow: %ld",
			     xevent->xclient.window));
	
	tmp_list = display_x11->client_filters;
	while (tmp_list)
	  {
	    GdkClientFilter *filter = tmp_list->data;
	    if (filter->type == message_type)
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
            if (window_private == NULL)
              {
                return_val = FALSE;
              }
            else
              {
                event->client.type = GDK_CLIENT_EVENT;
                event->client.window = window;
                event->client.message_type = message_type;
                event->client.data_format = xevent->xclient.format;
                memcpy(&event->client.data, &xevent->xclient.data,
                       sizeof(event->client.data));
              }
            break;
          }
      }
      
      break;
      
    case MappingNotify:
      GDK_NOTE (EVENTS,
		g_message ("mapping notify"));
      
      /* Let XLib know that there is a new keyboard mapping.
       */
      XRefreshKeyboardMapping (&xevent->xmapping);
      ++display_x11->keymap_serial;
      return_val = FALSE;
      break;

    default:
#ifdef HAVE_XKB
      if (xevent->type == display_x11->xkb_event_type)
	{
	  XkbEvent *xkb_event = (XkbEvent *)xevent;

	  switch (xkb_event->any.xkb_type)
	    {
	    case XkbMapNotify:
	      ++display_x11->keymap_serial;

	      return_val = FALSE;
	      break;
	      
	    case XkbStateNotify:
	      _gdk_keymap_state_changed (display);
	      break;
	    }
	}
      else
#endif
	{
	  /* something else - (e.g., a Xinput event) */
	  
	  if (window_private &&
	      !GDK_WINDOW_DESTROYED (window_private) &&
	      (window_private->extension_events != 0))
	    return_val = _gdk_input_other_event(event, xevent, window);
	  else
	    return_val = FALSE;
	  
	  break;
	}
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

static GdkFilterReturn
gdk_wm_protocols_filter (GdkXEvent *xev,
			 GdkEvent  *event,
			 gpointer data)
{
  XEvent *xevent = (XEvent *)xev;
  GdkWindow *win = event->any.window;
  GdkDisplay *display = GDK_WINDOW_DISPLAY (win);

  if ((Atom) xevent->xclient.data.l[0] == gdk_x11_get_xatom_by_name_for_display (display, "WM_DELETE_WINDOW"))
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
  else if ((Atom) xevent->xclient.data.l[0] == gdk_x11_get_xatom_by_name_for_display (display, "WM_TAKE_FOCUS"))
    {
      GdkWindow *win = event->any.window;
      Window focus_win = GDK_WINDOW_IMPL_X11(((GdkWindowObject *)win)->impl)->focus_window;

      /* There is no way of knowing reliably whether we are viewable so we need
       * to trap errors so we don't cause a BadMatch.
       */
      gdk_error_trap_push ();
      XSetInputFocus (GDK_WINDOW_XDISPLAY (win),
		      focus_win,
		      RevertToParent,
		      xevent->xclient.data.l[1]);
      XSync (GDK_WINDOW_XDISPLAY (win), False);
      gdk_error_trap_pop ();
    }
  else if ((Atom) xevent->xclient.data.l[0] == gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_PING"))
    {
      XEvent xev = *xevent;
      
      xev.xclient.window = GDK_WINDOW_XROOTWIN (win);
      XSendEvent (GDK_WINDOW_XDISPLAY (win), 
		  xev.xclient.window,
		  False, 
		  SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    }

  return GDK_FILTER_REMOVE;
}

void
_gdk_events_queue (GdkDisplay *display)
{
  GList *node;
  GdkEvent *event;
  XEvent xevent;
  Display *xdisplay = GDK_DISPLAY_XDISPLAY (display);

  while (!_gdk_event_queue_find_first(display) && XPending (xdisplay))
    {
      XNextEvent (xdisplay, &xevent);

      switch (xevent.type)
	{
	case KeyPress:
	case KeyRelease:
	  break;
	default:
	  if (XFilterEvent (&xevent, None))
	    continue;
	}
      
      event = _gdk_event_new ();
      
      event->any.type = GDK_NOTHING;
      event->any.window = NULL;
      event->any.send_event = xevent.xany.send_event ? TRUE : FALSE;

      ((GdkEventPrivate *)event)->flags |= GDK_EVENT_PENDING;

      node = _gdk_event_queue_append (display, event);

      if (gdk_event_translate (display, event, &xevent, FALSE))
	{
	  ((GdkEventPrivate *)event)->flags &= ~GDK_EVENT_PENDING;
	}
      else
	{
	  _gdk_event_queue_remove_link (display, node);
	  g_list_free_1 (node);
	  gdk_event_free (event);
	}
    }
}

static gboolean  
gdk_event_prepare (GSource  *source,
		   gint     *timeout)
{
  GdkDisplay *display = ((GdkDisplaySource*)source)->display;
  gboolean retval;
  
  GDK_THREADS_ENTER ();

  *timeout = -1;
  retval = (_gdk_event_queue_find_first (display) != NULL || 
	    gdk_check_xpending (display));
  
  GDK_THREADS_LEAVE ();

  return retval;
}

static gboolean  
gdk_event_check (GSource *source) 
{
  GdkDisplaySource *display_source = (GdkDisplaySource*)source;
  gboolean retval;

  GDK_THREADS_ENTER ();

  if (display_source->event_poll_fd.revents & G_IO_IN)
    retval = (_gdk_event_queue_find_first (display_source->display) != NULL || 
	      gdk_check_xpending (display_source->display));
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
  GdkDisplay *display = ((GdkDisplaySource*)source)->display;
  GdkEvent *event;
 
  GDK_THREADS_ENTER ();

  _gdk_events_queue (display);
  event = _gdk_event_unqueue (display);

  if (event)
    {
      if (_gdk_event_func)
	(*_gdk_event_func) (event, _gdk_event_data);
      
      gdk_event_free (event);
    }
  
  GDK_THREADS_LEAVE ();

  return TRUE;
}

/**
 * gdk_event_send_client_message_for_display :
 * @display : the #GdkDisplay for the window where the message is to be sent.
 * @event : the #GdkEvent to send, which should be a #GdkEventClient.
 * @xid : the X window to send the X ClientMessage event to.
 *
 * Sends an X ClientMessage event to a given window.
 *
 * This could be used for communicating between different applications,
 * though the amount of data is limited to 20 bytes.
 *
 * Returns : non-zero on success.
 */
gboolean
gdk_event_send_client_message_for_display (GdkDisplay *display,
					   GdkEvent *event,
					   guint32 xid)
{
  XEvent sev;
  
  g_return_val_if_fail(event != NULL, FALSE);

  /* Set up our event to send, with the exception of its target window */
  sev.xclient.type = ClientMessage;
  sev.xclient.display = GDK_DISPLAY_XDISPLAY (display);
  sev.xclient.format = event->client.data_format;
  sev.xclient.window = xid;
  memcpy(&sev.xclient.data, &event->client.data, sizeof (sev.xclient.data));
  sev.xclient.message_type = gdk_x11_atom_to_xatom_for_display (display, event->client.message_type);
  
  return _gdk_send_xevent (display, xid, False, NoEventMask, &sev);
}



/* Sends a ClientMessage to all toplevel client windows */
gboolean
gdk_event_send_client_message_to_all_recurse (GdkDisplay *display,
					      XEvent     *xev, 
					      guint32     xid,
					      guint       level)
{
  Atom type = None;
  int format;
  unsigned long nitems, after;
  unsigned char *data;
  Window *ret_children, ret_root, ret_parent;
  unsigned int ret_nchildren;
  gboolean send = FALSE;
  gboolean found = FALSE;
  gboolean result = FALSE;
  int i;
  
  gdk_error_trap_push ();
  
  if (XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), xid, 
			  gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_STATE"),
			  0, 0, False, AnyPropertyType,
			  &type, &format, &nitems, &after, &data) != Success)
    goto out;
  
  if (type)
    {
      send = TRUE;
      XFree (data);
    }
  else
    {
      /* OK, we're all set, now let's find some windows to send this to */
      if (!XQueryTree (GDK_DISPLAY_XDISPLAY (display), xid,
		      &ret_root, &ret_parent,
		      &ret_children, &ret_nchildren))	
	goto out;

      for(i = 0; i < ret_nchildren; i++)
	if (gdk_event_send_client_message_to_all_recurse (display, xev, ret_children[i], level + 1))
	  found = TRUE;

      XFree (ret_children);
    }

  if (send || (!found && (level == 1)))
    {
      xev->xclient.window = xid;
      _gdk_send_xevent (display, xid, False, NoEventMask, xev);
    }

  result = send || found;

 out:
  gdk_error_trap_pop ();

  return result;
}

/**
 * gdk_screen_broadcast_client_message:
 * @screen : the #GdkScreen where the event will be broadcasted.
 * @event : the #GdkEvent.
 *
 * Sends an X ClientMessage event to all toplevel windows on @screen.
 *
 * Toplevel windows are determined by checking for the WM_STATE property, 
 * as described in the Inter-Client Communication Conventions Manual (ICCCM).
 * If no windows are found with the WM_STATE property set, the message is 
 * sent to all children of the root window.
 */

void
gdk_screen_broadcast_client_message (GdkScreen *screen, 
				     GdkEvent  *event)
{
  XEvent sev;
  GdkWindow *root_window;

  g_return_if_fail (event != NULL);
  
  root_window = gdk_screen_get_root_window (screen);
  
  /* Set up our event to send, with the exception of its target window */
  sev.xclient.type = ClientMessage;
  sev.xclient.display = GDK_WINDOW_XDISPLAY (root_window);
  sev.xclient.format = event->client.data_format;
  memcpy(&sev.xclient.data, &event->client.data, sizeof (sev.xclient.data));
  sev.xclient.message_type = 
    gdk_x11_atom_to_xatom_for_display (GDK_WINDOW_DISPLAY (root_window),
				       event->client.message_type);

  gdk_event_send_client_message_to_all_recurse (gdk_screen_get_display (screen),
						&sev, 
						GDK_WINDOW_XID (root_window), 
						0);
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
  GSList *tmp_list = _gdk_displays;
  
  while (tmp_list)
    {
      XSync (GDK_DISPLAY_XDISPLAY (tmp_list->data), False);
      tmp_list = tmp_list->next;
    }
}

static Bool
timestamp_predicate (Display *display,
		     XEvent  *xevent,
		     XPointer arg)
{
  Window xwindow = GPOINTER_TO_UINT (arg);
  GdkDisplay *gdk_display = gdk_x11_lookup_xdisplay (display);

  if (xevent->type == PropertyNotify &&
      xevent->xproperty.window == xwindow &&
      xevent->xproperty.atom == gdk_x11_get_xatom_by_name_for_display 
      (gdk_display, "GDK_TIMESTAMP_PROP"))
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
  Atom timestamp_prop_atom;

  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  g_return_val_if_fail (!GDK_WINDOW_DESTROYED (window), 0);

  xdisplay = GDK_WINDOW_XDISPLAY (window);
  xwindow = GDK_WINDOW_XWINDOW (window);
  timestamp_prop_atom = 
    gdk_x11_get_xatom_by_name_for_display (GDK_WINDOW_DISPLAY (window),
					   "GDK_TIMESTAMP_PROP");
  
  XChangeProperty (xdisplay, xwindow, timestamp_prop_atom,
		   timestamp_prop_atom,
		   8, PropModeReplace, &c, 1);

  XIfEvent (xdisplay, &xevent,
	    timestamp_predicate, GUINT_TO_POINTER(xwindow));

  return xevent.xproperty.time;
}

typedef struct _NetWmSupportedAtoms NetWmSupportedAtoms;

struct _NetWmSupportedAtoms
{
  Atom *atoms;
  gulong n_atoms;
};

/**
 * gdk_x11_screen_supports_net_wm_hint:
 * @screen : the relevant #GdkScreen.
 * @property: a property atom.
 * 
 * This function is specific to the X11 backend of GDK, and indicates
 * whether the window manager supports a certain hint from the
 * Extended Window Manager Hints Specification. You can find this
 * specification on http://www.freedesktop.org.
 *
 * When using this function, keep in mind that the window manager
 * can change over time; so you shouldn't use this function in
 * a way that impacts persistent application state. A common bug
 * is that your application can start up before the window manager
 * does when the user logs in, and before the window manager starts
 * gdk_x11_screen_supports_net_wm_hint() will return %FALSE for every property.
 * 
 * Return value: %TRUE if the window manager supports @property
 **/
gboolean
gdk_x11_screen_supports_net_wm_hint (GdkScreen *screen,
				     GdkAtom    property)
{
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  Window *xwindow;
  gulong i;
  GdkScreenX11 *screen_x11;
  NetWmSupportedAtoms *supported_atoms;
  GdkDisplay *display;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);
  
  screen_x11 = GDK_SCREEN_X11 (screen);
  display = screen_x11->display;

  supported_atoms = g_object_get_data (G_OBJECT (screen), "gdk-net-wm-supported-atoms");
  if (!supported_atoms)
    {
      supported_atoms = g_new0 (NetWmSupportedAtoms, 1);
      g_object_set_data (G_OBJECT (screen), "gdk-net-wm-supported-atoms", supported_atoms);
    }

  if (screen_x11->wmspec_check_window != None)
    {
      if (supported_atoms->atoms == NULL)
        return FALSE;
      
      i = 0;
      while (i < supported_atoms->n_atoms)
        {
          if (supported_atoms->atoms[i] == gdk_x11_atom_to_xatom_for_display (display, property))
            return TRUE;
          
          ++i;
        }
      
      return FALSE;
    }

  if (supported_atoms->atoms)
    XFree (supported_atoms->atoms);

  supported_atoms->atoms = NULL;
  supported_atoms->n_atoms = 0;
  
  /* This function is very slow on every call if you are not running a
   * spec-supporting WM. For now not optimized, because it isn't in
   * any critical code paths, but if you used it somewhere that had to
   * be fast you want to avoid "GTK is slow with old WMs" complaints.
   * Probably at that point the function should be changed to query
   * _NET_SUPPORTING_WM_CHECK only once every 10 seconds or something.
   */
  
  XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), screen_x11->xroot_window,
		      gdk_x11_get_xatom_by_name_for_display (display, "_NET_SUPPORTING_WM_CHECK"),
		      0, G_MAXLONG, False, XA_WINDOW, &type, &format, 
		      &nitems, &bytes_after, (guchar **) & xwindow);
  
  if (type != XA_WINDOW)
    return FALSE;

  gdk_error_trap_push ();

  /* Find out if this WM goes away, so we can reset everything. */
  XSelectInput (screen_x11->xdisplay, *xwindow, StructureNotifyMask);

  gdk_display_sync (screen_x11->display);
  
  if (gdk_error_trap_pop ())
    {
      XFree (xwindow);
      return FALSE;
    }
  
  XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), screen_x11->xroot_window,
		      gdk_x11_get_xatom_by_name_for_display (display, "_NET_SUPPORTED"),
		      0, G_MAXLONG, False, XA_ATOM, &type, &format, 
		      &supported_atoms->n_atoms, &bytes_after,
		      (guchar **)&supported_atoms->atoms);
  
  if (type != XA_ATOM)
    return FALSE;
  
  screen_x11->wmspec_check_window = *xwindow;
  XFree (xwindow);
  
  /* since wmspec_check_window != None this isn't infinite. ;-) */
  return gdk_x11_screen_supports_net_wm_hint (screen, property);
}

/**
 * gdk_net_wm_supports:
 * @property: a property atom.
 * 
 * This function is specific to the X11 backend of GDK, and indicates
 * whether the window manager for the default screen supports a certain
 * hint from the Extended Window Manager Hints Specification. See
 * gdk_x11_screen_supports_net_wm_hint() for complete details.
 * 
 * Return value: %TRUE if the window manager supports @property
 **/
gboolean
gdk_net_wm_supports (GdkAtom property)
{
  return gdk_x11_screen_supports_net_wm_hint (gdk_get_default_screen (), property);
}

static struct
{
  const char *xsettings_name;
  const char *gdk_name;
} settings_map[] = {
  { "Net/DoubleClickTime", "gtk-double-click-time" },
  { "Net/DndDragThreshold", "gtk-dnd-drag-threshold" },
  { "Gtk/CanChangeAccels", "gtk-can-change-accels" },
  { "Gtk/ColorPalette", "gtk-color-palette" },
  { "Gtk/FontName", "gtk-font-name" },
  { "Gtk/KeyThemeName", "gtk-key-theme-name" },
  { "Gtk/ToolbarStyle", "gtk-toolbar-style" },
  { "Gtk/ToolbarIconSize", "gtk-toolbar-icon-size" },
  { "Net/CursorBlink", "gtk-cursor-blink" },
  { "Net/CursorBlinkTime", "gtk-cursor-blink-time" },
  { "Net/ThemeName", "gtk-theme-name" }
};

static void
gdk_xsettings_notify_cb (const char       *name,
			 XSettingsAction   action,
			 XSettingsSetting *setting,
			 void             *data)
{
  GdkEvent new_event;
  GdkScreen *screen = data;
  int i;

  new_event.type = GDK_SETTING;
  new_event.setting.window = gdk_screen_get_root_window (screen);
  new_event.setting.send_event = FALSE;
  new_event.setting.name = NULL;

  for (i = 0; i < G_N_ELEMENTS (settings_map) ; i++)
    if (strcmp (settings_map[i].xsettings_name, name) == 0)
      {
	new_event.setting.name = (char *)settings_map[i].gdk_name;
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

/**
 * gdk_screen_get_setting:
 * @screen: the #GdkScreen where the setting is located
 * @name: the name of the setting
 * @value: location to store the value of the setting
 *
 * Retrieves a desktop-wide setting such as double-click time
 * for the #GdkScreen @screen. 
 *
 * FIXME needs a list of valid settings here, or a link to 
 * more information.
 * 
 * Returns : %TRUE if the setting existed and a value was stored
 *   in @value, %FALSE otherwise.
 **/
gboolean
gdk_screen_get_setting (GdkScreen   *screen,
			const gchar *name,
			GValue      *value)
{

  const char *xsettings_name = NULL;
  XSettingsResult result;
  XSettingsSetting *setting;
  GdkScreenX11 *screen_x11;
  gboolean success = FALSE;
  gint i;
  GValue tmp_val = { 0, };
  
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);
  
  screen_x11 = GDK_SCREEN_X11 (screen);

  for (i = 0; i < G_N_ELEMENTS (settings_map) ; i++)
    if (strcmp (settings_map[i].gdk_name, name) == 0)
      {
	xsettings_name = settings_map[i].xsettings_name;
	break;
      }

  if (!xsettings_name)
    return FALSE;

  result = xsettings_client_get_setting (screen_x11->xsettings_client, 
					 xsettings_name, &setting);
  if (result != XSETTINGS_SUCCESS)
    return FALSE;

  switch (setting->type)
    {
    case XSETTINGS_TYPE_INT:
      if (check_transform (xsettings_name, G_TYPE_INT, G_VALUE_TYPE (value)))
	{
	  g_value_init (&tmp_val, G_TYPE_INT);
	  g_value_set_int (&tmp_val, setting->data.v_int);
	  g_value_transform (&tmp_val, value);

	  success = TRUE;
	}
      break;
    case XSETTINGS_TYPE_STRING:
      if (check_transform (xsettings_name, G_TYPE_STRING, G_VALUE_TYPE (value)))
	{
	  g_value_init (&tmp_val, G_TYPE_STRING);
	  g_value_set_string (&tmp_val, setting->data.v_string);
	  g_value_transform (&tmp_val, value);

	  success = TRUE;
	}
      break;
    case XSETTINGS_TYPE_COLOR:
      if (!check_transform (xsettings_name, GDK_TYPE_COLOR, G_VALUE_TYPE (value)))
	{
	  GdkColor color;
	  
	  g_value_init (&tmp_val, GDK_TYPE_COLOR);

	  color.pixel = 0;
	  color.red = setting->data.v_color.red;
	  color.green = setting->data.v_color.green;
	  color.blue = setting->data.v_color.blue;
	  
	  g_value_set_boxed (&tmp_val, &color);
	  
	  g_value_transform (&tmp_val, value);
	  
	  success = TRUE;
	}
      break;
    }
  
  g_value_unset (&tmp_val);

  xsettings_setting_free (setting);

  return success;
}

static GdkFilterReturn 
gdk_xsettings_client_event_filter (GdkXEvent *xevent,
				   GdkEvent  *event,
				   gpointer   data)
{
  GdkScreenX11 *screen = data;
  
  if (xsettings_client_process_event (screen->xsettings_client, (XEvent *)xevent))
    return GDK_FILTER_REMOVE;
  else
    return GDK_FILTER_CONTINUE;
}

static void 
gdk_xsettings_watch_cb (Window   window,
			 Bool	  is_start,
			 long     mask,
			 void    *cb_data)
{
  GdkWindow *gdkwin;
  GdkScreen *screen = cb_data;

  gdkwin = gdk_window_lookup_for_display (gdk_screen_get_display (screen), window);
  
  if (is_start)
    gdk_window_add_filter (gdkwin, gdk_xsettings_client_event_filter, screen);
  else
    gdk_window_remove_filter (gdkwin, gdk_xsettings_client_event_filter, screen);
}
