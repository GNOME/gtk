/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-1999 Tor Lillqvist
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
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.
 */

/*
 * GTK+ DirectFB backend
 * Copyright (C) 2001-2002  convergence integrated media GmbH
 * Copyright (C) 2002-2004  convergence GmbH
 * Written by Denis Oliver Kropp <dok@convergence.de> and
 *            Sven Neumann <sven@convergence.de>
 */

/*
  Main entry point for 2.6 seems to be open_display so
  most stuff in main is moved over to gdkdisplay-directfb.c
  I'll move stub functions here that make no sense for directfb
  and true globals
  Michael Emmel
*/

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include "gdk.h"

#include "gdkdisplay.h"
#include "gdkdirectfb.h"
#include "gdkprivate-directfb.h"

#include "gdkinternals.h"

#include "gdkinput-directfb.h"

#include "gdkintl.h"
#include "gdkalias.h"


void
_gdk_windowing_init (void)
{
  /* Not that usable called before parse_args
   */
}

void
gdk_set_use_xshm (gboolean use_xshm)
{
}

gboolean
gdk_get_use_xshm (void)
{
  return FALSE;
}

void
_gdk_windowing_display_set_sm_client_id (GdkDisplay *display,const gchar *sm_client_id)
{
  g_message ("gdk_set_sm_client_id() is unimplemented.");
}


void
_gdk_windowing_exit (void)
{

  if (_gdk_display->buffer)
    _gdk_display->buffer->Release (_gdk_display->buffer);

  _gdk_directfb_keyboard_exit ();

  if (_gdk_display->keyboard)
    _gdk_display->keyboard->Release (_gdk_display->keyboard);

  _gdk_display->layer->Release (_gdk_display->layer);

  _gdk_display->directfb->Release (_gdk_display->directfb);

  g_free (_gdk_display);
  _gdk_display = NULL;
}

gchar *
gdk_get_display (void)
{
  return g_strdup (gdk_display_get_name (gdk_display_get_default ()));
}


/* utils */
static const guint type_masks[] =
  {
    GDK_STRUCTURE_MASK,        /* GDK_DELETE            =  0, */
    GDK_STRUCTURE_MASK,        /* GDK_DESTROY           =  1, */
    GDK_EXPOSURE_MASK,         /* GDK_EXPOSE            =  2, */
    GDK_POINTER_MOTION_MASK,   /* GDK_MOTION_NOTIFY     =  3, */
    GDK_BUTTON_PRESS_MASK,     /* GDK_BUTTON_PRESS      =  4, */
    GDK_BUTTON_PRESS_MASK,     /* GDK_2BUTTON_PRESS     =  5, */
    GDK_BUTTON_PRESS_MASK,     /* GDK_3BUTTON_PRESS     =  6, */
    GDK_BUTTON_RELEASE_MASK,   /* GDK_BUTTON_RELEASE    =  7, */
    GDK_KEY_PRESS_MASK,        /* GDK_KEY_PRESS         =  8, */
    GDK_KEY_RELEASE_MASK,      /* GDK_KEY_RELEASE       =  9, */
    GDK_ENTER_NOTIFY_MASK,     /* GDK_ENTER_NOTIFY      = 10, */
    GDK_LEAVE_NOTIFY_MASK,     /* GDK_LEAVE_NOTIFY      = 11, */
    GDK_FOCUS_CHANGE_MASK,     /* GDK_FOCUS_CHANGE      = 12, */
    GDK_STRUCTURE_MASK,        /* GDK_CONFIGURE         = 13, */
    GDK_VISIBILITY_NOTIFY_MASK,/* GDK_MAP               = 14, */
    GDK_VISIBILITY_NOTIFY_MASK,/* GDK_UNMAP             = 15, */
    GDK_PROPERTY_CHANGE_MASK,  /* GDK_PROPERTY_NOTIFY   = 16, */
    GDK_PROPERTY_CHANGE_MASK,  /* GDK_SELECTION_CLEAR   = 17, */
    GDK_PROPERTY_CHANGE_MASK,  /* GDK_SELECTION_REQUEST = 18, */
    GDK_PROPERTY_CHANGE_MASK,  /* GDK_SELECTION_NOTIFY  = 19, */
    GDK_PROXIMITY_IN_MASK,     /* GDK_PROXIMITY_IN      = 20, */
    GDK_PROXIMITY_OUT_MASK,    /* GDK_PROXIMITY_OUT     = 21, */
    GDK_ALL_EVENTS_MASK,       /* GDK_DRAG_ENTER        = 22, */
    GDK_ALL_EVENTS_MASK,       /* GDK_DRAG_LEAVE        = 23, */
    GDK_ALL_EVENTS_MASK,       /* GDK_DRAG_MOTION       = 24, */
    GDK_ALL_EVENTS_MASK,       /* GDK_DRAG_STATUS       = 25, */
    GDK_ALL_EVENTS_MASK,       /* GDK_DROP_START        = 26, */
    GDK_ALL_EVENTS_MASK,       /* GDK_DROP_FINISHED     = 27, */
    GDK_ALL_EVENTS_MASK,       /* GDK_CLIENT_EVENT      = 28, */
    GDK_VISIBILITY_NOTIFY_MASK,/* GDK_VISIBILITY_NOTIFY = 29, */
    GDK_EXPOSURE_MASK,         /* GDK_NO_EXPOSE         = 30, */
    GDK_SCROLL_MASK            /* GDK_SCROLL            = 31  */
  };

GdkWindow *
gdk_directfb_other_event_window (GdkWindow    *window,
                                 GdkEventType  type)
{
  guint32    evmask;
  GdkWindow *w;

  w = window;
  while (w != _gdk_parent_root)
    {
      /* Huge hack, so that we don't propagate events to GtkWindow->frame */
      if ((w != window) &&
          (GDK_WINDOW_OBJECT (w)->window_type != GDK_WINDOW_CHILD) &&
          (g_object_get_data (G_OBJECT (w), "gdk-window-child-handler")))
        break;

      evmask = GDK_WINDOW_OBJECT (w)->event_mask;

      if (evmask & type_masks[type])
        return w;

      w = gdk_window_get_parent (w);
    }

  return NULL;
}

GdkWindow *
gdk_directfb_pointer_event_window (GdkWindow    *window,
                                   GdkEventType  type)
{
  guint            evmask;
  GdkModifierType  mask;
  GdkWindow       *w;

  gdk_directfb_mouse_get_info (NULL, NULL, &mask);

  if (_gdk_directfb_pointer_grab_window && !_gdk_directfb_pointer_grab_owner_events )
    {
      evmask = _gdk_directfb_pointer_grab_events;

      if (evmask & (GDK_BUTTON1_MOTION_MASK |
                    GDK_BUTTON2_MOTION_MASK |
                    GDK_BUTTON3_MOTION_MASK))
        {
          if (((mask & GDK_BUTTON1_MASK) &&
               (evmask & GDK_BUTTON1_MOTION_MASK)) ||
              ((mask & GDK_BUTTON2_MASK) &&
               (evmask & GDK_BUTTON2_MOTION_MASK)) ||
              ((mask & GDK_BUTTON3_MASK) &&
               (evmask & GDK_BUTTON3_MOTION_MASK)))
            evmask |= GDK_POINTER_MOTION_MASK;
        }

      if (evmask & type_masks[type]) {

        if (_gdk_directfb_pointer_grab_owner_events) {
          return _gdk_directfb_pointer_grab_window;
        } else {
          GdkWindowObject *obj = GDK_WINDOW_OBJECT (window);
          while (obj != NULL &&
                 obj != GDK_WINDOW_OBJECT (_gdk_directfb_pointer_grab_window)) {
            obj = (GdkWindowObject *)obj->parent;
          }
          if (obj == GDK_WINDOW_OBJECT (_gdk_directfb_pointer_grab_window)) {
            return window;
          } else {
            //was not  child of the grab window so return the grab window
            return _gdk_directfb_pointer_grab_window;
          }
        }
      }
    }

  w = window;
  while (w != _gdk_parent_root)
    {
      /* Huge hack, so that we don't propagate events to GtkWindow->frame */
      if ((w != window) &&
          (GDK_WINDOW_OBJECT (w)->window_type != GDK_WINDOW_CHILD) &&
          (g_object_get_data (G_OBJECT (w), "gdk-window-child-handler")))
        break;

      evmask = GDK_WINDOW_OBJECT (w)->event_mask;

      if (evmask & (GDK_BUTTON1_MOTION_MASK |
                    GDK_BUTTON2_MOTION_MASK |
                    GDK_BUTTON3_MOTION_MASK))
        {
          if (((mask & GDK_BUTTON1_MASK) &&
               (evmask & GDK_BUTTON1_MOTION_MASK)) ||
              ((mask & GDK_BUTTON2_MASK) &&
               (evmask & GDK_BUTTON2_MOTION_MASK)) ||
              ((mask & GDK_BUTTON3_MASK) &&
               (evmask & GDK_BUTTON3_MOTION_MASK)))
            evmask |= GDK_POINTER_MOTION_MASK;
        }

      if (evmask & type_masks[type])
        return w;

      w = gdk_window_get_parent (w);
    }

  return NULL;
}

GdkWindow *
gdk_directfb_keyboard_event_window (GdkWindow    *window,
                                    GdkEventType  type)
{
  guint32    evmask;
  GdkWindow *w;

  if (_gdk_directfb_keyboard_grab_window &&
      !_gdk_directfb_keyboard_grab_owner_events)
    {
      return _gdk_directfb_keyboard_grab_window;
    }

  w = window;
  while (w != _gdk_parent_root)
    {
      /* Huge hack, so that we don't propagate events to GtkWindow->frame */
      if ((w != window) &&
          (GDK_WINDOW_OBJECT (w)->window_type != GDK_WINDOW_CHILD) &&
          (g_object_get_data (G_OBJECT (w), "gdk-window-child-handler")))
        break;

      evmask = GDK_WINDOW_OBJECT (w)->event_mask;

      if (evmask & type_masks[type])
        return w;

      w = gdk_window_get_parent (w);
    }
  return w;
}


void
gdk_directfb_event_fill (GdkEvent     *event,
                         GdkWindow    *window,
                         GdkEventType  type)
{
  guint32 the_time = gdk_directfb_get_time ();

  event->any.type       = type;
  event->any.window     = g_object_ref (window);
  event->any.send_event = FALSE;

  switch (type)
    {
    case GDK_MOTION_NOTIFY:
      event->motion.time = the_time;
      event->motion.axes = NULL;
      break;
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      event->button.time = the_time;
      event->button.axes = NULL;
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
    case GDK_SCROLL:
      event->scroll.time = the_time;
      break;
    case GDK_FOCUS_CHANGE:
    case GDK_CONFIGURE:
    case GDK_MAP:
    case GDK_UNMAP:
    case GDK_CLIENT_EVENT:
    case GDK_VISIBILITY_NOTIFY:
    case GDK_NO_EXPOSE:
    case GDK_DELETE:
    case GDK_DESTROY:
    case GDK_EXPOSE:
    default:
      break;
    }
}

GdkEvent *
gdk_directfb_event_make (GdkWindow    *window,
                         GdkEventType  type)
{
  GdkEvent *event = gdk_event_new (GDK_NOTHING);

  gdk_directfb_event_fill (event, window, type);

  _gdk_event_queue_append (gdk_display_get_default (), event);

  return event;
}

void
gdk_error_trap_push (void)
{
}

gint
gdk_error_trap_pop (void)
{
  return 0;
}

GdkGrabStatus
gdk_keyboard_grab (GdkWindow *window,
                   gint       owner_events,
                   guint32    time)
{
  return gdk_directfb_keyboard_grab (gdk_display_get_default(),
                                     window,
                                     owner_events,
                                     time);
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


GdkGrabStatus
_gdk_windowing_pointer_grab (GdkWindow    *window,
                             GdkWindow    *native,
                             gboolean      owner_events,
                             GdkEventMask  event_mask,
                             GdkWindow    *confine_to,
                             GdkCursor    *cursor,
                             guint32       time)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  g_return_val_if_fail (confine_to == NULL || GDK_IS_WINDOW (confine_to), 0);

  _gdk_display_add_pointer_grab (&_gdk_display->parent,
                                 window,
                                 native,
                                 owner_events,
                                 event_mask,
                                 0,
                                 time,
                                 FALSE);

  return GDK_GRAB_SUCCESS;
}

#define __GDK_MAIN_X11_C__
#include "gdkaliasdef.c"
