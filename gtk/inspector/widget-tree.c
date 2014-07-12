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

#include "widget-tree.h"
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
#include "gtktreestore.h"
#include "gtkwidgetprivate.h"

enum
{
  OBJECT,
  OBJECT_TYPE,
  OBJECT_NAME,
  OBJECT_LABEL,
  OBJECT_ADDRESS,
  SENSITIVE
};


enum
{
  WIDGET_CHANGED,
  LAST_SIGNAL
};


struct _GtkInspectorWidgetTreePrivate
{
  GtkTreeStore *model;
  GHashTable *iters;
  gulong map_hook;
  gulong unmap_hook;
};

static guint widget_tree_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorWidgetTree, gtk_inspector_widget_tree, GTK_TYPE_TREE_VIEW)

static void
on_widget_selected (GtkTreeSelection       *selection,
                    GtkInspectorWidgetTree *wt)
{
  g_signal_emit (wt, widget_tree_signals[WIDGET_CHANGED], 0);
}

typedef struct
{
  GtkInspectorWidgetTree *wt;
  GObject *object;
  GtkTreeRowReference *row;
} ObjectData;

static void
remove_dead_object (gpointer data, GObject *dead_object)
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
    g_object_weak_unref (od->object, remove_dead_object, od);

  g_free (od);
}

static gboolean
map_or_unmap (GSignalInvocationHint *ihint,
              guint                  n_params,
              const GValue          *params,
              gpointer               data)
{
  GtkInspectorWidgetTree *wt = data;
  GtkWidget *widget;
  GtkTreeIter iter;

  widget = g_value_get_object (params);
  if (gtk_inspector_widget_tree_find_object (wt, G_OBJECT (widget), &iter))
    gtk_tree_store_set (wt->priv->model, &iter,
                        SENSITIVE, gtk_widget_get_mapped (widget),
                        -1);

  return TRUE;
}

static void
gtk_inspector_widget_tree_init (GtkInspectorWidgetTree *wt)
{
  guint signal_id;

  wt->priv = gtk_inspector_widget_tree_get_instance_private (wt);
  wt->priv->iters = g_hash_table_new_full (g_direct_hash,
                                           g_direct_equal,
                                           NULL,
                                           (GDestroyNotify) object_data_free);
  gtk_widget_init_template (GTK_WIDGET (wt));

  signal_id = g_signal_lookup ("map", GTK_TYPE_WIDGET);
  wt->priv->map_hook = g_signal_add_emission_hook (signal_id, 0,
                                                   map_or_unmap, wt, NULL);
  signal_id = g_signal_lookup ("unmap", GTK_TYPE_WIDGET);
  wt->priv->unmap_hook = g_signal_add_emission_hook (signal_id, 0,
                                                   map_or_unmap, wt, NULL);

  gtk_inspector_widget_tree_append_object (wt, G_OBJECT (gtk_settings_get_default ()), NULL, NULL);
}

static void
gtk_inspector_widget_tree_finalize (GObject *object)
{
  GtkInspectorWidgetTree *wt = GTK_INSPECTOR_WIDGET_TREE (object);
  guint signal_id;

  signal_id = g_signal_lookup ("map", GTK_TYPE_WIDGET);
  g_signal_remove_emission_hook (signal_id, wt->priv->map_hook);
  signal_id = g_signal_lookup ("unmap", GTK_TYPE_WIDGET);
  g_signal_remove_emission_hook (signal_id, wt->priv->unmap_hook);

  G_OBJECT_CLASS (gtk_inspector_widget_tree_parent_class)->finalize (object);
}

static void
gtk_inspector_widget_tree_class_init (GtkInspectorWidgetTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_inspector_widget_tree_finalize;

  klass->widget_changed = NULL;

  widget_tree_signals[WIDGET_CHANGED] =
      g_signal_new ("widget-changed",
                    G_OBJECT_CLASS_TYPE(klass),
                    G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                    G_STRUCT_OFFSET(GtkInspectorWidgetTreeClass, widget_changed),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/widget-tree.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorWidgetTree, model);
  gtk_widget_class_bind_template_callback (widget_class, on_widget_selected);
}

GObject *
gtk_inspector_widget_tree_get_selected_object (GtkInspectorWidgetTree *wt)
{
  GtkTreeIter iter;
  GtkTreeSelection *sel;
  GtkTreeModel *model;

  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (wt));

  if (gtk_tree_selection_get_selected (sel, &model, &iter))
    {
      GObject *object;
      gtk_tree_model_get (model, &iter,
                          OBJECT, &object,
                          -1);
      return object;
    }

  return NULL;
}

typedef struct
{
  GtkInspectorWidgetTree *wt;
  GtkTreeIter *iter;
  GObject *parent;
} FindAllData;

static void
child_callback (GtkWidget *widget,
                gpointer   data)
{
  FindAllData *d = data;

  gtk_inspector_widget_tree_append_object (d->wt, G_OBJECT (widget), d->iter, NULL);
}

static gboolean
cell_callback (GtkCellRenderer *renderer,
               gpointer         data)
{
  FindAllData *d = data;
  gpointer cell_layout;

  cell_layout = g_object_get_data (d->parent, "gtk-inspector-cell-layout");
  g_object_set_data (G_OBJECT (renderer), "gtk-inspector-cell-layout", cell_layout);
  gtk_inspector_widget_tree_append_object (d->wt, G_OBJECT (renderer), d->iter, NULL);

  return FALSE;
}

static void
tag_callback (GtkTextTag *tag,
              gpointer    data)
{
  FindAllData *d = data;
  gchar *name;

  g_object_get (tag, "name", &name, NULL);
  gtk_inspector_widget_tree_append_object (d->wt, G_OBJECT (tag), d->iter, name);
  g_free (name);
}

void
gtk_inspector_widget_tree_append_object (GtkInspectorWidgetTree *wt,
                                         GObject                *object,
                                         GtkTreeIter            *parent_iter,
                                         const gchar            *name)
{
  GtkTreeIter iter;
  GtkTreePath *path;
  const gchar *class_name;
  gchar *address;
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
      id = gtk_widget_get_name (GTK_WIDGET (object));
       if (name == NULL && id != NULL && g_strcmp0 (id, class_name) != 0)
         name = id;
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

  address = g_strdup_printf ("%p", object);

  gtk_tree_store_append (wt->priv->model, &iter, parent_iter);
  gtk_tree_store_set (wt->priv->model, &iter,
                      OBJECT, object,
                      OBJECT_TYPE, class_name,
                      OBJECT_NAME, name,
                      OBJECT_LABEL, label,
                      OBJECT_ADDRESS, address,
                      SENSITIVE, mapped,
                      -1);

  od = g_new0 (ObjectData, 1);
  od->wt = wt;
  od->object = object;
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (wt->priv->model), &iter);
  od->row = gtk_tree_row_reference_new (GTK_TREE_MODEL (wt->priv->model), path);
  gtk_tree_path_free (path);

  g_hash_table_insert (wt->priv->iters, object, od);
  g_object_weak_ref (object, remove_dead_object, od);

  g_free (address);

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
  if (GTK_IS_MENU_ITEM (object))
    {
      GtkWidget *submenu;

      submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (object));
      if (submenu)
        gtk_inspector_widget_tree_append_object (wt, G_OBJECT (submenu), &iter, "submenu");
    }

  if (GTK_IS_COMBO_BOX (object))
    {
      GtkWidget *popup;

      popup = gtk_combo_box_get_popup (GTK_COMBO_BOX (object));
      if (popup)
        gtk_inspector_widget_tree_append_object (wt, G_OBJECT (popup), &iter, "popup");
    }

  if (GTK_IS_TREE_VIEW (object))
    {
      gint n_columns, i;
      GObject *child;

      child = G_OBJECT (gtk_tree_view_get_model (GTK_TREE_VIEW (object)));
      if (child)
        gtk_inspector_widget_tree_append_object (wt, child, &iter, "model");

      n_columns = gtk_tree_view_get_n_columns (GTK_TREE_VIEW (object));
      for (i = 0; i < n_columns; i++)
        {
          child = G_OBJECT (gtk_tree_view_get_column (GTK_TREE_VIEW (object), i));
          gtk_inspector_widget_tree_append_object (wt, child, &iter, NULL);
        }
    }

  if (GTK_IS_ICON_VIEW (object))
    {
      GObject *child;

      child = G_OBJECT (gtk_icon_view_get_model (GTK_ICON_VIEW (object)));
      if (child)
        gtk_inspector_widget_tree_append_object (wt, child, &iter, "model");
    }

  if (GTK_IS_COMBO_BOX (object))
    {
      GObject *child;

      child = G_OBJECT (gtk_combo_box_get_model (GTK_COMBO_BOX (object)));
      if (child)
        gtk_inspector_widget_tree_append_object (wt, child, &iter, "model");
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
      gtk_inspector_widget_tree_append_object (wt, G_OBJECT (area), &iter, "cell-area");
    }

  if (GTK_IS_TEXT_VIEW (object))
    {
      GtkTextBuffer *buffer;

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (object));
      gtk_inspector_widget_tree_append_object (wt, G_OBJECT (buffer), &iter, "buffer");
    }

  if (GTK_IS_TEXT_BUFFER (object))
    {
      GtkTextTagTable *tags;

      tags = gtk_text_buffer_get_tag_table (GTK_TEXT_BUFFER (object));
      gtk_inspector_widget_tree_append_object (wt, G_OBJECT (tags), &iter, "tag-table");
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
              gtk_inspector_widget_tree_append_object (wt, controller, &iter, phases[i].name);
            }
          g_list_free (list);
        }
    }

}

void
gtk_inspector_widget_tree_scan (GtkInspectorWidgetTree *wt,
                                GtkWidget              *window)
{
  gtk_tree_store_clear (wt->priv->model);
  g_hash_table_remove_all (wt->priv->iters);
  gtk_inspector_widget_tree_append_object (wt, G_OBJECT (gtk_settings_get_default ()), NULL, NULL);
  if (g_application_get_default ())
    gtk_inspector_widget_tree_append_object (wt, G_OBJECT (g_application_get_default ()), NULL, NULL);
  gtk_inspector_widget_tree_append_object (wt, G_OBJECT (window), NULL, NULL);

  gtk_tree_view_columns_autosize (GTK_TREE_VIEW (wt));
}

gboolean
gtk_inspector_widget_tree_find_object (GtkInspectorWidgetTree *wt,
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
gtk_inspector_widget_tree_select_object (GtkInspectorWidgetTree *wt,
                                         GObject                *object)
{
  GtkTreeIter iter;

  if (gtk_inspector_widget_tree_find_object (wt, object, &iter))
    {
      GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (wt->priv->model), &iter);
      gtk_tree_view_expand_to_path (GTK_TREE_VIEW (wt), path);
      gtk_tree_selection_select_iter (gtk_tree_view_get_selection (GTK_TREE_VIEW (wt)), &iter);
      gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (wt), path, NULL, FALSE, 0, 0);
    }
}


// vim: set et sw=2 ts=2:
