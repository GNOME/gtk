/* GAIL - The GNOME Accessibility Enabling Library
 * Copyright 2001 Sun Microsystems Inc.
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

#include <config.h>

#include <gtk/gtkcellrenderertext.h>
#include "gailtextcellfactory.h"
#include "gailtextcell.h"

static void gail_text_cell_factory_class_init (GailTextCellFactoryClass        *klass);

static AtkObject* gail_text_cell_factory_create_accessible (
                                GObject                              *obj);

static GType gail_text_cell_factory_get_accessible_type (void);

GType
gail_text_cell_factory_get_type (void)
{
  static GType type = 0;

  if (!type) 
  {
    static const GTypeInfo tinfo =
    {
      sizeof (GailTextCellFactoryClass),
      (GBaseInitFunc) NULL, /* base init */
      (GBaseFinalizeFunc) NULL, /* base finalize */
      (GClassInitFunc) gail_text_cell_factory_class_init, /* class init */
      (GClassFinalizeFunc) NULL, /* class finalize */
      NULL, /* class data */
      sizeof (GailTextCellFactory), /* instance size */
      0, /* nb preallocs */
      (GInstanceInitFunc) NULL, /* instance init */
      NULL /* value table */
    };
    type = g_type_register_static (ATK_TYPE_OBJECT_FACTORY, 
                           "GailTextCellFactory" , &tinfo, 0);
  }

  return type;
}

static void 
gail_text_cell_factory_class_init (GailTextCellFactoryClass *klass)
{
  AtkObjectFactoryClass *class = ATK_OBJECT_FACTORY_CLASS (klass);

  class->create_accessible = gail_text_cell_factory_create_accessible;
  class->get_accessible_type = gail_text_cell_factory_get_accessible_type;
}

static AtkObject* 
gail_text_cell_factory_create_accessible (GObject     *obj)
{
  g_return_val_if_fail (GTK_IS_CELL_RENDERER_TEXT (obj), NULL);

  return gail_text_cell_new ();
}

static GType
gail_text_cell_factory_get_accessible_type (void)
{
  return GAIL_TYPE_TEXT_CELL;
}
