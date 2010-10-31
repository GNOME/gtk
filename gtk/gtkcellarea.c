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

#include "config.h"

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "gtkcelllayout.h"
#include "gtkcellarea.h"
#include "gtkcellareaiter.h"

#include <gobject/gvaluecollector.h>


/* GObjectClass */
static void      gtk_cell_area_dispose                             (GObject            *object);
static void      gtk_cell_area_finalize                            (GObject            *object);

/* GtkCellAreaClass */
static void      gtk_cell_area_real_get_preferred_height_for_width (GtkCellArea        *area,
								    GtkCellAreaIter    *iter,
								    GtkWidget          *widget,
								    gint                width,
								    gint               *minimum_height,
								    gint               *natural_height);
static void      gtk_cell_area_real_get_preferred_width_for_height (GtkCellArea        *area,
								    GtkCellAreaIter    *iter,
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
							      gint                   column);
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

/* Attribute/Cell metadata */
typedef struct {
  const gchar *attribute;
  gint         column;
} CellAttribute;

typedef struct {
  GSList                *attributes;

  GtkCellLayoutDataFunc  func;
  gpointer               data;
  GDestroyNotify         destroy;
} CellInfo;

static CellInfo       *cell_info_new       (GtkCellLayoutDataFunc  func,
					    gpointer               data,
					    GDestroyNotify         destroy);
static void            cell_info_free      (CellInfo              *info);
static CellAttribute  *cell_attribute_new  (GtkCellRenderer       *renderer,
					    const gchar           *attribute,
					    gint                   column);
static void            cell_attribute_free (CellAttribute         *attribute);
static gint            cell_attribute_find (CellAttribute         *cell_attribute,
					    const gchar           *attribute);

/* Struct to pass data along while looping over 
 * cell renderers to apply attributes
 */
typedef struct {
  GtkCellArea  *area;
  GtkTreeModel *model;
  GtkTreeIter  *iter;
} AttributeData;

struct _GtkCellAreaPrivate
{
  GHashTable *cell_info;
};

/* Keep the paramspec pool internal, no need to deliver notifications
 * on cells. at least no percieved need for now */
static GParamSpecPool *cell_property_pool = NULL;

#define PARAM_SPEC_PARAM_ID(pspec)              ((pspec)->param_id)
#define PARAM_SPEC_SET_PARAM_ID(pspec, id)      ((pspec)->param_id = (id))


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

  priv->cell_info = g_hash_table_new_full (g_direct_hash, 
					   g_direct_equal,
					   NULL, 
					   (GDestroyNotify)cell_info_free);
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

  /* geometry */
  class->create_iter                    = NULL;
  class->get_request_mode               = NULL;
  class->get_preferred_width            = NULL;
  class->get_preferred_height           = NULL;
  class->get_preferred_height_for_width = gtk_cell_area_real_get_preferred_height_for_width;
  class->get_preferred_width_for_height = gtk_cell_area_real_get_preferred_width_for_height;

  /* Cell properties */
  if (!cell_property_pool)
    cell_property_pool = g_param_spec_pool_new (FALSE);

  g_type_class_add_private (object_class, sizeof (GtkCellAreaPrivate));
}

/*************************************************************
 *                    CellInfo Basics                        *
 *************************************************************/
static CellInfo *
cell_info_new (GtkCellLayoutDataFunc  func,
	       gpointer               data,
	       GDestroyNotify         destroy)
{
  CellInfo *info = g_slice_new (CellInfo);
  
  info->attributes = NULL;
  info->func       = func;
  info->data       = data;
  info->destroy    = destroy;

  return info;
}

static void
cell_info_free (CellInfo *info)
{
  if (info->destroy)
    info->destroy (info->data);

  g_slist_foreach (info->attributes, (GFunc)cell_attribute_free, NULL);
  g_slist_free (info->attributes);

  g_slice_free (CellInfo, info);
}

static CellAttribute  *
cell_attribute_new  (GtkCellRenderer       *renderer,
		     const gchar           *attribute,
		     gint                   column)
{
  GParamSpec *pspec;

  /* Check if the attribute really exists and point to
   * the property string installed on the cell renderer
   * class (dont dup the string) 
   */
  pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (renderer), attribute);

  if (pspec)
    {
      CellAttribute *cell_attribute = g_slice_new (CellAttribute);

      cell_attribute->attribute = pspec->name;
      cell_attribute->column    = column;

      return cell_attribute;
    }

  return NULL;
}

static void
cell_attribute_free (CellAttribute *attribute)
{
  g_slice_free (CellAttribute, attribute);
}

/* GCompareFunc for g_slist_find_custom() */
static gint
cell_attribute_find (CellAttribute *cell_attribute,
		     const gchar   *attribute)
{
  return g_strcmp0 (cell_attribute->attribute, attribute);
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
  g_hash_table_destroy (priv->cell_info);

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
						   GtkCellAreaIter    *iter,
						   GtkWidget          *widget,
						   gint                width,
						   gint               *minimum_height,
						   gint               *natural_height)
{
  /* If the area doesnt do height-for-width, fallback on base preferred height */
  GTK_CELL_AREA_GET_CLASS (area)->get_preferred_width (area, iter, widget, minimum_height, natural_height);
}

static void
gtk_cell_area_real_get_preferred_width_for_height (GtkCellArea        *area,
						   GtkCellAreaIter    *iter,
						   GtkWidget          *widget,
						   gint                height,
						   gint               *minimum_width,
						   gint               *natural_width)
{
  /* If the area doesnt do width-for-height, fallback on base preferred width */
  GTK_CELL_AREA_GET_CLASS (area)->get_preferred_width (area, iter, widget, minimum_width, natural_width);
}

/*************************************************************
 *                   GtkCellLayoutIface                      *
 *************************************************************/
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
			     gint                   column)
{
  gtk_cell_area_attribute_connect (GTK_CELL_AREA (cell_layout),
				   renderer, attribute, column);
}

static void
gtk_cell_area_set_cell_data_func (GtkCellLayout         *cell_layout,
				  GtkCellRenderer       *renderer,
				  GtkCellLayoutDataFunc  func,
				  gpointer               func_data,
				  GDestroyNotify         destroy)
{
  GtkCellArea        *area   = GTK_CELL_AREA (cell_layout);
  GtkCellAreaPrivate *priv   = area->priv;
  CellInfo           *info;

  info = g_hash_table_lookup (priv->cell_info, renderer);

  if (info)
    {
      if (info->destroy && info->data)
	info->destroy (info->data);

      if (func)
	{
	  info->func    = func;
	  info->data    = func_data;
	  info->destroy = destroy;
	}
      else
	{
	  info->func    = NULL;
	  info->data    = NULL;
	  info->destroy = NULL;
	}
    }
  else
    {
      info = cell_info_new (func, func_data, destroy);

      g_hash_table_insert (priv->cell_info, renderer, info);
    }
}

static void
gtk_cell_area_clear_attributes (GtkCellLayout         *cell_layout,
				GtkCellRenderer       *renderer)
{
  GtkCellArea        *area = GTK_CELL_AREA (cell_layout);
  GtkCellAreaPrivate *priv = area->priv;
  CellInfo           *info;

  info = g_hash_table_lookup (priv->cell_info, renderer);

  if (info)
    {
      g_slist_foreach (info->attributes, (GFunc)cell_attribute_free, NULL);
      g_slist_free (info->attributes);

      info->attributes = NULL;
    }
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
  GtkCellAreaClass   *class;
  GtkCellAreaPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  class = GTK_CELL_AREA_GET_CLASS (area);
  priv  = area->priv;

  /* Remove any custom attributes and custom cell data func here first */
  g_hash_table_remove (priv->cell_info, renderer);

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
		     GtkCellAreaIter    *iter,
		     GtkWidget          *widget,
		     GdkEvent           *event,
		     const GdkRectangle *cell_area)
{
  GtkCellAreaClass *class;

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), 0);
  g_return_val_if_fail (GTK_IS_CELL_AREA_ITER (iter), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);
  g_return_val_if_fail (event != NULL, 0);
  g_return_val_if_fail (cell_area != NULL, 0);

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->event)
    return class->event (area, iter, widget, event, cell_area);

  g_warning ("GtkCellAreaClass::event not implemented for `%s'", 
	     g_type_name (G_TYPE_FROM_INSTANCE (area)));
  return 0;
}

void
gtk_cell_area_render (GtkCellArea        *area,
		      GtkCellAreaIter    *iter,
		      GtkWidget          *widget,
		      cairo_t            *cr,
		      const GdkRectangle *cell_area)
{
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (cell_area != NULL);

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->render)
    class->render (area, iter, widget, cr, cell_area);
  else
    g_warning ("GtkCellAreaClass::render not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

/* Geometry */
GtkCellAreaIter   *
gtk_cell_area_create_iter (GtkCellArea *area)
{
  GtkCellAreaClass *class;

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), NULL);

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->create_iter)
    return class->create_iter (area);

  g_warning ("GtkCellAreaClass::create_iter not implemented for `%s'", 
	     g_type_name (G_TYPE_FROM_INSTANCE (area)));
  
  return NULL;
}


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
				   GtkCellAreaIter    *iter,
				   GtkWidget          *widget,
				   gint               *minimum_size,
				   gint               *natural_size)
{
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->get_preferred_width)
    class->get_preferred_width (area, iter, widget, minimum_size, natural_size);
  else
    g_warning ("GtkCellAreaClass::get_preferred_width not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

void
gtk_cell_area_get_preferred_height_for_width (GtkCellArea        *area,
					      GtkCellAreaIter    *iter,
					      GtkWidget          *widget,
					      gint                width,
					      gint               *minimum_height,
					      gint               *natural_height)
{
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  class = GTK_CELL_AREA_GET_CLASS (area);
  class->get_preferred_height_for_width (area, iter, widget, width, minimum_height, natural_height);
}

void
gtk_cell_area_get_preferred_height (GtkCellArea        *area,
				    GtkCellAreaIter    *iter,
				    GtkWidget          *widget,
				    gint               *minimum_size,
				    gint               *natural_size)
{
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->get_preferred_height)
    class->get_preferred_height (area, iter, widget, minimum_size, natural_size);
  else
    g_warning ("GtkCellAreaClass::get_preferred_height not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

void
gtk_cell_area_get_preferred_width_for_height (GtkCellArea        *area,
					      GtkCellAreaIter    *iter,
					      GtkWidget          *widget,
					      gint                height,
					      gint               *minimum_width,
					      gint               *natural_width)
{
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  class = GTK_CELL_AREA_GET_CLASS (area);
  class->get_preferred_width_for_height (area, iter, widget, height, minimum_width, natural_width);
}

/* Attributes */
void
gtk_cell_area_attribute_connect (GtkCellArea        *area,
				 GtkCellRenderer    *renderer,
				 const gchar        *attribute,
				 gint                column)
{ 
  GtkCellAreaPrivate *priv;
  CellInfo           *info;
  CellAttribute      *cell_attribute;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (attribute != NULL);

  priv = area->priv;
  info = g_hash_table_lookup (priv->cell_info, renderer);

  if (!info)
    {
      info = cell_info_new (NULL, NULL, NULL);

      g_hash_table_insert (priv->cell_info, renderer, info);
    }
  else
    {
      GSList *node;

      /* Check we are not adding the same attribute twice */
      if ((node = g_slist_find_custom (info->attributes, attribute,
				       (GCompareFunc)cell_attribute_find)) != NULL)
	{
	  cell_attribute = node->data;

	  g_warning ("Cannot connect attribute `%s' for cell renderer class `%s' "
		     "since `%s' is already attributed to column %d", 
		     attribute,
		     g_type_name (G_TYPE_FROM_INSTANCE (area)),
		     attribute, cell_attribute->column);
	  return;
	}
    }

  cell_attribute = cell_attribute_new (renderer, attribute, column);

  if (!cell_attribute)
    {
      g_warning ("Cannot connect attribute `%s' for cell renderer class `%s' "
		 "since attribute does not exist", 
		 attribute,
		 g_type_name (G_TYPE_FROM_INSTANCE (area)));
      return;
    }

  info->attributes = g_slist_prepend (info->attributes, cell_attribute);
}

void 
gtk_cell_area_attribute_disconnect (GtkCellArea        *area,
				    GtkCellRenderer    *renderer,
				    const gchar        *attribute)
{
  GtkCellAreaPrivate *priv;
  CellInfo           *info;
  CellAttribute      *cell_attribute;
  GSList             *node;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (attribute != NULL);

  priv = area->priv;
  info = g_hash_table_lookup (priv->cell_info, renderer);

  if (info)
    {
      node = g_slist_find_custom (info->attributes, attribute,
				  (GCompareFunc)cell_attribute_find);
      if (node)
	{
	  cell_attribute = node->data;

	  cell_attribute_free (cell_attribute);

	  info->attributes = g_slist_delete_link (info->attributes, node);
	}
    }
}

static void
apply_cell_attributes (GtkCellRenderer *renderer,
		       CellInfo        *info,
		       AttributeData   *data)
{
  CellAttribute *attribute;
  GSList        *list;
  GValue         value = { 0, };

  /* Apply the attributes directly to the renderer */
  for (list = info->attributes; list; list = list->next)
    {
      attribute = list->data;

      gtk_tree_model_get_value (data->model, data->iter, attribute->column, &value);
      g_object_set_property (G_OBJECT (renderer), attribute->attribute, &value);
      g_value_unset (&value);
    }

  /* Call any GtkCellLayoutDataFunc that may have been set by the user
   */
  if (info->func)
    info->func (GTK_CELL_LAYOUT (data->area), renderer,
		data->model, data->iter, info->data);
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

  /* Go over any cells that have attributes or custom GtkCellLayoutDataFuncs and
   * apply the data from the treemodel */
  g_hash_table_foreach (priv->cell_info, (GHFunc)apply_cell_attributes, &data);
}

/* Cell Properties */
void
gtk_cell_area_class_install_cell_property (GtkCellAreaClass   *aclass,
					   guint               property_id,
					   GParamSpec         *pspec)
{
  g_return_if_fail (GTK_IS_CELL_AREA_CLASS (aclass));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));
  if (pspec->flags & G_PARAM_WRITABLE)
    g_return_if_fail (aclass->set_cell_property != NULL);
  if (pspec->flags & G_PARAM_READABLE)
    g_return_if_fail (aclass->get_cell_property != NULL);
  g_return_if_fail (property_id > 0);
  g_return_if_fail (PARAM_SPEC_PARAM_ID (pspec) == 0);  /* paranoid */
  g_return_if_fail ((pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY)) == 0);

  if (g_param_spec_pool_lookup (cell_property_pool, pspec->name, G_OBJECT_CLASS_TYPE (aclass), TRUE))
    {
      g_warning (G_STRLOC ": class `%s' already contains a cell property named `%s'",
		 G_OBJECT_CLASS_NAME (aclass), pspec->name);
      return;
    }
  g_param_spec_ref (pspec);
  g_param_spec_sink (pspec);
  PARAM_SPEC_SET_PARAM_ID (pspec, property_id);
  g_param_spec_pool_insert (cell_property_pool, pspec, G_OBJECT_CLASS_TYPE (aclass));
}

GParamSpec*
gtk_cell_area_class_find_cell_property (GtkCellAreaClass   *aclass,
					const gchar        *property_name)
{
  g_return_val_if_fail (GTK_IS_CELL_AREA_CLASS (aclass), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  return g_param_spec_pool_lookup (cell_property_pool,
				   property_name,
				   G_OBJECT_CLASS_TYPE (aclass),
				   TRUE);
}

GParamSpec**
gtk_cell_area_class_list_cell_properties (GtkCellAreaClass   *aclass,
					  guint		    *n_properties)
{
  GParamSpec **pspecs;
  guint n;

  g_return_val_if_fail (GTK_IS_CELL_AREA_CLASS (aclass), NULL);

  pspecs = g_param_spec_pool_list (cell_property_pool,
				   G_OBJECT_CLASS_TYPE (aclass),
				   &n);
  if (n_properties)
    *n_properties = n;

  return pspecs;
}

void
gtk_cell_area_add_with_properties (GtkCellArea        *area,
				   GtkCellRenderer    *renderer,
				   const gchar        *first_prop_name,
				   ...)
{
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->add)
    {
      va_list var_args;

      class->add (area, renderer);

      va_start (var_args, first_prop_name);
      gtk_cell_area_cell_set_valist (area, renderer, first_prop_name, var_args);
      va_end (var_args);
    }
  else
    g_warning ("GtkCellAreaClass::add not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

void
gtk_cell_area_cell_set (GtkCellArea        *area,
			GtkCellRenderer    *renderer,
			const gchar        *first_prop_name,
			...)
{
  va_list var_args;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  va_start (var_args, first_prop_name);
  gtk_cell_area_cell_set_valist (area, renderer, first_prop_name, var_args);
  va_end (var_args);
}

void
gtk_cell_area_cell_get (GtkCellArea        *area,
			GtkCellRenderer    *renderer,
			const gchar        *first_prop_name,
			...)
{
  va_list var_args;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  va_start (var_args, first_prop_name);
  gtk_cell_area_cell_get_valist (area, renderer, first_prop_name, var_args);
  va_end (var_args);
}

static inline void
area_get_cell_property (GtkCellArea     *area,
			GtkCellRenderer *renderer,
			GParamSpec      *pspec,
			GValue          *value)
{
  GtkCellAreaClass *class = g_type_class_peek (pspec->owner_type);
  
  class->get_cell_property (area, renderer, PARAM_SPEC_PARAM_ID (pspec), value, pspec);
}

static inline void
area_set_cell_property (GtkCellArea     *area,
			GtkCellRenderer *renderer,
			GParamSpec      *pspec,
			const GValue    *value)
{
  GValue tmp_value = { 0, };
  GtkCellAreaClass *class = g_type_class_peek (pspec->owner_type);

  /* provide a copy to work from, convert (if necessary) and validate */
  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  if (!g_value_transform (value, &tmp_value))
    g_warning ("unable to set cell property `%s' of type `%s' from value of type `%s'",
	       pspec->name,
	       g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
	       G_VALUE_TYPE_NAME (value));
  else if (g_param_value_validate (pspec, &tmp_value) && !(pspec->flags & G_PARAM_LAX_VALIDATION))
    {
      gchar *contents = g_strdup_value_contents (value);

      g_warning ("value \"%s\" of type `%s' is invalid for property `%s' of type `%s'",
		 contents,
		 G_VALUE_TYPE_NAME (value),
		 pspec->name,
		 g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)));
      g_free (contents);
    }
  else
    {
      class->set_cell_property (area, renderer, PARAM_SPEC_PARAM_ID (pspec), &tmp_value, pspec);
    }
  g_value_unset (&tmp_value);
}

void
gtk_cell_area_cell_set_valist (GtkCellArea        *area,
			       GtkCellRenderer    *renderer,
			       const gchar        *first_property_name,
			       va_list             var_args)
{
  const gchar *name;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  name = first_property_name;
  while (name)
    {
      GValue value = { 0, };
      gchar *error = NULL;
      GParamSpec *pspec = 
	g_param_spec_pool_lookup (cell_property_pool, name,
				  G_OBJECT_TYPE (area), TRUE);
      if (!pspec)
	{
	  g_warning ("%s: cell area class `%s' has no cell property named `%s'",
		     G_STRLOC, G_OBJECT_TYPE_NAME (area), name);
	  break;
	}
      if (!(pspec->flags & G_PARAM_WRITABLE))
	{
	  g_warning ("%s: cell property `%s' of cell area class `%s' is not writable",
		     G_STRLOC, pspec->name, G_OBJECT_TYPE_NAME (area));
	  break;
	}

      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      G_VALUE_COLLECT (&value, var_args, 0, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);

	  /* we purposely leak the value here, it might not be
	   * in a sane state if an error condition occoured
	   */
	  break;
	}
      area_set_cell_property (area, renderer, pspec, &value);
      g_value_unset (&value);
      name = va_arg (var_args, gchar*);
    }
}

void
gtk_cell_area_cell_get_valist (GtkCellArea        *area,
			       GtkCellRenderer    *renderer,
			       const gchar        *first_property_name,
			       va_list             var_args)
{
  const gchar *name;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));

  name = first_property_name;
  while (name)
    {
      GValue value = { 0, };
      GParamSpec *pspec;
      gchar *error;

      pspec = g_param_spec_pool_lookup (cell_property_pool, name,
					G_OBJECT_TYPE (area), TRUE);
      if (!pspec)
	{
	  g_warning ("%s: cell area class `%s' has no cell property named `%s'",
		     G_STRLOC, G_OBJECT_TYPE_NAME (area), name);
	  break;
	}
      if (!(pspec->flags & G_PARAM_READABLE))
	{
	  g_warning ("%s: cell property `%s' of cell area class `%s' is not readable",
		     G_STRLOC, pspec->name, G_OBJECT_TYPE_NAME (area));
	  break;
	}

      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      area_get_cell_property (area, renderer, pspec, &value);
      G_VALUE_LCOPY (&value, var_args, 0, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);
	  g_value_unset (&value);
	  break;
	}
      g_value_unset (&value);
      name = va_arg (var_args, gchar*);
    }
}

void
gtk_cell_area_cell_set_property (GtkCellArea        *area,
				 GtkCellRenderer    *renderer,
				 const gchar        *property_name,
				 const GValue       *value)
{
  GParamSpec *pspec;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (G_IS_VALUE (value));
  
  pspec = g_param_spec_pool_lookup (cell_property_pool, property_name,
				    G_OBJECT_TYPE (area), TRUE);
  if (!pspec)
    g_warning ("%s: cell area class `%s' has no cell property named `%s'",
	       G_STRLOC, G_OBJECT_TYPE_NAME (area), property_name);
  else if (!(pspec->flags & G_PARAM_WRITABLE))
    g_warning ("%s: cell property `%s' of cell area class `%s' is not writable",
	       G_STRLOC, pspec->name, G_OBJECT_TYPE_NAME (area));
  else
    {
      area_set_cell_property (area, renderer, pspec, value);
    }
}

void
gtk_cell_area_cell_get_property (GtkCellArea        *area,
				 GtkCellRenderer    *renderer,
				 const gchar        *property_name,
				 GValue             *value)
{
  GParamSpec *pspec;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (G_IS_VALUE (value));
  
  pspec = g_param_spec_pool_lookup (cell_property_pool, property_name,
				    G_OBJECT_TYPE (area), TRUE);
  if (!pspec)
    g_warning ("%s: cell area class `%s' has no cell property named `%s'",
	       G_STRLOC, G_OBJECT_TYPE_NAME (area), property_name);
  else if (!(pspec->flags & G_PARAM_READABLE))
    g_warning ("%s: cell property `%s' of cell area class `%s' is not readable",
	       G_STRLOC, pspec->name, G_OBJECT_TYPE_NAME (area));
  else
    {
      GValue *prop_value, tmp_value = { 0, };

      /* auto-conversion of the callers value type
       */
      if (G_VALUE_TYPE (value) == G_PARAM_SPEC_VALUE_TYPE (pspec))
	{
	  g_value_reset (value);
	  prop_value = value;
	}
      else if (!g_value_type_transformable (G_PARAM_SPEC_VALUE_TYPE (pspec), G_VALUE_TYPE (value)))
	{
	  g_warning ("can't retrieve cell property `%s' of type `%s' as value of type `%s'",
		     pspec->name,
		     g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)),
		     G_VALUE_TYPE_NAME (value));
	  return;
	}
      else
	{
	  g_value_init (&tmp_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
	  prop_value = &tmp_value;
	}

      area_get_cell_property (area, renderer, pspec, prop_value);

      if (prop_value != value)
	{
	  g_value_transform (prop_value, value);
	  g_value_unset (&tmp_value);
	}
    }
}

