/* gtktreemenu.c
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
#include "gtkintl.h"
#include "gtktreemenu.h"
#include "gtkmenuitem.h"
#include "gtkseparatormenuitem.h"
#include "gtkcellareabox.h"
#include "gtkcellareacontext.h"
#include "gtkcelllayout.h"
#include "gtkcellview.h"
#include "gtkprivate.h"


/* GObjectClass */
static GObject  *gtk_tree_menu_constructor                    (GType                  type,
							       guint                  n_construct_properties,
							       GObjectConstructParam *construct_properties);
static void      gtk_tree_menu_dispose                        (GObject            *object);
static void      gtk_tree_menu_finalize                       (GObject            *object);
static void      gtk_tree_menu_set_property                   (GObject            *object,
							       guint               prop_id,
							       const GValue       *value,
							       GParamSpec         *pspec);
static void      gtk_tree_menu_get_property                   (GObject            *object,
							       guint               prop_id,
							       GValue             *value,
							       GParamSpec         *pspec);

/* GtkWidgetClass */
static void      gtk_tree_menu_get_preferred_width            (GtkWidget           *widget,
							       gint                *minimum_size,
							       gint                *natural_size);
static void      gtk_tree_menu_get_preferred_height           (GtkWidget           *widget,
							       gint                *minimum_size,
							       gint                *natural_size);
static void      gtk_tree_menu_size_allocate                  (GtkWidget           *widget,
							       GtkAllocation       *allocation);

/* GtkCellLayoutIface */
static void      gtk_tree_menu_cell_layout_init               (GtkCellLayoutIface  *iface);
static void      gtk_tree_menu_cell_layout_pack_start         (GtkCellLayout       *layout,
							       GtkCellRenderer     *cell,
							       gboolean             expand);
static void      gtk_tree_menu_cell_layout_pack_end           (GtkCellLayout        *layout,
							       GtkCellRenderer      *cell,
							       gboolean              expand);
static GList    *gtk_tree_menu_cell_layout_get_cells          (GtkCellLayout        *layout);
static void      gtk_tree_menu_cell_layout_clear              (GtkCellLayout        *layout);
static void      gtk_tree_menu_cell_layout_add_attribute      (GtkCellLayout        *layout,
							       GtkCellRenderer      *cell,
							       const gchar          *attribute,
							       gint                  column);
static void      gtk_tree_menu_cell_layout_set_cell_data_func (GtkCellLayout        *layout,
							       GtkCellRenderer      *cell,
							       GtkCellLayoutDataFunc func,
							       gpointer              func_data,
							       GDestroyNotify        destroy);
static void      gtk_tree_menu_cell_layout_clear_attributes   (GtkCellLayout        *layout,
							       GtkCellRenderer      *cell);
static void      gtk_tree_menu_cell_layout_reorder            (GtkCellLayout        *layout,
							       GtkCellRenderer      *cell,
							       gint                  position);
static GtkCellArea *gtk_tree_menu_cell_layout_get_area        (GtkCellLayout        *layout);


/* TreeModel/DrawingArea callbacks and building menus/submenus */
static void      gtk_tree_menu_populate                       (GtkTreeMenu          *menu);
static GtkWidget *gtk_tree_menu_create_item                   (GtkTreeMenu          *menu,
							       GtkTreeIter          *iter);
static void      gtk_tree_menu_set_area                       (GtkTreeMenu          *menu,
							       GtkCellArea          *area);
static void      queue_resize_all                             (GtkWidget            *menu);
static void      context_size_changed_cb                      (GtkCellAreaContext   *context,
							       GParamSpec           *pspec,
							       GtkWidget            *menu);

struct _GtkTreeMenuPrivate
{
  /* TreeModel and parent for this menu */
  GtkTreeModel        *model;
  GtkTreeRowReference *root;

  /* CellArea and context for this menu */
  GtkCellArea         *area;
  GtkCellAreaContext  *context;

  gint                 last_alloc_width;
  gint                 last_alloc_height;
  
  /* Signals */
  gulong               size_changed_id;

  /* Row separators */
  GtkTreeViewRowSeparatorFunc row_separator_func;
  gpointer                    row_separator_data;
  GDestroyNotify              row_separator_destroy;
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_ROOT,
  PROP_CELL_AREA
};

G_DEFINE_TYPE_WITH_CODE (GtkTreeMenu, gtk_tree_menu, GTK_TYPE_MENU,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
						gtk_tree_menu_cell_layout_init));

static void
gtk_tree_menu_init (GtkTreeMenu *menu)
{
  GtkTreeMenuPrivate *priv;

  menu->priv = G_TYPE_INSTANCE_GET_PRIVATE (menu,
					    GTK_TYPE_TREE_MENU,
					    GtkTreeMenuPrivate);
  priv = menu->priv;

  gtk_menu_set_reserve_toggle_size (GTK_MENU (menu), FALSE);
}

static void 
gtk_tree_menu_class_init (GtkTreeMenuClass *class)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->constructor  = gtk_tree_menu_constructor;
  object_class->dispose      = gtk_tree_menu_dispose;
  object_class->finalize     = gtk_tree_menu_finalize;
  object_class->set_property = gtk_tree_menu_set_property;
  object_class->get_property = gtk_tree_menu_get_property;

  widget_class->get_preferred_width            = gtk_tree_menu_get_preferred_width;
  widget_class->get_preferred_height           = gtk_tree_menu_get_preferred_height;
  widget_class->size_allocate                  = gtk_tree_menu_size_allocate;

  g_object_class_install_property (object_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
                                                        P_("TreeMenu model"),
                                                        P_("The model for the tree menu"),
                                                        GTK_TYPE_TREE_MODEL,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_ROOT,
                                   g_param_spec_boxed ("root",
						       P_("TreeMenu root row"),
						       P_("The TreeMenu will display children of the "
							  "specified root"),
						       GTK_TYPE_TREE_PATH,
						       GTK_PARAM_READWRITE));

   g_object_class_install_property (object_class,
                                    PROP_CELL_AREA,
                                    g_param_spec_object ("cell-area",
							 P_("Cell Area"),
							 P_("The GtkCellArea used to layout cells"),
							 GTK_TYPE_CELL_AREA,
							 GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));


   g_type_class_add_private (object_class, sizeof (GtkTreeMenuPrivate));
}

/****************************************************************
 *                         GObjectClass                         *
 ****************************************************************/
static GObject  *
gtk_tree_menu_constructor (GType                  type,
			   guint                  n_construct_properties,
			   GObjectConstructParam *construct_properties)
{
  GObject            *object;
  GtkTreeMenu        *menu;
  GtkTreeMenuPrivate *priv;

  object = G_OBJECT_CLASS (gtk_tree_menu_parent_class)->constructor
    (type, n_construct_properties, construct_properties);

  menu = GTK_TREE_MENU (object);
  priv = menu->priv;

  if (!priv->area)
    {
      GtkCellArea *area = gtk_cell_area_box_new ();

      gtk_tree_menu_set_area (menu, area);
    }

  priv->context = gtk_cell_area_create_context (priv->area);
  priv->size_changed_id = 
    g_signal_connect (priv->context, "notify",
		      G_CALLBACK (context_size_changed_cb), menu);


  return object;
}

static void
gtk_tree_menu_dispose (GObject *object)
{
  GtkTreeMenu        *menu;
  GtkTreeMenuPrivate *priv;

  menu = GTK_TREE_MENU (object);
  priv = menu->priv;

  gtk_tree_menu_set_model (menu, NULL);
  gtk_tree_menu_set_area (menu, NULL);

  if (priv->context)
    {
      /* Disconnect signals */
      g_signal_handler_disconnect (priv->context, priv->size_changed_id);

      g_object_unref (priv->context);
      priv->context = NULL;
      priv->size_changed_id = 0;
    }

  G_OBJECT_CLASS (gtk_tree_menu_parent_class)->dispose (object);
}

static void
gtk_tree_menu_finalize (GObject *object)
{

  G_OBJECT_CLASS (gtk_tree_menu_parent_class)->finalize (object);
}

static void
gtk_tree_menu_set_property (GObject            *object,
			    guint               prop_id,
			    const GValue       *value,
			    GParamSpec         *pspec)
{
  GtkTreeMenu *menu = GTK_TREE_MENU (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_tree_menu_set_model (menu, g_value_get_object (value));
      break;

    case PROP_ROOT:
      gtk_tree_menu_set_root (menu, g_value_get_boxed (value));
      break;

    case PROP_CELL_AREA:
      /* Construct-only, can only be assigned once */
      gtk_tree_menu_set_area (menu, (GtkCellArea *)g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tree_menu_get_property (GObject            *object,
			    guint               prop_id,
			    GValue             *value,
			    GParamSpec         *pspec)
{
  GtkTreeMenu        *menu = GTK_TREE_MENU (object);
  GtkTreeMenuPrivate *priv = menu->priv;

  switch (prop_id)
    {
      case PROP_MODEL:
        g_value_set_object (value, priv->model);
        break;

      case PROP_ROOT:
        g_value_set_boxed (value, priv->root);
        break;

      case PROP_CELL_AREA:
	g_value_set_object (value, priv->area);
	break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/****************************************************************
 *                         GtkWidgetClass                       *
 ****************************************************************/

/* We tell all the menu items to reserve space for the submenu
 * indicator if there is at least one submenu, this way we ensure
 * that every internal cell area gets allocated the
 * same width (and requested height for the same appropriate width).
 */
static void
sync_reserve_submenu_size (GtkTreeMenu *menu)
{
  GList              *children, *l;
  gboolean            has_submenu = FALSE;

  children = gtk_container_get_children (GTK_CONTAINER (menu));
  for (l = children; l; l = l->next)
    {
      GtkMenuItem *item = l->data;

      if (gtk_menu_item_get_submenu (item) != NULL)
	{
	  has_submenu = TRUE;
	  break;
	}
    }

  for (l = children; l; l = l->next)
    {
      GtkMenuItem *item = l->data;

      gtk_menu_item_set_reserve_indicator (item, has_submenu);
    }

  g_list_free (children);
}

static void
gtk_tree_menu_get_preferred_width (GtkWidget           *widget,
				   gint                *minimum_size,
				   gint                *natural_size)
{
  GtkTreeMenu        *menu = GTK_TREE_MENU (widget);
  GtkTreeMenuPrivate *priv = menu->priv;
  GtkTreePath        *path = NULL;
  GtkTreeIter         iter;
  gboolean            valid = FALSE;

  g_signal_handler_block (priv->context, priv->size_changed_id);

  /* Before chaining up to the parent class and requesting the 
   * menu item/cell view sizes, we need to request the size of
   * each row for this menu and make sure all the cellviews 
   * request enough space 
   */
  gtk_cell_area_context_flush_preferred_width (priv->context);

  sync_reserve_submenu_size (menu);

  if (priv->model)
    {
      if (priv->root)
	path = gtk_tree_row_reference_get_path (priv->root);
      
      if (path)
	{
	  GtkTreeIter parent;

	  if (gtk_tree_model_get_iter (priv->model, &parent, path))
	    valid = gtk_tree_model_iter_children (priv->model, &iter, &parent);
	  
	  gtk_tree_path_free (path);
	}
      else
	valid = gtk_tree_model_iter_children (priv->model, &iter, NULL);
      
      while (valid)
	{
	  gboolean is_separator = FALSE;
	  
	  if (priv->row_separator_func)
	    is_separator = 
	      priv->row_separator_func (priv->model, &iter, priv->row_separator_data);

	  if (!is_separator)
	    {
	      gtk_cell_area_apply_attributes (priv->area, priv->model, &iter, FALSE, FALSE);
	      gtk_cell_area_get_preferred_width (priv->area, priv->context, widget, NULL, NULL);
	    }
	  
	  valid = gtk_tree_model_iter_next (priv->model, &iter);
	}
    }

  gtk_cell_area_context_sum_preferred_width (priv->context);

  g_signal_handler_unblock (priv->context, priv->size_changed_id);

  /* Now that we've requested all the row's and updated priv->context properly, we can go ahead
   * and calculate the sizes by requesting the menu items and thier cell views */
  GTK_WIDGET_CLASS (gtk_tree_menu_parent_class)->get_preferred_width (widget, minimum_size, natural_size);
}

static void
gtk_tree_menu_get_preferred_height (GtkWidget           *widget,
				    gint                *minimum_size,
				    gint                *natural_size)
{
  GtkTreeMenu        *menu = GTK_TREE_MENU (widget);
  GtkTreeMenuPrivate *priv = menu->priv;
  GtkTreePath        *path = NULL;
  GtkTreeIter         iter;
  gboolean            valid = FALSE;

  g_signal_handler_block (priv->context, priv->size_changed_id);

  /* Before chaining up to the parent class and requesting the 
   * menu item/cell view sizes, we need to request the size of
   * each row for this menu and make sure all the cellviews 
   * request enough space 
   */
  gtk_cell_area_context_flush_preferred_height (priv->context);

  sync_reserve_submenu_size (menu);

  if (priv->model)
    {
      if (priv->root)
	path = gtk_tree_row_reference_get_path (priv->root);
      
      if (path)
	{
	  GtkTreeIter parent;

	  if (gtk_tree_model_get_iter (priv->model, &parent, path))
	    valid = gtk_tree_model_iter_children (priv->model, &iter, &parent);
	  
	  gtk_tree_path_free (path);
	}
      else
	valid = gtk_tree_model_iter_children (priv->model, &iter, NULL);
      
      while (valid)
	{
	  gboolean is_separator = FALSE;
	  
	  if (priv->row_separator_func)
	    is_separator = 
	      priv->row_separator_func (priv->model, &iter, priv->row_separator_data);

	  if (!is_separator)
	    {
	      gtk_cell_area_apply_attributes (priv->area, priv->model, &iter, FALSE, FALSE);
	      gtk_cell_area_get_preferred_height (priv->area, priv->context, widget, NULL, NULL);
	    }
	  
	  valid = gtk_tree_model_iter_next (priv->model, &iter);
	}
    }

  gtk_cell_area_context_sum_preferred_height (priv->context);

  g_signal_handler_unblock (priv->context, priv->size_changed_id);

  /* Now that we've requested all the row's and updated priv->context properly, we can go ahead
   * and calculate the sizes by requesting the menu items and thier cell views */
  GTK_WIDGET_CLASS (gtk_tree_menu_parent_class)->get_preferred_height (widget, minimum_size, natural_size);
}

static void
gtk_tree_menu_size_allocate (GtkWidget           *widget,
			     GtkAllocation       *allocation)
{
  GtkTreeMenu        *menu = GTK_TREE_MENU (widget);
  GtkTreeMenuPrivate *priv = menu->priv;
  gint                new_width, new_height;

  /* flush the context allocation */
  gtk_cell_area_context_flush_allocation (priv->context);

  /* Leave it to the first cell area to allocate the size of priv->context, since
   * we configure the menu to allocate all children the same width this should work fine */
  GTK_WIDGET_CLASS (gtk_tree_menu_parent_class)->size_allocate (widget, allocation);

  /* In alot of cases the menu gets allocated while the children dont need
   * any reallocation, in this case we need to restore the context allocation */
  gtk_cell_area_context_get_allocation (priv->context, &new_width, &new_height);

  if (new_width <= 0 && new_height <= 0)
    {
      gtk_cell_area_context_allocate_width (priv->context, priv->last_alloc_width);
      gtk_cell_area_context_allocate_height (priv->context, priv->last_alloc_height);
    }

  /* Save the allocation for the next round */
  gtk_cell_area_context_get_allocation (priv->context, 
					&priv->last_alloc_width, 
					&priv->last_alloc_height);
}

/****************************************************************
 *                      GtkCellLayoutIface                      *
 ****************************************************************/
/* Just forward all the GtkCellLayoutIface methods to the 
 * underlying GtkCellArea
 */
static void
gtk_tree_menu_cell_layout_init (GtkCellLayoutIface  *iface)
{
  iface->pack_start         = gtk_tree_menu_cell_layout_pack_start;
  iface->pack_end           = gtk_tree_menu_cell_layout_pack_end;
  iface->get_cells          = gtk_tree_menu_cell_layout_get_cells;
  iface->clear              = gtk_tree_menu_cell_layout_clear;
  iface->add_attribute      = gtk_tree_menu_cell_layout_add_attribute;
  iface->set_cell_data_func = gtk_tree_menu_cell_layout_set_cell_data_func;
  iface->clear_attributes   = gtk_tree_menu_cell_layout_clear_attributes;
  iface->reorder            = gtk_tree_menu_cell_layout_reorder;
  iface->get_area           = gtk_tree_menu_cell_layout_get_area;
}

static void
gtk_tree_menu_cell_layout_pack_start (GtkCellLayout       *layout,
				      GtkCellRenderer     *cell,
				      gboolean             expand)
{
  GtkTreeMenu        *menu = GTK_TREE_MENU (layout);
  GtkTreeMenuPrivate *priv = menu->priv;

  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->area), cell, expand);
}

static void
gtk_tree_menu_cell_layout_pack_end (GtkCellLayout        *layout,
				    GtkCellRenderer      *cell,
				    gboolean              expand)
{
  GtkTreeMenu        *menu = GTK_TREE_MENU (layout);
  GtkTreeMenuPrivate *priv = menu->priv;

  gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (priv->area), cell, expand);
}

static GList *
gtk_tree_menu_cell_layout_get_cells (GtkCellLayout *layout)
{
  GtkTreeMenu        *menu = GTK_TREE_MENU (layout);
  GtkTreeMenuPrivate *priv = menu->priv;

  return gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (priv->area));
}

static void
gtk_tree_menu_cell_layout_clear (GtkCellLayout *layout)
{
  GtkTreeMenu        *menu = GTK_TREE_MENU (layout);
  GtkTreeMenuPrivate *priv = menu->priv;

  gtk_cell_layout_clear (GTK_CELL_LAYOUT (priv->area));
}

static void
gtk_tree_menu_cell_layout_add_attribute (GtkCellLayout        *layout,
					 GtkCellRenderer      *cell,
					 const gchar          *attribute,
					 gint                  column)
{
  GtkTreeMenu        *menu = GTK_TREE_MENU (layout);
  GtkTreeMenuPrivate *priv = menu->priv;

  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (priv->area), cell, attribute, column);
}

static void
gtk_tree_menu_cell_layout_set_cell_data_func (GtkCellLayout        *layout,
					      GtkCellRenderer      *cell,
					      GtkCellLayoutDataFunc func,
					      gpointer              func_data,
					      GDestroyNotify        destroy)
{
  GtkTreeMenu        *menu = GTK_TREE_MENU (layout);
  GtkTreeMenuPrivate *priv = menu->priv;

  gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (priv->area), cell, func, func_data, destroy);
}

static void
gtk_tree_menu_cell_layout_clear_attributes (GtkCellLayout        *layout,
					    GtkCellRenderer      *cell)
{
  GtkTreeMenu        *menu = GTK_TREE_MENU (layout);
  GtkTreeMenuPrivate *priv = menu->priv;

  gtk_cell_layout_clear_attributes (GTK_CELL_LAYOUT (priv->area), cell);
}

static void
gtk_tree_menu_cell_layout_reorder (GtkCellLayout        *layout,
				   GtkCellRenderer      *cell,
				   gint                  position)
{
  GtkTreeMenu        *menu = GTK_TREE_MENU (layout);
  GtkTreeMenuPrivate *priv = menu->priv;

  gtk_cell_layout_reorder (GTK_CELL_LAYOUT (priv->area), cell, position);
}

static GtkCellArea *
gtk_tree_menu_cell_layout_get_area (GtkCellLayout *layout)
{
  GtkTreeMenu        *menu = GTK_TREE_MENU (layout);
  GtkTreeMenuPrivate *priv = menu->priv;

  return priv->area;
}


/****************************************************************
 *             TreeModel callbacks/populating menus             *
 ****************************************************************/
static void
context_size_changed_cb (GtkCellAreaContext  *context,
			 GParamSpec          *pspec,
			 GtkWidget           *menu)
{
  if (!strcmp (pspec->name, "minimum-width") ||
      !strcmp (pspec->name, "natural-width") ||
      !strcmp (pspec->name, "minimum-height") ||
      !strcmp (pspec->name, "natural-height"))
    queue_resize_all (menu);
}

static void
queue_resize_all (GtkWidget *menu)
{
  GList *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (menu));
  for (l = children; l; l = l->next)
    {
      GtkWidget *widget = l->data;

      gtk_widget_queue_resize (widget);
    }

  g_list_free (children);

  gtk_widget_queue_resize (menu);
}


static void
gtk_tree_menu_set_area (GtkTreeMenu *menu,
			GtkCellArea *area)
{
  GtkTreeMenuPrivate *priv = menu->priv;

  if (priv->area)
    g_object_unref (area);

  priv->area = area;

  if (priv->area)
    g_object_ref_sink (area);
}


static GtkWidget *
gtk_tree_menu_create_item (GtkTreeMenu *menu,
			   GtkTreeIter *iter)
{
  GtkTreeMenuPrivate *priv = menu->priv;
  GtkWidget          *item, *view;
  GtkTreePath        *path;

  view = gtk_cell_view_new_with_context (priv->area, priv->context);
  item = gtk_menu_item_new ();
  gtk_widget_show (view);
  gtk_widget_show (item);

  path = gtk_tree_model_get_path (priv->model, iter);

  gtk_cell_view_set_model (GTK_CELL_VIEW (view), priv->model);
  gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (view), path);

  gtk_tree_path_free (path);

  gtk_widget_show (view);
  gtk_container_add (GTK_CONTAINER (item), view);

  return item;
}

static void
gtk_tree_menu_populate (GtkTreeMenu *menu)
{
  GtkTreeMenuPrivate *priv = menu->priv;
  GtkTreePath        *path = NULL;
  GtkTreeIter         parent;
  GtkTreeIter         iter;
  gboolean            valid = FALSE;
  GtkWidget          *menu_item;

  if (!priv->model)
    return;

  if (priv->root)
    path = gtk_tree_row_reference_get_path (priv->root);

  if (path)
    {
      if (gtk_tree_model_get_iter (priv->model, &parent, path))
	valid = gtk_tree_model_iter_children (priv->model, &iter, &parent);

      gtk_tree_path_free (path);
    }
  else
    valid = gtk_tree_model_iter_children (priv->model, &iter, NULL);

  /* Create a menu item for every row at the current depth, add a GtkTreeMenu
   * submenu for iters/items that have children */
  while (valid)
    {
      gboolean is_separator = FALSE;

      if (priv->row_separator_func)
	is_separator = 
	  priv->row_separator_func (priv->model, &iter, 
				    priv->row_separator_data);

      if (is_separator)
	menu_item = gtk_separator_menu_item_new ();
      else
	{
	  menu_item = gtk_tree_menu_create_item (menu, &iter);

	  /* Add a GtkTreeMenu submenu to render the children of this row */
	  if (gtk_tree_model_iter_has_child (priv->model, &iter))
	    {
	      GtkTreePath         *row_path;
	      GtkWidget           *submenu;
	      
	      row_path = gtk_tree_model_get_path (priv->model, &iter);
	      submenu  = gtk_tree_menu_new_full (priv->area, priv->model, row_path);

	      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), submenu);

	      gtk_tree_path_free (row_path);
	    }
	}

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

      valid = gtk_tree_model_iter_next (priv->model, &iter);
    }
}

/****************************************************************
 *                            API                               *
 ****************************************************************/
GtkWidget *
gtk_tree_menu_new (void)
{
  return (GtkWidget *)g_object_new (GTK_TYPE_TREE_MENU, NULL);
}

GtkWidget *
gtk_tree_menu_new_with_area (GtkCellArea    *area)
{
  return (GtkWidget *)g_object_new (GTK_TYPE_TREE_MENU, 
				    "cell-area", area, 
				    NULL);
}

GtkWidget *
gtk_tree_menu_new_full (GtkCellArea         *area,
			GtkTreeModel        *model,
			GtkTreePath         *root)
{
  return (GtkWidget *)g_object_new (GTK_TYPE_TREE_MENU, 
				    "cell-area", area, 
				    "model", model,
				    "root", root,
				    NULL);
}

void
gtk_tree_menu_set_model (GtkTreeMenu  *menu,
			 GtkTreeModel *model)
{
  GtkTreeMenuPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_MENU (menu));
  g_return_if_fail (model == NULL || GTK_IS_TREE_MODEL (model));

  priv = menu->priv;

  if (priv->model != model)
    {
      if (priv->model)
	{
	  /* Disconnect signals */

	  g_object_unref (priv->model);
	}

      priv->model = model;

      if (priv->model)
	{
	  /* Connect signals */

	  g_object_ref (priv->model);
	}
    }
}

GtkTreeModel *
gtk_tree_menu_get_model (GtkTreeMenu *menu)
{
  GtkTreeMenuPrivate *priv;

  g_return_val_if_fail (GTK_IS_TREE_MENU (menu), NULL);

  priv = menu->priv;

  return priv->model;
}

void
gtk_tree_menu_set_root (GtkTreeMenu         *menu,
			GtkTreePath         *path)
{
  GtkTreeMenuPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_MENU (menu));
  g_return_if_fail (menu->priv->model != NULL || path == NULL);

  priv = menu->priv;

  if (priv->root) 
    gtk_tree_row_reference_free (priv->root);

  if (path)
    priv->root = gtk_tree_row_reference_new (priv->model, path);
  else
    priv->root = NULL;

  /* Destroy all the menu items for the previous root */
  gtk_container_foreach (GTK_CONTAINER (menu), 
			 (GtkCallback) gtk_widget_destroy, NULL);
  
  /* Populate for the new root */
  if (priv->model)
    gtk_tree_menu_populate (menu);

  gtk_widget_queue_resize (GTK_WIDGET (menu));
}

GtkTreePath *
gtk_tree_menu_get_root (GtkTreeMenu *menu)
{
  GtkTreeMenuPrivate *priv;

  g_return_val_if_fail (GTK_IS_TREE_MENU (menu), NULL);

  priv = menu->priv;

  if (priv->root)
    return gtk_tree_row_reference_get_path (priv->root);

  return NULL;
}

void
gtk_tree_menu_set_row_separator_func (GtkTreeMenu          *menu,
				      GtkTreeViewRowSeparatorFunc func,
				      gpointer              data,
				      GDestroyNotify        destroy)
{
  GtkTreeMenuPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_MENU (menu));

  priv = menu->priv;

  if (priv->row_separator_destroy)
    priv->row_separator_destroy (priv->row_separator_data);

  priv->row_separator_func    = func;
  priv->row_separator_data    = data;
  priv->row_separator_destroy = destroy;
}

GtkTreeViewRowSeparatorFunc
gtk_tree_menu_get_row_separator_func (GtkTreeMenu *menu)
{
  GtkTreeMenuPrivate *priv;

  g_return_val_if_fail (GTK_IS_TREE_MENU (menu), NULL);

  priv = menu->priv;

  return priv->row_separator_func;
}
