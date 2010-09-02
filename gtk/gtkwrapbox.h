/* 
 * Copyright (C) 2010 Openismus GmbH
 *
 * Authors:
 *      Tristan Van Berkom <tristanvb@openismus.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_WRAP_BOX_H__
#define __GTK_WRAP_BOX_H__

#include <gtk/gtkcontainer.h>

G_BEGIN_DECLS


#define GTK_TYPE_WRAP_BOX                  (gtk_wrap_box_get_type ())
#define GTK_WRAP_BOX(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_WRAP_BOX, GtkWrapBox))
#define GTK_WRAP_BOX_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_WRAP_BOX, GtkWrapBoxClass))
#define GTK_IS_WRAP_BOX(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_WRAP_BOX))
#define GTK_IS_WRAP_BOX_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_WRAP_BOX))
#define GTK_WRAP_BOX_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_WRAP_BOX, GtkWrapBoxClass))

typedef struct _GtkWrapBox            GtkWrapBox;
typedef struct _GtkWrapBoxPrivate     GtkWrapBoxPrivate;
typedef struct _GtkWrapBoxClass       GtkWrapBoxClass;

struct _GtkWrapBox
{
  GtkContainer container;

  /*< private >*/
  GtkWrapBoxPrivate *priv;
};

struct _GtkWrapBoxClass
{
  GtkContainerClass parent_class;
};

GType                 gtk_wrap_box_get_type                  (void) G_GNUC_CONST;

GtkWidget            *gtk_wrap_box_new                       (GtkWrapAllocationMode mode,
                                                              GtkWrapBoxSpreading   spreading,
                                                              guint                 horizontal_spacing,
                                                              guint                 vertical_spacing);
void                  gtk_wrap_box_set_allocation_mode       (GtkWrapBox           *layout,
                                                              GtkWrapAllocationMode mode);
GtkWrapAllocationMode gtk_wrap_box_get_allocation_mode       (GtkWrapBox           *layout);

void                  gtk_wrap_box_set_spreading             (GtkWrapBox           *layout,
                                                              GtkWrapBoxSpreading   spreading);
GtkWrapBoxSpreading   gtk_wrap_box_get_spreading             (GtkWrapBox           *layout);

void                  gtk_wrap_box_set_vertical_spacing      (GtkWrapBox           *layout, 
                                                              guint                 spacing);
guint                 gtk_wrap_box_get_vertical_spacing      (GtkWrapBox           *layout);

void                  gtk_wrap_box_set_horizontal_spacing    (GtkWrapBox           *layout, 
                                                              guint                 spacing);
guint                 gtk_wrap_box_get_horizontal_spacing    (GtkWrapBox           *layout);

void                  gtk_wrap_box_set_minimum_line_children (GtkWrapBox           *layout, 
                                                              guint                 n_children);
guint                 gtk_wrap_box_get_minimum_line_children (GtkWrapBox           *layout);

void                  gtk_wrap_box_set_natural_line_children (GtkWrapBox           *layout, 
                                                              guint                 n_children);
guint                 gtk_wrap_box_get_natural_line_children (GtkWrapBox           *layout);

void                  gtk_wrap_box_insert_child              (GtkWrapBox           *layout,
                                                              GtkWidget            *widget,
                                                              gint                  index,
                                                              guint                 xpad,
                                                              guint                 ypad,
                                                              gboolean              xexpand,
                                                              gboolean              yexpand,
                                                              gboolean              xfill,
                                                              gboolean              yfill);

void                  gtk_wrap_box_reorder_child             (GtkWrapBox           *layout,
                                                              GtkWidget            *widget,
                                                              guint                 index);

G_END_DECLS


#endif /* __GTK_WRAP_BOX_H__ */
