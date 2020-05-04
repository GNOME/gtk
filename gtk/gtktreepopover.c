/*
 * Copyright © 2019 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include "gtktreepopoverprivate.h"

#include "gtktreemodel.h"
#include "gtkcellarea.h"
#include "gtkcelllayout.h"
#include "gtkcellview.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkgizmoprivate.h"
#include "gtkbuiltiniconprivate.h"

// TODO
// positioning + sizing

struct _GtkTreePopover
{
  GtkPopover parent_instance;

  GtkTreeModel *model;

  GtkCellArea *area;
  GtkCellAreaContext *context;

  gulong size_changed_id;
  gulong row_inserted_id;
  gulong row_deleted_id;
  gulong row_changed_id;
  gulong row_reordered_id;
  gulong apply_attributes_id;

  GtkTreeViewRowSeparatorFunc row_separator_func;
  gpointer                    row_separator_data;
  GDestroyNotify              row_separator_destroy;

  GtkWidget *active_item;
};

enum {
  PROP_0,
  PROP_MODEL,
  PROP_CELL_AREA,

  NUM_PROPERTIES
};

enum {
  MENU_ACTIVATE,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS];

static void gtk_tree_popover_cell_layout_init (GtkCellLayoutIface  *iface);
static void gtk_tree_popover_set_area (GtkTreePopover *popover,
                                       GtkCellArea    *area);
static void rebuild_menu (GtkTreePopover *popover);
static void context_size_changed_cb (GtkCellAreaContext *context,
                                     GParamSpec         *pspec,
                                     GtkWidget          *popover);
static GtkWidget * gtk_tree_popover_create_item (GtkTreePopover *popover,
                                                 GtkTreePath    *path,
                                                 GtkTreeIter    *iter,
                                                 gboolean        header_item);
static GtkWidget * gtk_tree_popover_get_path_item (GtkTreePopover *popover,
                                                   GtkTreePath    *search);
static void gtk_tree_popover_set_active_item (GtkTreePopover *popover,
                                              GtkWidget      *item);

G_DEFINE_TYPE_WITH_CODE (GtkTreePopover, gtk_tree_popover, GTK_TYPE_POPOVER,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_LAYOUT,
                                                gtk_tree_popover_cell_layout_init));

static void
gtk_tree_popover_constructed (GObject *object)
{
  GtkTreePopover *popover = GTK_TREE_POPOVER (object);

  G_OBJECT_CLASS (gtk_tree_popover_parent_class)->constructed (object);

  if (!popover->area)
    {
      GtkCellArea *area = gtk_cell_area_box_new ();
      gtk_tree_popover_set_area (popover, area);
    }

  popover->context = gtk_cell_area_create_context (popover->area);

  popover->size_changed_id = g_signal_connect (popover->context, "notify",
                                               G_CALLBACK (context_size_changed_cb), popover);
}

static void
gtk_tree_popover_dispose (GObject *object)
{
  GtkTreePopover *popover = GTK_TREE_POPOVER (object);

  gtk_tree_popover_set_model (popover, NULL);
  gtk_tree_popover_set_area (popover, NULL);

  if (popover->context)
    {
      g_signal_handler_disconnect (popover->context, popover->size_changed_id);
      popover->size_changed_id = 0;

      g_clear_object (&popover->context);
    }

  G_OBJECT_CLASS (gtk_tree_popover_parent_class)->dispose (object);
}

static void
gtk_tree_popover_finalize (GObject *object)
{
  GtkTreePopover *popover = GTK_TREE_POPOVER (object);

  if (popover->row_separator_destroy)
    popover->row_separator_destroy (popover->row_separator_data);

  G_OBJECT_CLASS (gtk_tree_popover_parent_class)->finalize (object);
}

static void
gtk_tree_popover_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkTreePopover *popover = GTK_TREE_POPOVER (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      gtk_tree_popover_set_model (popover, g_value_get_object (value));
      break;

    case PROP_CELL_AREA:
      gtk_tree_popover_set_area (popover, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tree_popover_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkTreePopover *popover = GTK_TREE_POPOVER (object);

  switch (prop_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, popover->model);
      break;

    case PROP_CELL_AREA:
      g_value_set_object (value, popover->area);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_tree_popover_class_init (GtkTreePopoverClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed  = gtk_tree_popover_constructed;
  object_class->dispose = gtk_tree_popover_dispose;
  object_class->finalize = gtk_tree_popover_finalize;
  object_class->set_property = gtk_tree_popover_set_property;
  object_class->get_property = gtk_tree_popover_get_property;

  g_object_class_install_property (object_class,
                                   PROP_MODEL,
                                   g_param_spec_object ("model",
                                                        P_("model"),
                                                        P_("The model for the popover"),
                                                        GTK_TYPE_TREE_MODEL,
                                                        GTK_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_CELL_AREA,
                                   g_param_spec_object ("cell-area",
                                                        P_("Cell Area"),
                                                        P_("The GtkCellArea used to layout cells"),
                                                        GTK_TYPE_CELL_AREA,
                                                        GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  signals[MENU_ACTIVATE] =
    g_signal_new (I_("menu-activate"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
gtk_tree_popover_add_submenu (GtkTreePopover *popover,
                              GtkWidget      *submenu,
                              const char     *name)
{
  GtkWidget *stack = gtk_popover_get_child (GTK_POPOVER (popover));
  gtk_stack_add_named (GTK_STACK (stack), submenu, name);
}

static GtkWidget *
gtk_tree_popover_get_submenu (GtkTreePopover *popover,
                              const char     *name)
{
  GtkWidget *stack = gtk_popover_get_child (GTK_POPOVER (popover));
  return gtk_stack_get_child_by_name (GTK_STACK (stack), name);
}

void
gtk_tree_popover_open_submenu (GtkTreePopover *popover,
                               const char     *name)
{
  GtkWidget *stack = gtk_popover_get_child (GTK_POPOVER (popover));
  gtk_stack_set_visible_child_name (GTK_STACK (stack), name);
}

static void
gtk_tree_popover_init (GtkTreePopover *popover)
{
  GtkWidget *stack;

  stack = gtk_stack_new ();
  gtk_stack_set_vhomogeneous (GTK_STACK (stack), FALSE);
  gtk_stack_set_transition_type (GTK_STACK (stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
  gtk_stack_set_interpolate_size (GTK_STACK (stack), TRUE);
  gtk_popover_set_child (GTK_POPOVER (popover), stack);

  gtk_widget_add_css_class (GTK_WIDGET (popover), GTK_STYLE_CLASS_MENU);
}

static GtkCellArea *
gtk_tree_popover_cell_layout_get_area (GtkCellLayout *layout)
{
  return GTK_TREE_POPOVER (layout)->area;
}

static void
gtk_tree_popover_cell_layout_init (GtkCellLayoutIface  *iface)
{
  iface->get_area = gtk_tree_popover_cell_layout_get_area;
}

static void
insert_at_position (GtkBox    *box,
                    GtkWidget *child,
                    int        position)
{
  GtkWidget *sibling = NULL;

  if (position > 0)
    {
      int i;

      sibling = gtk_widget_get_first_child (GTK_WIDGET (box));
      for (i = 1; i < position; i++)
        sibling = gtk_widget_get_next_sibling (sibling);
    }

  gtk_box_insert_child_after (box, child, sibling);
}

static GtkWidget *
ensure_submenu (GtkTreePopover *popover,
                GtkTreePath    *path)
{
  GtkWidget *box;
  char *name;

  if (path)
    name = gtk_tree_path_to_string (path);
  else
    name = NULL;

  box = gtk_tree_popover_get_submenu (popover, name ? name : "main");
  if (!box)
    {
      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_tree_popover_add_submenu (popover, box, name ? name : "main");
      if (path)
        {
          GtkTreeIter iter;
          GtkWidget *item;
          gtk_tree_model_get_iter (popover->model, &iter, path);
          item = gtk_tree_popover_create_item (popover, path, &iter, TRUE);
          gtk_container_add (GTK_CONTAINER (box), item);
          gtk_container_add (GTK_CONTAINER (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
        }

    }

  g_free (name);

  return box;
}

static void
row_inserted_cb (GtkTreeModel   *model,
                 GtkTreePath    *path,
                 GtkTreeIter    *iter,
                 GtkTreePopover *popover)
{
  gint *indices, depth, index;
  GtkWidget *item;
  GtkWidget *box;

  indices = gtk_tree_path_get_indices (path);
  depth = gtk_tree_path_get_depth (path);
  index = indices[depth - 1];

  item = gtk_tree_popover_create_item (popover, path, iter, FALSE);
  if (depth == 1)
    {
      box = ensure_submenu (popover, NULL);
      insert_at_position (GTK_BOX (box), item, index);
    }
  else
    {
      GtkTreePath *ppath;

      ppath = gtk_tree_path_copy (path);
      gtk_tree_path_up (ppath);

      box = ensure_submenu (popover, ppath);
      insert_at_position (GTK_BOX (box), item, index + 2);

      gtk_tree_path_free (ppath);
    }

  gtk_cell_area_context_reset (popover->context);
}

static void
row_deleted_cb (GtkTreeModel   *model,
                GtkTreePath    *path,
                GtkTreePopover *popover)
{
  GtkWidget *item;

  item = gtk_tree_popover_get_path_item (popover, path);

  if (item)
    {
      gtk_widget_destroy (item);
      gtk_cell_area_context_reset (popover->context);
    }
}

static void
row_changed_cb (GtkTreeModel   *model,
                GtkTreePath    *path,
                GtkTreeIter    *iter,
                GtkTreePopover *popover)
{
  gboolean is_separator = FALSE;
  GtkWidget *item;
  gint *indices, depth, index;

  item = gtk_tree_popover_get_path_item (popover, path);

  if (!item)
    return;

  indices = gtk_tree_path_get_indices (path);
  depth = gtk_tree_path_get_depth (path);
  index = indices[depth - 1];

  if (popover->row_separator_func)
    is_separator = popover->row_separator_func (model, iter, popover->row_separator_data);

  if (is_separator != GTK_IS_SEPARATOR (item))
    {
      GtkWidget *box= gtk_widget_get_parent (item);

      gtk_widget_destroy (item);

      item = gtk_tree_popover_create_item (popover, path, iter, FALSE);

      if (depth == 1)
        insert_at_position (GTK_BOX (box), item, index);
      else
        insert_at_position (GTK_BOX (box), item, index + 2);
    }
}

static void
row_reordered_cb (GtkTreeModel   *model,
                  GtkTreePath    *path,
                  GtkTreeIter    *iter,
                  gint           *new_order,
                  GtkTreePopover *popover)
{
  rebuild_menu (popover);
}

static void
context_size_changed_cb (GtkCellAreaContext *context,
                         GParamSpec         *pspec,
                         GtkWidget          *popover)
{
  if (!strcmp (pspec->name, "minimum-width") ||
      !strcmp (pspec->name, "natural-width") ||
      !strcmp (pspec->name, "minimum-height") ||
      !strcmp (pspec->name, "natural-height"))
    gtk_widget_queue_resize (popover);
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

static GtkWidget *
gtk_tree_popover_get_path_item (GtkTreePopover *popover,
                                GtkTreePath    *search)
{
  GtkWidget *stack = gtk_popover_get_child (GTK_POPOVER (popover));
  GtkWidget *item = NULL;
  GList *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (stack));

  for (l = children; !item && l; l = l->next)
    {
      GtkWidget *child;

      for (child = gtk_widget_get_first_child (GTK_WIDGET (l->data));
           !item && child;
           child = gtk_widget_get_next_sibling (child))
        {
          GtkTreePath *path  = NULL;

          if (GTK_IS_SEPARATOR (child))
            {
              GtkTreeRowReference *row = g_object_get_data (G_OBJECT (child), "gtk-tree-path");

              if (row)
                {
                  path = gtk_tree_row_reference_get_path (row);
                  if (!path)
                    item = child;
                }
            }
          else
            {
              GtkWidget *view = GTK_WIDGET (g_object_get_data (G_OBJECT (child), "view"));

              path = gtk_cell_view_get_displayed_row (GTK_CELL_VIEW (view));

              if (!path)
                item = child;
             }

           if (path)
             {
               if (gtk_tree_path_compare (search, path) == 0)
                 item = child;
               gtk_tree_path_free (path);
             }
        }
    }

  g_list_free (children);

  return item;
}

static void
area_apply_attributes_cb (GtkCellArea    *area,
                          GtkTreeModel   *tree_model,
                          GtkTreeIter    *iter,
                          gboolean       is_expander,
                          gboolean       is_expanded,
                          GtkTreePopover *popover)
{
  GtkTreePath*path;
  GtkWidget *item;
  gboolean sensitive;
  GtkTreeIter dummy;
  gboolean has_submenu = FALSE;

  if (gtk_tree_model_iter_children (popover->model, &dummy, iter))
    has_submenu = TRUE;

  path = gtk_tree_model_get_path (tree_model, iter);
  item = gtk_tree_popover_get_path_item (popover, path);

  if (item)
    {
      sensitive = area_is_sensitive (popover->area);
      gtk_widget_set_sensitive (item, sensitive || has_submenu);
    }

  gtk_tree_path_free (path);
}

static void
gtk_tree_popover_set_area (GtkTreePopover *popover,
                           GtkCellArea    *area)
{
  if (popover->area)
    {
      g_signal_handler_disconnect (popover->area, popover->apply_attributes_id);
      popover->apply_attributes_id = 0;
      g_clear_object (&popover->area);
    }

  popover->area = area;

  if (popover->area)
    {
      g_object_ref_sink (popover->area);
      popover->apply_attributes_id = g_signal_connect (popover->area, "apply-attributes",
                                                       G_CALLBACK (area_apply_attributes_cb), popover);
    }
}

static void
item_activated_cb (GtkGesture     *gesture,
                   guint           n_press,
                   double          x,
                   double          y,
                   GtkTreePopover *popover)
{
  GtkWidget *item;
  GtkCellView *view;
  GtkTreePath *path;
  gchar *path_str;
  gboolean is_header = FALSE;
  gboolean has_submenu = FALSE;

  item = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  is_header = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "is-header"));

  view = GTK_CELL_VIEW (g_object_get_data (G_OBJECT (item), "view"));

  path = gtk_cell_view_get_displayed_row (view);

  if (is_header)
    {
      gtk_tree_path_up (path);
    }
  else
    {
      GtkTreeIter iter;
      GtkTreeIter dummy;

      gtk_tree_model_get_iter (popover->model, &iter, path);
      if (gtk_tree_model_iter_children (popover->model, &dummy, &iter))
        has_submenu = TRUE;
    }

  path_str = gtk_tree_path_to_string (path);

  if (is_header || has_submenu)
    {
      gtk_tree_popover_open_submenu (popover, path_str ? path_str : "main");
    }
  else
    {
      g_signal_emit (popover, signals[MENU_ACTIVATE], 0, path_str);
      gtk_popover_popdown (GTK_POPOVER (popover));
    }

  g_free (path_str);
  gtk_tree_path_free (path);
}

static void
enter_cb (GtkEventController   *controller,
          double                x,
          double                y,
          GdkCrossingMode       mode,
          GtkTreePopover       *popover)
{
  GtkWidget *item;
  item = gtk_event_controller_get_widget (controller);

  gtk_tree_popover_set_active_item (popover, item);
}

static GtkWidget *
gtk_tree_popover_create_item (GtkTreePopover *popover,
                              GtkTreePath    *path,
                              GtkTreeIter    *iter,
                              gboolean        header_item)
{
  GtkWidget *item, *view;
  gboolean is_separator = FALSE;

  if (popover->row_separator_func)
    is_separator = popover->row_separator_func (popover->model, iter, popover->row_separator_data);

  if (is_separator)
    {
      item = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      g_object_set_data_full (G_OBJECT (item), "gtk-tree-path",
                                               gtk_tree_row_reference_new (popover->model, path),
                                               (GDestroyNotify)gtk_tree_row_reference_free);
    }
  else
    {
      GtkEventController *controller;
      GtkTreeIter dummy;
      gboolean has_submenu = FALSE;
      GtkWidget *indicator;

      if (!header_item &&
          gtk_tree_model_iter_children (popover->model, &dummy, iter))
        has_submenu = TRUE;

      view = gtk_cell_view_new_with_context (popover->area, popover->context);
      gtk_cell_view_set_model (GTK_CELL_VIEW (view), popover->model);
      gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (view), path);
      gtk_widget_set_hexpand (view, TRUE);

      item = gtk_gizmo_new ("modelbutton", NULL, NULL, NULL, NULL, NULL, NULL);
      gtk_widget_set_layout_manager (item, gtk_box_layout_new (GTK_ORIENTATION_HORIZONTAL));
      gtk_widget_add_css_class (item, "flat");

      if (header_item)
        {
          indicator = gtk_builtin_icon_new ("arrow");
          gtk_widget_add_css_class (indicator, "left");
          gtk_widget_set_parent (indicator, item);
        }

      gtk_widget_set_parent (view, item);

      indicator = gtk_builtin_icon_new (has_submenu ? "arrow" : "none");
      gtk_widget_add_css_class (indicator, "right");
      gtk_widget_set_parent (indicator, item);

      controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
      g_signal_connect (controller, "pressed", G_CALLBACK (item_activated_cb), popover);
      gtk_widget_add_controller (item, GTK_EVENT_CONTROLLER (controller));

      controller = gtk_event_controller_motion_new ();
      g_signal_connect (controller, "enter", G_CALLBACK (enter_cb), popover);
      gtk_widget_add_controller (item, controller);

      g_object_set_data (G_OBJECT (item), "is-header", GINT_TO_POINTER (header_item));
      g_object_set_data (G_OBJECT (item), "view", view);
    }

  return item;
}

static void
populate (GtkTreePopover *popover,
          GtkTreeIter    *parent)
{
  GtkTreeIter iter;
  gboolean valid = FALSE;

  if (!popover->model)
    return;

  valid = gtk_tree_model_iter_children (popover->model, &iter, parent);

  while (valid)
    {
      GtkTreePath *path;

      path = gtk_tree_model_get_path (popover->model, &iter);
      row_inserted_cb (popover->model, path, &iter, popover);

      populate (popover, &iter);

      valid = gtk_tree_model_iter_next (popover->model, &iter);
      gtk_tree_path_free (path);
    }
}

static void
gtk_tree_popover_populate (GtkTreePopover *popover)
{
  populate (popover, NULL);
}

static void
rebuild_menu (GtkTreePopover *popover)
{
  GtkWidget *stack;

  stack = gtk_popover_get_child (GTK_POPOVER (popover));
  gtk_container_foreach (GTK_CONTAINER (stack), (GtkCallback) gtk_widget_destroy, NULL);

  if (popover->model)
    gtk_tree_popover_populate (popover);
}

void
gtk_tree_popover_set_model (GtkTreePopover *popover,
                            GtkTreeModel   *model)
{
  if (popover->model == model)
    return;

  if (popover->model)
    {
      g_signal_handler_disconnect (popover->model, popover->row_inserted_id);
      g_signal_handler_disconnect (popover->model, popover->row_deleted_id);
      g_signal_handler_disconnect (popover->model, popover->row_changed_id);
      g_signal_handler_disconnect (popover->model, popover->row_reordered_id);
      popover->row_inserted_id  = 0;
      popover->row_deleted_id = 0;
      popover->row_changed_id = 0;
      popover->row_reordered_id = 0;

      g_object_unref (popover->model);
    }

  popover->model = model;

  if (popover->model)
    {
      g_object_ref (popover->model);

      popover->row_inserted_id = g_signal_connect (popover->model, "row-inserted",
                                                   G_CALLBACK (row_inserted_cb), popover);
      popover->row_deleted_id = g_signal_connect (popover->model, "row-deleted",
                                                  G_CALLBACK (row_deleted_cb), popover);
      popover->row_changed_id = g_signal_connect (popover->model, "row-changed",
                                                  G_CALLBACK (row_changed_cb), popover);
      popover->row_reordered_id = g_signal_connect (popover->model, "rows-reordered",
                                                    G_CALLBACK (row_reordered_cb), popover);
    }

  rebuild_menu (popover);
}

void
gtk_tree_popover_set_row_separator_func (GtkTreePopover              *popover,
                                         GtkTreeViewRowSeparatorFunc  func,
                                         gpointer                     data,
                                         GDestroyNotify               destroy)
{
  if (popover->row_separator_destroy)
    popover->row_separator_destroy (popover->row_separator_data);

  popover->row_separator_func = func;
  popover->row_separator_data = data;
  popover->row_separator_destroy = destroy;

  rebuild_menu (popover);
}

static void
gtk_tree_popover_set_active_item (GtkTreePopover *popover,
                                  GtkWidget      *item)
{
  if (popover->active_item == item)
    return;

  if (popover->active_item)
    {
      gtk_widget_unset_state_flags (popover->active_item, GTK_STATE_FLAG_SELECTED);
      g_object_remove_weak_pointer (G_OBJECT (popover->active_item), (gpointer *)&popover->active_item);
    }

  popover->active_item = item;

  if (popover->active_item)
    {
      g_object_add_weak_pointer (G_OBJECT (popover->active_item), (gpointer *)&popover->active_item);
      gtk_widget_set_state_flags (popover->active_item, GTK_STATE_FLAG_SELECTED, FALSE);
    }
}

void
gtk_tree_popover_set_active (GtkTreePopover *popover,
                             int             item)
{
  GtkWidget *box;
  GtkWidget *child;
  int pos;

  if (item == -1)
    {
      gtk_tree_popover_set_active_item (popover, NULL);
      return;
    }

  box = gtk_tree_popover_get_submenu (popover, "main");
  if (!box)
    return;

  for (child = gtk_widget_get_first_child (box), pos = 0;
       child;
       child = gtk_widget_get_next_sibling (child), pos++)
    {
      if (pos == item)
        {
          gtk_tree_popover_set_active_item (popover, child);
          break;
        }
    }
}

