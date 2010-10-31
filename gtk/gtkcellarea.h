/* gtkcellarea.h
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

#ifndef __GTK_CELL_AREA_H__
#define __GTK_CELL_AREA_H__

#include <gtk/gtkcellrenderer.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtktreemodel.h>

G_BEGIN_DECLS

#define GTK_TYPE_CELL_AREA		  (gtk_cell_area_get_type ())
#define GTK_CELL_AREA(obj)		  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CELL_AREA, GtkCellArea))
#define GTK_CELL_AREA_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CELL_AREA, GtkCellAreaClass))
#define GTK_IS_CELL_AREA(obj)	  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CELL_AREA))
#define GTK_IS_CELL_AREA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CELL_AREA))
#define GTK_CELL_AREA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CELL_AREA, GtkCellAreaClass))

typedef struct _GtkCellArea              GtkCellArea;
typedef struct _GtkCellAreaClass         GtkCellAreaClass;
typedef struct _GtkCellAreaPrivate       GtkCellAreaPrivate;
typedef struct _GtkCellAreaIter          GtkCellAreaIter;

/**
 * GtkCellCallback:
 * @renderer: the cell renderer to operate on
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

  GtkCellAreaPrivate *priv;
};

struct _GtkCellAreaClass
{
  GInitiallyUnownedClass parent_class;

  /* vtable - not signals */

  /* Basic methods */
  void               (* add)                             (GtkCellArea             *area,
							  GtkCellRenderer         *renderer);
  void               (* remove)                          (GtkCellArea             *area,
							  GtkCellRenderer         *renderer);
  void               (* forall)                          (GtkCellArea             *area,
							  GtkCellCallback          callback,
							  gpointer                 callback_data);
  gint               (* event)                           (GtkCellArea             *area,
							  GtkCellAreaIter         *iter,
							  GtkWidget               *widget,
							  GdkEvent                *event,
							  const GdkRectangle      *cell_area);
  void               (* render)                          (GtkCellArea             *area,
							  GtkCellAreaIter         *iter,
							  GtkWidget               *widget,
							  cairo_t                 *cr,
							  const GdkRectangle      *cell_area);

  /* Geometry */
  GtkCellAreaIter   *(* create_iter)                     (GtkCellArea             *area);
  GtkSizeRequestMode (* get_request_mode)                (GtkCellArea             *area);
  void               (* get_preferred_width)             (GtkCellArea             *area,
							  GtkCellAreaIter         *iter,
                                                          GtkWidget               *widget,
                                                          gint                    *minimum_size,
                                                          gint                    *natural_size);
  void               (* get_preferred_height_for_width)  (GtkCellArea             *area,
							  GtkCellAreaIter         *iter,
                                                          GtkWidget               *widget,
                                                          gint                     width,
                                                          gint                    *minimum_height,
                                                          gint                    *natural_height);
  void               (* get_preferred_height)            (GtkCellArea             *area,
							  GtkCellAreaIter         *iter,
                                                          GtkWidget               *widget,
                                                          gint                    *minimum_size,
                                                          gint                    *natural_size);
  void               (* get_preferred_width_for_height)  (GtkCellArea             *area,
							  GtkCellAreaIter         *iter,
                                                          GtkWidget               *widget,
                                                          gint                     height,
                                                          gint                    *minimum_width,
                                                          gint                    *natural_width);

  /* Cell Properties */
  void               (* set_cell_property)               (GtkCellArea             *area,
							  GtkCellRenderer         *renderer,
							  guint                    property_id,
							  const GValue            *value,
							  GParamSpec              *pspec);
  void               (* get_cell_property)               (GtkCellArea             *area,
							  GtkCellRenderer         *renderer,
							  guint                    property_id,
							  GValue                  *value,
							  GParamSpec              *pspec);

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
gint               gtk_cell_area_event                          (GtkCellArea        *area,
								 GtkCellAreaIter    *iter,
								 GtkWidget          *widget,
								 GdkEvent           *event,
								 const GdkRectangle *cell_area);
void               gtk_cell_area_render                         (GtkCellArea        *area,
								 GtkCellAreaIter    *iter,
								 GtkWidget          *widget,
								 cairo_t            *cr,
								 const GdkRectangle *cell_area);

/* Geometry */
GtkCellAreaIter   *gtk_cell_area_create_iter                    (GtkCellArea        *area);
GtkSizeRequestMode gtk_cell_area_get_request_mode               (GtkCellArea        *area);
void               gtk_cell_area_get_preferred_width            (GtkCellArea        *area,
								 GtkCellAreaIter    *iter,
								 GtkWidget          *widget,
								 gint               *minimum_size,
								 gint               *natural_size);
void               gtk_cell_area_get_preferred_height_for_width (GtkCellArea        *area,
								 GtkCellAreaIter    *iter,
								 GtkWidget          *widget,
								 gint                width,
								 gint               *minimum_height,
								 gint               *natural_height);
void               gtk_cell_area_get_preferred_height           (GtkCellArea        *area,
								 GtkCellAreaIter    *iter,
								 GtkWidget          *widget,
								 gint               *minimum_size,
								 gint               *natural_size);
void               gtk_cell_area_get_preferred_width_for_height (GtkCellArea        *area,
								 GtkCellAreaIter    *iter,
								 GtkWidget          *widget,
								 gint                height,
								 gint               *minimum_width,
								 gint               *natural_width);

/* Attributes */
void               gtk_cell_area_apply_attributes               (GtkCellArea        *area,
								 GtkTreeModel       *tree_model,
								 GtkTreeIter        *iter);
void               gtk_cell_area_attribute_connect              (GtkCellArea        *area,
								 GtkCellRenderer    *renderer,
								 const gchar        *attribute,
								 gint                column); 
void               gtk_cell_area_attribute_disconnect           (GtkCellArea        *area,
								 GtkCellRenderer    *renderer,
								 const gchar        *attribute);

/* Cell Properties */
void               gtk_cell_area_class_install_cell_property    (GtkCellAreaClass   *aclass,
								 guint               property_id,
								 GParamSpec         *pspec);
GParamSpec*        gtk_cell_area_class_find_cell_property       (GtkCellAreaClass   *aclass,
								 const gchar        *property_name);
GParamSpec**       gtk_cell_area_class_list_cell_properties     (GtkCellAreaClass   *aclass,
								 guint		    *n_properties);
void               gtk_cell_area_add_with_properties            (GtkCellArea        *area,
								 GtkCellRenderer    *renderer,
								 const gchar	    *first_prop_name,
								 ...) G_GNUC_NULL_TERMINATED;
void               gtk_cell_area_cell_set                       (GtkCellArea        *area,
								 GtkCellRenderer    *renderer,
								 const gchar        *first_prop_name,
								 ...) G_GNUC_NULL_TERMINATED;
void               gtk_cell_area_cell_get                       (GtkCellArea        *area,
								 GtkCellRenderer    *renderer,
								 const gchar        *first_prop_name,
								 ...) G_GNUC_NULL_TERMINATED;
void               gtk_cell_area_cell_set_valist                (GtkCellArea        *area,
								 GtkCellRenderer    *renderer,
								 const gchar        *first_property_name,
								 va_list             var_args);
void               gtk_cell_area_cell_get_valist                (GtkCellArea        *area,
								 GtkCellRenderer    *renderer,
								 const gchar        *first_property_name,
								 va_list             var_args);
void               gtk_cell_area_cell_set_property              (GtkCellArea        *area,
								 GtkCellRenderer    *renderer,
								 const gchar        *property_name,
								 const GValue       *value);
void               gtk_cell_area_cell_get_property              (GtkCellArea        *area,
								 GtkCellRenderer    *renderer,
								 const gchar        *property_name,
								 GValue             *value);

#define GTK_CELL_AREA_WARN_INVALID_CHILD_PROPERTY_ID(object, property_id, pspec) \
  G_OBJECT_WARN_INVALID_PSPEC ((object), "cell property id", (property_id), (pspec))

/* Margins */
gint               gtk_cell_area_get_cell_margin_left           (GtkCellArea        *area);
void               gtk_cell_area_set_cell_margin_left           (GtkCellArea        *area,
								 gint                margin);
gint               gtk_cell_area_get_cell_margin_right          (GtkCellArea        *area);
void               gtk_cell_area_set_cell_margin_right          (GtkCellArea        *area,
								 gint                margin);
gint               gtk_cell_area_get_cell_margin_top            (GtkCellArea        *area);
void               gtk_cell_area_set_cell_margin_top            (GtkCellArea        *area,
								 gint                margin);
gint               gtk_cell_area_get_cell_margin_bottom         (GtkCellArea        *area);
void               gtk_cell_area_set_cell_margin_bottom         (GtkCellArea        *area,
								 gint                margin);

/* Functions for area implementations */

/* Distinguish the inner cell area from the whole requested area including margins */
void               gtk_cell_area_inner_cell_area                (GtkCellArea        *area,
								 GdkRectangle       *cell_area,
								 GdkRectangle       *inner_cell_area);

/* Request the size of a cell while respecting the cell margins (requests are margin inclusive) */
void               gtk_cell_area_request_renderer               (GtkCellArea        *area,
								 GtkCellRenderer    *renderer,
								 GtkOrientation      orientation,
								 GtkWidget          *widget,
								 gint                for_size,
								 gint               *minimum_size,
								 gint               *natural_size);

G_END_DECLS

#endif /* __GTK_CELL_AREA_H__ */
