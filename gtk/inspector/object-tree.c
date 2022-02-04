/*
 * Copyright (c) 2008-2009  Christian Hammond
 * Copyright (c) 2008-2009  David Trowbridge
 * Copyright (c) 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#include <string.h>

#include "object-tree.h"
#include "prop-list.h"

#include "gtkbuildable.h"
#include "gtkbutton.h"
#include "gtkcelllayout.h"
#include "gtkcomboboxprivate.h"
#include "gtkiconview.h"
#include "gtklabel.h"
#include "gtkmenuitem.h"
#include "gtksettings.h"
#include "gtktextview.h"
#include "gtktreeview.h"
#include "gtktreeselection.h"
#include "gtktreestore.h"
#include "gtktreemodelsort.h"
#include "gtktreemodelfilter.h"
#include "gtkwidgetprivate.h"
#include "gtkstylecontext.h"
#include "gtksearchbar.h"
#include "gtksearchentry.h"
#include "treewalk.h"

enum
{
  OBJECT,
  OBJECT_TYPE,
  OBJECT_NAME,
  OBJECT_LABEL,
  OBJECT_CLASSES,
  SENSITIVE
};


enum
{
  OBJECT_SELECTED,
  OBJECT_ACTIVATED,
  LAST_SIGNAL
};


struct _GtkInspectorObjectTreePrivate
{
  GtkTreeView *tree;
  GtkTreeStore *model;
  gulong map_hook;
  gulong unmap_hook;
  GtkTreeViewColumn *object_column;
  GtkWidget *search_bar;
  GtkWidget *search_entry;
  GtkTreeWalk *walk;
  gint search_length;
};

typedef struct _ObjectTreeClassFuncs ObjectTreeClassFuncs;
typedef void (* ObjectTreeForallFunc) (GObject    *object,
                                       const char *name,
                                       gpointer    data);

struct _ObjectTreeClassFuncs {
  GType         (* get_type)            (void);
  GObject *     (* get_parent)          (GObject                *object);
  void          (* forall)              (GObject                *object,
                                         ObjectTreeForallFunc    forall_func,
                                         gpointer                forall_data);
  gboolean      (* get_sensitive)       (GObject                *object);
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorObjectTree, gtk_inspector_object_tree, GTK_TYPE_BOX)

static GObject *
object_tree_get_parent_default (GObject *object)
{
  return g_object_get_data (object, "inspector-object-tree-parent");
}

static void
object_tree_forall_default (GObject              *object,
                            ObjectTreeForallFunc  forall_func,
                            gpointer              forall_data)
{
}

static gboolean
object_tree_get_sensitive_default (GObject *object)
{
  return TRUE;
}

static GObject *
object_tree_widget_get_parent (GObject *object)
{
  return G_OBJECT (gtk_widget_get_parent (GTK_WIDGET (object)));
}

static void
object_tree_widget_forall (GObject              *object,
                           ObjectTreeForallFunc  forall_func,
                           gpointer              forall_data)
{
  struct {
    GtkPropagationPhase  phase;
    const gchar         *name;
  } phases[] = {
    { GTK_PHASE_CAPTURE, "capture" },
    { GTK_PHASE_TARGET,  "target" },
    { GTK_PHASE_BUBBLE,  "bubble" },
    { GTK_PHASE_NONE,    "" }
  };
  gint i;

  for (i = 0; i < G_N_ELEMENTS (phases); i++)
    {
      GList *list, *l;

      list = _gtk_widget_list_controllers (GTK_WIDGET (object), phases[i].phase);
      for (l = list; l; l = l->next)
        {
          GObject *controller = l->data;
          forall_func (controller, phases[i].name, forall_data);
        }
      g_list_free (list);
    }

   if (gtk_widget_is_toplevel (GTK_WIDGET (object)))
     {
       GObject *clock;

       clock = G_OBJECT (gtk_widget_get_frame_clock (GTK_WIDGET (object)));
       if (clock)
         forall_func (clock, "frame-clock", forall_data);
     }
}

static gboolean
object_tree_widget_get_sensitive (GObject *object)
{
  return gtk_widget_get_mapped (GTK_WIDGET (object));
}

typedef struct {
  ObjectTreeForallFunc forall_func;
  gpointer             forall_data;
} ForallData;

static void
container_children_callback (GtkWidget *widget,
                             gpointer   client_data)
{
  ForallData *forall_data = client_data;

  forall_data->forall_func (G_OBJECT (widget), NULL, forall_data->forall_data);
}

static void
object_tree_container_forall (GObject              *object,
                              ObjectTreeForallFunc  forall_func,
                              gpointer              forall_data)
{
  ForallData data = {
    forall_func,
    forall_data
  };

  gtk_container_forall (GTK_CONTAINER (object),
                        container_children_callback,
                        &data);
}

static void
object_tree_tree_model_sort_forall (GObject              *object,
                                    ObjectTreeForallFunc  forall_func,
                                    gpointer              forall_data)
{
  GObject *child = G_OBJECT (gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (object)));

  if (child)
    forall_func (child, "model", forall_data);
}

static void
object_tree_tree_model_filter_forall (GObject              *object,
                                      ObjectTreeForallFunc  forall_func,
                                      gpointer              forall_data)
{
  GObject *child = G_OBJECT (gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (object)));

  if (child)
    forall_func (child, "model", forall_data);
}

static void
object_tree_menu_item_forall (GObject              *object,
                              ObjectTreeForallFunc  forall_func,
                              gpointer              forall_data)
{
  GtkWidget *submenu;

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (object));
  if (submenu)
    forall_func (G_OBJECT (submenu), "submenu", forall_data);
}

static void
object_tree_combo_box_forall (GObject              *object,
                              ObjectTreeForallFunc  forall_func,
                              gpointer              forall_data)
{
  GtkWidget *popup;
  GObject *child;

  popup = gtk_combo_box_get_popup (GTK_COMBO_BOX (object));
  if (popup)
    forall_func (G_OBJECT (popup), "popup", forall_data);

  child = G_OBJECT (gtk_combo_box_get_model (GTK_COMBO_BOX (object)));
  if (child)
    forall_func (child, "model", forall_data);
}

static void
object_tree_tree_view_forall (GObject              *object,
                              ObjectTreeForallFunc  forall_func,
                              gpointer              forall_data)
{
  gint n_columns, i;
  GObject *child;

  child = G_OBJECT (gtk_tree_view_get_model (GTK_TREE_VIEW (object)));
  if (child)
    forall_func (child, "model", forall_data);

  child = G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (object)));
  if (child)
    forall_func (child, "selection", forall_data);

  n_columns = gtk_tree_view_get_n_columns (GTK_TREE_VIEW (object));
  for (i = 0; i < n_columns; i++)
    {
      child = G_OBJECT (gtk_tree_view_get_column (GTK_TREE_VIEW (object), i));
      forall_func (child, NULL, forall_data);
    }
}

static void
object_tree_icon_view_forall (GObject              *object,
                              ObjectTreeForallFunc  forall_func,
                              gpointer              forall_data)
{
  GObject *child;

  child = G_OBJECT (gtk_icon_view_get_model (GTK_ICON_VIEW (object)));
  if (child)
    forall_func (child, "model", forall_data);
}

typedef struct {
  ObjectTreeForallFunc forall_func;
  gpointer forall_data;
  GObject *parent;
} ParentForallData;

static gboolean
cell_callback (GtkCellRenderer *renderer,
               gpointer         data)
{
  ParentForallData *d = data;
  gpointer cell_layout;

  cell_layout = g_object_get_data (d->parent, "gtk-inspector-cell-layout");
  g_object_set_data (G_OBJECT (renderer), "gtk-inspector-cell-layout", cell_layout);
  d->forall_func (G_OBJECT (renderer), NULL, d->forall_data);

  return FALSE;
}

static void
object_tree_cell_area_forall (GObject              *object,
                              ObjectTreeForallFunc  forall_func,
                              gpointer              forall_data)
{
  ParentForallData data = {
    forall_func,
    forall_data,
    object
  };

  gtk_cell_area_foreach (GTK_CELL_AREA (object), cell_callback, &data);
}

static void
object_tree_cell_layout_forall (GObject              *object,
                                ObjectTreeForallFunc  forall_func,
                                gpointer              forall_data)
{
  GtkCellArea *area;

  /* cell areas handle their own stuff */
  if (GTK_IS_CELL_AREA (object))
    return;

  area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (object));
  if (!area)
    return;

  g_object_set_data (G_OBJECT (area), "gtk-inspector-cell-layout", object);
  forall_func (G_OBJECT (area), "cell-area", forall_data);
}

static void
object_tree_text_view_forall (GObject              *object,
                              ObjectTreeForallFunc  forall_func,
                              gpointer              forall_data)
{
  GtkTextBuffer *buffer;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (object));
  forall_func (G_OBJECT (buffer), "buffer", forall_data);
}

static void
object_tree_text_buffer_forall (GObject              *object,
                                ObjectTreeForallFunc  forall_func,
                                gpointer              forall_data)
{
  GtkTextTagTable *tags;

  tags = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (object));
  forall_func (G_OBJECT (tags), "tag-table", forall_data);
}

static void
tag_callback (GtkTextTag *tag,
              gpointer    data)
{
  ForallData *d = data;
  gchar *name;

  g_object_get (tag, "name", &name, NULL);
  d->forall_func (G_OBJECT (tag), name, d->forall_data);
  g_free (name);
}

static void
object_tree_text_tag_table_forall (GObject              *object,
                                   ObjectTreeForallFunc  forall_func,
                                   gpointer              forall_data)
{
  ForallData data = {
    forall_func,
    forall_data
  };

  gtk_text_tag_table_foreach (GTK_TEXT_TAG_TABLE (object), tag_callback, &data);
}

static void
object_tree_application_forall (GObject              *object,
                                ObjectTreeForallFunc  forall_func,
                                gpointer              forall_data)
{
  GObject *menu;

  menu = (GObject *)gtk_application_get_app_menu (GTK_APPLICATION (object));
  if (menu)
    forall_func (menu, "app-menu", forall_data);

  menu = (GObject *)gtk_application_get_menubar (GTK_APPLICATION (object));
  if (menu)
    forall_func (menu, "menubar", forall_data);
}

/* Note:
 * This tree must be sorted with the most specific types first.
 * We iterate over it top to bottom and return the first match
 * using g_type_is_a ()
 */
static const ObjectTreeClassFuncs object_tree_class_funcs[] = {
  {
    gtk_application_get_type,
    object_tree_get_parent_default,
    object_tree_application_forall,
    object_tree_get_sensitive_default
  },
  {
    gtk_text_tag_table_get_type,
    object_tree_get_parent_default,
    object_tree_text_tag_table_forall,
    object_tree_get_sensitive_default
  },
  {
    gtk_text_buffer_get_type,
    object_tree_get_parent_default,
    object_tree_text_buffer_forall,
    object_tree_get_sensitive_default
  },
  {
    gtk_text_view_get_type,
    object_tree_widget_get_parent,
    object_tree_text_view_forall,
    object_tree_widget_get_sensitive
  },
  {
    gtk_icon_view_get_type,
    object_tree_widget_get_parent,
    object_tree_icon_view_forall,
    object_tree_widget_get_sensitive
  },
  {
    gtk_tree_view_get_type,
    object_tree_widget_get_parent,
    object_tree_tree_view_forall,
    object_tree_widget_get_sensitive
  },
  {
    gtk_combo_box_get_type,
    object_tree_widget_get_parent,
    object_tree_combo_box_forall,
    object_tree_widget_get_sensitive
  },
  {
    gtk_menu_item_get_type,
    object_tree_widget_get_parent,
    object_tree_menu_item_forall,
    object_tree_widget_get_sensitive
  },
  {
    gtk_container_get_type,
    object_tree_widget_get_parent,
    object_tree_container_forall,
    object_tree_widget_get_sensitive
  },
  {
    gtk_widget_get_type,
    object_tree_widget_get_parent,
    object_tree_widget_forall,
    object_tree_widget_get_sensitive
  },
  {
    gtk_tree_model_filter_get_type,
    object_tree_get_parent_default,
    object_tree_tree_model_filter_forall,
    object_tree_get_sensitive_default
  },
  {
    gtk_tree_model_sort_get_type,
    object_tree_get_parent_default,
    object_tree_tree_model_sort_forall,
    object_tree_get_sensitive_default
  },
  {
    gtk_cell_area_get_type,
    object_tree_get_parent_default,
    object_tree_cell_area_forall,
    object_tree_get_sensitive_default
  },
  {
    gtk_cell_layout_get_type,
    object_tree_get_parent_default,
    object_tree_cell_layout_forall,
    object_tree_get_sensitive_default
  },
  {
    g_object_get_type,
    object_tree_get_parent_default,
    object_tree_forall_default,
    object_tree_get_sensitive_default
  },
};

static const ObjectTreeClassFuncs *
find_class_funcs (GObject *object)
{
  GType object_type;
  guint i;

  object_type = G_OBJECT_TYPE (object);

  for (i = 0; i < G_N_ELEMENTS (object_tree_class_funcs); i++)
    {
      if (g_type_is_a (object_type, object_tree_class_funcs[i].get_type ()))
        return &object_tree_class_funcs[i];
    }

  g_assert_not_reached ();

  return NULL;
}

static GObject *
object_get_parent (GObject *object)
{
  const ObjectTreeClassFuncs *funcs;

  funcs = find_class_funcs (object);
  
  return funcs->get_parent (object);
}

static void
object_forall (GObject              *object,
               ObjectTreeForallFunc  forall_func,
               gpointer              forall_data)
{
  GType object_type;
  guint i;

  object_type = G_OBJECT_TYPE (object);

  for (i = 0; i < G_N_ELEMENTS (object_tree_class_funcs); i++)
    {
      if (g_type_is_a (object_type, object_tree_class_funcs[i].get_type ()))
        object_tree_class_funcs[i].forall (object, forall_func, forall_data);
    }
}

static gboolean
object_get_sensitive (GObject *object)
{
  const ObjectTreeClassFuncs *funcs;

  funcs = find_class_funcs (object);
  
  return funcs->get_sensitive (object);
}

static void
on_row_activated (GtkTreeView            *tree,
                  GtkTreePath            *path,
                  GtkTreeViewColumn      *col,
                  GtkInspectorObjectTree *wt)
{
  GtkTreeIter iter;
  GObject *object;
  gchar *name;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (wt->priv->model), &iter, path);
  gtk_tree_model_get (GTK_TREE_MODEL (wt->priv->model), &iter,
                      OBJECT, &object,
                      OBJECT_NAME, &name,
                      -1);

  g_signal_emit (wt, signals[OBJECT_ACTIVATED], 0, object, name);

  g_free (name);
}

GObject *
gtk_inspector_object_tree_get_selected (GtkInspectorObjectTree *wt)
{
  GObject *object;
  GtkTreeIter iter;
  GtkTreeSelection *sel;
  GtkTreeModel *model;

  object = NULL;
  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (wt->priv->tree));
  if (gtk_tree_selection_get_selected (sel, &model, &iter))
    gtk_tree_model_get (model, &iter,
                        OBJECT, &object,
                        -1);

  return object;
}

static void
on_selection_changed (GtkTreeSelection       *selection,
                      GtkInspectorObjectTree *wt)
{
  GObject *object;
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    gtk_tree_walk_reset (wt->priv->walk, &iter);
  else
    gtk_tree_walk_reset (wt->priv->walk, NULL);
  object = gtk_inspector_object_tree_get_selected (wt);
  g_signal_emit (wt, signals[OBJECT_SELECTED], 0, object);
}

typedef struct {
  GObject *dead_object;
  GtkTreeWalk *walk;
  GtkTreePath *walk_pos;
} RemoveData;

static gboolean
remove_cb (GtkTreeModel *model,
           GtkTreePath  *path,
           GtkTreeIter  *iter,
           gpointer      data)
{
  RemoveData *remove_data = data;
  GObject *lookup;

  gtk_tree_model_get (model, iter, OBJECT, &lookup, -1);

  if (lookup == remove_data->dead_object)
    {
      if (remove_data->walk_pos != NULL &&
          gtk_tree_path_compare (path, remove_data->walk_pos) == 0)
        gtk_tree_walk_reset (remove_data->walk, NULL);

      gtk_tree_store_remove (GTK_TREE_STORE (model), iter);

      return TRUE;
    }

  return FALSE;
}

static void
gtk_object_tree_remove_dead_object (gpointer data, GObject *dead_object)
{
  GtkInspectorObjectTree *wt = data;
  GtkTreeIter iter;
  RemoveData remove_data;

  remove_data.dead_object = dead_object;
  remove_data.walk = wt->priv->walk;
  if (gtk_tree_walk_get_position (wt->priv->walk, &iter))
    remove_data.walk_pos = gtk_tree_model_get_path (GTK_TREE_MODEL (wt->priv->model), &iter);
  else
    remove_data.walk_pos = NULL;

  gtk_tree_model_foreach (GTK_TREE_MODEL (wt->priv->model), remove_cb, &remove_data);

  if (remove_data.walk_pos)
    gtk_tree_path_free (remove_data.walk_pos);
}

static gboolean
weak_unref_cb (GtkTreeModel *model,
               GtkTreePath  *path,
               GtkTreeIter  *iter,
               gpointer      data)
{
  GtkInspectorObjectTree *wt = data;
  GObject *object;

  gtk_tree_model_get (model, iter, OBJECT, &object, -1);

  g_object_weak_unref (object, gtk_object_tree_remove_dead_object, wt);

  return FALSE;
}

static void
clear_store (GtkInspectorObjectTree *wt)
{
  if (wt->priv->model)
    {
      gtk_tree_model_foreach (GTK_TREE_MODEL (wt->priv->model), weak_unref_cb, wt);
      gtk_tree_store_clear (wt->priv->model);
      gtk_tree_walk_reset (wt->priv->walk, NULL);
    }
}

static gboolean
map_or_unmap (GSignalInvocationHint *ihint,
              guint                  n_params,
              const GValue          *params,
              gpointer               data)
{
  GtkInspectorObjectTree *wt = data;
  GtkWidget *widget;
  GtkTreeIter iter;

  widget = g_value_get_object (params);
  if (gtk_inspector_object_tree_find_object (wt, G_OBJECT (widget), &iter))
    gtk_tree_store_set (wt->priv->model, &iter,
                        SENSITIVE, gtk_widget_get_mapped (widget),
                        -1);
  return TRUE;
}

static void
move_search_to_row (GtkInspectorObjectTree *wt,
                    GtkTreeIter            *iter)
{
  GtkTreeSelection *selection;
  GtkTreePath *path;

  selection = gtk_tree_view_get_selection (wt->priv->tree);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (wt->priv->model), iter);
  gtk_tree_view_expand_to_path (wt->priv->tree, path);
  gtk_tree_selection_select_path (selection, path);
  gtk_tree_view_scroll_to_cell (wt->priv->tree, path, NULL, TRUE, 0.5, 0.0);
  gtk_tree_path_free (path);
}

static gboolean
key_press_event (GtkWidget              *window,
                 GdkEvent               *event,
                 GtkInspectorObjectTree *wt)
{
  if (gtk_widget_get_mapped (GTK_WIDGET (wt)))
    {
      GdkModifierType default_accel;
      gboolean search_started;

      search_started = gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (wt->priv->search_bar));
      default_accel = gtk_widget_get_modifier_mask (GTK_WIDGET (wt), GDK_MODIFIER_INTENT_PRIMARY_ACCELERATOR);

      if (search_started &&
          (event->key.keyval == GDK_KEY_Return ||
           event->key.keyval == GDK_KEY_ISO_Enter ||
           event->key.keyval == GDK_KEY_KP_Enter))
        {
          GtkTreeSelection *selection;
          GtkTreeModel *model;
          GtkTreeIter iter;
          GtkTreePath *path;

          selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (wt->priv->tree));
          if (gtk_tree_selection_get_selected (selection, &model, &iter))
            {
              path = gtk_tree_model_get_path (model, &iter);
              gtk_tree_view_row_activated (GTK_TREE_VIEW (wt->priv->tree),
                                           path,
                                           wt->priv->object_column);
              gtk_tree_path_free (path);

              return GDK_EVENT_STOP;
            }
          else
            return GDK_EVENT_PROPAGATE;
        }
      else if (search_started &&
               (event->key.keyval == GDK_KEY_Escape))
        {
          gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (wt->priv->search_bar), FALSE);
          return GDK_EVENT_STOP;
        }
      else if (search_started &&
               ((event->key.state & (default_accel | GDK_SHIFT_MASK)) == (default_accel | GDK_SHIFT_MASK)) &&
               (event->key.keyval == GDK_KEY_g || event->key.keyval == GDK_KEY_G))
        {
          GtkTreeIter iter;
          if (gtk_tree_walk_next_match (wt->priv->walk, TRUE, TRUE, &iter))
            move_search_to_row (wt, &iter);
          else
            gtk_widget_error_bell (GTK_WIDGET (wt));

          return GDK_EVENT_STOP;
        }
      else if (search_started &&
               ((event->key.state & (default_accel | GDK_SHIFT_MASK)) == default_accel) &&
               (event->key.keyval == GDK_KEY_g || event->key.keyval == GDK_KEY_G))
        {
          GtkTreeIter iter;

          if (gtk_tree_walk_next_match (wt->priv->walk, TRUE, FALSE, &iter))
            move_search_to_row (wt, &iter);
          else
            gtk_widget_error_bell (GTK_WIDGET (wt));

          return GDK_EVENT_STOP;
        }

      return gtk_search_bar_handle_event (GTK_SEARCH_BAR (wt->priv->search_bar), event);
    }
  else
    return GDK_EVENT_PROPAGATE;
}

static void
on_hierarchy_changed (GtkWidget *widget,
                      GtkWidget *previous_toplevel)
{
  if (previous_toplevel)
    g_signal_handlers_disconnect_by_func (previous_toplevel, key_press_event, widget);
  g_signal_connect (gtk_widget_get_toplevel (widget), "key-press-event",
                    G_CALLBACK (key_press_event), widget);
}

static void
on_search_changed (GtkSearchEntry         *entry,
                   GtkInspectorObjectTree *wt)
{
  GtkTreeIter iter;
  gint length;
  gboolean backwards;

  length = strlen (gtk_entry_get_text (GTK_ENTRY (entry)));
  backwards = length < wt->priv->search_length;
  wt->priv->search_length = length;

  if (length == 0)
    return;

  if (gtk_tree_walk_next_match (wt->priv->walk, backwards, backwards, &iter))
    move_search_to_row (wt, &iter);
  else if (!backwards)
    gtk_widget_error_bell (GTK_WIDGET (wt));
}

static gboolean
match_string (const gchar *string,
              const gchar *text)
{
  gchar *lower;
  gboolean match = FALSE;

  if (string)
    {
      lower = g_ascii_strdown (string, -1);
      match = g_str_has_prefix (lower, text);
      g_free (lower);
    }

  return match;
}

static gboolean
match_row (GtkTreeModel *model,
           GtkTreeIter  *iter,
           gpointer      data)
{
  GtkInspectorObjectTree *wt = data;
  gpointer object;
  gchar *type, *name, *label, *address;
  const gchar *text;
  gboolean match;

  text = gtk_entry_get_text (GTK_ENTRY (wt->priv->search_entry));
  gtk_tree_model_get (model, iter,
                      OBJECT, &object,
                      OBJECT_TYPE, &type,
                      OBJECT_NAME, &name,
                      OBJECT_LABEL, &label,
                      -1);
  address = g_strdup_printf ("%p", object);

  match = (match_string (address, text) ||
           match_string (type, text) ||
           match_string (name, text) ||
           match_string (label, text));

  g_free (address);
  g_free (type);
  g_free (name);
  g_free (label);

  return match;
}

static void
search_mode_changed (GObject                *search_bar,
                     GParamSpec             *pspec,
                     GtkInspectorObjectTree *wt)
{
  if (!gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (search_bar)))
    {
      gtk_tree_walk_reset (wt->priv->walk, NULL);
      wt->priv->search_length = 0;
    }
}

static void
next_match (GtkButton              *button,
            GtkInspectorObjectTree *wt)
{
  if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (wt->priv->search_bar)))
    {
      GtkTreeIter iter;

      if (gtk_tree_walk_next_match (wt->priv->walk, TRUE, FALSE, &iter))
        move_search_to_row (wt, &iter);
      else
        gtk_widget_error_bell (GTK_WIDGET (wt));
    }
}

static void
previous_match (GtkButton              *button,
                GtkInspectorObjectTree *wt)
{
  if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (wt->priv->search_bar)))
    {
      GtkTreeIter iter;

      if (gtk_tree_walk_next_match (wt->priv->walk, TRUE, TRUE, &iter))
        move_search_to_row (wt, &iter);
      else
        gtk_widget_error_bell (GTK_WIDGET (wt));
    }
}

static void
stop_search (GtkWidget              *entry,
             GtkInspectorObjectTree *wt)
{
  gtk_entry_set_text (GTK_ENTRY (wt->priv->search_entry), "");
  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (wt->priv->search_bar), FALSE);
}

static void
gtk_inspector_object_tree_init (GtkInspectorObjectTree *wt)
{
  guint signal_id;

  wt->priv = gtk_inspector_object_tree_get_instance_private (wt);
  gtk_widget_init_template (GTK_WIDGET (wt));

  gtk_search_bar_connect_entry (GTK_SEARCH_BAR (wt->priv->search_bar),
                                GTK_ENTRY (wt->priv->search_entry));

  g_signal_connect (wt->priv->search_bar, "notify::search-mode-enabled",
                    G_CALLBACK (search_mode_changed), wt);
  wt->priv->walk = gtk_tree_walk_new (GTK_TREE_MODEL (wt->priv->model), match_row, wt, NULL);

  signal_id = g_signal_lookup ("map", GTK_TYPE_WIDGET);
  wt->priv->map_hook = g_signal_add_emission_hook (signal_id, 0,
                                                   map_or_unmap, wt, NULL);
  signal_id = g_signal_lookup ("unmap", GTK_TYPE_WIDGET);
  wt->priv->unmap_hook = g_signal_add_emission_hook (signal_id, 0,
                                                   map_or_unmap, wt, NULL);

  gtk_inspector_object_tree_append_object (wt, G_OBJECT (gtk_settings_get_default ()), NULL, NULL);
}

static void
gtk_inspector_object_tree_dispose (GObject *object)
{
  GtkInspectorObjectTree *wt = GTK_INSPECTOR_OBJECT_TREE (object);

  clear_store (wt);

  G_OBJECT_CLASS (gtk_inspector_object_tree_parent_class)->dispose (object);
}

static void
gtk_inspector_object_tree_finalize (GObject *object)
{
  GtkInspectorObjectTree *wt = GTK_INSPECTOR_OBJECT_TREE (object);
  guint signal_id;

  signal_id = g_signal_lookup ("map", GTK_TYPE_WIDGET);
  g_signal_remove_emission_hook (signal_id, wt->priv->map_hook);
  signal_id = g_signal_lookup ("unmap", GTK_TYPE_WIDGET);
  g_signal_remove_emission_hook (signal_id, wt->priv->unmap_hook);

  gtk_tree_walk_free (wt->priv->walk);

  G_OBJECT_CLASS (gtk_inspector_object_tree_parent_class)->finalize (object);
}

static void
gtk_inspector_object_tree_class_init (GtkInspectorObjectTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_inspector_object_tree_finalize;
  object_class->dispose = gtk_inspector_object_tree_dispose;

  signals[OBJECT_ACTIVATED] =
      g_signal_new ("object-activated",
                    G_OBJECT_CLASS_TYPE (klass),
                    G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                    G_STRUCT_OFFSET (GtkInspectorObjectTreeClass, object_activated),
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 2, G_TYPE_OBJECT, G_TYPE_STRING);

  signals[OBJECT_SELECTED] =
      g_signal_new ("object-selected",
                    G_OBJECT_CLASS_TYPE (klass),
                    G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                    G_STRUCT_OFFSET (GtkInspectorObjectTreeClass, object_selected),
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 1, G_TYPE_OBJECT);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/object-tree.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorObjectTree, model);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorObjectTree, tree);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorObjectTree, object_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorObjectTree, search_bar);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorObjectTree, search_entry);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_hierarchy_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_search_changed);
  gtk_widget_class_bind_template_callback (widget_class, next_match);
  gtk_widget_class_bind_template_callback (widget_class, previous_match);
  gtk_widget_class_bind_template_callback (widget_class, stop_search);
}

typedef struct
{
  GtkInspectorObjectTree *wt;
  GtkTreeIter *iter;
  GObject *parent;
} FindAllData;

static void
child_callback (GObject    *object,
                const char *name,
                gpointer    data)
{
  FindAllData *d = data;

  gtk_inspector_object_tree_append_object (d->wt, object, d->iter, NULL);
}

void
gtk_inspector_object_tree_append_object (GtkInspectorObjectTree *wt,
                                         GObject                *object,
                                         GtkTreeIter            *parent_iter,
                                         const gchar            *name)
{
  GtkTreeIter iter;
  const gchar *class_name;
  gchar *classes;
  const gchar *label;
  FindAllData data;

  class_name = G_OBJECT_CLASS_NAME (G_OBJECT_GET_CLASS (object));

  if (GTK_IS_WIDGET (object))
    {
      const gchar *id;
      GtkStyleContext *context;
      GList *list, *l;
      GString *string;

      id = gtk_widget_get_name (GTK_WIDGET (object));
      if (name == NULL && id != NULL && g_strcmp0 (id, class_name) != 0)
        name = id;

      context = gtk_widget_get_style_context (GTK_WIDGET (object));
      string = g_string_new ("");
      list = gtk_style_context_list_classes (context);
      for (l = list; l; l = l->next)
        {
          if (string->len > 0)
            g_string_append_c (string, ' ');
          g_string_append (string, (gchar *)l->data);
        }
      classes = g_string_free (string, FALSE);
      g_list_free (list);
    }
  else
    {
      if (parent_iter)
        {
          GObject *parent;

          gtk_tree_model_get (GTK_TREE_MODEL (wt->priv->model), parent_iter,
                              OBJECT, &parent,
                              -1);
          g_object_set_data (object, "inspector-object-tree-parent", parent);
        }
      classes = g_strdup ("");
    }

  if (GTK_IS_BUILDABLE (object))
    {
      const gchar *id;
      id = gtk_buildable_get_name (GTK_BUILDABLE (object));
      if (name == NULL && id != NULL && !g_str_has_prefix (id, "___object_"))
        name = id;
    }

  if (name == NULL)
    name = "";

  if (GTK_IS_LABEL (object))
    label = gtk_label_get_text (GTK_LABEL (object));
  else if (GTK_IS_BUTTON (object))
    label = gtk_button_get_label (GTK_BUTTON (object));
  else if (GTK_IS_WINDOW (object))
    label = gtk_window_get_title (GTK_WINDOW (object));
  else if (GTK_IS_TREE_VIEW_COLUMN (object))
    label = gtk_tree_view_column_get_title (GTK_TREE_VIEW_COLUMN (object));
  else
    label = "";

  gtk_tree_store_append (wt->priv->model, &iter, parent_iter);
  gtk_tree_store_set (wt->priv->model, &iter,
                      OBJECT, object,
                      OBJECT_TYPE, class_name,
                      OBJECT_NAME, name,
                      OBJECT_LABEL, label,
                      OBJECT_CLASSES, classes,
                      SENSITIVE, object_get_sensitive (object),
                      -1);

  if (name && *name)
    {
      gchar *title;
      title = g_strconcat (class_name, " — ", name, NULL);
      g_object_set_data_full (object, "gtk-inspector-object-title", title, g_free);
    }
  else
    {
      g_object_set_data (object, "gtk-inspector-object-title", (gpointer)class_name);
    }

  g_free (classes);

  g_object_weak_ref (object, gtk_object_tree_remove_dead_object, wt);
  
  data.wt = wt;
  data.iter = &iter;
  data.parent = object;

  object_forall (object, child_callback, &data);
}

static void
block_selection_changed (GtkInspectorObjectTree *wt)
{
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (wt->priv->tree));
  g_signal_handlers_block_by_func (selection, on_selection_changed, wt);
}

static void
unblock_selection_changed (GtkInspectorObjectTree *wt)
{
  GtkTreeSelection *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (wt->priv->tree));
  g_signal_handlers_unblock_by_func (selection, on_selection_changed, wt);
}

gboolean
select_object_internal (GtkInspectorObjectTree *wt,
                        GObject                *object,
                        gboolean                activate)
{
  GtkTreeIter iter;

  if (gtk_inspector_object_tree_find_object (wt, object, &iter))
    {
      GtkTreePath *path;
      GtkTreeSelection *selection;

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (wt->priv->tree));
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (wt->priv->model), &iter);
      gtk_tree_view_expand_to_path (GTK_TREE_VIEW (wt->priv->tree), path);
      if (!activate)
        block_selection_changed (wt);
      gtk_tree_selection_select_iter (selection, &iter);
      if (!activate)
        unblock_selection_changed (wt);

      gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (wt->priv->tree), path, NULL, TRUE, 0.5, 0);
      if (activate)
        gtk_tree_view_row_activated (GTK_TREE_VIEW (wt->priv->tree), path, NULL);
      gtk_tree_path_free (path);

      return TRUE;
    }

  return FALSE;
}

gboolean
gtk_inspector_object_tree_select_object (GtkInspectorObjectTree *wt,
                                         GObject                *object)
{
  return select_object_internal (wt, object, TRUE);
}

void
gtk_inspector_object_tree_scan (GtkInspectorObjectTree *wt,
                                GtkWidget              *window)
{
  GtkWidget *inspector_win;
  GList *toplevels, *l;
  GdkScreen *screen;
  GObject *selected;

  block_selection_changed (wt);

  selected = gtk_inspector_object_tree_get_selected (wt);

  clear_store (wt);
  gtk_inspector_object_tree_append_object (wt, G_OBJECT (gtk_settings_get_default ()), NULL, NULL);
  if (g_application_get_default ())
    gtk_inspector_object_tree_append_object (wt, G_OBJECT (g_application_get_default ()), NULL, NULL);

  if (window)
    gtk_inspector_object_tree_append_object (wt, G_OBJECT (window), NULL, NULL);

  screen = gdk_screen_get_default ();

  inspector_win = gtk_widget_get_toplevel (GTK_WIDGET (wt));
  toplevels = gtk_window_list_toplevels ();
  for (l = toplevels; l; l = l->next)
    {
      if (GTK_IS_WINDOW (l->data) &&
          gtk_window_get_window_type (l->data) == GTK_WINDOW_TOPLEVEL &&
          gtk_widget_get_screen (l->data) == screen &&
          l->data != window &&
          l->data != inspector_win)
        gtk_inspector_object_tree_append_object (wt, G_OBJECT (l->data), NULL, NULL);
    }
  g_list_free (toplevels);

  gtk_tree_view_columns_autosize (GTK_TREE_VIEW (wt->priv->tree));

  if (selected)
    select_object_internal (wt, selected, FALSE);

  unblock_selection_changed (wt);
}

static gboolean
gtk_inspector_object_tree_find_object_at_parent_iter (GtkTreeModel *model,
                                                      GObject      *object,
                                                      GtkTreeIter  *parent,
                                                      GtkTreeIter  *iter)
{
  if (!gtk_tree_model_iter_children (model, iter, parent))
    return FALSE;

  do {
    GObject *lookup;

    gtk_tree_model_get (model, iter, OBJECT, &lookup, -1);

    if (lookup == object)
      return TRUE;

  } while (gtk_tree_model_iter_next (model, iter));

  return FALSE;
}

gboolean
gtk_inspector_object_tree_find_object (GtkInspectorObjectTree *wt,
                                       GObject                *object,
                                       GtkTreeIter            *iter)
{
  GtkTreeIter parent_iter;
  GObject *parent;

  parent = object_get_parent (object);
  if (parent)
    {
      if (!gtk_inspector_object_tree_find_object (wt, parent, &parent_iter))
        return FALSE;

      return gtk_inspector_object_tree_find_object_at_parent_iter (GTK_TREE_MODEL (wt->priv->model),
                                                                   object,
                                                                   &parent_iter,
                                                                   iter);
    }
  else
    {
      return gtk_inspector_object_tree_find_object_at_parent_iter (GTK_TREE_MODEL (wt->priv->model),
                                                                   object,
                                                                   NULL,
                                                                   iter);
    }
}


// vim: set et sw=2 ts=2:
