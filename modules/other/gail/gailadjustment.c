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

#include <string.h>
#include <gtk/gtk.h>
#include "gailadjustment.h"

static void	 gail_adjustment_class_init        (GailAdjustmentClass *klass);

static void	 gail_adjustment_real_initialize   (AtkObject	        *obj,
                                                    gpointer            data);

static void	 atk_value_interface_init          (AtkValueIface       *iface);

static void	 gail_adjustment_get_current_value (AtkValue            *obj,
                                                    GValue              *value);
static void	 gail_adjustment_get_maximum_value (AtkValue            *obj,
                                                    GValue              *value);
static void	 gail_adjustment_get_minimum_value (AtkValue            *obj,
                                                    GValue              *value);
static gboolean	 gail_adjustment_set_current_value (AtkValue            *obj,
                                                    const GValue        *value);

static void      gail_adjustment_destroyed         (GtkAdjustment       *adjustment,
                                                    GailAdjustment      *gail_adjustment);

static gpointer parent_class = NULL;

GType
gail_adjustment_get_type (void)
{
  static GType type = 0;

  if (!type)
  {
    static const GTypeInfo tinfo =
    {
      sizeof (GailAdjustmentClass),
      (GBaseInitFunc) NULL, /* base init */
      (GBaseFinalizeFunc) NULL, /* base finalize */
      (GClassInitFunc) gail_adjustment_class_init, /* class init */
      (GClassFinalizeFunc) NULL, /* class finalize */
      NULL, /* class data */
      sizeof (GailAdjustment), /* instance size */
      0, /* nb preallocs */
      (GInstanceInitFunc) NULL, /* instance init */
      NULL /* value table */
    };

    static const GInterfaceInfo atk_value_info =
    {
        (GInterfaceInitFunc) atk_value_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
    };

    type = g_type_register_static (ATK_TYPE_OBJECT,
                                   "GailAdjustment", &tinfo, 0);

    g_type_add_interface_static (type, ATK_TYPE_VALUE,
                                 &atk_value_info);
  }
  return type;
}

static void	 
gail_adjustment_class_init (GailAdjustmentClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  class->initialize = gail_adjustment_real_initialize;
}

AtkObject* 
gail_adjustment_new (GtkAdjustment *adjustment)
{
  GObject *object;
  AtkObject *atk_object;

  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), NULL);

  object = g_object_new (GAIL_TYPE_ADJUSTMENT, NULL);

  g_return_val_if_fail (object != NULL, NULL);

  atk_object = ATK_OBJECT (object);
  atk_object_initialize (atk_object, adjustment);

  return atk_object;
}

static void
gail_adjustment_real_initialize (AtkObject *obj,
                                 gpointer  data)
{
  GtkAdjustment *adjustment;

  ATK_OBJECT_CLASS (parent_class)->initialize (obj, data);

  adjustment = GTK_ADJUSTMENT (data);

  obj->role = ATK_ROLE_UNKNOWN;
  GAIL_ADJUSTMENT (obj)->adjustment = adjustment;

  g_signal_connect_object (G_OBJECT (adjustment),
                           "destroy",
                           G_CALLBACK (gail_adjustment_destroyed),
                           obj, 0);
}

static void	 
atk_value_interface_init (AtkValueIface *iface)
{
  g_return_if_fail (iface != NULL);

  iface->get_current_value = gail_adjustment_get_current_value;
  iface->get_maximum_value = gail_adjustment_get_maximum_value;
  iface->get_minimum_value = gail_adjustment_get_minimum_value;
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

  current_value = adjustment->value;
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

  maximum_value = adjustment->upper;
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

  minimum_value = adjustment->lower;
  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, minimum_value);
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

static void
gail_adjustment_destroyed (GtkAdjustment       *adjustment,
                           GailAdjustment      *gail_adjustment)
{
  /*
   * This is the signal handler for the "destroy" signal for the 
   * GtkAdjustment. We set the  pointer location to NULL;
   */
  gail_adjustment->adjustment = NULL;
}
