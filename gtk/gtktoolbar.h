/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * GtkToolbar copyright (C) Federico Mena
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

#ifndef __GTK_TOOLBAR_H__
#define __GTK_TOOLBAR_H__


#include <gdk/gdk.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktooltips.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_TOOLBAR                  (gtk_toolbar_get_type ())
#define GTK_TOOLBAR(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_TOOLBAR, GtkToolbar))
#define GTK_TOOLBAR_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_TOOLBAR, GtkToolbarClass))
#define GTK_IS_TOOLBAR(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_TOOLBAR))
#define GTK_IS_TOOLBAR_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TOOLBAR))

typedef enum
{
  GTK_TOOLBAR_CHILD_SPACE,
  GTK_TOOLBAR_CHILD_BUTTON,
  GTK_TOOLBAR_CHILD_TOGGLEBUTTON,
  GTK_TOOLBAR_CHILD_RADIOBUTTON,
  GTK_TOOLBAR_CHILD_WIDGET
} GtkToolbarChildType;

typedef enum
{
  GTK_TOOLBAR_SPACE_EMPTY,
  GTK_TOOLBAR_SPACE_LINE
} GtkToolbarSpaceStyle;

typedef struct _GtkToolbarChild      GtkToolbarChild;
typedef struct _GtkToolbar           GtkToolbar;
typedef struct _GtkToolbarClass      GtkToolbarClass;

struct _GtkToolbarChild
{
  GtkToolbarChildType type;
  GtkWidget *widget;
  GtkWidget *icon;
  GtkWidget *label;
};

struct _GtkToolbar
{
  GtkContainer container;

  gint             num_children;
  GList           *children;
  GtkOrientation   orientation;
  GtkToolbarStyle  style;
  gint             space_size; /* big optional space between buttons */
  GtkToolbarSpaceStyle space_style;

  GtkTooltips     *tooltips;

  gint             button_maxw;
  gint             button_maxh;
  GtkReliefStyle   relief;
};

struct _GtkToolbarClass
{
  GtkContainerClass parent_class;

  void (* orientation_changed) (GtkToolbar      *toolbar,
				GtkOrientation   orientation);
  void (* style_changed)       (GtkToolbar      *toolbar,
				GtkToolbarStyle  style);
};


GtkType    gtk_toolbar_get_type        (void);
GtkWidget* gtk_toolbar_new             (GtkOrientation   orientation,
					GtkToolbarStyle  style);

/* Simple button items */
GtkWidget* gtk_toolbar_append_item     (GtkToolbar      *toolbar,
					const char      *text,
					const char      *tooltip_text,
					const char      *tooltip_private_text,
					GtkWidget       *icon,
					GtkSignalFunc    callback,
					gpointer         user_data);
GtkWidget* gtk_toolbar_prepend_item    (GtkToolbar      *toolbar,
					const char      *text,
					const char      *tooltip_text,
					const char      *tooltip_private_text,
					GtkWidget       *icon,
					GtkSignalFunc    callback,
					gpointer         user_data);
GtkWidget* gtk_toolbar_insert_item     (GtkToolbar      *toolbar,
					const char      *text,
					const char      *tooltip_text,
					const char      *tooltip_private_text,
					GtkWidget       *icon,
					GtkSignalFunc    callback,
					gpointer         user_data,
					gint             position);

/* Space Items */
void       gtk_toolbar_append_space    (GtkToolbar      *toolbar);
void       gtk_toolbar_prepend_space   (GtkToolbar      *toolbar);
void       gtk_toolbar_insert_space    (GtkToolbar      *toolbar,
					gint             position);

/* Any element type */
GtkWidget* gtk_toolbar_append_element  (GtkToolbar      *toolbar,
					GtkToolbarChildType type,
					GtkWidget       *widget,
					const char      *text,
					const char      *tooltip_text,
					const char      *tooltip_private_text,
					GtkWidget       *icon,
					GtkSignalFunc    callback,
					gpointer         user_data);

GtkWidget* gtk_toolbar_prepend_element (GtkToolbar      *toolbar,
					GtkToolbarChildType type,
					GtkWidget       *widget,
					const char      *text,
					const char      *tooltip_text,
					const char      *tooltip_private_text,
					GtkWidget       *icon,
					GtkSignalFunc    callback,
					gpointer         user_data);

GtkWidget* gtk_toolbar_insert_element  (GtkToolbar      *toolbar,
					GtkToolbarChildType type,
					GtkWidget       *widget,
					const char      *text,
					const char      *tooltip_text,
					const char      *tooltip_private_text,
					GtkWidget       *icon,
					GtkSignalFunc    callback,
					gpointer         user_data,
					gint             position);

/* Generic Widgets */
void       gtk_toolbar_append_widget   (GtkToolbar      *toolbar,
					GtkWidget       *widget,
					const char      *tooltip_text,
					const char      *tooltip_private_text);
void       gtk_toolbar_prepend_widget  (GtkToolbar      *toolbar,
					GtkWidget       *widget,
					const char      *tooltip_text,
					const char	*tooltip_private_text);
void       gtk_toolbar_insert_widget   (GtkToolbar      *toolbar,
					GtkWidget       *widget,
					const char      *tooltip_text,
					const char      *tooltip_private_text,
					gint             position);

/* Style functions */
void       gtk_toolbar_set_orientation       (GtkToolbar           *toolbar,
					      GtkOrientation        orientation);
void       gtk_toolbar_set_style             (GtkToolbar           *toolbar,
					      GtkToolbarStyle       style);
void       gtk_toolbar_set_space_size        (GtkToolbar           *toolbar,
					      gint                  space_size);
void       gtk_toolbar_set_space_style       (GtkToolbar           *toolbar,
					      GtkToolbarSpaceStyle  space_style);
void       gtk_toolbar_set_tooltips          (GtkToolbar           *toolbar,
					      gint                  enable);
void       gtk_toolbar_set_button_relief     (GtkToolbar           *toolbar,
					      GtkReliefStyle        relief);
GtkReliefStyle gtk_toolbar_get_button_relief (GtkToolbar           *toolbar);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_TOOLBAR_H__ */
