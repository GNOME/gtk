/* gtkseparatortoolitem.c
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@codefactory.se>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
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

#include "gtkseparatormenuitem.h"
#include "gtkseparatortoolitem.h"
#include "gtkintl.h"

static void gtk_separator_tool_item_class_init (GtkSeparatorToolItemClass*class);

static void gtk_separator_tool_item_add (GtkContainer *container,
					 GtkWidget    *child);

static GObjectClass *parent_class = NULL;


GType
gtk_separator_tool_item_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo type_info =
	{
	  sizeof (GtkSeparatorToolItemClass),
	  (GBaseInitFunc) 0,
	  (GBaseFinalizeFunc) 0,
	  (GClassInitFunc) gtk_separator_tool_item_class_init,
	  (GClassFinalizeFunc) 0,
	  NULL,
	  sizeof (GtkSeparatorToolItem),
	  0, /* n_preallocs */
	  (GInstanceInitFunc) NULL,
	};

      type = g_type_register_static (GTK_TYPE_TOOL_ITEM,
				     "GtkSeparatorToolItem", &type_info, 0);
    }
  return type;
}


static void
gtk_separator_tool_item_class_init (GtkSeparatorToolItemClass *class)
{
  GtkContainerClass *container_class;
  GtkToolItemClass *toolitem_class;

  parent_class = g_type_class_peek_parent (class);
  container_class = (GtkContainerClass *)class;
  toolitem_class = (GtkToolItemClass *)class;

  container_class->add = gtk_separator_tool_item_add;
}

static void
gtk_separator_tool_item_add (GtkContainer *container,
			     GtkWidget    *child)
{
  g_warning("attempt to add a child to an GtkSeparatorToolItem");
}

GtkToolItem *
gtk_separator_tool_item_new (void)
{
  GtkToolItem *self;

  self = g_object_new (GTK_TYPE_SEPARATOR_TOOL_ITEM,
		       NULL);
  
  return self;
}
