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
  GHashTable *iters;
  gulong map_hook;
  gulong unmap_hook;
  GtkTreeViewColumn *object_column;
  GtkWidget *search_bar;
  GtkWidget *search_entry;
  GtkTreeWalk *walk;
  gint search_length;
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorObjectTree, gtk_inspector_object_tree, GTK_TYPE_BOX)

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


typedef struct
{
  GtkInspectorObjectTree *wt;
  GObject *object;
  GtkTreeRowReference *row;
} ObjectData;

static void
gtk_object_tree_remove_dead_object (gpointer data, GObject *dead_object)
{
  ObjectData *od = data;

  if (gtk_tree_row_reference_valid (od->row))
    {
      GtkTreePath *path;
      GtkTreeIter iter;
      path = gtk_tree_row_reference_get_path (od->row);
      gtk_tree_model_get_iter (GTK_TREE_MODEL (od->wt->priv->model), &iter, path);
      gtk_tree_store_remove (od->wt->priv->model, &iter);
      gtk_tree_path_free (path);
    }
  od->object = NULL;
  g_hash_table_remove (od->wt->priv->iters, dead_object);
}

static void
object_data_free (gpointer data)
{
  ObjectData *od = data;

  gtk_tree_row_reference_free (od->row);

  if (od->object)
    g_object_weak_unref (od->object, gtk_object_tree_remove_dead_object, od);

  g_free (od);
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
  gchar *type, *name, *label;
  const gchar *text;
  gboolean match;

  text = gtk_entry_get_text (GTK_ENTRY (wt->priv->search_entry));
  gtk_tree_model_get (model, iter,
                      OBJECT_TYPE, &type,
                      OBJECT_NAME, &name,
                      OBJECT_LABEL, &label,
                      -1);

  match = (match_string (type, text) ||
           match_string (name, text) ||
           match_string (label, text));

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
gtk_inspector_object_tree_init (GtkInspectorObjectTree *wt)
{
  guint signal_id;

  wt->priv = gtk_inspector_object_tree_get_instance_private (wt);
  wt->priv->iters = g_hash_table_new_full (g_direct_hash,
                                           g_direct_equal,
                                           NULL,
                                           (GDestroyNotify) object_data_free);
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
gtk_inspector_object_tree_finalize (GObject *object)
{
  GtkInspectorObjectTree *wt = GTK_INSPECTOR_OBJECT_TREE (object);
  guint signal_id;

  g_hash_table_unref (wt->priv->iters);

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
}

typedef struct
{
  GtkInspectorObjectTree *wt;
  GtkTreeIter *iter;
  GObject *parent;
} FindAllData;

static void
child_callback (GtkWidget *widget,
                gpointer   data)
{
  FindAllData *d = data;

  gtk_inspector_object_tree_append_object (d->wt, G_OBJECT (widget), d->iter, NULL);
}

static gboolean
cell_callback (GtkCellRenderer *renderer,
               gpointer         data)
{
  FindAllData *d = data;
  gpointer cell_layout;

  cell_layout = g_object_get_data (d->parent, "gtk-inspector-cell-layout");
  g_object_set_data (G_OBJECT (renderer), "gtk-inspector-cell-layout", cell_layout);
  gtk_inspector_object_tree_append_object (d->wt, G_OBJECT (renderer), d->iter, NULL);

  return FALSE;
}

static void
tag_callback (GtkTextTag *tag,
              gpointer    data)
{
  FindAllData *d = data;
  gchar *name;

  g_object_get (tag, "name", &name, NULL);
  gtk_inspector_object_tree_append_object (d->wt, G_OBJECT (tag), d->iter, name);
  g_free (name);
}

void
gtk_inspector_object_tree_append_object (GtkInspectorObjectTree *wt,
                                         GObject                *object,
                                         GtkTreeIter            *parent_iter,
                                         const gchar            *name)
{
  GtkTreeIter iter;
  GtkTreePath *path;
  const gchar *class_name;
  gchar *classes;
  gboolean mapped;
  ObjectData *od;
  const gchar *label;

  if (GTK_IS_WIDGET (object))
    mapped = gtk_widget_get_mapped (GTK_WIDGET (object));
  else
    mapped = TRUE;

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
    classes = g_strdup ("");

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
                      SENSITIVE, mapped,
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

  od = g_new0 (ObjectData, 1);
  od->wt = wt;
  od->object = object;
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (wt->priv->model), &iter);
  od->row = gtk_tree_row_reference_new (GTK_TREE_MODEL (wt->priv->model), path);
  gtk_tree_path_free (path);

  g_hash_table_insert (wt->priv->iters, object, od);
  g_object_weak_ref (object, gtk_object_tree_remove_dead_object, od);

  if (GTK_IS_CONTAINER (object))
    {
      FindAllData data;

      data.wt = wt;
      data.iter = &iter;
      data.parent = object;

      gtk_container_forall (GTK_CONTAINER (object), child_callback, &data);
    }

  /* Below are special cases for dependent objects which are not
   * children in the GtkContainer sense, but which we still want
   * to show in the tree right away.
   */
  if (GTK_IS_TREE_MODEL_SORT (object))
    {
      GObject *child = G_OBJECT (gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (object)));
      if (child)
        gtk_inspector_object_tree_append_object (wt, child, &iter, "model");
    }

  if (GTK_IS_TREE_MODEL_FILTER (object))
    {
      GObject *child = G_OBJECT (gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (object)));
      if (child)
        gtk_inspector_object_tree_append_object (wt, child, &iter, "model");
    }

  if (GTK_IS_MENU_ITEM (object))
    {
      GtkWidget *submenu;

      submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (object));
      if (submenu)
        gtk_inspector_object_tree_append_object (wt, G_OBJECT (submenu), &iter, "submenu");
    }

  if (GTK_IS_COMBO_BOX (object))
    {
      GtkWidget *popup;
      GObject *child;

      popup = gtk_combo_box_get_popup (GTK_COMBO_BOX (object));
      if (popup)
        gtk_inspector_object_tree_append_object (wt, G_OBJECT (popup), &iter, "popup");

      child = G_OBJECT (gtk_combo_box_get_model (GTK_COMBO_BOX (object)));
      if (child)
        gtk_inspector_object_tree_append_object (wt, child, &iter, "model");
    }

  if (GTK_IS_TREE_VIEW (object))
    {
      gint n_columns, i;
      GObject *child;

      child = G_OBJECT (gtk_tree_view_get_model (GTK_TREE_VIEW (object)));
      if (child)
        gtk_inspector_object_tree_append_object (wt, child, &iter, "model");

      child = G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (object)));
      if (child)
        gtk_inspector_object_tree_append_object (wt, child, &iter, "selection");

      n_columns = gtk_tree_view_get_n_columns (GTK_TREE_VIEW (object));
      for (i = 0; i < n_columns; i++)
        {
          child = G_OBJECT (gtk_tree_view_get_column (GTK_TREE_VIEW (object), i));
          gtk_inspector_object_tree_append_object (wt, child, &iter, NULL);
        }
    }

  if (GTK_IS_ICON_VIEW (object))
    {
      GObject *child;

      child = G_OBJECT (gtk_icon_view_get_model (GTK_ICON_VIEW (object)));
      if (child)
        gtk_inspector_object_tree_append_object (wt, child, &iter, "model");
    }

  if (GTK_IS_CELL_AREA (object))
    {
      FindAllData data;

      data.wt = wt;
      data.iter = &iter;
      data.parent = object;

      gtk_cell_area_foreach (GTK_CELL_AREA (object), cell_callback, &data);
    }
  else if (GTK_IS_CELL_LAYOUT (object))
    {
      GtkCellArea *area;

      area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (object));
      g_object_set_data (G_OBJECT (area), "gtk-inspector-cell-layout", object);
      gtk_inspector_object_tree_append_object (wt, G_OBJECT (area), &iter, "cell-area");
    }

  if (GTK_IS_TEXT_VIEW (object))
    {
      GtkTextBuffer *buffer;

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (object));
      gtk_inspector_object_tree_append_object (wt, G_OBJECT (buffer), &iter, "buffer");
    }

  if (GTK_IS_TEXT_BUFFER (object))
    {
      GtkTextTagTable *tags;

      tags = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (object));
      gtk_inspector_object_tree_append_object (wt, G_OBJECT (tags), &iter, "tag-table");
    }

  if (GTK_IS_TEXT_TAG_TABLE (object))
    {
      FindAllData data;

      data.wt = wt;
      data.iter = &iter;
      data.parent = object;

      gtk_text_tag_table_foreach (GTK_TEXT_TAG_TABLE (object), tag_callback, &data);
    }

  if (GTK_IS_WIDGET (object))
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
              gtk_inspector_object_tree_append_object (wt, controller, &iter, phases[i].name);
            }
          g_list_free (list);
        }

       if (gtk_widget_is_toplevel (GTK_WIDGET (object)))
         {
           GObject *clock;

           clock = (GObject *)gtk_widget_get_frame_clock (GTK_WIDGET (object));
           if (clock)
             gtk_inspector_object_tree_append_object (wt, clock, &iter, "frame-clock");
         }
    }

  if (GTK_IS_APPLICATION (object))
    {
      GObject *menu;

      menu = (GObject *)gtk_application_get_app_menu (GTK_APPLICATION (object));
      if (menu)
        gtk_inspector_object_tree_append_object (wt, menu, &iter, "app-menu");

      menu = (GObject *)gtk_application_get_menubar (GTK_APPLICATION (object));
      if (menu)
        gtk_inspector_object_tree_append_object (wt, menu, &iter, "menubar");
    }
}

void
gtk_inspector_object_tree_scan (GtkInspectorObjectTree *wt,
                                GtkWidget              *window)
{
  GtkWidget *inspector_win;
  GList *toplevels, *l;
  GdkScreen *screen;

  gtk_tree_store_clear (wt->priv->model);
  g_hash_table_remove_all (wt->priv->iters);
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
}

gboolean
gtk_inspector_object_tree_find_object (GtkInspectorObjectTree *wt,
                                       GObject                *object,
                                       GtkTreeIter            *iter)
{
  ObjectData *od;

  od = g_hash_table_lookup (wt->priv->iters, object);
  if (od && gtk_tree_row_reference_valid (od->row))
    {
      GtkTreePath *path;

      path = gtk_tree_row_reference_get_path (od->row);
      gtk_tree_model_get_iter (GTK_TREE_MODEL (wt->priv->model), iter, path);
      gtk_tree_path_free (path);

      return TRUE;
    }

  return FALSE;
}

void
gtk_inspector_object_tree_select_object (GtkInspectorObjectTree *wt,
                                         GObject                *object)
{
  GtkTreeIter iter;

  if (gtk_inspector_object_tree_find_object (wt, object, &iter))
    {
      GtkTreePath *path;

      path = gtk_tree_model_get_path (GTK_TREE_MODEL (wt->priv->model), &iter);
      gtk_tree_view_expand_to_path (GTK_TREE_VIEW (wt->priv->tree), path);
      gtk_tree_selection_select_iter (gtk_tree_view_get_selection (GTK_TREE_VIEW (wt->priv->tree)), &iter);
      gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (wt->priv->tree), path, NULL, TRUE, 0.5, 0);
      gtk_tree_view_row_activated (GTK_TREE_VIEW (wt->priv->tree), path, NULL);
      gtk_tree_path_free (path);
    }
}


// vim: set et sw=2 ts=2:
