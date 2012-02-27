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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkeventtranslator.h"
#include "gdkwindow-x11.h"

typedef GdkEventTranslatorIface GdkEventTranslatorInterface;
G_DEFINE_INTERFACE (GdkEventTranslator, _gdk_x11_event_translator, G_TYPE_OBJECT);


static void
_gdk_x11_event_translator_default_init (GdkEventTranslatorInterface *iface)
{
}


GdkEvent *
_gdk_x11_event_translator_translate (GdkEventTranslator *translator,
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

GdkEventMask
_gdk_x11_event_translator_get_handled_events (GdkEventTranslator *translator)
{
  GdkEventTranslatorIface *iface;

  g_return_val_if_fail (GDK_IS_EVENT_TRANSLATOR (translator), 0);

  iface = GDK_EVENT_TRANSLATOR_GET_IFACE (translator);

  if (iface->get_handled_events)
    return iface->get_handled_events (translator);

  return 0;
}

void
_gdk_x11_event_translator_select_window_events (GdkEventTranslator *translator,
                                                Window              window,
                                                GdkEventMask        event_mask)
{
  GdkEventTranslatorIface *iface;

  g_return_if_fail (GDK_IS_EVENT_TRANSLATOR (translator));

  iface = GDK_EVENT_TRANSLATOR_GET_IFACE (translator);

  if (iface->select_window_events)
    iface->select_window_events (translator, window, event_mask);
}

GdkWindow *
_gdk_x11_event_translator_get_window (GdkEventTranslator *translator,
                                      GdkDisplay         *display,
                                      XEvent             *xevent)
{
  GdkEventTranslatorIface *iface;

  g_return_val_if_fail (GDK_IS_EVENT_TRANSLATOR (translator), NULL);

  iface = GDK_EVENT_TRANSLATOR_GET_IFACE (translator);

  if (iface->get_window)
    return iface->get_window (translator, xevent);

  return NULL;
}
