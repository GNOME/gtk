/*
 * Copyright (c) 2014 Benjamin Otte <otte@gnome.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicntnse,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright noticnt and this permission noticnt shall be included
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

#include "css-node-tree.h"
#include "prop-editor.h"
#include "window.h"

#include "gtktreemodelcssnode.h"
#include "gtktreeview.h"
#include "gtklabel.h"
#include "gtkpopover.h"
#include "gtk/gtkwidgetprivate.h"
#include "gtkcssproviderprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcssvalueprivate.h"
#include "gtkcssselectorprivate.h"
#include "gtkliststore.h"
#include "gtksettings.h"
#include "gtktreeview.h"
#include "gtktreeselection.h"
#include "gtktypebuiltins.h"
#include "gtkstack.h"
#include "gtknoselection.h"
#include "gtkcolumnview.h"
#include "gtkcolumnviewcolumn.h"

#include <glib/gi18n-lib.h>
#include <gtk/css/gtkcss.h>

/* {{{ CssProperty object */

typedef struct _CssProperty CssProperty;

G_DECLARE_FINAL_TYPE (CssProperty, css_property, CSS, PROPERTY, GObject);

struct _CssProperty
{
  GObject parent;

  char *name;
  char *value;
  char *location;
};

enum {
  CSS_PROPERTY_PROP_NAME = 1,
  CSS_PROPERTY_PROP_VALUE,
  CSS_PROPERTY_PROP_LOCATION,
  Css_PROPERTY_NUM_PROPERTIES
};

G_DEFINE_TYPE (CssProperty, css_property, G_TYPE_OBJECT);

static void
css_property_init (CssProperty *self)
{
}

static void
css_property_finalize (GObject *object)
{
  CssProperty *self = CSS_PROPERTY (object);

  g_free (self->name);
  g_free (self->value);
  g_free (self->location);

  G_OBJECT_CLASS (css_property_parent_class)->finalize (object);
}

static void
css_property_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  CssProperty *self = CSS_PROPERTY (object);

  switch (property_id)
    {
    case CSS_PROPERTY_PROP_NAME:
      g_value_set_string (value, self->name);
      break;

    case CSS_PROPERTY_PROP_VALUE:
      g_value_set_string (value, self->value);
      break;

    case CSS_PROPERTY_PROP_LOCATION:
      g_value_set_string (value, self->location);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
css_property_class_init (CssPropertyClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GParamSpec *pspec;

  object_class->finalize = css_property_finalize;
  object_class->get_property = css_property_get_property;

  pspec = g_param_spec_string ("name", NULL, NULL,
                               NULL,
                               G_PARAM_READABLE |
                               G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, CSS_PROPERTY_PROP_NAME, pspec);

  pspec = g_param_spec_string ("value", NULL, NULL,
                               NULL,
                               G_PARAM_READABLE |
                               G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, CSS_PROPERTY_PROP_VALUE, pspec);

  pspec = g_param_spec_string ("location", NULL, NULL,
                               NULL,
                               G_PARAM_READABLE |
                               G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class, CSS_PROPERTY_PROP_LOCATION, pspec);
}

static CssProperty *
css_property_new (const char *name,
                  const char *value,
                  const char *location)
{
  CssProperty *self;

  self = g_object_new (css_property_get_type (), NULL);

  self->name = g_strdup (name);
  self->value = g_strdup (value);
  self->location = g_strdup (location);

  return self;
}

/* }}} */

enum {
  COLUMN_NODE_NAME,
  COLUMN_NODE_VISIBLE,
  COLUMN_NODE_CLASSES,
  COLUMN_NODE_ID,
  COLUMN_NODE_STATE,
  /* add more */
  N_NODE_COLUMNS
};

enum
{
  PROP_0,
  PROP_NODE,

  N_PROPS
};

struct _GtkInspectorCssNodeTreePrivate
{
  GtkWidget *node_tree;
  GtkTreeModel *node_model;
  GtkTreeViewColumn *node_name_column;
  GtkTreeViewColumn *node_id_column;
  GtkTreeViewColumn *node_classes_column;
  GListStore *prop_model;
  GtkWidget *prop_tree;
  GtkCssNode *node;
};

static GParamSpec *properties[N_PROPS] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorCssNodeTree, gtk_inspector_css_node_tree, GTK_TYPE_BOX)

typedef struct {
  GtkCssNode *node;
  const char *prop_name;
  GdkRectangle rect;
  GtkInspectorCssNodeTree *cnt;
} NodePropEditor;

static void
show_node_prop_editor (NodePropEditor *npe)
{
  GtkWidget *popover;
  GtkWidget *editor;

  popover = gtk_popover_new ();
  gtk_widget_set_parent (popover, GTK_WIDGET (npe->cnt));
  gtk_popover_set_pointing_to (GTK_POPOVER (popover), &npe->rect);

  editor = gtk_inspector_prop_editor_new (G_OBJECT (npe->node), npe->prop_name, NULL);

  gtk_popover_set_child (GTK_POPOVER (popover), editor);

  gtk_popover_popup (GTK_POPOVER (popover));

  g_signal_connect (popover, "unmap", G_CALLBACK (gtk_widget_unparent), NULL);
}

static void
row_activated (GtkTreeView             *tv,
               GtkTreePath             *path,
               GtkTreeViewColumn       *col,
               GtkInspectorCssNodeTree *cnt)
{
  GtkTreeIter iter;
  NodePropEditor npe;

  npe.cnt = cnt;

  if (col == cnt->priv->node_name_column)
    npe.prop_name = "name";
  else if (col == cnt->priv->node_id_column)
    npe.prop_name = "id";
  else if (col == cnt->priv->node_classes_column)
    npe.prop_name = "classes";
  else
    return;

  gtk_tree_model_get_iter (cnt->priv->node_model, &iter, path);
  npe.node = gtk_tree_model_css_node_get_node_from_iter (GTK_TREE_MODEL_CSS_NODE (cnt->priv->node_model), &iter);
  gtk_tree_view_get_cell_area (tv, path, col, &npe.rect);
  gtk_tree_view_convert_bin_window_to_widget_coords (tv, npe.rect.x, npe.rect.y, &npe.rect.x, &npe.rect.y);

  show_node_prop_editor (&npe);
}

static void
gtk_inspector_css_node_tree_set_node (GtkInspectorCssNodeTree *cnt,
                                      GtkCssNode              *node);

static void
selection_changed (GtkTreeSelection *selection, GtkInspectorCssNodeTree *cnt)
{
  GtkTreeIter iter;
  GtkCssNode *node;

  if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
    return;

  node = gtk_tree_model_css_node_get_node_from_iter (GTK_TREE_MODEL_CSS_NODE (cnt->priv->node_model), &iter);
  gtk_inspector_css_node_tree_set_node (cnt, node);
}

static void
gtk_inspector_css_node_tree_unset_node (GtkInspectorCssNodeTree *cnt)
{
  GtkInspectorCssNodeTreePrivate *priv = cnt->priv;

  if (priv->node)
    {
      g_signal_handlers_disconnect_matched (priv->node,
                                            G_SIGNAL_MATCH_DATA,
                                            0, 0, NULL, NULL,
                                            cnt);
      g_object_unref (priv->node);
      priv->node = NULL;
    }
}

static void
gtk_inspector_css_node_tree_get_property (GObject    *object,
                                          guint       property_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GtkInspectorCssNodeTree *cnt = GTK_INSPECTOR_CSS_NODE_TREE (object);
  GtkInspectorCssNodeTreePrivate *priv = cnt->priv;

  switch (property_id)
    {
    case PROP_NODE:
      g_value_set_object (value, priv->node);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_inspector_css_node_tree_set_property (GObject      *object,
                                          guint         property_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_inspector_css_node_tree_finalize (GObject *object)
{
  GtkInspectorCssNodeTree *cnt = GTK_INSPECTOR_CSS_NODE_TREE (object);

  gtk_inspector_css_node_tree_unset_node (cnt);

  g_object_unref (cnt->priv->prop_model);

  G_OBJECT_CLASS (gtk_inspector_css_node_tree_parent_class)->finalize (object);
}

static void
gtk_inspector_css_node_tree_class_init (GtkInspectorCssNodeTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = gtk_inspector_css_node_tree_set_property;
  object_class->get_property = gtk_inspector_css_node_tree_get_property;
  object_class->finalize = gtk_inspector_css_node_tree_finalize;

  properties[PROP_NODE] =
    g_param_spec_object ("node", NULL, NULL,
                         GTK_TYPE_CSS_NODE,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/css-node-tree.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssNodeTree, node_tree);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssNodeTree, node_name_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssNodeTree, node_id_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssNodeTree, node_classes_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssNodeTree, prop_tree);

  gtk_widget_class_bind_template_callback (widget_class, row_activated);
  gtk_widget_class_bind_template_callback (widget_class, selection_changed);
}

static int
sort_strv (gconstpointer a,
           gconstpointer b,
           gpointer      data)
{
  char **ap = (char **) a;
  char **bp = (char **) b;

  return g_ascii_strcasecmp (*ap, *bp);
}

static void
strv_sort (char **strv)
{
  g_qsort_with_data (strv,
		     g_strv_length (strv),
                     sizeof (char *),
                     sort_strv,
                     NULL);
}

static char *
format_state_flags (GtkStateFlags state)
{
  if (state)
    {
      GString *str;
      int i;
      gboolean first = TRUE;

      str = g_string_new ("");

      for (i = 0; i < 31; i++)
        {
          if (state & (1 << i))
            {
              if (!first)
                g_string_append (str, " | ");
              first = FALSE;
              g_string_append (str, gtk_css_pseudoclass_name (1 << i));
            }
        }
      return g_string_free (str, FALSE);
    }

 return g_strdup ("");
}

static void
gtk_inspector_css_node_tree_get_node_value (GtkTreeModelCssNode *model,
                                            GtkCssNode          *node,
                                            int                  column,
                                            GValue              *value)
{
  char **strv;
  char *s;

  switch (column)
    {
    case COLUMN_NODE_NAME:
      g_value_set_string (value, g_quark_to_string (gtk_css_node_get_name (node)));
      break;

    case COLUMN_NODE_VISIBLE:
      g_value_set_boolean (value, gtk_css_node_get_visible (node));
      break;

    case COLUMN_NODE_CLASSES:
      strv = gtk_css_node_get_classes (node);
      strv_sort (strv);
      s = g_strjoinv (" ", strv);
      g_value_take_string (value, s);
      g_strfreev (strv);
      break;

    case COLUMN_NODE_ID:
      g_value_set_string (value, g_quark_to_string (gtk_css_node_get_id (node)));
      break;

    case COLUMN_NODE_STATE:
      g_value_take_string (value, format_state_flags (gtk_css_node_get_state (node)));
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

static void
setup_label (GtkSignalListItemFactory *factory,
             GtkListItem              *list_item)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.);
  gtk_list_item_set_child (list_item, label);
}

static void
setup_value (GtkSignalListItemFactory *factory,
             GtkListItem              *list_item)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_width_chars (GTK_LABEL (label), 20);
  gtk_list_item_set_child (list_item, label);
}

static void
bind_name (GtkSignalListItemFactory *factory,
           GtkListItem              *list_item)
{
  GtkWidget *label;
  CssProperty *property;

  property = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);
  gtk_label_set_text (GTK_LABEL (label), property->name);
}

static void
bind_value (GtkSignalListItemFactory *factory,
            GtkListItem              *list_item)
{
  GtkWidget *label;
  CssProperty *property;

  property = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);
  gtk_label_set_text (GTK_LABEL (label), property->value);
}

static void
bind_location (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item)
{
  GtkWidget *label;
  CssProperty *property;

  property = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);
  gtk_label_set_text (GTK_LABEL (label), property->location);
}

static void
gtk_inspector_css_node_tree_init (GtkInspectorCssNodeTree *cnt)
{
  GtkInspectorCssNodeTreePrivate *priv;
  int i;
  GtkListItemFactory *factory;
  GtkColumnViewColumn *column;
  GtkSorter *sorter;
  GtkSortListModel *sort_model;

  cnt->priv = gtk_inspector_css_node_tree_get_instance_private (cnt);
  gtk_widget_init_template (GTK_WIDGET (cnt));
  priv = cnt->priv;

  priv->node_model = gtk_tree_model_css_node_new (gtk_inspector_css_node_tree_get_node_value,
                                                  N_NODE_COLUMNS,
                                                  G_TYPE_STRING,
                                                  G_TYPE_BOOLEAN,
                                                  G_TYPE_STRING,
                                                  G_TYPE_STRING,
                                                  G_TYPE_STRING);
  gtk_tree_view_set_model (GTK_TREE_VIEW (priv->node_tree), priv->node_model);
  g_object_unref (priv->node_model);

  priv->prop_model = g_list_store_new (css_property_get_type ());

  sort_model = gtk_sort_list_model_new (G_LIST_MODEL (priv->prop_model),
                                        gtk_column_view_get_sorter (GTK_COLUMN_VIEW (priv->prop_tree)));

  gtk_column_view_set_model (GTK_COLUMN_VIEW (priv->prop_tree),
                             GTK_SELECTION_MODEL (gtk_no_selection_new (G_LIST_MODEL (sort_model))));

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (priv->prop_tree)), 0);
  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_name), NULL);
  gtk_column_view_column_set_factory (column, factory);
  sorter = GTK_SORTER (gtk_string_sorter_new (gtk_property_expression_new (css_property_get_type (), NULL, "name")));
  gtk_column_view_column_set_sorter (column, sorter);
  gtk_column_view_sort_by_column (GTK_COLUMN_VIEW (priv->prop_tree), column, GTK_SORT_ASCENDING);
  g_object_unref (sorter);
  g_object_unref (factory);
  g_object_unref (column);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (priv->prop_tree)), 1);
  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_value), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_value), NULL);
  gtk_column_view_column_set_factory (column, factory);
  g_object_unref (factory);
  g_object_unref (column);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (priv->prop_tree)), 2);
  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_location), NULL);
  gtk_column_view_column_set_factory (column, factory);
  g_object_unref (factory);
  g_object_unref (column);

  for (i = 0; i < _gtk_css_style_property_get_n_properties (); i++)
    {
      GtkCssStyleProperty *prop;
      const char *name;

      prop = _gtk_css_style_property_lookup_by_id (i);
      name = _gtk_style_property_get_name (GTK_STYLE_PROPERTY (prop));

      g_list_store_append (priv->prop_model, css_property_new (name, NULL, NULL));
    }
}

void
gtk_inspector_css_node_tree_set_object (GtkInspectorCssNodeTree *cnt,
                                        GObject                 *object)
{
  GtkWidget *stack;
  GtkStackPage *page;
  GtkInspectorCssNodeTreePrivate *priv;
  GtkCssNode *node, *root;
  GtkTreePath *path;
  GtkTreeIter iter;

  g_return_if_fail (GTK_INSPECTOR_IS_CSS_NODE_TREE (cnt));

  priv = cnt->priv;

  stack = gtk_widget_get_parent (GTK_WIDGET (cnt));
  page = gtk_stack_get_page (GTK_STACK (stack), GTK_WIDGET (cnt));

  if (!GTK_IS_WIDGET (object))
    {
      g_object_set (page, "visible", FALSE, NULL);
      return;
    }

  g_object_set (page, "visible", TRUE, NULL);

  root = node = gtk_widget_get_css_node (GTK_WIDGET (object));
  while (gtk_css_node_get_parent (root))
    root = gtk_css_node_get_parent (root);

  gtk_tree_model_css_node_set_root_node (GTK_TREE_MODEL_CSS_NODE (priv->node_model), root);

  gtk_tree_model_css_node_get_iter_from_node (GTK_TREE_MODEL_CSS_NODE (priv->node_model), &iter, node);
  path = gtk_tree_model_get_path (priv->node_model, &iter);

  gtk_tree_view_expand_to_path (GTK_TREE_VIEW (priv->node_tree), path);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->node_tree), path, NULL, FALSE);
  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (priv->node_tree), path, NULL, TRUE, 0.5, 0.0);

  gtk_tree_path_free (path);
}

static void
gtk_inspector_css_node_tree_update_style (GtkInspectorCssNodeTree *cnt,
                                          GtkCssStyle             *new_style)
{
  GtkInspectorCssNodeTreePrivate *priv = cnt->priv;
  int i;

  for (i = 0; i < _gtk_css_style_property_get_n_properties (); i++)
    {
      GtkCssStyleProperty *prop;
      const char *name;
      GtkCssSection *section;
      char *location;
      char *value;
      CssProperty *property;

      prop = _gtk_css_style_property_lookup_by_id (i);
      name = _gtk_style_property_get_name (GTK_STYLE_PROPERTY (prop));

      if (new_style)
        {
          value = _gtk_css_value_to_string (gtk_css_style_get_value (new_style, i));

          section = gtk_css_style_get_section (new_style, i);
          if (section)
            location = gtk_css_section_to_string (section);
          else
            location = NULL;
        }
      else
        {
          value = NULL;
          location = NULL;
        }

      property = css_property_new (name, value, location);
      g_list_store_splice (priv->prop_model, i, 1, (gpointer *)&property, 1);

      g_free (location);
      g_free (value);
    }
}

static void
gtk_inspector_css_node_tree_update_style_cb (GtkCssNode              *node,
                                             GtkCssStyleChange       *change,
                                             GtkInspectorCssNodeTree *cnt)
{
  gtk_inspector_css_node_tree_update_style (cnt, gtk_css_style_change_get_new_style (change));
}

static void
gtk_inspector_css_node_tree_set_node (GtkInspectorCssNodeTree *cnt,
                                      GtkCssNode              *node)
{
  GtkInspectorCssNodeTreePrivate *priv = cnt->priv;

  if (priv->node == node)
    return;

  if (node)
    g_object_ref (node);

  gtk_inspector_css_node_tree_update_style (cnt, node ? gtk_css_node_get_style (node) : NULL);

  gtk_inspector_css_node_tree_unset_node (cnt);

  priv->node = node;
  if (node)
    {
      g_signal_connect (node, "style-changed", G_CALLBACK (gtk_inspector_css_node_tree_update_style_cb), cnt);
    }

  g_object_notify_by_pspec (G_OBJECT (cnt), properties[PROP_NODE]);
}

GtkCssNode *
gtk_inspector_css_node_tree_get_node (GtkInspectorCssNodeTree *cnt)
{
  GtkInspectorCssNodeTreePrivate *priv = cnt->priv;

  return priv->node;
}

void
gtk_inspector_css_node_tree_set_display (GtkInspectorCssNodeTree *cnt,
                                         GdkDisplay *display)
{
  GtkSettings *settings;
  char *theme_name;

  settings = gtk_settings_get_for_display (display);
  g_object_get (settings, "gtk-theme-name", &theme_name, NULL);
  g_object_set (settings, "gtk-theme-name", theme_name, NULL);
  g_free (theme_name);
}

/* vim:set foldmethod=marker expandtab: */
