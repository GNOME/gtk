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
#ifndef __GTK_INPUTDIALOG_H__
#define __GTK_INPUTDIALOG_H__


#include <gdk/gdk.h>
#include <gtk/gtkdialog.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_INPUT_DIALOG(obj)          GTK_CHECK_CAST (obj, gtk_input_dialog_get_type (), GtkInputDialog)
#define GTK_INPUT_DIALOG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_input_dialog_get_type (), GtkInputDialogClass)
#define GTK_IS_INPUT_DIALOG(obj)       GTK_CHECK_TYPE (obj, gtk_input_dialog_get_type ())


typedef struct _GtkInputDialog       GtkInputDialog;
typedef struct _GtkInputDialogClass  GtkInputDialogClass;

struct _GtkInputDialog
{
  GtkDialog dialog;

  GtkWidget *axis_list;
  GtkWidget *axis_listbox;
  GtkWidget *mode_optionmenu;

  GtkWidget *close_button;
  GtkWidget *save_button;
  
  GtkWidget *axis_items[GDK_AXIS_LAST];
  guint32    current_device;

  GtkWidget *keys_list;
  GtkWidget *keys_listbox;
};

struct _GtkInputDialogClass
{
  GtkWindowClass parent_class;

  void (* enable_device)               (GtkInputDialog    *inputd,
					guint32            devid);
  void (* disable_device)              (GtkInputDialog    *inputd,
					guint32            devid);
};


guint      gtk_input_dialog_get_type     (void);
GtkWidget* gtk_input_dialog_new          (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_INPUTDIALOG_H__ */
