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
#ifndef __GTK_BUTTON_H__
#define __GTK_BUTTON_H__


#include <gdk/gdk.h>
#include <gtk/gtkcontainer.h>


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define GTK_TYPE_BUTTON			(gtk_button_get_type ())
#define GTK_BUTTON(obj)			(GTK_CHECK_CAST ((obj), GTK_TYPE_BUTTON, GtkButton))
#define GTK_BUTTON_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_BUTTON, GtkButtonClass))
#define GTK_IS_BUTTON(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_BUTTON))
#define GTK_IS_BUTTON_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_BUTTON))


typedef struct _GtkButton       GtkButton;
typedef struct _GtkButtonClass  GtkButtonClass;

struct _GtkButton
{
  GtkContainer container;

  GtkWidget *child;

  guint in_button : 1;
  guint button_down : 1;
};

struct _GtkButtonClass
{
  GtkContainerClass parent_class;

  void (* pressed)  (GtkButton *button);
  void (* released) (GtkButton *button);
  void (* clicked)  (GtkButton *button);
  void (* enter)    (GtkButton *button);
  void (* leave)    (GtkButton *button);
};


GtkType    gtk_button_get_type       (void);
GtkWidget* gtk_button_new            (void);
GtkWidget* gtk_button_new_with_label (const gchar *label);
void       gtk_button_pressed        (GtkButton *button);
void       gtk_button_released       (GtkButton *button);
void       gtk_button_clicked        (GtkButton *button);
void       gtk_button_enter          (GtkButton *button);
void       gtk_button_leave          (GtkButton *button);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_BUTTON_H__ */
