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

#include "config.h"

#include <gtk/gtk.h>
#include "gtkrenderercellaccessible.h"
#include "gtkintl.h"


enum {
  PROP_0,
  PROP_RENDERER
};

G_DEFINE_TYPE (GtkRendererCellAccessible, _gtk_renderer_cell_accessible, GTK_TYPE_CELL_ACCESSIBLE)

static void
gtk_renderer_cell_accessible_set_property (GObject         *object,
                                           guint            prop_id,
                                           const GValue    *value,
                                           GParamSpec      *pspec)
{
  GtkRendererCellAccessible *accessible = GTK_RENDERER_CELL_ACCESSIBLE (object);

  switch (prop_id)
    {
    case PROP_RENDERER:
      accessible->renderer = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_renderer_cell_accessible_get_property (GObject         *object,
                                           guint            prop_id,
                                           GValue          *value,
                                           GParamSpec      *pspec)
{
  GtkRendererCellAccessible *accessible = GTK_RENDERER_CELL_ACCESSIBLE (object);

  switch (prop_id)
    {
    case PROP_RENDERER:
      g_value_set_object (value, accessible->renderer);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_renderer_cell_accessible_finalize (GObject *object)
{
  GtkRendererCellAccessible *renderer_cell = GTK_RENDERER_CELL_ACCESSIBLE (object);

  if (renderer_cell->renderer)
    g_object_unref (renderer_cell->renderer);

  G_OBJECT_CLASS (_gtk_renderer_cell_accessible_parent_class)->finalize (object);
}

static void
_gtk_renderer_cell_accessible_class_init (GtkRendererCellAccessibleClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  klass->property_list = NULL;

  gobject_class->get_property = gtk_renderer_cell_accessible_get_property;
  gobject_class->set_property = gtk_renderer_cell_accessible_set_property;
  gobject_class->finalize = gtk_renderer_cell_accessible_finalize;

  g_object_class_install_property (gobject_class,
				   PROP_RENDERER,
				   g_param_spec_object ("renderer",
							P_("Cell renderer"),
							P_("The cell renderer represented by this accessible"),
							GTK_TYPE_CELL_RENDERER,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
_gtk_renderer_cell_accessible_init (GtkRendererCellAccessible *renderer_cell)
{
}

gboolean
_gtk_renderer_cell_accessible_update_cache (GtkRendererCellAccessible *cell,
                                            gboolean                   emit_change_signal)
{
  GtkRendererCellAccessibleClass *class = GTK_RENDERER_CELL_ACCESSIBLE_GET_CLASS (cell);

  if (class->update_cache)
    return (class->update_cache) (cell, emit_change_signal);

  return FALSE;
}

AtkObject *
_gtk_renderer_cell_accessible_new (void)
{
  GObject *object;
  AtkObject *atk_object;

  object = g_object_new (GTK_TYPE_RENDERER_CELL_ACCESSIBLE, NULL);

  atk_object = ATK_OBJECT (object);
  atk_object->role = ATK_ROLE_TABLE_CELL;

  return atk_object;
}
