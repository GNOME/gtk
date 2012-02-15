/* GAIL - The GNOME Accessibility Implementation Library
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif
#include "gtktreeviewaccessible.h"
#include "gtkrenderercellaccessible.h"
#include "gtkbooleancellaccessible.h"
#include "gtkimagecellaccessible.h"
#include "gtkcontainercellaccessible.h"
#include "gtktextcellaccessible.h"
#include "gtkcellaccessibleparent.h"

typedef struct _GtkTreeViewAccessibleCellInfo  GtkTreeViewAccessibleCellInfo;
struct _GtkTreeViewAccessibleCellInfo
{
  GtkCellAccessible *cell;
  GtkTreeRowReference *cell_row_ref;
  GtkTreeViewColumn *cell_col_ref;
  GtkTreeViewAccessible *view;
  gboolean in_use;
};

/* signal handling */

static gboolean row_expanded_cb      (GtkTreeView      *tree_view,
                                      GtkTreeIter      *iter,
                                      GtkTreePath      *path);
static gboolean row_collapsed_cb     (GtkTreeView      *tree_view,
                                      GtkTreeIter      *iter,
                                      GtkTreePath      *path);
static void     size_allocate_cb     (GtkWidget        *widget,
                                      GtkAllocation    *allocation);
static void     selection_changed_cb (GtkTreeSelection *selection,
                                      gpointer          data);

static void     columns_changed      (GtkTreeView      *tree_view);
static void     cursor_changed       (GtkTreeView      *tree_view);
static gboolean focus_in             (GtkWidget        *widget);
static gboolean focus_out            (GtkWidget        *widget);

static void     column_visibility_changed
                                     (GObject          *object,
                                      GParamSpec       *param,
                                      gpointer          user_data);
static void     destroy_count_func   (GtkTreeView      *tree_view,
                                      GtkTreePath      *path,
                                      gint              count,
                                      gpointer          user_data);

/* Misc */

static void             set_iter_nth_row                (GtkTreeView            *tree_view,
                                                         GtkTreeIter            *iter,
                                                         gint                   row);
static gint             get_row_from_tree_path          (GtkTreeView            *tree_view,
                                                         GtkTreePath            *path);
static GtkTreeViewColumn* get_column                    (GtkTreeView            *tree_view,
                                                         gint                   in_col);
static gint             get_actual_column_number        (GtkTreeView            *tree_view,
                                                         gint                   visible_column);
static gint             get_visible_column_number       (GtkTreeView            *tree_view,
                                                         gint                   actual_column);
static void             iterate_thru_children           (GtkTreeView            *tree_view,
                                                         GtkTreeModel           *tree_model,
                                                         GtkTreePath            *tree_path,
                                                         GtkTreePath            *orig,
                                                         gint                   *count,
                                                         gint                   depth);
static void             clean_rows                      (GtkTreeViewAccessible           *tree_view);
static void             clean_cols                      (GtkTreeViewAccessible           *tree_view,
                                                         GtkTreeViewColumn      *tv_col);
static void             traverse_cells                  (GtkTreeViewAccessible           *tree_view,
                                                         GtkTreePath            *tree_path,
                                                         gboolean               set_stale,
                                                         gboolean               inc_row);
static gboolean         update_cell_value               (GtkRendererCellAccessible       *renderer_cell,
                                                         GtkTreeViewAccessible           *accessible,
                                                         gboolean               emit_change_signal);
static void             set_cell_visibility             (GtkTreeView            *tree_view,
                                                         GtkCellAccessible      *cell,
                                                         GtkTreeViewColumn      *tv_col,
                                                         GtkTreePath            *tree_path,
                                                         gboolean               emit_signal);
static gboolean         is_cell_showing                 (GtkTreeView            *tree_view,
                                                         GdkRectangle           *cell_rect);
static void             set_expand_state                (GtkTreeView            *tree_view,
                                                         GtkTreeModel           *tree_model,
                                                         GtkTreeViewAccessible           *accessible,
                                                         GtkTreePath            *tree_path,
                                                         gboolean               set_on_ancestor);
static void             set_cell_expandable             (GtkCellAccessible     *cell);
static void             add_cell_actions                (GtkCellAccessible     *cell,
                                                         gboolean               editable);

static void             toggle_cell_toggled             (GtkCellAccessible     *cell);
static void             edit_cell                       (GtkCellAccessible     *cell);
static void             activate_cell                   (GtkCellAccessible     *cell);
static void             cell_destroyed                  (gpointer               data);
static void             cell_info_new                   (GtkTreeViewAccessible           *accessible,
                                                         GtkTreeModel           *tree_model,
                                                         GtkTreePath            *path,
                                                         GtkTreeViewColumn      *tv_col,
                                                         GtkCellAccessible      *cell);
static GtkCellAccessible *find_cell                       (GtkTreeViewAccessible           *accessible,
                                                         gint                   index);
static void             refresh_cell_index              (GtkCellAccessible     *cell);
static void             connect_model_signals           (GtkTreeView            *view,
                                                         GtkTreeViewAccessible           *accessible);
static void             disconnect_model_signals        (GtkTreeViewAccessible           *accessible);
static void             clear_cached_data               (GtkTreeViewAccessible           *view);
static gint             get_column_number               (GtkTreeView            *tree_view,
                                                         GtkTreeViewColumn      *column,
                                                         gboolean               visible);
static gint             get_focus_index                 (GtkTreeView            *tree_view);
static gint             get_index                       (GtkTreeView            *tree_view,
                                                         GtkTreePath            *path,
                                                         gint                   actual_column);
static void             count_rows                      (GtkTreeModel           *model,
                                                         GtkTreeIter            *iter,
                                                         GtkTreePath            *end_path,
                                                         gint                   *count,
                                                         gint                   level,
                                                         gint                   depth);

static gboolean         get_path_column_from_index      (GtkTreeView            *tree_view,
                                                         gint                   index,
                                                         GtkTreePath            **path,
                                                         GtkTreeViewColumn      **column);

static GtkTreeViewAccessibleCellInfo* find_cell_info    (GtkTreeViewAccessible           *view,
                                                         GtkCellAccessible               *cell,
                                                         gboolean                live_only);
static AtkObject *       get_header_from_column         (GtkTreeViewColumn      *tv_col);
static gboolean          idle_garbage_collect_cell_data (gpointer data);


static void atk_table_interface_init                  (AtkTableIface                *iface);
static void atk_selection_interface_init              (AtkSelectionIface            *iface);
static void atk_component_interface_init              (AtkComponentIface            *iface);
static void gtk_cell_accessible_parent_interface_init (GtkCellAccessibleParentIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkTreeViewAccessible, _gtk_tree_view_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_TABLE, atk_table_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_SELECTION, atk_selection_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_COMPONENT, atk_component_interface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_ACCESSIBLE_PARENT, gtk_cell_accessible_parent_interface_init))


static void
adjustment_changed (GtkAdjustment *adjustment,
                    GtkWidget     *widget)
{
  GtkTreeViewAccessible *accessible;

  accessible = GTK_TREE_VIEW_ACCESSIBLE (gtk_widget_get_accessible (widget));
  traverse_cells (accessible, NULL, FALSE, FALSE);
}

static void
hadjustment_set_cb (GObject    *widget,
                    GParamSpec *pspec,
                    gpointer    data)
{
  GtkTreeViewAccessible *accessible = data;
  GtkAdjustment *adj;

  g_object_get (widget, "hadjustment", &adj, NULL);
  accessible->old_hadj = adj;
  g_object_add_weak_pointer (G_OBJECT (accessible->old_hadj), (gpointer *)&accessible->old_hadj);
  g_signal_connect (adj, "value-changed", G_CALLBACK (adjustment_changed), widget);
}

static void
vadjustment_set_cb (GObject    *widget,
                    GParamSpec *pspec,
                    gpointer    data)
{
  GtkTreeViewAccessible *accessible = data;
  GtkAdjustment *adj;

  g_object_get (widget, "vadjustment", &adj, NULL);
  accessible->old_vadj = adj;
  g_object_add_weak_pointer (G_OBJECT (accessible->old_vadj), (gpointer *)&accessible->old_vadj);
  g_signal_connect (adj, "value-changed",
                    G_CALLBACK (adjustment_changed), widget);
}

static void
cell_info_free (GtkTreeViewAccessibleCellInfo *cell_info)
{
  /* g_object_unref (cell_info->cell); */
  if (cell_info->cell_row_ref)
    gtk_tree_row_reference_free (cell_info->cell_row_ref);
  g_free (cell_info);
}

static void
gtk_tree_view_accessible_initialize (AtkObject *obj,
                                     gpointer   data)
{
  GtkTreeViewAccessible *accessible;
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  GList *tv_cols, *tmp_list;
  GtkWidget *widget;
  GtkTreeSelection *selection;

  ATK_OBJECT_CLASS (_gtk_tree_view_accessible_parent_class)->initialize (obj, data);

  accessible = GTK_TREE_VIEW_ACCESSIBLE (obj);
  accessible->col_data = NULL;
  accessible->focus_cell = NULL;
  accessible->old_hadj = NULL;
  accessible->old_vadj = NULL;
  accessible->idle_expand_id = 0;
  accessible->idle_expand_path = NULL;
  accessible->n_children_deleted = 0;

  accessible->cell_info_by_index = g_hash_table_new_full (g_int_hash,
      g_int_equal, NULL, (GDestroyNotify) cell_info_free);

  widget = GTK_WIDGET (data);
  tree_view = GTK_TREE_VIEW (widget);
  tree_model = gtk_tree_view_get_model (tree_view);
  selection = gtk_tree_view_get_selection (tree_view);

  g_signal_connect_after (widget, "row-collapsed",
                          G_CALLBACK (row_collapsed_cb), NULL);
  g_signal_connect (widget, "row-expanded",
                    G_CALLBACK (row_expanded_cb), NULL);
  g_signal_connect (widget, "size-allocate",
                    G_CALLBACK (size_allocate_cb), NULL);
  g_signal_connect (selection, "changed",
                    G_CALLBACK (selection_changed_cb), obj);

  g_signal_connect (tree_view, "columns-changed",
                    G_CALLBACK (columns_changed), NULL);
  g_signal_connect (tree_view, "cursor-changed",
                    G_CALLBACK (cursor_changed), NULL);
  g_signal_connect (tree_view, "focus-in-event",
                    G_CALLBACK (focus_in), NULL);
  g_signal_connect (tree_view, "focus-out-event",
                    G_CALLBACK (focus_out), NULL);

  accessible->tree_model = tree_model;
  accessible->n_rows = 0;
  accessible->n_cols = 0;
  if (tree_model)
    {
      g_object_add_weak_pointer (G_OBJECT (accessible->tree_model), (gpointer *)&accessible->tree_model);
      count_rows (tree_model, NULL, NULL, &accessible->n_rows, 0, G_MAXINT);
      connect_model_signals (tree_view, accessible);

      if (gtk_tree_model_get_flags (tree_model) & GTK_TREE_MODEL_LIST_ONLY)
        obj->role = ATK_ROLE_TABLE;
      else
        obj->role = ATK_ROLE_TREE_TABLE;
    }

  hadjustment_set_cb (G_OBJECT (widget), NULL, accessible);
  vadjustment_set_cb (G_OBJECT (widget), NULL, accessible);
  g_signal_connect (widget, "notify::hadjustment",
                    G_CALLBACK (hadjustment_set_cb), accessible);
  g_signal_connect (widget, "notify::vadjustment",
                    G_CALLBACK (vadjustment_set_cb), accessible);

  accessible->col_data = g_array_sized_new (FALSE, TRUE,
                                            sizeof (GtkTreeViewColumn *), 0);

  tv_cols = gtk_tree_view_get_columns (tree_view);
  for (tmp_list = tv_cols; tmp_list; tmp_list = tmp_list->next)
    {
      accessible->n_cols++;
      g_signal_connect (tmp_list->data, "notify::visible",
                        G_CALLBACK (column_visibility_changed), tree_view);
      g_array_append_val (accessible->col_data, tmp_list->data);
    }
  g_list_free (tv_cols);

  gtk_tree_view_set_destroy_count_func (tree_view,
                                        destroy_count_func,
                                        NULL, NULL);
}

static void
gtk_tree_view_accessible_finalize (GObject *object)
{
  GtkTreeViewAccessible *accessible = GTK_TREE_VIEW_ACCESSIBLE (object);

  clear_cached_data (accessible);

  /* remove any idle handlers still pending */
  if (accessible->idle_garbage_collect_id)
    g_source_remove (accessible->idle_garbage_collect_id);
  if (accessible->idle_cursor_changed_id)
    g_source_remove (accessible->idle_cursor_changed_id);
  if (accessible->idle_expand_id)
    g_source_remove (accessible->idle_expand_id);

  if (accessible->tree_model)
    disconnect_model_signals (accessible);

  if (accessible->cell_info_by_index)
    g_hash_table_destroy (accessible->cell_info_by_index);

  if (accessible->col_data)
    {
      GArray *array = accessible->col_data;

     /* No need to free the contents of the array since it
      * just contains pointers to the GtkTreeViewColumn
      * objects that are in the GtkTreeView.
      */
      g_array_free (array, TRUE);
    }

  G_OBJECT_CLASS (_gtk_tree_view_accessible_parent_class)->finalize (object);
}

static void
gtk_tree_view_accessible_notify_gtk (GObject    *obj,
                                     GParamSpec *pspec)
{
  GtkWidget *widget;
  GtkTreeView *tree_view;
  GtkTreeViewAccessible *accessible;
  GtkAdjustment *adj;

  widget = GTK_WIDGET (obj);
  accessible = GTK_TREE_VIEW_ACCESSIBLE (gtk_widget_get_accessible (widget));
  tree_view = GTK_TREE_VIEW (widget);

  if (g_strcmp0 (pspec->name, "model") == 0)
    {
      GtkTreeModel *tree_model;
      AtkRole role;

      tree_model = gtk_tree_view_get_model (tree_view);
      if (accessible->tree_model)
        disconnect_model_signals (accessible);
      clear_cached_data (accessible);
      accessible->tree_model = tree_model;
      accessible->n_rows = 0;

      if (tree_model)
        {
          g_object_add_weak_pointer (G_OBJECT (accessible->tree_model), (gpointer *)&accessible->tree_model);
          count_rows (tree_model, NULL, NULL, &accessible->n_rows, 0, G_MAXINT);
          connect_model_signals (tree_view, accessible);

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
  else if (g_strcmp0 (pspec->name, "hadjustment") == 0)
    {
      g_object_get (tree_view, "hadjustment", &adj, NULL);
      g_signal_handlers_disconnect_by_func (accessible->old_hadj,
                                            (gpointer) adjustment_changed,
                                            widget);
      accessible->old_hadj = adj;
      g_object_add_weak_pointer (G_OBJECT (accessible->old_hadj), (gpointer *)&accessible->old_hadj);
      g_signal_connect (adj, "value-changed", G_CALLBACK (adjustment_changed), tree_view);
    }
  else if (g_strcmp0 (pspec->name, "vadjustment") == 0)
    {
      g_object_get (tree_view, "vadjustment", &adj, NULL);
      g_signal_handlers_disconnect_by_func (accessible->old_vadj,
                                            (gpointer) adjustment_changed,
                                            widget);
      accessible->old_vadj = adj;
      g_object_add_weak_pointer (G_OBJECT (accessible->old_hadj), (gpointer *)&accessible->old_vadj);
      g_signal_connect (adj, "value-changed", G_CALLBACK (adjustment_changed), tree_view);
    }
  else
    GTK_WIDGET_ACCESSIBLE_CLASS (_gtk_tree_view_accessible_parent_class)->notify_gtk (obj, pspec);
}

static void
gtk_tree_view_accessible_destroyed (GtkWidget     *widget,
                                    GtkAccessible *gtk_accessible)
{
  GtkAdjustment *adj;
  GtkTreeViewAccessible *accessible;

  if (!GTK_IS_TREE_VIEW (widget))
    return;

  accessible = GTK_TREE_VIEW_ACCESSIBLE (gtk_accessible);
  adj = accessible->old_hadj;
  if (adj)
    g_signal_handlers_disconnect_by_func (adj,
                                          (gpointer) adjustment_changed,
                                          widget);
  adj = accessible->old_vadj;
  if (adj)
    g_signal_handlers_disconnect_by_func (adj,
                                          (gpointer) adjustment_changed,
                                          widget);
  if (accessible->tree_model)
    {
      disconnect_model_signals (accessible);
      accessible->tree_model = NULL;
    }
  if (accessible->focus_cell)
    {
      g_object_unref (accessible->focus_cell);
      accessible->focus_cell = NULL;
    }
  if (accessible->idle_expand_id)
    {
      g_source_remove (accessible->idle_expand_id);
      accessible->idle_expand_id = 0;
    }
}

static void
gtk_tree_view_accessible_connect_widget_destroyed (GtkAccessible *accessible)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (accessible);
  if (widget)
    g_signal_connect_after (widget, "destroy",
                            G_CALLBACK (gtk_tree_view_accessible_destroyed), accessible);

  GTK_ACCESSIBLE_CLASS (_gtk_tree_view_accessible_parent_class)->connect_widget_destroyed (accessible);
}

static gint
gtk_tree_view_accessible_get_n_children (AtkObject *obj)
{
  GtkWidget *widget;
  GtkTreeViewAccessible *accessible;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return 0;

  accessible = GTK_TREE_VIEW_ACCESSIBLE (obj);
  return (accessible->n_rows + 1) * accessible->n_cols;
}

static AtkObject *
gtk_tree_view_accessible_ref_child (AtkObject *obj,
                                    gint       i)
{
  GtkWidget *widget;
  GtkTreeViewAccessible *accessible;
  GtkCellAccessible *cell;
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  GtkCellRenderer *renderer;
  GtkTreeIter iter;
  GtkTreeViewColumn *tv_col;
  GtkTreeSelection *selection;
  GtkTreePath *path;
  AtkObject *child;
  AtkObject *parent;
  GtkTreeViewColumn *expander_tv;
  GList *renderer_list;
  GList *l;
  GtkContainerCellAccessible *container = NULL;
  GtkRendererCellAccessible *renderer_cell;
  gboolean is_expander, is_expanded, retval;
  gboolean editable = FALSE;
  gint focus_index;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  if (i >= gtk_tree_view_accessible_get_n_children (obj))
    return NULL;

  accessible = GTK_TREE_VIEW_ACCESSIBLE (obj);
  tree_view = GTK_TREE_VIEW (widget);
  if (i < accessible->n_cols)
    {
      tv_col = gtk_tree_view_get_column (tree_view, i);
      child = get_header_from_column (tv_col);
      if (child)
        g_object_ref (child);
      return child;
    }

  /* Check whether the child is cached */
  cell = find_cell (accessible, i);
  if (cell)
    {
      g_object_ref (cell);
      return ATK_OBJECT (cell);
    }

  if (accessible->focus_cell == NULL)
      focus_index = get_focus_index (tree_view);
  else
      focus_index = -1;

  /* Find the TreePath and GtkTreeViewColumn for the index */
  if (!get_path_column_from_index (tree_view, i, &path, &tv_col))
    return NULL;

  tree_model = gtk_tree_view_get_model (tree_view);
  retval = gtk_tree_model_get_iter (tree_model, &iter, path);
  if (!retval)
    return NULL;

  expander_tv = gtk_tree_view_get_expander_column (tree_view);
  is_expander = FALSE;
  is_expanded = FALSE;
  if (gtk_tree_model_iter_has_child (tree_model, &iter))
    {
      if (expander_tv == tv_col)
        {
          is_expander = TRUE;
          is_expanded = gtk_tree_view_row_expanded (tree_view, path);
        }
    }
  gtk_tree_view_column_cell_set_cell_data (tv_col, tree_model, &iter,
                                           is_expander, is_expanded);

  renderer_list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (tv_col));

  /* If there are more than one renderer in the list,
   * make a container
   */
  if (renderer_list && renderer_list->next)
    {
      GtkCellAccessible *container_cell;

      container = _gtk_container_cell_accessible_new ();

      container_cell = GTK_CELL_ACCESSIBLE (container);
      _gtk_cell_accessible_initialise (container_cell, widget, ATK_OBJECT (accessible), i);

      /* The GtkTreeViewAccessibleCellInfo structure for the container will
       * be before the ones for the cells so that the first one we find for
       * a position will be for the container
       */
      cell_info_new (accessible, tree_model, path, tv_col, container_cell);
      container_cell->refresh_index = refresh_cell_index;
      parent = ATK_OBJECT (container);
    }
  else
    parent = ATK_OBJECT (accessible);

  child = NULL;

  /* Now we make a fake cell_renderer if there is no cell
   * in renderer_list
   */
  if (renderer_list == NULL)
    {
      GtkCellRenderer *fake_renderer;

      fake_renderer = g_object_new (GTK_TYPE_CELL_RENDERER_TEXT, NULL);
      child = _gtk_text_cell_accessible_new ();
      cell = GTK_CELL_ACCESSIBLE (child);
      renderer_cell = GTK_RENDERER_CELL_ACCESSIBLE (child);
      renderer_cell->renderer = fake_renderer;

      /* Create the GtkTreeViewAccessibleCellInfo structure for this cell */
      cell_info_new (accessible, tree_model, path, tv_col, cell);

      _gtk_cell_accessible_initialise (cell, widget, parent, i);

      cell->refresh_index = refresh_cell_index;

      /* Set state if it is expandable */
      if (is_expander)
        {
          set_cell_expandable (cell);
          if (is_expanded)
            _gtk_cell_accessible_add_state (cell, ATK_STATE_EXPANDED, FALSE);
        }
    }
  else
    {
      for (l = renderer_list; l; l = l->next)
        {
          renderer = GTK_CELL_RENDERER (l->data);

          if (GTK_IS_CELL_RENDERER_TEXT (renderer))
            {
              g_object_get (G_OBJECT (renderer), "editable", &editable, NULL);
              child = _gtk_text_cell_accessible_new ();
            }
          else if (GTK_IS_CELL_RENDERER_TOGGLE (renderer))
            child = _gtk_boolean_cell_accessible_new ();
          else if (GTK_IS_CELL_RENDERER_PIXBUF (renderer))
            child = _gtk_image_cell_accessible_new ();
          else
            child = _gtk_renderer_cell_accessible_new ();

          cell = GTK_CELL_ACCESSIBLE (child);
          renderer_cell = GTK_RENDERER_CELL_ACCESSIBLE (child);

          /* Create the GtkTreeViewAccessibleCellInfo for this cell */
          cell_info_new (accessible, tree_model, path, tv_col, cell);

          _gtk_cell_accessible_initialise (cell, widget, parent, i);

          if (container)
            _gtk_container_cell_accessible_add_child (container, cell);
          else
            cell->refresh_index = refresh_cell_index;

          update_cell_value (renderer_cell, accessible, FALSE);

          /* Add the actions appropriate for this cell */
          add_cell_actions (cell, editable);

          /* Set state if it is expandable */
          if (is_expander)
            {
              set_cell_expandable (cell);
              if (is_expanded)
                _gtk_cell_accessible_add_state (cell, ATK_STATE_EXPANDED, FALSE);
            }

          /* If the column is visible, sets the cell's state */
          if (gtk_tree_view_column_get_visible (tv_col))
            set_cell_visibility (tree_view, cell, tv_col, path, FALSE);

          /* If the row is selected, all cells on the row are selected */
          selection = gtk_tree_view_get_selection (tree_view);

          if (gtk_tree_selection_path_is_selected (selection, path))
            _gtk_cell_accessible_add_state (cell, ATK_STATE_SELECTED, FALSE);

          _gtk_cell_accessible_add_state (cell, ATK_STATE_FOCUSABLE, FALSE);
          if (focus_index == i)
            {
              accessible->focus_cell = g_object_ref (cell);
              _gtk_cell_accessible_add_state (cell, ATK_STATE_FOCUSED, FALSE);
              g_signal_emit_by_name (accessible, "active-descendant-changed", cell);
            }
        }
      g_list_free (renderer_list);
      if (container)
        child = ATK_OBJECT (container);
    }

  if (expander_tv == tv_col)
    {
      AtkRelationSet *relation_set;
      AtkObject *accessible_array[1];
      AtkRelation* relation;
      AtkObject *parent_node;

      relation_set = atk_object_ref_relation_set (ATK_OBJECT (child));

      gtk_tree_path_up (path);
      if (gtk_tree_path_get_depth (path) == 0)
        parent_node = obj;
      else
        {
          gint parent_index;

          parent_index = get_index (tree_view, path, i % accessible->n_cols);
          parent_node = atk_object_ref_accessible_child (obj, parent_index);
        }
      accessible_array[0] = parent_node;
      relation = atk_relation_new (accessible_array, 1,
                                   ATK_RELATION_NODE_CHILD_OF);
      atk_relation_set_add (relation_set, relation);
      atk_object_add_relationship (parent_node, ATK_RELATION_NODE_PARENT_OF, child);
      g_object_unref (relation);
      g_object_unref (relation_set);
    }
  gtk_tree_path_free (path);

  /* We do not increase the reference count here; when g_object_unref()
   * is called for the cell then cell_destroyed() is called and this
   * removes the cell from the cache.
   */
  return child;
}

static AtkStateSet*
gtk_tree_view_accessible_ref_state_set (AtkObject *obj)
{
  AtkStateSet *state_set;
  GtkWidget *widget;

  state_set = ATK_OBJECT_CLASS (_gtk_tree_view_accessible_parent_class)->ref_state_set (obj);
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));

  if (widget != NULL)
    atk_state_set_add_state (state_set, ATK_STATE_MANAGES_DESCENDANTS);

  return state_set;
}

static void
_gtk_tree_view_accessible_class_init (GtkTreeViewAccessibleClass *klass)
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

  accessible_class->connect_widget_destroyed = gtk_tree_view_accessible_connect_widget_destroyed;

  /* The children of a GtkTreeView are the buttons at the top of the columns
   * we do not represent these as children so we do not want to report
   * children added or deleted when these changed.
   */
  container_class->add_gtk = NULL;
  container_class->remove_gtk = NULL;

  gobject_class->finalize = gtk_tree_view_accessible_finalize;
}

static void
_gtk_tree_view_accessible_init (GtkTreeViewAccessible *view)
{
}

gint
get_focus_index (GtkTreeView *tree_view)
{
  GtkTreePath *focus_path;
  GtkTreeViewColumn *focus_column;
  gint index;

  gtk_tree_view_get_cursor (tree_view, &focus_path, &focus_column);
  if (focus_path && focus_column)
    index = get_index (tree_view, focus_path,
                       get_column_number (tree_view, focus_column, FALSE));
  else
    index = -1;

  if (focus_path)
    gtk_tree_path_free (focus_path);

  return index;
}

/* This function returns a reference to the accessible object
 * for the cell in the treeview which has focus, if any
 */
static AtkObject *
gtk_tree_view_accessible_ref_focus_cell (GtkTreeView *tree_view)
{
  AtkObject *focus_cell = NULL;
  AtkObject *atk_obj;
  gint focus_index;

  focus_index = get_focus_index (tree_view);
  if (focus_index >= 0)
    {
      atk_obj = gtk_widget_get_accessible (GTK_WIDGET (tree_view));
      focus_cell = atk_object_ref_accessible_child (atk_obj, focus_index);
    }

  return focus_cell;
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
  GtkTreeViewColumn *tv_column;
  gint x_pos, y_pos;
  gint bx, by;
  gboolean ret_val;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (component));
  if (widget == NULL)
    return NULL;

  tree_view = GTK_TREE_VIEW (widget);

  atk_component_get_extents (component, &x_pos, &y_pos, NULL, NULL, coord_type);
  gtk_tree_view_convert_widget_to_bin_window_coords (tree_view, x, y, &bx, &by);
  ret_val = gtk_tree_view_get_path_at_pos (tree_view,
                                           bx - x_pos, by - y_pos,
                                           &path, &tv_column, NULL, NULL);
  if (ret_val)
    {
      gint index, column;

      column = get_column_number (tree_view, tv_column, FALSE);
      index = get_index (tree_view, path, column);
      gtk_tree_path_free (path);

      return gtk_tree_view_accessible_ref_child (ATK_OBJECT (component), index);
    }

  return NULL;
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
  GtkTreeView *tree_view;
  gint actual_column;
  gint n_cols, n_rows;
  GtkTreeIter iter;
  GtkTreePath *path;
  gint index;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (table));
  if (widget == NULL)
    return -1;

  n_cols = atk_table_get_n_columns (table);
  n_rows = atk_table_get_n_rows (table);

  if (row >= n_rows || column >= n_cols)
    return -1;

  tree_view = GTK_TREE_VIEW (widget);
  actual_column = get_actual_column_number (tree_view, column);

  set_iter_nth_row (tree_view, &iter, row);
  path = gtk_tree_model_get_path (gtk_tree_view_get_model (tree_view), &iter);

  index = get_index (tree_view, path, actual_column);
  gtk_tree_path_free (path);

  return index;
}

static gint
gtk_tree_view_accessible_get_column_at_index (AtkTable *table,
                                              gint      index)
{
  GtkWidget *widget;
  GtkTreeView *tree_view;
  gint n_columns;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (table));
  if (widget == NULL)
    return -1;

  tree_view = GTK_TREE_VIEW (widget);
  n_columns = GTK_TREE_VIEW_ACCESSIBLE (table)->n_cols;

  if (n_columns == 0)
    return 0;

  index = index % n_columns;

  return get_visible_column_number (tree_view, index);
}

static gint
gtk_tree_view_accessible_get_row_at_index (AtkTable *table,
                                           gint      index)
{
  GtkWidget *widget;
  GtkTreeView *tree_view;
  GtkTreePath *path;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (table));
  if (widget == NULL)
    return -1;

  tree_view = GTK_TREE_VIEW (widget);
  if (get_path_column_from_index (tree_view, index, &path, NULL))
    {
      gint row = get_row_from_tree_path (tree_view, path);
      gtk_tree_path_free (path);
      return row;
    }
  else
    return -1;
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
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  gint n_rows;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (table));
  if (widget == NULL)
    return 0;

  tree_view = GTK_TREE_VIEW (widget);
  tree_model = gtk_tree_view_get_model (tree_view);

  if (!tree_model)
    n_rows = 0;
  else if (gtk_tree_model_get_flags (tree_model) & GTK_TREE_MODEL_LIST_ONLY)
    /* No collapsed rows in a list store */
    n_rows = GTK_TREE_VIEW_ACCESSIBLE (table)->n_rows;
  else
    {
      GtkTreePath *root_tree;

      n_rows = 0;
      root_tree = gtk_tree_path_new_first ();
      iterate_thru_children (tree_view, tree_model,
                             root_tree, NULL, &n_rows, 0);
      gtk_tree_path_free (root_tree);
    }

  return n_rows;
}

static gint
gtk_tree_view_accessible_get_n_columns (AtkTable *table)
{
  GtkWidget *widget;
  GtkTreeView *tree_view;
  GtkTreeViewColumn *tv_col;
  gint n_cols = 0;
  gint i = 0;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (table));
  if (widget == NULL)
    return 0;

  tree_view = GTK_TREE_VIEW (widget);
  tv_col = gtk_tree_view_get_column (tree_view, i);

  while (tv_col != NULL)
    {
      if (gtk_tree_view_column_get_visible (tv_col))
        n_cols++;

      i++;
      tv_col = gtk_tree_view_get_column (tree_view, i);
    }

  return n_cols;
}

static gboolean
gtk_tree_view_accessible_is_row_selected (AtkTable *table,
                                          gint      row)
{
  GtkWidget *widget;
  GtkTreeView *tree_view;
  GtkTreeSelection *selection;
  GtkTreeIter iter;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (table));
  if (widget == NULL)
    return FALSE;

  if (row < 0)
    return FALSE;

  tree_view = GTK_TREE_VIEW (widget);
  selection = gtk_tree_view_get_selection (tree_view);

  set_iter_nth_row (tree_view, &iter, row);
  return gtk_tree_selection_iter_is_selected (selection, &iter);
}

static gboolean
gtk_tree_view_accessible_is_selected (AtkTable *table,
                                      gint      row,
                                      gint      column)
{
  return gtk_tree_view_accessible_is_row_selected (table, row);
}

static void
get_selected_rows (GtkTreeModel *model,
                   GtkTreePath  *path,
                   GtkTreeIter  *iter,
                   gpointer      data)
{
  GPtrArray *array = (GPtrArray *)data;

  g_ptr_array_add (array, gtk_tree_path_copy (path));
}

static gint
gtk_tree_view_accessible_get_selected_rows (AtkTable  *table,
                                            gint     **rows_selected)
{
  GtkWidget *widget;
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  GtkTreeIter iter;
  GtkTreeSelection *selection;
  GtkTreePath *tree_path;
  gint ret_val = 0;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (table));
  if (widget == NULL)
    return 0;

  tree_view = GTK_TREE_VIEW (widget);
  selection = gtk_tree_view_get_selection (tree_view);

  switch (gtk_tree_selection_get_mode (selection))
    {
    case GTK_SELECTION_SINGLE:
    case GTK_SELECTION_BROWSE:
      if (gtk_tree_selection_get_selected (selection, &tree_model, &iter))
        {
          gint row;

          if (rows_selected)
            {
              *rows_selected = g_new (gint, 1);
              tree_path = gtk_tree_model_get_path (tree_model, &iter);
              row = get_row_from_tree_path (tree_view, tree_path);
              gtk_tree_path_free (tree_path);

              /* shouldn't ever happen */
              g_return_val_if_fail (row != -1, 0);

              *rows_selected[0] = row;
            }
          ret_val = 1;
        }
      break;
    case GTK_SELECTION_MULTIPLE:
      {
        GPtrArray *array = g_ptr_array_new();

        gtk_tree_selection_selected_foreach (selection, get_selected_rows, array);
        ret_val = array->len;

        if (rows_selected && ret_val)
          {
            gint i;

            *rows_selected = g_new (gint, ret_val);
            for (i = 0; i < ret_val; i++)
              {
                gint row;

                tree_path = (GtkTreePath *) g_ptr_array_index (array, i);
                row = get_row_from_tree_path (tree_view, tree_path);
                gtk_tree_path_free (tree_path);
                (*rows_selected)[i] = row;
              }
          }
        g_ptr_array_free (array, FALSE);
      }
      break;
    case GTK_SELECTION_NONE:
      break;
    }
  return ret_val;
}

static gboolean
gtk_tree_view_accessible_add_row_selection (AtkTable *table,
                                            gint      row)
{
  GtkWidget *widget;
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  GtkTreeSelection *selection;
  GtkTreePath *tree_path;
  GtkTreeIter iter_to_row;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (table));
  if (widget == NULL)
    return FALSE;

  if (!gtk_tree_view_accessible_is_row_selected (table, row))
    {
      tree_view = GTK_TREE_VIEW (widget);
      tree_model = gtk_tree_view_get_model (tree_view);
      selection = gtk_tree_view_get_selection (tree_view);

      if (gtk_tree_model_get_flags (tree_model) & GTK_TREE_MODEL_LIST_ONLY)
        {
          tree_path = gtk_tree_path_new ();
          gtk_tree_path_append_index (tree_path, row);
          gtk_tree_selection_select_path (selection,tree_path);
          gtk_tree_path_free (tree_path);
        }
      else
        {
          set_iter_nth_row (tree_view, &iter_to_row, row);
          gtk_tree_selection_select_iter (selection, &iter_to_row);
        }
    }

  return gtk_tree_view_accessible_is_row_selected (table, row);
}

static gboolean
gtk_tree_view_accessible_remove_row_selection (AtkTable *table,
                                               gint      row)
{
  GtkWidget *widget;
  GtkTreeView *tree_view;
  GtkTreeSelection *selection;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (table));
  if (widget == NULL)
    return FALSE;

  tree_view = GTK_TREE_VIEW (widget);
  selection = gtk_tree_view_get_selection (tree_view);

  if (gtk_tree_view_accessible_is_row_selected (table, row))
    {
      gtk_tree_selection_unselect_all (selection);
      return TRUE;
    }

  return FALSE;
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
  tv_col = get_column (tree_view, in_col);
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
  tv_col = get_column (tree_view, in_col);
  if (tv_col == NULL)
     return NULL;

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
  if (i >= n_columns * n_selected)
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

  row = atk_table_get_row_at_index (ATK_TABLE (selection), i);

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

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (parent));
  if (widget == NULL)
    return;

  tree_view = GTK_TREE_VIEW (widget);
  parent_cell = atk_object_get_parent (ATK_OBJECT (cell));
  if (parent_cell != ATK_OBJECT (parent))
    top_cell = GTK_CELL_ACCESSIBLE (parent_cell);
  else
    top_cell = cell;
  cell_info = find_cell_info (GTK_TREE_VIEW_ACCESSIBLE (parent), top_cell, TRUE);
  if (!cell_info || !cell_info->cell_col_ref || !cell_info->cell_row_ref)
    return;
  path = gtk_tree_row_reference_get_path (cell_info->cell_row_ref);
  tv_col = cell_info->cell_col_ref;
  if (path && cell_info->in_use)
    {
      GtkTreeViewColumn *expander_column;
      gint focus_line_width;

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
      gtk_widget_style_get (widget,
                            "focus-line-width", &focus_line_width,
                            NULL);

      cell_rect->x += focus_line_width;
      cell_rect->width -= 2 * focus_line_width;

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

  cell_info = find_cell_info (GTK_TREE_VIEW_ACCESSIBLE (parent), cell, TRUE);
  if (!cell_info || !cell_info->cell_col_ref || !cell_info->cell_row_ref)
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
      if (cell_info->in_use)
        {
          index = atk_object_get_index_in_parent (cell_object);
          renderer = g_list_nth_data (renderers, index);
        }
      g_list_free (renderers);
    }
  path = gtk_tree_row_reference_get_path (cell_info->cell_row_ref);
  if (path && cell_info->in_use)
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

static void
gtk_cell_accessible_parent_interface_init (GtkCellAccessibleParentIface *iface)
{
  iface->get_cell_extents = gtk_tree_view_accessible_get_cell_extents;
  iface->get_cell_area = gtk_tree_view_accessible_get_cell_area;
  iface->grab_focus = gtk_tree_view_accessible_grab_cell_focus;
}

/* signal handling */

static gboolean
idle_expand_row (gpointer data)
{
  GtkTreeViewAccessible *accessible = data;
  GtkTreePath *path;
  GtkTreeView *tree_view;
  GtkTreeIter iter;
  GtkTreeModel *tree_model;
  gint n_inserted, row;

  accessible->idle_expand_id = 0;

  path = accessible->idle_expand_path;
  tree_view = GTK_TREE_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible)));

  tree_model = gtk_tree_view_get_model (tree_view);
  if (!tree_model)
    return FALSE;

  if (!path || !gtk_tree_model_get_iter (tree_model, &iter, path))
    return FALSE;

  /* Update visibility of cells below expansion row */
  traverse_cells (accessible, path, FALSE, FALSE);

  /* Figure out number of visible children, the following test
   * should not fail
   */
  if (gtk_tree_model_iter_has_child (tree_model, &iter))
    {
      GtkTreePath *path_copy;

      /* By passing path into this function, we find the number of
       * visible children of path.
       */
      path_copy = gtk_tree_path_copy (path);
      gtk_tree_path_append_index (path_copy, 0);

      n_inserted = 0;
      iterate_thru_children (tree_view, tree_model,
                             path_copy, NULL, &n_inserted, 0);
      gtk_tree_path_free (path_copy);
    }
  else
    {
      /* We can get here if the row expanded callback deleted the row */
      return FALSE;
    }

  /* Set expand state */
  set_expand_state (tree_view, tree_model, accessible, path, TRUE);

  row = get_row_from_tree_path (tree_view, path);

  /* shouldn't ever happen */
  if (row == -1)
    g_assert_not_reached ();

  /* Must add 1 because the "added rows" are below the row being expanded */
  row += 1;

  g_signal_emit_by_name (accessible, "row-inserted", row, n_inserted);

  accessible->idle_expand_path = NULL;

  gtk_tree_path_free (path);

  return FALSE;
}

static gboolean
row_expanded_cb (GtkTreeView *tree_view,
                 GtkTreeIter *iter,
                 GtkTreePath *path)
{
  AtkObject *atk_obj;
  GtkTreeViewAccessible *accessible;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (tree_view));
  accessible = GTK_TREE_VIEW_ACCESSIBLE (atk_obj);

  /*
   * The visible rectangle has not been updated when this signal is emitted
   * so we process the signal when the GTK processing is completed
   */
  /* this seems wrong since it overwrites any other pending expand handlers... */
  accessible->idle_expand_path = gtk_tree_path_copy (path);
  if (accessible->idle_expand_id)
    g_source_remove (accessible->idle_expand_id);
  accessible->idle_expand_id = gdk_threads_add_idle (idle_expand_row, accessible);

  return FALSE;
}

static gboolean
row_collapsed_cb (GtkTreeView *tree_view,
                  GtkTreeIter *iter,
                  GtkTreePath *path)
{
  GtkTreeModel *tree_model;
  AtkObject *atk_obj;
  GtkTreeViewAccessible *accessible;
  gint row;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (tree_view));
  accessible = GTK_TREE_VIEW_ACCESSIBLE (atk_obj);
  tree_model = gtk_tree_view_get_model (tree_view);

  clean_rows (accessible);

  /* Update visibility of cells below collapsed row */
  traverse_cells (accessible, path, FALSE, FALSE);

  /* Set collapse state */
  set_expand_state (tree_view, tree_model, accessible, path, FALSE);
  if (accessible->n_children_deleted == 0)
    return FALSE;
  row = get_row_from_tree_path (tree_view, path);
  if (row == -1)
    return FALSE;
  g_signal_emit_by_name (atk_obj, "row-deleted", row,
                         accessible->n_children_deleted);
  accessible->n_children_deleted = 0;
  return FALSE;
}

static void
size_allocate_cb (GtkWidget     *widget,
                  GtkAllocation *allocation)
{
  AtkObject *atk_obj;
  GtkTreeViewAccessible *accessible;

  atk_obj = gtk_widget_get_accessible (widget);
  accessible = GTK_TREE_VIEW_ACCESSIBLE (atk_obj);

  /* If the size allocation changes, the visibility of cells
   * may change so update the cells visibility.
   */
  traverse_cells (accessible, NULL, FALSE, FALSE);
}

static void
selection_changed_cb (GtkTreeSelection *selection,
                      gpointer          data)
{
  GtkTreeViewAccessible *accessible;
  GtkTreeView *tree_view;
  GtkWidget *widget;
  GtkTreeViewAccessibleCellInfo *info;
  GtkTreeSelection *tree_selection;
  GtkTreePath *path;
  GHashTableIter iter;

  accessible = GTK_TREE_VIEW_ACCESSIBLE (data);
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return;

  tree_view = GTK_TREE_VIEW (widget);
  tree_selection = gtk_tree_view_get_selection (tree_view);

  clean_rows (accessible);

  /* FIXME: clean rows iterates through all cells too */
  g_hash_table_iter_init (&iter, accessible->cell_info_by_index);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&info))
    {
      if (info->in_use)
        {
          _gtk_cell_accessible_remove_state (info->cell, ATK_STATE_SELECTED, TRUE);

          path = gtk_tree_row_reference_get_path (info->cell_row_ref);
          if (path && gtk_tree_selection_path_is_selected (tree_selection, path))
            _gtk_cell_accessible_add_state (info->cell, ATK_STATE_SELECTED, TRUE);
          gtk_tree_path_free (path);
        }
    }
  if (gtk_widget_get_realized (widget))
    g_signal_emit_by_name (accessible, "selection-changed");
}

static void
columns_changed (GtkTreeView *tree_view)
{
  AtkObject *atk_obj;
  GtkTreeViewAccessible *accessible;
  GList *tv_cols, *tmp_list;
  gboolean column_found;
  gboolean move_found = FALSE;
  gboolean stale_set = FALSE;
  gint column_count = 0;
  gint i;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET(tree_view));
  accessible = GTK_TREE_VIEW_ACCESSIBLE (atk_obj);

  /* This function must determine if the change is an add, delete
   * or a move based upon its cache of TreeViewColumns in
   * accessible->col_data
   */
  tv_cols = gtk_tree_view_get_columns (tree_view);
  accessible->n_cols = g_list_length (tv_cols);

  /* check for adds or moves */
  for (tmp_list = tv_cols; tmp_list; tmp_list = tmp_list->next)
    {
      column_found = FALSE;

      for (i = 0; i < accessible->col_data->len; i++)
        {

          if ((GtkTreeViewColumn *)tmp_list->data ==
              (GtkTreeViewColumn *)g_array_index (accessible->col_data,
               GtkTreeViewColumn *, i))
            {
              column_found = TRUE;

              /* If the column isn't in the same position, a move happened */
              if (!move_found && i != column_count)
                {
                  if (!stale_set)
                    {
                      /* Set all rows to ATK_STATE_STALE */
                      traverse_cells (accessible, NULL, TRUE, FALSE);
                      stale_set = TRUE;
                    }

                  /* Just emit one column reordered signal when a move happens */
                  g_signal_emit_by_name (atk_obj, "column-reordered");
                  move_found = TRUE;
                }

              break;
            }
        }

     /* If column_found is FALSE, then an insert happened for column
      * number column_count
      */
      if (!column_found)
        {
          gint row;

          if (!stale_set)
            {
              /* Set all rows to ATK_STATE_STALE */
              traverse_cells (accessible, NULL, TRUE, FALSE);
              stale_set = TRUE;
            }

          /* Generate column-inserted signal */
          g_signal_emit_by_name (atk_obj, "column-inserted", column_count, 1);

          /* Generate children-changed signals */
          for (row = 0; row < accessible->n_rows; row++)
            {
             /* Pass NULL as the child object, i.e. 4th argument */
              g_signal_emit_by_name (atk_obj, "children-changed::add",
                                    ((row * accessible->n_cols) + column_count), NULL, NULL);
            }
        }

      column_count++;
    }

  /* check for deletes */
  for (i = 0; i < accessible->col_data->len; i++)
    {
      column_found = FALSE;

      for (tmp_list = tv_cols; tmp_list; tmp_list = tmp_list->next)
        {
            if ((GtkTreeViewColumn *)tmp_list->data ==
                (GtkTreeViewColumn *)g_array_index (accessible->col_data,
                 GtkTreeViewColumn *, i))
              {
                column_found = TRUE;
                break;
              }
        }

       /* If column_found is FALSE, then a delete happened for column
        * number i
        */
      if (!column_found)
        {
          gint row;

          clean_cols (accessible,
                      (GtkTreeViewColumn *)g_array_index (accessible->col_data,
                      GtkTreeViewColumn *, i));

          if (!stale_set)
            {
              /* Set all rows to ATK_STATE_STALE */
              traverse_cells (accessible, NULL, TRUE, FALSE);
              stale_set = TRUE;
            }

          /* Generate column-deleted signal */
          g_signal_emit_by_name (atk_obj, "column-deleted", i, 1);

          /* Generate children-changed signals */
          for (row = 0; row < accessible->n_rows; row++)
            {
              /* Pass NULL as the child object, 4th argument */
              g_signal_emit_by_name (atk_obj, "children-changed::remove",
                                     ((row * accessible->n_cols) + column_count), NULL, NULL);
            }
        }
    }

  /* rebuild the array */
  g_array_free (accessible->col_data, TRUE);
  accessible->col_data = g_array_sized_new (FALSE, TRUE, sizeof (GtkTreeViewColumn *), 0);

  for (tmp_list = tv_cols; tmp_list; tmp_list = tmp_list->next)
    g_array_append_val (accessible->col_data, tmp_list->data);
  g_list_free (tv_cols);
}

static gint
idle_cursor_changed (gpointer data)
{
  GtkTreeViewAccessible *accessible = GTK_TREE_VIEW_ACCESSIBLE (data);
  GtkTreeView *tree_view;
  GtkWidget *widget;
  AtkObject *cell;

  accessible->idle_cursor_changed_id = 0;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return FALSE;

  tree_view = GTK_TREE_VIEW (widget);

  cell = gtk_tree_view_accessible_ref_focus_cell (tree_view);
  if (cell)
    {
      if (cell != accessible->focus_cell)
        {
          if (accessible->focus_cell)
            {
              _gtk_cell_accessible_remove_state (GTK_CELL_ACCESSIBLE (accessible->focus_cell), ATK_STATE_ACTIVE, FALSE);
              _gtk_cell_accessible_remove_state (GTK_CELL_ACCESSIBLE (accessible->focus_cell), ATK_STATE_FOCUSED, FALSE);
              g_object_unref (accessible->focus_cell);
              accessible->focus_cell = cell;
            }

          if (gtk_widget_has_focus (widget))
            {
              _gtk_cell_accessible_add_state (GTK_CELL_ACCESSIBLE (cell), ATK_STATE_ACTIVE, FALSE);
              _gtk_cell_accessible_add_state (GTK_CELL_ACCESSIBLE (cell), ATK_STATE_FOCUSED, FALSE);
            }

          g_signal_emit_by_name (accessible, "active-descendant-changed", cell);
        }
      else
        g_object_unref (cell);
    }

  return FALSE;
}

static void
cursor_changed (GtkTreeView *tree_view)
{
  GtkTreeViewAccessible *accessible;

  accessible = GTK_TREE_VIEW_ACCESSIBLE (gtk_widget_get_accessible (GTK_WIDGET (tree_view)));
  if (accessible->idle_cursor_changed_id != 0)
    return;

  /* We notify the focus change in a idle handler so that the processing
   * of the cursor change is completed when the focus handler is called.
   * This will allow actions to be called in the focus handler
   */
  accessible->idle_cursor_changed_id = gdk_threads_add_idle (idle_cursor_changed, accessible);
}

static gboolean
focus_in (GtkWidget *widget)
{
  GtkTreeView *tree_view;
  GtkTreeViewAccessible *accessible;
  AtkStateSet *state_set;
  AtkObject *cell;

  tree_view = GTK_TREE_VIEW (widget);
  accessible = GTK_TREE_VIEW_ACCESSIBLE (gtk_widget_get_accessible (widget));

  if (accessible->focus_cell == NULL)
    {
      cell = gtk_tree_view_accessible_ref_focus_cell (tree_view);
      if (cell)
        {
          state_set = atk_object_ref_state_set (cell);
          if (state_set)
            {
              if (!atk_state_set_contains_state (state_set, ATK_STATE_FOCUSED))
                {
                  _gtk_cell_accessible_add_state (GTK_CELL_ACCESSIBLE (cell), ATK_STATE_ACTIVE, FALSE);
                  accessible->focus_cell = cell;
                  _gtk_cell_accessible_add_state (GTK_CELL_ACCESSIBLE (cell), ATK_STATE_FOCUSED, FALSE);
                  g_signal_emit_by_name (accessible, "active-descendant-changed", cell);
                }
              g_object_unref (state_set);
            }
        }
    }
  return FALSE;
}

static gboolean
focus_out (GtkWidget *widget)
{
  GtkTreeViewAccessible *accessible;

  accessible = GTK_TREE_VIEW_ACCESSIBLE (gtk_widget_get_accessible (widget));
  if (accessible->focus_cell)
    {
      _gtk_cell_accessible_remove_state (GTK_CELL_ACCESSIBLE (accessible->focus_cell), ATK_STATE_ACTIVE, FALSE);
      _gtk_cell_accessible_remove_state (GTK_CELL_ACCESSIBLE (accessible->focus_cell), ATK_STATE_FOCUSED, FALSE);
      g_object_unref (accessible->focus_cell);
      accessible->focus_cell = NULL;
    }
  return FALSE;
}

static void
model_row_changed (GtkTreeModel *tree_model,
                   GtkTreePath  *path,
                   GtkTreeIter  *iter,
                   gpointer      user_data)
{
  GtkTreeView *tree_view = GTK_TREE_VIEW (user_data);
  GtkTreeViewAccessible *accessible;
  GtkTreePath *cell_path;
  GtkTreeViewAccessibleCellInfo *cell_info;
  GHashTableIter hash_iter;

  accessible = GTK_TREE_VIEW_ACCESSIBLE (gtk_widget_get_accessible (GTK_WIDGET (tree_view)));

  /* Loop through our cached cells */
  /* Must loop through them all */
  g_hash_table_iter_init (&hash_iter, accessible->cell_info_by_index);
  while (g_hash_table_iter_next (&hash_iter, NULL, (gpointer *)&cell_info))
    {
      if (cell_info->in_use)
        {
          cell_path = gtk_tree_row_reference_get_path (cell_info->cell_row_ref);

          if (cell_path != NULL)
            {
              if (path && gtk_tree_path_compare (cell_path, path) == 0)
                {
                  if (GTK_IS_RENDERER_CELL_ACCESSIBLE (cell_info->cell))
                    update_cell_value (GTK_RENDERER_CELL_ACCESSIBLE (cell_info->cell),
                                       accessible, TRUE);
                }
              gtk_tree_path_free (cell_path);
            }
        }
    }
  g_signal_emit_by_name (accessible, "visible-data-changed");
}

static void
column_visibility_changed (GObject    *object,
                           GParamSpec *pspec,
                           gpointer    user_data)
{
  if (g_strcmp0 (pspec->name, "visible") == 0)
    {
      /* A column has been made visible or invisible
       * We update our cache of cells and emit model_changed signal
       */
      GtkTreeView *tree_view = (GtkTreeView *)user_data;
      GtkTreeViewAccessible *accessible;
      GtkTreeViewAccessibleCellInfo *cell_info;
      GtkTreeViewColumn *this_col = GTK_TREE_VIEW_COLUMN (object);
      GtkTreeViewColumn *tv_col;
      GHashTableIter iter;

      accessible = GTK_TREE_VIEW_ACCESSIBLE (gtk_widget_get_accessible (GTK_WIDGET (tree_view))
);
      g_signal_emit_by_name (accessible, "model-changed");

      g_hash_table_iter_init (&iter, accessible->cell_info_by_index);
      while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&cell_info))
        {
          if (cell_info->in_use)
            {
              tv_col = cell_info->cell_col_ref;
              if (tv_col == this_col)
                {
                  GtkTreePath *row_path;
                  row_path = gtk_tree_row_reference_get_path (cell_info->cell_row_ref);
                  if (GTK_IS_RENDERER_CELL_ACCESSIBLE (cell_info->cell))
                    {
                      if (gtk_tree_view_column_get_visible (tv_col))
                          set_cell_visibility (tree_view,
                                               cell_info->cell,
                                               tv_col, row_path, FALSE);
                      else
                        {
                          _gtk_cell_accessible_remove_state (cell_info->cell, ATK_STATE_VISIBLE, TRUE);
                          _gtk_cell_accessible_remove_state (cell_info->cell, ATK_STATE_SHOWING, TRUE);
                        }
                    }
                  gtk_tree_path_free (row_path);
                }
            }
        }
    }
}

static void
model_row_inserted (GtkTreeModel *tree_model,
                    GtkTreePath  *path,
                    GtkTreeIter  *iter,
                    gpointer      user_data)
{
  GtkTreeView *tree_view = (GtkTreeView *)user_data;
  AtkObject *atk_obj;
  GtkTreeViewAccessible *accessible;
  GtkTreePath *path_copy;
  gint row, n_inserted, child_row;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (tree_view));
  accessible = GTK_TREE_VIEW_ACCESSIBLE (atk_obj);
  accessible->n_rows++;

  if (accessible->idle_expand_id)
    {
      g_source_remove (accessible->idle_expand_id);
      accessible->idle_expand_id = 0;

      /* don't do this if the insertion precedes the idle path,
       * since it will now be invalid
       */
      if (path && accessible->idle_expand_path &&
          (gtk_tree_path_compare (path, accessible->idle_expand_path) > 0))
          set_expand_state (tree_view, tree_model, accessible, accessible->idle_expand_path, FALSE);
      if (accessible->idle_expand_path)
          gtk_tree_path_free (accessible->idle_expand_path);
    }
  /* Check to see if row is visible */
  row = get_row_from_tree_path (tree_view, path);

 /* A row insert is not necessarily visible.  For example,
  * a row can be draged & dropped into another row, which
  * causes an insert on the model that isn't visible in the
  * view.  Only generate a signal if the inserted row is
  * visible.
  */
  if (row != -1)
    {
      GtkTreeIter tmp_iter;
      gint n_cols, col;

      gtk_tree_model_get_iter (tree_model, &tmp_iter, path);

      /* Figure out number of visible children. */
      if (gtk_tree_model_iter_has_child (tree_model, &tmp_iter))
        {
          GtkTreePath *path2;
         /*
          * By passing path into this function, we find the number of
          * visible children of path.
          */
          n_inserted = 0;
          /* iterate_thru_children modifies path, we don't want that, so give
           * it a copy */
          path2 = gtk_tree_path_copy (path);
          iterate_thru_children (tree_view, tree_model,
                                 path2, NULL, &n_inserted, 0);
          gtk_tree_path_free (path2);

          /* Must add one to include the row that is being added */
          n_inserted++;
        }
      else
        n_inserted = 1;

      /* Set rows below the inserted row to ATK_STATE_STALE */
      traverse_cells (accessible, path, TRUE, TRUE);

      /* Generate row-inserted signal */
      g_signal_emit_by_name (atk_obj, "row-inserted", row, n_inserted);

      /* Generate children-changed signals */
      n_cols = gtk_tree_view_accessible_get_n_columns (ATK_TABLE (atk_obj));
      for (child_row = row; child_row < (row + n_inserted); child_row++)
        {
          for (col = 0; col < n_cols; col++)
            {
             /* Pass NULL as the child object, i.e. 4th argument */
              g_signal_emit_by_name (atk_obj, "children-changed::add",
                                    ((row * n_cols) + col), NULL, NULL);
            }
        }
    }
  else
    {
     /* The row has been inserted inside another row.  This can
      * cause a row that previously couldn't be expanded to now
      * be expandable.
      */
      path_copy = gtk_tree_path_copy (path);
      gtk_tree_path_up (path_copy);
      set_expand_state (tree_view, tree_model, accessible, path_copy, TRUE);
      gtk_tree_path_free (path_copy);
    }
}

static void
model_row_deleted (GtkTreeModel *tree_model,
                   GtkTreePath  *path,
                   gpointer      user_data)
{
  GtkTreeView *tree_view = (GtkTreeView *)user_data;
  GtkTreePath *path_copy;
  AtkObject *atk_obj;
  GtkTreeViewAccessible *accessible;
  gint row, col;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (tree_view));
  accessible = GTK_TREE_VIEW_ACCESSIBLE (atk_obj);

  accessible->n_rows--;

  if (accessible->idle_expand_id)
    {
      g_source_remove (accessible->idle_expand_id);
      gtk_tree_path_free (accessible->idle_expand_path);
      accessible->idle_expand_id = 0;
    }

  /* Check to see if row is visible */
  clean_rows (accessible);

  /* Set rows at or below the specified row to ATK_STATE_STALE */
  traverse_cells (accessible, path, TRUE, TRUE);

  /* If deleting a row with a depth > 1, then this may affect the
   * expansion/contraction of its parent(s). Make sure this is
   * handled.
   */
  if (gtk_tree_path_get_depth (path) > 1)
    {
      path_copy = gtk_tree_path_copy (path);
      gtk_tree_path_up (path_copy);
      set_expand_state (tree_view, tree_model, accessible, path_copy, TRUE);
      gtk_tree_path_free (path_copy);
    }
  row = get_row_from_tree_path (tree_view, path);

  /* If the row which is deleted is not visible because it is a child of
   * a collapsed row then row will be -1
   */
  if (row > 0)
    g_signal_emit_by_name (atk_obj, "row-deleted", row,
                           accessible->n_children_deleted + 1);
  accessible->n_children_deleted = 0;

  /* Generate children-changed signals */
  for (col = 0; col < accessible->n_cols; col++)
    {
      /* Pass NULL as the child object, 4th argument */
      g_signal_emit_by_name (atk_obj, "children-changed::remove",
                             ((row * accessible->n_cols) + col), NULL, NULL);
    }
}

/* This function gets called when a row is deleted or when rows are
 * removed from the view due to a collapse event. Note that the
 * count is the number of visible *children* of the deleted row,
 * so it does not include the row being deleted.
 *
 * As this function is called before the rows are removed we just note
 * the number of rows and then deal with it when we get a notification
 * that rows were deleted or collapsed.
 */
static void
destroy_count_func (GtkTreeView *tree_view,
                    GtkTreePath *path,
                    gint         count,
                    gpointer     user_data)
{
  AtkObject *atk_obj;
  GtkTreeViewAccessible *accessible;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (tree_view));
  accessible = GTK_TREE_VIEW_ACCESSIBLE (atk_obj);

  if (accessible->n_children_deleted != 0)
    return;

  accessible->n_children_deleted = count;
}

static void
model_rows_reordered (GtkTreeModel *tree_model,
                      GtkTreePath  *path,
                      GtkTreeIter  *iter,
                      gint         *new_order,
                      gpointer      user_data)
{
  GtkTreeView *tree_view = (GtkTreeView *)user_data;
  AtkObject *atk_obj;
  GtkTreeViewAccessible *accessible;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (tree_view));
  accessible = GTK_TREE_VIEW_ACCESSIBLE (atk_obj);

  if (accessible->idle_expand_id)
    {
      g_source_remove (accessible->idle_expand_id);
      gtk_tree_path_free (accessible->idle_expand_path);
      accessible->idle_expand_id = 0;
    }
  traverse_cells (accessible, NULL, TRUE, FALSE);

  g_signal_emit_by_name (atk_obj, "row-reordered");
}

static void
set_cell_visibility (GtkTreeView       *tree_view,
                     GtkCellAccessible *cell,
                     GtkTreeViewColumn *tv_col,
                     GtkTreePath       *tree_path,
                     gboolean           emit_signal)
{
  GdkRectangle cell_rect;

  /* Get these three values in tree coords */
  if (gtk_widget_get_realized (GTK_WIDGET (tree_view)))
    gtk_tree_view_get_cell_area (tree_view, tree_path, tv_col, &cell_rect);
  else
    cell_rect.height = 0;

  if (cell_rect.height > 0)
    {
      /* The height will be zero for a cell for which an antecedent
       * is not expanded
       */
      _gtk_cell_accessible_add_state (cell, ATK_STATE_VISIBLE, emit_signal);
      if (is_cell_showing (tree_view, &cell_rect))
        _gtk_cell_accessible_add_state (cell, ATK_STATE_SHOWING, emit_signal);
      else
        _gtk_cell_accessible_remove_state (cell, ATK_STATE_SHOWING, emit_signal);
    }
  else
    {
      _gtk_cell_accessible_remove_state (cell, ATK_STATE_VISIBLE, emit_signal);
      _gtk_cell_accessible_remove_state (cell, ATK_STATE_SHOWING, emit_signal);
    }
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

/* Misc Public */

/* This function is called when a cell's flyweight is created in
 * gtk_tree_view_accessible_table_ref_at with emit_change_signal
 * set to FALSE and in model_row_changed() on receipt of "row-changed"
 * signal when emit_change_signal is set to TRUE
 */
static gboolean
update_cell_value (GtkRendererCellAccessible      *renderer_cell,
                   GtkTreeViewAccessible *accessible,
                   gboolean               emit_change_signal)
{
  GtkTreeViewAccessibleCellInfo *cell_info;
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  GtkTreePath *path;
  GtkTreeIter iter;
  GList *renderers, *cur_renderer;
  GParamSpec *spec;
  GtkRendererCellAccessibleClass *renderer_cell_class;
  GtkCellRendererClass *gtk_cell_renderer_class;
  GtkCellAccessible *cell;
  gchar **prop_list;
  AtkObject *parent;
  gboolean is_expander, is_expanded;

  renderer_cell_class = GTK_RENDERER_CELL_ACCESSIBLE_GET_CLASS (renderer_cell);
  if (renderer_cell->renderer)
    gtk_cell_renderer_class = GTK_CELL_RENDERER_GET_CLASS (renderer_cell->renderer);
  else
    gtk_cell_renderer_class = NULL;

  prop_list = renderer_cell_class->property_list;

  cell = GTK_CELL_ACCESSIBLE (renderer_cell);
  cell_info = find_cell_info (accessible, cell, TRUE);
  if (!cell_info || !cell_info->cell_col_ref || !cell_info->cell_row_ref)
    return FALSE;

  if (emit_change_signal && cell_info->in_use)
    {
      tree_view = GTK_TREE_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible)));
      tree_model = gtk_tree_view_get_model (tree_view);
      path = gtk_tree_row_reference_get_path (cell_info->cell_row_ref);
      if (path == NULL)
        return FALSE;

      gtk_tree_model_get_iter (tree_model, &iter, path);
      is_expander = FALSE;
      is_expanded = FALSE;
      if (gtk_tree_model_iter_has_child (tree_model, &iter))
        {
          GtkTreeViewColumn *expander_tv;

          expander_tv = gtk_tree_view_get_expander_column (tree_view);
          if (expander_tv == cell_info->cell_col_ref)
            {
              is_expander = TRUE;
              is_expanded = gtk_tree_view_row_expanded (tree_view, path);
            }
        }
      gtk_tree_path_free (path);
      gtk_tree_view_column_cell_set_cell_data (cell_info->cell_col_ref,
                                               tree_model, &iter,
                                               is_expander, is_expanded);
    }
  renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (cell_info->cell_col_ref));
  if (!renderers)
    return FALSE;

  /* If the cell is in a container, its index is used to find the renderer
   * in the list. Otherwise, we assume that the cell is represented
   * by the first renderer in the list
   */

  if (cell_info->in_use)
    {
      parent = atk_object_get_parent (ATK_OBJECT (cell));

      if (GTK_IS_CONTAINER_CELL_ACCESSIBLE (parent))
        cur_renderer = g_list_nth (renderers, cell->index);
      else
        cur_renderer = renderers;
    }
  else
    return FALSE;

  if (cur_renderer == NULL)
    return FALSE;

  if (gtk_cell_renderer_class)
    {
      while (*prop_list)
        {
          spec = g_object_class_find_property
                           (G_OBJECT_CLASS (gtk_cell_renderer_class), *prop_list);

          if (spec != NULL)
            {
              GValue value = { 0, };

              g_value_init (&value, spec->value_type);
              g_object_get_property (cur_renderer->data, *prop_list, &value);
              g_object_set_property (G_OBJECT (renderer_cell->renderer),
                                     *prop_list, &value);
              g_value_unset (&value);
            }
          else
            g_warning ("Invalid property: %s\n", *prop_list);
          prop_list++;
        }
    }
  g_list_free (renderers);

  return _gtk_renderer_cell_accessible_update_cache (renderer_cell, emit_change_signal);
}

static gint
get_row_from_tree_path (GtkTreeView *tree_view,
                        GtkTreePath *path)
{
  GtkTreeModel *tree_model;
  GtkTreePath *root_tree;
  gint row;

  tree_model = gtk_tree_view_get_model (tree_view);

  if (gtk_tree_model_get_flags (tree_model) & GTK_TREE_MODEL_LIST_ONLY)
    row = gtk_tree_path_get_indices (path)[0];
  else
    {
      root_tree = gtk_tree_path_new_first ();
      row = 0;
      iterate_thru_children (tree_view, tree_model, root_tree, path, &row, 0);
      gtk_tree_path_free (root_tree);
    }

  return row;
}

/* Misc Private */

/*
 * Get the specified GtkTreeViewColumn in the GtkTreeView.
 * Only visible columns are considered.
 */
static GtkTreeViewColumn *
get_column (GtkTreeView *tree_view,
            gint         in_col)
{
  GtkTreeViewColumn *tv_col;
  gint n_cols = -1;
  gint i = 0;

  if (in_col < 0)
    {
       g_warning ("Request for invalid column %d\n", in_col);
       return NULL;
    }

  tv_col = gtk_tree_view_get_column (tree_view, i);
  while (tv_col != NULL)
    {
      if (gtk_tree_view_column_get_visible (tv_col))
        n_cols++;
      if (in_col == n_cols)
        break;
      tv_col = gtk_tree_view_get_column (tree_view, ++i);
    }

  if (in_col != n_cols)
    {
       g_warning ("Request for invalid column %d\n", in_col);
       return NULL;
    }
  return tv_col;
}

static gint
get_actual_column_number (GtkTreeView *tree_view,
                          gint         visible_column)
{
  GtkTreeViewColumn *tv_col;
  gint actual_column = 0;
  gint visible_columns = -1;

  /* This function calculates the column number which corresponds
   * to the specified visible column number
   */
  tv_col = gtk_tree_view_get_column (tree_view, actual_column);
  while (tv_col != NULL)
    {
      if (gtk_tree_view_column_get_visible (tv_col))
        visible_columns++;
      if (visible_columns == visible_column)
        return actual_column;
      tv_col = gtk_tree_view_get_column (tree_view, ++actual_column);
    }
  g_warning ("get_actual_column_number failed for %d\n", visible_column);
  return -1;
}

static gint
get_visible_column_number (GtkTreeView *tree_view,
                           gint         actual_column)
{
  GtkTreeViewColumn *tv_col;
  gint column = 0;
  gint visible_columns = -1;

  /* This function calculates the visible column number
   * which corresponds to the specified actual column number
   */
  tv_col = gtk_tree_view_get_column (tree_view, column);

  while (tv_col != NULL)
    {
      if (gtk_tree_view_column_get_visible (tv_col))
        {
          visible_columns++;
          if (actual_column == column)
            return visible_columns;
        }
      else
        if (actual_column == column)
          return -1;
      tv_col = gtk_tree_view_get_column (tree_view, ++column);
    }
  g_warning ("get_visible_column_number failed for %d\n", actual_column);
  return -1;
}

/* Helper recursive function that returns an iter to nth row
 */
static GtkTreeIter *
return_iter_nth_row (GtkTreeView  *tree_view,
                     GtkTreeModel *tree_model,
                     GtkTreeIter  *iter,
                     gint          increment,
                     gint          row)
{
  GtkTreePath *current_path;
  GtkTreeIter new_iter;
  gboolean row_expanded;

  current_path = gtk_tree_model_get_path (tree_model, iter);
  if (increment == row)
    {
      gtk_tree_path_free (current_path);
      return iter;
    }

  row_expanded = gtk_tree_view_row_expanded (tree_view, current_path);
  gtk_tree_path_free (current_path);

  new_iter = *iter;
  if ((row_expanded && gtk_tree_model_iter_children (tree_model, iter, &new_iter)) ||
      (gtk_tree_model_iter_next (tree_model, iter)) ||
      (gtk_tree_model_iter_parent (tree_model, iter, &new_iter) &&
          (gtk_tree_model_iter_next (tree_model, iter))))
    return return_iter_nth_row (tree_view, tree_model, iter,
      ++increment, row);

  return NULL;
}

static void
set_iter_nth_row (GtkTreeView *tree_view,
                  GtkTreeIter *iter,
                  gint         row)
{
  GtkTreeModel *tree_model;

  tree_model = gtk_tree_view_get_model (tree_view);
  gtk_tree_model_get_iter_first (tree_model, iter);
  iter = return_iter_nth_row (tree_view, tree_model, iter, 0, row);
}

/* Recursively called until the row specified by orig is found.
 *
 * *count will be set to the visible row number of the child
 * relative to the row that was initially passed in as tree_path.
 * tree_path could be modified by this function.
 *
 * *count will be -1 if orig is not found as a child (a row that is
 * not visible will not be found, e.g. if the row is inside a
 * collapsed row).  If NULL is passed in as orig, *count will
 * be a count of the visible children.
 *
 * NOTE: the value for depth must be 0 when this recursive function
 * is initially called, or it may not function as expected.
 */
static void
iterate_thru_children (GtkTreeView  *tree_view,
                       GtkTreeModel *tree_model,
                       GtkTreePath  *tree_path,
                       GtkTreePath  *orig,
                       gint         *count,
                       gint          depth)
{
  GtkTreeIter iter;

  if (!gtk_tree_model_get_iter (tree_model, &iter, tree_path))
    return;

  if (tree_path && orig && !gtk_tree_path_compare (tree_path, orig))
    /* Found it! */
    return;

  if (tree_path && orig && gtk_tree_path_compare (tree_path, orig) > 0)
    {
      /* Past it, so return -1 */
      *count = -1;
      return;
    }
  else if (gtk_tree_view_row_expanded (tree_view, tree_path) &&
    gtk_tree_model_iter_has_child (tree_model, &iter))
    {
      (*count)++;
      gtk_tree_path_append_index (tree_path, 0);
      iterate_thru_children (tree_view, tree_model, tree_path,
                             orig, count, (depth + 1));
      return;
    }
  else if (gtk_tree_model_iter_next (tree_model, &iter))
    {
      (*count)++;
      tree_path = gtk_tree_model_get_path (tree_model, &iter);
       if (tree_path)
         {
           iterate_thru_children (tree_view, tree_model, tree_path,
                                 orig, count, depth);
           gtk_tree_path_free (tree_path);
         }
      return;
  }
  else if (gtk_tree_path_up (tree_path))
    {
      GtkTreeIter temp_iter;
      gboolean exit_loop = FALSE;
      gint new_depth = depth - 1;

      (*count)++;

     /* Make sure that we back up until we find a row
      * where gtk_tree_path_next does not return NULL.
      */
      while (!exit_loop)
        {
          if (gtk_tree_path_get_depth (tree_path) == 0)
              /* depth is now zero so */
            return;
          gtk_tree_path_next (tree_path);

          /* Verify that the next row is a valid row! */
          exit_loop = gtk_tree_model_get_iter (tree_model, &temp_iter, tree_path);

          if (!exit_loop)
            {
              /* Keep going up until we find a row that has a valid next */
              if (gtk_tree_path_get_depth(tree_path) > 1)
                {
                  new_depth--;
                  gtk_tree_path_up (tree_path);
                }
              else
                {
                 /* If depth is 1 and gtk_tree_model_get_iter returns FALSE,
                  * then we are at the last row, so just return.
                  */
                  if (orig != NULL)
                    *count = -1;

                  return;
                }
            }
        }

     /* This guarantees that we will stop when we hit the end of the
      * children.
      */
      if (new_depth < 0)
        return;

      iterate_thru_children (tree_view, tree_model, tree_path,
                             orig, count, new_depth);
      return;
    }

 /* If it gets here, then the path wasn't found.  Situations
  * that would cause this would be if the path passed in is
  * invalid or contained within the last row, but not visible
  * because the last row is not expanded.  If NULL was passed
  * in then a row count is desired, so only set count to -1
  * if orig is not NULL.
  */
  if (orig != NULL)
    *count = -1;

  return;
}

static void
clean_cell_info (GtkTreeViewAccessible         *accessible,
                 GtkTreeViewAccessibleCellInfo *cell_info)
{
  GObject *obj;

  if (cell_info->in_use)
    {
      obj = G_OBJECT (cell_info->cell);

      _gtk_cell_accessible_add_state (cell_info->cell, ATK_STATE_DEFUNCT, FALSE);
      g_object_weak_unref (obj, (GWeakNotify) cell_destroyed, cell_info);
      cell_info->in_use = FALSE;
      if (!accessible->garbage_collection_pending)
        {
          accessible->garbage_collection_pending = TRUE;
          g_assert (accessible->idle_garbage_collect_id == 0);
          accessible->idle_garbage_collect_id =
            gdk_threads_add_idle (idle_garbage_collect_cell_data, accessible);
        }
    }
}

static void
clean_rows (GtkTreeViewAccessible *accessible)
{
  GtkTreeViewAccessibleCellInfo *cell_info;
  GHashTableIter iter;

  /* Clean GtkTreeViewAccessibleCellInfo data */
  g_hash_table_iter_init (&iter, accessible->cell_info_by_index);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&cell_info))
    {
      GtkTreePath *row_path;

      row_path = gtk_tree_row_reference_get_path (cell_info->cell_row_ref);

      /* If the cell has become invalid because the row has been removed,
       * then set the cell's state to ATK_STATE_DEFUNCT and schedule
       * its removal.  If row_path is NULL then the row has
       * been removed.
       */
      if (row_path == NULL)
        clean_cell_info (accessible, cell_info);
      else
        gtk_tree_path_free (row_path);
    }
}

static void
clean_cols (GtkTreeViewAccessible *accessible,
            GtkTreeViewColumn     *tv_col)
{
  GtkTreeViewAccessibleCellInfo *cell_info;
  GHashTableIter iter;

  /* Clean GtkTreeViewAccessibleCellInfo data */
  g_hash_table_iter_init (&iter, accessible->cell_info_by_index);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &cell_info))
    {
      /* If the cell has become invalid because the column tv_col
       * has been removed, then set the cell's state to ATK_STATE_DEFUNCT
       * and remove the cell from accessible->cell_data.
       */
      if (cell_info->cell_col_ref == tv_col)
        clean_cell_info (accessible, cell_info);
    }
}

static gboolean
garbage_collect_cell_data (gpointer data)
{
  GtkTreeViewAccessible *accessible;
  GtkTreeViewAccessibleCellInfo *cell_info;
  GHashTableIter iter;

  accessible = (GtkTreeViewAccessible *)data;

  accessible->garbage_collection_pending = FALSE;
  if (accessible->idle_garbage_collect_id != 0)
    {
      g_source_remove (accessible->idle_garbage_collect_id);
      accessible->idle_garbage_collect_id = 0;
    }

  /* Must loop through them all */
  g_hash_table_iter_init (&iter, accessible->cell_info_by_index);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&cell_info))
    {
      if (!cell_info->in_use)
        {
          g_hash_table_iter_remove (&iter);
        }
    }

  return accessible->garbage_collection_pending;
}

static gboolean
idle_garbage_collect_cell_data (gpointer data)
{
  GtkTreeViewAccessible *accessible;

  accessible = (GtkTreeViewAccessible *)data;

  /* this is the idle handler (only one instance allowed), so
   * we can safely delete it.
   */
  accessible->garbage_collection_pending = FALSE;
  accessible->idle_garbage_collect_id = 0;

  accessible->garbage_collection_pending = garbage_collect_cell_data (data);

  /* N.B.: if for some reason another handler has re-enterantly been
   * queued while this handler was being serviced, it has its own gsource,
   * therefore this handler should always return FALSE.
   */
  return FALSE;
}

/* If tree_path is passed in as NULL, then all cells are acted on.
 * Otherwise, just act on those cells that are on a row greater than
 * the specified tree_path. If inc_row is passed in as TRUE, then rows
 * greater and equal to the specified tree_path are acted on.
 *
 * If set_stale is set the ATK_STATE_STALE is set on cells which
 * are to be acted on.
 *
 * The function set_cell_visibility() is called on all cells to be
 * acted on to update the visibility of the cell.
 */
static void
traverse_cells (GtkTreeViewAccessible *accessible,
                GtkTreePath           *tree_path,
                gboolean               set_stale,
                gboolean               inc_row)
{
  GtkTreeViewAccessibleCellInfo *cell_info;
  GtkWidget *widget;
  GHashTableIter iter;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (!widget)
    return;

  /* Must loop through them all */
  g_hash_table_iter_init (&iter, accessible->cell_info_by_index);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&cell_info))
    {
      GtkTreePath *row_path;
      gboolean act_on_cell;

      if (cell_info->in_use)
        {
          row_path = gtk_tree_row_reference_get_path (cell_info->cell_row_ref);
          g_return_if_fail (row_path != NULL);
          if (tree_path == NULL)
            act_on_cell = TRUE;
          else
            {
              gint comparison;

              comparison =  gtk_tree_path_compare (row_path, tree_path);
              if ((comparison > 0) ||
                  (comparison == 0 && inc_row))
                act_on_cell = TRUE;
              else
                act_on_cell = FALSE;
            }

          if (!cell_info->in_use)
            g_warning ("warning: cell info destroyed during traversal");

          if (act_on_cell && cell_info->in_use)
            {
              if (set_stale)
                _gtk_cell_accessible_add_state (cell_info->cell, ATK_STATE_STALE, TRUE);
              set_cell_visibility (GTK_TREE_VIEW (widget),
                                   cell_info->cell,
                                   cell_info->cell_col_ref,
                                   row_path, TRUE);
            }
          gtk_tree_path_free (row_path);
        }
    }

  g_signal_emit_by_name (accessible, "visible-data-changed");
}

/* If the tree_path passed in has children, then
 * ATK_STATE_EXPANDABLE is set.  If the row is expanded
 * ATK_STATE_EXPANDED is turned on.  If the row is
 * collapsed, then ATK_STATE_EXPANDED is removed.
 *
 * If the tree_path passed in has no children, then
 * ATK_STATE_EXPANDABLE and ATK_STATE_EXPANDED are removed.
 *
 * If set_on_ancestor is TRUE, then this function will also
 * update all cells that are ancestors of the tree_path.
 */
static void
set_expand_state (GtkTreeView           *tree_view,
                  GtkTreeModel          *tree_model,
                  GtkTreeViewAccessible *accessible,
                  GtkTreePath           *tree_path,
                  gboolean               set_on_ancestor)
{
  GtkTreeViewColumn *expander_tv;
  GtkTreeViewAccessibleCellInfo *cell_info;
  GtkTreePath *cell_path;
  GtkTreeIter iter;
  gboolean found;
  GHashTableIter hash_iter;

  g_hash_table_iter_init (&hash_iter, accessible->cell_info_by_index);
  while (g_hash_table_iter_next (&hash_iter, NULL, (gpointer *) &cell_info))
    {
      if (cell_info->in_use)
        {
          cell_path = gtk_tree_row_reference_get_path (cell_info->cell_row_ref);
          found = FALSE;

          if (cell_path != NULL)
            {
              GtkCellAccessible *cell = GTK_CELL_ACCESSIBLE (cell_info->cell);

              expander_tv = gtk_tree_view_get_expander_column (tree_view);

              /* Only set state for the cell that is in the column with the
               * expander toggle
               */
              if (expander_tv == cell_info->cell_col_ref)
                {
                  if (tree_path && gtk_tree_path_compare (cell_path, tree_path) == 0)
                    found = TRUE;
                  else if (set_on_ancestor &&
                           gtk_tree_path_get_depth (cell_path) <
                           gtk_tree_path_get_depth (tree_path) &&
                           gtk_tree_path_is_ancestor (cell_path, tree_path) == 1)
                    /* Only set if set_on_ancestor was passed in as TRUE */
                    found = TRUE;
                }

              /* Set ATK_STATE_EXPANDABLE and ATK_STATE_EXPANDED
               * for ancestors and found cells.
               */
              if (found)
                {
                  /* Must check against cell_path since cell_path
                   * can be equal to or an ancestor of tree_path.
                   */
                  gtk_tree_model_get_iter (tree_model, &iter, cell_path);

                  /* Set or unset ATK_STATE_EXPANDABLE as appropriate */
                  if (gtk_tree_model_iter_has_child (tree_model, &iter))
                    {
                      set_cell_expandable (cell);

                      if (gtk_tree_view_row_expanded (tree_view, cell_path))
                        _gtk_cell_accessible_add_state (cell, ATK_STATE_EXPANDED, TRUE);
                      else
                        _gtk_cell_accessible_remove_state (cell, ATK_STATE_EXPANDED, TRUE);
                    }
                  else
                    {
                      _gtk_cell_accessible_remove_state (cell, ATK_STATE_EXPANDED, TRUE);
                      if (_gtk_cell_accessible_remove_state (cell, ATK_STATE_EXPANDABLE, TRUE))
                      /* The state may have been propagated to the container cell */
                      if (!GTK_IS_CONTAINER_CELL_ACCESSIBLE (cell))
                        _gtk_cell_accessible_remove_action_by_name (cell,
                                                                    "expand or contract");
                    }

                  /* We assume that each cell in the cache once and
                   * a container cell is before its child cells so we are
                   * finished if set_on_ancestor is not set to TRUE.
                   */
                  if (!set_on_ancestor)
                    break;
                }
            }
          gtk_tree_path_free (cell_path);
        }
    }
}

static void
add_cell_actions (GtkCellAccessible *cell,
                  gboolean           editable)
{
  if (GTK_IS_BOOLEAN_CELL_ACCESSIBLE (cell))
    _gtk_cell_accessible_add_action (cell,
                                     "toggle", "toggles the cell",
                                     NULL, toggle_cell_toggled);
  if (editable)
    _gtk_cell_accessible_add_action (cell,
                                     "edit", "creates a widget in which the contents of the cell can be edited",
                                     NULL, edit_cell);
  _gtk_cell_accessible_add_action (cell,
                                   "activate", "activate the cell",
                                   NULL, activate_cell);
}

static void
toggle_cell_expanded (GtkCellAccessible *cell)
{
  GtkTreeViewAccessibleCellInfo *cell_info;
  GtkTreeView *tree_view;
  GtkTreePath *path;
  AtkObject *parent;
  AtkStateSet *stateset;

  parent = atk_object_get_parent (ATK_OBJECT (cell));
  if (GTK_IS_CONTAINER_CELL_ACCESSIBLE (parent))
    parent = atk_object_get_parent (parent);

  cell_info = find_cell_info (GTK_TREE_VIEW_ACCESSIBLE (parent), cell, TRUE);
  if (!cell_info || !cell_info->cell_col_ref || !cell_info->cell_row_ref)
    return;

  tree_view = GTK_TREE_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (parent)));
  path = gtk_tree_row_reference_get_path (cell_info->cell_row_ref);
  if (!path)
    return;

  stateset = atk_object_ref_state_set (ATK_OBJECT (cell));
  if (atk_state_set_contains_state (stateset, ATK_STATE_EXPANDED))
    gtk_tree_view_collapse_row (tree_view, path);
  else
    gtk_tree_view_expand_row (tree_view, path, TRUE);
  g_object_unref (stateset);
  gtk_tree_path_free (path);
}

static void
toggle_cell_toggled (GtkCellAccessible *cell)
{
  GtkTreeViewAccessibleCellInfo *cell_info;
  GtkTreePath *path;
  gchar *pathstring;
  GList *renderers, *cur_renderer;
  AtkObject *parent;
  gboolean is_container_cell = FALSE;

  parent = atk_object_get_parent (ATK_OBJECT (cell));
  if (GTK_IS_CONTAINER_CELL_ACCESSIBLE (parent))
    {
      is_container_cell = TRUE;
      parent = atk_object_get_parent (parent);
    }

  cell_info = find_cell_info (GTK_TREE_VIEW_ACCESSIBLE (parent), cell, TRUE);
  if (!cell_info || !cell_info->cell_col_ref || !cell_info->cell_row_ref)
    return;

  path = gtk_tree_row_reference_get_path (cell_info->cell_row_ref);
  if (!path)
    return;

  /* If the cell is in a container, its index is used to find the
   * renderer in the list. Otherwise, we assume that the cell is
   * represented by the first renderer in the list
   */
  renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (cell_info->cell_col_ref));
  if (is_container_cell)
    cur_renderer = g_list_nth (renderers, cell->index);
  else
    cur_renderer = renderers;

  if (cur_renderer)
    {
      pathstring = gtk_tree_path_to_string (path);
      g_signal_emit_by_name (cur_renderer->data, "toggled", pathstring);
      g_free (pathstring);
    }

  g_list_free (renderers);
  gtk_tree_path_free (path);
}

static void
edit_cell (GtkCellAccessible *cell)
{
  GtkTreeViewAccessibleCellInfo *cell_info;
  GtkTreeView *tree_view;
  GtkTreePath *path;
  AtkObject *parent;

  parent = atk_object_get_parent (ATK_OBJECT (cell));
  if (GTK_IS_CONTAINER_CELL_ACCESSIBLE (parent))
    parent = atk_object_get_parent (parent);

  cell_info = find_cell_info (GTK_TREE_VIEW_ACCESSIBLE (parent), cell, TRUE);
  if (!cell_info || !cell_info->cell_col_ref || !cell_info->cell_row_ref)
    return;

  tree_view = GTK_TREE_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (parent)));
  path = gtk_tree_row_reference_get_path (cell_info->cell_row_ref);
  if (!path)
    return;
  gtk_tree_view_set_cursor (tree_view, path, cell_info->cell_col_ref, TRUE);
  gtk_tree_path_free (path);
}

static void
activate_cell (GtkCellAccessible *cell)
{
  GtkTreeViewAccessibleCellInfo *cell_info;
  GtkTreeView *tree_view;
  GtkTreePath *path;
  AtkObject *parent;

  parent = atk_object_get_parent (ATK_OBJECT (cell));
  if (GTK_IS_CONTAINER_CELL_ACCESSIBLE (parent))
    parent = atk_object_get_parent (parent);

  cell_info = find_cell_info (GTK_TREE_VIEW_ACCESSIBLE (parent), cell, TRUE);
  if (!cell_info || !cell_info->cell_col_ref || !cell_info->cell_row_ref)
    return;

  tree_view = GTK_TREE_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (parent)));
  path = gtk_tree_row_reference_get_path (cell_info->cell_row_ref);
  if (!path)
    return;
  gtk_tree_view_row_activated (tree_view, path, cell_info->cell_col_ref);
  gtk_tree_path_free (path);
}

static void
cell_destroyed (gpointer data)
{
  GtkTreeViewAccessibleCellInfo *cell_info = data;

  if (!cell_info)
    return;
  if (cell_info->in_use)
    {
      cell_info->in_use = FALSE;

      if (!cell_info->view->garbage_collection_pending)
        {
          cell_info->view->garbage_collection_pending = TRUE;
          cell_info->view->idle_garbage_collect_id =
            gdk_threads_add_idle (idle_garbage_collect_cell_data, cell_info->view);
        }
    }
}

static void
cell_info_get_index (GtkTreeView                     *tree_view,
                     GtkTreeViewAccessibleCellInfo   *info,
                     gint                            *index)
{
  GtkTreePath *path;
  gint column_number;

  path = gtk_tree_row_reference_get_path (info->cell_row_ref);
  if (!path)
    return;

  column_number = get_column_number (tree_view, info->cell_col_ref, FALSE);
  *index = get_index (tree_view, path, column_number);
  gtk_tree_path_free (path);
}

static void
cell_info_new (GtkTreeViewAccessible *accessible,
               GtkTreeModel          *tree_model,
               GtkTreePath           *path,
               GtkTreeViewColumn     *tv_col,
               GtkCellAccessible     *cell)
{
  GtkTreeViewAccessibleCellInfo *cell_info;

  cell_info = g_new (GtkTreeViewAccessibleCellInfo, 1);
  cell_info->cell_row_ref = gtk_tree_row_reference_new (tree_model, path);

  cell_info->cell_col_ref = tv_col;
  cell_info->cell = cell;
  cell_info->in_use = TRUE; /* if we've created it, assume it's in use */
  cell_info->view = accessible;
  g_hash_table_insert (accessible->cell_info_by_index, &cell->index, cell_info);

  /* Setup weak reference notification */
  g_object_weak_ref (G_OBJECT (cell), (GWeakNotify) cell_destroyed, cell_info);
}

static GtkCellAccessible *
find_cell (GtkTreeViewAccessible *accessible,
           gint                   index)
{
  GtkTreeViewAccessibleCellInfo *info;

  info = g_hash_table_lookup (accessible->cell_info_by_index, &index);
  if (!info)
    return NULL;

  return info->cell;
}

static void
refresh_cell_index (GtkCellAccessible *cell)
{
  GtkTreeViewAccessibleCellInfo *info;
  AtkObject *parent;
  GtkTreeView *tree_view;
  GtkTreeViewAccessible *accessible;
  gint index;

  parent = atk_object_get_parent (ATK_OBJECT (cell));
  if (!GTK_IS_TREE_VIEW_ACCESSIBLE (parent))
    return;

  accessible = GTK_TREE_VIEW_ACCESSIBLE (parent);

  tree_view = GTK_TREE_VIEW (gtk_accessible_get_widget (GTK_ACCESSIBLE (parent)));

  /* Find this cell in the GtkTreeViewAccessible's cache */
  info = find_cell_info (accessible, cell, TRUE);
  if (!info)
    return;

  cell_info_get_index (tree_view, info, &index);
  g_hash_table_steal (accessible->cell_info_by_index, &cell->index);
  cell->index = index;
  g_hash_table_insert (accessible->cell_info_by_index, &cell->index, info);
}

static void
connect_model_signals (GtkTreeView           *view,
                       GtkTreeViewAccessible *accessible)
{
  GObject *obj;

  obj = G_OBJECT (accessible->tree_model);
  g_signal_connect_data (obj, "row-changed",
                         G_CALLBACK (model_row_changed), view, NULL, 0);
  g_signal_connect_data (obj, "row-inserted",
                         G_CALLBACK (model_row_inserted), view, NULL,
                         G_CONNECT_AFTER);
  g_signal_connect_data (obj, "row-deleted",
                         G_CALLBACK (model_row_deleted), view, NULL,
                         G_CONNECT_AFTER);
  g_signal_connect_data (obj, "rows-reordered",
                         G_CALLBACK (model_rows_reordered), view, NULL,
                         G_CONNECT_AFTER);
}

static void
disconnect_model_signals (GtkTreeViewAccessible *accessible)
{
  GObject *obj;
  GtkWidget *widget;

  obj = G_OBJECT (accessible->tree_model);
  g_object_remove_weak_pointer (obj, (gpointer *) &accessible->tree_model);
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  g_signal_handlers_disconnect_by_func (obj, model_row_changed, widget);
  g_signal_handlers_disconnect_by_func (obj, model_row_inserted, widget);
  g_signal_handlers_disconnect_by_func (obj, model_row_deleted, widget);
  g_signal_handlers_disconnect_by_func (obj, model_rows_reordered, widget);
}

static void
clear_cached_data (GtkTreeViewAccessible *accessible)
{
  GtkTreeViewAccessibleCellInfo *cell_info;
  GHashTableIter iter;

  /* Must loop through them all */
  g_hash_table_iter_init (&iter, accessible->cell_info_by_index);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &cell_info))
    clean_cell_info (accessible, cell_info);

  /* FIXME: seems pretty inefficient to loop again here */
  garbage_collect_cell_data (accessible);
}

/* Returns the column number of the specified GtkTreeViewColumn
 *
 * If visible is set, the value returned will be the visible column number,
 * i.e. suitable for use in AtkTable function. If visible is not set, the
 * value returned is the actual column number, which is suitable for use in
 * getting an index value.
 */
static gint
get_column_number (GtkTreeView       *tree_view,
                   GtkTreeViewColumn *column,
                   gboolean           visible)
{
  GtkTreeViewColumn *tv_column;
  gint ret_val;
  gint i;
  AtkObject *atk_obj;
  GtkTreeViewAccessible *accessible;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (tree_view));
  accessible = GTK_TREE_VIEW_ACCESSIBLE (atk_obj);

  ret_val = 0;
  for (i = 0; i < accessible->col_data->len; i++)
    {
      tv_column = g_array_index (accessible->col_data, GtkTreeViewColumn *, i);
      if (tv_column == column)
        break;
      if (!visible || gtk_tree_view_column_get_visible (tv_column))
        ret_val++;
    }
  if (i == accessible->col_data->len)
    ret_val = -1;

  return ret_val;
}

static gint
get_index (GtkTreeView *tree_view,
           GtkTreePath *path,
           gint         actual_column)
{
  AtkObject *atk_obj;
  GtkTreeViewAccessible *accessible;
  gint depth = 0;
  gint index = 1;
  gint *indices = NULL;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (tree_view));
  accessible = GTK_TREE_VIEW_ACCESSIBLE (atk_obj);

  if (path)
    {
      depth = gtk_tree_path_get_depth (path);
      indices = gtk_tree_path_get_indices (path);
    }

  if (depth > 1)
    {
      GtkTreePath *copy_path;
      GtkTreeModel *model;

      model = gtk_tree_view_get_model (tree_view);
      copy_path = gtk_tree_path_copy (path);
      gtk_tree_path_up (copy_path);
      count_rows (model, NULL, copy_path, &index, 0, depth);
      gtk_tree_path_free (copy_path);
    }

  if (path)
    index += indices[depth - 1];
  index *= accessible->n_cols;
  index +=  actual_column;
  return index;
}

/* The function count_rows counts the number of rows starting at iter
 * and ending at end_path. The value of level is the depth of iter and
 * the value of depth is the depth of end_path. Rows at depth before
 * end_path are counted. This functions counts rows which are not visible
 * because an ancestor is collapsed.
 */
static void
count_rows (GtkTreeModel *model,
            GtkTreeIter  *iter,
            GtkTreePath  *end_path,
            gint         *count,
            gint          level,
            gint          depth)
{
  GtkTreeIter child_iter;

  if (!model)
    return;

  level++;
  *count += gtk_tree_model_iter_n_children (model, iter);

  if (gtk_tree_model_get_flags (model) & GTK_TREE_MODEL_LIST_ONLY)
    return;

  if (level >= depth)
    return;

  if (gtk_tree_model_iter_children (model, &child_iter, iter))
    {
      gboolean ret_val = TRUE;

      while (ret_val)
        {
          if (level == depth - 1)
            {
              GtkTreePath *iter_path;
              gboolean finished = FALSE;

              iter_path = gtk_tree_model_get_path (model, &child_iter);
              if (end_path && gtk_tree_path_compare (iter_path, end_path) >= 0)
                finished = TRUE;
              gtk_tree_path_free (iter_path);
              if (finished)
                break;
            }
          if (gtk_tree_model_iter_has_child (model, &child_iter))
            count_rows (model, &child_iter, end_path, count, level, depth);
          ret_val = gtk_tree_model_iter_next (model, &child_iter);
        }
    }
}

/* Find the next node, which has children, at the specified depth below
 * the specified iter. The level is the depth of the current iter.
 * The position of the node is returned in path and the return value
 * of TRUE means that a node was found.
 */

static gboolean
get_next_node_with_child_at_depth (GtkTreeModel  *model,
                                   GtkTreeIter   *iter,
                                   GtkTreePath  **path,
                                   gint           level,
                                   gint           depth)
{
  GtkTreeIter child_iter;

  *path = NULL;

  if (gtk_tree_model_iter_children (model, &child_iter, iter))
    {
      level++;

      while (TRUE)
        {
          while (!gtk_tree_model_iter_has_child (model, &child_iter))
            {
              if (!gtk_tree_model_iter_next (model, &child_iter))
                return FALSE;
            }

          if (level == depth)
            {
              /* We have found what we were looking for */
              *path = gtk_tree_model_get_path (model, &child_iter);
              return TRUE;
            }

          if (get_next_node_with_child_at_depth (model, &child_iter, path,
                                                 level, depth))
            return TRUE;

          if (!gtk_tree_model_iter_next (model, &child_iter))
            return FALSE;
        }
    }
  return FALSE;
}

/* Find the next node, which has children, at the same depth
 * as the specified GtkTreePath.
 */
static gboolean
get_next_node_with_child (GtkTreeModel  *model,
                          GtkTreePath   *path,
                          GtkTreePath  **return_path)
{
  GtkTreeIter iter;
  gint depth;

  gtk_tree_model_get_iter (model, &iter, path);

  while (gtk_tree_model_iter_next (model, &iter))
    {
      if (gtk_tree_model_iter_has_child (model, &iter))
        {
          *return_path = gtk_tree_model_get_path (model, &iter);
          return TRUE;
        }
    }
  depth = gtk_tree_path_get_depth (path);
  while (gtk_tree_path_up (path))
    {
      if (gtk_tree_path_get_depth (path) == 0)
        break;

      gtk_tree_model_get_iter (model, &iter, path);
      while (gtk_tree_model_iter_next (model, &iter))
        if (get_next_node_with_child_at_depth (model, &iter, return_path,
                                         gtk_tree_path_get_depth (path), depth))
          return TRUE;
    }
  *return_path = NULL;
  return FALSE;
}

static gboolean
get_tree_path_from_row_index (GtkTreeModel  *model,
                              gint           row_index,
                              GtkTreePath  **tree_path)
{
  GtkTreeIter iter;
  gint count;
  gint depth;

  count = gtk_tree_model_iter_n_children (model, NULL);
  if (count > row_index)
    {
      if (gtk_tree_model_iter_nth_child (model, &iter, NULL, row_index))
        {
          *tree_path = gtk_tree_model_get_path (model, &iter);
          return TRUE;
        }
      else
        return FALSE;
    }
  else
     row_index -= count;

  depth = 0;
  while (TRUE)
    {
      depth++;

      if (get_next_node_with_child_at_depth (model, NULL, tree_path, 0, depth))
        {
          GtkTreePath *next_path;

          while (TRUE)
            {
              gtk_tree_model_get_iter (model, &iter, *tree_path);
              count = gtk_tree_model_iter_n_children (model, &iter);
              if (count > row_index)
                {
                  gtk_tree_path_append_index (*tree_path, row_index);
                  return TRUE;
                }
              else
                row_index -= count;

              if (!get_next_node_with_child (model, *tree_path, &next_path))
                break;

              gtk_tree_path_free (*tree_path);
              *tree_path = next_path;
            }
        }
      else
        {
          g_warning ("Index value is too large\n");
          gtk_tree_path_free (*tree_path);
           *tree_path = NULL;
          return FALSE;
        }
    }
}

static gboolean
get_path_column_from_index (GtkTreeView        *tree_view,
                            gint                index,
                            GtkTreePath       **path,
                            GtkTreeViewColumn **column)
{
  GtkTreeModel *tree_model;
  AtkObject *atk_obj;
  GtkTreeViewAccessible *accessible;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (tree_view));
  accessible = GTK_TREE_VIEW_ACCESSIBLE (atk_obj);

  tree_model = gtk_tree_view_get_model (tree_view);
  if (accessible->n_cols == 0)
    return FALSE;
  /* First row is the column headers */
  index -= accessible->n_cols;
  if (index < 0)
    return FALSE;

  if (path)
    {
      gint row_index;
      gboolean retval;

      row_index = index / accessible->n_cols;
      retval = get_tree_path_from_row_index (tree_model, row_index, path);
      if (!retval)
        return FALSE;
      if (*path == NULL)
        return FALSE;
    }

  if (column)
    {
      *column = gtk_tree_view_get_column (tree_view, index % accessible->n_cols);
      if (*column == NULL)
        {
          if (path)
            gtk_tree_path_free (*path);
          return FALSE;
        }
  }
  return TRUE;
}

static void
set_cell_expandable (GtkCellAccessible *cell)
{
  if (_gtk_cell_accessible_add_state (cell, ATK_STATE_EXPANDABLE, FALSE))
    _gtk_cell_accessible_add_action (cell,
                                     "expand or contract",
                                     "expands or contracts the row in the tree view containing this cell",
                                     NULL, toggle_cell_expanded);
}

static GtkTreeViewAccessibleCellInfo *
find_cell_info (GtkTreeViewAccessible *accessible,
                GtkCellAccessible     *cell,
                gboolean               live_only)
{
  GtkTreeViewAccessibleCellInfo *cell_info;
  GHashTableIter iter;

  /* Clean GtkTreeViewAccessibleCellInfo data */
  g_hash_table_iter_init (&iter, accessible->cell_info_by_index);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &cell_info))
    {
      if (cell_info->cell == cell && (!live_only || cell_info->in_use))
        return cell_info;
    }
  return NULL;
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
