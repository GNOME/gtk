/* gtktreemenu.c
 *
 * Copyright (C) 2010 Openismus GmbH
 *
 * Authors:
 *      Tristan Van Berkom <tristanvb@openismus.com>
 *
 * Based on some GtkComboBox menu code by Kristian Rietveld <kris@gtk.org>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * SECTION:gtktreemenu
 * @Short_Description: A GtkMenu automatically created from a #GtkTreeModel
 * @Title: GtkTreeMenu
 *
 * The #GtkTreeMenu is used to display a drop-down menu allowing selection
 * of every row in the model and is used by the #GtkComboBox for its drop-down
 * menu.
 */

#include "config.h"
#include "gtkintl.h"
#include "gtktreemenu.h"
#include "gtkmarshalers.h"
#include "gtkmenuitem.h"
#include "gtkseparatormenuitem.h"
#include "gtkcellareabox.h"
#include "gtkcellareacontext.h"
#include "gtkcelllayout.h"
#include "gtkcellview.h"
#include "gtkmenushellprivate.h"
#include "gtkprivate.h"

/* GObjectClass */
static void      gtk_tree_menu_constructed                    (GObject            *object);
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
static void gtk_tree_menu_measure (GtkWidget      *widget,
                                   GtkOrientation  orientation,
                                   int             for_size,
                                   int            *minimum,
                                   int            *natural,
                                   int            *minimum_baseline,
                                   int            *natural_baseline);

/* GtkCellLayoutIface */
static void      gtk_tree_menu_cell_layout_init               (GtkCellLayoutIface  *iface);
static GtkCellArea *gtk_tree_menu_cell_layout_get_area        (GtkCellLayout        *layout);


/* TreeModel/DrawingArea callbacks and building menus/submenus */
static inline void rebuild_menu                               (GtkTreeMenu          *menu);
static void       gtk_tree_menu_populate                      (GtkTreeMenu          *menu);
static GtkWidget *gtk_tree_menu_create_item                   (GtkTreeMenu          *menu,
                                                               GtkTreeIter          *iter,
                                                               gboolean              header_item);
static void       gtk_tree_menu_create_submenu                (GtkTreeMenu          *menu,
                                                               GtkWidget            *item,
                                                               GtkTreePath          *path);
static void       gtk_tree_menu_set_area                      (GtkTreeMenu          *menu,
                                                               GtkCellArea          *area);
static GtkWidget *gtk_tree_menu_get_path_item                 (GtkTreeMenu          *menu,
                                                               GtkTreePath          *path);
static gboolean   gtk_tree_menu_path_in_menu                  (GtkTreeMenu          *menu,
                                                               GtkTreePath          *path,
                                                               gboolean             *header_item);
static void       row_inserted_cb                             (GtkTreeModel         *model,
                                                               GtkTreePath          *path,
                                                               GtkTreeIter          *iter,
                                                               GtkTreeMenu          *menu);
static void       row_deleted_cb                              (GtkTreeModel         *model,
                                                               GtkTreePath          *path,
                                                               GtkTreeMenu          *menu);
static void       row_reordered_cb                            (GtkTreeModel         *model,
                                                               GtkTreePath          *path,
                                                               GtkTreeIter          *iter,
                                                               gint                 *new_order,
                                                               GtkTreeMenu          *menu);
static void       row_changed_cb                              (GtkTreeModel         *model,
                                                               GtkTreePath          *path,
                                                               GtkTreeIter          *iter,
                                                               GtkTreeMenu          *menu);
static void       context_size_changed_cb                     (GtkCellAreaContext   *context,
                                                               GParamSpec           *pspec,
                                                               GtkWidget            *menu);
static void       area_apply_attributes_cb                    (GtkCellArea          *area,
                                                               GtkTreeModel         *tree_model,
                                                               GtkTreeIter          *iter,
                                                               gboolean              is_expander,
                                                               gboolean              is_expanded,
                                                               GtkTreeMenu          *menu);
static void       item_activated_cb                           (GtkMenuItem          *item,
                                                               GtkTreeMenu          *menu);
static void       submenu_activated_cb                        (GtkTreeMenu          *submenu,
                                                               const gchar          *path,
                                                               GtkTreeMenu          *menu);
static void       gtk_tree_menu_set_model_internal            (GtkTreeMenu          *menu,
                                                               GtkTreeModel         *model);



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
  gulong               apply_attributes_id;
  gulong               row_inserted_id;
  gulong               row_deleted_id;
  gulong               row_reordered_id;
  gulong               row_changed_id;

  /* Flags */
  guint32              menu_with_header : 1;

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

enum {
  SIGNAL_MENU_ACTIVATE,
  N_SIGNALS
};

static guint   tree_menu_signals[N_SIGNALS] = { 0 };
static GQuark  tree_menu_path_quark = 0;

G_DEFINE_TYPE_WITH_CODE (GtkTreeMenu, _gtk_tree_menu, GTK_TYPE_MENU,
                         G_ADD_PRIVATE (GtkTreeMenu)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
                                                gtk_tree_menu_cell_layout_init));

static void
_gtk_tree_menu_init (GtkTreeMenu *menu)
{
  menu->priv = _gtk_tree_menu_get_instance_private (menu);

  gtk_menu_set_reserve_toggle_size (GTK_MENU (menu), FALSE);
}

static void
_gtk_tree_menu_class_init (GtkTreeMenuClass *class)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  tree_menu_path_quark = g_quark_from_static_string ("gtk-tree-menu-path");

  object_class->constructed  = gtk_tree_menu_constructed;
  object_class->dispose      = gtk_tree_menu_dispose;
  object_class->finalize     = gtk_tree_menu_finalize;
  object_class->set_property = gtk_tree_menu_set_property;
  object_class->get_property = gtk_tree_menu_get_property;

  widget_class->measure = gtk_tree_menu_measure;

  /*
   * GtkTreeMenu::menu-activate:
   * @menu: a #GtkTreeMenu
   * @path: the #GtkTreePath string for the item which was activated
   * @user_data: the user data
   *
   * This signal is emitted to notify that a menu item in the #GtkTreeMenu
   * was activated and provides the path string from the #GtkTreeModel
   * to specify which row was selected.
   */
  tree_menu_signals[SIGNAL_MENU_ACTIVATE] =
    g_signal_new (I_("menu-activate"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0, /* No class closure here */
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING);

  /*
   * GtkTreeMenu:model:
   *
   * The #GtkTreeModel from which the menu is constructed.
   */
  g_object_class_install_property (object_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
                                                        P_("TreeMenu model"),
                                                        P_("The model for the tree menu"),
                                                        GTK_TYPE_TREE_MODEL,
                                                        GTK_PARAM_READWRITE));

  /*
   * GtkTreeMenu:root:
   *
   * The #GtkTreePath that is the root for this menu, or %NULL.
   *
   * The #GtkTreeMenu recursively creates submenus for #GtkTreeModel
   * rows that have children and the "root" for each menu is provided
   * by the parent menu.
   *
   * If you dont provide a root for the #GtkTreeMenu then the whole
   * model will be added to the menu. Specifying a root allows you
   * to build a menu for a given #GtkTreePath and its children.
   */
  g_object_class_install_property (object_class,
                                   PROP_ROOT,
                                   g_param_spec_boxed ("root",
                                                       P_("TreeMenu root row"),
                                                       P_("The TreeMenu will display children of the "
                                                          "specified root"),
                                                       GTK_TYPE_TREE_PATH,
                                                       GTK_PARAM_READWRITE));

  /*
   * GtkTreeMenu:cell-area:
   *
   * The #GtkCellArea used to render cells in the menu items.
   *
   * You can provide a different cell area at object construction
   * time, otherwise the #GtkTreeMenu will use a #GtkCellAreaBox.
   */
  g_object_class_install_property (object_class,
                                   PROP_CELL_AREA,
                                   g_param_spec_object ("cell-area",
                                                        P_("Cell Area"),
                                                        P_("The GtkCellArea used to layout cells"),
                                                        GTK_TYPE_CELL_AREA,
                                                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

/****************************************************************
 *                         GObjectClass                         *
 ****************************************************************/
static void
gtk_tree_menu_constructed (GObject *object)
{
  GtkTreeMenu *menu = GTK_TREE_MENU (object);
  GtkTreeMenuPrivate *priv = menu->priv;

  G_OBJECT_CLASS (_gtk_tree_menu_parent_class)->constructed (object);

  if (!priv->area)
    {
      GtkCellArea *area = gtk_cell_area_box_new ();

      gtk_tree_menu_set_area (menu, area);
    }

  priv->context = gtk_cell_area_create_context (priv->area);

  priv->size_changed_id =
    g_signal_connect (priv->context, "notify",
                      G_CALLBACK (context_size_changed_cb), menu);
}

static void
gtk_tree_menu_dispose (GObject *object)
{
  GtkTreeMenu        *menu;
  GtkTreeMenuPrivate *priv;

  menu = GTK_TREE_MENU (object);
  priv = menu->priv;

  _gtk_tree_menu_set_model (menu, NULL);
  gtk_tree_menu_set_area (menu, NULL);

  if (priv->context)
    {
      /* Disconnect signals */
      g_signal_handler_disconnect (priv->context, priv->size_changed_id);

      g_object_unref (priv->context);
      priv->context = NULL;
      priv->size_changed_id = 0;
    }

  G_OBJECT_CLASS (_gtk_tree_menu_parent_class)->dispose (object);
}

static void
gtk_tree_menu_finalize (GObject *object)
{
  GtkTreeMenu        *menu;
  GtkTreeMenuPrivate *priv;

  menu = GTK_TREE_MENU (object);
  priv = menu->priv;

  _gtk_tree_menu_set_row_separator_func (menu, NULL, NULL, NULL);

  if (priv->root)
    gtk_tree_row_reference_free (priv->root);

  G_OBJECT_CLASS (_gtk_tree_menu_parent_class)->finalize (object);
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
      _gtk_tree_menu_set_model (menu, g_value_get_object (value));
      break;

    case PROP_ROOT:
      _gtk_tree_menu_set_root (menu, g_value_get_boxed (value));
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
gtk_tree_menu_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             for_size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkTreeMenu *menu = GTK_TREE_MENU (widget);
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

  GTK_WIDGET_CLASS (_gtk_tree_menu_parent_class)->measure (widget,
                                                           orientation,
                                                           for_size,
                                                           minimum, natural,
                                                           minimum_baseline, natural_baseline);

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

          if (row)
            {
              path = gtk_tree_row_reference_get_path (row);

              if (!path)
                /* Return any first child where its row-reference became invalid,
                 * this is because row-references get null paths before we recieve
                 * the "row-deleted" signal.
                 */
                item = child;
            }
        }
      else
        {
          GtkWidget *view = gtk_bin_get_child (GTK_BIN (child));

          /* It's always a cellview */
          if (GTK_IS_CELL_VIEW (view))
            path = gtk_cell_view_get_displayed_row (GTK_CELL_VIEW (view));

          if (!path)
            /* Return any first child where its row-reference became invalid,
             * this is because row-references get null paths before we recieve
             * the "row-deleted" signal.
             */
            item = child;
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

static gboolean
gtk_tree_menu_path_in_menu (GtkTreeMenu  *menu,
                            GtkTreePath  *path,
                            gboolean     *header_item)
{
  GtkTreeMenuPrivate *priv = menu->priv;
  gboolean            in_menu = FALSE;
  gboolean            is_header = FALSE;

  /* Check if the is in root of the model */
  if (gtk_tree_path_get_depth (path) == 1 && !priv->root)
    in_menu = TRUE;
  /* If we are a submenu, compare the parent path */
  else if (priv->root)
    {
      GtkTreePath *root_path   = gtk_tree_row_reference_get_path (priv->root);
      GtkTreePath *search_path = gtk_tree_path_copy (path);

      if (root_path)
        {
          if (priv->menu_with_header &&
              gtk_tree_path_compare (root_path, search_path) == 0)
            {
              in_menu   = TRUE;
              is_header = TRUE;
            }
          else if (gtk_tree_path_get_depth (search_path) > 1)
            {
              gtk_tree_path_up (search_path);

              if (gtk_tree_path_compare (root_path, search_path) == 0)
                in_menu = TRUE;
            }
        }
      gtk_tree_path_free (root_path);
      gtk_tree_path_free (search_path);
    }

  if (header_item)
    *header_item = is_header;

  return in_menu;
}

static GtkWidget *
gtk_tree_menu_path_needs_submenu (GtkTreeMenu *menu,
                                  GtkTreePath *search)
{
  GtkWidget   *item = NULL;
  GList       *children, *l;
  GtkTreePath *parent_path;

  if (gtk_tree_path_get_depth (search) <= 1)
    return NULL;

  parent_path = gtk_tree_path_copy (search);
  gtk_tree_path_up (parent_path);

  children    = gtk_container_get_children (GTK_CONTAINER (menu));

  for (l = children; item == NULL && l != NULL; l = l->next)
    {
      GtkWidget   *child = l->data;
      GtkTreePath *path  = NULL;

      /* Separators dont get submenus, if it already has a submenu then let
       * the submenu handle inserted rows */
      if (!GTK_IS_SEPARATOR_MENU_ITEM (child) &&
          !gtk_menu_item_get_submenu (GTK_MENU_ITEM (child)))
        {
          GtkWidget *view = gtk_bin_get_child (GTK_BIN (child));

          /* It's always a cellview */
          if (GTK_IS_CELL_VIEW (view))
            path = gtk_cell_view_get_displayed_row (GTK_CELL_VIEW (view));
        }

      if (path)
        {
          if (gtk_tree_path_compare (parent_path, path) == 0)
            item = child;

          gtk_tree_path_free (path);
        }
    }

  g_list_free (children);
  gtk_tree_path_free (parent_path);

  return item;
}

static GtkWidget *
find_empty_submenu (GtkTreeMenu  *menu)
{
  GtkTreeMenuPrivate *priv = menu->priv;
  GList              *children, *l;
  GtkWidget          *submenu = NULL;

  children = gtk_container_get_children (GTK_CONTAINER (menu));

  for (l = children; submenu == NULL && l != NULL; l = l->next)
    {
      GtkWidget   *child = l->data;
      GtkTreePath *path  = NULL;
      GtkTreeIter  iter;

      /* Separators dont get submenus, if it already has a submenu then let
       * the submenu handle inserted rows */
      if (!GTK_IS_SEPARATOR_MENU_ITEM (child))
        {
          GtkWidget *view = gtk_bin_get_child (GTK_BIN (child));

          /* It's always a cellview */
          if (GTK_IS_CELL_VIEW (view))
            path = gtk_cell_view_get_displayed_row (GTK_CELL_VIEW (view));
        }

      if (path)
        {
          if (gtk_tree_model_get_iter (priv->model, &iter, path) &&
              !gtk_tree_model_iter_has_child (priv->model, &iter))
            submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (child));

          gtk_tree_path_free (path);
        }
    }

  g_list_free (children);

  return submenu;
}

static void
row_inserted_cb (GtkTreeModel     *model,
                 GtkTreePath      *path,
                 GtkTreeIter      *iter,
                 GtkTreeMenu      *menu)
{
  GtkTreeMenuPrivate *priv = menu->priv;
  gint               *indices, index, depth;
  GtkWidget          *item;

  /* If the iter should be in this menu then go ahead and insert it */
  if (gtk_tree_menu_path_in_menu (menu, path, NULL))
    {
      /* Get the index of the path for this depth */
      indices = gtk_tree_path_get_indices (path);
      depth   = gtk_tree_path_get_depth (path);
      index   = indices[depth -1];

      /* Menus with a header include a menuitem for its root node
       * and a separator menu item */
      if (priv->menu_with_header)
        index += 2;

      item = gtk_tree_menu_create_item (menu, iter, FALSE);
      gtk_menu_shell_insert (GTK_MENU_SHELL (menu), item, index);

      /* Resize everything */
      gtk_cell_area_context_reset (menu->priv->context);
    }
  else
    {
      /* Create submenus for iters if we need to */
      item = gtk_tree_menu_path_needs_submenu (menu, path);
      if (item)
        {
          GtkTreePath *item_path = gtk_tree_path_copy (path);

          gtk_tree_path_up (item_path);
          gtk_tree_menu_create_submenu (menu, item, item_path);
          gtk_tree_path_free (item_path);
        }
    }
}

static void
row_deleted_cb (GtkTreeModel     *model,
                GtkTreePath      *path,
                GtkTreeMenu      *menu)
{
  GtkWidget          *item;

  /* If it's the header item we leave it to the parent menu
   * to remove us from its menu
   */
  item = gtk_tree_menu_get_path_item (menu, path);

  if (item)
    {
      /* Get rid of the deleted item */
      gtk_widget_destroy (item);

      /* Resize everything */
      gtk_cell_area_context_reset (menu->priv->context);
    }
  else
    {
      /* It's up to the parent menu to destroy a child menu that becomes empty
       * since the topmost menu belongs to the user and is allowed to have no contents */
      GtkWidget *submenu = find_empty_submenu (menu);
      if (submenu)
        gtk_widget_destroy (submenu);
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

  if (gtk_tree_path_get_depth (path) == 0 && !priv->root)
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
    rebuild_menu (menu);
}

static gint
menu_item_position (GtkTreeMenu *menu,
                    GtkWidget   *item)
{
  GList *children;
  gint   position;

  children = gtk_container_get_children (GTK_CONTAINER (menu));
  position = g_list_index (children, item);
  g_list_free (children);

  return position;
}

static void
row_changed_cb (GtkTreeModel         *model,
                GtkTreePath          *path,
                GtkTreeIter          *iter,
                GtkTreeMenu          *menu)
{
  GtkTreeMenuPrivate *priv = menu->priv;
  gboolean            is_separator = FALSE;
  GtkWidget          *item;
  GList *children;

  item = gtk_tree_menu_get_path_item (menu, path);

  if (priv->root)
    {
      GtkTreePath *root_path =
        gtk_tree_row_reference_get_path (priv->root);

      if (root_path && gtk_tree_path_compare (root_path, path) == 0)
        {
          if (item)
            {
              /* Destroy the header item and then the following separator */
              gtk_widget_destroy (item);
              children = gtk_menu_shell_get_items (GTK_MENU_SHELL (menu));
              gtk_widget_destroy (children->data);
              g_list_free (children);

              priv->menu_with_header = FALSE;
            }

          gtk_tree_path_free (root_path);
        }
    }

  if (item)
    {
      if (priv->row_separator_func)
        is_separator = priv->row_separator_func (model, iter, priv->row_separator_data);

      if (is_separator != GTK_IS_SEPARATOR_MENU_ITEM (item))
        {
          gint position = menu_item_position (menu, item);

          gtk_widget_destroy (item);
          item = gtk_tree_menu_create_item (menu, iter, FALSE);
          gtk_menu_shell_insert (GTK_MENU_SHELL (menu), item, position);
        }
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

static gboolean
area_is_sensitive (GtkCellArea *area)
{
  GList    *cells, *list;
  gboolean  sensitive = FALSE;

  cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (area));

  for (list = cells; list; list = list->next)
    {
      g_object_get (list->data, "sensitive", &sensitive, NULL);

      if (sensitive)
        break;
    }
  g_list_free (cells);

  return sensitive;
}

static void
area_apply_attributes_cb (GtkCellArea          *area,
                          GtkTreeModel         *tree_model,
                          GtkTreeIter          *iter,
                          gboolean              is_expander,
                          gboolean              is_expanded,
                          GtkTreeMenu          *menu)
{
  /* If the menu for this iter has a submenu */
  GtkTreeMenuPrivate *priv = menu->priv;
  GtkTreePath        *path;
  GtkWidget          *item;
  gboolean            is_header;
  gboolean            sensitive;
  GList *children;

  path = gtk_tree_model_get_path (tree_model, iter);

  if (gtk_tree_menu_path_in_menu (menu, path, &is_header))
    {
      item = gtk_tree_menu_get_path_item (menu, path);

      /* If there is no submenu, go ahead and update item sensitivity,
       * items with submenus are always sensitive */
      if (item && !gtk_menu_item_get_submenu (GTK_MENU_ITEM (item)))
        {
          sensitive = area_is_sensitive (priv->area);

          gtk_widget_set_sensitive (item, sensitive);

          if (is_header)
            {
              /* For header items we need to set the sensitivity
               * of the following separator item
               */
              children = gtk_menu_shell_get_items (GTK_MENU_SHELL (menu));
              if (children && children->next)
                {
                  GtkWidget *separator = children->next->data;

                  gtk_widget_set_sensitive (separator, sensitive);
                }
              g_list_free (children);
            }
        }
    }

  gtk_tree_path_free (path);
}

static void
gtk_tree_menu_set_area (GtkTreeMenu *menu,
                        GtkCellArea *area)
{
  GtkTreeMenuPrivate *priv = menu->priv;

  if (priv->area)
    {
      g_signal_handler_disconnect (priv->area,
                                   priv->apply_attributes_id);
      priv->apply_attributes_id = 0;

      g_object_unref (priv->area);
    }

  priv->area = area;

  if (priv->area)
    {
      g_object_ref_sink (priv->area);

      priv->apply_attributes_id =
        g_signal_connect (priv->area, "apply-attributes",
                          G_CALLBACK (area_apply_attributes_cb), menu);
    }
}

static void
gtk_tree_menu_create_submenu (GtkTreeMenu *menu,
                              GtkWidget   *item,
                              GtkTreePath *path)
{
  GtkTreeMenuPrivate *priv = menu->priv;
  GtkWidget          *view;
  GtkWidget          *submenu;

  view = gtk_bin_get_child (GTK_BIN (item));
  gtk_cell_view_set_draw_sensitive (GTK_CELL_VIEW (view), TRUE);

  submenu = _gtk_tree_menu_new_with_area (priv->area);

  _gtk_tree_menu_set_row_separator_func (GTK_TREE_MENU (submenu),
                                         priv->row_separator_func,
                                         priv->row_separator_data,
                                         priv->row_separator_destroy);

  gtk_tree_menu_set_model_internal (GTK_TREE_MENU (submenu), priv->model);
  _gtk_tree_menu_set_root (GTK_TREE_MENU (submenu), path);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);

  g_signal_connect (submenu, "menu-activate",
                    G_CALLBACK (submenu_activated_cb), menu);
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

      gtk_cell_view_set_model (GTK_CELL_VIEW (view), priv->model);
      gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (view), path);

      gtk_container_add (GTK_CONTAINER (item), view);

      g_signal_connect (item, "activate", G_CALLBACK (item_activated_cb), menu);

      /* Add a GtkTreeMenu submenu to render the children of this row */
      if (header_item == FALSE &&
          gtk_tree_model_iter_has_child (priv->model, iter))
        gtk_tree_menu_create_submenu (menu, item, path);
    }

  gtk_tree_path_free (path);

  return item;
}

static inline void
rebuild_menu (GtkTreeMenu *menu)
{
  GtkTreeMenuPrivate *priv = menu->priv;

  /* Destroy all the menu items */
  gtk_container_foreach (GTK_CONTAINER (menu),
                         (GtkCallback) gtk_widget_destroy, NULL);

  /* Populate */
  if (priv->model)
    gtk_tree_menu_populate (menu);
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
    {
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

/* Sets the model without rebuilding the menu, prevents
 * infinite recursion while building submenus (we wait
 * until the root is set and then build the menu) */
static void
gtk_tree_menu_set_model_internal (GtkTreeMenu  *menu,
                                  GtkTreeModel *model)
{
  GtkTreeMenuPrivate *priv;

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
          g_signal_handler_disconnect (priv->model,
                                       priv->row_changed_id);
          priv->row_inserted_id  = 0;
          priv->row_deleted_id   = 0;
          priv->row_reordered_id = 0;
          priv->row_changed_id = 0;

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
          priv->row_changed_id   = g_signal_connect (priv->model, "row-changed",
                                                     G_CALLBACK (row_changed_cb), menu);
        }
    }
}

/****************************************************************
 *                            API                               *
 ****************************************************************/

/**
 * _gtk_tree_menu_new:
 *
 * Creates a new #GtkTreeMenu.
 *
 * Returns: A newly created #GtkTreeMenu with no model or root.
 */
GtkWidget *
_gtk_tree_menu_new (void)
{
  return (GtkWidget *)g_object_new (GTK_TYPE_TREE_MENU, NULL);
}

/*
 * _gtk_tree_menu_new_with_area:
 * @area: the #GtkCellArea to use to render cells in the menu
 *
 * Creates a new #GtkTreeMenu using @area to render its cells.
 *
 * Returns: A newly created #GtkTreeMenu with no model or root.
 */
GtkWidget *
_gtk_tree_menu_new_with_area (GtkCellArea    *area)
{
  return (GtkWidget *)g_object_new (GTK_TYPE_TREE_MENU,
                                    "cell-area", area,
                                    NULL);
}

/*
 * _gtk_tree_menu_new_full:
 * @area: (allow-none): the #GtkCellArea to use to render cells in the menu, or %NULL.
 * @model: (allow-none): the #GtkTreeModel to build the menu hierarchy from, or %NULL.
 * @root: (allow-none): the #GtkTreePath indicating the root row for this menu, or %NULL.
 *
 * Creates a new #GtkTreeMenu hierarchy from the provided @model and @root using @area to render its cells.
 *
 * Returns: A newly created #GtkTreeMenu.
 */
GtkWidget *
_gtk_tree_menu_new_full (GtkCellArea         *area,
                         GtkTreeModel        *model,
                         GtkTreePath         *root)
{
  return (GtkWidget *)g_object_new (GTK_TYPE_TREE_MENU,
                                    "cell-area", area,
                                    "model", model,
                                    "root", root,
                                    NULL);
}

/*
 * _gtk_tree_menu_set_model:
 * @menu: a #GtkTreeMenu
 * @model: (allow-none): the #GtkTreeModel to build the menu hierarchy from, or %NULL.
 *
 * Sets @model to be used to build the menu hierarhcy.
 */
void
_gtk_tree_menu_set_model (GtkTreeMenu  *menu,
                          GtkTreeModel *model)
{
  g_return_if_fail (GTK_IS_TREE_MENU (menu));
  g_return_if_fail (model == NULL || GTK_IS_TREE_MODEL (model));

  gtk_tree_menu_set_model_internal (menu, model);

  rebuild_menu (menu);
}

/*
 * _gtk_tree_menu_get_model:
 * @menu: a #GtkTreeMenu
 *
 * Gets the @model currently used for the menu hierarhcy.
 *
 * Returns: (transfer none): the #GtkTreeModel which is used
 * for @menu’s hierarchy.
 */
GtkTreeModel *
_gtk_tree_menu_get_model (GtkTreeMenu *menu)
{
  GtkTreeMenuPrivate *priv;

  g_return_val_if_fail (GTK_IS_TREE_MENU (menu), NULL);

  priv = menu->priv;

  return priv->model;
}

/*
 * _gtk_tree_menu_set_root:
 * @menu: a #GtkTreeMenu
 * @path: (allow-none): the #GtkTreePath which is the root of @menu, or %NULL.
 *
 * Sets the root of a @menu’s hierarchy to be @path. @menu must already
 * have a model set and @path must point to a valid path inside the model.
 */
void
_gtk_tree_menu_set_root (GtkTreeMenu *menu,
                         GtkTreePath *path)
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

  rebuild_menu (menu);
}

/*
 * _gtk_tree_menu_get_root:
 * @menu: a #GtkTreeMenu
 *
 * Gets the @root path for @menu’s hierarchy, or returns %NULL if @menu
 * has no model or is building a hierarchy for the entire model. *
 *
 * Returns: (transfer full) (allow-none): A newly created #GtkTreePath
 * pointing to the root of @menu which must be freed with gtk_tree_path_free().
 */
GtkTreePath *
_gtk_tree_menu_get_root (GtkTreeMenu *menu)
{
  GtkTreeMenuPrivate *priv;

  g_return_val_if_fail (GTK_IS_TREE_MENU (menu), NULL);

  priv = menu->priv;

  if (priv->root)
    return gtk_tree_row_reference_get_path (priv->root);

  return NULL;
}

/*
 * _gtk_tree_menu_get_row_separator_func:
 * @menu: a #GtkTreeMenu
 *
 * Gets the current #GtkTreeViewRowSeparatorFunc separator function.
 *
 * Returns: the current row separator function.
 */
GtkTreeViewRowSeparatorFunc
_gtk_tree_menu_get_row_separator_func (GtkTreeMenu *menu)
{
  GtkTreeMenuPrivate *priv;

  g_return_val_if_fail (GTK_IS_TREE_MENU (menu), NULL);

  priv = menu->priv;

  return priv->row_separator_func;
}

/*
 * _gtk_tree_menu_set_row_separator_func:
 * @menu: a #GtkTreeMenu
 * @func: (allow-none): a #GtkTreeViewRowSeparatorFunc, or %NULL to unset the separator function.
 * @data: (allow-none): user data to pass to @func, or %NULL
 * @destroy: (allow-none): destroy notifier for @data, or %NULL
 *
 * Sets the row separator function, which is used to determine
 * whether a row should be drawn as a separator. If the row separator
 * function is %NULL, no separators are drawn. This is the default value.
 */
void
_gtk_tree_menu_set_row_separator_func (GtkTreeMenu          *menu,
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

  rebuild_menu (menu);
}
