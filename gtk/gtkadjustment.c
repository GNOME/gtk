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
#include "gtkadjustment.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

enum
{
  PROP_0,
  PROP_VALUE,
  PROP_LOWER,
  PROP_UPPER,
  PROP_STEP_INCREMENT,
  PROP_PAGE_INCREMENT,
  PROP_PAGE_SIZE
};

enum {
  CHANGED,
  VALUE_CHANGED,
  LAST_SIGNAL
};


static void gtk_adjustment_get_property (GObject      *object,
                                         guint         prop_id,
                                         GValue       *value,
                                         GParamSpec   *pspec);
static void gtk_adjustment_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec);

static guint adjustment_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GtkAdjustment, gtk_adjustment, GTK_TYPE_OBJECT)

static void
gtk_adjustment_class_init (GtkAdjustmentClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->set_property = gtk_adjustment_set_property;
  gobject_class->get_property = gtk_adjustment_get_property;

  class->changed = NULL;
  class->value_changed = NULL;

  /**
   * GtkAdjustment:value:
   * 
   * The value of the adjustment.
   * 
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_VALUE,
                                   g_param_spec_double ("value",
							P_("Value"),
							P_("The value of the adjustment"),
							-G_MAXDOUBLE, 
							G_MAXDOUBLE, 
							0.0, 
							GTK_PARAM_READWRITE));
  
  /**
   * GtkAdjustment:lower:
   * 
   * The minimum value of the adjustment.
   * 
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_LOWER,
                                   g_param_spec_double ("lower",
							P_("Minimum Value"),
							P_("The minimum value of the adjustment"),
							-G_MAXDOUBLE, 
							G_MAXDOUBLE, 
							0.0,
							GTK_PARAM_READWRITE));
  
  /**
   * GtkAdjustment:upper:
   * 
   * The maximum value of the adjustment. 
   * Note that values will be restricted by 
   * <literal>upper - page-size</literal> if the page-size 
   * property is nonzero.
   *
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_UPPER,
                                   g_param_spec_double ("upper",
							P_("Maximum Value"),
							P_("The maximum value of the adjustment"),
							-G_MAXDOUBLE, 
							G_MAXDOUBLE, 
							0.0, 
							GTK_PARAM_READWRITE));
  
  /**
   * GtkAdjustment:step-increment:
   * 
   * The step increment of the adjustment.
   * 
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_STEP_INCREMENT,
                                   g_param_spec_double ("step-increment",
							P_("Step Increment"),
							P_("The step increment of the adjustment"),
							-G_MAXDOUBLE, 
							G_MAXDOUBLE, 
							0.0, 
							GTK_PARAM_READWRITE));
  
  /**
   * GtkAdjustment:page-increment:
   * 
   * The page increment of the adjustment.
   * 
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PAGE_INCREMENT,
                                   g_param_spec_double ("page-increment",
							P_("Page Increment"),
							P_("The page increment of the adjustment"),
							-G_MAXDOUBLE, 
							G_MAXDOUBLE, 
							0.0, 
							GTK_PARAM_READWRITE));
  
  /**
   * GtkAdjustment:page-size:
   * 
   * The page size of the adjustment. 
   * Note that the page-size is irrelevant and should be set to zero
   * if the adjustment is used for a simple scalar value, e.g. in a 
   * #GtkSpinButton.
   * 
   * Since: 2.4
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PAGE_SIZE,
                                   g_param_spec_double ("page-size",
							P_("Page Size"),
							P_("The page size of the adjustment"),
							-G_MAXDOUBLE, 
							G_MAXDOUBLE, 
							0.0, 
							GTK_PARAM_READWRITE));
  

  adjustment_signals[CHANGED] =
    g_signal_new (I_("changed"),
		  G_OBJECT_CLASS_TYPE (class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		  G_STRUCT_OFFSET (GtkAdjustmentClass, changed),
		  NULL, NULL,
		  _gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  adjustment_signals[VALUE_CHANGED] =
    g_signal_new (I_("value_changed"),
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

static void
gtk_adjustment_get_property (GObject *object, guint prop_id, GValue *value, 
	                     GParamSpec *pspec)
{
  GtkAdjustment *adjustment = GTK_ADJUSTMENT (object);

  switch (prop_id)
    {
    case PROP_VALUE:
      g_value_set_double (value, adjustment->value);
      break;
    case PROP_LOWER:
      g_value_set_double (value, adjustment->lower);
      break;
    case PROP_UPPER:
      g_value_set_double (value, adjustment->upper);
      break;
    case PROP_STEP_INCREMENT:
      g_value_set_double (value, adjustment->step_increment);
      break;
    case PROP_PAGE_INCREMENT:
      g_value_set_double (value, adjustment->page_increment);
      break;
    case PROP_PAGE_SIZE:
      g_value_set_double (value, adjustment->page_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_adjustment_set_property (GObject * object, guint prop_id, 
	                     const GValue * value, GParamSpec * pspec)
{
  GtkAdjustment *adjustment = GTK_ADJUSTMENT (object);
  gdouble double_value = g_value_get_double (value);

  switch (prop_id)
    {
    case PROP_VALUE:
      gtk_adjustment_set_value (GTK_ADJUSTMENT (object), double_value);
      break;
    case PROP_LOWER:
      if (adjustment->lower != double_value)
        {
          adjustment->lower = double_value;
          g_object_notify (G_OBJECT (adjustment), "lower");
        }
      break;
    case PROP_UPPER:
      if (adjustment->upper != double_value)
        {
          adjustment->upper = double_value;
          g_object_notify (G_OBJECT (adjustment), "upper");
        }
      break;
    case PROP_STEP_INCREMENT:
      if (adjustment->step_increment != double_value)
        {
          adjustment->step_increment = double_value;
          g_object_notify (G_OBJECT (adjustment), "step-increment");
        }
      break;
    case PROP_PAGE_INCREMENT:
      if (adjustment->page_increment != double_value)
        {
          adjustment->page_increment = double_value;
          g_object_notify (G_OBJECT (adjustment), "page-increment");
        }
      break;
    case PROP_PAGE_SIZE:
      if (adjustment->page_size != double_value)
        {
          adjustment->page_size = double_value;
          g_object_notify (G_OBJECT (adjustment), "page-size");
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

GtkObject*
gtk_adjustment_new (gdouble value,
		    gdouble lower,
		    gdouble upper,
		    gdouble step_increment,
		    gdouble page_increment,
		    gdouble page_size)
{
  return g_object_new (GTK_TYPE_ADJUSTMENT, 
		       "lower", lower,
		       "upper", upper,
		       "step-increment", step_increment,
		       "page-increment", page_increment,
		       "page-size", page_size,
		       "value", value,
		       NULL);
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
  g_object_notify (G_OBJECT (adjustment), "value");
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

#define __GTK_ADJUSTMENT_C__
#include "gtkaliasdef.c"
