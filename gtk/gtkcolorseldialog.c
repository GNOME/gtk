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
#include "gtkcolorseldialog.h"
#include "gtkframe.h"
#include "gtkhbbox.h"
#include "gtkbutton.h"
#include "gtkstock.h"
#include "gtkintl.h"
#include "gtkalias.h"


/***************************/
/* GtkColorSelectionDialog */
/***************************/

G_DEFINE_TYPE (GtkColorSelectionDialog, gtk_color_selection_dialog, GTK_TYPE_DIALOG)

static void
gtk_color_selection_dialog_class_init (GtkColorSelectionDialogClass *klass)
{
}

static void
gtk_color_selection_dialog_init (GtkColorSelectionDialog *colorseldiag)
{
  GtkDialog *dialog = GTK_DIALOG (colorseldiag);

  gtk_dialog_set_has_separator (dialog, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
  gtk_box_set_spacing (GTK_BOX (dialog->vbox), 2); /* 2 * 5 + 2 = 12 */
  gtk_container_set_border_width (GTK_CONTAINER (dialog->action_area), 5);
  gtk_box_set_spacing (GTK_BOX (dialog->action_area), 6);

  colorseldiag->colorsel = gtk_color_selection_new ();
  gtk_container_set_border_width (GTK_CONTAINER (colorseldiag->colorsel), 5);
  gtk_color_selection_set_has_palette (GTK_COLOR_SELECTION(colorseldiag->colorsel), FALSE); 
  gtk_color_selection_set_has_opacity_control (GTK_COLOR_SELECTION(colorseldiag->colorsel), FALSE);
  gtk_container_add (GTK_CONTAINER (GTK_DIALOG (colorseldiag)->vbox), colorseldiag->colorsel);
  gtk_widget_show (colorseldiag->colorsel);
  
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

  _gtk_dialog_set_ignore_separator (dialog, TRUE);
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

#define __GTK_COLOR_SELECTION_DIALOG_C__
#include "gtkaliasdef.c"
