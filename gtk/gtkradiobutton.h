/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GTK_RADIO_BUTTON_H__
#define __GTK_RADIO_BUTTON_H__


#include <gdk/gdk.h>
#include <gtk/gtkcheckbutton.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_RADIO_BUTTON(obj)          GTK_CHECK_CAST (obj, gtk_radio_button_get_type (), GtkRadioButton)
#define GTK_RADIO_BUTTON_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_radio_button_get_type (), GtkRadioButtonClass)
#define GTK_IS_RADIO_BUTTON(obj)       GTK_CHECK_TYPE (obj, gtk_radio_button_get_type ())


typedef struct _GtkRadioButton       GtkRadioButton;
typedef struct _GtkRadioButtonClass  GtkRadioButtonClass;

struct _GtkRadioButton
{
  GtkCheckButton check_button;

  GSList *group;
};

struct _GtkRadioButtonClass
{
  GtkCheckButtonClass parent_class;
};


guint       gtk_radio_button_get_type        (void);
GtkWidget*  gtk_radio_button_new             (GSList         *group);
GtkWidget*  gtk_radio_button_new_from_widget (GtkRadioButton *group);
GtkWidget*  gtk_radio_button_new_with_label  (GSList         *group,
					      const gchar    *label);
GtkWidget*  gtk_radio_button_new_with_label_from_widget (GtkRadioButton *group,
							 const gchar    *label);
GSList*     gtk_radio_button_group           (GtkRadioButton *radio_button);
void        gtk_radio_button_set_group       (GtkRadioButton *radio_button,
					      GSList         *group);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_RADIO_BUTTON_H__ */
