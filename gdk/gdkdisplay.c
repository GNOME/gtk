/* GDK - The GIMP Drawing Kit
 * gdkdisplay.c
 * 
 * Copyright 2001 Sun Microsystems Inc. 
 *
 * Erwann Chenede <erwann.chenede@sun.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <glib.h>
#include "gdk.h"		/* gdk_event_send_client_message() */
#include "gdkdisplay.h"
#include "gdkinternals.h"
#include "gdkmarshalers.h"
#include "gdkscreen.h"
#include "gdkalias.h"

enum {
  CLOSED,
  LAST_SIGNAL
};

static void gdk_display_dispose    (GObject         *object);
static void gdk_display_finalize   (GObject         *object);


static void       singlehead_get_pointer (GdkDisplay       *display,
					  GdkScreen       **screen,
					  gint             *x,
					  gint             *y,
					  GdkModifierType  *mask);
static GdkWindow* singlehead_window_get_pointer (GdkDisplay       *display,
						 GdkWindow        *window,
						 gint             *x,
						 gint             *y,
						 GdkModifierType  *mask);
static GdkWindow* singlehead_window_at_pointer  (GdkDisplay       *display,
						 gint             *win_x,
						 gint             *win_y);

static GdkWindow* singlehead_default_window_get_pointer (GdkWindow       *window,
							 gint            *x,
							 gint            *y,
							 GdkModifierType *mask);
static GdkWindow* singlehead_default_window_at_pointer  (GdkScreen       *screen,
							 gint            *win_x,
							 gint            *win_y);

static guint signals[LAST_SIGNAL] = { 0 };

static char *gdk_sm_client_id;

static const GdkDisplayPointerHooks default_pointer_hooks = {
  _gdk_windowing_get_pointer,
  _gdk_windowing_window_get_pointer,
  _gdk_windowing_window_at_pointer
};

static const GdkDisplayPointerHooks singlehead_pointer_hooks = {
  singlehead_get_pointer,
  singlehead_window_get_pointer,
  singlehead_window_at_pointer
};

static const GdkPointerHooks singlehead_default_pointer_hooks = {
  singlehead_default_window_get_pointer,
  singlehead_default_window_at_pointer
};

static const GdkPointerHooks *singlehead_current_pointer_hooks = &singlehead_default_pointer_hooks;

G_DEFINE_TYPE (GdkDisplay, gdk_display, G_TYPE_OBJECT)

static void
gdk_display_class_init (GdkDisplayClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  
  object_class->finalize = gdk_display_finalize;
  object_class->dispose = gdk_display_dispose;

  /**
   * GdkDisplay::closed:
   * @display: the object on which the signal is emitted
   * @is_error: %TRUE if the display was closed due to an error
   *
   * The ::closed signal is emitted when the connection to the windowing
   * system for @display is closed.
   *
   * Since: 2.2
   */   
  signals[CLOSED] =
    g_signal_new (g_intern_static_string ("closed"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GdkDisplayClass, closed),
		  NULL, NULL,
		  gdk_marshal_VOID__BOOLEAN,
		  G_TYPE_NONE,
		  1,
		  G_TYPE_BOOLEAN);
}

static void
gdk_display_init (GdkDisplay *display)
{
  _gdk_displays = g_slist_prepend (_gdk_displays, display);

  display->button_click_time[0] = display->button_click_time[1] = 0;
  display->button_window[0] = display->button_window[1] = NULL;
  display->button_number[0] = display->button_number[1] = -1;
  display->button_x[0] = display->button_x[1] = 0;
  display->button_y[0] = display->button_y[1] = 0;

  display->double_click_time = 250;
  display->double_click_distance = 5;

  display->pointer_hooks = &default_pointer_hooks;
}

static void
gdk_display_dispose (GObject *object)
{
  GdkDisplay *display = GDK_DISPLAY_OBJECT (object);

  g_list_foreach (display->queued_events, (GFunc)gdk_event_free, NULL);
  g_list_free (display->queued_events);
  display->queued_events = NULL;
  display->queued_tail = NULL;

  _gdk_displays = g_slist_remove (_gdk_displays, object);

  if (gdk_display_get_default() == display)
    {
      if (_gdk_displays)
        gdk_display_manager_set_default_display (gdk_display_manager_get(),
                                                 _gdk_displays->data);
      else
        gdk_display_manager_set_default_display (gdk_display_manager_get(),
                                                 NULL);
    }

  G_OBJECT_CLASS (gdk_display_parent_class)->dispose (object);
}

static void
gdk_display_finalize (GObject *object)
{
  G_OBJECT_CLASS (gdk_display_parent_class)->finalize (object);
}

/**
 * gdk_display_close:
 * @display: a #GdkDisplay
 *
 * Closes the connection to the windowing system for the given display,
 * and cleans up associated resources.
 *
 * Since: 2.2
 */
void
gdk_display_close (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (!display->closed)
    {
      display->closed = TRUE;
      
      g_signal_emit (display, signals[CLOSED], 0, FALSE);
      g_object_run_dispose (G_OBJECT (display));
      
      g_object_unref (display);
    }
}

/**
 * gdk_display_get_event:
 * @display: a #GdkDisplay
 * 
 * Gets the next #GdkEvent to be processed for @display, fetching events from the
 * windowing system if necessary.
 * 
 * Return value: the next #GdkEvent to be processed, or %NULL if no events
 * are pending. The returned #GdkEvent should be freed with gdk_event_free().
 *
 * Since: 2.2
 **/
GdkEvent*
gdk_display_get_event (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  
  _gdk_events_queue (display);
  return _gdk_event_unqueue (display);
}

/**
 * gdk_display_peek_event:
 * @display: a #GdkDisplay 
 * 
 * Gets a copy of the first #GdkEvent in the @display's event queue, without
 * removing the event from the queue.  (Note that this function will
 * not get more events from the windowing system.  It only checks the events
 * that have already been moved to the GDK event queue.)
 * 
 * Return value: a copy of the first #GdkEvent on the event queue, or %NULL 
 * if no events are in the queue. The returned #GdkEvent should be freed with
 * gdk_event_free().
 *
 * Since: 2.2
 **/
GdkEvent*
gdk_display_peek_event (GdkDisplay *display)
{
  GList *tmp_list;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  tmp_list = _gdk_event_queue_find_first (display);
  
  if (tmp_list)
    return gdk_event_copy (tmp_list->data);
  else
    return NULL;
}

/**
 * gdk_display_put_event:
 * @display: a #GdkDisplay
 * @event: a #GdkEvent.
 *
 * Appends a copy of the given event onto the front of the event
 * queue for @display.
 *
 * Since: 2.2
 **/
void
gdk_display_put_event (GdkDisplay *display,
		       GdkEvent   *event)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  g_return_if_fail (event != NULL);

  _gdk_event_queue_append (display, gdk_event_copy (event));
}

/**
 * gdk_pointer_ungrab:
 * @time_: a timestamp from a #GdkEvent, or %GDK_CURRENT_TIME if no 
 *  timestamp is available.
 *
 * Ungrabs the pointer, if it is grabbed by this application.
 **/
void
gdk_pointer_ungrab (guint32 time)
{
  gdk_display_pointer_ungrab (gdk_display_get_default (), time);
}

/**
 * gdk_pointer_is_grabbed:
 * 
 * Returns %TRUE if the pointer is currently grabbed by this application.
 *
 * Note that this does not take the inmplicit pointer grab on button
 * presses into account.

 * Return value: %TRUE if the pointer is currently grabbed by this application.* 
 **/
gboolean
gdk_pointer_is_grabbed (void)
{
  return gdk_display_pointer_is_grabbed (gdk_display_get_default ());
}

/**
 * gdk_keyboard_ungrab:
 * @time_: a timestamp from a #GdkEvent, or %GDK_CURRENT_TIME if no
 *        timestamp is available.
 * 
 * Ungrabs the keyboard, if it is grabbed by this application.
 **/
void
gdk_keyboard_ungrab (guint32 time)
{
  gdk_display_keyboard_ungrab (gdk_display_get_default (), time);
}

/**
 * gdk_beep:
 * 
 * Emits a short beep on the default display.
 **/
void
gdk_beep (void)
{
  gdk_display_beep (gdk_display_get_default ());
}

/**
 * gdk_event_send_client_message:
 * @event: the #GdkEvent to send, which should be a #GdkEventClient.
 * @winid:  the window to send the X ClientMessage event to.
 * 
 * Sends an X ClientMessage event to a given window (which must be
 * on the default #GdkDisplay.)
 * This could be used for communicating between different applications,
 * though the amount of data is limited to 20 bytes.
 * 
 * Return value: non-zero on success.
 **/
gboolean
gdk_event_send_client_message (GdkEvent        *event,
			       GdkNativeWindow  winid)
{
  g_return_val_if_fail (event != NULL, FALSE);

  return gdk_event_send_client_message_for_display (gdk_display_get_default (),
						    event, winid);
}

/**
 * gdk_event_send_clientmessage_toall:
 * @event: the #GdkEvent to send, which should be a #GdkEventClient.
 *
 * Sends an X ClientMessage event to all toplevel windows on the default
 * #GdkScreen.
 *
 * Toplevel windows are determined by checking for the WM_STATE property, as
 * described in the Inter-Client Communication Conventions Manual (ICCCM).
 * If no windows are found with the WM_STATE property set, the message is sent
 * to all children of the root window.
 **/
void
gdk_event_send_clientmessage_toall (GdkEvent *event)
{
  g_return_if_fail (event != NULL);

  gdk_screen_broadcast_client_message (gdk_screen_get_default (), event);
}

/**
 * gdk_device_get_core_pointer:
 * 
 * Returns the core pointer device for the default display.
 * 
 * Return value: the core pointer device; this is owned by the
 *   display and should not be freed.
 **/
GdkDevice *
gdk_device_get_core_pointer (void)
{
  return gdk_display_get_core_pointer (gdk_display_get_default ());
}

/**
 * gdk_display_get_core_pointer:
 * @display: a #GdkDisplay
 * 
 * Returns the core pointer device for the given display
 * 
 * Return value: the core pointer device; this is owned by the
 *   display and should not be freed.
 *
 * Since: 2.2
 **/
GdkDevice *
gdk_display_get_core_pointer (GdkDisplay *display)
{
  return display->core_pointer;
}

/**
 * gdk_set_sm_client_id:
 * @sm_client_id: the client id assigned by the session manager when the
 *    connection was opened, or %NULL to remove the property.
 * 
 * Sets the <literal>SM_CLIENT_ID</literal> property on the application's leader window so that
 * the window manager can save the application's state using the X11R6 ICCCM
 * session management protocol.
 *
 * See the X Session Management Library documentation for more information on
 * session management and the Inter-Client Communication Conventions Manual
 * (ICCCM) for information on the <literal>WM_CLIENT_LEADER</literal> property. 
 * (Both documents are part of the X Window System distribution.)
 **/
void
gdk_set_sm_client_id (const gchar* sm_client_id)
{
  GSList *displays, *tmp_list;
  
  g_free (gdk_sm_client_id);
  gdk_sm_client_id = g_strdup (sm_client_id);

  displays = gdk_display_manager_list_displays (gdk_display_manager_get ());
  for (tmp_list = displays; tmp_list; tmp_list = tmp_list->next)
    _gdk_windowing_display_set_sm_client_id (tmp_list->data, sm_client_id);

  g_slist_free (displays);
}

/**
 * _gdk_get_sm_client_id:
 * 
 * Gets the client ID set with gdk_set_sm_client_id(), if any.
 * 
 * Return value: Session ID, or %NULL if gdk_set_sm_client_id()
 *               has never been called.
 **/
const char *
_gdk_get_sm_client_id (void)
{
  return gdk_sm_client_id;
}

/**
 * gdk_display_get_pointer:
 * @display: a #GdkDisplay
 * @screen: location to store the screen that the
 *          cursor is on, or %NULL.
 * @x: location to store root window X coordinate of pointer, or %NULL.
 * @y: location to store root window Y coordinate of pointer, or %NULL.
 * @mask: location to store current modifier mask, or %NULL
 * 
 * Gets the current location of the pointer and the current modifier
 * mask for a given display.
 *
 * Since: 2.2
 **/
void
gdk_display_get_pointer (GdkDisplay      *display,
			 GdkScreen      **screen,
			 gint            *x,
			 gint            *y,
			 GdkModifierType *mask)
{
  GdkScreen *tmp_screen;
  gint tmp_x, tmp_y;
  GdkModifierType tmp_mask;
  
  g_return_if_fail (GDK_IS_DISPLAY (display));

  display->pointer_hooks->get_pointer (display, &tmp_screen, &tmp_x, &tmp_y, &tmp_mask);

  if (screen)
    *screen = tmp_screen;
  if (x)
    *x = tmp_x;
  if (y)
    *y = tmp_y;
  if (mask)
    *mask = tmp_mask;
}

/**
 * gdk_display_get_window_at_pointer:
 * @display: a #GdkDisplay
 * @win_x: return location for origin of the window under the pointer
 * @win_y: return location for origin of the window under the pointer
 * 
 * Obtains the window underneath the mouse pointer, returning the location
 * of that window in @win_x, @win_y for @screen. Returns %NULL if the window 
 * under the mouse pointer is not known to GDK (for example, belongs to
 * another application).
 * 
 * Returns: the window under the mouse pointer, or %NULL
 *
 * Since: 2.2
 **/
GdkWindow *
gdk_display_get_window_at_pointer (GdkDisplay *display,
				   gint       *win_x,
				   gint       *win_y)
{
  gint tmp_x, tmp_y;
  GdkWindow *window;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  window = display->pointer_hooks->window_at_pointer (display, &tmp_x, &tmp_y);

  if (win_x)
    *win_x = tmp_x;
  if (win_y)
    *win_y = tmp_y;

  return window;
}

/**
 * gdk_display_set_pointer_hooks:
 * @display: a #GdkDisplay
 * @new_hooks: a table of pointers to functions for getting
 *   quantities related to the current pointer position,
 *   or %NULL to restore the default table.
 * 
 * This function allows for hooking into the operation
 * of getting the current location of the pointer on a particular
 * display. This is only useful for such low-level tools as an
 * event recorder. Applications should never have any
 * reason to use this facility.
 *
 * Return value: the previous pointer hook table
 *
 * Since: 2.2
 **/
GdkDisplayPointerHooks *
gdk_display_set_pointer_hooks (GdkDisplay                   *display,
			       const GdkDisplayPointerHooks *new_hooks)
{
  const GdkDisplayPointerHooks *result;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  result = display->pointer_hooks;

  if (new_hooks)
    display->pointer_hooks = new_hooks;
  else
    display->pointer_hooks = &default_pointer_hooks;

  return (GdkDisplayPointerHooks *)result;
}

static void
singlehead_get_pointer (GdkDisplay       *display,
			GdkScreen       **screen,
			gint             *x,
			gint             *y,
			GdkModifierType  *mask)
{
  GdkScreen *default_screen = gdk_display_get_default_screen (display);
  GdkWindow *root_window = gdk_screen_get_root_window (default_screen);

  *screen = default_screen;

  singlehead_current_pointer_hooks->get_pointer (root_window, x, y, mask);
}

static GdkWindow*
singlehead_window_get_pointer (GdkDisplay       *display,
			       GdkWindow        *window,
			       gint             *x,
			       gint             *y,
			       GdkModifierType  *mask)
{
  return singlehead_current_pointer_hooks->get_pointer (window, x, y, mask);
}

static GdkWindow*
singlehead_window_at_pointer   (GdkDisplay *display,
				gint       *win_x,
				gint       *win_y)
{
  GdkScreen *default_screen = gdk_display_get_default_screen (display);

  return singlehead_current_pointer_hooks->window_at_pointer (default_screen,
							      win_x, win_y);
}

static GdkWindow*
singlehead_default_window_get_pointer (GdkWindow       *window,
				       gint            *x,
				       gint            *y,
				       GdkModifierType *mask)
{
  return _gdk_windowing_window_get_pointer (gdk_drawable_get_display (window),
					    window, x, y, mask);
}

static GdkWindow*
singlehead_default_window_at_pointer  (GdkScreen       *screen,
				       gint            *win_x,
				       gint            *win_y)
{
  return _gdk_windowing_window_at_pointer (gdk_screen_get_display (screen),
					   win_x, win_y);
}

/**
 * gdk_set_pointer_hooks:
 * @new_hooks: a table of pointers to functions for getting
 *   quantities related to the current pointer position,
 *   or %NULL to restore the default table.
 * 
 * This function allows for hooking into the operation
 * of getting the current location of the pointer. This
 * is only useful for such low-level tools as an
 * event recorder. Applications should never have any
 * reason to use this facility.
 *
 * This function is not multihead safe. For multihead operation,
 * see gdk_display_set_pointer_hooks().
 * 
 * Return value: the previous pointer hook table
 **/
GdkPointerHooks *
gdk_set_pointer_hooks (const GdkPointerHooks *new_hooks)
{
  const GdkPointerHooks *result = singlehead_current_pointer_hooks;

  if (new_hooks)
    singlehead_current_pointer_hooks = new_hooks;
  else
    singlehead_current_pointer_hooks = &singlehead_default_pointer_hooks;

  gdk_display_set_pointer_hooks (gdk_display_get_default (),
				 &singlehead_pointer_hooks);
  
  return (GdkPointerHooks *)result;
}

#define __GDK_DISPLAY_C__
#include "gdkaliasdef.c"
