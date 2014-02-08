/*
 * Copyright (c) 2013 - 2014 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __GTK_CENTER_BOX_H__
#define __GTK_CENTER_BOX_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkcontainer.h>

G_BEGIN_DECLS

#define GTK_TYPE_CENTER_BOX            (gtk_center_box_get_type ())
#define GTK_CENTER_BOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CENTER_BOX, GtkCenterBox))
#define GTK_CENTER_BOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CENTER_BOX, GtkCenterBoxClass))
#define GTK_IS_CENTER_BOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CENTER_BOX))
#define GTK_IS_CENTER_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CENTER_BOX))
#define GTK_CENTER_BOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CENTER_BOX, GtkCenterBoxClass))

typedef struct _GtkCenterBox              GtkCenterBox;
typedef struct _GtkCenterBoxPrivate       GtkCenterBoxPrivate;
typedef struct _GtkCenterBoxClass         GtkCenterBoxClass;

struct _GtkCenterBox
{
  /*< private >*/
  GtkContainer container;
};

struct _GtkCenterBoxClass
{
  /*< private >*/
  GtkContainerClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GDK_AVAILABLE_IN_3_12
GType        gtk_center_box_get_type          (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_3_12
GtkWidget   *gtk_center_box_new               (void);
GDK_AVAILABLE_IN_3_12
GtkWidget   *gtk_center_box_get_center_widget (GtkCenterBox *box);
GDK_AVAILABLE_IN_3_12
void         gtk_center_box_set_center_widget (GtkCenterBox *box,
                                               GtkWidget    *center_widget);

GDK_AVAILABLE_IN_3_12
void         gtk_center_box_pack_start        (GtkCenterBox *box,
                                               GtkWidget    *child);
GDK_AVAILABLE_IN_3_12
void         gtk_center_box_pack_end          (GtkCenterBox *box,
                                               GtkWidget    *child);
G_END_DECLS

#endif /* __GTK_CENTER_BOX_H__ */
