/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2014 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_OPTION_LIST_H__
#define __GTK_OPTION_LIST_H__

#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_OPTION_LIST                 (gtk_option_list_get_type ())
#define GTK_OPTION_LIST(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_OPTION_LIST, GtkOptionList))
#define GTK_OPTION_LIST_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_OPTION_LIST, GtkOptionListClass))
#define GTK_IS_OPTION_LIST(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_OPTION_LIST))
#define GTK_IS_OPTION_LIST_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_OPTION_LIST))
#define GTK_OPTION_LIST_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_OPTION_LIST, GtkOptionListClass))

typedef struct _GtkOptionList             GtkOptionList;
typedef struct _GtkOptionListClass        GtkOptionListClass;

GDK_AVAILABLE_IN_3_16
GType             gtk_option_list_get_type                (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_16
GtkWidget *       gtk_option_list_new                   (void);

GDK_AVAILABLE_IN_3_16
const gchar **    gtk_option_list_get_selected_items    (GtkOptionList    *list);

GDK_AVAILABLE_IN_3_16
void              gtk_option_list_select_item           (GtkOptionList    *list,
                                                         const gchar      *id);
GDK_AVAILABLE_IN_3_16
void              gtk_option_list_unselect_item         (GtkOptionList    *list,
                                                         const gchar      *id);

GDK_AVAILABLE_IN_3_16
void              gtk_option_list_add_item              (GtkOptionList    *list,
                                                         const gchar      *id,
                                                         const gchar      *text);
GDK_AVAILABLE_IN_3_16
void              gtk_option_list_remove_item           (GtkOptionList    *list,
                                                         const gchar      *id);

GDK_AVAILABLE_IN_3_16
const gchar *     gtk_option_list_item_get_text         (GtkOptionList    *list,
                                                         const gchar      *id);

GDK_AVAILABLE_IN_3_16
void              gtk_option_list_item_set_sort_key     (GtkOptionList    *list,
                                                         const gchar      *id,
                                                         const gchar      *sort);
GDK_AVAILABLE_IN_3_16
void              gtk_option_list_item_set_group_key    (GtkOptionList    *list,
                                                         const gchar      *id,
                                                         const gchar      *group);

GDK_AVAILABLE_IN_3_16
void              gtk_option_list_add_group             (GtkOptionList    *list,
                                                         const gchar      *group,
                                                         const gchar      *text,
                                                         const gchar      *sort);

GDK_AVAILABLE_IN_3_16
void              gtk_option_list_set_allow_custom      (GtkOptionList    *list,
                                                         gboolean          allow);
GDK_AVAILABLE_IN_3_16
gboolean          gtk_option_list_get_allow_custom      (GtkOptionList    *list);

GDK_AVAILABLE_IN_3_16
void              gtk_option_list_set_custom_text       (GtkOptionList    *list,
                                                         const gchar      *text);
GDK_AVAILABLE_IN_3_16
const gchar *     gtk_option_list_get_custom_text       (GtkOptionList    *list);

GDK_AVAILABLE_IN_3_16
void              gtk_option_list_set_selection_mode    (GtkOptionList    *list,
                                                         GtkSelectionMode  mode);
GDK_AVAILABLE_IN_3_16
GtkSelectionMode  gtk_option_list_get_selection_mode    (GtkOptionList    *list);

GDK_AVAILABLE_IN_3_16
gboolean          gtk_option_list_handle_key_event      (GtkOptionList    *list,
                                                         GdkEvent         *event);

G_END_DECLS

#endif /* __GTK_OPTION_LIST_H__ */
