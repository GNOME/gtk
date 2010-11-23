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

#include "config.h"

#include "gdkdisplay.h"

#include "gdkevents.h"
#include "gdkwindowimpl.h"
#include "gdkinternals.h"
#include "gdkmarshalers.h"
#include "gdkscreen.h"

#include <glib.h>
#include <math.h>


/**
 * SECTION:gdkdisplay
 * @Short_description: Controls the keyboard/mouse pointer grabs and a set of <type>GdkScreen</type>s
 * @Title: GdkDisplay
 *
 * #GdkDisplay objects purpose are two fold:
 * <itemizedlist>
 * <listitem><para>
 *   To grab/ungrab keyboard focus and mouse pointer
 * </para></listitem>
 * <listitem><para>
 *   To manage and provide information about the #GdkScreen(s)
 *   available for this #GdkDisplay
 * </para></listitem>
 * </itemizedlist>
 *
 * #GdkDisplay objects are the GDK representation of the X Display which can be
 * described as <emphasis>a workstation consisting of a keyboard a pointing
 * device (such as a mouse) and one or more screens</emphasis>.
 * It is used to open and keep track of various #GdkScreen objects currently
 * instanciated by the application. It is also used to grab and release the keyboard
 * and the mouse pointer.
 */


enum {
  OPENED,
  CLOSED,
  LAST_SIGNAL
};

static void gdk_display_dispose     (GObject         *object);
static void gdk_display_finalize    (GObject         *object);

static void        multihead_get_device_state           (GdkDisplay       *display,
                                                         GdkDevice        *device,
                                                         GdkScreen       **screen,
                                                         gint             *x,
                                                         gint             *y,
                                                         GdkModifierType  *mask);
static GdkWindow * multihead_window_get_device_position (GdkDisplay       *display,
                                                         GdkDevice        *device,
                                                         GdkWindow        *window,
                                                         gint             *x,
                                                         gint             *y,
                                                         GdkModifierType  *mask);
static GdkWindow * multihead_window_at_device_position  (GdkDisplay       *display,
                                                         GdkDevice        *device,
                                                         gint             *win_x,
                                                         gint             *win_y);

static void        multihead_default_get_pointer        (GdkDisplay       *display,
                                                         GdkScreen       **screen,
                                                         gint             *x,
                                                         gint             *y,
                                                         GdkModifierType  *mask);
static GdkWindow * multihead_default_window_get_pointer (GdkDisplay      *display,
                                                         GdkWindow       *window,
                                                         gint            *x,
                                                         gint            *y,
                                                         GdkModifierType *mask);
static GdkWindow * multihead_default_window_at_pointer  (GdkDisplay      *display,
                                                         gint            *win_x,
                                                         gint            *win_y);


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
static GdkWindow *gdk_window_real_window_get_device_position     (GdkDisplay       *display,
                                                                  GdkDevice        *device,
                                                                  GdkWindow        *window,
                                                                  gint             *x,
                                                                  gint             *y,
                                                                  GdkModifierType  *mask);
static GdkWindow *gdk_display_real_get_window_at_device_position (GdkDisplay       *display,
                                                                  GdkDevice        *device,
                                                                  gint             *win_x,
                                                                  gint             *win_y);

static guint signals[LAST_SIGNAL] = { 0 };

static char *gdk_sm_client_id;

static const GdkDisplayDeviceHooks default_device_hooks = {
  _gdk_windowing_get_device_state,
  gdk_window_real_window_get_device_position,
  gdk_display_real_get_window_at_device_position
};

static const GdkDisplayDeviceHooks multihead_pointer_hooks = {
  multihead_get_device_state,
  multihead_window_get_device_position,
  multihead_window_at_device_position
};

static const GdkDisplayPointerHooks multihead_default_pointer_hooks = {
  multihead_default_get_pointer,
  multihead_default_window_get_pointer,
  multihead_default_window_at_pointer
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
static const GdkDisplayPointerHooks *multihead_current_pointer_hooks = &multihead_default_pointer_hooks;

G_DEFINE_TYPE (GdkDisplay, gdk_display, G_TYPE_OBJECT)

static void
gdk_display_class_init (GdkDisplayClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gdk_display_finalize;
  object_class->dispose = gdk_display_dispose;

  /**
   * GdkDisplay::opened:
   * @display: the object on which the signal is emitted
   *
   * The ::opened signal is emitted when the connection to the windowing
   * system for @display is opened.
   */
  signals[OPENED] =
    g_signal_new (g_intern_static_string ("opened"),
		  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

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
		  _gdk_marshal_VOID__BOOLEAN,
		  G_TYPE_NONE,
		  1,
		  G_TYPE_BOOLEAN);
}

static void
free_pointer_info (GdkPointerWindowInfo *info)
{
  g_object_unref (info->toplevel_under_pointer);
  g_slice_free (GdkPointerWindowInfo, info);
}

static void
free_device_grab (GdkDeviceGrabInfo *info)
{
  g_object_unref (info->window);
  g_object_unref (info->native_window);
  g_free (info);
}

static gboolean
free_device_grabs_foreach (gpointer key,
                           gpointer value,
                           gpointer user_data)
{
  GList *list = value;

  g_list_foreach (list, (GFunc) free_device_grab, NULL);
  g_list_free (list);

  return TRUE;
}

static void
device_removed_cb (GdkDeviceManager *device_manager,
                   GdkDevice        *device,
                   GdkDisplay       *display)
{
  g_hash_table_remove (display->multiple_click_info, device);
  g_hash_table_remove (display->device_grabs, device);
  g_hash_table_remove (display->pointers_info, device);

  /* FIXME: change core pointer and remove from device list */
}

static void
gdk_display_opened (GdkDisplay *display)
{
  GdkDeviceManager *device_manager;

  device_manager = gdk_display_get_device_manager (display);

  g_signal_connect (device_manager, "device-removed",
                    G_CALLBACK (device_removed_cb), display);
}

static void
gdk_display_init (GdkDisplay *display)
{
  _gdk_displays = g_slist_prepend (_gdk_displays, display);

  display->double_click_time = 250;
  display->double_click_distance = 5;

  display->device_hooks = &default_device_hooks;

  display->device_grabs = g_hash_table_new (NULL, NULL);
  display->motion_hint_info = g_hash_table_new_full (NULL, NULL, NULL,
                                                     (GDestroyNotify) g_free);

  display->pointers_info = g_hash_table_new_full (NULL, NULL, NULL,
                                                  (GDestroyNotify) free_pointer_info);

  display->multiple_click_info = g_hash_table_new_full (NULL, NULL, NULL,
                                                        (GDestroyNotify) g_free);

  g_signal_connect (display, "opened",
                    G_CALLBACK (gdk_display_opened), NULL);
}

static void
gdk_display_dispose (GObject *object)
{
  GdkDisplay *display = GDK_DISPLAY_OBJECT (object);
  GdkDeviceManager *device_manager;

  device_manager = gdk_display_get_device_manager (GDK_DISPLAY_OBJECT (object));

  g_list_foreach (display->queued_events, (GFunc)gdk_event_free, NULL);
  g_list_free (display->queued_events);
  display->queued_events = NULL;
  display->queued_tail = NULL;

  _gdk_displays = g_slist_remove (_gdk_displays, object);

  if (gdk_display_get_default () == display)
    {
      if (_gdk_displays)
        gdk_display_manager_set_default_display (gdk_display_manager_get(),
                                                 _gdk_displays->data);
      else
        gdk_display_manager_set_default_display (gdk_display_manager_get(),
                                                 NULL);
    }

  if (device_manager)
    {
      /* this is to make it drop devices which may require using the X
       * display and therefore can't be cleaned up in finalize.
       * It will also disconnect device_removed_cb
       */
      g_object_run_dispose (G_OBJECT (display->device_manager));
    }

  G_OBJECT_CLASS (gdk_display_parent_class)->dispose (object);
}

static void
gdk_display_finalize (GObject *object)
{
  GdkDisplay *display = GDK_DISPLAY_OBJECT (object);

  g_hash_table_foreach_remove (display->device_grabs,
                               free_device_grabs_foreach,
                               NULL);
  g_hash_table_destroy (display->device_grabs);

  g_hash_table_destroy (display->pointers_info);
  g_hash_table_destroy (display->multiple_click_info);

  if (display->device_manager)
    g_object_unref (display->device_manager);

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
 * gdk_display_is_closed:
 * @display: a #GdkDisplay
 *
 * Finds out if the display has been closed.
 *
 * Returns: %TRUE if the display is closed.
 *
 * Since: 2.22
 */
gboolean
gdk_display_is_closed  (GdkDisplay  *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  return display->closed;
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
gdk_display_put_event (GdkDisplay     *display,
		       const GdkEvent *event)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
  g_return_if_fail (event != NULL);

  _gdk_event_queue_append (display, gdk_event_copy (event));
  /* If the main loop is blocking in a different thread, wake it up */
  g_main_context_wakeup (NULL); 
}

/**
 * gdk_display_pointer_ungrab:
 * @display: a #GdkDisplay.
 * @time_: a timestap (e.g. %GDK_CURRENT_TIME).
 *
 * Release any pointer grab.
 *
 * Since: 2.2
 *
 * Deprecated: 3.0: Use gdk_device_ungrab(), together with gdk_device_grab()
 *             instead.
 */
void
gdk_display_pointer_ungrab (GdkDisplay *display,
			    guint32     time_)
{
  GdkDeviceManager *device_manager;
  GList *devices, *dev;
  GdkDevice *device;

  g_return_if_fail (GDK_IS_DISPLAY (display));

  device_manager = gdk_display_get_device_manager (display);
  devices = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);

  /* FIXME: Should this be generic to all backends? */
  /* FIXME: What happens with extended devices? */
  for (dev = devices; dev; dev = dev->next)
    {
      device = dev->data;

      if (gdk_device_get_source (device) != GDK_SOURCE_MOUSE)
        continue;

      gdk_device_ungrab (device, time_);
    }

  g_list_free (devices);
}

/**
 * gdk_pointer_ungrab:
 * @time_: a timestamp from a #GdkEvent, or %GDK_CURRENT_TIME if no 
 *  timestamp is available.
 *
 * Ungrabs the pointer on the default display, if it is grabbed by this 
 * application.
 *
 * Deprecated: 3.0: Use gdk_device_ungrab(), together with gdk_device_grab()
 *             instead.
 **/
void
gdk_pointer_ungrab (guint32 time)
{
  gdk_display_pointer_ungrab (gdk_display_get_default (), time);
}

/**
 * gdk_pointer_is_grabbed:
 * 
 * Returns %TRUE if the pointer on the default display is currently 
 * grabbed by this application.
 *
 * Note that this does not take the inmplicit pointer grab on button
 * presses into account.
 *
 * Return value: %TRUE if the pointer is currently grabbed by this application.
 *
 * Deprecated: 3.0: Use gdk_display_device_is_grabbed() instead.
 **/
gboolean
gdk_pointer_is_grabbed (void)
{
  return gdk_display_pointer_is_grabbed (gdk_display_get_default ());
}

/**
 * gdk_display_keyboard_ungrab:
 * @display: a #GdkDisplay.
 * @time_: a timestap (e.g #GDK_CURRENT_TIME).
 *
 * Release any keyboard grab
 *
 * Since: 2.2
 *
 * Deprecated: 3.0: Use gdk_device_ungrab(), together with gdk_device_grab()
 *             instead.
 */
void
gdk_display_keyboard_ungrab (GdkDisplay *display,
			     guint32     time)
{
  GdkDeviceManager *device_manager;
  GList *devices, *dev;
  GdkDevice *device;

  g_return_if_fail (GDK_IS_DISPLAY (display));

  device_manager = gdk_display_get_device_manager (display);
  devices = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);

  /* FIXME: Should this be generic to all backends? */
  /* FIXME: What happens with extended devices? */
  for (dev = devices; dev; dev = dev->next)
    {
      device = dev->data;

      if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
        continue;

      gdk_device_ungrab (device, time);
    }

  g_list_free (devices);
}

/**
 * gdk_keyboard_ungrab:
 * @time_: a timestamp from a #GdkEvent, or %GDK_CURRENT_TIME if no
 *        timestamp is available.
 * 
 * Ungrabs the keyboard on the default display, if it is grabbed by this 
 * application.
 *
 * Deprecated: 3.0: Use gdk_device_ungrab(), together with gdk_device_grab()
 *             instead.
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
 *
 * Deprecated: 3.0: Use gdk_device_manager_get_client_pointer() instead, or
 *             gdk_event_get_device() if a #GdkEvent with pointer device
 *             information is available.
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
 *
 * Deprecated: 3.0: Use gdk_device_manager_get_client_pointer() instead, or
 *             gdk_event_get_device() if a #GdkEvent with device
 *             information is available.
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

void
_gdk_display_enable_motion_hints (GdkDisplay *display,
                                  GdkDevice  *device)
{
  gulong *device_serial, serial;

  device_serial = g_hash_table_lookup (display->motion_hint_info, device);

  if (!device_serial)
    {
      device_serial = g_new0 (gulong, 1);
      *device_serial = G_MAXULONG;
      g_hash_table_insert (display->motion_hint_info, device, device_serial);
    }

  if (*device_serial != 0)
    {
      serial = _gdk_windowing_window_get_next_serial (display);
      /* We might not actually generate the next request, so
	 make sure this triggers always, this may cause it to
	 trigger slightly too early, but this is just a hint
	 anyway. */
      if (serial > 0)
	serial--;
      if (serial < *device_serial)
	*device_serial = serial;
    }
}

/**
 * gdk_display_get_device_state:
 * @display: a #GdkDisplay.
 * @device: device to query status to.
 * @screen: location to store the #GdkScreen the @device is on, or %NULL.
 * @x: location to store root window X coordinate of @device, or %NULL.
 * @y: location to store root window Y coordinate of @device, or %NULL.
 * @mask: location to store current modifier mask for @device, or %NULL.
 *
 * Gets the current location and state of @device for a given display.
 *
 * Since: 3.0
 **/
void
gdk_display_get_device_state (GdkDisplay       *display,
                              GdkDevice        *device,
                              GdkScreen       **screen,
                              gint             *x,
                              gint             *y,
                              GdkModifierType  *mask)
{
  GdkScreen *tmp_screen;
  gint tmp_x, tmp_y;
  GdkModifierType tmp_mask;

  g_return_if_fail (GDK_IS_DISPLAY (display));
  g_return_if_fail (GDK_IS_DEVICE (device));

  display->device_hooks->get_device_state (display, device, &tmp_screen, &tmp_x, &tmp_y, &tmp_mask);

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
 * gdk_display_get_window_at_device_position:
 * @display: a #GdkDisplay.
 * @device: #GdkDevice to query info to.
 * @win_x: return location for the X coordinate of the device location, relative to the window origin, or %NULL.
 * @win_y: return location for the Y coordinate of the device location, relative to the window origin, or %NULL.
 *
 * Obtains the window underneath @device, returning the location of the device in @win_x and @win_y. Returns
 * %NULL if the window tree under @device is not known to GDK (for example, belongs to another application).
 *
 * Returns: the #GdkWindow under the device position, or %NULL.
 *
 * Since: 3.0
 **/
GdkWindow *
gdk_display_get_window_at_device_position (GdkDisplay *display,
                                           GdkDevice  *device,
                                           gint       *win_x,
                                           gint       *win_y)
{
  gint tmp_x, tmp_y;
  GdkWindow *window;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  window = display->device_hooks->window_at_device_position (display, device, &tmp_x, &tmp_y);

  if (win_x)
    *win_x = tmp_x;
  if (win_y)
    *win_y = tmp_y;

  return window;
}

/**
 * gdk_display_set_device_hooks:
 * @display: a #GdkDisplay.
 * @new_hooks: a table of pointers to functions for getting quantities related to all
 *             devices position, or %NULL to restore the default table.
 *
 * This function allows for hooking into the operation of getting the current location of any
 * #GdkDevice on a particular #GdkDisplay. This is only useful for such low-level tools as
 * an event recorder. Applications should never have any reason to use this facility.
 *
 * Returns: The previous device hook table.
 *
 * Since: 3.0
 **/
GdkDisplayDeviceHooks *
gdk_display_set_device_hooks (GdkDisplay                  *display,
                              const GdkDisplayDeviceHooks *new_hooks)
{
  const GdkDisplayDeviceHooks *result;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  result = display->device_hooks;

  if (new_hooks)
    display->device_hooks = new_hooks;
  else
    display->device_hooks = &default_device_hooks;

  return (GdkDisplayDeviceHooks *) result;
}

/**
 * gdk_display_get_pointer:
 * @display: a #GdkDisplay
 * @screen: (allow-none): location to store the screen that the
 *          cursor is on, or %NULL.
 * @x: (out) (allow-none): location to store root window X coordinate of pointer, or %NULL.
 * @y: (out) (allow-none): location to store root window Y coordinate of pointer, or %NULL.
 * @mask: (out) (allow-none): location to store current modifier mask, or %NULL
 *
 * Gets the current location of the pointer and the current modifier
 * mask for a given display.
 *
 * Since: 2.2
 *
 * Deprecated: 3.0: Use gdk_display_get_device_state() instead.
 **/
void
gdk_display_get_pointer (GdkDisplay      *display,
			 GdkScreen      **screen,
			 gint            *x,
			 gint            *y,
			 GdkModifierType *mask)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  gdk_display_get_device_state (display, display->core_pointer, screen, x, y, mask);
}

static GdkWindow *
gdk_display_real_get_window_at_device_position (GdkDisplay *display,
                                                GdkDevice  *device,
                                                gint       *win_x,
                                                gint       *win_y)
{
  GdkWindow *window;
  gint x, y;

  window = _gdk_windowing_window_at_device_position (display, device, &x, &y, NULL, FALSE);

  /* This might need corrections, as the native window returned
     may contain client side children */
  if (window)
    {
      double xx, yy;

      window = _gdk_window_find_descendant_at (window,
					       x, y,
					       &xx, &yy);
      x = floor (xx + 0.5);
      y = floor (yy + 0.5);
    }

  *win_x = x;
  *win_y = y;

  return window;
}

static GdkWindow *
gdk_window_real_window_get_device_position (GdkDisplay       *display,
                                            GdkDevice        *device,
                                            GdkWindow        *window,
                                            gint             *x,
                                            gint             *y,
                                            GdkModifierType  *mask)
{
  GdkWindowObject *private;
  gint tmpx, tmpy;
  GdkModifierType tmp_mask;
  gboolean normal_child;

  private = (GdkWindowObject *) window;

  normal_child = GDK_WINDOW_IMPL_GET_IFACE (private->impl)->get_device_state (window,
                                                                              device,
                                                                              &tmpx, &tmpy,
                                                                              &tmp_mask);
  /* We got the coords on the impl, convert to the window */
  tmpx -= private->abs_x;
  tmpy -= private->abs_y;

  if (x)
    *x = tmpx;
  if (y)
    *y = tmpy;
  if (mask)
    *mask = tmp_mask;

  if (normal_child)
    return _gdk_window_find_child_at (window, tmpx, tmpy);
  return NULL;
}

/**
 * gdk_display_get_window_at_pointer:
 * @display: a #GdkDisplay
 * @win_x: (out) (allow-none): return location for x coordinate of the pointer location relative
 *    to the window origin, or %NULL
 * @win_y: (out) (allow-none): return location for y coordinate of the pointer location relative
 &    to the window origin, or %NULL
 *
 * Obtains the window underneath the mouse pointer, returning the location
 * of the pointer in that window in @win_x, @win_y for @screen. Returns %NULL
 * if the window under the mouse pointer is not known to GDK (for example, 
 * belongs to another application).
 *
 * Returns: (transfer none): the window under the mouse pointer, or %NULL
 *
 * Since: 2.2
 *
 * Deprecated: 3.0: Use gdk_display_get_window_at_device_position() instead.
 **/
GdkWindow *
gdk_display_get_window_at_pointer (GdkDisplay *display,
				   gint       *win_x,
				   gint       *win_y)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return gdk_display_get_window_at_device_position (display, display->core_pointer, win_x, win_y);
}

static void
multihead_get_device_state (GdkDisplay       *display,
                            GdkDevice        *device,
                            GdkScreen       **screen,
                            gint             *x,
                            gint             *y,
                            GdkModifierType  *mask)
{
  multihead_current_pointer_hooks->get_pointer (display, screen, x, y, mask);
}

static GdkWindow *
multihead_window_get_device_position (GdkDisplay      *display,
                                      GdkDevice       *device,
                                      GdkWindow       *window,
                                      gint            *x,
                                      gint            *y,
                                      GdkModifierType *mask)
{
  return multihead_current_pointer_hooks->window_get_pointer (display, window, x, y, mask);
}

static GdkWindow *
multihead_window_at_device_position (GdkDisplay *display,
                                     GdkDevice  *device,
                                     gint       *win_x,
                                     gint       *win_y)
{
  return multihead_current_pointer_hooks->window_at_pointer (display, win_x, win_y);
}

static void
multihead_default_get_pointer (GdkDisplay       *display,
                               GdkScreen       **screen,
                               gint             *x,
                               gint             *y,
                               GdkModifierType  *mask)
{
  return _gdk_windowing_get_device_state (display,
                                          display->core_pointer,
                                          screen, x, y, mask);
}

static GdkWindow *
multihead_default_window_get_pointer (GdkDisplay      *display,
                                      GdkWindow       *window,
                                      gint            *x,
                                      gint            *y,
                                      GdkModifierType *mask)
{
  return gdk_window_real_window_get_device_position (display,
                                                     display->core_pointer,
                                                     window, x, y, mask);
}

static GdkWindow *
multihead_default_window_at_pointer (GdkDisplay *display,
                                     gint       *win_x,
                                     gint       *win_y)
{
  return gdk_display_real_get_window_at_device_position (display,
                                                         display->core_pointer,
                                                         win_x, win_y);
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
 *
 * Deprecated: 3.0: Use gdk_display_set_device_hooks() instead.
 **/
GdkDisplayPointerHooks *
gdk_display_set_pointer_hooks (GdkDisplay                   *display,
			       const GdkDisplayPointerHooks *new_hooks)
{
  const GdkDisplayPointerHooks *result = multihead_current_pointer_hooks;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  if (new_hooks)
    multihead_current_pointer_hooks = new_hooks;
  else
    multihead_current_pointer_hooks = &multihead_default_pointer_hooks;

  gdk_display_set_device_hooks (display, &multihead_pointer_hooks);

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
  GdkDisplay *display;

  display = gdk_window_get_display (window);

  return gdk_window_real_window_get_device_position (display,
                                                     display->core_pointer,
                                                     window, x, y, mask);
}

static GdkWindow*
singlehead_default_window_at_pointer  (GdkScreen       *screen,
				       gint            *win_x,
				       gint            *win_y)
{
  GdkDisplay *display;

  display = gdk_screen_get_display (screen);

  return gdk_display_real_get_window_at_device_position (display,
                                                         display->core_pointer,
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
 *
 * Deprecated: 3.0: Use gdk_display_set_device_hooks() instead.
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

static void
generate_grab_broken_event (GdkWindow *window,
                            GdkDevice *device,
			    gboolean   implicit,
			    GdkWindow *grab_window)
{
  g_return_if_fail (window != NULL);

  if (!GDK_WINDOW_DESTROYED (window))
    {
      GdkEvent *event;

      event = gdk_event_new (GDK_GRAB_BROKEN);
      event->grab_broken.window = g_object_ref (window);
      event->grab_broken.send_event = FALSE;
      event->grab_broken.implicit = implicit;
      event->grab_broken.grab_window = grab_window;
      gdk_event_set_device (event, device);
      event->grab_broken.keyboard = (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD) ? TRUE : FALSE;

      gdk_event_put (event);
      gdk_event_free (event);
    }
}

GdkDeviceGrabInfo *
_gdk_display_get_last_device_grab (GdkDisplay *display,
                                   GdkDevice  *device)
{
  GList *l;

  l = g_hash_table_lookup (display->device_grabs, device);

  if (l)
    {
      l = g_list_last (l);
      return l->data;
    }

  return NULL;
}

GdkDeviceGrabInfo *
_gdk_display_add_device_grab (GdkDisplay       *display,
                              GdkDevice        *device,
                              GdkWindow        *window,
                              GdkWindow        *native_window,
                              GdkGrabOwnership  grab_ownership,
                              gboolean          owner_events,
                              GdkEventMask      event_mask,
                              unsigned long     serial_start,
                              guint32           time,
                              gboolean          implicit)
{
  GdkDeviceGrabInfo *info, *other_info;
  GList *grabs, *l;

  info = g_new0 (GdkDeviceGrabInfo, 1);

  info->window = g_object_ref (window);
  info->native_window = g_object_ref (native_window);
  info->serial_start = serial_start;
  info->serial_end = G_MAXULONG;
  info->owner_events = owner_events;
  info->event_mask = event_mask;
  info->time = time;
  info->implicit = implicit;
  info->ownership = grab_ownership;

  grabs = g_hash_table_lookup (display->device_grabs, device);

  /* Find the first grab that has a larger start time (if any) and insert
   * before that. I.E we insert after already existing grabs with same
   * start time */
  for (l = grabs; l != NULL; l = l->next)
    {
      other_info = l->data;

      if (info->serial_start < other_info->serial_start)
	break;
    }

  grabs = g_list_insert_before (grabs, l, info);

  /* Make sure the new grab end before next grab */
  if (l)
    {
      other_info = l->data;
      info->serial_end = other_info->serial_start;
    }

  /* Find any previous grab and update its end time */
  l = g_list_find (grabs, info);
  l = l->prev;
  if (l)
    {
      other_info = l->data;
      other_info->serial_end = serial_start;
    }

  g_hash_table_insert (display->device_grabs, device, grabs);

  return info;
}

/* _gdk_synthesize_crossing_events only works inside one toplevel.
   This function splits things into two calls if needed, converting the
   coordinates to the right toplevel */
static void
synthesize_crossing_events (GdkDisplay      *display,
                            GdkDevice       *device,
			    GdkWindow       *src_window,
			    GdkWindow       *dest_window,
			    GdkCrossingMode  crossing_mode,
			    guint32          time,
			    gulong           serial)
{
  GdkWindow *src_toplevel, *dest_toplevel;
  GdkModifierType state;
  int x, y;

  /* We use the native crossing events if all native */
  if (_gdk_native_windows)
    return;
  
  if (src_window)
    src_toplevel = gdk_window_get_toplevel (src_window);
  else
    src_toplevel = NULL;
  if (dest_window)
    dest_toplevel = gdk_window_get_toplevel (dest_window);
  else
    dest_toplevel = NULL;

  if (src_toplevel == NULL && dest_toplevel == NULL)
    return;
  
  if (src_toplevel == NULL ||
      src_toplevel == dest_toplevel)
    {
      /* Same toplevels */
      gdk_window_get_pointer (dest_toplevel,
			      &x, &y, &state);
      _gdk_synthesize_crossing_events (display,
				       src_window,
				       dest_window,
                                       device,
				       crossing_mode,
				       x, y, state,
				       time,
				       NULL,
				       serial, FALSE);
    }
  else if (dest_toplevel == NULL)
    {
      gdk_window_get_pointer (src_toplevel,
			      &x, &y, &state);
      _gdk_synthesize_crossing_events (display,
                                       src_window,
                                       NULL,
                                       device,
                                       crossing_mode,
                                       x, y, state,
                                       time,
                                       NULL,
                                       serial, FALSE);
    }
  else
    {
      /* Different toplevels */
      gdk_window_get_pointer (src_toplevel,
			      &x, &y, &state);
      _gdk_synthesize_crossing_events (display,
				       src_window,
				       NULL,
                                       device,
				       crossing_mode,
				       x, y, state,
				       time,
				       NULL,
				       serial, FALSE);
      gdk_window_get_pointer (dest_toplevel,
			      &x, &y, &state);
      _gdk_synthesize_crossing_events (display,
				       NULL,
				       dest_window,
                                       device,
				       crossing_mode,
				       x, y, state,
				       time,
				       NULL,
				       serial, FALSE);
    }
}

static GdkWindow *
get_current_toplevel (GdkDisplay      *display,
                      GdkDevice       *device,
                      int             *x_out,
                      int             *y_out,
		      GdkModifierType *state_out)
{
  GdkWindow *pointer_window;
  int x, y;
  GdkModifierType state;

  pointer_window = _gdk_windowing_window_at_device_position (display, device, &x, &y, &state, TRUE);

  if (pointer_window != NULL &&
      (GDK_WINDOW_DESTROYED (pointer_window) ||
       GDK_WINDOW_TYPE (pointer_window) == GDK_WINDOW_ROOT ||
       GDK_WINDOW_TYPE (pointer_window) == GDK_WINDOW_FOREIGN))
    pointer_window = NULL;

  *x_out = x;
  *y_out = y;
  *state_out = state;
  return pointer_window;
}

static void
switch_to_pointer_grab (GdkDisplay        *display,
                        GdkDevice         *device,
			GdkDeviceGrabInfo *grab,
			GdkDeviceGrabInfo *last_grab,
			guint32            time,
			gulong             serial)
{
  GdkWindow *src_window, *pointer_window, *new_toplevel;
  GdkPointerWindowInfo *info;
  GList *old_grabs;
  GdkModifierType state;
  int x, y;

  /* Temporarily unset pointer to make sure we send the crossing events below */
  old_grabs = g_hash_table_lookup (display->device_grabs, device);
  g_hash_table_steal (display->device_grabs, device);
  info = _gdk_display_get_pointer_info (display, device);

  if (grab)
    {
      /* New grab is in effect */

      /* We need to generate crossing events for the grab.
       * However, there are never any crossing events for implicit grabs
       * TODO: ... Actually, this could happen if the pointer window
       *           doesn't have button mask so a parent gets the event...
       */
      if (!grab->implicit)
	{
	  /* We send GRAB crossing events from the window under the pointer to the
	     grab window. Except if there is an old grab then we start from that */
	  if (last_grab)
	    src_window = last_grab->window;
	  else
	    src_window = info->window_under_pointer;

	  if (src_window != grab->window)
            synthesize_crossing_events (display, device,
                                        src_window, grab->window,
                                        GDK_CROSSING_GRAB, time, serial);

	  /* !owner_event Grabbing a window that we're not inside, current status is
	     now NULL (i.e. outside grabbed window) */
	  if (!grab->owner_events && info->window_under_pointer != grab->window)
	    _gdk_display_set_window_under_pointer (display, device, NULL);
	}

      grab->activated = TRUE;
    }

  if (last_grab)
    {
      new_toplevel = NULL;

      if (grab == NULL /* ungrab */ ||
	  (!last_grab->owner_events && grab->owner_events) /* switched to owner_events */ )
	{
	  /* We force check what window we're in, and update the toplevel_under_pointer info,
	   * as that won't get told of this change with toplevel enter events.
	   */
	  if (info->toplevel_under_pointer)
	    g_object_unref (info->toplevel_under_pointer);
	  info->toplevel_under_pointer = NULL;

	  new_toplevel = get_current_toplevel (display, device, &x, &y, &state);
	  if (new_toplevel)
	    {
	      /* w is now toplevel and x,y in toplevel coords */
	      info->toplevel_under_pointer = g_object_ref (new_toplevel);
	      info->toplevel_x = x;
	      info->toplevel_y = y;
	      info->state = state;
	    }
	}

      if (grab == NULL) /* Ungrabbed, send events */
	{
	  pointer_window = NULL;
	  if (new_toplevel)
	    {
	      /* Find (possibly virtual) child window */
	      pointer_window =
		_gdk_window_find_descendant_at (new_toplevel,
						x, y,
						NULL, NULL);
	    }

	  if (pointer_window != last_grab->window)
            synthesize_crossing_events (display, device,
                                        last_grab->window, pointer_window,
                                        GDK_CROSSING_UNGRAB, time, serial);

	  /* We're now ungrabbed, update the window_under_pointer */
	  _gdk_display_set_window_under_pointer (display, device, pointer_window);
	}
    }

  g_hash_table_insert (display->device_grabs, device, old_grabs);
}

void
_gdk_display_device_grab_update (GdkDisplay *display,
                                 GdkDevice  *device,
                                 gulong      current_serial)
{
  GdkDeviceGrabInfo *current_grab, *next_grab;
  GList *grabs;
  guint32 time;

  time = display->last_event_time;
  grabs = g_hash_table_lookup (display->device_grabs, device);

  while (grabs != NULL)
    {
      current_grab = grabs->data;

      if (current_grab->serial_start > current_serial)
	return; /* Hasn't started yet */

      if (current_grab->serial_end > current_serial)
	{
	  /* This one hasn't ended yet.
	     its the currently active one or scheduled to be active */

	  if (!current_grab->activated)
            {
              if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
                switch_to_pointer_grab (display, device, current_grab, NULL, time, current_serial);
            }

	  break;
	}

      next_grab = NULL;
      if (grabs->next)
	{
	  /* This is the next active grab */
	  next_grab = grabs->next->data;

	  if (next_grab->serial_start > current_serial)
	    next_grab = NULL; /* Actually its not yet active */
	}

      if ((next_grab == NULL && current_grab->implicit_ungrab) ||
	  (next_grab != NULL && current_grab->window != next_grab->window))
	generate_grab_broken_event (GDK_WINDOW (current_grab->window),
                                    device,
				    current_grab->implicit,
				    next_grab? next_grab->window : NULL);

      /* Remove old grab */
      grabs = g_list_delete_link (grabs, grabs);
      g_hash_table_insert (display->device_grabs, device, grabs);

      if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
        switch_to_pointer_grab (display, device,
                                next_grab, current_grab,
                                time, current_serial);

      free_device_grab (current_grab);
    }
}

static GList *
grab_list_find (GList  *grabs,
                gulong  serial)
{
  GdkDeviceGrabInfo *grab;

  while (grabs)
    {
      grab = grabs->data;

      if (serial >= grab->serial_start && serial < grab->serial_end)
	return grabs;

      grabs = grabs->next;
    }

  return NULL;
}

static GList *
find_device_grab (GdkDisplay *display,
                   GdkDevice  *device,
                   gulong      serial)
{
  GList *l;

  l = g_hash_table_lookup (display->device_grabs, device);
  return grab_list_find (l, serial);
}

GdkDeviceGrabInfo *
_gdk_display_has_device_grab (GdkDisplay *display,
                              GdkDevice  *device,
                              gulong      serial)
{
  GList *l;

  l = find_device_grab (display, device, serial);
  if (l)
    return l->data;

  return NULL;
}

/* Returns true if last grab was ended
 * If if_child is non-NULL, end the grab only if the grabbed
 * window is the same as if_child or a descendant of it */
gboolean
_gdk_display_end_device_grab (GdkDisplay *display,
                              GdkDevice  *device,
                              gulong      serial,
                              GdkWindow  *if_child,
                              gboolean    implicit)
{
  GdkDeviceGrabInfo *grab;
  GList *l;

  l = find_device_grab (display, device, serial);

  if (l == NULL)
    return FALSE;

  grab = l->data;
  if (grab &&
      (if_child == NULL ||
       _gdk_window_event_parent_of (if_child, grab->window)))
    {
      grab->serial_end = serial;
      grab->implicit_ungrab = implicit;
      return l->next == NULL;
    }
  
  return FALSE;
}

/* Returns TRUE if device events are not blocked by any grab */
gboolean
_gdk_display_check_grab_ownership (GdkDisplay *display,
                                   GdkDevice  *device,
                                   gulong      serial)
{
  GHashTableIter iter;
  gpointer key, value;
  GdkGrabOwnership higher_ownership, device_ownership;
  gboolean device_is_keyboard;

  g_hash_table_iter_init (&iter, display->device_grabs);
  higher_ownership = device_ownership = GDK_OWNERSHIP_NONE;
  device_is_keyboard = (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GdkDeviceGrabInfo *grab;
      GdkDevice *dev;
      GList *grabs;

      dev = key;
      grabs = value;
      grabs = grab_list_find (grabs, serial);

      if (!grabs)
        continue;

      /* Discard device if it's not of the same type */
      if ((device_is_keyboard && gdk_device_get_source (dev) != GDK_SOURCE_KEYBOARD) ||
          (!device_is_keyboard && gdk_device_get_source (dev) == GDK_SOURCE_KEYBOARD))
        continue;

      grab = grabs->data;

      if (dev == device)
        device_ownership = grab->ownership;
      else
        {
          if (grab->ownership > higher_ownership)
            higher_ownership = grab->ownership;
        }
    }

  if (higher_ownership > device_ownership)
    {
      /* There's a higher priority ownership
       * going on for other device(s)
       */
      return FALSE;
    }

  return TRUE;
}

GdkPointerWindowInfo *
_gdk_display_get_pointer_info (GdkDisplay *display,
                               GdkDevice  *device)
{
  GdkPointerWindowInfo *info;

  if (G_UNLIKELY (!device))
    return NULL;

  info = g_hash_table_lookup (display->pointers_info, device);

  if (G_UNLIKELY (!info))
    {
      info = g_slice_new0 (GdkPointerWindowInfo);
      g_hash_table_insert (display->pointers_info, device, info);
    }

  return info;
}

void
_gdk_display_pointer_info_foreach (GdkDisplay                   *display,
                                   GdkDisplayPointerInfoForeach  func,
                                   gpointer                      user_data)
{
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, display->pointers_info);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GdkPointerWindowInfo *info = value;
      GdkDevice *device = key;

      (func) (display, device, info, user_data);
    }
}

/**
 * gdk_device_grab_info_libgtk_only:
 * @display: the display for which to get the grab information
 * @device: device to get the grab information from
 * @grab_window: location to store current grab window
 * @owner_events: location to store boolean indicating whether
 *   the @owner_events flag to gdk_keyboard_grab() or
 *   gdk_pointer_grab() was %TRUE.
 *
 * Determines information about the current keyboard grab.
 * This is not public API and must not be used by applications.
 *
 * Return value: %TRUE if this application currently has the
 *  keyboard grabbed.
 **/
gboolean
gdk_device_grab_info_libgtk_only (GdkDisplay  *display,
                                  GdkDevice   *device,
                                  GdkWindow  **grab_window,
                                  gboolean    *owner_events)
{
  GdkDeviceGrabInfo *info;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);

  info = _gdk_display_get_last_device_grab (display, device);

  if (info)
    {
      if (grab_window)
        *grab_window = info->window;
      if (owner_events)
        *owner_events = info->owner_events;

      return TRUE;
    }
  else
    return FALSE;
}

/**
 * gdk_display_pointer_is_grabbed:
 * @display: a #GdkDisplay
 *
 * Test if the pointer is grabbed.
 *
 * Returns: %TRUE if an active X pointer grab is in effect
 *
 * Since: 2.2
 *
 * Deprecated: 3.0: Use gdk_display_device_is_grabbed() instead.
 */
gboolean
gdk_display_pointer_is_grabbed (GdkDisplay *display)
{
  GdkDeviceManager *device_manager;
  GList *devices, *dev;
  GdkDevice *device;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), TRUE);

  device_manager = gdk_display_get_device_manager (display);
  devices = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);

  for (dev = devices; dev; dev = dev->next)
    {
      device = dev->data;

      if (gdk_device_get_source (device) == GDK_SOURCE_MOUSE &&
          gdk_display_device_is_grabbed (display, device))
        return TRUE;
    }

  return FALSE;
}

/**
 * gdk_display_device_is_grabbed:
 * @display: a #GdkDisplay
 * @device: a #GdkDevice
 *
 * Returns %TRUE if there is an ongoing grab on @device for @display.
 *
 * Returns: %TRUE if there is a grab in effect for @device.
 **/
gboolean
gdk_display_device_is_grabbed (GdkDisplay *display,
                               GdkDevice  *device)
{
  GdkDeviceGrabInfo *info;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), TRUE);
  g_return_val_if_fail (GDK_IS_DEVICE (device), TRUE);

  /* What we're interested in is the steady state (ie last grab),
     because we're interested e.g. if we grabbed so that we
     can ungrab, even if our grab is not active just yet. */
  info = _gdk_display_get_last_device_grab (display, device);

  return (info && !info->implicit);
}

/**
 * gdk_display_get_device_manager:
 * @display: a #GdkDisplay.
 *
 * Returns the #GdkDeviceManager associated to @display.
 *
 * Returns: A #GdkDeviceManager, or %NULL. This memory is
 *          owned by GDK and must not be freed or unreferenced.
 *
 * Since: 3.0
 **/
GdkDeviceManager *
gdk_display_get_device_manager (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return display->device_manager;
}
