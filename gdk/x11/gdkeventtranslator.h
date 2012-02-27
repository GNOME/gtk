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

#ifndef __GDK_EVENT_TRANSLATOR_H__
#define __GDK_EVENT_TRANSLATOR_H__

#include "gdktypes.h"
#include "gdkdisplay.h"

#include <X11/Xlib.h>

G_BEGIN_DECLS

#define GDK_TYPE_EVENT_TRANSLATOR         (_gdk_x11_event_translator_get_type ())
#define GDK_EVENT_TRANSLATOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_EVENT_TRANSLATOR, GdkEventTranslator))
#define GDK_IS_EVENT_TRANSLATOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_EVENT_TRANSLATOR))
#define GDK_EVENT_TRANSLATOR_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE  ((o), GDK_TYPE_EVENT_TRANSLATOR, GdkEventTranslatorIface))

typedef struct _GdkEventTranslatorIface GdkEventTranslatorIface;
typedef struct _GdkEventTranslator GdkEventTranslator; /* Dummy typedef */

struct _GdkEventTranslatorIface
{
  GTypeInterface iface;

  /* VMethods */
  gboolean (* translate_event) (GdkEventTranslator *translator,
                                GdkDisplay         *display,
                                GdkEvent           *event,
                                XEvent             *xevent);

  GdkEventMask (* get_handled_events)   (GdkEventTranslator *translator);
  void         (* select_window_events) (GdkEventTranslator *translator,
                                         Window              window,
                                         GdkEventMask        event_mask);
  GdkWindow *  (* get_window)           (GdkEventTranslator *translator,
                                         XEvent             *xevent);
};

GType      _gdk_x11_event_translator_get_type (void) G_GNUC_CONST;

GdkEvent * _gdk_x11_event_translator_translate (GdkEventTranslator *translator,
                                               GdkDisplay         *display,
                                               XEvent             *xevent);
GdkEventMask _gdk_x11_event_translator_get_handled_events   (GdkEventTranslator *translator);
void         _gdk_x11_event_translator_select_window_events (GdkEventTranslator *translator,
                                                             Window              window,
                                                             GdkEventMask        event_mask);
GdkWindow *  _gdk_x11_event_translator_get_window           (GdkEventTranslator *translator,
                                                             GdkDisplay         *display,
                                                             XEvent             *xevent);

G_END_DECLS

#endif /* __GDK_EVENT_TRANSLATOR_H__ */
