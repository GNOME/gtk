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
#include "gailrenderercell.h"

static void      gail_renderer_cell_class_init          (GailRendererCellClass *klass);
static void      gail_renderer_cell_init                (GailRendererCell      *renderer_cell);

static void      gail_renderer_cell_finalize            (GObject               *object);

G_DEFINE_TYPE (GailRendererCell, gail_renderer_cell, GAIL_TYPE_CELL)

static void 
gail_renderer_cell_class_init (GailRendererCellClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  klass->property_list = NULL;

  gobject_class->finalize = gail_renderer_cell_finalize;
}

static void
gail_renderer_cell_init (GailRendererCell *renderer_cell)
{
  renderer_cell->renderer = NULL;
}

static void
gail_renderer_cell_finalize (GObject  *object)
{
  GailRendererCell *renderer_cell = GAIL_RENDERER_CELL (object);

  if (renderer_cell->renderer)
    g_object_unref (renderer_cell->renderer);
  G_OBJECT_CLASS (gail_renderer_cell_parent_class)->finalize (object);
}

gboolean
gail_renderer_cell_update_cache (GailRendererCell *cell, 
                                 gboolean         emit_change_signal)
{
  GailRendererCellClass *class = GAIL_RENDERER_CELL_GET_CLASS(cell);
  if (class->update_cache)
    return (class->update_cache)(cell, emit_change_signal);
  return FALSE;
}

AtkObject*
gail_renderer_cell_new (void)
{
  GObject *object;
  AtkObject *atk_object;
  GailRendererCell *cell;

  object = g_object_new (GAIL_TYPE_RENDERER_CELL, NULL);

  g_return_val_if_fail (object != NULL, NULL);

  atk_object = ATK_OBJECT (object);
  atk_object->role = ATK_ROLE_TABLE_CELL;

  cell = GAIL_RENDERER_CELL(object);

  return atk_object;
}
