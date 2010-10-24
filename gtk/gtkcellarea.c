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

/* GObjectClass */
static void      gtk_cell_area_dispose                             (GObject            *object);
static void      gtk_cell_area_finalize                            (GObject            *object);

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
static void      gtk_cell_area_set_cell_data_func            (GtkCellLayout         *cell_layout,
							      GtkCellRenderer       *cell,
							      GtkCellLayoutDataFunc  func,
							      gpointer               func_data,
							      GDestroyNotify         destroy);
static void      gtk_cell_area_clear_attributes              (GtkCellLayout         *cell_layout,
							      GtkCellRenderer       *renderer);
static void      gtk_cell_area_reorder                       (GtkCellLayout         *cell_layout,
							      GtkCellRenderer       *cell,
							      gint                   position);
static GList    *gtk_cell_area_get_cells                     (GtkCellLayout         *cell_layout);

/* GtkCellLayoutDataFunc handling */
typedef struct {
  GtkCellLayoutDataFunc func;
  gpointer              data;
  GDestroyNotify        destroy;
} CustomCellData;

static CustomCellData *custom_cell_data_new  (GtkCellLayoutDataFunc  func,
					      gpointer               data,
					      GDestroyNotify         destroy);
static void            custom_cell_data_free (CustomCellData        *custom);

/* Struct to pass data while looping over 
 * cell renderer attributes
 */
typedef struct {
  GtkCellArea  *area;
  GtkTreeModel *model;
  GtkTreeIter  *iter;
} AttributeData;

struct _GtkCellAreaPrivate
{
  GHashTable *custom_cell_data;
};

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GtkCellArea, gtk_cell_area, G_TYPE_INITIALLY_UNOWNED,
				  G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
							 gtk_cell_area_cell_layout_init));

static void
gtk_cell_area_init (GtkCellArea *area)
{
  GtkCellAreaPrivate *priv;

  area->priv = G_TYPE_INSTANCE_GET_PRIVATE (area,
					    GTK_TYPE_CELL_AREA,
					    GtkCellAreaPrivate);
  priv = area->priv;

  priv->custom_cell_data = g_hash_table_new_full (g_direct_hash, 
						  g_direct_equal,
						  NULL, 
						  (GDestroyNotify)custom_cell_data_free);
}

static void 
gtk_cell_area_class_init (GtkCellAreaClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  
  /* GObjectClass */
  object_class->dispose  = gtk_cell_area_dispose;
  object_class->finalize = gtk_cell_area_finalize;

  /* general */
  class->add     = NULL;
  class->remove  = NULL;
  class->forall  = NULL;
  class->event   = NULL;
  class->render  = NULL;

  /* attributes */
  class->attribute_connect    = NULL;
  class->attribute_disconnect = NULL;
  class->attribute_forall     = NULL;

  /* geometry */
  class->get_request_mode               = NULL;
  class->get_preferred_width            = NULL;
  class->get_preferred_height           = NULL;
  class->get_preferred_height_for_width = gtk_cell_area_real_get_preferred_height_for_width;
  class->get_preferred_width_for_height = gtk_cell_area_real_get_preferred_width_for_height;

  g_type_class_add_private (object_class, sizeof (GtkCellAreaPrivate));
}


/*************************************************************
 *                      GObjectClass                         *
 *************************************************************/
static void
gtk_cell_area_finalize (GObject *object)
{
  GtkCellArea        *area   = GTK_CELL_AREA (object);
  GtkCellAreaPrivate *priv   = area->priv;

  /* All cell renderers should already be removed at this point,
   * just kill our hash table here. 
   */
  g_hash_table_destroy (priv->custom_cell_data);

  G_OBJECT_CLASS (gtk_cell_area_parent_class)->finalize (object);
}


static void
gtk_cell_area_dispose (GObject *object)
{
  /* This removes every cell renderer that may be added to the GtkCellArea,
   * subclasses should be breaking references to the GtkCellRenderers 
   * at this point.
   */
  gtk_cell_layout_clear (GTK_CELL_LAYOUT (object));

  G_OBJECT_CLASS (gtk_cell_area_parent_class)->dispose (object);
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
 *                   GtkCellLayoutIface                      *
 *************************************************************/
static CustomCellData *
custom_cell_data_new (GtkCellLayoutDataFunc  func,
		      gpointer               data,
		      GDestroyNotify         destroy)
{
  CustomCellData *custom = g_slice_new (CustomCellData);
  
  custom->func    = func;
  custom->data    = data;
  custom->destroy = destroy;

  return custom;
}

static void
custom_cell_data_free (CustomCellData *custom)
{
  if (custom->destroy)
    custom->destroy (custom->data);

  g_slice_free (CustomCellData, custom);
}

static void
gtk_cell_area_cell_layout_init (GtkCellLayoutIface *iface)
{
  iface->pack_start         = gtk_cell_area_pack_default;
  iface->pack_end           = gtk_cell_area_pack_default;
  iface->clear              = gtk_cell_area_clear;
  iface->add_attribute      = gtk_cell_area_add_attribute;
  iface->set_cell_data_func = gtk_cell_area_set_cell_data_func;
  iface->clear_attributes   = gtk_cell_area_clear_attributes;
  iface->reorder            = gtk_cell_area_reorder;
  iface->get_cells          = gtk_cell_area_get_cells;
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
gtk_cell_area_set_cell_data_func (GtkCellLayout         *cell_layout,
				  GtkCellRenderer       *cell,
				  GtkCellLayoutDataFunc  func,
				  gpointer               func_data,
				  GDestroyNotify         destroy)
{
  GtkCellArea        *area   = GTK_CELL_AREA (cell_layout);
  GtkCellAreaPrivate *priv   = area->priv;
  CustomCellData     *custom;

  if (func)
    {
      custom = custom_cell_data_new (func, func_data, destroy);
      g_hash_table_insert (priv->custom_cell_data, cell, custom);
    }
  else
    g_hash_table_remove (priv->custom_cell_data, cell);
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
gtk_cell_area_reorder (GtkCellLayout   *cell_layout,
		       GtkCellRenderer *cell,
		       gint             position)
{
  g_warning ("GtkCellLayout::reorder not implemented for `%s'", 
	     g_type_name (G_TYPE_FROM_INSTANCE (cell_layout)));
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
 *                            API                            *
 *************************************************************/

/* Basic methods */
void
gtk_cell_area_add (GtkCellArea        *area,
		   GtkCellRenderer    *renderer)
{
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->add)
    class->add (area, renderer);
  else
    g_warning ("GtkCellAreaClass::add not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

void
gtk_cell_area_remove (GtkCellArea        *area,
		      GtkCellRenderer    *renderer)
{
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  class = GTK_CELL_AREA_GET_CLASS (area);

  /* Remove any custom cell data func we have for this renderer */
  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (area),
				      renderer, NULL, NULL, NULL);

  if (class->remove)
    class->remove (area, renderer);
  else
    g_warning ("GtkCellAreaClass::remove not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

void
gtk_cell_area_forall (GtkCellArea        *area,
		      GtkCellCallback     callback,
		      gpointer            callback_data)
{
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (callback != NULL);

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->forall)
    class->forall (area, callback, callback_data);
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
  GtkCellAreaClass *class;

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);
  g_return_val_if_fail (event != NULL, 0);
  g_return_val_if_fail (cell_area != NULL, 0);

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->event)
    return class->event (area, widget, event, cell_area);

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
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (cell_area != NULL);

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->render)
    class->render (area, cr, widget, cell_area);
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
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (attribute != NULL);

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->attribute_connect)
    class->attribute_connect (area, renderer, attribute, id);
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
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (attribute != NULL);

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->attribute_disconnect)
    class->attribute_disconnect (area, renderer, attribute, id);
  else
    g_warning ("GtkCellAreaClass::attribute_disconnect not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

void
gtk_cell_area_attribute_forall (GtkCellArea             *area,
				GtkCellRenderer         *renderer,
				GtkCellAttributeCallback callback,
				gpointer                 user_data)
{
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (callback != NULL);

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->attribute_forall)
    class->attribute_forall (area, renderer, callback, user_data);
  else
    g_warning ("GtkCellAreaClass::attribute_forall not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}


/* Geometry */
GtkSizeRequestMode 
gtk_cell_area_get_request_mode (GtkCellArea *area)
{
  GtkCellAreaClass *class;

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), 
			GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH);

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->get_request_mode)
    return class->get_request_mode (area);

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
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->get_preferred_width)
    class->get_preferred_width (area, widget, minimum_size, natural_size);
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
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  class = GTK_CELL_AREA_GET_CLASS (area);
  class->get_preferred_height_for_width (area, widget, width, minimum_height, natural_height);
}

void
gtk_cell_area_get_preferred_height (GtkCellArea        *area,
				    GtkWidget          *widget,
				    gint               *minimum_size,
				    gint               *natural_size)
{
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->get_preferred_height)
    class->get_preferred_height (area, widget, minimum_size, natural_size);
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
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  class = GTK_CELL_AREA_GET_CLASS (area);
  class->get_preferred_width_for_height (area, widget, height, minimum_width, natural_width);
}


static void 
apply_attributes (GtkCellRenderer  *renderer,
		  const gchar      *attribute,
		  gint              id,
		  AttributeData    *data)
{
  GValue value = { 0, };

  /* For each attribute of each renderer we apply the value
   * from the model to the renderer here 
   */
  gtk_tree_model_get_value (data->model, data->iter, id, &value);
  g_object_set_property (G_OBJECT (renderer), attribute, &value);
  g_value_unset (&value);
}

static void 
apply_render_attributes (GtkCellRenderer *renderer,
			 AttributeData   *data)
{
  gtk_cell_area_attribute_forall (data->area, renderer, 
				  (GtkCellAttributeCallback)apply_attributes,
				  data);
}

static void
apply_custom_cell_data (GtkCellRenderer *renderer,
			CustomCellData  *custom,
			AttributeData   *data)
{
  g_assert (custom->func);

  /* For each renderer that has a GtkCellLayoutDataFunc set,
   * go ahead and envoke it to apply the data from the model 
   */
  custom->func (GTK_CELL_LAYOUT (data->area), renderer,
		data->model, data->iter, custom->data);
}

void
gtk_cell_area_apply_attributes (GtkCellArea  *area,
				GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
  GtkCellAreaPrivate *priv;
  AttributeData       data;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (iter != NULL);

  priv = area->priv;

  /* For every cell renderer, for every attribute, apply the attribute */
  data.area  = area;
  data.model = tree_model;
  data.iter  = iter;
  gtk_cell_area_forall (area, (GtkCellCallback)apply_render_attributes, &data);

  /* Now go over any custom cell data functions */
  g_hash_table_foreach (priv->custom_cell_data, (GHFunc)apply_custom_cell_data, &data);
}
