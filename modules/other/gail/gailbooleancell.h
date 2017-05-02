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

#ifndef __GAIL_BOOLEAN_CELL_H__
#define __GAIL_BOOLEAN_CELL_H__

#include <atk/atk.h>
#include <gail/gailrenderercell.h>

G_BEGIN_DECLS

#define GAIL_TYPE_BOOLEAN_CELL            (gail_boolean_cell_get_type ())
#define GAIL_BOOLEAN_CELL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_BOOLEAN_CELL, GailBooleanCell))
#define GAIL_BOOLEAN_CELL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_BOOLEAN_CELL, GailBooleanCellClass))
#define GAIL_IS_BOOLEAN_CELL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_BOOLEAN_CELL))
#define GAIL_IS_BOOLEAN_CELL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_BOOLEAN_CELL))
#define GAIL_BOOLEAN_CELL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_BOOLEAN_CELL, GailBooleanCellClass))

typedef struct _GailBooleanCell                  GailBooleanCell;
typedef struct _GailBooleanCellClass             GailBooleanCellClass;

struct _GailBooleanCell
{
  GailRendererCell parent;
  gboolean cell_value;
  gboolean cell_sensitive;
};

 GType gail_boolean_cell_get_type (void);

struct _GailBooleanCellClass
{
  GailRendererCellClass parent_class;
};

AtkObject *gail_boolean_cell_new (void);

G_END_DECLS

#endif /* __GAIL_TREE_VIEW_BOOLEAN_CELL_H__ */
