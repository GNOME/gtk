/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gtkorientable.c
 * Copyright (C) 2008 Imendio AB
 * Contact: Michael Natterer <mitch@imendio.com>
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

#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"


static void   gtk_orientable_base_init (GtkOrientableIface *iface);


GType
gtk_orientable_get_type (void)
{
  static GType orientable_type = 0;

  if (! orientable_type)
    {
      const GTypeInfo orientable_info =
      {
        sizeof (GtkOrientableIface),
        (GBaseInitFunc)     gtk_orientable_base_init,
        (GBaseFinalizeFunc) NULL,
      };

      orientable_type = g_type_register_static (G_TYPE_INTERFACE,
                                                I_("GtkOrientable"),
                                                &orientable_info, 0);
    }

  return orientable_type;
}

static void
gtk_orientable_base_init (GtkOrientableIface *iface)
{
  static gboolean initialized = FALSE;

  if (initialized)
    return;

  /**
   * GtkOrientable:orientation:
   *
   * The orientation of the orientable.
   *
   * Since: 2.16
   **/
  g_object_interface_install_property (iface,
                                       g_param_spec_enum ("orientation",
                                                          P_("Orientation"),
                                                          P_("The orientation of the orientable"),
                                                          GTK_TYPE_ORIENTATION,
                                                          GTK_ORIENTATION_HORIZONTAL,
                                                          GTK_PARAM_READWRITE));

  initialized = TRUE;
}

/**
 * gtk_orientable_set_orientation:
 * @orientable: a #GtkOrientable
 * @orientation: the orientable's new orientation.
 *
 * Sets the orientation of the @orientable.
 *
 * Since: 2.16
 **/
void
gtk_orientable_set_orientation (GtkOrientable  *orientable,
                                GtkOrientation  orientation)
{
  g_return_if_fail (GTK_IS_ORIENTABLE (orientable));

  g_object_set (orientable,
                "orientation", orientation,
                NULL);
}

/**
 * gtk_orientable_get_orientation:
 * @orientable: a #GtkOrientable
 *
 * Retrieves the orientation of the @orientable.
 *
 * Return value: the orientation of the @orientable.
 *
 * Since: 2.16
 **/
GtkOrientation
gtk_orientable_get_orientation (GtkOrientable *orientable)
{
  GtkOrientation orientation;

  g_return_val_if_fail (GTK_IS_ORIENTABLE (orientable),
                        GTK_ORIENTATION_HORIZONTAL);

  g_object_get (orientable,
                "orientation", &orientation,
                NULL);

  return orientation;
}

#define __GTK_ORIENTABLE_C__
#include "gtkaliasdef.c"
