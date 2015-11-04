/*
 * Copyright (C) 2016 Carlos Soriano <csoriano@gnome.org>
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

#ifndef __GTK_PATH_BAR_CONTAINER_H__
#define __GTK_PATH_BAR_CONTAINER_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkbin.h>

G_BEGIN_DECLS

#define GTK_TYPE_PATH_BAR_CONTAINER            (gtk_path_bar_container_get_type())
#define GTK_PATH_BAR_CONTAINER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PATH_BAR_CONTAINER, GtkPathBarContainer))
#define GTK_PATH_BAR_CONTAINER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PATH_BAR_CONTAINER, GtkPathBarContainerClass))
#define GTK_IS_PATH_BAR_CONTAINER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PATH_BAR_CONTAINER))
#define GTK_IS_PATH_BAR_CONTAINER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PATH_BAR_CONTAINER)
#define GTK_PATH_BAR_CONTAINER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PATH_BAR_CONTAINER, GtkPathBarContainerClass))

typedef struct _GtkPathBarContainer GtkPathBarContainer;
typedef struct _GtkPathBarContainerClass GtkPathBarContainerClass;
typedef struct _GtkPathBarContainerPrivate GtkPathBarContainerPrivate;

struct _GtkPathBarContainerClass
{
  GtkBinClass parent;

  /* Get the preferred size for a specific allocation, assuming the container
   * manages overflow. The return value indicates whether the container children
   * overflow and the container reports preferred size for the non overflowing
   * children or the ones the container consider that are going to be visible
   */
  void            (* get_preferred_size_for_requisition)         (GtkWidget                *widget,
                                                                  GtkRequisition           *available_size,
                                                                  GtkRequisition           *minimum_size,
                                                                  GtkRequisition           *natural_size,
                                                                  GtkRequisition           *distributed_size);

  void            (* invert_animation_done)                      (GtkPathBarContainer      *self);

  /* Padding for future expansion */
  gpointer reserved[10];
};

struct _GtkPathBarContainer
{
  GtkBin parent_instance;
};

GDK_AVAILABLE_IN_3_20
GType             gtk_path_bar_container_get_type                (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_20
GtkWidget        *gtk_path_bar_container_new                     (void);

GDK_AVAILABLE_IN_3_20
void              gtk_path_bar_container_set_spacing             (GtkPathBarContainer      *box,
                                                                  gint                      spacing);

GDK_AVAILABLE_IN_3_20
gint              gtk_path_bar_container_get_spacing             (GtkPathBarContainer      *box);

GDK_AVAILABLE_IN_3_20
void              gtk_path_bar_container_set_inverted            (GtkPathBarContainer      *box,
                                                                  gboolean                  inverted);

GDK_AVAILABLE_IN_3_20
gboolean          gtk_path_bar_container_get_inverted            (GtkPathBarContainer      *box);

GDK_AVAILABLE_IN_3_20
GList             *gtk_path_bar_container_get_overflow_children  (GtkPathBarContainer      *box);

GDK_AVAILABLE_IN_3_20
void              gtk_path_bar_container_add                     (GtkPathBarContainer      *self,
                                                                  GtkWidget                *widget);

GDK_AVAILABLE_IN_3_20
void              gtk_path_bar_container_remove                  (GtkPathBarContainer      *self,
                                                                  GtkWidget                *widget);

GDK_AVAILABLE_IN_3_20
void              gtk_path_bar_container_remove_all_children     (GtkPathBarContainer      *self);

GDK_AVAILABLE_IN_3_20
GList*            gtk_path_bar_container_get_children            (GtkPathBarContainer      *self);

GDK_AVAILABLE_IN_3_20
gint              gtk_path_bar_container_get_unused_width        (GtkPathBarContainer      *self);

GDK_AVAILABLE_IN_3_20
void              gtk_path_bar_container_get_preferred_size_for_requisition         (GtkWidget                *widget,
                                                                                     GtkRequisition           *available_size,
                                                                                     GtkRequisition           *minimum_size,
                                                                                     GtkRequisition           *natural_size,
                                                                                     GtkRequisition           *distributed_size);
GDK_AVAILABLE_IN_3_20
void              gtk_path_bar_container_adapt_to_size                              (GtkPathBarContainer      *self,
                                                                                     GtkRequisition           *available_size);
GDK_AVAILABLE_IN_3_20
GList *           gtk_path_bar_container_get_shown_children                         (GtkPathBarContainer      *self);

G_END_DECLS

#endif /* GTK_PATH_BAR_CONTAINER_H_ */
