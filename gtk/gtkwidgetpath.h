/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_WIDGET_PATH_H__
#define __GTK_WIDGET_PATH_H__

#include <glib-object.h>
#include "gtkenums.h"

G_BEGIN_DECLS

typedef struct GtkWidgetPath GtkWidgetPath;


GtkWidgetPath * gtk_widget_path_new (void);

GtkWidgetPath * gtk_widget_path_copy                (const GtkWidgetPath *path);
void            gtk_widget_path_free                (GtkWidgetPath       *path);

guint           gtk_widget_path_length              (GtkWidgetPath       *path);

guint           gtk_widget_path_prepend_type        (GtkWidgetPath       *path,
                                                     GType                type);

GType               gtk_widget_path_get_element_type (GtkWidgetPath      *path,
                                                      guint               pos);
void                gtk_widget_path_set_element_type (GtkWidgetPath      *path,
                                                      guint               pos,
                                                      GType               type);

G_CONST_RETURN gchar * gtk_widget_path_get_element_name (GtkWidgetPath   *path,
                                                         guint            pos);
void                   gtk_widget_path_set_element_name (GtkWidgetPath   *path,
                                                         guint            pos,
                                                         const gchar     *name);

void     gtk_widget_path_iter_add_region    (GtkWidgetPath      *path,
                                             guint               pos,
                                             const gchar        *name,
                                             GtkChildClassFlags  flags);
void     gtk_widget_path_iter_remove_region (GtkWidgetPath      *path,
                                             guint               pos,
                                             const gchar        *name);
void     gtk_widget_path_iter_clear_regions (GtkWidgetPath      *path,
                                             guint               pos);

GSList * gtk_widget_path_iter_list_regions  (GtkWidgetPath      *path,
                                             guint               pos);

gboolean gtk_widget_path_iter_has_region    (GtkWidgetPath      *path,
                                             guint               pos,
                                             const gchar        *name,
                                             GtkChildClassFlags *flags);

gboolean        gtk_widget_path_has_parent          (const GtkWidgetPath *path,
                                                     GType                type);

G_END_DECLS

#endif /* __GTK_WIDGET_PATH_H__ */
