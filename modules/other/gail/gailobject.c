/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2003 Sun Microsystems Inc.
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

#include <gtk/gtk.h>
#include "gailobject.h"

static void       gail_object_class_init               (GailObjectClass *klass);

static void       gail_object_init                     (GailObject *object);

static void       gail_object_real_initialize          (AtkObject       *obj,
                                                        gpointer        data);

G_DEFINE_TYPE (GailObject, gail_object, ATK_TYPE_GOBJECT_ACCESSIBLE)

static void
gail_object_class_init (GailObjectClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->initialize = gail_object_real_initialize;
}

static void
gail_object_init (GailObject *object)
{
}

static void
gail_object_real_initialize (AtkObject *obj,
                             gpointer  data)
{
  ATK_OBJECT_CLASS (gail_object_parent_class)->initialize (obj, data);

  obj->role = ATK_ROLE_UNKNOWN;
}
