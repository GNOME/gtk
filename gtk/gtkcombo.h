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

#ifndef __GTK_COMBO_H__
#define __GTK_COMBO_H__

#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_COMBO                 (gtk_combo_get_type ())
#define GTK_COMBO(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_COMBO, GtkCombo))
#define GTK_COMBO_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_COMBO, GtkComboClass))
#define GTK_IS_COMBO(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_COMBO))
#define GTK_IS_COMBO_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_COMBO))
#define GTK_COMBO_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_COMBO, GtkComboClass))

typedef struct _GtkCombo             GtkCombo;
typedef struct _GtkComboClass        GtkComboClass;

GDK_AVAILABLE_IN_3_16
GType         gtk_combo_get_type                (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_16
GtkWidget *   gtk_combo_new                     (void);

GDK_AVAILABLE_IN_3_16
const gchar * gtk_combo_get_active              (GtkCombo       *combo);

GDK_AVAILABLE_IN_3_16
void          gtk_combo_set_active              (GtkCombo       *combo,
                                                 const gchar    *id);

GDK_AVAILABLE_IN_3_16
void          gtk_combo_add_item                (GtkCombo       *combo,
                                                 const gchar    *id,
                                                 const gchar    *text);
GDK_AVAILABLE_IN_3_16
void          gtk_combo_remove_item             (GtkCombo       *combo,
                                                 const gchar    *id);

GDK_AVAILABLE_IN_3_16
const gchar  *gtk_combo_item_get_text           (GtkCombo       *combo,
                                                 const gchar    *id);

GDK_AVAILABLE_IN_3_16
void          gtk_combo_item_set_sort_key       (GtkCombo       *combo,
                                                 const gchar    *id,
                                                 const gchar    *sort);
GDK_AVAILABLE_IN_3_16
void          gtk_combo_item_set_group_key      (GtkCombo       *combo,
                                                 const gchar    *id,
                                                 const gchar    *group);

GDK_AVAILABLE_IN_3_16
void          gtk_combo_set_placeholder_text    (GtkCombo       *combo,
                                                 const gchar    *text);
GDK_AVAILABLE_IN_3_16
const gchar * gtk_combo_get_placeholder_text    (GtkCombo       *combo);

GDK_AVAILABLE_IN_3_16
void          gtk_combo_set_allow_custom        (GtkCombo       *combo,
                                                 gboolean        allow);
GDK_AVAILABLE_IN_3_16
gboolean      gtk_combo_get_allow_custom        (GtkCombo       *combo);

GDK_AVAILABLE_IN_3_16
void          gtk_combo_set_custom_text         (GtkCombo       *combo,
                                                 const gchar    *text);
GDK_AVAILABLE_IN_3_16
const gchar * gtk_combo_get_custom_text         (GtkCombo       *combo);

GDK_AVAILABLE_IN_3_16
void          gtk_combo_add_group               (GtkCombo       *combo,
                                                 const gchar    *group,
                                                 const gchar    *text,
                                                 const gchar    *sort);

G_END_DECLS

#endif /* __GTK_COMBO_H__ */
