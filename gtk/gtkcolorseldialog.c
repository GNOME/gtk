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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */
#include <glib.h>
#include "gtkcolorseldialog.h"
#include "gtkframe.h"
#include "gtkhbbox.h"
#include "gtkbutton.h"
#include "gtkstock.h"
#include "gtkintl.h"


static void gtk_color_selection_dialog_class_init (GtkColorSelectionDialogClass *klass);
static void gtk_color_selection_dialog_init (GtkColorSelectionDialog *colorseldiag);

static GtkWindowClass *color_selection_dialog_parent_class = NULL;


/***************************/
/* GtkColorSelectionDialog */
/***************************/

GtkType
gtk_color_selection_dialog_get_type (void)
{
  static GtkType color_selection_dialog_type = 0;
  
  if (!color_selection_dialog_type)
    {
      GtkTypeInfo colorsel_diag_info =
      {
	"GtkColorSelectionDialog",
	sizeof (GtkColorSelectionDialog),
	sizeof (GtkColorSelectionDialogClass),
	(GtkClassInitFunc) gtk_color_selection_dialog_class_init,
	(GtkObjectInitFunc) gtk_color_selection_dialog_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };
      
      color_selection_dialog_type = gtk_type_unique (GTK_TYPE_DIALOG, &colorsel_diag_info);
    }
  
  return color_selection_dialog_type;
}

static void
gtk_color_selection_dialog_class_init (GtkColorSelectionDialogClass *klass)
{
  GtkObjectClass *object_class;
  
  object_class = (GtkObjectClass*) klass;
  
  color_selection_dialog_parent_class = gtk_type_class (GTK_TYPE_DIALOG);
}

static void
gtk_color_selection_dialog_init (GtkColorSelectionDialog *colorseldiag)
{
  GtkWidget *action_area_button_box, *frame;  
  
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (colorseldiag)->vbox), frame);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 10); 
  gtk_widget_show (frame); 
  
  colorseldiag->colorsel = gtk_color_selection_new ();
  gtk_color_selection_set_use_palette (GTK_COLOR_SELECTION(colorseldiag->colorsel), FALSE); 
  gtk_color_selection_set_use_opacity (GTK_COLOR_SELECTION(colorseldiag->colorsel), FALSE);
  gtk_container_add (GTK_CONTAINER (frame), colorseldiag->colorsel);
  gtk_widget_show (colorseldiag->colorsel);
  
  action_area_button_box = GTK_DIALOG (colorseldiag)->action_area;

  colorseldiag->ok_button = gtk_dialog_add_button (GTK_DIALOG (colorseldiag),
                                                   GTK_STOCK_BUTTON_OK,
                                                   GTK_RESPONSE_OK);
                                                   
  gtk_widget_grab_default (colorseldiag->ok_button);
  
  colorseldiag->cancel_button = gtk_dialog_add_button (GTK_DIALOG (colorseldiag),
                                                       GTK_STOCK_BUTTON_CANCEL,
                                                       GTK_RESPONSE_CANCEL);
  
  colorseldiag->help_button = gtk_dialog_add_button (GTK_DIALOG (colorseldiag),
                                                     GTK_STOCK_HELP,
                                                     GTK_RESPONSE_HELP);
}

GtkWidget*
gtk_color_selection_dialog_new (const gchar *title)
{
  GtkColorSelectionDialog *colorseldiag;
  
  colorseldiag = gtk_type_new (GTK_TYPE_COLOR_SELECTION_DIALOG);
  gtk_window_set_title (GTK_WINDOW (colorseldiag), title);
  gtk_window_set_policy(GTK_WINDOW (colorseldiag), FALSE, FALSE, TRUE);
  
  return GTK_WIDGET (colorseldiag);
}
