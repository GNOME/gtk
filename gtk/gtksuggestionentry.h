/* GTK - The GIMP Toolkit
 * Copyright (C) 2020 Red Hat, Inc.
 *
 * Author: Matthias Clasen <mclasen@redhat.com>
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

#ifndef __GTK_SUGGESTION_ENTRY_H__
#define __GTK_SUGGESTION_ENTRY_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkfilter.h>
#include <gtk/gtkexpression.h>
#include <gtk/gtklistitemfactory.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS


#define GTK_TYPE_SUGGESTION_ENTRY               (gtk_suggestion_entry_get_type ())
#define GTK_SUGGESTION_ENTRY(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SUGGESTION_ENTRY, GtkSuggestionEntry))
#define GTK_IS_SUGGESTION_ENTRY(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SUGGESTION_ENTRY))

typedef struct _GtkSuggestionEntry       GtkSuggestionEntry;

GDK_AVAILABLE_IN_ALL
GType           gtk_suggestion_entry_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkWidget*      gtk_suggestion_entry_new                (void);

GDK_AVAILABLE_IN_ALL
void            gtk_suggestion_entry_set_model          (GtkSuggestionEntry     *self,
                                                         GListModel             *model);
GDK_AVAILABLE_IN_ALL
GListModel *    gtk_suggestion_entry_get_model          (GtkSuggestionEntry     *self);

GDK_AVAILABLE_IN_ALL
void            gtk_suggestion_entry_set_from_strings   (GtkSuggestionEntry     *self,
                                                         const char            **strings);

GDK_AVAILABLE_IN_ALL
void            gtk_suggestion_entry_set_factory        (GtkSuggestionEntry     *self,
                                                         GtkListItemFactory     *factory);
GDK_AVAILABLE_IN_ALL
GtkListItemFactory *
                gtk_suggestion_entry_get_factory        (GtkSuggestionEntry     *self);

GDK_AVAILABLE_IN_ALL
void            gtk_suggestion_entry_set_use_filter     (GtkSuggestionEntry     *self,
                                                         gboolean                use_ilter);
GDK_AVAILABLE_IN_ALL
gboolean        gtk_suggestion_entry_get_use_filter     (GtkSuggestionEntry     *self);

GDK_AVAILABLE_IN_ALL
void            gtk_suggestion_entry_set_expression     (GtkSuggestionEntry     *self,
                                                         GtkExpression          *expression);
GDK_AVAILABLE_IN_ALL
GtkExpression * gtk_suggestion_entry_get_expression     (GtkSuggestionEntry     *self);

GDK_AVAILABLE_IN_ALL
void            gtk_suggestion_entry_set_insert_selection (GtkSuggestionEntry  *self,
                                                           gboolean             insert_selection);
GDK_AVAILABLE_IN_ALL
gboolean        gtk_suggestion_entry_get_insert_selection (GtkSuggestionEntry  *self);

GDK_AVAILABLE_IN_ALL
void            gtk_suggestion_entry_set_insert_prefix    (GtkSuggestionEntry  *self,
                                                           gboolean             insert_prefix);
GDK_AVAILABLE_IN_ALL
gboolean        gtk_suggestion_entry_get_insert_prefix    (GtkSuggestionEntry  *self);


GDK_AVAILABLE_IN_ALL
void            gtk_suggestion_entry_set_show_button      (GtkSuggestionEntry  *self,
                                                           gboolean             show_button);
GDK_AVAILABLE_IN_ALL
gboolean        gtk_suggestion_entry_get_show_button      (GtkSuggestionEntry  *self);

GDK_AVAILABLE_IN_ALL
void            gtk_suggestion_entry_set_minimum_length   (GtkSuggestionEntry  *self,
                                                           guint                minimum_length);
GDK_AVAILABLE_IN_ALL
guint           gtk_suggestion_entry_get_minimum_length   (GtkSuggestionEntry  *self);


G_END_DECLS

#endif /* __GTK_SUGGESTION_ENTRY_H__ */
