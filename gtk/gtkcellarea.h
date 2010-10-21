/* gtkcellarea.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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

#ifndef __GTK_CELL_AREA_H__
#define __GTK_CELL_AREA_H__

#include <gtk/gtkcellrenderer.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_CELL_AREA		  (gtk_cell_area_get_type ())
#define GTK_CELL_AREA(obj)		  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CELL_AREA, GtkCellArea))
#define GTK_CELL_AREA_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CELL_AREA, GtkCellAreaClass))
#define GTK_IS_CELL_AREA(obj)	  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CELL_AREA))
#define GTK_IS_CELL_AREA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CELL_AREA))
#define GTK_CELL_AREA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CELL_AREA, GtkCellAreaClass))

typedef struct _GtkCellArea              GtkCellArea;
typedef struct _GtkCellAreaClass         GtkCellAreaClass;


/**
 * GtkCellCallback:
 * @cell: the cell renderer to operate on
 * @data: user-supplied data
 *
 * The type of the callback functions used for iterating over
 * the cell renderers of a #GtkCellArea, see gtk_cell_area_forall().
 */
typedef void    (*GtkCellCallback)     (GtkCellRenderer  *renderer,
				        gpointer          data);


struct _GtkCellArea
{
  GInitiallyUnowned parent_instance;

  /* <private> */
  GtkCellAreaPrivate *priv;
};

struct _GtkCellAreaClass
{
  GInitiallyUnownedClass parent_class;

  /* vtable - not signals */
  void               (* add)                             (GtkCellArea        *area,
							  GtkCellRenderer    *renderer);
  void               (* remove)                          (GtkCellArea        *area,
							  GtkCellRenderer    *renderer);
  void               (* forall)                          (GtkCellArea        *area,
							  GtkCellCallback     callback,
							  gpointer            callback_data);
  void               (* apply_attribute)                 (GtkCellArea        *area,
							  gint                attribute,
							  GValue             *value);
  gint               (* event)                           (GtkCellArea        *area,
							  GtkWidget          *widget,
							  GdkEvent           *event,
							  const GdkRectangle *cell_area);
  void               (* render)                          (GtkCellArea        *area,
							  cairo_t            *cr,
							  GtkWidget          *widget,
							  const GdkRectangle *cell_area);

  GtkSizeRequestMode (* get_request_mode)                (GtkCellArea        *area);
  void               (* get_preferred_width)             (GtkCellArea        *area,
                                                          GtkWidget          *widget,
                                                          gint               *minimum_size,
                                                          gint               *natural_size);
  void               (* get_preferred_height_for_width)  (GtkCellArea        *area,
                                                          GtkWidget          *widget,
                                                          gint                width,
                                                          gint               *minimum_height,
                                                          gint               *natural_height);
  void               (* get_preferred_height)            (GtkCellArea        *area,
                                                          GtkWidget          *widget,
                                                          gint               *minimum_size,
                                                          gint               *natural_size);
  void               (* get_preferred_width_for_height)  (GtkCellArea        *area,
                                                          GtkWidget          *widget,
                                                          gint                height,
                                                          gint               *minimum_width,
                                                          gint               *natural_width);


  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
  void (*_gtk_reserved5) (void);
  void (*_gtk_reserved6) (void);
  void (*_gtk_reserved7) (void);
  void (*_gtk_reserved8) (void);
};

GType              gtk_cell_area_get_type                       (void) G_GNUC_CONST;

/* Basic methods */
void               gtk_cell_area_add                            (GtkCellArea        *area,
								 GtkCellRenderer    *renderer);
void               gtk_cell_area_remove                         (GtkCellArea        *area,
								 GtkCellRenderer    *renderer);
void               gtk_cell_area_forall                         (GtkCellArea        *area,
								 GtkCellCallback     callback,
								 gpointer            callback_data);
void               gtk_cell_area_apply_attribute                (GtkCellArea        *area,
								 gint                attribute,
								 GValue             *value);
gint               gtk_cell_area_event                          (GtkCellArea        *area,
								 GtkWidget          *widget,
								 GdkEvent           *event,
								 const GdkRectangle *cell_area);
void               gtk_cell_area_render                         (GtkCellArea        *area,
								 cairo_t            *cr,
								 GtkWidget          *widget,
								 const GdkRectangle *cell_area);

/* Geometry */
GtkSizeRequestMode gtk_cell_area_get_request_mode               (GtkCellArea        *area);
void               gtk_cell_area_get_preferred_width            (GtkCellArea        *area,
								 GtkWidget          *widget,
								 gint               *minimum_size,
								 gint               *natural_size);
void               gtk_cell_area_get_preferred_height_for_width (GtkCellArea        *area,
								 GtkWidget          *widget,
								 gint                width,
								 gint               *minimum_height,
								 gint               *natural_height);
void               gtk_cell_area_get_preferred_height           (GtkCellArea        *area,
								 GtkWidget          *widget,
								 gint               *minimum_size,
								 gint               *natural_size);
void               gtk_cell_area_get_preferred_width_for_height (GtkCellArea        *area,
								 GtkWidget          *widget,
								 gint                height,
								 gint               *minimum_width,
								 gint               *natural_width);
void               gtk_cell_area_get_preferred_size             (GtkCellArea        *area,
								 GtkWidget          *widget,
								 GtkRequisition     *minimum_size,
								 GtkRequisition     *natural_size);


G_END_DECLS

#endif /* __GTK_CELL_AREA_H__ */
