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

#include "gtkadjustment.h"
#include "gtkmarshalers.h"


enum {
  CHANGED,
  VALUE_CHANGED,
  LAST_SIGNAL
};


static void gtk_adjustment_class_init (GtkAdjustmentClass *klass);
static void gtk_adjustment_init       (GtkAdjustment      *adjustment);


static guint adjustment_signals[LAST_SIGNAL] = { 0 };


GType
gtk_adjustment_get_type (void)
{
  static GType adjustment_type = 0;

  if (!adjustment_type)
    {
      static const GTypeInfo adjustment_info =
      {
	sizeof (GtkAdjustmentClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_adjustment_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkAdjustment),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_adjustment_init,
      };

      adjustment_type =
	g_type_register_static (GTK_TYPE_OBJECT, "GtkAdjustment",
				&adjustment_info, 0);
    }

  return adjustment_type;
}

static void
gtk_adjustment_class_init (GtkAdjustmentClass *class)
{
  class->changed = NULL;
  class->value_changed = NULL;

  adjustment_signals[CHANGED] =
    g_signal_new ("changed",
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (GtkAdjustmentClass, changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  adjustment_signals[VALUE_CHANGED] =
    g_signal_new ("value_changed",
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (GtkAdjustmentClass, value_changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
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
gtk_adjustment_new (gdouble value,
		    gdouble lower,
		    gdouble upper,
		    gdouble step_increment,
		    gdouble page_increment,
		    gdouble page_size)
{
  GtkAdjustment *adjustment;

  adjustment = g_object_new (GTK_TYPE_ADJUSTMENT, NULL);

  adjustment->value = value;
  adjustment->lower = lower;
  adjustment->upper = upper;
  adjustment->step_increment = step_increment;
  adjustment->page_increment = page_increment;
  adjustment->page_size = page_size;

  return GTK_OBJECT (adjustment);
}

/**
 * gtk_adjustment_get_value:
 * @adjustment: a #GtkAdjustment
 *
 * Gets the current value of the adjustment. See
 * gtk_adjustment_set_value ().
 *
 * Return value: The current value of the adjustment.
 **/
gdouble
gtk_adjustment_get_value (GtkAdjustment *adjustment)
{
  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), 0.);

  return adjustment->value;
}

void
gtk_adjustment_set_value (GtkAdjustment        *adjustment,
			  gdouble               value)
{
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
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  g_signal_emit (adjustment, adjustment_signals[CHANGED], 0);
}

void
gtk_adjustment_value_changed (GtkAdjustment        *adjustment)
{
  g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  g_signal_emit (adjustment, adjustment_signals[VALUE_CHANGED], 0);
}

void
gtk_adjustment_clamp_page (GtkAdjustment *adjustment,
			   gdouble        lower,
			   gdouble        upper)
{
  gboolean need_emission;

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
