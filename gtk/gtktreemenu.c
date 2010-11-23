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
#include "gtkmarshalers.h"
#include "gtkmenuitem.h"
#include "gtktearoffmenuitem.h"
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

/* GtkCellLayoutIface */
static void      gtk_tree_menu_cell_layout_init               (GtkCellLayoutIface  *iface);
static GtkCellArea *gtk_tree_menu_cell_layout_get_area        (GtkCellLayout        *layout);


/* TreeModel/DrawingArea callbacks and building menus/submenus */
static void       gtk_tree_menu_populate                      (GtkTreeMenu          *menu);
static GtkWidget *gtk_tree_menu_create_item                   (GtkTreeMenu          *menu,
							       GtkTreeIter          *iter,
							       gboolean              header_item);
static void       gtk_tree_menu_set_area                      (GtkTreeMenu          *menu,
							       GtkCellArea          *area);
static GtkWidget *gtk_tree_menu_get_path_item                 (GtkTreeMenu          *menu,
							       GtkTreePath          *path);

static void       context_size_changed_cb                     (GtkCellAreaContext   *context,
							       GParamSpec           *pspec,
							       GtkWidget            *menu);
static void       item_activated_cb                           (GtkMenuItem          *item,
							       GtkTreeMenu          *menu);
static void       submenu_activated_cb                        (GtkTreeMenu          *submenu,
							       const gchar          *path,
							       GtkTreeMenu          *menu);

struct _GtkTreeMenuPrivate
{
  /* TreeModel and parent for this menu */
  GtkTreeModel        *model;
  GtkTreeRowReference *root;

  /* CellArea and context for this menu */
  GtkCellArea         *area;
  GtkCellAreaContext  *context;
  
  /* Signals */
  gulong               size_changed_id;
  gulong               row_inserted_id;
  gulong               row_deleted_id;
  gulong               row_reordered_id;

  /* Row separators */
  GtkTreeViewRowSeparatorFunc row_separator_func;
  gpointer                    row_separator_data;
  GDestroyNotify              row_separator_destroy;

  /* Submenu headers */
  GtkTreeMenuHeaderFunc header_func;
  gpointer              header_data;
  GDestroyNotify        header_destroy;

  guint32               menu_with_header : 1;
  guint32               tearoff     : 1;
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_ROOT,
  PROP_CELL_AREA,
  PROP_TEAROFF
};

enum {
  SIGNAL_MENU_ACTIVATE,
  N_SIGNALS
};

static guint   tree_menu_signals[N_SIGNALS] = { 0 };
static GQuark  tree_menu_path_quark = 0;

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

  tree_menu_path_quark = g_quark_from_static_string ("gtk-tree-menu-path");

  object_class->constructor  = gtk_tree_menu_constructor;
  object_class->dispose      = gtk_tree_menu_dispose;
  object_class->finalize     = gtk_tree_menu_finalize;
  object_class->set_property = gtk_tree_menu_set_property;
  object_class->get_property = gtk_tree_menu_get_property;

  widget_class->get_preferred_width            = gtk_tree_menu_get_preferred_width;
  widget_class->get_preferred_height           = gtk_tree_menu_get_preferred_height;

  tree_menu_signals[SIGNAL_MENU_ACTIVATE] =
    g_signal_new (I_("menu-activate"),
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_FIRST,
		  0, /* No class closure here */
		  NULL, NULL,
		  _gtk_marshal_VOID__STRING,
		  G_TYPE_NONE, 1, G_TYPE_STRING);

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

   g_object_class_install_property (object_class,
                                    PROP_TEAROFF,
                                    g_param_spec_boolean ("tearoff",
							  P_("Tearoff"),
							  P_("Whether the menu has a tearoff item"),
							  FALSE,
							  GTK_PARAM_READWRITE));


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
  GtkTreeMenu        *menu;
  GtkTreeMenuPrivate *priv;

  menu = GTK_TREE_MENU (object);
  priv = menu->priv;

  gtk_tree_menu_set_row_separator_func (menu, NULL, NULL, NULL);
  gtk_tree_menu_set_header_func (menu, NULL, NULL, NULL);

  if (priv->root) 
    gtk_tree_row_reference_free (priv->root);

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

    case PROP_TEAROFF:
      gtk_tree_menu_set_tearoff (menu, g_value_get_boolean (value));
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

    case PROP_TEAROFF:
      g_value_set_boolean (value, priv->tearoff);
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

  /* We leave the requesting work up to the cellviews which operate in the same
   * context, reserving space for the submenu indicator if any of the items have
   * submenus ensures that every cellview will receive the same allocated width.
   *
   * Since GtkMenu does hieght-for-width correctly, we know that the width of
   * every cell will be requested before the height-for-widths are requested.
   */
  g_signal_handler_block (priv->context, priv->size_changed_id);

  sync_reserve_submenu_size (menu);

  GTK_WIDGET_CLASS (gtk_tree_menu_parent_class)->get_preferred_width (widget, minimum_size, natural_size);

  g_signal_handler_unblock (priv->context, priv->size_changed_id);
}

static void
gtk_tree_menu_get_preferred_height (GtkWidget           *widget,
				    gint                *minimum_size,
				    gint                *natural_size)
{
  GtkTreeMenu        *menu = GTK_TREE_MENU (widget);
  GtkTreeMenuPrivate *priv = menu->priv;

  g_signal_handler_block (priv->context, priv->size_changed_id);

  sync_reserve_submenu_size (menu);

  GTK_WIDGET_CLASS (gtk_tree_menu_parent_class)->get_preferred_height (widget, minimum_size, natural_size);

  g_signal_handler_unblock (priv->context, priv->size_changed_id);
}

/****************************************************************
 *                      GtkCellLayoutIface                      *
 ****************************************************************/
static void
gtk_tree_menu_cell_layout_init (GtkCellLayoutIface  *iface)
{
  iface->get_area = gtk_tree_menu_cell_layout_get_area;
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
static GtkWidget *
gtk_tree_menu_get_path_item (GtkTreeMenu          *menu,
			     GtkTreePath          *search)
{
  GtkWidget *item = NULL;
  GList     *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (menu));

  for (l = children; item == NULL && l != NULL; l = l->next)
    {
      GtkWidget   *child = l->data;
      GtkTreePath *path  = NULL;

      if (GTK_IS_SEPARATOR_MENU_ITEM (child))
	{
	  GtkTreeRowReference *row =
	    g_object_get_qdata (G_OBJECT (child), tree_menu_path_quark);

	  if (row && gtk_tree_row_reference_valid (row))
	    path = gtk_tree_row_reference_get_path (row);
	}
      else
	{
	  GtkWidget *view = gtk_bin_get_child (GTK_BIN (child));

	  /* It's always a cellview */
	  if (GTK_IS_CELL_VIEW (view))
	    path = gtk_cell_view_get_displayed_row (GTK_CELL_VIEW (view));
	}

      if (path)
	{
	  if (gtk_tree_path_compare (search, path) == 0)
	    item = child;

	  gtk_tree_path_free (path);
	}
    }

  g_list_free (children);

  return item;
}

static void
row_inserted_cb (GtkTreeModel     *model,
		 GtkTreePath      *path,
		 GtkTreeIter      *iter,
		 GtkTreeMenu      *menu)
{
  GtkTreeMenuPrivate *priv = menu->priv;
  GtkTreePath        *parent_path;
  gboolean            this_menu = FALSE;
  gint               *indices, index, depth;

  parent_path = gtk_tree_path_copy (path);

  /* Check if the menu and the added iter are in root of the model */
  if (!gtk_tree_path_up (parent_path))
    {
      if (!priv->root)
	this_menu = TRUE;
    }
  /* If we are a submenu, compare the path */
  else if (priv->root)
    {
      GtkTreePath *root_path =
	gtk_tree_row_reference_get_path (priv->root);

      if (gtk_tree_path_compare (root_path, parent_path) == 0)
	this_menu = TRUE;

      gtk_tree_path_free (root_path);
    }

  gtk_tree_path_free (parent_path);

  /* If the iter should be in this menu then go ahead and insert it */
  if (this_menu)
    {
      GtkWidget *item;

      /* Get the index of the path for this depth */
      indices = gtk_tree_path_get_indices (path);
      depth   = gtk_tree_path_get_depth (path);
      index   = indices[depth -1];

      /* Menus with a header include a menuitem for it's root node
       * and a separator menu item */
      if (priv->menu_with_header)
	index += 2;

      /* Index after the tearoff item for the root menu if
       * there is a tearoff item
       */
      if (priv->root == NULL && priv->tearoff)
	index += 1;

      item = gtk_tree_menu_create_item (menu, iter, FALSE);
      gtk_menu_shell_insert (GTK_MENU_SHELL (menu), item, index);

      /* Resize everything */
      gtk_cell_area_context_flush (menu->priv->context);
    }
}

static void
row_deleted_cb (GtkTreeModel     *model,
		GtkTreePath      *path,
		GtkTreeMenu      *menu)
{
  GtkTreeMenuPrivate *priv = menu->priv;
  GtkTreePath        *root_path;
  GtkWidget          *item;

  /* If it's the root node we leave it to the parent menu to remove us
   * from its menu */
  if (priv->root)
    {
      root_path = gtk_tree_row_reference_get_path (priv->root);

      if (gtk_tree_path_compare (root_path, path) == 0)
	{
	  gtk_tree_path_free (root_path);
	  return;
	}
    }

  /* Get rid of the deleted item */
  item = gtk_tree_menu_get_path_item (menu, path);
  if (item)
    {
      gtk_widget_destroy (item);

      /* Resize everything */
      gtk_cell_area_context_flush (menu->priv->context);
    }
}

static void
row_reordered_cb (GtkTreeModel    *model,
		  GtkTreePath     *path,
		  GtkTreeIter     *iter,
		  gint            *new_order,
		  GtkTreeMenu     *menu)
{
  GtkTreeMenuPrivate *priv = menu->priv;
  gboolean            this_menu = FALSE;

  if (iter == NULL && priv->root == NULL)
    this_menu = TRUE;
  else if (priv->root)
    {
      GtkTreePath *root_path =
	gtk_tree_row_reference_get_path (priv->root);

      if (gtk_tree_path_compare (root_path, path) == 0)
	this_menu = TRUE;

      gtk_tree_path_free (root_path);
    }

  if (this_menu)
    {
      /* Destroy and repopulate the menu at the level where the order changed */
      gtk_container_foreach (GTK_CONTAINER (menu), 
			     (GtkCallback) gtk_widget_destroy, NULL);

      gtk_tree_menu_populate (menu);

      /* Resize everything */
      gtk_cell_area_context_flush (menu->priv->context);
    }
}

static void
context_size_changed_cb (GtkCellAreaContext  *context,
			 GParamSpec          *pspec,
			 GtkWidget           *menu)
{
  if (!strcmp (pspec->name, "minimum-width") ||
      !strcmp (pspec->name, "natural-width") ||
      !strcmp (pspec->name, "minimum-height") ||
      !strcmp (pspec->name, "natural-height"))
    gtk_widget_queue_resize (menu);
}

static void
gtk_tree_menu_set_area (GtkTreeMenu *menu,
			GtkCellArea *area)
{
  GtkTreeMenuPrivate *priv = menu->priv;

  if (priv->area)
    g_object_unref (priv->area);

  priv->area = area;

  if (priv->area)
    g_object_ref_sink (priv->area);
}

static GtkWidget *
gtk_tree_menu_create_item (GtkTreeMenu *menu,
			   GtkTreeIter *iter,
			   gboolean     header_item)
{
  GtkTreeMenuPrivate *priv = menu->priv;
  GtkWidget          *item, *view;
  GtkTreePath        *path;
  gboolean            is_separator = FALSE;

  path = gtk_tree_model_get_path (priv->model, iter);

  if (priv->row_separator_func)
    is_separator = 
      priv->row_separator_func (priv->model, iter,
				priv->row_separator_data);

  if (is_separator)
    {
      item = gtk_separator_menu_item_new ();

      g_object_set_qdata_full (G_OBJECT (item),
			       tree_menu_path_quark,
			       gtk_tree_row_reference_new (priv->model, path),
			       (GDestroyNotify)gtk_tree_row_reference_free);
    }
  else
    {
      view = gtk_cell_view_new_with_context (priv->area, priv->context);
      item = gtk_menu_item_new ();
      gtk_widget_show (view);
      gtk_widget_show (item);
      
      gtk_cell_view_set_model (GTK_CELL_VIEW (view), priv->model);
      gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (view), path);
      
      gtk_widget_show (view);
      gtk_container_add (GTK_CONTAINER (item), view);
      
      g_signal_connect (item, "activate", G_CALLBACK (item_activated_cb), menu);

      /* Add a GtkTreeMenu submenu to render the children of this row */
      if (header_item == FALSE &&
	  gtk_tree_model_iter_has_child (priv->model, iter))
	{
	  GtkWidget           *submenu;

	  submenu = gtk_tree_menu_new_with_area (priv->area);

	  gtk_tree_menu_set_row_separator_func (GTK_TREE_MENU (submenu), 
						priv->row_separator_func,
						priv->row_separator_data,
						priv->row_separator_destroy);
	  gtk_tree_menu_set_header_func (GTK_TREE_MENU (submenu), 
					 priv->header_func,
					 priv->header_data,
					 priv->header_destroy);

	  gtk_tree_menu_set_model (GTK_TREE_MENU (submenu), priv->model);
	  gtk_tree_menu_set_root (GTK_TREE_MENU (submenu), path);

	  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);

	  g_signal_connect (submenu, "menu-activate", 
			    G_CALLBACK (submenu_activated_cb), menu);
	}
    }

  gtk_tree_path_free (path);

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
	{
	  valid = gtk_tree_model_iter_children (priv->model, &iter, &parent);

	  if (priv->header_func && 
	      priv->header_func (priv->model, &parent, priv->header_data))
	    {
	      /* Add a submenu header for rows which desire one, used for
	       * combo boxes to allow all rows to be activatable/selectable 
	       */
	      menu_item = gtk_tree_menu_create_item (menu, &parent, TRUE);
	      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	      
	      menu_item = gtk_separator_menu_item_new ();
	      gtk_widget_show (menu_item);
	      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

	      priv->menu_with_header = TRUE;
	    }
	}
      gtk_tree_path_free (path);
    }
  else
    {
      /* Tearoff menu items only go in the root menu */
      if (priv->tearoff)
	{
	  menu_item = gtk_tearoff_menu_item_new ();
	  gtk_widget_show (menu_item);

	  /* Here if wrap_width > 0 then we need to attach the menu
	   * item to span the entire first row of the grid menu */
	  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	}


      valid = gtk_tree_model_iter_children (priv->model, &iter, NULL);
    }

  /* Create a menu item for every row at the current depth, add a GtkTreeMenu
   * submenu for iters/items that have children */
  while (valid)
    {
      menu_item = gtk_tree_menu_create_item (menu, &iter, FALSE);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

      valid = gtk_tree_model_iter_next (priv->model, &iter);
    }
}

static void
item_activated_cb (GtkMenuItem          *item,
		   GtkTreeMenu          *menu)
{
  GtkCellView *view;
  GtkTreePath *path;
  gchar       *path_str;

  /* Only activate leafs, not parents */
  if (!gtk_menu_item_get_submenu (item))
    {
      view     = GTK_CELL_VIEW (gtk_bin_get_child (GTK_BIN (item)));
      path     = gtk_cell_view_get_displayed_row (view);
      path_str = gtk_tree_path_to_string (path);
      
      g_signal_emit (menu, tree_menu_signals[SIGNAL_MENU_ACTIVATE], 0, path_str);
      
      g_free (path_str);
      gtk_tree_path_free (path);
    }
}

static void
submenu_activated_cb (GtkTreeMenu          *submenu,
		      const gchar          *path,
		      GtkTreeMenu          *menu)
{
  g_signal_emit (menu, tree_menu_signals[SIGNAL_MENU_ACTIVATE], 0, path);
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
	  g_signal_handler_disconnect (priv->model,
				       priv->row_inserted_id);
	  g_signal_handler_disconnect (priv->model,
				       priv->row_deleted_id);
	  g_signal_handler_disconnect (priv->model,
				       priv->row_reordered_id);
	  priv->row_inserted_id  = 0;
	  priv->row_deleted_id   = 0;
	  priv->row_reordered_id = 0;

	  g_object_unref (priv->model);
	}

      priv->model = model;

      if (priv->model)
	{
	  g_object_ref (priv->model);

	  /* Connect signals */
	  priv->row_inserted_id  = g_signal_connect (priv->model, "row-inserted",
						     G_CALLBACK (row_inserted_cb), menu);
	  priv->row_deleted_id   = g_signal_connect (priv->model, "row-deleted",
						     G_CALLBACK (row_deleted_cb), menu);
	  priv->row_reordered_id = g_signal_connect (priv->model, "rows-reordered",
						     G_CALLBACK (row_reordered_cb), menu);
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

gboolean
gtk_tree_menu_get_tearoff (GtkTreeMenu *menu)
{
  GtkTreeMenuPrivate *priv;

  g_return_val_if_fail (GTK_IS_TREE_MENU (menu), FALSE);

  priv = menu->priv;

  return priv->tearoff;
}

void
gtk_tree_menu_set_tearoff (GtkTreeMenu *menu,
			   gboolean     tearoff)
{
  GtkTreeMenuPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_MENU (menu));

  priv = menu->priv;

  if (priv->tearoff != tearoff)
    {
      priv->tearoff = tearoff;

      /* Destroy all the menu items */
      gtk_container_foreach (GTK_CONTAINER (menu), 
			     (GtkCallback) gtk_widget_destroy, NULL);
      
      /* Populate again */
      if (priv->model)
	gtk_tree_menu_populate (menu);

      g_object_notify (G_OBJECT (menu), "tearoff");
    }
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

  /* Destroy all the menu items */
  gtk_container_foreach (GTK_CONTAINER (menu), 
			 (GtkCallback) gtk_widget_destroy, NULL);
  
  /* Populate again */
  if (priv->model)
    gtk_tree_menu_populate (menu);
}

GtkTreeViewRowSeparatorFunc
gtk_tree_menu_get_row_separator_func (GtkTreeMenu *menu)
{
  GtkTreeMenuPrivate *priv;

  g_return_val_if_fail (GTK_IS_TREE_MENU (menu), NULL);

  priv = menu->priv;

  return priv->row_separator_func;
}

void
gtk_tree_menu_set_header_func (GtkTreeMenu          *menu,
			       GtkTreeMenuHeaderFunc func,
			       gpointer              data,
			       GDestroyNotify        destroy)
{
  GtkTreeMenuPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_MENU (menu));

  priv = menu->priv;

  if (priv->header_destroy)
    priv->header_destroy (priv->header_data);

  priv->header_func    = func;
  priv->header_data    = data;
  priv->header_destroy = destroy;

  /* Destroy all the menu items */
  gtk_container_foreach (GTK_CONTAINER (menu), 
			 (GtkCallback) gtk_widget_destroy, NULL);
  
  /* Populate again */
  if (priv->model)
    gtk_tree_menu_populate (menu);
}

GtkTreeMenuHeaderFunc
gtk_tree_menu_get_header_func (GtkTreeMenu *menu)
{
  GtkTreeMenuPrivate *priv;

  g_return_val_if_fail (GTK_IS_TREE_MENU (menu), NULL);

  priv = menu->priv;

  return priv->header_func;
}
