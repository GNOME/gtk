/*
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

#include "classes-list.h"

#include "gtkliststore.h"
#include "gtktreeview.h"
#include "gtkcellrenderertoggle.h"
#include "gtkbutton.h"
#include "gtkdialog.h"
#include "gtkstylecontext.h"
#include "gtklabel.h"

enum
{
  COLUMN_ENABLED,
  COLUMN_NAME,
  COLUMN_STYLE
};

typedef struct
{
  gboolean enabled;
  PangoStyle style;
} GtkInspectorClassesListByContext;

struct _GtkInspectorClassesListPrivate
{
  GtkListStore *model;
  GtkStyleContext *context;
  GtkWidget *object_title;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorClassesList, gtk_inspector_classes_list, GTK_TYPE_BOX)

static void
set_hash_context (GtkInspectorClassesList *cl, GHashTable *hash_context)
{
  g_object_set_data_full (G_OBJECT (cl->priv->context),
                                    "gtk-inspector-hash-context",
                                    hash_context, (GDestroyNotify)g_hash_table_unref);
}

static GHashTable *
get_hash_context (GtkInspectorClassesList *cl)
{
  return (GHashTable *)g_object_get_data (G_OBJECT (cl->priv->context),
                                          "gtk-inspector-hash-context");
}

static void
enabled_toggled (GtkCellRendererToggle   *renderer,
                 const gchar             *path,
                 GtkInspectorClassesList *cl)
{
  GtkTreeIter iter;
  gboolean enabled;
  GHashTable *context;
  GtkInspectorClassesListByContext *c;
  gchar *name;

  if (!gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (cl->priv->model), &iter, path))
    {
      g_warning ("GtkInspector: Couldn't find the css class path for %s.", path);
      return;
    }

  gtk_tree_model_get (GTK_TREE_MODEL (cl->priv->model), &iter,
                      COLUMN_ENABLED, &enabled,
                      COLUMN_NAME, &name,
                      -1);
  enabled = !enabled;
  gtk_list_store_set (cl->priv->model, &iter,
                      COLUMN_ENABLED, enabled,
                      -1);

  context = get_hash_context (cl);
  if (context)
    {
      c = g_hash_table_lookup (context, name);
      if (c)
        {
          c->enabled = enabled;
          if (enabled)
            gtk_style_context_add_class (cl->priv->context, name);
          else
            gtk_style_context_remove_class (cl->priv->context, name);
        }
      else
        g_warning ("GtkInspector: Couldn't find the css class %s in the class hash table.", name);
    }
  else
    g_warning ("GtkInspector: Couldn't find the hash table for the style context for css class %s.", name);
}

static void
add_clicked (GtkButton               *button,
             GtkInspectorClassesList *cl)
{
  GtkWidget *dialog, *content_area, *entry;

  dialog = gtk_dialog_new_with_buttons (_("New class"),
                                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (cl))),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
                                         _("_OK"), GTK_RESPONSE_OK,
                                         _("Cancel"), GTK_RESPONSE_CANCEL,
                                         NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  entry = g_object_new (GTK_TYPE_ENTRY,
                        "visible", TRUE,
                        "margin", 5,
                        "placeholder-text", _("Class name"),
                        "activates-default", TRUE,
                        NULL);
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_container_add (GTK_CONTAINER (content_area), entry);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      const gchar *name;
      GHashTable *context;

      context = get_hash_context (cl);
      name = gtk_entry_get_text (GTK_ENTRY (entry));
      if (*name && !g_hash_table_contains (context, name))
        {
          GtkTreeIter tree_iter;
          GtkInspectorClassesListByContext *c;

          gtk_style_context_add_class (cl->priv->context, name);

          c = g_new0 (GtkInspectorClassesListByContext, 1);
          c->enabled = TRUE;
          c->style = PANGO_STYLE_ITALIC;
          g_hash_table_insert (context, (gpointer)g_strdup (name), c);

          gtk_list_store_append (cl->priv->model, &tree_iter);
          gtk_list_store_set (cl->priv->model, &tree_iter,
                             COLUMN_ENABLED, TRUE,
                             COLUMN_NAME, name,
                             COLUMN_STYLE, PANGO_STYLE_ITALIC,
                            -1);
        }
    }

  gtk_widget_destroy (dialog);
}

static void
read_classes_from_style_context (GtkInspectorClassesList *cl)
{
  GList *l, *classes;
  GtkInspectorClassesListByContext *c;
  GtkTreeIter tree_iter;
  GHashTable *hash_context;

  hash_context = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  classes = gtk_style_context_list_classes (cl->priv->context);

  for (l = classes; l; l = l->next)
    {
      c = g_new0 (GtkInspectorClassesListByContext, 1);
      c->enabled = TRUE;
      g_hash_table_insert (hash_context, g_strdup (l->data), c);

      gtk_list_store_append (cl->priv->model, &tree_iter);
      gtk_list_store_set (cl->priv->model, &tree_iter,
                          COLUMN_ENABLED, TRUE,
                          COLUMN_NAME, l->data,
                          COLUMN_STYLE, PANGO_STYLE_NORMAL,
                          -1);
    }
    g_list_free (classes);
    set_hash_context (cl, hash_context);
}

static void
restore_defaults_clicked (GtkButton           *button,
                          GtkInspectorClassesList *cl)
{
  GHashTableIter hash_iter;
  gchar *name;
  GtkInspectorClassesListByContext *c;
  GHashTable *hash_context;

  hash_context = get_hash_context (cl);
  g_hash_table_iter_init (&hash_iter, hash_context);
  while (g_hash_table_iter_next (&hash_iter, (gpointer *)&name, (gpointer *)&c))
    {
      if (c->style == PANGO_STYLE_ITALIC)
        gtk_style_context_remove_class (cl->priv->context, name);
      else if (!c->enabled)
        gtk_style_context_add_class (cl->priv->context, name);
    }

  gtk_list_store_clear (cl->priv->model);
  read_classes_from_style_context (cl);
}

static void
gtk_inspector_classes_list_init (GtkInspectorClassesList *cl)
{
  cl->priv = gtk_inspector_classes_list_get_instance_private (cl);
  gtk_widget_init_template (GTK_WIDGET (cl));
}

static void gtk_inspector_classes_list_remove_dead_object (gpointer data, GObject *dead_object);

static void
cleanup_context (GtkInspectorClassesList *cl)
{
  if (cl->priv->context)
    {
      g_object_weak_unref (G_OBJECT (cl->priv->context),gtk_inspector_classes_list_remove_dead_object, cl);
      cl->priv->context = NULL;
    }

  if (cl->priv->model)
    gtk_list_store_clear (cl->priv->model);
}

static void
gtk_inspector_classes_list_remove_dead_object (gpointer data, GObject *dead_object)
{
  GtkInspectorClassesList *cl = data;

  cl->priv->context = NULL;
  cleanup_context (cl);
  gtk_widget_hide (GTK_WIDGET (cl));
}

void
gtk_inspector_classes_list_set_object (GtkInspectorClassesList *cl,
                                       GObject                 *object)
{
  GHashTable *hash_context;
  GtkTreeIter tree_iter;
  GtkInspectorClassesListByContext *c;
  const gchar *title;

  cleanup_context (cl);

  if (!GTK_IS_WIDGET (object))
    {
      gtk_widget_hide (GTK_WIDGET (cl));
      return;
    }

  gtk_widget_show (GTK_WIDGET (cl));

  cl->priv->context = gtk_widget_get_style_context (GTK_WIDGET (object));

  g_object_weak_ref (G_OBJECT (cl->priv->context), gtk_inspector_classes_list_remove_dead_object, cl);

  title = (const gchar *)g_object_get_data (object, "gtk-inspector-object-title");
  gtk_label_set_label (GTK_LABEL (cl->priv->object_title), title);

  hash_context = get_hash_context (cl);
  if (hash_context)
    {
      GHashTableIter hash_iter;
      gchar *name;

      g_hash_table_iter_init (&hash_iter, hash_context);
      while (g_hash_table_iter_next (&hash_iter, (gpointer *)&name, (gpointer *)&c))
        {
          gtk_list_store_append (cl->priv->model, &tree_iter);
          gtk_list_store_set (cl->priv->model, &tree_iter,
                             COLUMN_ENABLED, c->enabled,
                             COLUMN_NAME, name,
                             COLUMN_STYLE, c->style,
                            -1);
        }
    }
  else
    read_classes_from_style_context (cl);
}

static void
gtk_inspector_classes_list_finalize (GObject *object)
{
  GtkInspectorClassesList *cl = GTK_INSPECTOR_CLASSES_LIST (object);

  cleanup_context (cl);
  
  G_OBJECT_CLASS (gtk_inspector_classes_list_parent_class)->finalize (object);
}

static void
gtk_inspector_classes_list_class_init (GtkInspectorClassesListClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_inspector_classes_list_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/classes-list.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorClassesList, model);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorClassesList, object_title);
  gtk_widget_class_bind_template_callback (widget_class, add_clicked);
  gtk_widget_class_bind_template_callback (widget_class, restore_defaults_clicked);
  gtk_widget_class_bind_template_callback (widget_class, enabled_toggled);
}

// vim: set et sw=2 ts=2:
