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

#undef GDK_DEPRECATED
#undef GDK_DEPRECATED_FOR
#define GDK_DEPRECATED
#define GDK_DEPRECATED_FOR(f)

#include "deprecated/gtktearoffmenuitem.h"

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
static inline void rebuild_menu                               (GtkTreeMenu          *menu);
static gboolean   menu_occupied                               (GtkTreeMenu          *menu,
                                                               guint                 left_attach,
                                                               guint                 right_attach,
                                                               guint                 top_attach,
                                                               guint                 bottom_attach);
static void       relayout_item                               (GtkTreeMenu          *menu,
                                                               GtkWidget            *item,
                                                               GtkTreeIter          *iter,
                                                               GtkWidget            *prev);
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

  /* Grid menu mode */
  gint                 wrap_width;
  gint                 row_span_col;
  gint                 col_span_col;

  /* Flags */
  guint32              menu_with_header : 1;
  guint32              tearoff     : 1;

  /* Row separators */
  GtkTreeViewRowSeparatorFunc row_separator_func;
  gpointer                    row_separator_data;
  GDestroyNotify              row_separator_destroy;

  /* Submenu headers */
  GtkTreeMenuHeaderFunc header_func;
  gpointer              header_data;
  GDestroyNotify        header_destroy;
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_ROOT,
  PROP_CELL_AREA,
  PROP_TEAROFF,
  PROP_WRAP_WIDTH,
  PROP_ROW_SPAN_COL,
  PROP_COL_SPAN_COL
};

enum {
  SIGNAL_MENU_ACTIVATE,
  N_SIGNALS
};

static guint   tree_menu_signals[N_SIGNALS] = { 0 };
static GQuark  tree_menu_path_quark = 0;

G_DEFINE_TYPE_WITH_CODE (GtkTreeMenu, _gtk_tree_menu, GTK_TYPE_MENU,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
                                                gtk_tree_menu_cell_layout_init));

static void
_gtk_tree_menu_init (GtkTreeMenu *menu)
{
  GtkTreeMenuPrivate *priv;

  menu->priv = G_TYPE_INSTANCE_GET_PRIVATE (menu,
                                            GTK_TYPE_TREE_MENU,
                                            GtkTreeMenuPrivate);
  priv = menu->priv;

  priv->model     = NULL;
  priv->root      = NULL;
  priv->area      = NULL;
  priv->context   = NULL;

  priv->size_changed_id  = 0;
  priv->row_inserted_id  = 0;
  priv->row_deleted_id   = 0;
  priv->row_reordered_id = 0;
  priv->row_changed_id   = 0;

  priv->wrap_width   = 0;
  priv->row_span_col = -1;
  priv->col_span_col = -1;

  priv->menu_with_header = FALSE;
  priv->tearoff          = FALSE;

  priv->row_separator_func    = NULL;
  priv->row_separator_data    = NULL;
  priv->row_separator_destroy = NULL;

  priv->header_func    = NULL;
  priv->header_data    = NULL;
  priv->header_destroy = NULL;

  gtk_menu_set_reserve_toggle_size (GTK_MENU (menu), FALSE);
}

static void
_gtk_tree_menu_class_init (GtkTreeMenuClass *class)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  tree_menu_path_quark = g_quark_from_static_string ("gtk-tree-menu-path");

  object_class->constructor  = gtk_tree_menu_constructor;
  object_class->dispose      = gtk_tree_menu_dispose;
  object_class->finalize     = gtk_tree_menu_finalize;
  object_class->set_property = gtk_tree_menu_set_property;
  object_class->get_property = gtk_tree_menu_get_property;

  widget_class->get_preferred_width  = gtk_tree_menu_get_preferred_width;
  widget_class->get_preferred_height = gtk_tree_menu_get_preferred_height;

  /*
   * GtkTreeMenu::menu-activate:
   * @menu: a #GtkTreeMenu
   * @path: the #GtkTreePath string for the item which was activated
   * @user_data: the user data
   *
   * This signal is emitted to notify that a menu item in the #GtkTreeMenu
   * was activated and provides the path string from the #GtkTreeModel
   * to specify which row was selected.
   *
   * Since: 3.0
   */
  tree_menu_signals[SIGNAL_MENU_ACTIVATE] =
    g_signal_new (I_("menu-activate"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0, /* No class closure here */
                  NULL, NULL,
                  _gtk_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);

  /*
   * GtkTreeMenu:model:
   *
   * The #GtkTreeModel from which the menu is constructed.
   *
   * Since: 3.0
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
   * 
   * Since: 3.0
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
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_CELL_AREA,
                                   g_param_spec_object ("cell-area",
                                                        P_("Cell Area"),
                                                        P_("The GtkCellArea used to layout cells"),
                                                        GTK_TYPE_CELL_AREA,
                                                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /*
   * GtkTreeMenu:tearoff:
   *
   * Specifies whether this menu comes with a leading tearoff menu item
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_TEAROFF,
                                   g_param_spec_boolean ("tearoff",
                                                         P_("Tearoff"),
                                                         P_("Whether the menu has a tearoff item"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE));

  /*
   * GtkTreeMenu:wrap-width:
   *
   * If wrap-width is set to a positive value, the list will be
   * displayed in multiple columns, the number of columns is
   * determined by wrap-width.
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_WRAP_WIDTH,
                                   g_param_spec_int ("wrap-width",
                                                     P_("Wrap Width"),
                                                     P_("Wrap width for laying out items in a grid"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     GTK_PARAM_READWRITE));

  /*
   * GtkTreeMenu:row-span-column:
   *
   * If this is set to a non-negative value, it must be the index of a column
   * of type %G_TYPE_INT in the model.
   *
   * The values of that column are used to determine how many rows a value in
   * the list will span. Therefore, the values in the model column pointed to
   * by this property must be greater than zero and not larger than wrap-width.
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_ROW_SPAN_COL,
                                   g_param_spec_int ("row-span-column",
                                                     P_("Row span column"),
                                                     P_("TreeModel column containing the row span values"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     GTK_PARAM_READWRITE));

  /*
   * GtkTreeMenu:column-span-column:
   *
   * If this is set to a non-negative value, it must be the index of a column
   * of type %G_TYPE_INT in the model.
   *
   * The values of that column are used to determine how many columns a value
   * in the list will span.
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_COL_SPAN_COL,
                                   g_param_spec_int ("column-span-column",
                                                     P_("Column span column"),
                                                     P_("TreeModel column containing the column span values"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
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

  object = G_OBJECT_CLASS (_gtk_tree_menu_parent_class)->constructor
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
  _gtk_tree_menu_set_header_func (menu, NULL, NULL, NULL);

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

    case PROP_TEAROFF:
      _gtk_tree_menu_set_tearoff (menu, g_value_get_boolean (value));
      break;

    case PROP_WRAP_WIDTH:
      _gtk_tree_menu_set_wrap_width (menu, g_value_get_int (value));
      break;

     case PROP_ROW_SPAN_COL:
      _gtk_tree_menu_set_row_span_column (menu, g_value_get_int (value));
      break;

     case PROP_COL_SPAN_COL:
      _gtk_tree_menu_set_column_span_column (menu, g_value_get_int (value));
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

  GTK_WIDGET_CLASS (_gtk_tree_menu_parent_class)->get_preferred_width (widget, minimum_size, natural_size);

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

  GTK_WIDGET_CLASS (_gtk_tree_menu_parent_class)->get_preferred_height (widget, minimum_size, natural_size);

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
      else if (!GTK_IS_TEAROFF_MENU_ITEM (child))
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
      if (!GTK_IS_SEPARATOR_MENU_ITEM (child) && !GTK_IS_TEAROFF_MENU_ITEM (child))
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
      if (priv->wrap_width > 0)
        rebuild_menu (menu);
      else
        {
          /* Get the index of the path for this depth */
          indices = gtk_tree_path_get_indices (path);
          depth   = gtk_tree_path_get_depth (path);
          index   = indices[depth -1];

          /* Menus with a header include a menuitem for its root node
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
          gtk_cell_area_context_reset (menu->priv->context);
        }
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
  GtkTreeMenuPrivate *priv = menu->priv;
  GtkWidget          *item;

  /* If it's the header item we leave it to the parent menu
   * to remove us from its menu
   */
  item = gtk_tree_menu_get_path_item (menu, path);

  if (item)
    {
      if (priv->wrap_width > 0)
        rebuild_menu (menu);
      else
        {
          /* Get rid of the deleted item */
          gtk_widget_destroy (item);

          /* Resize everything */
          gtk_cell_area_context_reset (menu->priv->context);
        }
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
  GList *children, *l;
  gint   position;

  children = gtk_container_get_children (GTK_CONTAINER (menu));
  for (position = 0, l = children; l; position++, l = l->next)
    {
      GtkWidget *iitem = l->data;

      if (item == iitem)
        break;
    }

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
  gboolean            has_header = FALSE;
  GtkWidget          *item;

  item = gtk_tree_menu_get_path_item (menu, path);

  if (priv->root)
    {
      GtkTreePath *root_path =
        gtk_tree_row_reference_get_path (priv->root);

      if (root_path && gtk_tree_path_compare (root_path, path) == 0)
        {
          if (priv->header_func)
            has_header =
              priv->header_func (priv->model, iter, priv->header_data);

          if (has_header && !item)
            {
              item = gtk_separator_menu_item_new ();
              gtk_widget_show (item);
              gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);

              item = gtk_tree_menu_create_item (menu, iter, TRUE);
              gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), item);

              priv->menu_with_header = TRUE;
            }
          else if (!has_header && item)
            {
              /* Destroy the header item and then the following separator */
              gtk_widget_destroy (item);
              gtk_widget_destroy (GTK_MENU_SHELL (menu)->priv->children->data);

              priv->menu_with_header = FALSE;
            }

          gtk_tree_path_free (root_path);
        }
    }

  if (item)
    {
      if (priv->wrap_width > 0)
        /* Ugly, we need to rebuild the menu here if
         * the row-span/row-column values change
         */
        rebuild_menu (menu);
      else
        {
          if (priv->row_separator_func)
            is_separator =
              priv->row_separator_func (model, iter,
                                        priv->row_separator_data);


          if (is_separator != GTK_IS_SEPARATOR_MENU_ITEM (item))
            {
              gint position = menu_item_position (menu, item);

              gtk_widget_destroy (item);
              item = gtk_tree_menu_create_item (menu, iter, FALSE);
              gtk_menu_shell_insert (GTK_MENU_SHELL (menu), item, position);
            }
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
              if (GTK_MENU_SHELL (menu)->priv->children &&
                  GTK_MENU_SHELL (menu)->priv->children->next)
                {
                  GtkWidget *separator =
                    GTK_MENU_SHELL (menu)->priv->children->next->data;

                  gtk_widget_set_sensitive (separator, sensitive);
                }
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

static gboolean
menu_occupied (GtkTreeMenu *menu,
               guint        left_attach,
               guint        right_attach,
               guint        top_attach,
               guint        bottom_attach)
{
  GList *i;

  for (i = GTK_MENU_SHELL (menu)->priv->children; i; i = i->next)
    {
      guint l, r, b, t;

      gtk_container_child_get (GTK_CONTAINER (menu),
                               i->data,
                               "left-attach", &l,
                               "right-attach", &r,
                               "bottom-attach", &b,
                               "top-attach", &t,
                               NULL);

      /* look if this item intersects with the given coordinates */
      if (right_attach > l && left_attach < r && bottom_attach > t && top_attach < b)
        return TRUE;
    }

  return FALSE;
}

static void
relayout_item (GtkTreeMenu *menu,
               GtkWidget   *item,
               GtkTreeIter *iter,
               GtkWidget   *prev)
{
  GtkTreeMenuPrivate *priv = menu->priv;
  gint                current_col = 0, current_row = 0;
  gint                rows = 1, cols = 1;

  if (priv->col_span_col == -1 &&
      priv->row_span_col == -1 &&
      prev)
    {
      gtk_container_child_get (GTK_CONTAINER (menu), prev,
                               "right-attach", &current_col,
                               "top-attach", &current_row,
                               NULL);
      if (current_col + cols > priv->wrap_width)
        {
          current_col = 0;
          current_row++;
        }
    }
  else
    {
      if (priv->col_span_col != -1)
        gtk_tree_model_get (priv->model, iter,
                            priv->col_span_col, &cols,
                            -1);
      if (priv->row_span_col != -1)
        gtk_tree_model_get (priv->model, iter,
                            priv->row_span_col, &rows,
                            -1);

      while (1)
        {
          if (current_col + cols > priv->wrap_width)
            {
              current_col = 0;
              current_row++;
            }

          if (!menu_occupied (menu,
                              current_col, current_col + cols,
                              current_row, current_row + rows))
            break;

          current_col++;
        }
    }

  /* set attach props */
  gtk_menu_attach (GTK_MENU (menu), item,
                   current_col, current_col + cols,
                   current_row, current_row + rows);
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
  _gtk_tree_menu_set_header_func (GTK_TREE_MENU (submenu),
                                  priv->header_func,
                                  priv->header_data,
                                  priv->header_destroy);

  _gtk_tree_menu_set_wrap_width (GTK_TREE_MENU (submenu), priv->wrap_width);
  _gtk_tree_menu_set_row_span_column (GTK_TREE_MENU (submenu), priv->row_span_col);
  _gtk_tree_menu_set_column_span_column (GTK_TREE_MENU (submenu), priv->col_span_col);

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
      gtk_widget_show (item);

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
  GtkWidget          *menu_item, *prev = NULL;

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

              prev = menu_item;
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

          if (priv->wrap_width > 0)
            gtk_menu_attach (GTK_MENU (menu), menu_item, 0, priv->wrap_width, 0, 1);
          else
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

          prev = menu_item;
        }

      valid = gtk_tree_model_iter_children (priv->model, &iter, NULL);
    }

  /* Create a menu item for every row at the current depth, add a GtkTreeMenu
   * submenu for iters/items that have children */
  while (valid)
    {
      menu_item = gtk_tree_menu_create_item (menu, &iter, FALSE);

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

      if (priv->wrap_width > 0)
        relayout_item (menu, menu_item, &iter, prev);

      prev  = menu_item;
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
 * Return value: A newly created #GtkTreeMenu with no model or root.
 *
 * Since: 3.0
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
 * Return value: A newly created #GtkTreeMenu with no model or root.
 *
 * Since: 3.0
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
 * @model: (allow-none): the #GtkTreeModel to build the menu heirarchy from, or %NULL.
 * @root: (allow-none): the #GtkTreePath indicating the root row for this menu, or %NULL.
 *
 * Creates a new #GtkTreeMenu hierarchy from the provided @model and @root using @area to render its cells.
 *
 * Return value: A newly created #GtkTreeMenu.
 *
 * Since: 3.0
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
 * Sets @model to be used to build the menu heirarhcy.
 *
 * Since: 3.0
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
 * Gets the @model currently used for the menu heirarhcy.
 *
 * Return value: (transfer none): the #GtkTreeModel which is used
 * for @menu's hierarchy.
 *
 * Since: 3.0
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
 * Sets the root of a @menu's hierarchy to be @path. @menu must already
 * have a model set and @path must point to a valid path inside the model.
 *
 * Since: 3.0
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
 * Gets the @root path for @menu's hierarchy, or returns %NULL if @menu
 * has no model or is building a heirarchy for the entire model. *
 *
 * Return value: (transfer full) (allow-none): A newly created #GtkTreePath
 * pointing to the root of @menu which must be freed with gtk_tree_path_free().
 *
 * Since: 3.0
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
 * _gtk_tree_menu_get_tearoff:
 * @menu: a #GtkTreeMenu
 *
 * Gets whether this menu is build with a leading tearoff menu item.
 *
 * Return value: %TRUE if the menu has a tearoff item.
 *
 * Since: 3.0
 */
gboolean
_gtk_tree_menu_get_tearoff (GtkTreeMenu *menu)
{
  GtkTreeMenuPrivate *priv;

  g_return_val_if_fail (GTK_IS_TREE_MENU (menu), FALSE);

  priv = menu->priv;

  return priv->tearoff;
}

/*
 * _gtk_tree_menu_set_tearoff:
 * @menu: a #GtkTreeMenu
 * @tearoff: whether the menu should have a leading tearoff menu item.
 *
 * Sets whether this menu has a leading tearoff menu item.
 *
 * Since: 3.0
 */
void
_gtk_tree_menu_set_tearoff (GtkTreeMenu *menu,
                            gboolean     tearoff)
{
  GtkTreeMenuPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_MENU (menu));

  priv = menu->priv;

  if (priv->tearoff != tearoff)
    {
      priv->tearoff = tearoff;

      rebuild_menu (menu);

      g_object_notify (G_OBJECT (menu), "tearoff");
    }
}

/*
 * _gtk_tree_menu_get_wrap_width:
 * @menu: a #GtkTreeMenu
 *
 * Gets the wrap width which is used to determine the number of columns
 * for @menu. If the wrap width is larger than 1, @menu is in table mode.
 *
 * Return value: the wrap width.
 *
 * Since: 3.0
 */
gint
_gtk_tree_menu_get_wrap_width (GtkTreeMenu *menu)
{
  GtkTreeMenuPrivate *priv;

  g_return_val_if_fail (GTK_IS_TREE_MENU (menu), FALSE);

  priv = menu->priv;

  return priv->wrap_width;
}

/*
 * _gtk_tree_menu_set_wrap_width:
 * @menu: a #GtkTreeMenu
 * @width: the wrap width
 *
 * Sets the wrap width which is used to determine the number of columns
 * for @menu. If the wrap width is larger than 1, @menu is in table mode.
 *
 * Since: 3.0
 */
void
_gtk_tree_menu_set_wrap_width (GtkTreeMenu *menu,
                               gint         width)
{
  GtkTreeMenuPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_MENU (menu));
  g_return_if_fail (width >= 0);

  priv = menu->priv;

  if (priv->wrap_width != width)
    {
      priv->wrap_width = width;

      rebuild_menu (menu);

      g_object_notify (G_OBJECT (menu), "wrap-width");
    }
}

/*
 * _gtk_tree_menu_get_row_span_column:
 * @menu: a #GtkTreeMenu
 *
 * Gets the column with row span information for @menu.
 * The row span column contains integers which indicate how many rows
 * a menu item should span.
 *
 * Return value: the column in @menu's model containing row span information, or -1.
 *
 * Since: 3.0
 */
gint
_gtk_tree_menu_get_row_span_column (GtkTreeMenu *menu)
{
  GtkTreeMenuPrivate *priv;

  g_return_val_if_fail (GTK_IS_TREE_MENU (menu), FALSE);

  priv = menu->priv;

  return priv->row_span_col;
}

/*
 * _gtk_tree_menu_set_row_span_column:
 * @menu: a #GtkTreeMenu
 * @row_span: the column in the model to fetch the row span for a given menu item.
 *
 * Sets the column with row span information for @menu to be @row_span.
 * The row span column contains integers which indicate how many rows
 * a menu item should span.
 *
 * Since: 3.0
 */
void
_gtk_tree_menu_set_row_span_column (GtkTreeMenu *menu,
                                    gint         row_span)
{
  GtkTreeMenuPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_MENU (menu));

  priv = menu->priv;

  if (priv->row_span_col != row_span)
    {
      priv->row_span_col = row_span;

      if (priv->wrap_width > 0)
        rebuild_menu (menu);

      g_object_notify (G_OBJECT (menu), "row-span-column");
    }
}

/*
 * _gtk_tree_menu_get_column_span_column:
 * @menu: a #GtkTreeMenu
 *
 * Gets the column with column span information for @menu.
 * The column span column contains integers which indicate how many columns
 * a menu item should span.
 *
 * Return value: the column in @menu's model containing column span information, or -1.
 *
 * Since: 3.0
 */
gint
_gtk_tree_menu_get_column_span_column (GtkTreeMenu *menu)
{
  GtkTreeMenuPrivate *priv;

  g_return_val_if_fail (GTK_IS_TREE_MENU (menu), FALSE);

  priv = menu->priv;

  return priv->col_span_col;
}

/*
 * _gtk_tree_menu_set_column_span_column:
 * @menu: a #GtkTreeMenu
 * @column_span: the column in the model to fetch the column span for a given menu item.
 *
 * Sets the column with column span information for @menu to be @column_span.
 * The column span column contains integers which indicate how many columns
 * a menu item should span.
 *
 * Since: 3.0
 */
void
_gtk_tree_menu_set_column_span_column (GtkTreeMenu *menu,
                                       gint         column_span)
{
  GtkTreeMenuPrivate *priv;

  g_return_if_fail (GTK_IS_TREE_MENU (menu));

  priv = menu->priv;

  if (priv->col_span_col != column_span)
    {
      priv->col_span_col = column_span;

      if (priv->wrap_width > 0)
        rebuild_menu (menu);

      g_object_notify (G_OBJECT (menu), "column-span-column");
    }
}

/*
 * _gtk_tree_menu_get_row_separator_func:
 * @menu: a #GtkTreeMenu
 *
 * Gets the current #GtkTreeViewRowSeparatorFunc separator function.
 *
 * Return value: the current row separator function.
 *
 * Since: 3.0
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
 *
 * Since: 3.0
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

/*
 * _gtk_tree_menu_get_header_func:
 * @menu: a #GtkTreeMenu
 *
 * Gets the current #GtkTreeMenuHeaderFunc header function.
 *
 * Return value: the current header function.
 *
 * Since: 3.0
 */
GtkTreeMenuHeaderFunc
_gtk_tree_menu_get_header_func (GtkTreeMenu *menu)
{
  GtkTreeMenuPrivate *priv;

  g_return_val_if_fail (GTK_IS_TREE_MENU (menu), NULL);

  priv = menu->priv;

  return priv->header_func;
}

/*
 * _gtk_tree_menu_set_header_func:
 * @menu: a #GtkTreeMenu
 * @func: (allow-none): a #GtkTreeMenuHeaderFunc, or %NULL to unset the header function.
 * @data: (allow-none): user data to pass to @func, or %NULL
 * @destroy: (allow-none): destroy notifier for @data, or %NULL
 *
 * Sets the header function, which is used to determine
 * whether a row width children should contain a leading header
 * menu item to allow that row to be selectable as an independant
 * menu item. If the header function is %NULL, no rows with children
 * have menu items which can be activated as leafs.
 * This is the default value.
 *
 * Since: 3.0
 */
void
_gtk_tree_menu_set_header_func (GtkTreeMenu          *menu,
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

  rebuild_menu (menu);
}
