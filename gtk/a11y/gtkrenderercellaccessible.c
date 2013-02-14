/* GTK+ - accessibility implementations
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gtk/gtk.h>
#include "gtkrenderercellaccessible.h"
#include "gtkintl.h"

struct _GtkRendererCellAccessiblePrivate
{
  GtkCellRenderer *renderer;
};

enum {
  PROP_0,
  PROP_RENDERER
};

G_DEFINE_TYPE (GtkRendererCellAccessible, gtk_renderer_cell_accessible, GTK_TYPE_CELL_ACCESSIBLE)

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
      accessible->priv->renderer = g_value_dup_object (value);
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
      g_value_set_object (value, accessible->priv->renderer);
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

  if (renderer_cell->priv->renderer)
    g_object_unref (renderer_cell->priv->renderer);

  G_OBJECT_CLASS (gtk_renderer_cell_accessible_parent_class)->finalize (object);
}

static void
gtk_renderer_cell_accessible_class_init (GtkRendererCellAccessibleClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

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

  g_type_class_add_private (klass, sizeof (GtkRendererCellAccessiblePrivate));
}

static void
gtk_renderer_cell_accessible_init (GtkRendererCellAccessible *renderer_cell)
{
  renderer_cell->priv = G_TYPE_INSTANCE_GET_PRIVATE (renderer_cell,
                                                     GTK_TYPE_RENDERER_CELL_ACCESSIBLE,
                                                     GtkRendererCellAccessiblePrivate);
}

AtkObject *
gtk_renderer_cell_accessible_new (GtkCellRenderer *renderer)
{
  AtkObject *object;

  g_return_val_if_fail (GTK_IS_CELL_RENDERER (renderer), NULL);

  object = g_object_new (_gtk_cell_renderer_get_accessible_type (renderer),
                         "renderer", renderer,
                         NULL);

  atk_object_set_role (object, ATK_ROLE_TABLE_CELL);

  return object;
}
