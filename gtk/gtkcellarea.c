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

#include "gtkintl.h"
#include "gtkcelllayout.h"
#include "gtkcellarea.h"
#include "gtkcellareaiter.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"

#include <gobject/gvaluecollector.h>


/* GObjectClass */
static void      gtk_cell_area_dispose                             (GObject            *object);
static void      gtk_cell_area_finalize                            (GObject            *object);
static void      gtk_cell_area_set_property                        (GObject            *object,
								    guint               prop_id,
								    const GValue       *value,
								    GParamSpec         *pspec);
static void      gtk_cell_area_get_property                        (GObject            *object,
								    guint               prop_id,
								    GValue             *value,
								    GParamSpec         *pspec);

/* GtkCellAreaClass */
static gint      gtk_cell_area_real_event                          (GtkCellArea          *area,
								    GtkCellAreaIter      *iter,
								    GtkWidget            *widget,
								    GdkEvent             *event,
								    const GdkRectangle   *cell_area,
								    GtkCellRendererState  flags);
static void      gtk_cell_area_real_get_preferred_height_for_width (GtkCellArea           *area,
								    GtkCellAreaIter       *iter,
								    GtkWidget             *widget,
								    gint                   width,
								    gint                  *minimum_height,
								    gint                  *natural_height);
static void      gtk_cell_area_real_get_preferred_width_for_height (GtkCellArea           *area,
								    GtkCellAreaIter       *iter,
								    GtkWidget             *widget,
								    gint                   height,
								    gint                  *minimum_width,
								    gint                  *natural_width);
static void      gtk_cell_area_real_update_focus                   (GtkCellArea           *area);

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
  gboolean      is_expander;
  gboolean      is_expanded;
} AttributeData;

struct _GtkCellAreaPrivate
{
  /* The GtkCellArea bookkeeps any connected 
   * attributes in this hash table.
   */
  GHashTable      *cell_info;

  /* The cell border decides how much space to reserve
   * around each cell for the background_area
   */
  GtkBorder        cell_border;

  /* Current path is saved as a side-effect
   * of gtk_cell_area_apply_attributes() */
  gchar           *current_path;

  GtkCellRenderer *edited_cell;
  GtkCellRenderer *focus_cell;
  guint            can_focus : 1;

};

enum {
  PROP_0,
  PROP_CELL_MARGIN_LEFT,
  PROP_CELL_MARGIN_RIGHT,
  PROP_CELL_MARGIN_TOP,
  PROP_CELL_MARGIN_BOTTOM,
  PROP_FOCUS_CELL,
  PROP_EDITED_CELL
};

enum {
  SIGNAL_FOCUS_LEAVE,
  SIGNAL_EDITING_STARTED,
  LAST_SIGNAL
};

/* Keep the paramspec pool internal, no need to deliver notifications
 * on cells. at least no percieved need for now */
static GParamSpecPool *cell_property_pool = NULL;
static guint           cell_area_signals[LAST_SIGNAL] = { 0 };

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

  priv->cell_border.left   = 0;
  priv->cell_border.right  = 0;
  priv->cell_border.top    = 0;
  priv->cell_border.bottom = 0;

  priv->focus_cell         = NULL;
}

static void 
gtk_cell_area_class_init (GtkCellAreaClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  
  /* GObjectClass */
  object_class->dispose      = gtk_cell_area_dispose;
  object_class->finalize     = gtk_cell_area_finalize;
  object_class->get_property = gtk_cell_area_get_property;
  object_class->set_property = gtk_cell_area_set_property;

  /* general */
  class->add     = NULL;
  class->remove  = NULL;
  class->forall  = NULL;
  class->event   = gtk_cell_area_real_event;
  class->render  = NULL;

  /* geometry */
  class->create_iter                    = NULL;
  class->get_request_mode               = NULL;
  class->get_preferred_width            = NULL;
  class->get_preferred_height           = NULL;
  class->get_preferred_height_for_width = gtk_cell_area_real_get_preferred_height_for_width;
  class->get_preferred_width_for_height = gtk_cell_area_real_get_preferred_width_for_height;

  /* focus */
  class->grab_focus = NULL;
  class->update_focus = gtk_cell_area_real_update_focus;

  /* Signals */
  cell_area_signals[SIGNAL_FOCUS_LEAVE] =
    g_signal_new (I_("focus-leave"),
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_LAST,
		  0, /* Class offset (just a notification, no class handler) */
		  NULL, NULL,
		  _gtk_marshal_VOID__ENUM_STRING,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_DIRECTION_TYPE, G_TYPE_STRING);

  cell_area_signals[SIGNAL_EDITING_STARTED] =
    g_signal_new (I_("editing-started"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  0, /* No class closure here */
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_OBJECT_STRING,
		  G_TYPE_NONE, 3,
		  GTK_TYPE_CELL_RENDERER,
		  GTK_TYPE_CELL_EDITABLE,
		  G_TYPE_STRING);

  /* Properties */
  g_object_class_install_property (object_class,
                                   PROP_CELL_MARGIN_LEFT,
                                   g_param_spec_int
				   ("cell-margin-left",
				    P_("Margin on Left"),
				    P_("Pixels of extra space on the left side of each cell"),
				    0,
				    G_MAXINT16,
				    0,
				    GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_CELL_MARGIN_RIGHT,
                                   g_param_spec_int
				   ("cell-margin-right",
				    P_("Margin on Right"),
				    P_("Pixels of extra space on the right side of each cell"),
				    0,
				    G_MAXINT16,
				    0,
				    GTK_PARAM_READWRITE));
  
  g_object_class_install_property (object_class,
                                   PROP_CELL_MARGIN_TOP,
                                   g_param_spec_int 
				   ("cell-margin-top",
				    P_("Margin on Top"),
				    P_("Pixels of extra space on the top side of each cell"),
				    0,
				    G_MAXINT16,
				    0,
				    GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_CELL_MARGIN_BOTTOM,
                                   g_param_spec_int
				   ("cell-margin-bottom",
				    P_("Margin on Bottom"),
				    P_("Pixels of extra space on the bottom side of each cell"),
				    0,
				    G_MAXINT16,
				    0,
				    GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_FOCUS_CELL,
                                   g_param_spec_object
				   ("focus-cell",
				    P_("Focus Cell"),
				    P_("The cell which currently has focus"),
				    GTK_TYPE_CELL_RENDERER,
				    GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_EDITED_CELL,
                                   g_param_spec_object
				   ("edited-cell",
				    P_("Edited Cell"),
				    P_("The cell which is currently being edited"),
				    GTK_TYPE_CELL_RENDERER,
				    GTK_PARAM_READWRITE));

  /* Pool for Cell Properties */
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

  g_free (priv->current_path);

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

  /* Remove any ref to a focused/edited cell */
  gtk_cell_area_set_focus_cell (GTK_CELL_AREA (object), NULL);
  gtk_cell_area_set_edited_cell (GTK_CELL_AREA (object), NULL);

  G_OBJECT_CLASS (gtk_cell_area_parent_class)->dispose (object);
}

static void
gtk_cell_area_set_property (GObject       *object,
			    guint          prop_id,
			    const GValue  *value,
			    GParamSpec    *pspec)
{
  GtkCellArea *area = GTK_CELL_AREA (object);

  switch (prop_id)
    {
    case PROP_CELL_MARGIN_LEFT:
      gtk_cell_area_set_cell_margin_left (area, g_value_get_int (value));
      break;
    case PROP_CELL_MARGIN_RIGHT:
      gtk_cell_area_set_cell_margin_right (area, g_value_get_int (value));
      break;
    case PROP_CELL_MARGIN_TOP:
      gtk_cell_area_set_cell_margin_top (area, g_value_get_int (value));
      break;
    case PROP_CELL_MARGIN_BOTTOM:
      gtk_cell_area_set_cell_margin_bottom (area, g_value_get_int (value));
      break;
    case PROP_FOCUS_CELL:
      gtk_cell_area_set_focus_cell (area, (GtkCellRenderer *)g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_cell_area_get_property (GObject     *object,
			    guint        prop_id,
			    GValue      *value,
			    GParamSpec  *pspec)
{
  GtkCellArea        *area = GTK_CELL_AREA (object);
  GtkCellAreaPrivate *priv = area->priv;

  switch (prop_id)
    {
    case PROP_CELL_MARGIN_LEFT:
      g_value_set_int (value, priv->cell_border.left);
      break;
    case PROP_CELL_MARGIN_RIGHT:
      g_value_set_int (value, priv->cell_border.right);
      break;
    case PROP_CELL_MARGIN_TOP:
      g_value_set_int (value, priv->cell_border.top);
      break;
    case PROP_CELL_MARGIN_BOTTOM:
      g_value_set_int (value, priv->cell_border.bottom);
      break;
    case PROP_FOCUS_CELL:
      g_value_set_object (value, priv->focus_cell);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/*************************************************************
 *                    GtkCellAreaClass                       *
 *************************************************************/
static gint
gtk_cell_area_real_event (GtkCellArea          *area,
			  GtkCellAreaIter      *iter,
			  GtkWidget            *widget,
			  GdkEvent             *event,
			  const GdkRectangle   *cell_area,
			  GtkCellRendererState  flags)
{
  if (event->type == GDK_KEY_PRESS && (flags & GTK_CELL_RENDERER_FOCUSED) != 0)
    {
      GdkEventKey        *key_event = (GdkEventKey *)event;
      GtkCellAreaPrivate *priv = area->priv;

      if (priv->focus_cell && 
	  (key_event->keyval == GDK_KEY_space ||
	   key_event->keyval == GDK_KEY_KP_Space ||
	   key_event->keyval == GDK_KEY_Return ||
	   key_event->keyval == GDK_KEY_ISO_Enter ||
	   key_event->keyval == GDK_KEY_KP_Enter))
	{
	  /* Activate or Edit the currently focused cell */
	  GtkCellRendererMode mode;
	  GdkRectangle        background_area;
	  GdkRectangle        inner_area;

	  /* Get the allocation of the focused cell.
	   */
	  gtk_cell_area_get_cell_allocation (area, iter, widget, priv->focus_cell,
					     cell_area, &background_area);

	  /* Remove margins from the background area to produce the cell area.
	   */
	  gtk_cell_area_inner_cell_area (area, &background_area, &inner_area);

	  /* XXX Need to do some extra right-to-left casing either here 
	   * or inside the above called apis.
	   */

	  g_object_get (priv->focus_cell, "mode", &mode, NULL);

	  if (mode == GTK_CELL_RENDERER_MODE_ACTIVATABLE)
	    {
	      if (gtk_cell_renderer_activate (priv->focus_cell,
					      event, widget,
					      priv->current_path,
					      &background_area,
					      &inner_area,
					      flags))
		  return TRUE;
	    }
	  else if (mode == GTK_CELL_RENDERER_MODE_EDITABLE)
	    {
	      GtkCellEditable *editable_widget;

	      editable_widget =
		gtk_cell_renderer_start_editing (priv->focus_cell,
						 event, widget,
						 priv->current_path,
						 &background_area,
						 &inner_area,
						 flags);

	      if (editable_widget != NULL)
		{
		  g_return_val_if_fail (GTK_IS_CELL_EDITABLE (editable_widget), FALSE);

		  gtk_cell_area_set_edited_cell (area, priv->focus_cell);

		  /* Signal that editing started so that callers can get 
		   * a handle on the editable_widget */
		  gtk_cell_area_editing_started (area, priv->focus_cell, editable_widget);
		  
		  return TRUE;
		}
	    }
	}
    }

  return FALSE;
}

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

static void
update_can_focus (GtkCellRenderer *renderer,
		  gboolean        *can_focus)
{

  if (gtk_cell_renderer_can_focus (renderer))
    *can_focus = TRUE;
}

static void
gtk_cell_area_real_update_focus (GtkCellArea *area)
{
  gboolean can_focus = FALSE;

  /* Update the area's can focus flag, if any of the renderers can
   * focus then the area can focus.
   *
   * Subclasses can override this in the case that they are also
   * rendering widgets as well as renderers.
   */
  gtk_cell_area_forall (area, (GtkCellCallback)update_can_focus, &can_focus);
  gtk_cell_area_set_can_focus (area, can_focus);

  /* Unset the currently focused cell if the area can not receive
   * focus for the given row data */
  if (!can_focus)
    gtk_cell_area_set_focus_cell (area, NULL);
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

/**
 * gtk_cell_area_add:
 * @area: a #GtkCellArea
 * @renderer: the #GtkCellRenderer to add to @area
 *
 * Adds @renderer to @area with the default child cell properties.
 */
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

/**
 * gtk_cell_area_remove:
 * @area: a #GtkCellArea
 * @renderer: the #GtkCellRenderer to add to @area
 *
 * Removes @renderer from @area.
 */
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

/**
 * gtk_cell_area_forall
 * @area: a #GtkCellArea
 * @callback: the #GtkCellCallback to call
 * @callback_data: user provided data pointer
 *
 * Calls @callback for every #GtkCellRenderer in @area.
 */
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

/**
 * gtk_cell_area_get_cell_allocation:
 * @area: a #GtkCellArea
 * @iter: the #GtkCellAreaIter used to hold sizes for @area.
 * @widget: the #GtkWidget that @area is rendering on
 * @renderer: the #GtkCellRenderer to get the allocation for
 * @cell_area: the whole allocated area for @area in @widget
 *             for this row
 * @allocation: where to store the allocation for @renderer
 *
 * Derives the allocation of @renderer inside @area if @area
 * were to be renderered in @cell_area.
 */
void
gtk_cell_area_get_cell_allocation (GtkCellArea          *area,
				   GtkCellAreaIter      *iter,	
				   GtkWidget            *widget,
				   GtkCellRenderer      *renderer,
				   const GdkRectangle   *cell_area,
				   GdkRectangle         *allocation)
{
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (cell_area != NULL);
  g_return_if_fail (allocation != NULL);

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->get_cell_allocation)
    class->get_cell_allocation (area, iter, widget, renderer, cell_area, allocation);
  else
    g_warning ("GtkCellAreaClass::get_cell_allocation not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

gint
gtk_cell_area_event (GtkCellArea          *area,
		     GtkCellAreaIter      *iter,
		     GtkWidget            *widget,
		     GdkEvent             *event,
		     const GdkRectangle   *cell_area,
		     GtkCellRendererState  flags)
{
  GtkCellAreaClass *class;

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), 0);
  g_return_val_if_fail (GTK_IS_CELL_AREA_ITER (iter), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);
  g_return_val_if_fail (event != NULL, 0);
  g_return_val_if_fail (cell_area != NULL, 0);

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->event)
    return class->event (area, iter, widget, event, cell_area, flags);

  g_warning ("GtkCellAreaClass::event not implemented for `%s'", 
	     g_type_name (G_TYPE_FROM_INSTANCE (area)));
  return 0;
}

void
gtk_cell_area_render (GtkCellArea          *area,
		      GtkCellAreaIter      *iter,
		      GtkWidget            *widget,
		      cairo_t              *cr,
		      const GdkRectangle   *cell_area,
		      GtkCellRendererState  flags)
{
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_AREA_ITER (iter));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (cr != NULL);
  g_return_if_fail (cell_area != NULL);

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->render)
    class->render (area, iter, widget, cr, cell_area, flags);
  else
    g_warning ("GtkCellAreaClass::render not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

/*************************************************************
 *                      API: Geometry                        *
 *************************************************************/
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

/*************************************************************
 *                      API: Attributes                      *
 *************************************************************/

/**
 * gtk_cell_area_attribute_connect:
 * @area: a #GtkCellArea
 * @renderer: the #GtkCellRenderer to connect an attribute for
 * @attribute: the attribute name
 * @column: the #GtkTreeModel column to fetch attribute values from
 *
 * Connects an @attribute to apply values from @column for the
 * #GtkTreeModel in use.
 */
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

/**
 * gtk_cell_area_attribute_disconnect:
 * @area: a #GtkCellArea
 * @renderer: the #GtkCellRenderer to disconnect an attribute for
 * @attribute: the attribute name
 *
 * Disconnects @attribute for the @renderer in @area so that
 * attribute will no longer be updated with values from the
 * model.
 */
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
  gboolean       is_expander;
  gboolean       is_expanded;

  g_object_freeze_notify (G_OBJECT (renderer));

  /* Whether a row expands or is presently expanded can only be 
   * provided by the view (as these states can vary across views
   * accessing the same model).
   */
  g_object_get (renderer, "is-expander", &is_expander, NULL);
  if (is_expander != data->is_expander)
    g_object_set (renderer, "is-expander", data->is_expander, NULL);
  
  g_object_get (renderer, "is-expanded", &is_expanded, NULL);
  if (is_expanded != data->is_expanded)
    g_object_set (renderer, "is-expanded", data->is_expanded, NULL);

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

  g_object_thaw_notify (G_OBJECT (renderer));
}

/**
 * gtk_cell_area_apply_attributes
 * @area: a #GtkCellArea
 * @tree_model: a #GtkTreeModel to pull values from
 * @iter: the #GtkTreeIter in @tree_model to apply values for
 * @is_expander: whether @iter has children
 * @is_expanded: whether @iter is expanded in the view and
 *               children are visible
 *
 * Applies any connected attributes to the renderers in 
 * @area by pulling the values from @tree_model.
 */
void
gtk_cell_area_apply_attributes (GtkCellArea  *area,
				GtkTreeModel *tree_model,
				GtkTreeIter  *iter,
				gboolean      is_expander,
				gboolean      is_expanded)
{
  GtkCellAreaPrivate *priv;
  AttributeData       data;
  GtkTreePath        *path;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (iter != NULL);

  priv = area->priv;

  /* Feed in data needed to apply to every renderer */
  data.area        = area;
  data.model       = tree_model;
  data.iter        = iter;
  data.is_expander = is_expander;
  data.is_expanded = is_expanded;

  /* Go over any cells that have attributes or custom GtkCellLayoutDataFuncs and
   * apply the data from the treemodel */
  g_hash_table_foreach (priv->cell_info, (GHFunc)apply_cell_attributes, &data);

  /* Update the currently applied path */
  g_free (priv->current_path);
  path               = gtk_tree_model_get_path (tree_model, iter);
  priv->current_path = gtk_tree_path_to_string (path);
  gtk_tree_path_free (path);
}

/**
 * gtk_cell_area_get_current_path_string:
 * @area: a #GtkCellArea
 *
 * Gets the current #GtkTreePath string for the currently
 * applied #GtkTreeIter, this is implicitly updated when
 * gtk_cell_area_apply_attributes() is called and can be
 * used to interact with renderers from #GtkCellArea
 * subclasses.
 */
const gchar *
gtk_cell_area_get_current_path_string (GtkCellArea *area)
{
  GtkCellAreaPrivate *priv;

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), NULL);

  priv = area->priv;

  return priv->current_path;
}


/*************************************************************
 *                    API: Cell Properties                   *
 *************************************************************/
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

/*************************************************************
 *                         API: Focus                        *
 *************************************************************/

/**
 * gtk_cell_area_grab_focus:
 * @area: a #GtkCellArea
 * @direction: the #GtkDirectionType from which focus came
 *
 * This should be called by the @area's owning layout widget
 * when focus should be passed to @area for a given row data.
 *
 * Note that after applying new attributes for @area that
 * gtk_cell_area_update_focus() should be called and
 * gtk_cell_area_can_focus() should be checked before trying
 * to pass focus to @area.
 *
 * Implementing #GtkCellArea classes should implement this
 * method to receive focus in it's own way particular to
 * how it lays out cells.
 */
void
gtk_cell_area_grab_focus (GtkCellArea      *area,
			  GtkDirectionType  direction)
{
  GtkCellAreaClass *class;

  g_return_if_fail (GTK_IS_CELL_AREA (area));

  class = GTK_CELL_AREA_GET_CLASS (area);

  if (class->grab_focus)
    class->grab_focus (area, direction);
  else
    g_warning ("GtkCellAreaClass::grab_focus not implemented for `%s'", 
	       g_type_name (G_TYPE_FROM_INSTANCE (area)));
}

/**
 * gtk_cell_area_focus_leave:
 * @area: a #GtkCellArea
 * @direction: the #GtkDirectionType in which focus
 *             is to leave @area
 * @path: the current #GtkTreePath string for the 
 *        event which was handled by @area
 *
 * Notifies that focus is to leave @area in the
 * given @direction.
 *
 * This is called by #GtkCellArea implementations upon
 * handling a key event that caused focus to leave the
 * cell. The resulting signal can be handled by the
 * owning layouting widget to decide which new @area
 * to pass focus to and from what @direction. Or to
 * pass focus along to an entirely new data row.
 */
void
gtk_cell_area_focus_leave (GtkCellArea        *area,
			   GtkDirectionType    direction,
			   const gchar        *path)
{
  g_return_if_fail (GTK_IS_CELL_AREA (area));

  g_signal_emit (area, cell_area_signals[SIGNAL_FOCUS_LEAVE], 0, direction, path);
}

/**
 * gtk_cell_area_update_focus:
 * @area: a #GtkCellArea
 *
 * Updates focus information on @area for a given 
 * row of data.
 *
 * After calling gtk_cell_area_apply_attributes() to
 * the @area this method should be called to update
 * information about whether the @area can focus and
 * which is the cell currently in focus.
 */
void
gtk_cell_area_update_focus (GtkCellArea *area)
{
  g_return_if_fail (GTK_IS_CELL_AREA (area));

  GTK_CELL_AREA_GET_CLASS (area)->update_focus (area);
}

/**
 * gtk_cell_area_set_can_focus:
 * @area: a #GtkCellArea
 * @can_focus: whether @area can receive focus
 *
 * This is generally called from GtkCellArea::update_focus() 
 * implementations to update if the @area can focus after
 * applying new row data attributes.
 */
void
gtk_cell_area_set_can_focus (GtkCellArea *area,
			     gboolean     can_focus)
{
  GtkCellAreaPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA (area));

  priv = area->priv;

  if (priv->can_focus != can_focus)
    {
      priv->can_focus = can_focus;
    }
}

/**
 * gtk_cell_area_get_can_focus:
 * @area: a #GtkCellArea
 *
 * Returns whether the area can receive keyboard focus,
 * after applying new attributes to @area, 
 * gtk_cell_area_update_focus() needs to be called before
 * calling this method.
 *
 * Returns: whether @area can receive focus.
 */
gboolean
gtk_cell_area_get_can_focus (GtkCellArea *area)
{
  GtkCellAreaPrivate *priv;

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), FALSE);

  priv = area->priv;

  return priv->can_focus;
}


/**
 * gtk_cell_area_set_focus_cell:
 * @area: a #GtkCellArea
 * @focus_cell: the #GtkCellRenderer to give focus to
 *
 * This is generally called from #GtkCellArea implementations
 * either gtk_cell_area_grab_focus() or gtk_cell_area_update_focus()
 * is called. It's also up to the #GtkCellArea implementation
 * to update the focused cell when receiving events from
 * gtk_cell_area_event() appropriately.
 */
void
gtk_cell_area_set_focus_cell (GtkCellArea     *area,
			      GtkCellRenderer *renderer)
{
  GtkCellAreaPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (renderer == NULL || GTK_IS_CELL_RENDERER (renderer));

  priv = area->priv;

  if (priv->focus_cell != renderer)
    {
      if (priv->focus_cell)
	g_object_unref (priv->focus_cell);

      priv->focus_cell = renderer;

      if (priv->focus_cell)
	g_object_ref (priv->focus_cell);

      g_object_notify (G_OBJECT (area), "focus-cell");
    }
}

/**
 * gtk_cell_area_get_focus_cell:
 * @area: a #GtkCellArea
 *
 * Retrieves the currently focused cell for @area
 *
 * Returns: the currently focused cell in @area.
 */
GtkCellRenderer *
gtk_cell_area_get_focus_cell (GtkCellArea *area)
{
  GtkCellAreaPrivate *priv;

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), NULL);

  priv = area->priv;

  return priv->focus_cell;
}

void
gtk_cell_area_set_edited_cell (GtkCellArea     *area,
			       GtkCellRenderer *renderer)
{
  GtkCellAreaPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (renderer == NULL || GTK_IS_CELL_RENDERER (renderer));

  priv = area->priv;

  if (priv->edited_cell != renderer)
    {
      if (priv->edited_cell)
	g_object_unref (priv->edited_cell);

      priv->edited_cell = renderer;

      if (priv->edited_cell)
	g_object_ref (priv->edited_cell);

      g_object_notify (G_OBJECT (area), "edited-cell");
    }
}

GtkCellRenderer   *
gtk_cell_area_get_edited_cell (GtkCellArea *area)
{
  GtkCellAreaPrivate *priv;

  g_return_val_if_fail (GTK_IS_CELL_AREA (area), NULL);

  priv = area->priv;

  return priv->edited_cell;
}

/*************************************************************
 *                        API: Margins                       *
 *************************************************************/
gint
gtk_cell_area_get_cell_margin_left (GtkCellArea *area)
{
  g_return_val_if_fail (GTK_IS_CELL_AREA (area), 0);

  return area->priv->cell_border.left;
}

void
gtk_cell_area_set_cell_margin_left (GtkCellArea *area,
				    gint         margin)
{
  GtkCellAreaPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA (area));

  priv = area->priv;

  if (priv->cell_border.left != margin)
    {
      priv->cell_border.left = margin;

      g_object_notify (G_OBJECT (area), "margin-left");
    }
}

gint
gtk_cell_area_get_cell_margin_right (GtkCellArea *area)
{
  g_return_val_if_fail (GTK_IS_CELL_AREA (area), 0);

  return area->priv->cell_border.right;
}

void
gtk_cell_area_set_cell_margin_right (GtkCellArea *area,
				     gint         margin)
{
  GtkCellAreaPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA (area));

  priv = area->priv;

  if (priv->cell_border.right != margin)
    {
      priv->cell_border.right = margin;

      g_object_notify (G_OBJECT (area), "margin-right");
    }
}

gint
gtk_cell_area_get_cell_margin_top (GtkCellArea *area)
{
  g_return_val_if_fail (GTK_IS_CELL_AREA (area), 0);

  return area->priv->cell_border.top;
}

void
gtk_cell_area_set_cell_margin_top (GtkCellArea *area,
				   gint         margin)
{
  GtkCellAreaPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA (area));

  priv = area->priv;

  if (priv->cell_border.top != margin)
    {
      priv->cell_border.top = margin;

      g_object_notify (G_OBJECT (area), "margin-top");
    }
}

gint
gtk_cell_area_get_cell_margin_bottom (GtkCellArea *area)
{
  g_return_val_if_fail (GTK_IS_CELL_AREA (area), 0);

  return area->priv->cell_border.bottom;
}

void
gtk_cell_area_set_cell_margin_bottom (GtkCellArea *area,
				      gint         margin)
{
  GtkCellAreaPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA (area));

  priv = area->priv;

  if (priv->cell_border.bottom != margin)
    {
      priv->cell_border.bottom = margin;

      g_object_notify (G_OBJECT (area), "margin-bottom");
    }
}

/* For convenience in area implementations */
void
gtk_cell_area_editing_started (GtkCellArea        *area,
			       GtkCellRenderer    *renderer,
			       GtkCellEditable    *editable)
{
  GtkCellAreaPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA (area));

  priv = area->priv;

  g_signal_emit (area, cell_area_signals[SIGNAL_EDITING_STARTED], 0, 
		 renderer, editable, priv->current_path);
}

void
gtk_cell_area_inner_cell_area (GtkCellArea        *area,
			       GdkRectangle       *background_area,
			       GdkRectangle       *cell_area)
{
  GtkCellAreaPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (background_area != NULL);
  g_return_if_fail (cell_area != NULL);

  priv = area->priv;

  *cell_area = *background_area;

  cell_area->x      += priv->cell_border.left;
  cell_area->width  -= (priv->cell_border.left + priv->cell_border.right);
  cell_area->y      += priv->cell_border.top;
  cell_area->height -= (priv->cell_border.top + priv->cell_border.bottom);
}

void
gtk_cell_area_request_renderer (GtkCellArea        *area,
				GtkCellRenderer    *renderer,
				GtkOrientation      orientation,
				GtkWidget          *widget,
				gint                for_size,
				gint               *minimum_size,
				gint               *natural_size)
{
  GtkCellAreaPrivate *priv;

  g_return_if_fail (GTK_IS_CELL_AREA (area));
  g_return_if_fail (GTK_IS_CELL_RENDERER (renderer));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (minimum_size != NULL);
  g_return_if_fail (natural_size != NULL);

  priv = area->priv;

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (for_size < 0)
	  gtk_cell_renderer_get_preferred_width (renderer, widget, minimum_size, natural_size);
      else
	{
	  for_size = MAX (0, for_size - (priv->cell_border.top + priv->cell_border.bottom));

	  gtk_cell_renderer_get_preferred_width_for_height (renderer, widget, for_size, 
							    minimum_size, natural_size);
	}

      *minimum_size += (priv->cell_border.left + priv->cell_border.right);
      *natural_size += (priv->cell_border.left + priv->cell_border.right);
    }
  else /* GTK_ORIENTATION_VERTICAL */
    {
      if (for_size < 0)
	gtk_cell_renderer_get_preferred_height (renderer, widget, minimum_size, natural_size);
      else
	{
	  for_size = MAX (0, for_size - (priv->cell_border.left + priv->cell_border.right));

	  gtk_cell_renderer_get_preferred_height_for_width (renderer, widget, for_size, 
							    minimum_size, natural_size);
	}

      *minimum_size += (priv->cell_border.top + priv->cell_border.bottom);
      *natural_size += (priv->cell_border.top + priv->cell_border.bottom);
    }
}
