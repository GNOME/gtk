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

#include "gtkbutton.h"
#include "gtkdialog.h"
#include "gtkhbox.h"
#include "gtkhseparator.h"
#include "gtkvbox.h"


static void gtk_dialog_class_init (GtkDialogClass *klass);
static void gtk_dialog_init       (GtkDialog      *dialog);


GtkType
gtk_dialog_get_type (void)
{
  static GtkType dialog_type = 0;

  if (!dialog_type)
    {
      static const GtkTypeInfo dialog_info =
      {
	"GtkDialog",
	sizeof (GtkDialog),
	sizeof (GtkDialogClass),
	(GtkClassInitFunc) gtk_dialog_class_init,
	(GtkObjectInitFunc) gtk_dialog_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      dialog_type = gtk_type_unique (GTK_TYPE_WINDOW, &dialog_info);
    }

  return dialog_type;
}

static void
gtk_dialog_class_init (GtkDialogClass *class)
{
}

static void
gtk_dialog_init (GtkDialog *dialog)
{
  GtkWidget *separator;

  dialog->vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (dialog), dialog->vbox);
  gtk_widget_show (dialog->vbox);

  dialog->action_area = gtk_hbox_new (TRUE, 5);
  gtk_container_set_border_width (GTK_CONTAINER (dialog->action_area), 10);
  gtk_box_pack_end (GTK_BOX (dialog->vbox), dialog->action_area, FALSE, TRUE, 0);
  gtk_widget_show (dialog->action_area);

  separator = gtk_hseparator_new ();
  gtk_box_pack_end (GTK_BOX (dialog->vbox), separator, FALSE, TRUE, 0);
  gtk_widget_show (separator);
}

GtkWidget*
gtk_dialog_new (void)
{
  return GTK_WIDGET (gtk_type_new (GTK_TYPE_DIALOG));
}
