/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gtkorientable.h
 * Copyright (C) 2008 Imendio AB
 * Contact: Michael Natterer <mitch@imendio.com>
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

#ifndef __GTK_ORIENTABLE_H__
#define __GTK_ORIENTABLE_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_ORIENTABLE (gtk_orientable_get_type ())
GDK_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (GtkOrientable, gtk_orientable, GTK, ORIENTABLE, GObject)

struct _GtkOrientableInterface
{
  GTypeInterface base_iface;
};


GDK_AVAILABLE_IN_ALL
void           gtk_orientable_set_orientation (GtkOrientable  *orientable,
                                               GtkOrientation  orientation);
GDK_AVAILABLE_IN_ALL
GtkOrientation gtk_orientable_get_orientation (GtkOrientable  *orientable);

G_END_DECLS

#endif /* __GTK_ORIENTABLE_H__ */
