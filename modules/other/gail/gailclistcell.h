/* GAIL - The GNOME Accessibility Implementation Library
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

#ifndef __GAIL_CLIST_CELL_H__
#define __GAIL_CLIST_CELL_H__

#include <atk/atk.h>
#include <gail/gailcell.h>

G_BEGIN_DECLS

#define GAIL_TYPE_CLIST_CELL                     (gail_clist_cell_get_type ())
#define GAIL_CLIST_CELL(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), GAIL_TYPE_CLIST_CELL, GailCListCell))
#define GAIL_CLIST_CELL_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), GAIL_TYPE_CLIST_CELL, GailCListCellClass))
#define GAIL_IS_CLIST_CELL(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIL_TYPE_CLIST_CELL))
#define GAIL_IS_CLIST_CELL_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), GAIL_TYPE_CLIST_CELL))
#define GAIL_CLIST_CELL_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIL_TYPE_CLIST_CELL, GailCListCellClass))

typedef struct _GailCListCell                  GailCListCell;
typedef struct _GailCListCellClass             GailCListCellClass;

struct _GailCListCell
{
  GailCell parent;
};

GType gail_clist_cell_get_type (void);

struct _GailCListCellClass
{
  GailCellClass parent_class;
};

AtkObject *gail_clist_cell_new (void);

G_END_DECLS

#endif /* __GAIL_CLIST_CELL_H__ */
