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

#include "gtkdata.h"
#include "gtksignal.h"


enum {
  DISCONNECT,
  LAST_SIGNAL
};


static void gtk_data_class_init (GtkDataClass *klass);


static guint data_signals[LAST_SIGNAL] = { 0 };


GtkType
gtk_data_get_type (void)
{
  static GtkType data_type = 0;

  if (!data_type)
    {
      static const GtkTypeInfo data_info =
      {
	"GtkData",
	sizeof (GtkData),
	sizeof (GtkDataClass),
	(GtkClassInitFunc) gtk_data_class_init,
	(GtkObjectInitFunc) NULL,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      data_type = gtk_type_unique (gtk_object_get_type (), &data_info);
    }

  return data_type;
}

static void
gtk_data_class_init (GtkDataClass *class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;

  data_signals[DISCONNECT] =
    gtk_signal_new ("disconnect",
                    GTK_RUN_FIRST,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkDataClass, disconnect),
                    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, data_signals, LAST_SIGNAL);
}

