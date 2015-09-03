/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Red Hat, Inc.
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

#ifndef __GTK_MULTI_CHOICE_PRIVATE_H__
#define __GTK_MULTI_CHOICE_PRIVATE_H__

#include <gtk/gtkbin.h>

G_BEGIN_DECLS

#define GTK_TYPE_MULTI_CHOICE                  (gtk_multi_choice_get_type ())
#define GTK_MULTI_CHOICE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_MULTI_CHOICE, GtkMultiChoice))
#define GTK_MULTI_CHOICE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_MULTI_CHOICE, GtkMultiChoiceClass))
#define GTK_IS_MULTI_CHOICE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_MULTI_CHOICE))
#define GTK_IS_MULTI_CHOICE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_MULTI_CHOICE))
#define GTK_MULTI_CHOICE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_MULTI_CHOICE, GtkMultiChoiceClass))


typedef struct _GtkMultiChoice        GtkMultiChoice;
typedef struct _GtkMultiChoiceClass   GtkMultiChoiceClass;

GType      gtk_multi_choice_get_type            (void) G_GNUC_CONST;

GtkWidget *gtk_multi_choice_new                 (void);
void       gtk_multi_choice_set_value           (GtkMultiChoice               *choice,
                                                 gint                          value);
gint       gtk_multi_choice_get_value           (GtkMultiChoice               *choice);
void       gtk_multi_choice_set_choices         (GtkMultiChoice               *choice,
                                                 const gchar                 **choices);

typedef gchar * (* GtkMultiChoiceFormatCallback) (GtkMultiChoice *choice,
                                                  gint            value,
                                                  gpointer        user_data);

void       gtk_multi_choice_set_format_callback (GtkMultiChoice               *choice,
                                                 GtkMultiChoiceFormatCallback  callback,
                                                 gpointer                      user_data,
                                                 GDestroyNotify                notify);

G_END_DECLS

#endif /* __GTK_MULTI_CHOICE_PRIVATE_H__ */
