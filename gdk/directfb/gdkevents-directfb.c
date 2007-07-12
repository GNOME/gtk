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

#include "config.h"
#include "gdk.h"
#include "gdkdirectfb.h"
#include "gdkprivate-directfb.h"

#include "gdkinternals.h"

#include "gdkkeysyms.h"

#include "gdkinput-directfb.h"
#include <string.h>

#ifndef __GDK_X_H__
#define __GDK_X_H__
gboolean gdk_net_wm_supports (GdkAtom property);
#endif

#include "gdkalias.h"

#define EventBuffer _gdk_display->buffer
#define DirectFB _gdk_display->directfb




#include "gdkaliasdef.c"

/*********************************************
 * Functions for maintaining the event queue *
 *********************************************/

static GdkEvent * gdk_event_translate  (DFBWindowEvent  *dfbevent,
                                        GdkWindow       *window);

/*
 * Private variable declarations
 */
static GList *client_filters;  /* Filters for client messages */

static void
dfb_events_process_window_event (DFBWindowEvent *event)
{
  GdkWindow *window = gdk_directfb_window_id_table_lookup (event->window_id);

  if (! window)
     return;

  gdk_event_translate (event, window);
}

static gboolean
gdk_event_send_client_message_by_window (GdkEvent *event,
                                        GdkWindow *window)
{
  GdkEvent *new_event;

  g_return_val_if_fail(event != NULL, FALSE);
  g_return_val_if_fail(GDK_IS_WINDOW(window), FALSE);

  new_event = gdk_directfb_event_make (window, GDK_CLIENT_EVENT);
  new_event->client.message_type = event->client.message_type;
  new_event->client.data_format = event->client.data_format;
  memcpy(&new_event->client.data,
        &event->client.data,
        sizeof(event->client.data));

  return TRUE;
}


static void
dfb_events_dispatch (void)
{
  GdkDisplay *display = gdk_display_get_default ();
  GdkEvent   *event;

  while ((event = _gdk_event_unqueue (display)) != NULL)
    {
      if (_gdk_event_func)
        (*_gdk_event_func) (event, _gdk_event_data);

      gdk_event_free (event);
    }
}

static gboolean
dfb_events_io_func (GIOChannel   *channel,
                    GIOCondition  condition,
                    gpointer      data)
{
  gsize      i;
  gsize      read;
  GIOStatus  result;
  DFBEvent   buf[23];
  DFBEvent  *event;

  result = g_io_channel_read_chars (channel,
                                    (gchar *) buf, sizeof (buf), &read, NULL);

  if (result == G_IO_STATUS_ERROR)
    {
      g_warning ("%s: GIOError occured", __FUNCTION__);
      return TRUE;
    }

  read /= sizeof (DFBEvent);

  for (i = 0, event = buf; i < read; i++, event++)
    {
      switch (event->clazz)
        {
        case DFEC_WINDOW:
          /* TODO workaround to prevent two DWET_ENTER in a row from being delivered */
          if (event->window.type == DWET_ENTER ) {
            if ( i>0 && buf[i-1].window.type != DWET_ENTER )
              dfb_events_process_window_event (&event->window);
          }
          else
          dfb_events_process_window_event (&event->window);
          break;
        default:
          break;
        }
    }

  EventBuffer->Reset (EventBuffer);

  dfb_events_dispatch ();

  return TRUE;
}

void
_gdk_events_init (void)
{
  GIOChannel *channel;
  GSource    *source;
  DFBResult   ret;
  gint        fd;

  ret = DirectFB->CreateEventBuffer (DirectFB, &EventBuffer);
  if (ret)
    {
      DirectFBError ("_gdk_events_init: "
                     "IDirectFB::CreateEventBuffer() failed", ret);
      return;
    }

  ret = EventBuffer->CreateFileDescriptor (EventBuffer, &fd);
  if (ret)
    {
      DirectFBError ("_gdk_events_init: "
                     "IDirectFBEventBuffer::CreateFileDescriptor() failed",
                     ret);
      return;
    }

  channel = g_io_channel_unix_new (fd);

  g_io_channel_set_encoding (channel, NULL, NULL);
  g_io_channel_set_buffered (channel, FALSE);

  source = g_io_create_watch (channel, G_IO_IN);

  g_source_set_priority (source, G_PRIORITY_DEFAULT);
  g_source_set_can_recurse (source, TRUE);
  g_source_set_callback (source, (GSourceFunc) dfb_events_io_func, NULL, NULL);

  g_source_attach (source, NULL);
  g_source_unref (source);
}

gboolean
gdk_events_pending (void)
{
  GdkDisplay *display = gdk_display_get_default ();

  return _gdk_event_queue_find_first (display) ? TRUE : FALSE;
}

GdkEvent *
gdk_event_get_graphics_expose (GdkWindow *window)
{
  GdkDisplay *display;
  GList      *list;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  display = gdk_drawable_get_display (GDK_DRAWABLE (window));

  for (list = _gdk_event_queue_find_first (display); list; list = list->next)
    {
      GdkEvent *event = list->data;
      if (event->type == GDK_EXPOSE && event->expose.window == window)
        break;
    }

  if (list)
    {
      GdkEvent *retval = list->data;

      _gdk_event_queue_remove_link (display, list);
      g_list_free_1 (list);

      return retval;
    }

  return NULL;
}

void
_gdk_events_queue (GdkDisplay *display)
{
}

void
gdk_flush (void)
{
gdk_display_flush ( GDK_DISPLAY_OBJECT(_gdk_display));
}

/* Sends a ClientMessage to all toplevel client windows */
gboolean
gdk_event_send_client_message_for_display (GdkDisplay *display,
                                           GdkEvent   *event,
                                           guint32     xid)
{
  GdkWindow *win = NULL;
  gboolean ret = TRUE;

  g_return_val_if_fail(event != NULL, FALSE);

  win = gdk_window_lookup_for_display (display, (GdkNativeWindow) xid);

  g_return_val_if_fail(win != NULL, FALSE);

  if ((GDK_WINDOW_OBJECT(win)->window_type != GDK_WINDOW_CHILD) &&
      (g_object_get_data (G_OBJECT (win), "gdk-window-child-handler")))
    {
      /* Managed window, check children */
      GList *ltmp = NULL;
      for (ltmp = GDK_WINDOW_OBJECT(win)->children; ltmp; ltmp = ltmp->next)
       {
         ret &= gdk_event_send_client_message_by_window (event,
                                                         GDK_WINDOW(ltmp->data));
       }
    }
  else
    {
      ret &= gdk_event_send_client_message_by_window (event, win);
    }

  return ret;
}

/*****/

guint32
gdk_directfb_get_time (void)
{
  GTimeVal tv;

  g_get_current_time (&tv);

  return (guint32) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void
gdk_directfb_event_windows_add (GdkWindow *window)
{
  GdkWindowImplDirectFB *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (window)->impl);

  if (!impl->window)
    return;

  if (EventBuffer)
    impl->window->AttachEventBuffer (impl->window, EventBuffer);
  else
    impl->window->CreateEventBuffer (impl->window, &EventBuffer);
}

#if (DIRECTFB_MAJOR_VERSION >= 1)
void
gdk_directfb_event_windows_remove (GdkWindow *window)
{
  GdkWindowImplDirectFB *impl;

  g_return_if_fail (GDK_IS_WINDOW (window));

  impl = GDK_WINDOW_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (window)->impl);

  if (!impl->window)
    return;

  if (EventBuffer)
    impl->window->DetachEventBuffer (impl->window, EventBuffer);
/* FIXME: should we warn if (! EventBuffer) ? */
}
#endif

GdkWindow *
gdk_directfb_child_at (GdkWindow *window,
                       gint      *winx,
                       gint      *winy)
{
  GdkWindowObject *private;
  GList           *list;

  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  private = GDK_WINDOW_OBJECT (window);
  for (list = private->children; list; list = list->next)
    {
      GdkWindowObject *win = list->data;

      if (GDK_WINDOW_IS_MAPPED (win) &&
          *winx >= win->x  &&
          *winx <  win->x + GDK_DRAWABLE_IMPL_DIRECTFB (win->impl)->width  &&
          *winy >= win->y  &&
          *winy <  win->y + GDK_DRAWABLE_IMPL_DIRECTFB (win->impl)->height)
        {
          *winx -= win->x;
          *winy -= win->y;

          return gdk_directfb_child_at (GDK_WINDOW (win), winx, winy );
        }
    }

  return window;
}

static GdkEvent *
gdk_event_translate (DFBWindowEvent *dfbevent,
                     GdkWindow      *window)
{
  GdkWindowObject *private;
  GdkDisplay      *display;
  GdkEvent        *event    = NULL;

  g_return_val_if_fail (dfbevent != NULL, NULL);
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  private = GDK_WINDOW_OBJECT (window);

  g_object_ref (G_OBJECT (window));
  display = gdk_drawable_get_display (GDK_DRAWABLE (window));

  switch (dfbevent->type)
    {
    case DWET_BUTTONDOWN:
    case DWET_BUTTONUP:
      {
        static gboolean  click_grab = FALSE;
        GdkWindow       *child;
        gint             wx, wy;
        guint            mask;
        guint            button;

        _gdk_directfb_mouse_x = wx = dfbevent->cx;
        _gdk_directfb_mouse_y = wy = dfbevent->cy;

        switch (dfbevent->button)
          {
          case DIBI_LEFT:
            button = 1;
            mask   = GDK_BUTTON1_MASK;
            break;
          case DIBI_MIDDLE:
            button = 2;
            mask   = GDK_BUTTON2_MASK;
            break;
          case DIBI_RIGHT:
            button = 3;
            mask   = GDK_BUTTON3_MASK;
            break;
          default:
            button = dfbevent->button + 1;
            mask   = 0;
            break;
          }

        child = gdk_directfb_child_at (_gdk_parent_root, &wx, &wy);

        if (_gdk_directfb_pointer_grab_window &&
            (_gdk_directfb_pointer_grab_events & (dfbevent->type ==
                                                  DWET_BUTTONDOWN ?
                                                  GDK_BUTTON_PRESS_MASK :
                                                  GDK_BUTTON_RELEASE_MASK)) &&
            (_gdk_directfb_pointer_grab_owner_events == FALSE ||
             child == _gdk_parent_root) )
          {
            GdkDrawableImplDirectFB *impl;

            child = _gdk_directfb_pointer_grab_window;
            impl  = GDK_DRAWABLE_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (child)->impl);

            dfbevent->x = dfbevent->cx - impl->abs_x;
            dfbevent->y = dfbevent->cy - impl->abs_y;
          }
        else if (!_gdk_directfb_pointer_grab_window ||
                 (_gdk_directfb_pointer_grab_owner_events == TRUE))
          {
            dfbevent->x = wx;
            dfbevent->y = wy;
          }
        else
          {
            child = NULL;
          }

        if (dfbevent->type == DWET_BUTTONDOWN)
          _gdk_directfb_modifiers |= mask;
        else
          _gdk_directfb_modifiers &= ~mask;

        if (child)
          {
            event =
              gdk_directfb_event_make (child,
                                       dfbevent->type == DWET_BUTTONDOWN ?
                                       GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE);

            event->button.x_root = _gdk_directfb_mouse_x;
            event->button.y_root = _gdk_directfb_mouse_y;

            event->button.x = dfbevent->x;
            event->button.y = dfbevent->y;

            event->button.state  = _gdk_directfb_modifiers;
            event->button.button = button;
            event->button.device = display->core_pointer;

            GDK_NOTE (EVENTS,
                      g_message ("button: %d at %d,%d %s with state 0x%08x",
                                 event->button.button,
                                 (int)event->button.x, (int)event->button.y,
                                 dfbevent->type == DWET_BUTTONDOWN ?
                                 "pressed" : "released",
                                 _gdk_directfb_modifiers));

            if (dfbevent->type == DWET_BUTTONDOWN)
              _gdk_event_button_generate (display, event);
          }

        /* Handle implicit button grabs: */
        if (dfbevent->type == DWET_BUTTONDOWN  &&  !click_grab  &&  child)
          {
            if (gdk_directfb_pointer_grab (child, FALSE,
                                           gdk_window_get_events (child),
                                           NULL, NULL,
                                           GDK_CURRENT_TIME,
                                           TRUE) == GDK_GRAB_SUCCESS)
              click_grab = TRUE;
          }
        else if (dfbevent->type == DWET_BUTTONUP &&
                 !(_gdk_directfb_modifiers & (GDK_BUTTON1_MASK |
                                              GDK_BUTTON2_MASK |
                                              GDK_BUTTON3_MASK)) && click_grab)
          {
            gdk_directfb_pointer_ungrab (GDK_CURRENT_TIME, TRUE);
            click_grab = FALSE;
          }
      }
      break;

    case DWET_MOTION:
      {
        GdkWindow *event_win=NULL;
        GdkWindow *child;

        _gdk_directfb_mouse_x = dfbevent->cx;
        _gdk_directfb_mouse_y = dfbevent->cy;

	//child = gdk_directfb_child_at (window, &dfbevent->x, &dfbevent->y);
    /* Go all the way to root to catch popup menus */
    int wx=_gdk_directfb_mouse_x;
    int wy=_gdk_directfb_mouse_y;
	child = gdk_directfb_child_at (_gdk_parent_root, &wx, &wy);

    /* first let's see if any cossing event has to be send */
    gdk_directfb_window_send_crossing_events (NULL, child, GDK_CROSSING_NORMAL);

    /* then dispatch the motion event to the window the cursor it's inside */
	event_win = gdk_directfb_pointer_event_window (child, GDK_MOTION_NOTIFY);


	if (event_win)
	  {

	    if (event_win == _gdk_directfb_pointer_grab_window) {
		GdkDrawableImplDirectFB *impl;

		child = _gdk_directfb_pointer_grab_window;
		impl = GDK_DRAWABLE_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (child)->impl);

		dfbevent->x = _gdk_directfb_mouse_x - impl->abs_x;
		dfbevent->y = _gdk_directfb_mouse_y - impl->abs_y;
	      }

	    event = gdk_directfb_event_make (child, GDK_MOTION_NOTIFY);

	    event->motion.x_root = _gdk_directfb_mouse_x;
	    event->motion.y_root = _gdk_directfb_mouse_y;

	    //event->motion.x = dfbevent->x;
	    //event->motion.y = dfbevent->y;
	    event->motion.x = wx;
	    event->motion.y = wy;

	    event->motion.state   = _gdk_directfb_modifiers;
	    event->motion.is_hint = FALSE;
	    event->motion.device  = display->core_pointer;

	    if (GDK_WINDOW_OBJECT (event_win)->event_mask &
		GDK_POINTER_MOTION_HINT_MASK)
	      {
		while (EventBuffer->PeekEvent (EventBuffer,
					       DFB_EVENT (dfbevent)) == DFB_OK
		       && dfbevent->type == DWET_MOTION)
		  {
		    EventBuffer->GetEvent (EventBuffer, DFB_EVENT (dfbevent));
		    event->motion.is_hint = TRUE;
		  }
	      }
	  }
          /* make sure crossing events go to the event window found */
/*        GdkWindow *ev_win = ( event_win == NULL ) ? gdk_window_at_pointer (NULL,NULL) :event_win;
	     gdk_directfb_window_send_crossing_events (NULL,ev_win,GDK_CROSSING_NORMAL);
*/
      }
      break;

    case DWET_GOTFOCUS:
      gdk_directfb_change_focus (window);

      break;

    case DWET_LOSTFOCUS:
      gdk_directfb_change_focus (_gdk_parent_root);

      break;

    case DWET_POSITION:
      {
        GdkWindow *event_win;

        private->x = dfbevent->x;
        private->y = dfbevent->y;

        event_win = gdk_directfb_other_event_window (window, GDK_CONFIGURE);

        if (event_win)
          {
            event = gdk_directfb_event_make (event_win, GDK_CONFIGURE);
            event->configure.x = dfbevent->x;
            event->configure.y = dfbevent->y;
            event->configure.width =
              GDK_DRAWABLE_IMPL_DIRECTFB (private->impl)->width;
            event->configure.height =
              GDK_DRAWABLE_IMPL_DIRECTFB (private->impl)->height;
          }

        _gdk_directfb_calc_abs (window);
      }
      break;

    case DWET_POSITION_SIZE:
      private->x = dfbevent->x;
      private->y = dfbevent->y;
      /* fallthru */

    case DWET_SIZE:
      {
        GdkDrawableImplDirectFB *impl;
        GdkWindow               *event_win;
        GList                   *list;

        impl = GDK_DRAWABLE_IMPL_DIRECTFB (private->impl);

        event_win = gdk_directfb_other_event_window (window, GDK_CONFIGURE);

        if (event_win)
          {
            event = gdk_directfb_event_make (event_win, GDK_CONFIGURE);
            event->configure.x      = private->x;
            event->configure.y      = private->y;
            event->configure.width  = dfbevent->w;
            event->configure.height = dfbevent->h;
          }

        impl->width  = dfbevent->w;
        impl->height = dfbevent->h;

        for (list = private->children; list; list = list->next)
          {
            GdkWindowObject         *win;
            GdkDrawableImplDirectFB *impl;

            win  = GDK_WINDOW_OBJECT (list->data);
            impl = GDK_DRAWABLE_IMPL_DIRECTFB (win->impl);

            _gdk_directfb_move_resize_child (GDK_WINDOW (win),
                                             win->x, win->y,
                                             impl->width, impl->height);
          }

        _gdk_directfb_calc_abs (window);

        gdk_window_clear (window);
        gdk_window_invalidate_rect (window, NULL, TRUE);
      }
      break;

    case DWET_KEYDOWN:
    case DWET_KEYUP:
      {

        GdkEventType type = (dfbevent->type == DWET_KEYUP ?
                             GDK_KEY_RELEASE : GDK_KEY_PRESS);
        GdkWindow *event_win =
          gdk_directfb_keyboard_event_window (gdk_directfb_window_find_focus (),
                                              type);
        if (event_win)
          {
            event = gdk_directfb_event_make (event_win, type);
            gdk_directfb_translate_key_event (dfbevent, &event->key);
          }
      }
      break;

    case DWET_LEAVE:
      _gdk_directfb_mouse_x = dfbevent->cx;
      _gdk_directfb_mouse_y = dfbevent->cy;

      gdk_directfb_window_send_crossing_events (NULL, _gdk_parent_root,
                                                GDK_CROSSING_NORMAL);

      if (gdk_directfb_apply_focus_opacity)
        {
          if (GDK_WINDOW_IS_MAPPED (window))
            GDK_WINDOW_IMPL_DIRECTFB (private->impl)->window->SetOpacity
              (GDK_WINDOW_IMPL_DIRECTFB (private->impl)->window,
               (GDK_WINDOW_IMPL_DIRECTFB (private->impl)->opacity >> 1) +
               (GDK_WINDOW_IMPL_DIRECTFB (private->impl)->opacity >> 2));
        }
      break;

    case DWET_ENTER:
      {
        GdkWindow *child;

        _gdk_directfb_mouse_x = dfbevent->cx;
        _gdk_directfb_mouse_y = dfbevent->cy;

        child = gdk_directfb_child_at (window, &dfbevent->x, &dfbevent->y);

        /* this makes sure pointer is set correctly when it previously left
         * a window being not standard shaped
         */
        gdk_window_set_cursor (window, NULL);
        gdk_directfb_window_send_crossing_events (NULL, child,
                                                  GDK_CROSSING_NORMAL);

        if (gdk_directfb_apply_focus_opacity)
          {
            GDK_WINDOW_IMPL_DIRECTFB (private->impl)->window->SetOpacity
              (GDK_WINDOW_IMPL_DIRECTFB (private->impl)->window,
               GDK_WINDOW_IMPL_DIRECTFB (private->impl)->opacity);
          }
      }
      break;

    case DWET_CLOSE:
      {
 
        GdkWindow *event_win;

        event_win = gdk_directfb_other_event_window (window, GDK_DELETE);

        if (event_win)
          event = gdk_directfb_event_make (event_win, GDK_DELETE);
      }
      break;

    case DWET_DESTROYED:
      {
        GdkWindow *event_win;

        event_win = gdk_directfb_other_event_window (window, GDK_DESTROY);

        if (event_win)
          event = gdk_directfb_event_make (event_win, GDK_DESTROY);

	gdk_window_destroy_notify (window);
      }
      break;

    case DWET_WHEEL:
      {
        GdkWindow *event_win;

        _gdk_directfb_mouse_x = dfbevent->cx;
        _gdk_directfb_mouse_y = dfbevent->cy;

        if (_gdk_directfb_pointer_grab_window)
          {
            GdkDrawableImplDirectFB *impl;

            event_win = _gdk_directfb_pointer_grab_window;
            impl =
              GDK_DRAWABLE_IMPL_DIRECTFB (GDK_WINDOW_OBJECT (event_win)->impl);

            dfbevent->x = dfbevent->cx - impl->abs_x;
            dfbevent->y = dfbevent->cy - impl->abs_y;
          }
        else
          {
            event_win = gdk_directfb_child_at (window,
                                               &dfbevent->x, &dfbevent->y);
          }

        if (event_win)
          {
            event = gdk_directfb_event_make (event_win, GDK_SCROLL);

            event->scroll.direction = (dfbevent->step < 0 ?
                                       GDK_SCROLL_DOWN : GDK_SCROLL_UP);

            event->scroll.x_root = _gdk_directfb_mouse_x;
            event->scroll.y_root = _gdk_directfb_mouse_y;
            event->scroll.x      = dfbevent->x;
            event->scroll.y      = dfbevent->y;
            event->scroll.state  = _gdk_directfb_modifiers;
            event->scroll.device = display->core_pointer;
          }
      }
      break;

    default:
      g_message ("unhandled DirectFB windowing event 0x%08x", dfbevent->type);
    }

  g_object_unref (G_OBJECT (window));

  return event;
}

gboolean
gdk_screen_get_setting (GdkScreen   *screen,
                        const gchar *name,
                        GValue      *value)
{
  return FALSE;
}

void
gdk_display_add_client_message_filter (GdkDisplay   *display,
                                       GdkAtom       message_type,
                                       GdkFilterFunc func,
                                       gpointer      data)
{
  /* XXX: display should be used */
  GdkClientFilter *filter = g_new (GdkClientFilter, 1);

  filter->type = message_type;
  filter->function = func;
  filter->data = data;
  client_filters = g_list_append (client_filters, filter);
}


void
gdk_add_client_message_filter (GdkAtom       message_type,
                               GdkFilterFunc func,
                               gpointer      data)
{
  gdk_display_add_client_message_filter (gdk_display_get_default (),
                                         message_type, func, data);
}

void
gdk_screen_broadcast_client_message (GdkScreen *screen,
				     GdkEvent  *sev)
{
  GdkWindow *root_window;
  GdkWindowObject *private;
  GList *top_level = NULL;

  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail(sev != NULL);

  root_window = gdk_screen_get_root_window (screen);

  g_return_if_fail(GDK_IS_WINDOW(root_window));

  private = GDK_WINDOW_OBJECT (root_window);

  for (top_level = private->children; top_level; top_level = top_level->next)
    {
      gdk_event_send_client_message_for_display (gdk_drawable_get_display(GDK_DRAWABLE(root_window)),
                                                sev,
                                                (guint32)(GDK_WINDOW_DFB_ID(GDK_WINDOW(top_level->data))));
   }
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
   return FALSE;
}

#define __GDK_EVENTS_X11_C__
#include "gdkaliasdef.c"
