/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_BUTTON_BOX_H__
#define __GTK_BUTTON_BOX_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkbox.h>


G_BEGIN_DECLS  

#define GTK_TYPE_BUTTON_BOX             (gtk_button_box_get_type ())
#define GTK_BUTTON_BOX(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_BUTTON_BOX, GtkButtonBox))
#define GTK_BUTTON_BOX_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_BUTTON_BOX, GtkButtonBoxClass))
#define GTK_IS_BUTTON_BOX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_BUTTON_BOX))
#define GTK_IS_BUTTON_BOX_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_BUTTON_BOX))
#define GTK_BUTTON_BOX_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_BUTTON_BOX, GtkButtonBoxClass))


typedef struct _GtkButtonBox              GtkButtonBox;
typedef struct _GtkButtonBoxPrivate       GtkButtonBoxPrivate;
typedef struct _GtkButtonBoxClass         GtkButtonBoxClass;

struct _GtkButtonBox
{
  GtkBox box;

  /*< private >*/
  GtkButtonBoxPrivate *priv;
};

struct _GtkButtonBoxClass
{
  GtkBoxClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};


GType             gtk_button_box_get_type            (void) G_GNUC_CONST;
GtkWidget       * gtk_button_box_new                 (GtkOrientation     orientation);
GtkButtonBoxStyle gtk_button_box_get_layout          (GtkButtonBox      *widget);
void              gtk_button_box_set_layout          (GtkButtonBox      *widget,
                                                      GtkButtonBoxStyle  layout_style);
gboolean          gtk_button_box_get_child_secondary (GtkButtonBox      *widget,
                                                      GtkWidget         *child);
void              gtk_button_box_set_child_secondary (GtkButtonBox      *widget,
                                                      GtkWidget         *child,
                                                      gboolean           is_secondary);
GDK_AVAILABLE_IN_3_2
gboolean          gtk_button_box_get_child_non_homogeneous (GtkButtonBox *widget,
                                                            GtkWidget    *child);
GDK_AVAILABLE_IN_3_2
void              gtk_button_box_set_child_non_homogeneous (GtkButtonBox *widget,
                                                            GtkWidget    *child,
                                                            gboolean      non_homogeneous);


G_END_DECLS

#endif /* __GTK_BUTTON_BOX_H__ */
