/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998 Elliot Lee
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __GTK_COMBO_H__
#define __GTK_COMBO_H__


#include <gdk/gdk.h>
#include <gtk/gtkentry.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_COMBO_BOX(obj)          GTK_CHECK_CAST (obj, gtk_combo_box_get_type (), GtkComboBox)
#define GTK_COMBO_BOX_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_combo_box_get_type (), GtkComboBoxClass)
#define GTK_IS_COMBO_BOX(obj)       GTK_CHECK_TYPE (obj, gtk_combo_box_get_type ())


typedef struct _GtkComboBox       GtkComboBox;
typedef struct _GtkComboBoxClass  GtkComboBoxClass;

struct _GtkComboBox
{
  GtkEntry parent_widget;
  GtkWidget *popdown;
  gboolean menu_is_down;
};

struct _GtkComboBoxClass
{
  GtkEntryClass parent_class;
};


guint      gtk_combo_box_get_type     (void);
/* These GList's should be lists of strings that should be placed
   in the popdown menu */
GtkWidget* gtk_combo_box_new          (GList *popdown_strings);
GtkWidget* gtk_combo_box_new_with_max_length (GList *popdown_strings,
					      guint16 max);
void gtk_combo_box_set_popdown_strings(GtkComboBox *combobox,
				       GList *popdown_strings);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_ENTRY_H__ */
