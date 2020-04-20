/* GTK - The GIMP Toolkit
 * Copyright 2001 Sun Microsystems Inc.
 * Copyright 2020 GNOME Foundation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_ACCESSIBLE_H__
#define __GTK_ACCESSIBLE_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <atk/atk.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_ACCESSIBLE                  (gtk_accessible_get_type ())

#if !ATK_CHECK_VERSION(2, 38, 0)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(AtkObject, g_object_unref)
#endif

GDK_AVAILABLE_IN_ALL
G_DECLARE_DERIVABLE_TYPE (GtkAccessible, gtk_accessible, GTK, ACCESSIBLE, AtkObject)

struct _GtkAccessibleClass
{
  AtkObjectClass parent_class;

  void (* widget_set)   (GtkAccessible *accessible);
  void (* widget_unset) (GtkAccessible *accessible);

  gpointer _padding[8];
};

GDK_AVAILABLE_IN_ALL
void       gtk_accessible_set_widget               (GtkAccessible *accessible,
                                                    GtkWidget     *widget);
GDK_AVAILABLE_IN_ALL
GtkWidget *gtk_accessible_get_widget               (GtkAccessible *accessible);

G_END_DECLS

#endif /* __GTK_ACCESSIBLE_H__ */
