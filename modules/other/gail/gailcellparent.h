/* GAIL - The GNOME Accessibility Implementation Library
 *
 * Copyright 2001 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GAIL_CELL_PARENT_H__
#define __GAIL_CELL_PARENT_H__

#include <atk/atk.h>
#include <gail/gailcell.h>

G_BEGIN_DECLS

/*
 * The GailCellParent interface should be supported by any object which
 * contains children which are flyweights, i.e. do not have corresponding
 * widgets and the children need help from their parent to provide
 * functionality. One example is GailTreeView where the children GailCell
 * need help from the GailTreeView in order to implement 
 * atk_component_get_extents
 */

#define GAIL_TYPE_CELL_PARENT            (gail_cell_parent_get_type ())
#define GAIL_IS_CELL_PARENT(obj)         G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_CELL_PARENT)
#define GAIL_CELL_PARENT(obj)            G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_CELL_PARENT, GailCellParent)
#define GAIL_CELL_PARENT_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GAIL_TYPE_CELL_PARENT, GailCellParentIface))

#ifndef _TYPEDEF_GAIL_CELL_PARENT_
#define _TYPEDEF_GAIL_CELL_PARENT_
typedef struct _GailCellParent GailCellParent;
#endif
typedef struct _GailCellParentIface GailCellParentIface;

struct _GailCellParentIface
{
  GTypeInterface parent;
  void                  ( *get_cell_extents)      (GailCellParent        *parent,
                                                   GailCell              *cell,
                                                   gint                  *x,
                                                   gint                  *y,
                                                   gint                  *width,
                                                   gint                  *height,
                                                   AtkCoordType          coord_type);
  void                  ( *get_cell_area)         (GailCellParent        *parent,
                                                   GailCell              *cell,
                                                   GdkRectangle          *cell_rect);
  gboolean              ( *grab_focus)            (GailCellParent        *parent,
                                                   GailCell              *cell);
};

GType  gail_cell_parent_get_type               (void);

void   gail_cell_parent_get_cell_extents       (GailCellParent        *parent,
                                                GailCell              *cell,
                                                gint                  *x,
                                                gint                  *y,
                                                gint                  *width,
                                                gint                  *height,
                                                AtkCoordType          coord_type
);
void  gail_cell_parent_get_cell_area           (GailCellParent        *parent,
                                                GailCell              *cell,
                                                GdkRectangle          *cell_rect);
gboolean gail_cell_parent_grab_focus           (GailCellParent        *parent,
                                                GailCell              *cell);

G_END_DECLS

#endif /* __GAIL_CELL_PARENT_H__ */
