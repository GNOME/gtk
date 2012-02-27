/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2004 Sun Microsystems Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gtk/gtk.h>
#include "gtkboxaccessible.h"


G_DEFINE_TYPE (GtkBoxAccessible, _gtk_box_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE)

static void
gtk_box_accessible_initialize (AtkObject *accessible,
                               gpointer   data)
{
  ATK_OBJECT_CLASS (_gtk_box_accessible_parent_class)->initialize (accessible, data);
  accessible->role = ATK_ROLE_FILLER;
}

static void
_gtk_box_accessible_class_init (GtkBoxAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->initialize = gtk_box_accessible_initialize;
}

static void
_gtk_box_accessible_init (GtkBoxAccessible *scale)
{
}
