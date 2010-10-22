/* gtkcellarea.c
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

#include "gtkcelllayout.h"
#include "gtkcellarea.h"

/* GtkCellAreaClass */
static void      gtk_cell_area_real_get_preferred_height_for_width (GtkCellArea        *area,
								    GtkWidget          *widget,
								    gint                width,
								    gint               *minimum_height,
								    gint               *natural_height);
static void      gtk_cell_area_real_get_preferred_width_for_height (GtkCellArea        *area,
								    GtkWidget          *widget,
								    gint                height,
								    gint               *minimum_width,
								    gint               *natural_width);


/* GtkCellLayoutIface */
static void      gtk_cell_area_cell_layout_init              (GtkCellLayoutIface    *iface);
static void      gtk_cell_area_pack_default                  (GtkCellLayout         *cell_layout,
							      GtkCellRenderer       *renderer,
							      gboolean               expand);
static void      gtk_cell_area_clear                         (GtkCellLayout         *cell_layout);
static void      gtk_cell_area_add_attribute                 (GtkCellLayout         *cell_layout,
							      GtkCellRenderer       *renderer,
							      const gchar           *attribute,
							      gint                   id);
static void      gtk_cell_area_clear_attributes              (GtkCellLayout         *cell_layout,
							      GtkCellRenderer       *renderer);
static GList    *gtk_cell_area_get_cells                     (GtkCellLayout         *cell_layout);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GtkCellArea, gtk_cell_area, G_TYPE_INITIALLY_UNOWNED,
				  G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
							 gtk_cell_area_cell_layout_init));


static void
gtk_cell_area_init (GtkCellArea *area)
{

}

static void 
gtk_cell_area_class_init (GtkCellAreaClass *klass)
{
  /* general */
  klass->add                = NULL;
  klass->remove             = NULL;
  klass->forall             = NULL;
  klass->event              = NULL;
  klass->render             = NULL;

  /* attributes */
  klass->attribute_connect    = NULL;
  klass->attribute_disconnect = NULL;
  klass->attribute_apply      = NULL;
  klass->attribute_forall     = NULL;

  /* geometry */
  klass->get_request_mode               = NULL;
  klass->get_preferred_width            = NULL;
  klass->get_preferred_height           = NULL;
  klass->get_preferred_height_for_width = gtk_cell_area_real_get_preferred_height_for_width;
  klass->get_preferred_width_for_height = gtk_cell_area_real_get_preferred_width_for_height;
}


/*************************************************************
 *                   GtkCellLayoutIface                      *
 *************************************************************/
static void
gtk_cell_area_cell_layout_init (GtkCellLayoutIface *iface)
{
  iface->pack_start       = gtk_cell_area_pack_default;
  iface->pack_end         = gtk_cell_area_pack_default;
  iface->clear            = gtk_cell_area_clear;
  iface->add_attribute    = gtk_cell_area_add_attribute;
  iface->clear_attributes = gtk_cell_area_clear_attributes;
  iface->get_cells        = gtk_cell_area_get_cells;
}

static void
gtk_cell_area_pack_default (GtkCellLayout         *cell_layout,
			    GtkCellRenderer       *renderer,
			    gboolean               expand)
{
  gtk_cell_area_add (GTK_CELL_AREA (cell_layout), renderer);
}

static void
gtk_cell_area_clear (GtkCellLayout *cell_layout)
{
  GtkCellArea *area = GTK_CELL_AREA (cell_layout);
  GList *l, *cells  =
    gtk_cell_layout_get_cells (cell_layout);

  for (l = cells; l; l = l->next)
    {
      GtkCellRenderer *renderer = l->data;
      gtk_cell_area_remove (area, renderer);
    }

  g_list_free (cells);
}


static void
gtk_cell_area_add_attribute (GtkCellLayout         *cell_layout,
			     GtkCellRenderer       *renderer,
			     const gchar           *attribute,
			     gint                   id)
{
  gtk_cell_area_attribute_connect (GTK_CELL_AREA (cell_layout),
				   renderer, attribute, id);
}


typedef struct {
  const gchar *attribute;
  gchar        id;
} CellAttribute;

static void
accum_attributes (GtkCellArea      *area,
		  GtkCellRenderer  *renderer,
		  const gchar      *attribute,
		  gint              id,
		  GList           **accum)
{
  CellAttribute *attrib = g_slice_new (CellAttribute);

  attrib->attribute = attribute;
  attrib->id        = id;

  *accum = g_list_prepend (*accum, attrib);
}

static void
gtk_cell_area_clear_attributes (GtkCellLayout         *cell_layout,
				GtkCellRenderer       *renderer)
{
  GtkCellArea *area = GTK_CELL_AREA (cell_layout);
  GList       *l, *attributes = NULL;

  /* Get a list of attributes so we dont modify the list inline */
  gtk_cell_area_attribute_forall (area, renderer, 
				  (GtkCellAttributeCallback)accum_attributes,
				  &attributes);

  for (l = attributes; l; l = l->next)
    {
      CellAttribute *attrib = l->data;

      gtk_cell_area_attribute_disconnect (area, renderer, 
					  attrib->attribute, attrib->id);

      g_slice_free (CellAttribute, attrib);
    }

  g_list_free (attributes);
}


static void
accum_cells (GtkCellRenderer *renderer,
	     GList          **accum)
{
  *accum = g_list_prepend (*accum, renderer);
}

static GList *
gtk_cell_area_get_cells (GtkCellLayout *cell_layout)
{
  GList *cells = NULL;

  gtk_cell_area_forall (GTK_CELL_AREA (cell_layout), 
			(GtkCellCallback)accum_cells,
			&cells);

  return g_list_reverse (cells);
}


/*************************************************************
 *                    GtkCellAreaClass                       *
 *************************************************************/
static void
gtk_cell_area_real_get_preferred_height_for_width (GtkCellArea        *area,
						   GtkWidget          *widget,
						   gint                width,
						   gint               *minimum_height,
						   gint               *natural_height)
{
  /* If the area doesnt do height-for-width, fallback on base preferred height */
  GTK_CELL_AREA_GET_CLASS (area)->get_preferred_width (area, widget, minimum_height, natural_height);
}

static void
gtk_cell_area_real_get_preferred_width_for_height (GtkCellArea        *area,
						   GtkWidget          *widget,
						   gint                height,
						   gint               *minimum_width,
						   gint               *natural_width)
{
  /* If the area doesnt do width-for-height, fallback on base preferred width */
  GTK_CELL_AREA_GET_CLASS (area)->get_preferred_width (area, widget, minimum_width, natural_width);
}

/*************************************************************
 *                            API                            *
 *************************************************************/

/* Basic methods */
void
gtk_cell_area_add (GtkCellArea        *area,
		   GtkCellRenderer    *renderer)
{
  GtkCellAreaClass *klass;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  klass = GTK_CELL_AREA_GET_CLASS (area);

  if (klass->add)
    klass->add (area, renderer);
  else
    g_warning ("GtkCellAreaClass::add not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

void
gtk_cell_area_remove (GtkCellArea        *area,
		      GtkCellRenderer    *renderer)
{
  GtkCellAreaClass *klass;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  klass = GTK_CELL_AREA_GET_CLASS (area);

  if (klass->remove)
    klass->remove (area, renderer);
  else
    g_warning ("GtkCellAreaClass::remove not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

void
gtk_cell_area_forall (GtkCellArea        *area,
		      GtkCellCallback     callback,
		      gpointer            callback_data)
{
  GtkCellAreaClass *klass;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (callback != NULL);

  klass = GTK_CELL_AREA_GET_CLASS (area);

  if (klass->forall)
    klass->forall (area, callback, callback_data);
  else
    g_warning ("GtkCellAreaClass::forall not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

gint
gtk_cell_area_event (GtkCellArea        *area,
		     GtkWidget          *widget,
		     GdkEvent           *event,
		     const GdkRectangle *cell_area)
{
  GtkCellAreaClass *klass;

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);
  g_return_val_if_fail (event != NULL, 0);
  g_return_val_if_fail (cell_area != NULL, 0);

  klass = GTK_CELL_AREA_GET_CLASS (area);

  if (klass->event)
    return klass->event (area, widget, event, cell_area);

  g_warning ("GtkCellAreaClass::event not implemented for `%s'", 
	     g_type_name (G_TYPE_FROM_INSTANCE (area)));
  return 0;
}

void
gtk_cell_area_render (GtkCellArea        *area,
		      cairo_t            *cr,
		      GtkWidget          *widget,
		      const GdkRectangle *cell_area)
{
  GtkCellAreaClass *klass;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (cell_area != NULL);

  klass = GTK_CELL_AREA_GET_CLASS (area);

  if (klass->render)
    klass->render (area, cr, widget, cell_area);
  else
    g_warning ("GtkCellAreaClass::render not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}


/* Attributes */ 
void
gtk_cell_area_attribute_connect (GtkCellArea        *area,
				 GtkCellRenderer    *renderer,
				 const gchar        *attribute,
				 gint                id)
{
  GtkCellAreaClass *klass;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (attribute != NULL);

  klass = GTK_CELL_AREA_GET_CLASS (area);

  if (klass->attribute_connect)
    klass->attribute_connect (area, renderer, attribute, id);
  else
    g_warning ("GtkCellAreaClass::attribute_connect not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

void 
gtk_cell_area_attribute_disconnect (GtkCellArea        *area,
				    GtkCellRenderer    *renderer,
				    const gchar        *attribute,
				    gint                id)
{
  GtkCellAreaClass *klass;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (attribute != NULL);

  klass = GTK_CELL_AREA_GET_CLASS (area);

  if (klass->attribute_disconnect)
    klass->attribute_disconnect (area, renderer, attribute, id);
  else
    g_warning ("GtkCellAreaClass::attribute_disconnect not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

void
gtk_cell_area_attribute_apply (GtkCellArea        *area,
			       gint                id,
			       GValue             *value)
{
  GtkCellAreaClass *klass;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (G_IS_VALUE (value));

  klass = GTK_CELL_AREA_GET_CLASS (area);

  if (klass->attribute_apply)
    klass->attribute_apply (area, id, value);
  else
    g_warning ("GtkCellAreaClass::attribute_apply not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

void
gtk_cell_area_attribute_forall (GtkCellArea             *area,
				GtkCellRenderer         *renderer,
				GtkCellAttributeCallback callback,
				gpointer                 user_data)
{
  GtkCellAreaClass *klass;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (callback != NULL);

  klass = GTK_CELL_AREA_GET_CLASS (area);

  if (klass->attribute_forall)
    klass->attribute_forall (area, renderer, callback, user_data);
  else
    g_warning ("GtkCellAreaClass::attribute_forall not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}


/* Geometry */
GtkSizeRequestMode 
gtk_cell_area_get_request_mode (GtkCellArea *area)
{
  GtkCellAreaClass *klass;

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), 
			GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH);

  klass = GTK_CELL_AREA_GET_CLASS (area);

  if (klass->get_request_mode)
    return klass->get_request_mode (area);

  g_warning ("GtkCellAreaClass::get_request_mode not implemented for `%s'", 
	     g_type_name (G_TYPE_FROM_INSTANCE (area)));
  
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

void
gtk_cell_area_get_preferred_width (GtkCellArea        *area,
				   GtkWidget          *widget,
				   gint               *minimum_size,
				   gint               *natural_size)
{
  GtkCellAreaClass *klass;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  klass = GTK_CELL_AREA_GET_CLASS (area);

  if (klass->get_preferred_width)
    klass->get_preferred_width (area, widget, minimum_size, natural_size);
  else
    g_warning ("GtkCellAreaClass::get_preferred_width not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

void
gtk_cell_area_get_preferred_height_for_width (GtkCellArea        *area,
					      GtkWidget          *widget,
					      gint                width,
					      gint               *minimum_height,
					      gint               *natural_height)
{
  GtkCellAreaClass *klass;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  klass = GTK_CELL_AREA_GET_CLASS (area);
  klass->get_preferred_height_for_width (area, widget, width, minimum_height, natural_height);
}

void
gtk_cell_area_get_preferred_height (GtkCellArea        *area,
				    GtkWidget          *widget,
				    gint               *minimum_size,
				    gint               *natural_size)
{
  GtkCellAreaClass *klass;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  klass = GTK_CELL_AREA_GET_CLASS (area);

  if (klass->get_preferred_height)
    klass->get_preferred_height (area, widget, minimum_size, natural_size);
  else
    g_warning ("GtkCellAreaClass::get_preferred_height not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

void
gtk_cell_area_get_preferred_width_for_height (GtkCellArea        *area,
					      GtkWidget          *widget,
					      gint                height,
					      gint               *minimum_width,
					      gint               *natural_width)
{
  GtkCellAreaClass *klass;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  klass = GTK_CELL_AREA_GET_CLASS (area);
  klass->get_preferred_width_for_height (area, widget, height, minimum_width, natural_width);
}
