/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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

#ifndef __GTK_CLIPBOARD_WAYLAND_WAYLAND_PRIVATE_H__
#define __GTK_CLIPBOARD_WAYLAND_WAYLAND_PRIVATE_H__

#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_WAYLAND

#include <gdk/wayland/gdkwayland.h>
#include <gtk/gtkclipboardprivate.h>

#define GTK_TYPE_CLIPBOARD_WAYLAND            (gtk_clipboard_wayland_get_type ())
#define GTK_CLIPBOARD_WAYLAND(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CLIPBOARD_WAYLAND, GtkClipboardWayland))
#define GTK_IS_CLIPBOARD_WAYLAND(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CLIPBOARD_WAYLAND))
#define GTK_CLIPBOARD_WAYLAND_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CLIPBOARD_WAYLAND, GtkClipboardWaylandClass))
#define GTK_IS_CLIPBOARD_WAYLAND_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CLIPBOARD_WAYLAND))
#define GTK_CLIPBOARD_WAYLAND_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CLIPBOARD_WAYLAND, GtkClipboardWaylandClass))

typedef struct _GtkClipboardWayland GtkClipboardWayland;
typedef struct _GtkClipboardWaylandClass GtkClipboardWaylandClass;

typedef struct _SetContentClosure SetContentClosure;

struct _GtkClipboardWayland
{
  GtkClipboard parent_instance;

  SetContentClosure *last_closure;
};

struct _GtkClipboardWaylandClass
{
  GtkClipboardClass parent_class;
};

GType         gtk_clipboard_wayland_get_type (void) G_GNUC_CONST;

#endif /* GDK_WINDOWING_WAYLAND */

#endif /* __GTK_CLIPBOARD_WAYLAND_WAYLAND_PRIVATE_H__ */
