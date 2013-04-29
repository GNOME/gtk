/* gtkpathbar.h
 * Copyright (C) 2004  Red Hat, Inc.,  Jonathan Blandford <jrb@gnome.org>
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

#ifndef __GTK_PATH_BAR_H__
#define __GTK_PATH_BAR_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkcontainer.h>
#include <gtk/gtkfilesystem.h>
#include <gtk/gtkplacessidebar.h>

G_BEGIN_DECLS

typedef struct _GtkPathBar        GtkPathBar;
typedef struct _GtkPathBarClass   GtkPathBarClass;
typedef struct _GtkPathBarPrivate GtkPathBarPrivate;


#define GTK_TYPE_PATH_BAR                 (gtk_path_bar_get_type ())
#define GTK_PATH_BAR(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PATH_BAR, GtkPathBar))
#define GTK_PATH_BAR_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PATH_BAR, GtkPathBarClass))
#define GTK_IS_PATH_BAR(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PATH_BAR))
#define GTK_IS_PATH_BAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PATH_BAR))
#define GTK_PATH_BAR_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PATH_BAR, GtkPathBarClass))

struct _GtkPathBar
{
  GtkContainer parent;

  GtkPathBarPrivate *priv;
};

struct _GtkPathBarClass
{
  GtkContainerClass parent_class;

  void (* open_location) (GtkPathBar  *path_bar,
			  GFile       *file,
			  GFile       *child_file,
			  gboolean     child_is_hidden);
};

GType    gtk_path_bar_get_type (void) G_GNUC_CONST;

GtkWidget *gtk_path_bar_new (void);

GtkPlacesOpenFlags gtk_path_bar_get_open_flags (GtkPathBar *path_bar);
void gtk_path_bar_set_open_flags (GtkPathBar *path_bar, GtkPlacesOpenFlags flags);

void gtk_path_bar_set_location (GtkPathBar *path_bar,
				GFile      *location,
				gboolean    keep_trail);

void     _gtk_path_bar_set_file_system (GtkPathBar         *path_bar,
					GtkFileSystem      *file_system);
void     _gtk_path_bar_up              (GtkPathBar *path_bar);
void     _gtk_path_bar_down            (GtkPathBar *path_bar);

G_END_DECLS

#endif /* __GTK_PATH_BAR_H__ */
