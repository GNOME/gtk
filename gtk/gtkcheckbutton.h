/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_CHECK_BUTTON_H__
#define __GTK_CHECK_BUTTON_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtktogglebutton.h>


G_BEGIN_DECLS

#define GTK_TYPE_CHECK_BUTTON                  (gtk_check_button_get_type ())
#define GTK_CHECK_BUTTON(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CHECK_BUTTON, GtkCheckButton))
#define GTK_CHECK_BUTTON_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CHECK_BUTTON, GtkCheckButtonClass))
#define GTK_IS_CHECK_BUTTON(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CHECK_BUTTON))
#define GTK_IS_CHECK_BUTTON_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CHECK_BUTTON))
#define GTK_CHECK_BUTTON_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CHECK_BUTTON, GtkCheckButtonClass))


typedef struct _GtkCheckButton       GtkCheckButton;
typedef struct _GtkCheckButtonClass  GtkCheckButtonClass;

struct _GtkCheckButton
{
  GtkToggleButton toggle_button;
};

struct _GtkCheckButtonClass
{
  GtkToggleButtonClass parent_class;

  /*< private >*/

  gpointer padding[8];
};


GDK_AVAILABLE_IN_ALL
GType      gtk_check_button_get_type       (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GtkWidget* gtk_check_button_new               (void);
GDK_AVAILABLE_IN_ALL
GtkWidget* gtk_check_button_new_with_label    (const gchar *label);
GDK_AVAILABLE_IN_ALL
GtkWidget* gtk_check_button_new_with_mnemonic (const gchar *label);
GDK_AVAILABLE_IN_ALL
void       gtk_check_button_set_draw_indicator (GtkCheckButton *check_button,
                                                gboolean        draw_indicator);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_check_button_get_draw_indicator (GtkCheckButton *check_button);
GDK_AVAILABLE_IN_ALL
void       gtk_check_button_set_inconsistent   (GtkCheckButton *check_button,
                                                gboolean        inconsistent);
GDK_AVAILABLE_IN_ALL
gboolean   gtk_check_button_get_inconsistent   (GtkCheckButton *check_button);

G_END_DECLS

#endif /* __GTK_CHECK_BUTTON_H__ */
