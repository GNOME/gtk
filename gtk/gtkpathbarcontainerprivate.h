/* gtkpathbarcontainerprivate.h
 *
 * Copyright (C) 2015 Red Hat
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Carlos Soriano <csoriano@gnome.org>
 */

#ifndef __GTK_PATH_BAR_CONTAINER_PRIVATE_H__
#define __GTK_PATH_BAR_CONTAINER_PRIVATE_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkcontainer.h>

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
  GtkContainerClass parent;

  /* Padding for future expansion */
  gpointer reserved[10];
};

struct _GtkPathBarContainer
{
  GtkContainer parent_instance;
};

GType             gtk_path_bar_container_get_type                (void) G_GNUC_CONST;

GtkWidget        *gtk_path_bar_container_new                     (void);

GtkWidget        *gtk_path_bar_container_get_path_box            (GtkPathBarContainer       *path_bar_container);

GtkWidget        *gtk_path_bar_container_get_overflow_button     (GtkPathBarContainer       *path_bar_container);

G_END_DECLS

#endif /* GTK_PATH_BAR_CONTAINER_PRIVATE_H_ */
