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


G_DEFINE_TYPE (GtkRendererCellAccessible, _gtk_renderer_cell_accessible, GTK_TYPE_CELL_ACCESSIBLE)


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

  gobject_class->finalize = gtk_renderer_cell_accessible_finalize;
}

static void
_gtk_renderer_cell_accessible_init (GtkRendererCellAccessible *renderer_cell)
{
  renderer_cell->renderer = NULL;
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
