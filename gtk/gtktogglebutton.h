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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_TOGGLE_BUTTON_H__
#define __GTK_TOGGLE_BUTTON_H__


#include <gdk/gdk.h>
#include <gtk/gtkbutton.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_TOGGLE_BUTTON                  (gtk_toggle_button_get_type ())
#define GTK_TOGGLE_BUTTON(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_TOGGLE_BUTTON, GtkToggleButton))
#define GTK_TOGGLE_BUTTON_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TOGGLE_BUTTON, GtkToggleButtonClass))
#define GTK_IS_TOGGLE_BUTTON(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_TOGGLE_BUTTON))
#define GTK_IS_TOGGLE_BUTTON_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TOGGLE_BUTTON))


typedef struct _GtkToggleButton       GtkToggleButton;
typedef struct _GtkToggleButtonClass  GtkToggleButtonClass;

struct _GtkToggleButton
{
  GtkButton button;

  guint active : 1;
  guint draw_indicator : 1;
  
  GdkWindow *event_window;
};

struct _GtkToggleButtonClass
{
  GtkButtonClass parent_class;

  void (* toggled) (GtkToggleButton *toggle_button);
};


GtkType    gtk_toggle_button_get_type       (void);
GtkWidget* gtk_toggle_button_new            (void);
GtkWidget* gtk_toggle_button_new_with_label (const gchar     *label);
void       gtk_toggle_button_set_mode       (GtkToggleButton *toggle_button,
					     gboolean         draw_indicator);
void       gtk_toggle_button_set_active     (GtkToggleButton *toggle_button,
					     gboolean         is_active);
gboolean   gtk_toggle_button_get_active	    (GtkToggleButton *toggle_button);
void       gtk_toggle_button_toggled        (GtkToggleButton *toggle_button);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_TOGGLE_BUTTON_H__ */
