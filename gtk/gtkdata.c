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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "gtkdata.h"
#include "gtksignal.h"


enum {
  DISCONNECT,
  LAST_SIGNAL
};


static void gtk_data_class_init (GtkDataClass *klass);


static gint data_signals[LAST_SIGNAL] = { 0 };


guint
gtk_data_get_type ()
{
  static guint data_type = 0;

  if (!data_type)
    {
      GtkTypeInfo data_info =
      {
	"GtkData",
	sizeof (GtkData),
	sizeof (GtkDataClass),
	(GtkClassInitFunc) gtk_data_class_init,
	(GtkObjectInitFunc) NULL,
	(GtkArgSetFunc) NULL,
        (GtkArgGetFunc) NULL,
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
                    gtk_signal_default_marshaller,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, data_signals, LAST_SIGNAL);
}
