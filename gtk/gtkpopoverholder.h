/* GTK - The GIMP Toolkit
 * Copyright (C) 2020 Red Hat, Inc.
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

#ifndef __GTK_POPOVER_HOLDER_H__
#define __GTK_POPOVER_HOLDER_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>
#include <gtk/gtkpopover.h>

G_BEGIN_DECLS

#define GTK_TYPE_POPOVER_HOLDER                  (gtk_popover_holder_get_type ())
#define GTK_POPOVER_HOLDER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_POPOVER_HOLDER, GtkPopoverHolder))
#define GTK_POPOVER_HOLDER_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_POPOVER_HOLDER, GtkPopoverHolderClass))
#define GTK_IS_POPOVER_HOLDER(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_POPOVER_HOLDER))
#define GTK_IS_POPOVER_HOLDER_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_POPOVER_HOLDER))
#define GTK_POPOVER_HOLDER_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_POPOVER_HOLDER, GtkPopoverHolderClass))


typedef struct _GtkPopoverHolder              GtkPopoverHolder;
typedef struct _GtkPopoverHolderClass         GtkPopoverHolderClass;


GDK_AVAILABLE_IN_ALL
GType           gtk_popover_holder_get_type     (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkWidget *     gtk_popover_holder_new          (void);

GDK_AVAILABLE_IN_ALL
GtkWidget *     gtk_popover_holder_get_child    (GtkPopoverHolder *self);

GDK_AVAILABLE_IN_ALL
void            gtk_popover_holder_set_child    (GtkPopoverHolder *self,
                                                 GtkWidget        *widget);

GDK_AVAILABLE_IN_ALL
GtkPopover *    gtk_popover_holder_get_popover  (GtkPopoverHolder *self);

GDK_AVAILABLE_IN_ALL
void            gtk_popover_holder_set_popover  (GtkPopoverHolder *self,
                                                 GtkPopover       *popover);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtkPopoverHolder, g_object_unref)

G_END_DECLS

#endif /* __GTK_POPOVER_HOLDER_H__ */
