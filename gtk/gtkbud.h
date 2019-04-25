/*
 * Copyright © 2019 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __GTK_BUD_H__
#define __GTK_BUD_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_BUD               (gtk_bud_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (GtkBud, gtk_bud, GTK, BUD, GtkWidget)

/**
 * GtkBudIface:
 *
 * The list of functions that must be implemented for the #GtkBud interface.
 */
struct _GtkBudInterface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/
  GskRenderer * (* get_renderer)          (GtkBud    *self);

  void          (* get_surface_transform) (GtkBud    *self,
                                           int       *x,
                                           int       *y);

  void          (* check_resize)          (GtkBud    *self);
};

GDK_AVAILABLE_IN_ALL
GtkWidget * gtk_bud_get_for_surface (GdkSurface *surface);

GDK_AVAILABLE_IN_ALL
void        gtk_bud_check_resize (GtkBud *self);


G_END_DECLS

#endif /* __GTK_BUD_H__ */
