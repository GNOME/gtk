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
#include <config.h>
#include <glib.h>
#include "gtkalias.h"
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

GType
gtk_color_selection_dialog_get_type (void)
{
  static GType color_selection_dialog_type = 0;
  
  if (!color_selection_dialog_type)
    {
      static const GTypeInfo colorsel_diag_info =
      {
	sizeof (GtkColorSelectionDialogClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_color_selection_dialog_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkColorSelectionDialog),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_color_selection_dialog_init,
      };
      
      color_selection_dialog_type =
	g_type_register_static (GTK_TYPE_DIALOG, "GtkColorSelectionDialog",
				&colorsel_diag_info, 0);
    }
  
  return color_selection_dialog_type;
}

static void
gtk_color_selection_dialog_class_init (GtkColorSelectionDialogClass *klass)
{
  color_selection_dialog_parent_class = g_type_class_peek_parent (klass);
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
  gtk_color_selection_set_has_palette (GTK_COLOR_SELECTION(colorseldiag->colorsel), FALSE); 
  gtk_color_selection_set_has_opacity_control (GTK_COLOR_SELECTION(colorseldiag->colorsel), FALSE);
  gtk_container_add (GTK_CONTAINER (frame), colorseldiag->colorsel);
  gtk_widget_show (colorseldiag->colorsel);
  
  action_area_button_box = GTK_DIALOG (colorseldiag)->action_area;

  colorseldiag->cancel_button = gtk_dialog_add_button (GTK_DIALOG (colorseldiag),
                                                       GTK_STOCK_CANCEL,
                                                       GTK_RESPONSE_CANCEL);

  colorseldiag->ok_button = gtk_dialog_add_button (GTK_DIALOG (colorseldiag),
                                                   GTK_STOCK_OK,
                                                   GTK_RESPONSE_OK);
                                                   
  gtk_widget_grab_default (colorseldiag->ok_button);
  
  colorseldiag->help_button = gtk_dialog_add_button (GTK_DIALOG (colorseldiag),
                                                     GTK_STOCK_HELP,
                                                     GTK_RESPONSE_HELP);

  gtk_widget_hide (colorseldiag->help_button);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (colorseldiag),
					   GTK_RESPONSE_OK,
					   GTK_RESPONSE_CANCEL,
					   GTK_RESPONSE_HELP,
					   -1);

  gtk_window_set_title (GTK_WINDOW (colorseldiag),
                        _("Color Selection"));
}

GtkWidget*
gtk_color_selection_dialog_new (const gchar *title)
{
  GtkColorSelectionDialog *colorseldiag;
  
  colorseldiag = g_object_new (GTK_TYPE_COLOR_SELECTION_DIALOG, NULL);

  if (title)
    gtk_window_set_title (GTK_WINDOW (colorseldiag), title);

  gtk_window_set_resizable (GTK_WINDOW (colorseldiag), FALSE);
  
  return GTK_WIDGET (colorseldiag);
}

