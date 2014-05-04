/* GTK+ - accessibility implementations
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

#include "gtktreeprivate.h"
#include "gtkwidgetprivate.h"

#include "gtktreeviewaccessibleprivate.h"

#include "gtkrenderercellaccessible.h"
#include "gtkbooleancellaccessible.h"
#include "gtkimagecellaccessible.h"
#include "gtkcontainercellaccessible.h"
#include "gtktextcellaccessible.h"
#include "gtkcellaccessibleparent.h"
#include "gtkcellaccessibleprivate.h"

struct _GtkTreeViewAccessiblePrivate
{
  GHashTable *cell_infos;
};

typedef struct _GtkTreeViewAccessibleCellInfo  GtkTreeViewAccessibleCellInfo;
struct _GtkTreeViewAccessibleCellInfo
{
  GtkCellAccessible *cell;
  GtkRBTree *tree;
  GtkRBNode *node;
  GtkTreeViewColumn *cell_col_ref;
  GtkTreeViewAccessible *view;
};

/* Misc */

static int              cell_info_get_index             (GtkTreeView                     *tree_view,
                                                         GtkTreeViewAccessibleCellInfo   *info);
static gboolean         is_cell_showing                 (GtkTreeView            *tree_view,
                                                         GdkRectangle           *cell_rect);

static void             cell_info_new                   (GtkTreeViewAccessible  *accessible,
                                                         GtkRBTree              *tree,
                                                         GtkRBNode              *node,
                                                         GtkTreeViewColumn      *tv_col,
                                                         GtkCellAccessible      *cell);
static gint             get_column_number               (GtkTreeView            *tree_view,
                                                         GtkTreeViewColumn      *column);

static gboolean         get_rbtree_column_from_index    (GtkTreeView            *tree_view,
                                                         gint                   index,
                                                         GtkRBTree              **tree,
                                                         GtkRBNode              **node,
                                                         GtkTreeViewColumn      **column);

static GtkTreeViewAccessibleCellInfo* find_cell_info    (GtkTreeViewAccessible           *view,
                                                         GtkCellAccessible               *cell);
static AtkObject *       get_header_from_column         (GtkTreeViewColumn      *tv_col);


static void atk_table_interface_init                  (AtkTableIface                *iface);
static void atk_selection_interface_init              (AtkSelectionIface            *iface);
static void atk_component_interface_init              (AtkComponentIface            *iface);
static void gtk_cell_accessible_parent_interface_init (GtkCellAccessibleParentIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkTreeViewAccessible, gtk_tree_view_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE,
                         G_ADD_PRIVATE (GtkTreeViewAccessible)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TABLE, atk_table_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_SELECTION, atk_selection_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, atk_component_interface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_ACCESSIBLE_PARENT, gtk_cell_accessible_parent_interface_init))


static GQuark
gtk_tree_view_accessible_get_data_quark (void)
{
  static GQuark quark = 0;

  if (G_UNLIKELY (quark == 0))
    quark = g_quark_from_static_string ("gtk-tree-view-accessible-data");

  return quark;
}

static void
cell_info_free (GtkTreeViewAccessibleCellInfo *cell_info)
{
  gtk_accessible_set_widget (GTK_ACCESSIBLE (cell_info->cell), NULL);
  g_object_unref (cell_info->cell);

  g_free (cell_info);
}

static GtkTreePath *
cell_info_get_path (GtkTreeViewAccessibleCellInfo *cell_info)
{
  return _gtk_tree_path_new_from_rbtree (cell_info->tree, cell_info->node);
}

static guint
cell_info_hash (gconstpointer info)
{
  const GtkTreeViewAccessibleCellInfo *cell_info = info;
  guint node, col;

  node = GPOINTER_TO_UINT (cell_info->node);
  col = GPOINTER_TO_UINT (cell_info->cell_col_ref);

  return ((node << sizeof (guint) / 2) | (node >> sizeof (guint) / 2)) ^ col;
}

static gboolean
cell_info_equal (gconstpointer a, gconstpointer b)
{
  const GtkTreeViewAccessibleCellInfo *cell_info_a = a;
  const GtkTreeViewAccessibleCellInfo *cell_info_b = b;

  return cell_info_a->node == cell_info_b->node &&
         cell_info_a->cell_col_ref == cell_info_b->cell_col_ref;
}

static void
gtk_tree_view_accessible_initialize (AtkObject *obj,
                                     gpointer   data)
{
  GtkTreeViewAccessible *accessible;
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  GtkWidget *widget;

  ATK_OBJECT_CLASS (gtk_tree_view_accessible_parent_class)->initialize (obj, data);

  accessible = GTK_TREE_VIEW_ACCESSIBLE (obj);

  accessible->priv->cell_infos = g_hash_table_new_full (cell_info_hash,
      cell_info_equal, NULL, (GDestroyNotify) cell_info_free);

  widget = GTK_WIDGET (data);
  tree_view = GTK_TREE_VIEW (widget);
  tree_model = gtk_tree_view_get_model (tree_view);

  if (tree_model)
    {
      if (gtk_tree_model_get_flags (tree_model) & GTK_TREE_MODEL_LIST_ONLY)
        obj->role = ATK_ROLE_TABLE;
      else
        obj->role = ATK_ROLE_TREE_TABLE;
    }
}

static void
gtk_tree_view_accessible_finalize (GObject *object)
{
  GtkTreeViewAccessible *accessible = GTK_TREE_VIEW_ACCESSIBLE (object);

  if (accessible->priv->cell_infos)
    g_hash_table_destroy (accessible->priv->cell_infos);

  G_OBJECT_CLASS (gtk_tree_view_accessible_parent_class)->finalize (object);
}

static void
gtk_tree_view_accessible_notify_gtk (GObject    *obj,
                                     GParamSpec *pspec)
{
  GtkWidget *widget;
  GtkTreeView *tree_view;
  GtkTreeViewAccessible *accessible;

  widget = GTK_WIDGET (obj);
  accessible = GTK_TREE_VIEW_ACCESSIBLE (gtk_widget_get_accessible (widget));
  tree_view = GTK_TREE_VIEW (widget);

  if (g_strcmp0 (pspec->name, "model") == 0)
    {
      GtkTreeModel *tree_model;
      AtkRole role;

      tree_model = gtk_tree_view_get_model (tree_view);
      g_hash_table_remove_all (accessible->priv->cell_infos);

      if (tree_model)
        {
          if (gtk_tree_model_get_flags (tree_model) & GTK_TREE_MODEL_LIST_ONLY)
            role = ATK_ROLE_TABLE;
          else
            role = ATK_ROLE_TREE_TABLE;
        }
      else
        {
          role = ATK_ROLE_UNKNOWN;
        }
      atk_object_set_role (ATK_OBJECT (accessible), role);
      g_object_freeze_notify (G_OBJECT (accessible));
      g_signal_emit_by_name (accessible, "model-changed");
      g_signal_emit_by_name (accessible, "visible-data-changed");
      g_object_thaw_notify (G_OBJECT (accessible));
    }
  else
    GTK_WIDGET_ACCESSIBLE_CLASS (gtk_tree_view_accessible_parent_class)->notify_gtk (obj, pspec);
}

static void
gtk_tree_view_accessible_widget_unset (GtkAccessible *gtkaccessible)
{
  GtkTreeViewAccessible *accessible = GTK_TREE_VIEW_ACCESSIBLE (gtkaccessible);

  g_hash_table_remove_all (accessible->priv->cell_infos);

  GTK_ACCESSIBLE_CLASS (gtk_tree_view_accessible_parent_class)->widget_unset (gtkaccessible);
}

static gint
get_n_rows (GtkTreeView *tree_view)
{
  GtkRBTree *tree;

  tree = _gtk_tree_view_get_rbtree (tree_view);

  if (tree == NULL)
    return 0;

  return tree->root->total_count;
}

static gint
get_n_columns (GtkTreeView *tree_view)
{
  guint i, visible_columns;

  visible_columns = 0;

  for (i = 0; i < gtk_tree_view_get_n_columns (tree_view); i++)
    {
      GtkTreeViewColumn *column = gtk_tree_view_get_column (tree_view, i);

      if (gtk_tree_view_column_get_visible (column))
        visible_columns++;
    }

  return visible_columns;
}

static gint
gtk_tree_view_accessible_get_n_children (AtkObject *obj)
{
  GtkWidget *widget;
  GtkTreeView *tree_view;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return 0;

  tree_view = GTK_TREE_VIEW (widget);
  return (get_n_rows (tree_view) + 1) * get_n_columns (tree_view);
}

static GtkTreeViewColumn *
get_visible_column (GtkTreeView *tree_view,
                    guint        id)
{
  guint i;

  for (i = 0; i < gtk_tree_view_get_n_columns (tree_view); i++)
    {
      GtkTreeViewColumn *column = gtk_tree_view_get_column (tree_view, i);

      if (!gtk_tree_view_column_get_visible (column))
        continue;

      if (id == 0)
        return column;

      id--;
    }

  g_return_val_if_reached (NULL);
}

static void
set_cell_data (GtkTreeView           *treeview,
               GtkTreeViewAccessible *accessible,
               GtkCellAccessible     *cell)
{
  GtkTreeViewAccessibleCellInfo *cell_info;
  gboolean is_expander, is_expanded;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;

  cell_info = find_cell_info (accessible, cell);
  if (!cell_info)
    return;

  model = gtk_tree_view_get_model (treeview);

  if (GTK_RBNODE_FLAG_SET (cell_info->node, GTK_RBNODE_IS_PARENT) &&
      cell_info->cell_col_ref == gtk_tree_view_get_expander_column (treeview))
    {
      is_expander = TRUE;
      is_expanded = cell_info->node->children != NULL;
    }
  else
    {
      is_expander = FALSE;
      is_expanded = FALSE;
    }

  path = cell_info_get_path (cell_info);
  if (path == NULL ||
      !gtk_tree_model_get_iter (model, &iter, path))
    {
      /* We only track valid cells, this should never happen */
      g_return_if_reached ();
    }
  gtk_tree_path_free (path);

  gtk_tree_view_column_cell_set_cell_data (cell_info->cell_col_ref,
                                           model,
                                           &iter,
                                           is_expander,
                                           is_expanded);
}

static GtkCellAccessible *
peek_cell (GtkTreeViewAccessible *accessible,
           GtkRBTree             *tree,
           GtkRBNode             *node,
           GtkTreeViewColumn     *column)
{
  GtkTreeViewAccessibleCellInfo lookup, *cell_info;

  lookup.tree = tree;
  lookup.node = node;
  lookup.cell_col_ref = column;

  cell_info = g_hash_table_lookup (accessible->priv->cell_infos, &lookup);
  if (cell_info == NULL)
    return NULL;

  return cell_info->cell;
}

static GtkCellAccessible *
create_cell_accessible_for_renderer (GtkCellRenderer *renderer,
                                     GtkWidget       *widget,
                                     AtkObject       *parent)
{
  GtkCellAccessible *cell;

  cell = GTK_CELL_ACCESSIBLE (gtk_renderer_cell_accessible_new (renderer));
  
  _gtk_cell_accessible_initialize (cell, widget, parent);

  return cell;
}

static GtkCellAccessible *
create_cell_accessible (GtkTreeView           *treeview,
                        GtkTreeViewAccessible *accessible,
                        GtkTreeViewColumn     *column)
{
  GList *renderer_list;
  GList *l;
  GtkCellAccessible *cell;

  renderer_list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));

  /* If there is exactly one renderer in the list (which is a 
   * common case), shortcut and don't make a container
   */
  if (g_list_length (renderer_list) == 1)
    {
      cell = create_cell_accessible_for_renderer (renderer_list->data, GTK_WIDGET (treeview), ATK_OBJECT (accessible));
    }
  else
    {
      GtkContainerCellAccessible *container;

      container = gtk_container_cell_accessible_new ();
      _gtk_cell_accessible_initialize (GTK_CELL_ACCESSIBLE (container), GTK_WIDGET (treeview), ATK_OBJECT (accessible));

      for (l = renderer_list; l; l = l->next)
        {
          cell = create_cell_accessible_for_renderer (l->data, GTK_WIDGET (treeview), ATK_OBJECT (container));
          gtk_container_cell_accessible_add_child (container, cell);
        }

      cell = GTK_CELL_ACCESSIBLE (container);
    }

  g_list_free (renderer_list);

  return cell;
}
                        
static GtkCellAccessible *
create_cell (GtkTreeView           *treeview,
             GtkTreeViewAccessible *accessible,
             GtkRBTree             *tree,
             GtkRBNode             *node,
             GtkTreeViewColumn     *column)
{
  GtkCellAccessible *cell;

  cell = create_cell_accessible (treeview, accessible, column);
  cell_info_new (accessible, tree, node, column, cell);

  set_cell_data (treeview, accessible, cell);
  _gtk_cell_accessible_update_cache (cell);

  return cell;
}

static AtkObject *
gtk_tree_view_accessible_ref_child (AtkObject *obj,
                                    gint       i)
{
  GtkWidget *widget;
  GtkTreeViewAccessible *accessible;
  GtkCellAccessible *cell;
  GtkTreeView *tree_view;
  GtkTreeViewColumn *tv_col;
  GtkRBTree *tree;
  GtkRBNode *node;
  AtkObject *child;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  if (i >= gtk_tree_view_accessible_get_n_children (obj))
    return NULL;

  accessible = GTK_TREE_VIEW_ACCESSIBLE (obj);
  tree_view = GTK_TREE_VIEW (widget);
  if (i < get_n_columns (tree_view))
    {
      tv_col = get_visible_column (tree_view, i);
      child = get_header_from_column (tv_col);
      if (child)
        g_object_ref (child);
      return child;
    }

  /* Find the RBTree and GtkTreeViewColumn for the index */
  if (!get_rbtree_column_from_index (tree_view, i, &tree, &node, &tv_col))
    return NULL;

  cell = peek_cell (accessible, tree, node, tv_col);
  if (cell == NULL)
    cell = create_cell (tree_view, accessible, tree, node, tv_col);

  return g_object_ref (cell);
}

static AtkStateSet*
gtk_tree_view_accessible_ref_state_set (AtkObject *obj)
{
  AtkStateSet *state_set;
  GtkWidget *widget;

  state_set = ATK_OBJECT_CLASS (gtk_tree_view_accessible_parent_class)->ref_state_set (obj);
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));

  if (widget != NULL)
    atk_state_set_add_state (state_set, ATK_STATE_MANAGES_DESCENDANTS);

  return state_set;
}

static void
gtk_tree_view_accessible_class_init (GtkTreeViewAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkAccessibleClass *accessible_class = (GtkAccessibleClass*)klass;
  GtkWidgetAccessibleClass *widget_class = (GtkWidgetAccessibleClass*)klass;
  GtkContainerAccessibleClass *container_class = (GtkContainerAccessibleClass*)klass;

  class->get_n_children = gtk_tree_view_accessible_get_n_children;
  class->ref_child = gtk_tree_view_accessible_ref_child;
  class->ref_state_set = gtk_tree_view_accessible_ref_state_set;
  class->initialize = gtk_tree_view_accessible_initialize;

  widget_class->notify_gtk = gtk_tree_view_accessible_notify_gtk;

  accessible_class->widget_unset = gtk_tree_view_accessible_widget_unset;

  /* The children of a GtkTreeView are the buttons at the top of the columns
   * we do not represent these as children so we do not want to report
   * children added or deleted when these changed.
   */
  container_class->add_gtk = NULL;
  container_class->remove_gtk = NULL;

  gobject_class->finalize = gtk_tree_view_accessible_finalize;
}

static void
gtk_tree_view_accessible_init (GtkTreeViewAccessible *view)
{
  view->priv = gtk_tree_view_accessible_get_instance_private (view);
}

/* atkcomponent.h */

static AtkObject *
gtk_tree_view_accessible_ref_accessible_at_point (AtkComponent *component,
                                                  gint          x,
                                                  gint          y,
                                                  AtkCoordType  coord_type)
{
  GtkWidget *widget;
  GtkTreeView *tree_view;
  GtkTreePath *path;
  GtkTreeViewColumn *column;
  gint x_pos, y_pos;
  gint bx, by;
  GtkCellAccessible *cell;
  GtkRBTree *tree;
  GtkRBNode *node;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (component));
  if (widget == NULL)
    return NULL;

  tree_view = GTK_TREE_VIEW (widget);

  atk_component_get_extents (component, &x_pos, &y_pos, NULL, NULL, coord_type);
  gtk_tree_view_convert_widget_to_bin_window_coords (tree_view, x, y, &bx, &by);
  if (!gtk_tree_view_get_path_at_pos (tree_view,
                                      bx - x_pos, by - y_pos,
                                      &path, &column, NULL, NULL))
    return NULL;

  if (_gtk_tree_view_find_node (tree_view, path, &tree, &node))
    {
      gtk_tree_path_free (path);
      return NULL;
    }

  cell = peek_cell (GTK_TREE_VIEW_ACCESSIBLE (component), tree, node, column);
  if (cell == NULL)
    cell = create_cell (tree_view, GTK_TREE_VIEW_ACCESSIBLE (component), tree, node, column);

  return g_object_ref (cell);
}

static void
atk_component_interface_init (AtkComponentIface *iface)
{
  iface->ref_accessible_at_point = gtk_tree_view_accessible_ref_accessible_at_point;
}

/* atktable.h */

static gint
gtk_tree_view_accessible_get_index_at (AtkTable *table,
                                       gint      row,
                                       gint      column)
{
  GtkWidget *widget;
  gint n_cols, n_rows;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (table));
  if (widget == NULL)
    return -1;

  n_cols = atk_table_get_n_columns (table);
  n_rows = atk_table_get_n_rows (table);

  if (row >= n_rows || column >= n_cols)
    return -1;

  return (row + 1) * n_cols + column;
}

static gint
gtk_tree_view_accessible_get_column_at_index (AtkTable *table,
                                              gint      index)
{
  GtkWidget *widget;
  gint n_columns;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (table));
  if (widget == NULL)
    return -1;

  if (index >= gtk_tree_view_accessible_get_n_children (ATK_OBJECT (table)))
    return -1;

  n_columns = get_n_columns (GTK_TREE_VIEW (widget));

  /* checked by the n_children() check above */
  g_assert (n_columns > 0);

  return index % n_columns;
}

static gint
gtk_tree_view_accessible_get_row_at_index (AtkTable *table,
                                           gint      index)
{
  GtkWidget *widget;
  GtkTreeView *tree_view;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (table));
  if (widget == NULL)
    return -1;

  tree_view = GTK_TREE_VIEW (widget);

  index /= get_n_columns (tree_view);
  index--;
  if (index >= get_n_rows (tree_view))
    return -1;

  return index;
}

static AtkObject *
gtk_tree_view_accessible_table_ref_at (AtkTable *table,
                                       gint      row,
                                       gint      column)
{
  gint index;

  index = gtk_tree_view_accessible_get_index_at (table, row, column);
  if (index == -1)
    return NULL;

  return gtk_tree_view_accessible_ref_child (ATK_OBJECT (table), index);
}

static gint
gtk_tree_view_accessible_get_n_rows (AtkTable *table)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (table));
  if (widget == NULL)
    return 0;

  return get_n_rows (GTK_TREE_VIEW (widget));
}

static gint
gtk_tree_view_accessible_get_n_columns (AtkTable *table)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (table));
  if (widget == NULL)
    return 0;

  return get_n_columns (GTK_TREE_VIEW (widget));
}

static gboolean
gtk_tree_view_accessible_is_row_selected (AtkTable *table,
                                          gint      row)
{
  GtkWidget *widget;
  GtkRBTree *tree;
  GtkRBNode *node;

  if (row < 0)
    return FALSE;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (table));
  if (widget == NULL)
    return FALSE;

  if (!_gtk_rbtree_find_index (_gtk_tree_view_get_rbtree (GTK_TREE_VIEW (widget)),
                               row,
                               &tree,
                               &node))
    return FALSE;

  return GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED);
}

static gboolean
gtk_tree_view_accessible_is_selected (AtkTable *table,
                                      gint      row,
                                      gint      column)
{
  return gtk_tree_view_accessible_is_row_selected (table, row);
}

typedef struct {
  GArray *array;
  GtkTreeView *treeview;
} SelectedRowsData;

static void
get_selected_rows (GtkTreeModel *model,
                   GtkTreePath  *path,
                   GtkTreeIter  *iter,
                   gpointer      datap)
{
  SelectedRowsData *data = datap;
  GtkRBTree *tree;
  GtkRBNode *node;
  int id;

  if (_gtk_tree_view_find_node (data->treeview,
                                path,
                                &tree, &node))
    {
      g_assert_not_reached ();
    }

  id = _gtk_rbtree_node_get_index (tree, node);

  g_array_append_val (data->array, id);
}

static gint
gtk_tree_view_accessible_get_selected_rows (AtkTable  *table,
                                            gint     **rows_selected)
{
  SelectedRowsData data;
  GtkWidget *widget;
  gint n_rows;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (table));
  if (widget == NULL)
    {
      if (rows_selected != NULL)
        *rows_selected = NULL;
      return 0;
    }

  data.treeview = GTK_TREE_VIEW (widget);
  data.array = g_array_new (FALSE, FALSE, sizeof (gint));

  gtk_tree_selection_selected_foreach (gtk_tree_view_get_selection (data.treeview),
                                       get_selected_rows,
                                       &data);

  n_rows = data.array->len;
  if (rows_selected)
    *rows_selected = (gint *) g_array_free (data.array, FALSE);
  else
    g_array_free (data.array, TRUE);
  
  return n_rows;
}

static gboolean
gtk_tree_view_accessible_add_row_selection (AtkTable *table,
                                            gint      row)
{
  GtkTreeView *treeview;
  GtkTreePath *path;
  GtkRBTree *tree;
  GtkRBNode *node;

  if (row < 0)
    return FALSE;

  treeview = GTK_TREE_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (table)));
  if (treeview == NULL)
    return FALSE;

  if (!_gtk_rbtree_find_index (_gtk_tree_view_get_rbtree (treeview),
                               row,
                               &tree,
                               &node))
    return FALSE;

  if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
    return FALSE;

  path = _gtk_tree_path_new_from_rbtree (tree, node);
  gtk_tree_selection_select_path (gtk_tree_view_get_selection (treeview), path);
  gtk_tree_path_free (path);

  return TRUE;
}

static gboolean
gtk_tree_view_accessible_remove_row_selection (AtkTable *table,
                                               gint      row)
{
  GtkTreeView *treeview;
  GtkTreePath *path;
  GtkRBTree *tree;
  GtkRBNode *node;

  if (row < 0)
    return FALSE;

  treeview = GTK_TREE_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (table)));
  if (treeview == NULL)
    return FALSE;

  if (!_gtk_rbtree_find_index (_gtk_tree_view_get_rbtree (treeview),
                               row,
                               &tree,
                               &node))
    return FALSE;

  if (! GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
    return FALSE;

  path = _gtk_tree_path_new_from_rbtree (tree, node);
  gtk_tree_selection_unselect_path (gtk_tree_view_get_selection (treeview), path);
  gtk_tree_path_free (path);

  return TRUE;
}

static AtkObject *
gtk_tree_view_accessible_get_column_header (AtkTable *table,
                                            gint      in_col)
{
  GtkWidget *widget;
  GtkTreeView *tree_view;
  GtkTreeViewColumn *tv_col;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (table));
  if (widget == NULL)
    return NULL;

  tree_view = GTK_TREE_VIEW (widget);
  if (in_col < 0 || in_col >= get_n_columns (tree_view))
    return NULL;

  tv_col = get_visible_column (tree_view, in_col);
  return get_header_from_column (tv_col);
}

static const gchar *
gtk_tree_view_accessible_get_column_description (AtkTable *table,
                                                 gint      in_col)
{
  GtkWidget *widget;
  GtkTreeView *tree_view;
  GtkTreeViewColumn *tv_col;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (table));
  if (widget == NULL)
    return NULL;

  tree_view = GTK_TREE_VIEW (widget);
  if (in_col < 0 || in_col >= get_n_columns (tree_view))
    return NULL;

  tv_col = get_visible_column (tree_view, in_col);
  return gtk_tree_view_column_get_title (tv_col);
}

static void
atk_table_interface_init (AtkTableIface *iface)
{
  iface->ref_at = gtk_tree_view_accessible_table_ref_at;
  iface->get_n_rows = gtk_tree_view_accessible_get_n_rows;
  iface->get_n_columns = gtk_tree_view_accessible_get_n_columns;
  iface->get_index_at = gtk_tree_view_accessible_get_index_at;
  iface->get_column_at_index = gtk_tree_view_accessible_get_column_at_index;
  iface->get_row_at_index = gtk_tree_view_accessible_get_row_at_index;
  iface->is_row_selected = gtk_tree_view_accessible_is_row_selected;
  iface->is_selected = gtk_tree_view_accessible_is_selected;
  iface->get_selected_rows = gtk_tree_view_accessible_get_selected_rows;
  iface->add_row_selection = gtk_tree_view_accessible_add_row_selection;
  iface->remove_row_selection = gtk_tree_view_accessible_remove_row_selection;
  iface->get_column_extent_at = NULL;
  iface->get_row_extent_at = NULL;
  iface->get_column_header = gtk_tree_view_accessible_get_column_header;
  iface->get_column_description = gtk_tree_view_accessible_get_column_description;
}

/* atkselection.h */

static gboolean
gtk_tree_view_accessible_add_selection (AtkSelection *selection,
                                        gint          i)
{
  AtkTable *table;
  gint n_columns;
  gint row;

  table = ATK_TABLE (selection);
  n_columns = gtk_tree_view_accessible_get_n_columns (table);
  if (n_columns != 1)
    return FALSE;

  row = gtk_tree_view_accessible_get_row_at_index (table, i);
  return gtk_tree_view_accessible_add_row_selection (table, row);
}

static gboolean
gtk_tree_view_accessible_clear_selection (AtkSelection *selection)
{
  GtkWidget *widget;
  GtkTreeView *tree_view;
  GtkTreeSelection *tree_selection;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  tree_view = GTK_TREE_VIEW (widget);
  tree_selection = gtk_tree_view_get_selection (tree_view);

  gtk_tree_selection_unselect_all (tree_selection);
  return TRUE;
}

static AtkObject *
gtk_tree_view_accessible_ref_selection (AtkSelection *selection,
                                        gint          i)
{
  AtkTable *table;
  gint row;
  gint n_selected;
  gint n_columns;
  gint *selected;

  table = ATK_TABLE (selection);
  n_columns = gtk_tree_view_accessible_get_n_columns (table);
  n_selected = gtk_tree_view_accessible_get_selected_rows (table, &selected);
  if (n_columns == 0 || i >= n_columns * n_selected)
    return NULL;

  row = selected[i / n_columns];
  g_free (selected);

  return gtk_tree_view_accessible_table_ref_at (table, row, i % n_columns);
}

static gint
gtk_tree_view_accessible_get_selection_count (AtkSelection *selection)
{
  AtkTable *table;
  gint n_selected;

  table = ATK_TABLE (selection);
  n_selected = gtk_tree_view_accessible_get_selected_rows (table, NULL);
  if (n_selected > 0)
    n_selected *= gtk_tree_view_accessible_get_n_columns (table);
  return n_selected;
}

static gboolean
gtk_tree_view_accessible_is_child_selected (AtkSelection *selection,
                                            gint          i)
{
  GtkWidget *widget;
  gint row;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  row = gtk_tree_view_accessible_get_row_at_index (ATK_TABLE (selection), i);

  return gtk_tree_view_accessible_is_row_selected (ATK_TABLE (selection), row);
}

static void atk_selection_interface_init (AtkSelectionIface *iface)
{
  iface->add_selection = gtk_tree_view_accessible_add_selection;
  iface->clear_selection = gtk_tree_view_accessible_clear_selection;
  iface->ref_selection = gtk_tree_view_accessible_ref_selection;
  iface->get_selection_count = gtk_tree_view_accessible_get_selection_count;
  iface->is_child_selected = gtk_tree_view_accessible_is_child_selected;
}

#define EXTRA_EXPANDER_PADDING 4

static void
gtk_tree_view_accessible_get_cell_area (GtkCellAccessibleParent *parent,
                                        GtkCellAccessible       *cell,
                                        GdkRectangle            *cell_rect)
{
  GtkWidget *widget;
  GtkTreeView *tree_view;
  GtkTreeViewColumn *tv_col;
  GtkTreePath *path;
  AtkObject *parent_cell;
  GtkTreeViewAccessibleCellInfo *cell_info;
  GtkCellAccessible *top_cell;

  /* Default value. */
  cell_rect->x = 0;
  cell_rect->y = 0;
  cell_rect->width = 0;
  cell_rect->height = 0;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (parent));
  if (widget == NULL)
    return;

  tree_view = GTK_TREE_VIEW (widget);
  parent_cell = atk_object_get_parent (ATK_OBJECT (cell));
  if (parent_cell != ATK_OBJECT (parent))
    top_cell = GTK_CELL_ACCESSIBLE (parent_cell);
  else
    top_cell = cell;
  cell_info = find_cell_info (GTK_TREE_VIEW_ACCESSIBLE (parent), top_cell);
  if (!cell_info)
    return;
  path = cell_info_get_path (cell_info);
  tv_col = cell_info->cell_col_ref;
  if (path)
    {
      GtkTreeViewColumn *expander_column;

      gtk_tree_view_get_cell_area (tree_view, path, tv_col, cell_rect);
      expander_column = gtk_tree_view_get_expander_column (tree_view);
      if (expander_column == tv_col)
        {
          gint expander_size;
          gtk_widget_style_get (widget,
                                "expander-size", &expander_size,
                                NULL);
          cell_rect->x += expander_size + EXTRA_EXPANDER_PADDING;
          cell_rect->width -= expander_size + EXTRA_EXPANDER_PADDING;
        }

      gtk_tree_path_free (path);

      /* A column has more than one renderer so we find the position
       * and width of each
       */
      if (top_cell != cell)
        {
          gint cell_index;
          gboolean found;
          gint cell_start;
          gint cell_width;
          GList *renderers;
          GtkCellRenderer *renderer;

          cell_index = atk_object_get_index_in_parent (ATK_OBJECT (cell));
          renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (tv_col));
          renderer = g_list_nth_data (renderers, cell_index);

          found = gtk_tree_view_column_cell_get_position (tv_col, renderer, &cell_start, &cell_width);
          if (found)
            {
              cell_rect->x += cell_start;
              cell_rect->width = cell_width;
            }
          g_list_free (renderers);
        }

    }
}

static void
gtk_tree_view_accessible_get_cell_extents (GtkCellAccessibleParent *parent,
                                           GtkCellAccessible       *cell,
                                           gint                    *x,
                                           gint                    *y,
                                           gint                    *width,
                                           gint                    *height,
                                           AtkCoordType             coord_type)
{
  GtkWidget *widget;
  GtkTreeView *tree_view;
  GdkWindow *bin_window;
  GdkRectangle cell_rect;
  gint w_x, w_y;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (parent));
  if (widget == NULL)
    return;

  tree_view = GTK_TREE_VIEW (widget);
  gtk_tree_view_accessible_get_cell_area (parent, cell, &cell_rect);
  bin_window = gtk_tree_view_get_bin_window (tree_view);
  gdk_window_get_origin (bin_window, &w_x, &w_y);

  if (coord_type == ATK_XY_WINDOW)
    {
      GdkWindow *window;
      gint x_toplevel, y_toplevel;

      window = gdk_window_get_toplevel (bin_window);
      gdk_window_get_origin (window, &x_toplevel, &y_toplevel);

      w_x -= x_toplevel;
      w_y -= y_toplevel;
    }

  *width = cell_rect.width;
  *height = cell_rect.height;
  if (is_cell_showing (tree_view, &cell_rect))
    {
      *x = cell_rect.x + w_x;
      *y = cell_rect.y + w_y;
    }
  else
    {
      *x = G_MININT;
      *y = G_MININT;
    }
}

static gboolean
gtk_tree_view_accessible_grab_cell_focus (GtkCellAccessibleParent *parent,
                                          GtkCellAccessible       *cell)
{
  GtkWidget *widget;
  GtkTreeView *tree_view;
  GtkTreeViewColumn *tv_col;
  GtkTreePath *path;
  AtkObject *parent_cell;
  AtkObject *cell_object;
  GtkTreeViewAccessibleCellInfo *cell_info;
  GtkCellRenderer *renderer = NULL;
  GtkWidget *toplevel;
  gint index;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (parent));
  if (widget == NULL)
    return FALSE;

  tree_view = GTK_TREE_VIEW (widget);

  cell_info = find_cell_info (GTK_TREE_VIEW_ACCESSIBLE (parent), cell);
  if (!cell_info)
    return FALSE;
  cell_object = ATK_OBJECT (cell);
  parent_cell = atk_object_get_parent (cell_object);
  tv_col = cell_info->cell_col_ref;
  if (parent_cell != ATK_OBJECT (parent))
    {
      /* GtkCellAccessible is in a GtkContainerCellAccessible.
       * The GtkTreeViewColumn has multiple renderers;
       * find the corresponding one.
       */
      GList *renderers;

      renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (tv_col));
      index = atk_object_get_index_in_parent (cell_object);
      renderer = g_list_nth_data (renderers, index);
      g_list_free (renderers);
    }
  path = cell_info_get_path (cell_info);
  if (path)
    {
      if (renderer)
        gtk_tree_view_set_cursor_on_cell (tree_view, path, tv_col, renderer, FALSE);
      else
        gtk_tree_view_set_cursor (tree_view, path, tv_col, FALSE);

      gtk_tree_path_free (path);
      gtk_widget_grab_focus (widget);
      toplevel = gtk_widget_get_toplevel (widget);
      if (gtk_widget_is_toplevel (toplevel))
        {
#ifdef GDK_WINDOWING_X11
          gtk_window_present_with_time (GTK_WINDOW (toplevel),
                                        gdk_x11_get_server_time (gtk_widget_get_window (widget)));
#else
          gtk_window_present (GTK_WINDOW (toplevel));
#endif
        }

      return TRUE;
    }
  else
      return FALSE;
}

static int
gtk_tree_view_accessible_get_child_index (GtkCellAccessibleParent *parent,
                                          GtkCellAccessible       *cell)
{
  GtkTreeViewAccessibleCellInfo *cell_info;
  GtkTreeView *tree_view;

  cell_info = find_cell_info (GTK_TREE_VIEW_ACCESSIBLE (parent), cell);
  if (!cell_info)
    return -1;

  tree_view = GTK_TREE_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (parent)));

  return cell_info_get_index (tree_view, cell_info);
}

static GtkCellRendererState
gtk_tree_view_accessible_get_renderer_state (GtkCellAccessibleParent *parent,
                                             GtkCellAccessible       *cell)
{
  GtkTreeViewAccessibleCellInfo *cell_info;
  GtkTreeView *treeview;
  GtkCellRendererState flags;

  cell_info = find_cell_info (GTK_TREE_VIEW_ACCESSIBLE (parent), cell);
  if (!cell_info)
    return 0;

  flags = 0;

  if (GTK_RBNODE_FLAG_SET (cell_info->node, GTK_RBNODE_IS_SELECTED))
    flags |= GTK_CELL_RENDERER_SELECTED;

  if (GTK_RBNODE_FLAG_SET (cell_info->node, GTK_RBNODE_IS_PRELIT))
    flags |= GTK_CELL_RENDERER_PRELIT;

  if (gtk_tree_view_column_get_sort_indicator (cell_info->cell_col_ref))
    flags |= GTK_CELL_RENDERER_SORTED;

  treeview = GTK_TREE_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (parent)));

  if (cell_info->cell_col_ref == gtk_tree_view_get_expander_column (treeview))
    {
      if (GTK_RBNODE_FLAG_SET (cell_info->node, GTK_RBNODE_IS_PARENT))
        flags |= GTK_CELL_RENDERER_EXPANDABLE;

      if (cell_info->node->children)
        flags |= GTK_CELL_RENDERER_EXPANDED;
    }

  if (gtk_widget_has_focus (GTK_WIDGET (treeview)))
    {
      GtkTreeViewColumn *column;
      GtkTreePath *path;
      GtkRBTree *tree;
      GtkRBNode *node = NULL;
      
      gtk_tree_view_get_cursor (treeview, &path, &column);
      if (path)
        {
          _gtk_tree_view_find_node (treeview, path, &tree, &node);
          gtk_tree_path_free (path);
        }
      else
        tree = NULL;

      if (cell_info->cell_col_ref == column
          && cell_info->tree == tree
          && cell_info->node == node)
        flags |= GTK_CELL_RENDERER_FOCUSED;
    }

  return flags;
}

static void
gtk_tree_view_accessible_expand_collapse (GtkCellAccessibleParent *parent,
                                          GtkCellAccessible       *cell)
{
  GtkTreeViewAccessibleCellInfo *cell_info;
  GtkTreeView *treeview;
  GtkTreePath *path;

  treeview = GTK_TREE_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (parent)));

  cell_info = find_cell_info (GTK_TREE_VIEW_ACCESSIBLE (parent), cell);
  if (!cell_info ||
      cell_info->cell_col_ref != gtk_tree_view_get_expander_column (treeview))
    return;

  path = cell_info_get_path (cell_info);

  if (cell_info->node->children)
    gtk_tree_view_collapse_row (treeview, path);
  else
    gtk_tree_view_expand_row (treeview, path, FALSE);

  gtk_tree_path_free (path);
}

static void
gtk_tree_view_accessible_activate (GtkCellAccessibleParent *parent,
                                   GtkCellAccessible       *cell)
{
  GtkTreeViewAccessibleCellInfo *cell_info;
  GtkTreeView *treeview;
  GtkTreePath *path;

  treeview = GTK_TREE_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (parent)));

  cell_info = find_cell_info (GTK_TREE_VIEW_ACCESSIBLE (parent), cell);
  if (!cell_info)
    return;

  path = cell_info_get_path (cell_info);

  gtk_tree_view_row_activated (treeview, path, cell_info->cell_col_ref);

  gtk_tree_path_free (path);
}

static void
gtk_tree_view_accessible_edit (GtkCellAccessibleParent *parent,
                               GtkCellAccessible       *cell)
{
  GtkTreeView *treeview;

  if (!gtk_tree_view_accessible_grab_cell_focus (parent, cell))
    return;

  treeview = GTK_TREE_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (parent)));

  g_signal_emit_by_name (treeview,
                         "real-select-cursor-row",
                         TRUE);
}

static void
gtk_tree_view_accessible_update_relationset (GtkCellAccessibleParent *parent,
                                             GtkCellAccessible       *cell,
                                             AtkRelationSet          *relationset)
{
  GtkTreeViewAccessibleCellInfo *cell_info;
  GtkTreeViewAccessible *accessible;
  GtkTreeViewColumn *column;
  GtkTreeView *treeview;
  AtkRelation *relation;
  GtkRBTree *tree;
  GtkRBNode *node;
  AtkObject *object;

  /* Don't set relations on cells that aren't direct descendants of the treeview.
   * So only set it on the container, not on the renderer accessibles */
  if (atk_object_get_parent (ATK_OBJECT (cell)) != ATK_OBJECT (parent))
    return;

  accessible = GTK_TREE_VIEW_ACCESSIBLE (parent);
  cell_info = find_cell_info (accessible, cell);
  if (!cell_info)
    return;

  /* only set parent/child rows on the expander column */
  treeview = GTK_TREE_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (parent)));
  column = gtk_tree_view_get_expander_column (treeview);
  if (column != cell_info->cell_col_ref)
    return;

  /* Update CHILD_OF relation to parent cell */
  relation = atk_relation_set_get_relation_by_type (relationset, ATK_RELATION_NODE_CHILD_OF);
  if (relation)
    atk_relation_set_remove (relationset, relation);

  if (cell_info->tree->parent_tree)
    {
      object = ATK_OBJECT (peek_cell (accessible, cell_info->tree->parent_tree, cell_info->tree->parent_node, column));
      if (object == NULL)
        object = ATK_OBJECT (create_cell (treeview, accessible, cell_info->tree->parent_tree, cell_info->tree->parent_node, column));
    }
  else
    object = ATK_OBJECT (accessible);

  atk_relation_set_add_relation_by_type (relationset, ATK_RELATION_NODE_CHILD_OF, object);

  /* Update PARENT_OF relation for all child cells */
  relation = atk_relation_set_get_relation_by_type (relationset, ATK_RELATION_NODE_PARENT_OF);
  if (relation)
    atk_relation_set_remove (relationset, relation);

  tree = cell_info->node->children;
  if (tree)
    {
      for (node = _gtk_rbtree_first (tree);
           node != NULL;
           node = _gtk_rbtree_next (tree, node))
        {
          object = ATK_OBJECT (peek_cell (accessible, tree, node, column));
          if (object == NULL)
            object = ATK_OBJECT (create_cell (treeview, accessible, tree, node, column));

          atk_relation_set_add_relation_by_type (relationset, ATK_RELATION_NODE_PARENT_OF, ATK_OBJECT (object));
        }
    }
}

static void
gtk_cell_accessible_parent_interface_init (GtkCellAccessibleParentIface *iface)
{
  iface->get_cell_extents = gtk_tree_view_accessible_get_cell_extents;
  iface->get_cell_area = gtk_tree_view_accessible_get_cell_area;
  iface->grab_focus = gtk_tree_view_accessible_grab_cell_focus;
  iface->get_child_index = gtk_tree_view_accessible_get_child_index;
  iface->get_renderer_state = gtk_tree_view_accessible_get_renderer_state;
  iface->expand_collapse = gtk_tree_view_accessible_expand_collapse;
  iface->activate = gtk_tree_view_accessible_activate;
  iface->edit = gtk_tree_view_accessible_edit;
  iface->update_relationset = gtk_tree_view_accessible_update_relationset;
}

void
_gtk_tree_view_accessible_reorder (GtkTreeView *treeview)
{
  GtkTreeViewAccessible *accessible;

  accessible = GTK_TREE_VIEW_ACCESSIBLE (_gtk_widget_peek_accessible (GTK_WIDGET (treeview)));
  if (accessible == NULL)
    return;

  g_signal_emit_by_name (accessible, "row-reordered");
}

static gboolean
is_cell_showing (GtkTreeView  *tree_view,
                 GdkRectangle *cell_rect)
{
  GdkRectangle rect, *visible_rect;
  GdkRectangle rect1, *tree_cell_rect;
  gint bx, by;
  gboolean is_showing;

 /* A cell is considered "SHOWING" if any part of the cell is
  * in the visible area. Other ways we could do this is by a
  * cell's midpoint or if the cell is fully in the visible range.
  * Since we have the cell_rect x, y, width, height of the cell,
  * any of these is easy to compute.
  *
  * It is assumed that cell's rectangle is in widget coordinates
  * so we must transform to tree cordinates.
  */
  visible_rect = &rect;
  tree_cell_rect = &rect1;
  tree_cell_rect->x = cell_rect->x;
  tree_cell_rect->y = cell_rect->y;
  tree_cell_rect->width = cell_rect->width;
  tree_cell_rect->height = cell_rect->height;

  gtk_tree_view_get_visible_rect (tree_view, visible_rect);
  gtk_tree_view_convert_tree_to_bin_window_coords (tree_view, visible_rect->x,
                                                   visible_rect->y, &bx, &by);

  if (((tree_cell_rect->x + tree_cell_rect->width) < bx) ||
     ((tree_cell_rect->y + tree_cell_rect->height) < by) ||
     (tree_cell_rect->x > (bx + visible_rect->width)) ||
     (tree_cell_rect->y > (by + visible_rect->height)))
    is_showing =  FALSE;
  else
    is_showing = TRUE;

  return is_showing;
}

/* Misc Private */

static int
cell_info_get_index (GtkTreeView                     *tree_view,
                     GtkTreeViewAccessibleCellInfo   *info)
{
  int index;

  index = _gtk_rbtree_node_get_index (info->tree, info->node) + 1;
  index *= get_n_columns (tree_view);
  index += get_column_number (tree_view, info->cell_col_ref);

  return index;
}

static void
cell_info_new (GtkTreeViewAccessible *accessible,
               GtkRBTree             *tree,
               GtkRBNode             *node,
               GtkTreeViewColumn     *tv_col,
               GtkCellAccessible     *cell)
{
  GtkTreeViewAccessibleCellInfo *cell_info;

  cell_info = g_new (GtkTreeViewAccessibleCellInfo, 1);

  cell_info->tree = tree;
  cell_info->node = node;
  cell_info->cell_col_ref = tv_col;
  cell_info->cell = cell;
  cell_info->view = accessible;

  g_object_set_qdata (G_OBJECT (cell), 
                      gtk_tree_view_accessible_get_data_quark (),
                      cell_info);

  g_hash_table_replace (accessible->priv->cell_infos, cell_info, cell_info);
}

/* Returns the column number of the specified GtkTreeViewColumn
 * The column must be visible.
 */
static gint
get_column_number (GtkTreeView       *treeview,
                   GtkTreeViewColumn *column)
{
  GtkTreeViewColumn *cur;
  guint i, number;

  number = 0;

  for (i = 0; i < gtk_tree_view_get_n_columns (treeview); i++)
    {
      cur = gtk_tree_view_get_column (treeview, i);
      
      if (!gtk_tree_view_column_get_visible (cur))
        continue;

      if (cur == column)
        break;

      number++;
    }

  g_return_val_if_fail (i < gtk_tree_view_get_n_columns (treeview), 0);

  return number;
}

static gboolean
get_rbtree_column_from_index (GtkTreeView        *tree_view,
                              gint                index,
                              GtkRBTree         **tree,
                              GtkRBNode         **node,
                              GtkTreeViewColumn **column)
{
  guint n_columns = get_n_columns (tree_view);

  if (n_columns == 0)
    return FALSE;
  /* First row is the column headers */
  index -= n_columns;
  if (index < 0)
    return FALSE;

  if (tree)
    {
      g_return_val_if_fail (node != NULL, FALSE);

      if (!_gtk_rbtree_find_index (_gtk_tree_view_get_rbtree (tree_view),
                                   index / n_columns,
                                   tree,
                                   node))
        return FALSE;
    }

  if (column)
    {
      *column = get_visible_column (tree_view, index % n_columns);
      if (*column == NULL)
        return FALSE;
  }
  return TRUE;
}

static GtkTreeViewAccessibleCellInfo *
find_cell_info (GtkTreeViewAccessible *accessible,
                GtkCellAccessible     *cell)
{
  AtkObject *parent;
  
  parent = atk_object_get_parent (ATK_OBJECT (cell));
  while (parent != ATK_OBJECT (accessible))
    {
      cell = GTK_CELL_ACCESSIBLE (parent);
      parent = atk_object_get_parent (ATK_OBJECT (cell));
    }

  return g_object_get_qdata (G_OBJECT (cell),
                             gtk_tree_view_accessible_get_data_quark ());
}

static AtkObject *
get_header_from_column (GtkTreeViewColumn *tv_col)
{
  AtkObject *rc;
  GtkWidget *header_widget;

  if (tv_col == NULL)
    return NULL;

  header_widget = gtk_tree_view_column_get_button (tv_col);

  if (header_widget)
    rc = gtk_widget_get_accessible (header_widget);
  else
    rc = NULL;

  return rc;
}

void
_gtk_tree_view_accessible_add (GtkTreeView *treeview,
                               GtkRBTree   *tree,
                               GtkRBNode   *node)
{
  GtkTreeViewAccessible *accessible;
  guint row, n_rows, n_cols, i;

  accessible = GTK_TREE_VIEW_ACCESSIBLE (_gtk_widget_peek_accessible (GTK_WIDGET (treeview)));
  if (accessible == NULL)
    return;

  if (node == NULL)
    {
      row = tree->parent_tree ? _gtk_rbtree_node_get_index (tree->parent_tree, tree->parent_node) : 0;
      n_rows = tree->root->total_count;
    }
  else
    {
      row = _gtk_rbtree_node_get_index (tree, node);
      n_rows = 1 + (node->children ? node->children->root->total_count : 0);
    }

  g_signal_emit_by_name (accessible, "row-inserted", row, n_rows);

  n_cols = get_n_columns (treeview);
  if (n_cols)
    {
      for (i = (row + 1) * n_cols; i < (row + n_rows + 1) * n_cols; i++)
        {
         /* Pass NULL as the child object, i.e. 4th argument */
          g_signal_emit_by_name (accessible, "children-changed::add", i, NULL, NULL);
        }
    }
}

void
_gtk_tree_view_accessible_remove (GtkTreeView *treeview,
                                  GtkRBTree   *tree,
                                  GtkRBNode   *node)
{
  GtkTreeViewAccessibleCellInfo *cell_info;
  GHashTableIter iter;
  GtkTreeViewAccessible *accessible;
  guint row, n_rows, n_cols, i;

  accessible = GTK_TREE_VIEW_ACCESSIBLE (_gtk_widget_peek_accessible (GTK_WIDGET (treeview)));
  if (accessible == NULL)
    return;

  /* if this shows up in profiles, special-case node->children == NULL */

  if (node == NULL)
    {
      row = tree->parent_tree ? _gtk_rbtree_node_get_index (tree->parent_tree, tree->parent_node) : 0;
      n_rows = tree->root->total_count + 1;
    }
  else
    {
      row = _gtk_rbtree_node_get_index (tree, node);
      n_rows = 1 + (node->children ? node->children->root->total_count : 0);

      tree = node->children;
    }

  g_signal_emit_by_name (accessible, "row-deleted", row, n_rows);

  n_cols = get_n_columns (treeview);
  if (n_cols)
    {
      for (i = (n_rows + row + 1) * n_cols - 1; i >= (row + 1) * n_cols; i--)
        {
         /* Pass NULL as the child object, i.e. 4th argument */
          g_signal_emit_by_name (accessible, "children-changed::remove", i, NULL, NULL);
        }

      g_hash_table_iter_init (&iter, accessible->priv->cell_infos);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&cell_info))
        {
          if (node == cell_info->node ||
              tree == cell_info->tree ||
              (tree && _gtk_rbtree_contains (tree, cell_info->tree)))
            g_hash_table_iter_remove (&iter);
        }
    }
}

void
_gtk_tree_view_accessible_changed (GtkTreeView *treeview,
                                   GtkRBTree   *tree,
                                   GtkRBNode   *node)
{
  GtkTreeViewAccessible *accessible;
  guint i;

  accessible = GTK_TREE_VIEW_ACCESSIBLE (gtk_widget_get_accessible (GTK_WIDGET (treeview)));

  for (i = 0; i < gtk_tree_view_get_n_columns (treeview); i++)
    {
      GtkCellAccessible *cell = peek_cell (accessible,
                                           tree, node,
                                           gtk_tree_view_get_column (treeview, i));

      if (cell == NULL)
        continue;

      set_cell_data (treeview, accessible, cell);
      _gtk_cell_accessible_update_cache (cell);
    }

  g_signal_emit_by_name (accessible, "visible-data-changed");
}

/* NB: id is not checked, only columns < id are.
 * This is important so the function works for notification of removal of a column */
static guint
to_visible_column_id (GtkTreeView *treeview,
                      guint        id)
{
  guint i;
  guint invisible;

  invisible = 0;

  for (i = 0; i < id; i++)
    {
      GtkTreeViewColumn *column = gtk_tree_view_get_column (treeview, i);

      if (!gtk_tree_view_column_get_visible (column))
        invisible++;
    }

  return id - invisible;
}

static void
gtk_tree_view_accessible_do_add_column (GtkTreeViewAccessible *accessible,
                                        GtkTreeView           *treeview,
                                        GtkTreeViewColumn     *column,
                                        guint                  id)
{
  guint row, n_rows, n_cols;

  /* Generate column-inserted signal */
  g_signal_emit_by_name (accessible, "column-inserted", id, 1);

  n_rows = get_n_rows (treeview);
  n_cols = get_n_columns (treeview);

  /* Generate children-changed signals */
  for (row = 0; row <= n_rows; row++)
    {
     /* Pass NULL as the child object, i.e. 4th argument */
      g_signal_emit_by_name (accessible, "children-changed::add",
                             (row * n_cols) + id, NULL, NULL);
    }
}

void
_gtk_tree_view_accessible_add_column (GtkTreeView       *treeview,
                                      GtkTreeViewColumn *column,
                                      guint              id)
{
  AtkObject *obj;

  if (!gtk_tree_view_column_get_visible (column))
    return;

  obj = _gtk_widget_peek_accessible (GTK_WIDGET (treeview));
  if (obj == NULL)
    return;

  gtk_tree_view_accessible_do_add_column (GTK_TREE_VIEW_ACCESSIBLE (obj),
                                          treeview,
                                          column,
                                          to_visible_column_id (treeview, id));
}

static void
gtk_tree_view_accessible_do_remove_column (GtkTreeViewAccessible *accessible,
                                           GtkTreeView           *treeview,
                                           GtkTreeViewColumn     *column,
                                           guint                  id)
{
  GtkTreeViewAccessibleCellInfo *cell_info;
  GHashTableIter iter;
  gpointer value;
  guint row, n_rows, n_cols;

  /* Clean column from cache */
  g_hash_table_iter_init (&iter, accessible->priv->cell_infos);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      cell_info = value;
      if (cell_info->cell_col_ref == column)
        g_hash_table_iter_remove (&iter);
    }

  /* Generate column-deleted signal */
  g_signal_emit_by_name (accessible, "column-deleted", id, 1);

  n_rows = get_n_rows (treeview);
  n_cols = get_n_columns (treeview);

  /* Generate children-changed signals */
  for (row = 0; row <= n_rows; row++)
    {
      /* Pass NULL as the child object, 4th argument */
      g_signal_emit_by_name (accessible, "children-changed::remove",
                             (row * n_cols) + id, NULL, NULL);
    }
}

void
_gtk_tree_view_accessible_remove_column (GtkTreeView       *treeview,
                                         GtkTreeViewColumn *column,
                                         guint              id)
{
  AtkObject *obj;

  if (!gtk_tree_view_column_get_visible (column))
    return;

  obj = _gtk_widget_peek_accessible (GTK_WIDGET (treeview));
  if (obj == NULL)
    return;

  gtk_tree_view_accessible_do_remove_column (GTK_TREE_VIEW_ACCESSIBLE (obj),
                                             treeview,
                                             column,
                                             to_visible_column_id (treeview, id));
}

void
_gtk_tree_view_accessible_reorder_column (GtkTreeView       *treeview,
                                          GtkTreeViewColumn *column)
{
  AtkObject *obj;

  obj = _gtk_widget_peek_accessible (GTK_WIDGET (treeview));
  if (obj == NULL)
    return;

  g_signal_emit_by_name (obj, "column-reordered");
}

void
_gtk_tree_view_accessible_toggle_visibility (GtkTreeView       *treeview,
                                             GtkTreeViewColumn *column)
{
  AtkObject *obj;
  guint i, id;

  obj = _gtk_widget_peek_accessible (GTK_WIDGET (treeview));
  if (obj == NULL)
    return;

  if (gtk_tree_view_column_get_visible (column))
    {
      id = get_column_number (treeview, column);

      gtk_tree_view_accessible_do_add_column (GTK_TREE_VIEW_ACCESSIBLE (obj),
                                              treeview,
                                              column,
                                              id);
    }
  else
    {
      id = 0;

      for (i = 0; i < gtk_tree_view_get_n_columns (treeview); i++)
        {
          GtkTreeViewColumn *cur = gtk_tree_view_get_column (treeview, i);
          
          if (gtk_tree_view_column_get_visible (cur))
            id++;

          if (cur == column)
            break;
        }

      gtk_tree_view_accessible_do_remove_column (GTK_TREE_VIEW_ACCESSIBLE (obj),
                                                 treeview,
                                                 column,
                                                 id);
    }
}

static GtkTreeViewColumn *
get_effective_focus_column (GtkTreeView       *treeview,
                            GtkTreeViewColumn *column)
{
  if (column == NULL)
    column = get_visible_column (treeview, 0);

  return column;
}

void
_gtk_tree_view_accessible_update_focus_column (GtkTreeView       *treeview,
                                               GtkTreeViewColumn *old_focus,
                                               GtkTreeViewColumn *new_focus)
{
  GtkTreeViewAccessible *accessible;
  AtkObject *obj;
  GtkRBTree *cursor_tree;
  GtkRBNode *cursor_node;
  GtkCellAccessible *cell;

  old_focus = get_effective_focus_column (treeview, old_focus);
  new_focus = get_effective_focus_column (treeview, new_focus);
  if (old_focus == new_focus)
    return;

  obj = _gtk_widget_peek_accessible (GTK_WIDGET (treeview));
  if (obj == NULL)
    return;

  accessible = GTK_TREE_VIEW_ACCESSIBLE (obj);

  if (!_gtk_tree_view_get_cursor_node (treeview, &cursor_tree, &cursor_node))
    return;

  if (old_focus)
    {
      cell = peek_cell (accessible, cursor_tree, cursor_node, old_focus);
      if (cell != NULL)
        _gtk_cell_accessible_state_changed (cell, GTK_CELL_RENDERER_FOCUSED, 0);
    }

  if (new_focus)
    {
      cell = peek_cell (accessible, cursor_tree, cursor_node, new_focus);
      if (cell != NULL)
        _gtk_cell_accessible_state_changed (cell, 0, GTK_CELL_RENDERER_FOCUSED);
      else
        cell = create_cell (treeview, accessible, cursor_tree, cursor_node, new_focus);

      g_signal_emit_by_name (accessible, "active-descendant-changed", cell);
    }
}

void
_gtk_tree_view_accessible_add_state (GtkTreeView          *treeview,
                                     GtkRBTree            *tree,
                                     GtkRBNode            *node,
                                     GtkCellRendererState  state)
{
  GtkTreeViewAccessible *accessible;
  GtkTreeViewColumn *single_column;
  AtkObject *obj;
  guint i;

  obj = _gtk_widget_peek_accessible (GTK_WIDGET (treeview));
  if (obj == NULL)
    return;

  accessible = GTK_TREE_VIEW_ACCESSIBLE (obj);

  if (state == GTK_CELL_RENDERER_FOCUSED)
    {
      single_column = get_effective_focus_column (treeview, _gtk_tree_view_get_focus_column (treeview));
    }
  else if (state == GTK_CELL_RENDERER_EXPANDED ||
           state == GTK_CELL_RENDERER_EXPANDABLE)
    {
      single_column = gtk_tree_view_get_expander_column (treeview);
    }
  else
    single_column = NULL;

  if (single_column)
    {
      GtkCellAccessible *cell = peek_cell (accessible,
                                           tree, node,
                                           single_column);

      if (cell != NULL)
        _gtk_cell_accessible_state_changed (cell, state, 0);

      if (state == GTK_CELL_RENDERER_FOCUSED)
        {
          if (cell == NULL)
            cell = create_cell (treeview, accessible, tree, node, single_column);
          
          g_signal_emit_by_name (accessible, "active-descendant-changed", cell);
        }
    }
  else
    {
      for (i = 0; i < gtk_tree_view_get_n_columns (treeview); i++)
        {
          GtkCellAccessible *cell = peek_cell (accessible,
                                               tree, node,
                                               gtk_tree_view_get_column (treeview, i));

          if (cell == NULL)
            continue;

          _gtk_cell_accessible_state_changed (cell, state, 0);
        }
    }

  if (state == GTK_CELL_RENDERER_SELECTED)
    g_signal_emit_by_name (accessible, "selection-changed");
}

void
_gtk_tree_view_accessible_remove_state (GtkTreeView          *treeview,
                                        GtkRBTree            *tree,
                                        GtkRBNode            *node,
                                        GtkCellRendererState  state)
{
  GtkTreeViewAccessible *accessible;
  GtkTreeViewColumn *single_column;
  AtkObject *obj;
  guint i;

  obj = _gtk_widget_peek_accessible (GTK_WIDGET (treeview));
  if (obj == NULL)
    return;

  accessible = GTK_TREE_VIEW_ACCESSIBLE (obj);

  if (state == GTK_CELL_RENDERER_FOCUSED)
    {
      single_column = get_effective_focus_column (treeview, _gtk_tree_view_get_focus_column (treeview));
    }
  else if (state == GTK_CELL_RENDERER_EXPANDED ||
           state == GTK_CELL_RENDERER_EXPANDABLE)
    {
      single_column = gtk_tree_view_get_expander_column (treeview);
    }
  else
    single_column = NULL;

  if (single_column)
    {
      GtkCellAccessible *cell = peek_cell (accessible,
                                           tree, node,
                                           single_column);

      if (cell != NULL)
        _gtk_cell_accessible_state_changed (cell, 0, state);
    }
  else
    {
      for (i = 0; i < gtk_tree_view_get_n_columns (treeview); i++)
        {
          GtkCellAccessible *cell = peek_cell (accessible,
                                               tree, node,
                                               gtk_tree_view_get_column (treeview, i));

          if (cell == NULL)
            continue;

          _gtk_cell_accessible_state_changed (cell, 0, state);
        }
    }

  if (state == GTK_CELL_RENDERER_SELECTED)
    g_signal_emit_by_name (accessible, "selection-changed");
}
