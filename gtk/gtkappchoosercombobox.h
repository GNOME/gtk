/*
 * gtkappchoosercombobox.h: an app-chooser combobox
 *
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Cosimo Cecchi <ccecchi@redhat.com>
 */

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_APP_CHOOSER_COMBO_BOX_H__
#define __GTK_APP_CHOOSER_COMBO_BOX_H__

#include <gtk/gtkcombobox.h>
#include <gio/gio.h>

#define GTK_TYPE_APP_CHOOSER_COMBO_BOX\
  (gtk_app_chooser_combo_box_get_type ())
#define GTK_APP_CHOOSER_COMBO_BOX(obj)\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_APP_CHOOSER_COMBO_BOX, GtkAppChooserComboBox))
#define GTK_APP_CHOOSER_COMBO_BOX_CLASS(klass)\
  (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_APP_CHOOSER_COMBO_BOX, GtkAppChooserComboBoxClass))
#define GTK_IS_APP_CHOOSER_COMBO_BOX(obj)\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_APP_CHOOSER_COMBO_BOX))
#define GTK_IS_APP_CHOOSER_COMBO_BOX_CLASS(klass)\
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_APP_CHOOSER_COMBO_BOX))
#define GTK_APP_CHOOSER_COMBO_BOX_GET_CLASS(obj)\
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_APP_CHOOSER_COMBO_BOX, GtkAppChooserComboBoxClass))

typedef struct _GtkAppChooserComboBox        GtkAppChooserComboBox;
typedef struct _GtkAppChooserComboBoxClass   GtkAppChooserComboBoxClass;
typedef struct _GtkAppChooserComboBoxPrivate GtkAppChooserComboBoxPrivate;

typedef void (* GtkAppChooserComboBoxItemFunc) (GtkAppChooserComboBox *self,
						gpointer user_data);

struct _GtkAppChooserComboBox {
  GtkComboBox parent;

  /*< private >*/
  GtkAppChooserComboBoxPrivate *priv;
};

struct _GtkAppChooserComboBoxClass {
  GtkComboBoxClass parent_class;

  /* padding for future class expansion */
  gpointer padding[16];
};

GType      gtk_app_chooser_combo_box_get_type (void) G_GNUC_CONST;

GtkWidget * gtk_app_chooser_combo_box_new (const gchar *content_type);

void gtk_app_chooser_combo_box_append_separator (GtkAppChooserComboBox *self);
void gtk_app_chooser_combo_box_append_custom_item (GtkAppChooserComboBox *self,
						   const gchar *label,
						   GIcon *icon,
						   GtkAppChooserComboBoxItemFunc func,
						   gpointer user_data);

#endif /* __GTK_APP_CHOOSER_COMBO_BOX_H__ */
