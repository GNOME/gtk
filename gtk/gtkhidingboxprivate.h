/*
 * Copyright (C) 2015 Rafał Lużyński <digitalfreak@lingonborough.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __GTK_HIDING_BOX_PRIVATE_H__
#define __GTK_HIDING_BOX_PRIVATE_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkcontainer.h>

G_BEGIN_DECLS

#define GTK_TYPE_HIDING_BOX            (gtk_hiding_box_get_type())
#define GTK_HIDING_BOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_HIDING_BOX, GtkHidingBox))
#define GTK_HIDING_BOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_HIDING_BOX, GtkHidingBoxClass))
#define GTK_IS_HIDING_BOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_HIDING_BOX))
#define GTK_IS_HIDING_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_HIDING_BOX)
#define GTK_HIDING_BOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_HIDING_BOX, GtkHidingBoxClass))

typedef struct _GtkHidingBox GtkHidingBox;
typedef struct _GtkHidingBoxClass GtkHidingBoxClass;
typedef struct _GtkHidingBoxPrivate GtkHidingBoxPrivate;

struct _GtkHidingBoxClass
{
  GtkContainerClass parent;

  /* Padding for future expansion */
  gpointer reserved[10];
};

struct _GtkHidingBox
{
  GtkContainer parent_instance;
};

GDK_AVAILABLE_IN_3_20
GType             gtk_hiding_box_get_type                (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_20
GtkWidget        *gtk_hiding_box_new                     (void);
GDK_AVAILABLE_IN_3_20
void              gtk_hiding_box_set_spacing             (GtkHidingBox      *box,
                                                          gint               spacing);
GDK_AVAILABLE_IN_3_20
gint              gtk_hiding_box_get_spacing             (GtkHidingBox      *box);

GDK_AVAILABLE_IN_3_20
void              gtk_hiding_box_set_inverted            (GtkHidingBox      *box,
                                                          gboolean           inverted);
GDK_AVAILABLE_IN_3_20
gboolean          gtk_hiding_box_get_inverted            (GtkHidingBox      *box);

GDK_AVAILABLE_IN_3_20
GList             *gtk_hiding_box_get_overflow_children  (GtkHidingBox      *box);
G_END_DECLS

#endif /* GTK_HIDING_BOX_PRIVATE_H_ */
