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

#include <glib.h>
#include "gdk.h"		/* gdk_event_send_client_message() */
#include "gdkdisplay.h"
#include "gdkinternals.h"
#include "gdkmarshalers.h"
#include "gdkscreen.h"

enum {
  CLOSED,
  LAST_SIGNAL
};

static void gdk_display_class_init (GdkDisplayClass *class);
static void gdk_display_init       (GdkDisplay      *display);
static void gdk_display_dispose    (GObject         *object);
static void gdk_display_finalize   (GObject         *object);

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class;

GType
gdk_display_get_type (void)
{

  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info = {
	sizeof (GdkDisplayClass),
	(GBaseInitFunc) NULL,
	(GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_display_class_init,
	NULL,			/* class_finalize */
	NULL,			/* class_data */
	sizeof (GdkDisplay),
	0,			/* n_preallocs */
	(GInstanceInitFunc) gdk_display_init
      };
      object_type = g_type_register_static (G_TYPE_OBJECT,
					    "GdkDisplay", &object_info, 0);
    }

  return object_type;
}

static void
gdk_display_class_init (GdkDisplayClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  
  parent_class = g_type_class_peek_parent (class);

  object_class->finalize = gdk_display_finalize;
  object_class->dispose = gdk_display_dispose;

  signals[CLOSED] =
    g_signal_new ("closed",
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

  display->double_click_time = 250;
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
    gdk_display_manager_set_default_display (gdk_display_manager_get(), NULL);
}

static void
gdk_display_finalize (GObject *object)
{
  GdkDisplay *display = GDK_DISPLAY_OBJECT (object);
  
  parent_class->finalize (object);
}

/**
 * gdk_display_close:
 * @display: a #GdkDisplay
 *
 * Closes the connection windowing system for the given display,
 * and cleans up associated resources.
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
      
      g_object_unref (G_OBJECT (display));
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
 * Return value: a copy of the first #GdkEvent on the event queue, or %NULL if no
 * events are in the queue. The returned #GdkEvent should be freed with
 * gdk_event_free().
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
 * @time: a timestamp from a #GdkEvent, or %GDK_CURRENT_TIME if no timestamp is
 *        available.
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
 * @time: a timestamp from a #GdkEvent, or %GDK_CURRENT_TIME if no
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
 * Emits a short beep.
 **/
void
gdk_beep (void)
{
  gdk_display_beep (gdk_display_get_default ());
}

/**
 * gdk_event_send_client_message:
 * @event: the #GdkEvent to send, which should be a #GdkEventClient.
 * @xid:  the window to send the X ClientMessage event to.
 * 
 * Sends an X ClientMessage event to a given window (which must be
 * on the default #GdkDisplay.)
 * This could be used for communicating between different applications,
 * though the amount of data is limited to 20 bytes.
 * 
 * Return value: non-zero on success.
 **/
gboolean
gdk_event_send_client_message (GdkEvent *event, guint32 xid)
{
  g_return_val_if_fail (event != NULL, FALSE);

  return gdk_event_send_client_message_for_display (gdk_display_get_default (),
						    event, xid);
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
  gdk_display_set_sm_client_id (gdk_display_get_default (), sm_client_id);
}

