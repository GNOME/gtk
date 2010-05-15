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

D_DEBUG_DOMAIN (GDKDFB_Events, "GDKDFB/Events", "GDK DirectFB Events");
D_DEBUG_DOMAIN (GDKDFB_MouseEvents, "GDKDFB/Events/Mouse", "GDK DirectFB Mouse Events");
D_DEBUG_DOMAIN (GDKDFB_WindowEvents, "GDKDFB/Events/Window", "GDK DirectFB Window Events");
D_DEBUG_DOMAIN (GDKDFB_KeyEvents, "GDKDFB/Events/Key", "GDK DirectFB Key Events");

/*********************************************
 * Functions for maintaining the event queue *
 *********************************************/

static gboolean gdk_event_translate  (GdkEvent       *event,
                                      DFBWindowEvent *dfbevent,
                                      GdkWindow      *window);

/*
 * Private variable declarations
 */
static GList *client_filters;  /* Filters for client messages */

static gint
gdk_event_apply_filters (DFBWindowEvent *dfbevent,
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
      result = filter->function (dfbevent, event, filter->data);
      if (result !=  GDK_FILTER_CONTINUE)
        return result;
    }

  return GDK_FILTER_CONTINUE;
}

static void
dfb_events_process_window_event (DFBWindowEvent *dfbevent)
{
  GdkDisplay *display = gdk_display_get_default ();
  GdkWindow  *window;
  GdkEvent   *event;
  GList      *node;

  window = gdk_directfb_window_id_table_lookup (dfbevent->window_id);
  if (!window)
    return;

  event = gdk_event_new (GDK_NOTHING);

  event->any.window = NULL;

  ((GdkEventPrivate *)event)->flags |= GDK_EVENT_PENDING;

  node = _gdk_event_queue_append (display, event);

  if (gdk_event_translate (event, dfbevent, window))
    {
      ((GdkEventPrivate *)event)->flags &= ~GDK_EVENT_PENDING;
      _gdk_windowing_got_event (display, node, event, 0);
    }
  else
    {
      _gdk_event_queue_remove_link (display, node);
      g_list_free_1 (node);
      gdk_event_free (event);
    }
}

static gboolean
gdk_event_send_client_message_by_window (GdkEvent *event,
                                         GdkWindow *window)
{
  DFBUserEvent evt;

  g_return_val_if_fail(event != NULL, FALSE);
  g_return_val_if_fail(GDK_IS_WINDOW(window), FALSE);

  evt.clazz = DFEC_USER;
  evt.type = GPOINTER_TO_UINT (GDK_ATOM_TO_POINTER (event->client.message_type));
  evt.data = (void *) event->client.data.l[0];

  _gdk_display->buffer->PostEvent (_gdk_display->buffer, DFB_EVENT (&evt));

  return TRUE;
}

static void
dfb_events_dispatch (void)
{
  GdkDisplay *display = gdk_display_get_default ();
  GdkEvent   *event;

  GDK_THREADS_ENTER ();

  while ((event = _gdk_event_unqueue (display)) != NULL)
    {
      if (_gdk_event_func)
        (*_gdk_event_func) (event, _gdk_event_data);

      gdk_event_free (event);
    }

  GDK_THREADS_LEAVE ();
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
      g_warning ("%s: GIOError occured", G_STRFUNC);
      return TRUE;
    }

  read /= sizeof (DFBEvent);

  for (i = 0, event = buf; i < read; i++, event++)
    {
      switch (event->clazz)
        {
        case DFEC_WINDOW:
          /* TODO workaround to prevent two DWET_ENTER in a row from being delivered */
          if (event->window.type == DWET_ENTER) {
            if (i > 0 && buf[i - 1].window.type != DWET_ENTER)
              dfb_events_process_window_event (&event->window);
          }
          else
            dfb_events_process_window_event (&event->window);
          break;

        case DFEC_USER:
          {
            GList *list;

            GDK_NOTE (EVENTS, g_print (" client_message"));

            for (list = client_filters; list; list = list->next)
              {
                GdkClientFilter *filter     = list->data;
                DFBUserEvent    *user_event = (DFBUserEvent *) event;
                GdkAtom          type;

                type = GDK_POINTER_TO_ATOM (GUINT_TO_POINTER (user_event->type));

                if (filter->type == type)
                  {
                    if (filter->function (user_event,
                                          NULL,
                                          filter->data) != GDK_FILTER_CONTINUE)
                      break;
                  }
              }
          }
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
  gdk_display_flush (GDK_DISPLAY_OBJECT (_gdk_display));
}

/* Sends a ClientMessage to all toplevel client windows */
gboolean
gdk_event_send_client_message_for_display (GdkDisplay *display,
                                           GdkEvent   *event,
                                           guint32     xid)
{
  GdkWindow *win = NULL;
  gboolean ret = TRUE;

  g_return_val_if_fail (event != NULL, FALSE);

  win = gdk_window_lookup_for_display (display, (GdkNativeWindow) xid);

  g_return_val_if_fail (win != NULL, FALSE);

  if ((GDK_WINDOW_OBJECT (win)->window_type != GDK_WINDOW_CHILD) &&
      (g_object_get_data (G_OBJECT (win), "gdk-window-child-handler")))
    {
      /* Managed window, check children */
      GList *ltmp = NULL;
      for (ltmp = GDK_WINDOW_OBJECT (win)->children; ltmp; ltmp = ltmp->next)
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
      gint wx, wy, ww, wh;

      gdk_window_get_geometry (GDK_WINDOW (win), &wx, &wy, &ww, &wh, NULL);

      if (GDK_WINDOW_IS_MAPPED (win) &&
          *winx >= wx  && *winx <  wx + ww  &&
          *winy >= wy  && *winy <  wy + wh)
        {
          *winx -= win->x;
          *winy -= win->y;

          return gdk_directfb_child_at (GDK_WINDOW (win), winx, winy);
        }
    }

  return window;
}

static gboolean
gdk_event_translate (GdkEvent       *event,
                     DFBWindowEvent *dfbevent,
                     GdkWindow      *window)
{
  GdkWindowObject *private;
  GdkDisplay      *display;
  /* GdkEvent        *event    = NULL; */
  gboolean return_val = FALSE;

  g_return_val_if_fail (event != NULL, FALSE);
  g_return_val_if_fail (dfbevent != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  D_DEBUG_AT (GDKDFB_Events, "%s( %p, %p, %p )\n", G_STRFUNC,
              event, dfbevent, window);

  private = GDK_WINDOW_OBJECT (window);

  g_object_ref (G_OBJECT (window));

  event->any.window = NULL;
  event->any.send_event = FALSE;

  /*
   * Apply global filters
   *
   * If result is GDK_FILTER_CONTINUE, we continue as if nothing
   * happened. If it is GDK_FILTER_REMOVE or GDK_FILTER_TRANSLATE,
   * we return TRUE and won't dispatch the event.
   */
  if (_gdk_default_filters)
    {
      GdkFilterReturn result;
      result = gdk_event_apply_filters (dfbevent, event, _gdk_default_filters);

      if (result != GDK_FILTER_CONTINUE)
        {
          return_val = (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
          goto done;
        }
    }

  /* Apply per-window filters */
  if (GDK_IS_WINDOW (window))
    {
      GdkFilterReturn result;

      if (private->filters)
	{
	  result = gdk_event_apply_filters (dfbevent, event, private->filters);

	  if (result != GDK_FILTER_CONTINUE)
	    {
	      return_val = (result == GDK_FILTER_TRANSLATE) ? TRUE : FALSE;
	      goto done;
	    }
	}
    }

  display = gdk_drawable_get_display (GDK_DRAWABLE (window));

  return_val = TRUE;

  switch (dfbevent->type)
    {
    case DWET_BUTTONDOWN:
    case DWET_BUTTONUP:
      /* Backend store */
      _gdk_directfb_mouse_x = dfbevent->cx;
      _gdk_directfb_mouse_y = dfbevent->cy;

      /* Event translation */
      gdk_directfb_event_fill (event, window,
                               dfbevent->type == DWET_BUTTONDOWN ?
                               GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE);
      switch (dfbevent->button)
        {
        case DIBI_LEFT:
          event->button.button = 1;
          break;

        case DIBI_MIDDLE:
            event->button.button = 2;
            break;

        case DIBI_RIGHT:
          event->button.button = 3;
          break;

          default:
            event->button.button = dfbevent->button + 1;
            break;
        }

      event->button.window = window;
      event->button.x_root = dfbevent->cx;
      event->button.y_root = dfbevent->cy;
      event->button.x      = dfbevent->x;
      event->button.y      = dfbevent->y;
      event->button.state  = _gdk_directfb_modifiers;
      event->button.device = display->core_pointer;
      gdk_event_set_screen (event, _gdk_screen);

      D_DEBUG_AT (GDKDFB_MouseEvents, "  -> %s at %ix%i\n",
                  event->type == GDK_BUTTON_PRESS ? "buttonpress" : "buttonrelease",
                  (gint) event->button.x, (gint) event->button.y);
      break;

    case DWET_MOTION:
      /* Backend store */
      _gdk_directfb_mouse_x = dfbevent->cx;
      _gdk_directfb_mouse_y = dfbevent->cy;

      /* Event translation */
      gdk_directfb_event_fill (event, window, GDK_MOTION_NOTIFY);
      event->motion.x_root  = dfbevent->cx;
      event->motion.y_root  = dfbevent->cy;
      event->motion.x       = dfbevent->x;
      event->motion.y       = dfbevent->y;
      event->motion.axes    = NULL;
      event->motion.state   = _gdk_directfb_modifiers;
      event->motion.is_hint = FALSE;
      event->motion.device  = display->core_pointer;
      gdk_event_set_screen (event, _gdk_screen);

      D_DEBUG_AT (GDKDFB_MouseEvents, "  -> move pointer to %ix%i\n",
                  (gint) event->button.x, (gint) event->button.y);
      break;

    case DWET_GOTFOCUS:
      gdk_directfb_event_fill (event, window, GDK_FOCUS_CHANGE);
      event->focus_change.window = window;
      event->focus_change.in     = TRUE;
      break;

    case DWET_LOSTFOCUS:
      gdk_directfb_event_fill (event, window, GDK_FOCUS_CHANGE);
      event->focus_change.window = window;
      event->focus_change.in     = FALSE;
      break;

    case DWET_POSITION:
      gdk_directfb_event_fill (event, window, GDK_CONFIGURE);
      event->configure.x      = dfbevent->x;
      event->configure.y      = dfbevent->y;
      event->configure.width  = private->width;
      event->configure.height = private->height;
      break;

    case DWET_POSITION_SIZE:
      event->configure.x = dfbevent->x;
      event->configure.y = dfbevent->y;
      /* fallthru */

    case DWET_SIZE:
      gdk_directfb_event_fill (event, window, GDK_CONFIGURE);
      event->configure.window = window;
      event->configure.width  = dfbevent->w;
      event->configure.height = dfbevent->h;

      D_DEBUG_AT (GDKDFB_WindowEvents,
                  "  -> configure window %p at %ix%i-%ix%i\n",
                  window, event->configure.x, event->configure.y,
                  event->configure.width, event->configure.height);
      break;

    case DWET_KEYDOWN:
    case DWET_KEYUP:
      gdk_directfb_event_fill (event, window,
                               dfbevent->type == DWET_KEYUP ?
                               GDK_KEY_RELEASE : GDK_KEY_PRESS);
      event->key.window = window;
      gdk_directfb_translate_key_event (dfbevent, (GdkEventKey *) event);

      D_DEBUG_AT (GDKDFB_KeyEvents, "  -> key window=%p val=%x code=%x str=%s\n",
                  window,  event->key.keyval, event->key.hardware_keycode,
                  event->key.string);
      break;

    case DWET_ENTER:
    case DWET_LEAVE:
      /* Backend store */
      _gdk_directfb_mouse_x = dfbevent->cx;
      _gdk_directfb_mouse_y = dfbevent->cy;

      /* Event translation */
      gdk_directfb_event_fill (event, window,
                               dfbevent->type == DWET_ENTER ?
                               GDK_ENTER_NOTIFY : GDK_LEAVE_NOTIFY);
      event->crossing.window    = g_object_ref (window);
      event->crossing.subwindow = NULL;
      event->crossing.time      = GDK_CURRENT_TIME;
      event->crossing.x         = dfbevent->x;
      event->crossing.y         = dfbevent->y;
      event->crossing.x_root    = dfbevent->cx;
      event->crossing.y_root    = dfbevent->cy;
      event->crossing.mode      = GDK_CROSSING_NORMAL;
      event->crossing.detail    = GDK_NOTIFY_ANCESTOR;
      event->crossing.state     = 0;

      /**/
      if (gdk_directfb_apply_focus_opacity)
        {
          if (dfbevent->type == DWET_ENTER)
            {
              if (GDK_WINDOW_IS_MAPPED (window))
                GDK_WINDOW_IMPL_DIRECTFB (private->impl)->window->SetOpacity
                  (GDK_WINDOW_IMPL_DIRECTFB (private->impl)->window,
                   (GDK_WINDOW_IMPL_DIRECTFB (private->impl)->opacity >> 1) +
                   (GDK_WINDOW_IMPL_DIRECTFB (private->impl)->opacity >> 2));
            }
          else
            {
              GDK_WINDOW_IMPL_DIRECTFB (private->impl)->window->SetOpacity
                (GDK_WINDOW_IMPL_DIRECTFB (private->impl)->window,
                 GDK_WINDOW_IMPL_DIRECTFB (private->impl)->opacity);
            }
        }

      D_DEBUG_AT (GDKDFB_WindowEvents, "  -> %s window %p at relative=%ix%i absolute=%ix%i\n",
                  dfbevent->type == DWET_ENTER ? "enter" : "leave",
                  window, (gint) event->crossing.x, (gint) event->crossing.y,
                  (gint) event->crossing.x_root, (gint) event->crossing.y_root);
      break;

    case DWET_CLOSE:
      gdk_directfb_event_fill (event, window, GDK_DELETE);
      break;

    case DWET_DESTROYED:
      gdk_directfb_event_fill (event, window, GDK_DESTROY);
      gdk_window_destroy_notify (window);
      break;

    case DWET_WHEEL:
      /* Backend store */
      _gdk_directfb_mouse_x = dfbevent->cx;
      _gdk_directfb_mouse_y = dfbevent->cy;

      /* Event translation */
      gdk_directfb_event_fill (event, window, GDK_SCROLL);
      event->scroll.direction = (dfbevent->step > 0 ?
                                 GDK_SCROLL_UP : GDK_SCROLL_DOWN);
      event->scroll.x_root    = dfbevent->cx;
      event->scroll.y_root    = dfbevent->cy;
      event->scroll.x         = dfbevent->x;
      event->scroll.y         = dfbevent->y;
      event->scroll.state     = _gdk_directfb_modifiers;
      event->scroll.device    = display->core_pointer;

      D_DEBUG_AT (GDKDFB_MouseEvents, "  -> mouse scroll %s at %ix%i\n",
                  event->scroll.direction == GDK_SCROLL_UP ? "up" : "down",
                  (gint) event->scroll.x, (gint) event->scroll.y);
      break;

    default:
      g_message ("unhandled DirectFB windowing event 0x%08x", dfbevent->type);
    }

 done:

  g_object_unref (G_OBJECT (window));

  return return_val;
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
  GdkWindow       *root_window;
  GdkWindowObject *private;
  GList           *top_level = NULL;

  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (sev != NULL);

  root_window = gdk_screen_get_root_window (screen);

  g_return_if_fail (GDK_IS_WINDOW (root_window));

  private = GDK_WINDOW_OBJECT (root_window);

  for (top_level = private->children; top_level; top_level = top_level->next)
    {
      gdk_event_send_client_message_for_display (gdk_drawable_get_display (GDK_DRAWABLE (root_window)),
                                                 sev,
                                                 (guint32)(GDK_WINDOW_DFB_ID (GDK_WINDOW (top_level->data))));
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

void
_gdk_windowing_event_data_copy (const GdkEvent *src,
                                GdkEvent       *dst)
{
}

void
_gdk_windowing_event_data_free (GdkEvent *event)
{
}

#define __GDK_EVENTS_X11_C__
#include "gdkaliasdef.c"
