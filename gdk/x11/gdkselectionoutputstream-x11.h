/* GIO - GLib Output, Output and Streaming Library
 *
 * Copyright (C) 2017 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Benjamin Otte <otte@gnome.org>
 *         Christian Kellner <gicmo@gnome.org>
 */

#ifndef __GDK_X11_SELECTION_OUTPUT_STREAM_H__
#define __GDK_X11_SELECTION_OUTPUT_STREAM_H__

#include <gio/gio.h>
#include "gdktypes.h"

#include <X11/Xlib.h>

G_BEGIN_DECLS

#define GDK_TYPE_X11_SELECTION_OUTPUT_STREAM         (gdk_x11_selection_output_stream_get_type ())
#define GDK_X11_SELECTION_OUTPUT_STREAM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_X11_SELECTION_OUTPUT_STREAM, GdkX11SelectionOutputStream))
#define GDK_X11_SELECTION_OUTPUT_STREAM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GDK_TYPE_X11_SELECTION_OUTPUT_STREAM, GdkX11SelectionOutputStreamClass))
#define GDK_IS_X11_SELECTION_OUTPUT_STREAM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_X11_SELECTION_OUTPUT_STREAM))
#define GDK_IS_X11_SELECTION_OUTPUT_STREAM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GDK_TYPE_X11_SELECTION_OUTPUT_STREAM))
#define GDK_X11_SELECTION_OUTPUT_STREAM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_X11_SELECTION_OUTPUT_STREAM, GdkX11SelectionOutputStreamClass))

typedef struct _GdkX11PendingSelectionNotify GdkX11PendingSelectionNotify;

typedef struct GdkX11SelectionOutputStream         GdkX11SelectionOutputStream;
typedef struct GdkX11SelectionOutputStreamClass    GdkX11SelectionOutputStreamClass;

struct GdkX11SelectionOutputStream
{
  GOutputStream parent_instance;
};

struct GdkX11SelectionOutputStreamClass
{
  GOutputStreamClass parent_class;
};


GType           gdk_x11_selection_output_stream_get_type        (void) G_GNUC_CONST;

GOutputStream * gdk_x11_selection_output_stream_new             (GdkDisplay             *display,
                                                                 GdkX11PendingSelectionNotify *notify,
                                                                 Window                  window,
                                                                 const char             *selection,
                                                                 const char             *target,
                                                                 const char             *property,
                                                                 const char             *type,
                                                                 int                     format,
                                                                 gulong                  timestamp);

GdkX11PendingSelectionNotify *
                gdk_x11_pending_selection_notify_new            (Window                  window,
                                                                 Atom                    selection,
                                                                 Atom                    target,
                                                                 Atom                    property,
                                                                 Time                    timestamp);
void            gdk_x11_pending_selection_notify_require        (GdkX11PendingSelectionNotify *notify,
                                                                 guint                   n_sends);
void            gdk_x11_pending_selection_notify_send           (GdkX11PendingSelectionNotify *notify,
                                                                 GdkDisplay             *display,
                                                                 gboolean                success);

G_END_DECLS

#endif /* __GDK_X11_SELECTION_OUTPUT_STREAM_H__ */
