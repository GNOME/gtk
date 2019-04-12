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

enum
{
  PROP_0,
  PROP_OBJECT_TREE,
  PROP_SEARCH_ENTRY
};

struct _GtkInspectorPropListPrivate
{
  GObject *object;
  gulong notify_handler_id;
  GtkInspectorObjectTree *object_tree;
  GtkWidget *search_entry;
  GtkWidget *search_stack;
  GtkWidget *list2;
  GtkSizeGroup *names;
  GtkSizeGroup *types;
  GtkSizeGroup *values;
  GtkSizeGroup *origins;
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

static gboolean
filter_func (GtkListBoxRow *row,
             gpointer       data)
{
  GtkInspectorPropList *pl = data;
  GParamSpec *pspec = (GParamSpec *)g_object_get_data (G_OBJECT (row), "pspec");
  const char *text = gtk_editable_get_text (GTK_EDITABLE (pl->priv->search_entry));
  
  return g_str_has_prefix (pspec->name, text);
}

static void
gtk_inspector_prop_list_init (GtkInspectorPropList *pl)
{
  pl->priv = gtk_inspector_prop_list_get_instance_private (pl);
  gtk_widget_init_template (GTK_WIDGET (pl));
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
  GtkWidget *popover;

  popover = gtk_widget_get_ancestor (GTK_WIDGET (editor), GTK_TYPE_POPOVER);
  gtk_widget_hide (popover);

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

  g_object_unref (pl->priv->names);
  g_object_unref (pl->priv->types);
  g_object_unref (pl->priv->values);
  g_object_unref (pl->priv->origins);

  G_OBJECT_CLASS (gtk_inspector_prop_list_parent_class)->finalize (object);
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
                            G_CALLBACK (gtk_list_box_invalidate_filter), pl->priv->list2);

  gtk_list_box_set_filter_func (GTK_LIST_BOX (pl->priv->list2), filter_func, pl, NULL);
}

static void
map (GtkWidget *widget)
{
  GtkInspectorPropList *pl = GTK_INSPECTOR_PROP_LIST (widget);
  GtkWidget *toplevel;

  GTK_WIDGET_CLASS (gtk_inspector_prop_list_parent_class)->map (widget);

  toplevel = gtk_widget_get_toplevel (widget);
  gtk_search_entry_set_key_capture_widget (GTK_SEARCH_ENTRY (pl->priv->search_entry), toplevel);
}

static void
unmap (GtkWidget *widget)
{
  GtkInspectorPropList *pl = GTK_INSPECTOR_PROP_LIST (widget);

  gtk_search_entry_set_key_capture_widget (GTK_SEARCH_ENTRY (pl->priv->search_entry), NULL);

  GTK_WIDGET_CLASS (gtk_inspector_prop_list_parent_class)->unmap (widget);
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

  g_object_class_install_property (object_class, PROP_OBJECT_TREE,
      g_param_spec_object ("object-tree", "Object Tree", "Object tree",
                           GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SEARCH_ENTRY,
      g_param_spec_object ("search-entry", "Search Entry", "Search Entry",
                           GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/prop-list.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, list2);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, names);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, types);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, values);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, origins);
}

/* Like g_strdup_value_contents, but keeps the type name separate */
static void
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

static GtkWidget *
gtk_inspector_prop_list_create_row (GtkInspectorPropList *pl,
                                    GParamSpec           *prop)
{
  GValue gvalue = {0};
  gchar *value;
  gchar *type;
  gchar *attribute = NULL;
  gboolean writable;
  GtkWidget *row;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *widget;

  g_value_init (&gvalue, prop->value_type);
  g_object_get_property (pl->priv->object, prop->name, &gvalue);

  strdup_value_contents (&gvalue, &value, &type);

  if (GTK_IS_CELL_RENDERER (pl->priv->object))
    {
      gpointer *layout;
      GtkCellArea *area;
      gint column = -1;

      area = NULL;
      layout = g_object_get_data (pl->priv->object, "gtk-inspector-cell-layout");
      if (layout)
        area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (layout));
      if (area)
        column = gtk_cell_area_attribute_get_column (area,
                                                     GTK_CELL_RENDERER (pl->priv->object),
                                                     prop->name);

       if (column != -1)
         attribute = g_strdup_printf ("%d", column);
    }

  writable = ((prop->flags & G_PARAM_WRITABLE) != 0) &&
             ((prop->flags & G_PARAM_CONSTRUCT_ONLY) == 0);

  row = gtk_list_box_row_new ();
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
  g_object_set_data (G_OBJECT (row), "pspec", prop);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (row), box);

  label = gtk_label_new (prop->name);
  gtk_widget_set_sensitive (label, writable);
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_size_group_add_widget (pl->priv->names, label);
  gtk_container_add (GTK_CONTAINER (box), label);

  label = gtk_label_new (type ? type : "");
  gtk_widget_set_sensitive (label, writable);
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_size_group_add_widget (pl->priv->types, label);
  gtk_container_add (GTK_CONTAINER (box), label);

  label = gtk_label_new (g_type_name (prop->owner_type));
  gtk_widget_set_sensitive (label, writable);
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_size_group_add_widget (pl->priv->origins, label);
  gtk_container_add (GTK_CONTAINER (box), label);

  widget = gtk_inspector_prop_editor_new (pl->priv->object, prop->name, pl->priv->values);
  gtk_container_add (GTK_CONTAINER (box), widget);
  g_signal_connect (widget, "show-object", G_CALLBACK (show_object), pl);

  g_free (value);
  g_free (type);
  g_free (attribute);
  g_value_unset (&gvalue);

  return row;
}

static void
gtk_inspector_prop_list_prop_changed_cb (GObject              *pspec,
                                         GParamSpec           *prop,
                                         GtkInspectorPropList *pl)
{
  if (!pl->priv->object)
    return;

#if 0
  iter = g_hash_table_lookup (pl->priv->prop_iters, prop->name);
  if (iter != NULL)
    gtk_inspector_prop_list_update_prop (pl, iter, prop);
#endif
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
  GtkWidget *w;

  if (!object)
    return FALSE;

  if (pl->priv->object == object)
    return TRUE;

  cleanup_object (pl);

  gtk_editable_set_text (GTK_EDITABLE (pl->priv->search_entry), "");
  gtk_stack_set_visible_child_name (GTK_STACK (pl->priv->search_stack), "title");

  props = g_object_class_list_properties (G_OBJECT_GET_CLASS (object), &num_properties);

  pl->priv->object = object;

  while ((w = gtk_widget_get_first_child (pl->priv->list2)) != NULL)
    gtk_widget_destroy (w);

  for (i = 0; i < num_properties; i++)
    {
      GParamSpec *prop = props[i];
      GtkWidget *row;

      if (! (prop->flags & G_PARAM_READABLE))
        continue;

      row = gtk_inspector_prop_list_create_row (pl, prop);
      gtk_container_add (GTK_CONTAINER (pl->priv->list2), row);      
    }

  g_free (props);

  if (GTK_IS_WIDGET (object))
    g_signal_connect_object (object, "destroy", G_CALLBACK (cleanup_object), pl, G_CONNECT_SWAPPED);

  /* Listen for updates */
  pl->priv->notify_handler_id =
      g_signal_connect_object (object, "notify",
                               G_CALLBACK (gtk_inspector_prop_list_prop_changed_cb), pl, 0);

  gtk_widget_show (GTK_WIDGET (pl));

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
