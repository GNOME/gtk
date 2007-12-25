/* GAIL - The GNOME Accessibility Enabling Library
 * Copyright 2001 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GAIL_RENDERER_CELL_H__
#define __GAIL_RENDERER_CELL_H__

#include <atk/atk.h>
#include <gail/gailcell.h>

G_BEGIN_DECLS

#define GAIL_TYPE_RENDERER_CELL            (gail_renderer_cell_get_type ())
#define GAIL_RENDERER_CELL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_RENDERER_CELL, GailRendererCell))
#define GAIL_RENDERER_CELL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_TYPE_RENDERER_CELL, GailRendererCellClass))
#define GAIL_IS_RENDERER_CELL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_RENDERER_CELL))
#define GAIL_IS_RENDERER_CELL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_RENDERER_CELL))
#define GAIL_RENDERER_CELL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_RENDERER_CELL, GailRendererCellClass))

typedef struct _GailRendererCell                  GailRendererCell;
typedef struct _GailRendererCellClass             GailRendererCellClass;

struct _GailRendererCell
{
  GailCell parent;
  GtkCellRenderer *renderer;
};

GType gail_renderer_cell_get_type (void);

struct _GailRendererCellClass
{
  GailCellClass parent_class;
  gchar **property_list;
  gboolean (*update_cache)(GailRendererCell *cell, gboolean emit_change_signal);
};

gboolean
gail_renderer_cell_update_cache (GailRendererCell *cell, gboolean emit_change_signal);

AtkObject *gail_renderer_cell_new (void);

G_END_DECLS

#endif /* __GAIL_TREE_VIEW_TEXT_CELL_H__ */
