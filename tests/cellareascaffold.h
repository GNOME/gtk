/* cellareascaffold.h
 *
 * Copyright (C) 2010 Openismus GmbH
 *
 * Authors:
 *      Tristan Van Berkom <tristanvb@openismus.com>
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

#ifndef __CELL_AREA_SCAFFOLD_H__
#define __CELL_AREA_SCAFFOLD_H__

#include <gtk/gtk.h>


G_BEGIN_DECLS

#define TYPE_CELL_AREA_SCAFFOLD            (cell_area_scaffold_get_type ())
#define CELL_AREA_SCAFFOLD(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_CELL_AREA_SCAFFOLD, CellAreaScaffold))
#define CELL_AREA_SCAFFOLD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_CELL_AREA_SCAFFOLD, CellAreaScaffoldClass))
#define IS_CELL_AREA_SCAFFOLD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_CELL_AREA_SCAFFOLD))
#define IS_CELL_AREA_SCAFFOLD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_CELL_AREA_SCAFFOLD))
#define CELL_AREA_SCAFFOLD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_CELL_AREA_SCAFFOLD, CellAreaScaffoldClass))


typedef struct _CellAreaScaffold         CellAreaScaffold;
typedef struct _CellAreaScaffoldClass    CellAreaScaffoldClass;
typedef struct _CellAreaScaffoldPrivate  CellAreaScaffoldPrivate;

struct _CellAreaScaffold
{
  GtkContainer widget;

  CellAreaScaffoldPrivate *priv;
};

struct _CellAreaScaffoldClass
{
  GtkContainerClass parent_class;

  void  (* activate) (CellAreaScaffold *scaffold);
};


GType         cell_area_scaffold_get_type              (void) G_GNUC_CONST;
GtkWidget    *cell_area_scaffold_new                   (void);

GtkCellArea  *cell_area_scaffold_get_area              (CellAreaScaffold *scaffold);
void          cell_area_scaffold_set_model             (CellAreaScaffold *scaffold,
							GtkTreeModel     *model);
GtkTreeModel *cell_area_scaffold_get_model             (CellAreaScaffold *scaffold);

void          cell_area_scaffold_set_row_spacing       (CellAreaScaffold *scaffold,
							gint              spacing);
gint          cell_area_scaffold_get_row_spacing       (CellAreaScaffold *scaffold);

void          cell_area_scaffold_set_indentation       (CellAreaScaffold *scaffold,
							gint              indent);
gint          cell_area_scaffold_get_indentation       (CellAreaScaffold *scaffold);



G_END_DECLS

#endif /* __CELL_AREA_SCAFFOLD_H__ */
