/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 1998, 1999 Red Hat, Inc.
 * All rights reserved.
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Author: James Henstridge <james@daa.com.au>
 *
 * Modified by the GTK+ Team and others 2003.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */
#ifndef __GTK_MENU_MERGE_H__
#define __GTK_MENU_MERGE_H__


#include <glib.h>
#include <glib-object.h>
#include <gtk/gtkaccelgroup.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkaction.h>
#include <gtk/gtkactiongroup.h>

#define GTK_TYPE_MENU_MERGE            (gtk_menu_merge_get_type ())
#define GTK_MENU_MERGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_MENU_MERGE, GtkMenuMerge))
#define GTK_MENU_MERGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_MENU_MERGE, GtkMenuMergeClass))
#define GTK_IS_MENU_MERGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_MENU_MERGE))
#define GTK_IS_MENU_MERGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), GTK_TYPE_MENU_MERGE))
#define GTK_MENU_MERGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_MENU_MERGE, GtkMenuMergeClass))

typedef struct _GtkMenuMerge      GtkMenuMerge;
typedef struct _GtkMenuMergeClass GtkMenuMergeClass;
typedef struct _GtkMenuMergePrivate GtkMenuMergePrivate;


struct _GtkMenuMerge {
  GObject parent;

  /*< private >*/

  GtkMenuMergePrivate *private_data;
};

struct _GtkMenuMergeClass {
  GObjectClass parent_class;

  void (* add_widget)    (GtkMenuMerge *merge, 
                          GtkWidget    *widget);
  void (* remove_widget) (GtkMenuMerge *merge, 
                          GtkWidget    *widget);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GType         gtk_menu_merge_get_type            (void);
GtkMenuMerge *gtk_menu_merge_new                 (void);

/* these two functions will dirty all merge nodes, as they may need to
 * be connected up to different actions */
void          gtk_menu_merge_insert_action_group (GtkMenuMerge   *self,
						  GtkActionGroup *action_group,
						  gint            pos);
void          gtk_menu_merge_remove_action_group (GtkMenuMerge   *self,
						  GtkActionGroup *action_group);
GList        *gtk_menu_merge_get_action_groups   (GtkMenuMerge   *self);
GtkAccelGroup *gtk_menu_merge_get_accel_group    (GtkMenuMerge   *self);



GtkWidget    *gtk_menu_merge_get_widget          (GtkMenuMerge   *self,
						  const gchar    *path);

/* these two functions are for adding UI elements to the merged user
 * interface */
guint         gtk_menu_merge_add_ui_from_string  (GtkMenuMerge   *self,
						  const gchar    *buffer,
						  gsize           length,
						  GError        **error);
guint         gtk_menu_merge_add_ui_from_file    (GtkMenuMerge   *self,
						  const gchar    *filename,
						  GError        **error);
void          gtk_menu_merge_remove_ui           (GtkMenuMerge   *self,
						  guint           merge_id);

gchar        *gtk_menu_merge_get_ui              (GtkMenuMerge   *self);


#endif /* __GTK_MENU_MERGE_H__ */
