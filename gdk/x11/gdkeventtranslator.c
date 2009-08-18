/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
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

#include "gdkeventtranslator.h"

GType
gdk_event_translator_get_type (void)
{
  static GType translator_type = 0;

  if (G_UNLIKELY (!translator_type))
    {
      translator_type = g_type_register_static_simple (G_TYPE_INTERFACE,
                                                       g_intern_static_string ("GdkEventTranslator"),
                                                       sizeof (GdkEventTranslatorIface),
                                                       NULL, 0, NULL, 0);

      g_type_interface_add_prerequisite (translator_type, G_TYPE_OBJECT);
    }

  return translator_type;
}

GdkEvent *
gdk_event_translator_translate (GdkEventTranslator *translator,
                                GdkDisplay         *display,
                                XEvent             *xevent)
{
  GdkEventTranslatorIface *iface;
  GdkEvent *event;

  g_return_val_if_fail (GDK_IS_EVENT_TRANSLATOR (translator), NULL);
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  iface = GDK_EVENT_TRANSLATOR_GET_IFACE (translator);

  if (!iface->translate_event)
    return NULL;

  event = gdk_event_new (GDK_NOTHING);

  if ((iface->translate_event) (translator, display, event, xevent))
    return event;

  gdk_event_free (event);

  return NULL;
}

static GdkWindow *
get_gdk_window (GdkDisplay *display,
                Window      xwindow)
{
  GdkWindow *window;

  /* Now find out the corresponding GdkWindows */
  window = gdk_window_lookup_for_display (display, xwindow);

  /* We may receive events such as NoExpose/GraphicsExpose
   * and ShmCompletion for pixmaps
   */
  if (window && !GDK_IS_WINDOW (window))
    window = NULL;

  return window;
}

GdkWindow *
gdk_event_translator_get_event_window (GdkEventTranslator *translator,
                                       GdkDisplay         *display,
                                       XEvent             *xevent)
{
  GdkEventTranslatorIface *iface;
  Window xwindow = None;

  g_return_val_if_fail (GDK_IS_EVENT_TRANSLATOR (translator), NULL);

  iface = GDK_EVENT_TRANSLATOR_GET_IFACE (translator);

  if (iface->get_event_window)
    xwindow = (iface->get_event_window) (translator, xevent);

  if (xwindow == None)
    {
      /* Default implementation */
      switch (xevent->type)
	{
	case CreateNotify:
	  xwindow = xevent->xcreatewindow.window;
	  break;
	case DestroyNotify:
	  xwindow = xevent->xdestroywindow.window;
	  break;
	case UnmapNotify:
	  xwindow = xevent->xunmap.window;
	  break;
	case MapNotify:
	  xwindow = xevent->xmap.window;
	  break;
	case MapRequest:
	  xwindow = xevent->xmaprequest.window;
	  break;
	case ReparentNotify:
	  xwindow = xevent->xreparent.window;
	  break;
	case ConfigureNotify:
	  xwindow = xevent->xconfigure.window;
	  break;
	case ConfigureRequest:
	  xwindow = xevent->xconfigurerequest.window;
	  break;
	case GravityNotify:
	  xwindow = xevent->xgravity.window;
	  break;
	case CirculateNotify:
	  xwindow = xevent->xcirculate.window;
	  break;
	case CirculateRequest:
	  xwindow = xevent->xcirculaterequest.window;
	  break;
	default:
	  xwindow = xevent->xany.window;
	}
    }

  return get_gdk_window (display, xwindow);
}
