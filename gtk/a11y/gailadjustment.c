/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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

#include "config.h"

#include <string.h>
#include <gtk/gtk.h>
#include "gailadjustment.h"

static void	 gail_adjustment_class_init        (GailAdjustmentClass *klass);

static void	 gail_adjustment_init              (GailAdjustment      *adjustment);

static void	 gail_adjustment_real_initialize   (AtkObject	        *obj,
                                                    gpointer            data);

static void	 atk_value_interface_init          (AtkValueIface       *iface);

static void	 gail_adjustment_get_current_value (AtkValue            *obj,
                                                    GValue              *value);
static void	 gail_adjustment_get_maximum_value (AtkValue            *obj,
                                                    GValue              *value);
static void	 gail_adjustment_get_minimum_value (AtkValue            *obj,
                                                    GValue              *value);
static void	 gail_adjustment_get_minimum_increment (AtkValue        *obj,
                                                    GValue              *value);
static gboolean	 gail_adjustment_set_current_value (AtkValue            *obj,
                                                    const GValue        *value);

G_DEFINE_TYPE_WITH_CODE (GailAdjustment, gail_adjustment, ATK_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, atk_value_interface_init))

static void	 
gail_adjustment_class_init (GailAdjustmentClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->initialize = gail_adjustment_real_initialize;
}

static void
gail_adjustment_init (GailAdjustment *adjustment)
{
}

AtkObject* 
gail_adjustment_new (GtkAdjustment *adjustment)
{
  GObject *object;
  AtkObject *atk_object;

  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), NULL);

  object = g_object_new (GAIL_TYPE_ADJUSTMENT, NULL);

  atk_object = ATK_OBJECT (object);
  atk_object_initialize (atk_object, adjustment);

  return atk_object;
}

static void
gail_adjustment_real_initialize (AtkObject *obj,
                                 gpointer  data)
{
  GtkAdjustment *adjustment;
  GailAdjustment *gail_adjustment;

  ATK_OBJECT_CLASS (gail_adjustment_parent_class)->initialize (obj, data);

  adjustment = GTK_ADJUSTMENT (data);

  obj->role = ATK_ROLE_UNKNOWN;
  gail_adjustment = GAIL_ADJUSTMENT (obj);
  gail_adjustment->adjustment = adjustment;

  g_object_add_weak_pointer (G_OBJECT (adjustment),
                             (gpointer *) &gail_adjustment->adjustment);
}

static void	 
atk_value_interface_init (AtkValueIface *iface)
{
  iface->get_current_value = gail_adjustment_get_current_value;
  iface->get_maximum_value = gail_adjustment_get_maximum_value;
  iface->get_minimum_value = gail_adjustment_get_minimum_value;
  iface->get_minimum_increment = gail_adjustment_get_minimum_increment;
  iface->set_current_value = gail_adjustment_set_current_value;
}

static void	 
gail_adjustment_get_current_value (AtkValue             *obj,
                                   GValue               *value)
{
  GtkAdjustment* adjustment;
  gdouble current_value;
 
  adjustment = GAIL_ADJUSTMENT (obj)->adjustment;
  if (adjustment == NULL)
  {
    /* State is defunct */
    return;
  }

  current_value = gtk_adjustment_get_value (adjustment);
  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value,current_value);
}

static void	 
gail_adjustment_get_maximum_value (AtkValue             *obj,
                                   GValue               *value)
{
  GtkAdjustment* adjustment;
  gdouble maximum_value;
 
  adjustment = GAIL_ADJUSTMENT (obj)->adjustment;
  if (adjustment == NULL)
  {
    /* State is defunct */
    return;
  }

  maximum_value = gtk_adjustment_get_upper (adjustment);
  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, maximum_value);
}

static void	 
gail_adjustment_get_minimum_value (AtkValue             *obj,
                                   GValue               *value)
{
  GtkAdjustment* adjustment;
  gdouble minimum_value;
 
  adjustment = GAIL_ADJUSTMENT (obj)->adjustment;
  if (adjustment == NULL)
  {
    /* State is defunct */
    return;
  }

  minimum_value = gtk_adjustment_get_lower (adjustment);
  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, minimum_value);
}

static void
gail_adjustment_get_minimum_increment (AtkValue        *obj,
                                       GValue          *value)
{
  GtkAdjustment* adjustment;
  gdouble minimum_increment;
 
  adjustment = GAIL_ADJUSTMENT (obj)->adjustment;
  if (adjustment == NULL)
  {
    /* State is defunct */
    return;
  }

  if (gtk_adjustment_get_step_increment (adjustment) != 0 &&
      gtk_adjustment_get_page_increment (adjustment) != 0)
    {
      if (ABS (gtk_adjustment_get_step_increment (adjustment)) < ABS (gtk_adjustment_get_page_increment (adjustment)))
        minimum_increment = gtk_adjustment_get_step_increment (adjustment);
      else
        minimum_increment = gtk_adjustment_get_page_increment (adjustment);
    }
  else if (gtk_adjustment_get_step_increment (adjustment) == 0 &&
           gtk_adjustment_get_page_increment (adjustment) == 0)
    {
      minimum_increment = 0;
    }
  else if (gtk_adjustment_get_step_increment (adjustment) == 0)
    {
      minimum_increment = gtk_adjustment_get_page_increment (adjustment);
    }
  else
    {
      minimum_increment = gtk_adjustment_get_step_increment (adjustment);
    }

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, minimum_increment);
}

static gboolean	 
gail_adjustment_set_current_value (AtkValue             *obj,
                                   const GValue         *value)
{
  if (G_VALUE_HOLDS_DOUBLE (value))
  {
    GtkAdjustment* adjustment;
    gdouble new_value;
 
    adjustment = GAIL_ADJUSTMENT (obj)->adjustment;
    if (adjustment == NULL)
    {
      /* State is defunct */
      return FALSE;
    }
    new_value = g_value_get_double (value);
    gtk_adjustment_set_value (adjustment, new_value);

    return TRUE;
  }
  else
    return FALSE;
}
