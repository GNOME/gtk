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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_WIDGET_PATH_H__
#define __GTK_WIDGET_PATH_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <glib-object.h>
#include <gdk/gdk.h>
#include <gtk/gtkenums.h>
#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_WIDGET_PATH (gtk_widget_path_get_type ())

GType           gtk_widget_path_get_type            (void) G_GNUC_CONST;
GtkWidgetPath * gtk_widget_path_new                 (void);

GtkWidgetPath * gtk_widget_path_copy                (const GtkWidgetPath *path);
GDK_AVAILABLE_IN_3_2
GtkWidgetPath * gtk_widget_path_ref                 (GtkWidgetPath       *path);
GDK_AVAILABLE_IN_3_2
void            gtk_widget_path_unref               (GtkWidgetPath       *path);
void            gtk_widget_path_free                (GtkWidgetPath       *path);

GDK_AVAILABLE_IN_3_2
char *          gtk_widget_path_to_string           (const GtkWidgetPath *path);
gint            gtk_widget_path_length              (const GtkWidgetPath *path);

gint            gtk_widget_path_append_type         (GtkWidgetPath       *path,
                                                     GType                type);
void            gtk_widget_path_prepend_type        (GtkWidgetPath       *path,
                                                     GType                type);
GDK_AVAILABLE_IN_3_2
gint            gtk_widget_path_append_with_siblings(GtkWidgetPath       *path,
                                                     GtkWidgetPath       *siblings,
                                                     guint                sibling_index);
/* gtk_widget_path_append_for_widget() is declared in gtkwidget.c */
GDK_AVAILABLE_IN_3_2
gint            gtk_widget_path_append_for_widget   (GtkWidgetPath       *path,
                                                     GtkWidget           *widget);

GType               gtk_widget_path_iter_get_object_type  (const GtkWidgetPath *path,
                                                           gint                 pos);
void                gtk_widget_path_iter_set_object_type  (GtkWidgetPath       *path,
                                                           gint                 pos,
                                                           GType                type);
const GtkWidgetPath *
                    gtk_widget_path_iter_get_siblings     (const GtkWidgetPath *path,
                                                           gint                 pos);
guint               gtk_widget_path_iter_get_sibling_index(const GtkWidgetPath *path,
                                                           gint                 pos);

const gchar *          gtk_widget_path_iter_get_name  (const GtkWidgetPath *path,
                                                       gint                 pos);
void                   gtk_widget_path_iter_set_name  (GtkWidgetPath       *path,
                                                       gint                 pos,
                                                       const gchar         *name);
gboolean               gtk_widget_path_iter_has_name  (const GtkWidgetPath *path,
                                                       gint                 pos,
                                                       const gchar         *name);
gboolean               gtk_widget_path_iter_has_qname (const GtkWidgetPath *path,
                                                       gint                 pos,
                                                       GQuark               qname);

void     gtk_widget_path_iter_add_class     (GtkWidgetPath       *path,
                                             gint                 pos,
                                             const gchar         *name);
void     gtk_widget_path_iter_remove_class  (GtkWidgetPath       *path,
                                             gint                 pos,
                                             const gchar         *name);
void     gtk_widget_path_iter_clear_classes (GtkWidgetPath       *path,
                                             gint                 pos);
GSList * gtk_widget_path_iter_list_classes  (const GtkWidgetPath *path,
                                             gint                 pos);
gboolean gtk_widget_path_iter_has_class     (const GtkWidgetPath *path,
                                             gint                 pos,
                                             const gchar         *name);
gboolean gtk_widget_path_iter_has_qclass    (const GtkWidgetPath *path,
                                             gint                 pos,
                                             GQuark               qname);

void     gtk_widget_path_iter_add_region    (GtkWidgetPath      *path,
                                             gint                pos,
                                             const gchar        *name,
                                             GtkRegionFlags     flags);
void     gtk_widget_path_iter_remove_region (GtkWidgetPath      *path,
                                             gint                pos,
                                             const gchar        *name);
void     gtk_widget_path_iter_clear_regions (GtkWidgetPath      *path,
                                             gint                pos);

GSList * gtk_widget_path_iter_list_regions  (const GtkWidgetPath *path,
                                             gint                 pos);

gboolean gtk_widget_path_iter_has_region    (const GtkWidgetPath *path,
                                             gint                 pos,
                                             const gchar         *name,
                                             GtkRegionFlags      *flags);
gboolean gtk_widget_path_iter_has_qregion   (const GtkWidgetPath *path,
                                             gint                 pos,
                                             GQuark               qname,
                                             GtkRegionFlags      *flags);

GType           gtk_widget_path_get_object_type (const GtkWidgetPath *path);

gboolean        gtk_widget_path_is_type    (const GtkWidgetPath *path,
                                            GType                type);
gboolean        gtk_widget_path_has_parent (const GtkWidgetPath *path,
                                            GType                type);

G_END_DECLS

#endif /* __GTK_WIDGET_PATH_H__ */
