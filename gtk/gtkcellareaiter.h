/* gtkcellareaiter.h
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

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_CELL_AREA_ITER_H__
#define __GTK_CELL_AREA_ITER_H__

#include <gtk/gtkcellarea.h>

G_BEGIN_DECLS

#define GTK_TYPE_CELL_AREA_ITER		  (gtk_cell_area_iter_get_type ())
#define GTK_CELL_AREA_ITER(obj)		  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CELL_AREA_ITER, GtkCellAreaIter))
#define GTK_CELL_AREA_ITER_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CELL_AREA_ITER, GtkCellAreaIterClass))
#define GTK_IS_CELL_AREA_ITER(obj)	  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CELL_AREA_ITER))
#define GTK_IS_CELL_AREA_ITER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CELL_AREA_ITER))
#define GTK_CELL_AREA_ITER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CELL_AREA_ITER, GtkCellAreaIterClass))

typedef struct _GtkCellAreaIterPrivate       GtkCellAreaIterPrivate;
typedef struct _GtkCellAreaIterClass         GtkCellAreaIterClass;

struct _GtkCellAreaIter
{
  GObject parent_instance;

  GtkCellAreaIterPrivate *priv;
};

struct _GtkCellAreaIterClass
{
  GObjectClass parent_class;

  /* Subclasses can use this to flush their alignments/allocations */
  void    (* flush_preferred_width)              (GtkCellAreaIter *iter);
  void    (* flush_preferred_height_for_width)   (GtkCellAreaIter *iter,
						  gint             width);
  void    (* flush_preferred_height)             (GtkCellAreaIter *iter);
  void    (* flush_preferred_width_for_height)   (GtkCellAreaIter *iter,
						  gint             height);
  void    (* flush_allocation)                   (GtkCellAreaIter *iter);

  /* These must be invoked after a series of requests before consulting 
   * the iter values, implementors use this to push the overall
   * requests while acconting for any internal alignments */
  void    (* sum_preferred_width)                (GtkCellAreaIter *iter);
  void    (* sum_preferred_height_for_width)     (GtkCellAreaIter *iter,
						  gint             width);
  void    (* sum_preferred_height)               (GtkCellAreaIter *iter);
  void    (* sum_preferred_width_for_height)     (GtkCellAreaIter *iter,
						  gint             height);

  /* Store an allocation value for a GtkCellArea contextual to a range of
   * treemodel rows */
  void    (* allocate_width)                     (GtkCellAreaIter *iter,
						  gint             width);
  void    (* allocate_height)                    (GtkCellAreaIter *iter,
						  gint             height);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GType        gtk_cell_area_iter_get_type                         (void) G_GNUC_CONST;

GtkCellArea *gtk_cell_area_iter_get_area                         (GtkCellAreaIter *iter);

/* Apis for GtkCellArea clients to flush the cache */
void         gtk_cell_area_iter_flush                            (GtkCellAreaIter *iter);
void         gtk_cell_area_iter_flush_preferred_width            (GtkCellAreaIter *iter);
void         gtk_cell_area_iter_flush_preferred_height_for_width (GtkCellAreaIter *iter,
								  gint             for_width);
void         gtk_cell_area_iter_flush_preferred_height           (GtkCellAreaIter *iter);
void         gtk_cell_area_iter_flush_preferred_width_for_height (GtkCellAreaIter *iter,
								  gint             for_height);
void         gtk_cell_area_iter_flush_allocation                 (GtkCellAreaIter *iter);

/* Apis for GtkCellArea clients to sum up the results of a series of requests, this
 * call is required to reduce the processing while calculating the size of each row */
void         gtk_cell_area_iter_sum_preferred_width              (GtkCellAreaIter *iter);
void         gtk_cell_area_iter_sum_preferred_height_for_width   (GtkCellAreaIter *iter,
								  gint             for_width);
void         gtk_cell_area_iter_sum_preferred_height             (GtkCellAreaIter *iter);
void         gtk_cell_area_iter_sum_preferred_width_for_height   (GtkCellAreaIter *iter,
								  gint             for_height);

/* Apis to set an allocation size in one dimension or another, the subclass specific iter
 * will store allocated positions/sizes for individual cells or groups of cells */
void         gtk_cell_area_iter_allocate_width                   (GtkCellAreaIter *iter,
								  gint             width);
void         gtk_cell_area_iter_allocate_height                  (GtkCellAreaIter *iter,
								  gint             height);

/* Apis for GtkCellArea clients to consult cached values for multiple GtkTreeModel rows */
void         gtk_cell_area_iter_get_preferred_width              (GtkCellAreaIter *iter,
								  gint            *minimum_width,
								  gint            *natural_width);
void         gtk_cell_area_iter_get_preferred_height_for_width   (GtkCellAreaIter *iter,
								  gint             for_width,
								  gint            *minimum_height,
								  gint            *natural_height);
void         gtk_cell_area_iter_get_preferred_height             (GtkCellAreaIter *iter,
								  gint            *minimum_height,
								  gint            *natural_height);
void         gtk_cell_area_iter_get_preferred_width_for_height   (GtkCellAreaIter *iter,
								  gint             for_height,
								  gint            *minimum_width,
								  gint            *natural_width);
void         gtk_cell_area_iter_get_allocation                   (GtkCellAreaIter *iter,
								  gint            *width,
								  gint            *height);

/* Apis for GtkCellArea implementations to update cached values for multiple GtkTreeModel rows */
void         gtk_cell_area_iter_push_preferred_width             (GtkCellAreaIter *iter,
								  gint             minimum_width,
								  gint             natural_width);
void         gtk_cell_area_iter_push_preferred_height_for_width  (GtkCellAreaIter *iter,
								  gint             for_width,
								  gint             minimum_height,
								  gint             natural_height);
void         gtk_cell_area_iter_push_preferred_height            (GtkCellAreaIter *iter,
								  gint             minimum_height,
								  gint             natural_height);
void         gtk_cell_area_iter_push_preferred_width_for_height  (GtkCellAreaIter *iter,
								  gint             for_height,
								  gint             minimum_width,
								  gint             natural_width);

G_END_DECLS

#endif /* __GTK_CELL_AREA_ITER_H__ */
