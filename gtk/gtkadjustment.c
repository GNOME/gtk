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

#include "gtkadjustment.h"
#include "gtksignal.h"


enum {
  CHANGED,
  VALUE_CHANGED,
  LAST_SIGNAL
};


static void gtk_adjustment_class_init (GtkAdjustmentClass *klass);
static void gtk_adjustment_init       (GtkAdjustment      *adjustment);


static guint adjustment_signals[LAST_SIGNAL] = { 0 };


GtkType
gtk_adjustment_get_type (void)
{
  static GtkType adjustment_type = 0;

  if (!adjustment_type)
    {
      static const GtkTypeInfo adjustment_info =
      {
	"GtkAdjustment",
	sizeof (GtkAdjustment),
	sizeof (GtkAdjustmentClass),
	(GtkClassInitFunc) gtk_adjustment_class_init,
	(GtkObjectInitFunc) gtk_adjustment_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      adjustment_type = gtk_type_unique (GTK_TYPE_DATA, &adjustment_info);
    }

  return adjustment_type;
}

static void
gtk_adjustment_class_init (GtkAdjustmentClass *class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;

  adjustment_signals[CHANGED] =
    gtk_signal_new ("changed",
                    GTK_RUN_FIRST | GTK_RUN_NO_RECURSE,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkAdjustmentClass, changed),
                    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  adjustment_signals[VALUE_CHANGED] =
    gtk_signal_new ("value_changed",
                    GTK_RUN_FIRST | GTK_RUN_NO_RECURSE,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkAdjustmentClass, value_changed),
                    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, adjustment_signals, LAST_SIGNAL);

  class->changed = NULL;
  class->value_changed = NULL;
}

static void
gtk_adjustment_init (GtkAdjustment *adjustment)
{
  adjustment->value = 0.0;
  adjustment->lower = 0.0;
  adjustment->upper = 0.0;
  adjustment->step_increment = 0.0;
  adjustment->page_increment = 0.0;
  adjustment->page_size = 0.0;
}

GtkObject*
gtk_adjustment_new (gfloat value,
		    gfloat lower,
		    gfloat upper,
		    gfloat step_increment,
		    gfloat page_increment,
		    gfloat page_size)
{
  GtkAdjustment *adjustment;

  adjustment = gtk_type_new (gtk_adjustment_get_type ());

  adjustment->value = value;
  adjustment->lower = lower;
  adjustment->upper = upper;
  adjustment->step_increment = step_increment;
  adjustment->page_increment = page_increment;
  adjustment->page_size = page_size;

  return GTK_OBJECT (adjustment);
}

void
gtk_adjustment_set_value (GtkAdjustment        *adjustment,
			  gfloat                value)
{
  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  value = CLAMP (value, adjustment->lower, adjustment->upper);

  if (value != adjustment->value)
    {
      adjustment->value = value;

      gtk_adjustment_value_changed (adjustment);
    }
}

void
gtk_adjustment_changed (GtkAdjustment        *adjustment)
{
  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  gtk_signal_emit (GTK_OBJECT (adjustment), adjustment_signals[CHANGED]);
}

void
gtk_adjustment_value_changed (GtkAdjustment        *adjustment)
{
  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  gtk_signal_emit (GTK_OBJECT (adjustment), adjustment_signals[VALUE_CHANGED]);
}

void
gtk_adjustment_clamp_page (GtkAdjustment *adjustment,
			   gfloat         lower,
			   gfloat         upper)
{
  gint need_emission;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  lower = CLAMP (lower, adjustment->lower, adjustment->upper);
  upper = CLAMP (upper, adjustment->lower, adjustment->upper);

  need_emission = FALSE;

  if (adjustment->value + adjustment->page_size < upper)
    {
      adjustment->value = upper - adjustment->page_size;
      need_emission = TRUE;
    }
  if (adjustment->value > lower)
    {
      adjustment->value = lower;
      need_emission = TRUE;
    }

  if (need_emission)
    gtk_adjustment_value_changed (adjustment);
}
