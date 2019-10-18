/* gtkshortcutlabelprivate.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_SHORTCUT_LABEL_H__
#define __GTK_SHORTCUT_LABEL_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_SHORTCUT_LABEL (gtk_shortcut_label_get_type())
#define GTK_SHORTCUT_LABEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SHORTCUT_LABEL, GtkShortcutLabel))
#define GTK_IS_SHORTCUT_LABEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SHORTCUT_LABEL))


typedef struct _GtkShortcutLabel      GtkShortcutLabel;
typedef struct _GtkShortcutLabelClass GtkShortcutLabelClass;

GDK_AVAILABLE_IN_ALL
GType        gtk_shortcut_label_get_type        (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkWidget   *gtk_shortcut_label_new             (const gchar      *accelerator);

GDK_AVAILABLE_IN_ALL
const gchar *gtk_shortcut_label_get_accelerator (GtkShortcutLabel *self);

GDK_AVAILABLE_IN_ALL
void         gtk_shortcut_label_set_accelerator (GtkShortcutLabel *self,
                                                 const gchar      *accelerator);

GDK_AVAILABLE_IN_ALL
const gchar *gtk_shortcut_label_get_disabled_text (GtkShortcutLabel *self);

GDK_AVAILABLE_IN_ALL
void         gtk_shortcut_label_set_disabled_text (GtkShortcutLabel *self,
                                                   const gchar      *disabled_text);

G_END_DECLS

#endif /* __GTK_SHORTCUT_LABEL_H__ */
