/*
 * Copyright (c) 2008-2009  Christian Hammond
 * Copyright (c) 2008-2009  David Trowbridge
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

#include "prop-list.h"

#include "prop-editor.h"
#include "object-tree.h"

#include "gtkcelllayout.h"
#include "gtktreeview.h"
#include "gtktreeselection.h"
#include "gtkpopover.h"
#include "gtksearchentry.h"
#include "gtklabel.h"
#include "gtkmain.h"
#include "gtkstack.h"
#include "gtkeventcontrollerkey.h"
#include "gtklayoutmanager.h"
#include "gtklistbox.h"
#include "gtksizegroup.h"
#include "gtkroot.h"
#include "gtkgestureclick.h"
#include "gtkstylecontext.h"
#include "prop-holder.h"

enum
{
  PROP_0,
  PROP_OBJECT_TREE,
  PROP_SEARCH_ENTRY
};

typedef enum {
  COLUMN_NAME,
  COLUMN_ORIGIN
} SortColumn;

struct _GtkInspectorPropListPrivate
{
  GObject *object;
  gulong notify_handler_id;
  GtkInspectorObjectTree *object_tree;
  GtkWidget *search_entry;
  GtkWidget *search_stack;
  GtkWidget *list2;
  GtkFilter *filter;
  GtkColumnViewColumn *name;
  GtkColumnViewColumn *type;
  GtkColumnViewColumn *origin;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorPropList, gtk_inspector_prop_list, GTK_TYPE_BOX)

static void
search_close_clicked (GtkWidget            *button,
                      GtkInspectorPropList *pl)
{
  gtk_editable_set_text (GTK_EDITABLE (pl->priv->search_entry), "");
  gtk_stack_set_visible_child_name (GTK_STACK (pl->priv->search_stack), "title");
}

static void
show_search_entry (GtkInspectorPropList *pl)
{
  gtk_stack_set_visible_child (GTK_STACK (pl->priv->search_stack),
                               pl->priv->search_entry);
}

static char *
holder_prop (gpointer item)
{
  GParamSpec *prop = prop_holder_get_pspec (PROP_HOLDER (item));

  return g_strdup (prop->name);
}

static char *
holder_type (gpointer item)
{
  GParamSpec *prop = prop_holder_get_pspec (PROP_HOLDER (item));

  return g_strdup (g_type_name (prop->value_type));
}

static char *
holder_origin (gpointer item)
{
  GParamSpec *prop = prop_holder_get_pspec (PROP_HOLDER (item));

  return g_strdup (g_type_name (prop->owner_type));
}

static void
gtk_inspector_prop_list_init (GtkInspectorPropList *pl)
{
  GtkExpression *expression;
  GtkSorter *sorter;

  pl->priv = gtk_inspector_prop_list_get_instance_private (pl);
  gtk_widget_init_template (GTK_WIDGET (pl));
  pl->priv->filter = gtk_string_filter_new ();
  gtk_string_filter_set_match_mode (GTK_STRING_FILTER (pl->priv->filter), GTK_STRING_FILTER_MATCH_PREFIX);

  expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL,
                                            0, NULL,
                                            (GCallback)holder_prop,
                                            NULL, NULL);
 
  gtk_string_filter_set_expression (GTK_STRING_FILTER (pl->priv->filter), expression);

  sorter = gtk_string_sorter_new ();
  gtk_string_sorter_set_expression (GTK_STRING_SORTER (sorter), expression);
  gtk_column_view_column_set_sorter (pl->priv->name, sorter);
  g_object_unref (sorter);

  gtk_expression_unref (expression);

  expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL,
                                            0, NULL,
                                            (GCallback)holder_type,
                                            NULL, NULL);

  sorter = gtk_string_sorter_new ();
  gtk_string_sorter_set_expression (GTK_STRING_SORTER (sorter), expression);
  gtk_column_view_column_set_sorter (pl->priv->type, sorter);
  g_object_unref (sorter);

  gtk_expression_unref (expression);

  expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL,
                                            0, NULL,
                                            (GCallback)holder_origin,
                                            NULL, NULL);

  sorter = gtk_string_sorter_new ();
  gtk_string_sorter_set_expression (GTK_STRING_SORTER (sorter), expression);
  gtk_column_view_column_set_sorter (pl->priv->origin, sorter);
  g_object_unref (sorter);

  gtk_expression_unref (expression);
}

static void
get_property (GObject    *object,
              guint       param_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GtkInspectorPropList *pl = GTK_INSPECTOR_PROP_LIST (object);

  switch (param_id)
    {
      case PROP_OBJECT_TREE:
        g_value_take_object (value, pl->priv->object_tree);
        break;

      case PROP_SEARCH_ENTRY:
        g_value_take_object (value, pl->priv->search_entry);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
set_property (GObject      *object,
              guint         param_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GtkInspectorPropList *pl = GTK_INSPECTOR_PROP_LIST (object);

  switch (param_id)
    {
      case PROP_OBJECT_TREE:
        pl->priv->object_tree = g_value_get_object (value);
        break;

      case PROP_SEARCH_ENTRY:
        pl->priv->search_entry = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
show_object (GtkInspectorPropEditor *editor,
             GObject                *object,
             const gchar            *name,
             const gchar            *tab,
             GtkInspectorPropList   *pl)
{
  g_object_set_data_full (G_OBJECT (pl->priv->object_tree), "next-tab", g_strdup (tab), g_free);
  gtk_inspector_object_tree_select_object (pl->priv->object_tree, object);
  gtk_inspector_object_tree_activate_object (pl->priv->object_tree, object);
}


static void cleanup_object (GtkInspectorPropList *pl);

static void
finalize (GObject *object)
{
  GtkInspectorPropList *pl = GTK_INSPECTOR_PROP_LIST (object);

  cleanup_object (pl);

  G_OBJECT_CLASS (gtk_inspector_prop_list_parent_class)->finalize (object);
}

static void
update_filter (GtkInspectorPropList *pl,
               GtkSearchEntry *entry)
{
  const char *text;

  text = gtk_editable_get_text (GTK_EDITABLE (entry));
  gtk_string_filter_set_search (GTK_STRING_FILTER (pl->priv->filter), text);
}

static void
constructed (GObject *object)
{
  GtkInspectorPropList *pl = GTK_INSPECTOR_PROP_LIST (object);

  pl->priv->search_stack = gtk_widget_get_parent (pl->priv->search_entry);

  g_signal_connect (pl->priv->search_entry, "stop-search",
                    G_CALLBACK (search_close_clicked), pl);

  g_signal_connect_swapped (pl->priv->search_entry, "search-started",
                            G_CALLBACK (show_search_entry), pl);
  g_signal_connect_swapped (pl->priv->search_entry, "search-changed",
                            G_CALLBACK (update_filter), pl);
}

static void
update_key_capture (GtkInspectorPropList *pl)
{
  GtkWidget *capture_widget;

  if (gtk_widget_get_mapped (GTK_WIDGET (pl)))
    {
      GtkWidget *toplevel;
      GtkWidget *focus;

      toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (pl)));
      focus = gtk_root_get_focus (GTK_ROOT (toplevel));

      if (GTK_IS_EDITABLE (focus) &&
          gtk_widget_is_ancestor (focus, pl->priv->list2))
        capture_widget = NULL;
      else
        capture_widget = toplevel;
    }
  else
    capture_widget = NULL;

  gtk_search_entry_set_key_capture_widget (GTK_SEARCH_ENTRY (pl->priv->search_entry),
                                           capture_widget);
}

static void
map (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_inspector_prop_list_parent_class)->map (widget);

  update_key_capture (GTK_INSPECTOR_PROP_LIST (widget));
}

static void
unmap (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_inspector_prop_list_parent_class)->unmap (widget);

  update_key_capture (GTK_INSPECTOR_PROP_LIST (widget));
}

static void
root (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_inspector_prop_list_parent_class)->root (widget);

  g_signal_connect_swapped (gtk_widget_get_root (widget), "notify::focus-widget",
                            G_CALLBACK (update_key_capture), widget);
}

static void
unroot (GtkWidget *widget)
{
  g_signal_handlers_disconnect_by_func (gtk_widget_get_root (widget),
                                        update_key_capture, widget);

  GTK_WIDGET_CLASS (gtk_inspector_prop_list_parent_class)->unroot (widget);
}

static void
setup_name_cb (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_list_item_set_child (list_item, label);
}

static void
bind_name_cb (GtkSignalListItemFactory *factory,
              GtkListItem              *list_item)
{
  GObject *item;
  GtkWidget *label;

  item = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);

  gtk_label_set_label (GTK_LABEL (label), prop_holder_get_name (PROP_HOLDER (item)));
}

static void
setup_type_cb (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_list_item_set_child (list_item, label);
}

static void
bind_type_cb (GtkSignalListItemFactory *factory,
              GtkListItem              *list_item)
{
  GObject *item;
  GtkWidget *label;
  GParamSpec *prop;
  const char *type;

  item = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);

  prop = prop_holder_get_pspec (PROP_HOLDER (item));
  type = g_type_name (G_PARAM_SPEC_VALUE_TYPE (prop));

  gtk_label_set_label (GTK_LABEL (label), type);
}

static void
setup_origin_cb (GtkSignalListItemFactory *factory,
                 GtkListItem              *list_item)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_list_item_set_child (list_item, label);
}

static void
bind_origin_cb (GtkSignalListItemFactory *factory,
                GtkListItem              *list_item)
{
  GObject *item;
  GtkWidget *label;
  GParamSpec *prop;
  const char *origin;

  item = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);

  prop = prop_holder_get_pspec (PROP_HOLDER (item));
  origin = g_type_name (prop->owner_type);

  gtk_label_set_label (GTK_LABEL (label), origin);
}

static void
setup_value_cb (GtkSignalListItemFactory *factory,
                GtkListItem              *list_item)
{
}

static void
bind_value_cb (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item,
               gpointer                  data)
{
  GObject *item;
  GtkWidget *widget;
  GObject *object;
  const char *name;

  item = gtk_list_item_get_item (list_item);

  object = prop_holder_get_object (PROP_HOLDER (item));
  name = prop_holder_get_name (PROP_HOLDER (item));

  widget = gtk_inspector_prop_editor_new (object, name, NULL);
  g_signal_connect (widget, "show-object", G_CALLBACK (show_object), data);
  gtk_list_item_set_child (list_item, widget);
}


static void
gtk_inspector_prop_list_class_init (GtkInspectorPropListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = finalize;
  object_class->get_property = get_property;
  object_class->set_property = set_property;
  object_class->constructed = constructed;

  widget_class->map = map;
  widget_class->unmap = unmap;
  widget_class->root = root;
  widget_class->unroot = unroot;

  g_object_class_install_property (object_class, PROP_OBJECT_TREE,
      g_param_spec_object ("object-tree", "Object Tree", "Object tree",
                           GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SEARCH_ENTRY,
      g_param_spec_object ("search-entry", "Search Entry", "Search Entry",
                           GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/prop-list.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, list2);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, name);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, type);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, origin);
  gtk_widget_class_bind_template_callback (widget_class, setup_name_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_name_cb);
  gtk_widget_class_bind_template_callback (widget_class, setup_type_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_type_cb);
  gtk_widget_class_bind_template_callback (widget_class, setup_origin_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_origin_cb);
  gtk_widget_class_bind_template_callback (widget_class, setup_value_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_value_cb);
}

/* Like g_strdup_value_contents, but keeps the type name separate */
void
strdup_value_contents (const GValue  *value,
                       gchar        **contents,
                       gchar        **type)
{
  const gchar *src;

  if (G_VALUE_HOLDS_STRING (value))
    {
      src = g_value_get_string (value);

      *type = g_strdup ("char*");

      if (!src)
        {
          *contents = g_strdup ("NULL");
        }
      else
        {
          gchar *s = g_strescape (src, NULL);
          *contents = g_strdup_printf ("\"%s\"", s);
          g_free (s);
        }
    }
  else if (g_value_type_transformable (G_VALUE_TYPE (value), G_TYPE_STRING))
    {
      GValue tmp_value = G_VALUE_INIT;

      *type = g_strdup (g_type_name (G_VALUE_TYPE (value)));

      g_value_init (&tmp_value, G_TYPE_STRING);
      g_value_transform (value, &tmp_value);
      src = g_value_get_string (&tmp_value);
      if (!src)
        *contents = g_strdup ("NULL");
      else
        *contents = g_strescape (src, NULL);
      g_value_unset (&tmp_value);
    }
  else if (g_value_fits_pointer (value))
    {
      gpointer p = g_value_peek_pointer (value);

      if (!p)
        {
          *type = g_strdup (g_type_name (G_VALUE_TYPE (value)));
          *contents = g_strdup ("NULL");
        }
      else if (G_VALUE_HOLDS_OBJECT (value))
        {
          *type = g_strdup (G_OBJECT_TYPE_NAME (p));
          *contents = g_strdup_printf ("%p", p);
        }
      else if (G_VALUE_HOLDS_PARAM (value))
        {
          *type = g_strdup (G_PARAM_SPEC_TYPE_NAME (p));
          *contents = g_strdup_printf ("%p", p);
        }
      else if (G_VALUE_HOLDS (value, G_TYPE_STRV))
        {
          GStrv strv = g_value_get_boxed (value);
          GString *tmp = g_string_new ("[");

          while (*strv != NULL)
            {
              gchar *escaped = g_strescape (*strv, NULL);

              g_string_append_printf (tmp, "\"%s\"", escaped);
              g_free (escaped);

              if (*++strv != NULL)
                g_string_append (tmp, ", ");
            }

          g_string_append (tmp, "]");
          *type = g_strdup ("char**");
          *contents = g_string_free (tmp, FALSE);
        }
      else if (G_VALUE_HOLDS_BOXED (value))
        {
          *type = g_strdup (g_type_name (G_VALUE_TYPE (value)));
          *contents = g_strdup_printf ("%p", p);
        }
      else if (G_VALUE_HOLDS_POINTER (value))
        {
          *type = g_strdup ("gpointer");
          *contents = g_strdup_printf ("%p", p);
        }
      else
        {
          *type = g_strdup ("???");
          *contents = g_strdup ("???");
        }
    }
  else
    {
      *type = g_strdup ("???");
      *contents = g_strdup ("???");
    }
}

static void
cleanup_object (GtkInspectorPropList *pl)
{
  if (pl->priv->object &&
      g_signal_handler_is_connected (pl->priv->object, pl->priv->notify_handler_id))
    g_signal_handler_disconnect (pl->priv->object, pl->priv->notify_handler_id);

  pl->priv->object = NULL;
  pl->priv->notify_handler_id = 0;
}

gboolean
gtk_inspector_prop_list_set_object (GtkInspectorPropList *pl,
                                    GObject              *object)
{
  GParamSpec **props;
  guint num_properties;
  guint i;
  GListStore *store;
  GListModel *list;
  GListModel *filtered;
  GtkSortListModel *sorted;

  if (!object)
    return FALSE;

  if (pl->priv->object == object)
    return TRUE;

  cleanup_object (pl);

  gtk_editable_set_text (GTK_EDITABLE (pl->priv->search_entry), "");
  gtk_stack_set_visible_child_name (GTK_STACK (pl->priv->search_stack), "title");

  props = g_object_class_list_properties (G_OBJECT_GET_CLASS (object), &num_properties);

  pl->priv->object = object;

  store = g_list_store_new (PROP_TYPE_HOLDER);

  for (i = 0; i < num_properties; i++)
    {
      GParamSpec *prop = props[i];
      PropHolder *holder;

      if (! (prop->flags & G_PARAM_READABLE))
        continue;

      holder = prop_holder_new (object, prop);
      g_list_store_append (store, holder);
      g_object_unref (holder);
    }

  g_free (props);

  if (GTK_IS_WIDGET (object))
    g_signal_connect_object (object, "destroy", G_CALLBACK (cleanup_object), pl, G_CONNECT_SWAPPED);

  filtered = G_LIST_MODEL (gtk_filter_list_model_new (G_LIST_MODEL (store), pl->priv->filter));
  sorted = gtk_sort_list_model_new (filtered, NULL);
  list = G_LIST_MODEL (gtk_no_selection_new (G_LIST_MODEL (sorted)));

  gtk_column_view_set_model (GTK_COLUMN_VIEW (pl->priv->list2), list);
  gtk_column_view_set_sort_model (GTK_COLUMN_VIEW (pl->priv->list2), sorted);

  gtk_widget_show (GTK_WIDGET (pl));

  g_object_unref (list);
  g_object_unref (sorted);
  g_object_unref (filtered);
  g_object_unref (store);

  return TRUE;
}

void
gtk_inspector_prop_list_set_layout_child (GtkInspectorPropList *pl,
                                          GObject              *object)
{
  GtkWidget *stack;
  GtkStackPage *page;
  GtkWidget *parent;
  GtkLayoutManager *layout_manager;
  GtkLayoutChild *layout_child;

  stack = gtk_widget_get_parent (GTK_WIDGET (pl));
  page = gtk_stack_get_page (GTK_STACK (stack), GTK_WIDGET (pl));
  g_object_set (page, "visible", FALSE, NULL);

  if (!GTK_IS_WIDGET (object))
    return;

  parent = gtk_widget_get_parent (GTK_WIDGET (object));
  if (!parent)
    return;

  layout_manager = gtk_widget_get_layout_manager (parent);
  if (!layout_manager)
    return;

  if (GTK_LAYOUT_MANAGER_GET_CLASS (layout_manager)->layout_child_type == G_TYPE_INVALID)
    return;

  layout_child = gtk_layout_manager_get_layout_child (layout_manager, GTK_WIDGET (object));
  if (!layout_child)
    return;

  if (!gtk_inspector_prop_list_set_object (pl, G_OBJECT (layout_child)))
    return;
  
  g_object_set (page, "visible", TRUE, NULL);
}

// vim: set et sw=2 ts=2:
