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
#ifndef __GTK_TOGGLE_BUTTON_H__
#define __GTK_TOGGLE_BUTTON_H__


#include <gdk/gdk.h>
#include <gtk/gtkbutton.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TOGGLE_BUTTON(obj)          GTK_CHECK_CAST (obj, gtk_toggle_button_get_type (), GtkToggleButton)
#define GTK_TOGGLE_BUTTON_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_toggle_button_get_type (), GtkToggleButtonClass)
#define GTK_IS_TOGGLE_BUTTON(obj)       GTK_CHECK_TYPE (obj, gtk_toggle_button_get_type ())


typedef struct _GtkToggleButton       GtkToggleButton;
typedef struct _GtkToggleButtonClass  GtkToggleButtonClass;

struct _GtkToggleButton
{
  GtkButton button;

  guint active : 1;
  guint draw_indicator : 1;
};

struct _GtkToggleButtonClass
{
  GtkButtonClass parent_class;

  void (* toggled) (GtkToggleButton *toggle_button);
};


guint      gtk_toggle_button_get_type       (void);
GtkWidget* gtk_toggle_button_new            (void);
GtkWidget* gtk_toggle_button_new_with_label (const gchar     *label);
void       gtk_toggle_button_set_mode       (GtkToggleButton *toggle_button,
					     gint             draw_indicator);
void       gtk_toggle_button_set_state      (GtkToggleButton *toggle_button,
					     gint             state);
void       gtk_toggle_button_toggled        (GtkToggleButton *toggle_button);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TOGGLE_BUTTON_H__ */
