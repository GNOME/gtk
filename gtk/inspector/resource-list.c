/*
 * Copyright (c) 2014 Red Hat, Inc.
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

#include "resource-list.h"

#include "gtkbutton.h"
#include "gtklabel.h"
#include "gtksearchbar.h"
#include "gtksearchentry.h"
#include "gtkstack.h"
#include "gtktextbuffer.h"
#include "gtktreeselection.h"
#include "gtktreestore.h"
#include "gtkeventcontrollerkey.h"
#include "gtkpicture.h"
#include "gtkmediafile.h"
#include "resource-holder.h"

#include <glib/gi18n-lib.h>

enum
{
  PROP_0,
  PROP_BUTTONS
};

struct _GtkInspectorResourceListPrivate
{
  GtkTextBuffer *buffer;
  GtkWidget *video;
  GtkWidget *image;
  GtkWidget *content;
  GtkWidget *name_label;
  GtkWidget *type;
  GtkWidget *type_label;
  GtkWidget *size_label;
  GtkWidget *info_grid;
  GtkWidget *stack;
  GtkWidget *buttons;
  GtkWidget *open_details_button;
  GtkWidget *close_details_button;
  GtkWidget *search_bar;
  GtkWidget *search_entry;

  GtkWidget *list;
  GtkColumnViewColumn *path;
  GtkColumnViewColumn *count;
  GtkColumnViewColumn *size;

  GtkTreeListModel *tree_model;
  GtkSingleSelection *selection;
};


G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorResourceList, gtk_inspector_resource_list, GTK_TYPE_BOX)

static GListModel *
load_resources_recurse (const char *path,
                        int        *count_out,
                        gsize      *size_out)
{
  gchar **names;
  gint i;
  GListStore *result;

  result = g_list_store_new (RESOURCE_TYPE_HOLDER);

  names = g_resources_enumerate_children (path, 0, NULL);
  for (i = 0; names[i]; i++)
    {
      gint len;
      gchar *p;
      gboolean has_slash;
      gint count;
      gsize size;
      GListModel *children;
      ResourceHolder *holder;

      p = g_strconcat (path, names[i], NULL);

      len = strlen (names[i]);
      has_slash = names[i][len - 1] == '/';

      if (has_slash)
        names[i][len - 1] = '\0';

      count = 0;
      size = 0;

      if (has_slash)
        {
          children = load_resources_recurse (p, &count, &size);

          *count_out += count;
          *size_out += size;
        }
      else
        {
          count = 0;
          if (g_resources_get_info (p, 0, &size, NULL, NULL))
            {
              *count_out += 1;
              *size_out += size;
            }
          children = NULL;
        }

      holder = resource_holder_new (names[i], p, count, size, children);
      g_clear_object (&children);
      g_list_store_append (result, holder);
      g_object_unref (holder);

      g_free (p);
    }

  g_strfreev (names);

  return G_LIST_MODEL (result);
}

static gboolean
populate_details (GtkInspectorResourceList *rl,
                  ResourceHolder           *holder)
{
  const char *path;
  const char *name;
  GBytes *bytes;
  gchar *type;
  gconstpointer data;
  gsize size;
  GError *error = NULL;
  gchar *markup;

  path = resource_holder_get_path (holder);
  name = resource_holder_get_name (holder);
  size = resource_holder_get_size (holder);

   if (g_str_has_suffix (path, "/"))
     return FALSE;

  markup = g_strconcat ("<span face='Monospace' size='small'>", path, "</span>", NULL);
  gtk_label_set_markup (GTK_LABEL (rl->priv->name_label), markup);
  g_free (markup);

  bytes = g_resources_lookup_data (path, 0, &error);
  if (bytes == NULL)
    {
      gtk_text_buffer_set_text (rl->priv->buffer, error->message, -1);
      g_error_free (error);
      gtk_stack_set_visible_child_name (GTK_STACK (rl->priv->content), "text");
    }
  else
    {
      gchar *text;
      gchar *content_image;
      gchar *content_text;
      gchar *content_video;

      content_image = g_content_type_from_mime_type ("image/*");
      content_text = g_content_type_from_mime_type ("text/*");
      content_video = g_content_type_from_mime_type ("video/*");

      data = g_bytes_get_data (bytes, &size);
      type = g_content_type_guess (name, data, size, NULL);

      text = g_content_type_get_description (type);
      gtk_label_set_text (GTK_LABEL (rl->priv->type_label), text);
      g_free (text);

      text = g_format_size (size);
      gtk_label_set_text (GTK_LABEL (rl->priv->size_label), text);
      g_free (text);

      if (g_content_type_is_a (type, content_text))
        {
          gtk_text_buffer_set_text (rl->priv->buffer, data, -1);
          gtk_stack_set_visible_child_name (GTK_STACK (rl->priv->content), "text");
        }
      else if (g_content_type_is_a (type, content_image))
        {
          gtk_picture_set_resource (GTK_PICTURE (rl->priv->image), path);
          gtk_stack_set_visible_child_name (GTK_STACK (rl->priv->content), "image");
        }
      else if (g_content_type_is_a (type, content_video))
        {
          GtkMediaStream *stream;

          stream = gtk_media_file_new_for_resource (path);
          gtk_media_stream_set_loop (GTK_MEDIA_STREAM (stream), TRUE);
          gtk_picture_set_paintable (GTK_PICTURE (rl->priv->image), GDK_PAINTABLE (stream));
          gtk_stack_set_visible_child_name (GTK_STACK (rl->priv->content), "image");
          gtk_media_stream_play (GTK_MEDIA_STREAM (stream));
          g_object_unref (stream);
        }
      else
        {
          gtk_text_buffer_set_text (rl->priv->buffer, "", 0);
          gtk_stack_set_visible_child_name (GTK_STACK (rl->priv->content), "text");
        }

      g_free (type);
      g_bytes_unref (bytes);

      g_free (content_image);
      g_free (content_text);
    }

  return TRUE;
}

static void
on_row_activated (GtkColumnView            *view,
                  guint                     position,
                  GtkInspectorResourceList *rl)
{
  gpointer item;
  ResourceHolder *holder;

  item = g_list_model_get_item (G_LIST_MODEL (rl->priv->selection), position);
  holder = gtk_tree_list_row_get_item (item);
  g_object_unref (item);
  if (populate_details (rl, holder))
    {
      gtk_stack_set_visible_child_name (GTK_STACK (rl->priv->stack), "details");
      gtk_stack_set_visible_child_name (GTK_STACK (rl->priv->buttons), "details");
    }
  g_object_unref (holder);
}

static gboolean
can_show_details (GtkInspectorResourceList *rl)
{
  gpointer item;
  ResourceHolder *holder;
  const char *path;

  item = gtk_single_selection_get_selected_item (rl->priv->selection);
  holder = gtk_tree_list_row_get_item (item);
  if (holder == NULL)
    return FALSE;
  path = resource_holder_get_path (holder);
  g_object_unref (holder);
  return !g_str_has_suffix (path, "/");
}

static void
on_selection_changed (GtkSelectionModel        *selection,
                      guint                     position,
                      guint                     n_items,
                      GtkInspectorResourceList *rl)
{
  gtk_widget_set_sensitive (rl->priv->open_details_button, can_show_details (rl));
}

static void
open_details (GtkWidget                *button,
              GtkInspectorResourceList *rl)
{
  gpointer item;
  ResourceHolder *holder;

  item = gtk_single_selection_get_selected_item (rl->priv->selection);
  holder = gtk_tree_list_row_get_item (item);
  if (holder == NULL)
    return;
  if (populate_details (rl, holder))
    {
      gtk_stack_set_visible_child_name (GTK_STACK (rl->priv->stack), "details");
      gtk_stack_set_visible_child_name (GTK_STACK (rl->priv->buttons), "details");
    }
  g_object_unref (holder);
}

static void
close_details (GtkWidget                *button,
               GtkInspectorResourceList *rl)
{
  gtk_stack_set_visible_child_name (GTK_STACK (rl->priv->stack), "list");
  gtk_stack_set_visible_child_name (GTK_STACK (rl->priv->buttons), "list");
}

static GListModel *
load_resources (void)
{
  gint count = 0;
  gsize size = 0;

  return load_resources_recurse ("/", &count, &size);
}

static void
on_map (GtkWidget *widget)
{
  GtkInspectorResourceList *rl = GTK_INSPECTOR_RESOURCE_LIST (widget);

  gtk_stack_set_visible_child_name (GTK_STACK (rl->priv->stack), "list");
  gtk_widget_set_sensitive (rl->priv->open_details_button, can_show_details (rl));
}

static gboolean search (GtkInspectorResourceList *rl,
                        gboolean                  forward,
                        gboolean                  force_progress);

static gboolean
key_pressed (GtkEventController       *controller,
             guint                     keyval,
             guint                     keycode,
             GdkModifierType           state,
             GtkInspectorResourceList *rl)
{
  if (gtk_widget_get_mapped (GTK_WIDGET (rl)))
    {
      GdkModifierType default_accel;
      gboolean search_started;

      search_started = gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (rl->priv->search_bar));
      default_accel = GDK_CONTROL_MASK;

      if (search_started &&
          (keyval == GDK_KEY_Return ||
           keyval == GDK_KEY_ISO_Enter ||
           keyval == GDK_KEY_KP_Enter))
        {
          gtk_widget_activate (GTK_WIDGET (rl->priv->list));
          return GDK_EVENT_PROPAGATE;
        }
      else if (search_started &&
               (keyval == GDK_KEY_Escape))
        {
          gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (rl->priv->search_bar), FALSE);
          return GDK_EVENT_STOP;
        }
      else if (search_started &&
               ((state & (default_accel | GDK_SHIFT_MASK)) == (default_accel | GDK_SHIFT_MASK)) &&
               (keyval == GDK_KEY_g || keyval == GDK_KEY_G))
        {
          if (!search (rl, FALSE, TRUE))
            gtk_widget_error_bell (GTK_WIDGET (rl));
          return GDK_EVENT_STOP;
        }
      else if (search_started &&
               ((state & (default_accel | GDK_SHIFT_MASK)) == default_accel) &&
               (keyval == GDK_KEY_g || keyval == GDK_KEY_G))
        {
          if (!search (rl, TRUE, TRUE))
            gtk_widget_error_bell (GTK_WIDGET (rl));
          return GDK_EVENT_STOP;
        }
    }

  return GDK_EVENT_PROPAGATE;
}

static void
destroy_controller (GtkEventController *controller)
{
  gtk_widget_remove_controller (gtk_event_controller_get_widget (controller), controller);
}

static void
root (GtkWidget *widget)
{
  GtkInspectorResourceList *rl = GTK_INSPECTOR_RESOURCE_LIST (widget);
  GtkEventController *controller;
  GtkWidget *toplevel;

  GTK_WIDGET_CLASS (gtk_inspector_resource_list_parent_class)->root (widget);

  toplevel = GTK_WIDGET (gtk_widget_get_root (widget));

  controller = gtk_event_controller_key_new ();
  g_object_set_data_full (G_OBJECT (toplevel), "resource-controller", controller, (GDestroyNotify)destroy_controller);
  g_signal_connect (controller, "key-pressed", G_CALLBACK (key_pressed), widget);
  gtk_widget_add_controller (toplevel, controller);

  gtk_search_bar_set_key_capture_widget (GTK_SEARCH_BAR (rl->priv->search_bar), toplevel);
}

static void
unroot (GtkWidget *widget)
{
  GtkWidget *toplevel;

  toplevel = GTK_WIDGET (gtk_widget_get_root (widget));
  g_object_set_data (G_OBJECT (toplevel), "resource-controller", NULL);

  GTK_WIDGET_CLASS (gtk_inspector_resource_list_parent_class)->unroot (widget);
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
match_object (GObject    *object,
              const char *text)
{
  const char *name = resource_holder_get_name (RESOURCE_HOLDER (object));

  if (match_string (name, text))
    return TRUE;

  return FALSE;
}

static GObject *
search_children (GObject    *object,
                 const char *text,
                 gboolean    forward)
{
  GListModel *children;
  GObject *child, *result;
  guint i, n;

  children = resource_holder_get_children (RESOURCE_HOLDER (object));
  if (children == NULL)
    return NULL;

  n = g_list_model_get_n_items (children);
  for (i = 0; i < n; i++)
    {
      child = g_list_model_get_item (children, forward ? i : n - i - 1);
      if (match_object (child, text))
        return child;

      result = search_children (child, text, forward);
      g_object_unref (child);
      if (result)
        return result;
    }

  return NULL;
}

static guint
model_get_item_index (GListModel *model,
                      gpointer    item)
{
  gpointer cmp;
  guint i;

  for (i = 0; (cmp = g_list_model_get_item (model, i)); i++)
    {
      if (cmp == item)
        {
          g_object_unref (cmp);
          return i;
        }
      g_object_unref (cmp);
    }

  return G_MAXUINT;
}

static GtkTreeListRow *
find_and_expand_object (GtkTreeListModel *model,
                        GObject          *object)
{
  GtkTreeListRow *result;
  GObject *parent;
  guint pos;

  parent = G_OBJECT (resource_holder_get_parent (RESOURCE_HOLDER (object)));
  if (parent)
    {
      GtkTreeListRow *parent_row = find_and_expand_object (model, parent);
      if (parent_row == NULL)
        return NULL;

      gtk_tree_list_row_set_expanded (parent_row, TRUE);
      pos = model_get_item_index (gtk_tree_list_row_get_children (parent_row), object);
      result = gtk_tree_list_row_get_child_row (parent_row, pos);
      g_object_unref (parent_row);
    }
  else
    {
      pos = model_get_item_index (gtk_tree_list_model_get_model (model), object);
      result = gtk_tree_list_model_get_child_row (model, pos);
    }

  return result;
}

static void
select_object (GtkInspectorResourceList *rl,
               GObject *object)
{
  GtkTreeListRow *row_item;

  row_item = find_and_expand_object (rl->priv->tree_model, object);
  if (row_item == NULL)
    return;

  gtk_single_selection_set_selected (rl->priv->selection,
                                     gtk_tree_list_row_get_position (row_item));
}

static gboolean
search (GtkInspectorResourceList *rl,
        gboolean                  forward,
        gboolean                  force_progress)
{
  GListModel *model = G_LIST_MODEL (rl->priv->tree_model);
  GtkTreeListRow *row_item;
  GObject *child, *result;
  guint i, selected, n, row;
  const char *text;

  text = gtk_editable_get_text (GTK_EDITABLE (rl->priv->search_entry));
  selected = gtk_single_selection_get_selected (rl->priv->selection);
  n = g_list_model_get_n_items (model);
  if (selected >= n)
    selected = 0;

  for (i = 0; i < n; i++)
    {
      row = (selected + (forward ? i : n - i - 1)) % n;
      row_item = g_list_model_get_item (model, row);
      child = gtk_tree_list_row_get_item (row_item);
      if (i > 0 || !force_progress)
        {
          if (match_object (child, text))
            {
              gtk_single_selection_set_selected (rl->priv->selection, row);
              g_object_unref (child);
              g_object_unref (row_item);
              return TRUE;
            }
        }

      if (!gtk_tree_list_row_get_expanded (row_item))
        {
          result = search_children (child, text, forward);
          if (result)
            {
              select_object (rl, result);
              g_object_unref (result);
              g_object_unref (child);
              g_object_unref (row_item);
              return TRUE;
            }
        }
      g_object_unref (child);
      g_object_unref (row_item);
    }

  return FALSE;
}

static void
on_search_changed (GtkSearchEntry           *entry,
                   GtkInspectorResourceList *rl)
{
  if (!search (rl, TRUE, FALSE))
    gtk_widget_error_bell (GTK_WIDGET (rl));
}

static void
next_match (GtkButton                *button,
            GtkInspectorResourceList *rl)
{
  if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (rl->priv->search_bar)))
    {
      if (!search (rl, TRUE, TRUE))
        gtk_widget_error_bell (GTK_WIDGET (rl));
    }
}

static void
previous_match (GtkButton                *button,
                GtkInspectorResourceList *rl)
{
  if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (rl->priv->search_bar)))
    {
      if (!search (rl, FALSE, TRUE))
        gtk_widget_error_bell (GTK_WIDGET (rl));
    }
}

static void
stop_search (GtkWidget                *entry,
             GtkInspectorResourceList *rl)
{
  gtk_editable_set_text (GTK_EDITABLE (rl->priv->search_entry), "");
  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (rl->priv->search_bar), FALSE);
}

static char *
holder_name (gpointer item)
{
  return g_strdup (resource_holder_get_name (RESOURCE_HOLDER (item)));
}

static int
holder_count (gpointer item)
{
  return resource_holder_get_count (RESOURCE_HOLDER (item));
}

static gsize
holder_size (gpointer item)
{
  return resource_holder_get_size (RESOURCE_HOLDER (item));
}

static void
gtk_inspector_resource_list_init (GtkInspectorResourceList *rl)
{
  GtkSorter *sorter;

  rl->priv = gtk_inspector_resource_list_get_instance_private (rl);

  gtk_widget_init_template (GTK_WIDGET (rl));

  g_signal_connect (rl, "map", G_CALLBACK (on_map), NULL);

  gtk_search_bar_connect_entry (GTK_SEARCH_BAR (rl->priv->search_bar),
                                GTK_EDITABLE (rl->priv->search_entry));

  sorter = gtk_string_sorter_new (gtk_cclosure_expression_new (G_TYPE_STRING, NULL,
                                                               0, NULL,
                                                               (GCallback)holder_name,
                                                               NULL, NULL));

  gtk_column_view_column_set_sorter (rl->priv->path, sorter);
  g_object_unref (sorter);

  sorter = gtk_numeric_sorter_new (gtk_cclosure_expression_new (G_TYPE_INT, NULL,
                                                                0, NULL,
                                                                (GCallback)holder_count,
                                                                NULL, NULL));

  gtk_column_view_column_set_sorter (rl->priv->count, sorter);
  g_object_unref (sorter);

  sorter = gtk_numeric_sorter_new (gtk_cclosure_expression_new (G_TYPE_UINT64, NULL,
                                                                0, NULL,
                                                                (GCallback)holder_size,
                                                                NULL, NULL));

  gtk_column_view_column_set_sorter (rl->priv->size, sorter);
  g_object_unref (sorter);
}

static GListModel *
create_model_for_object (gpointer item, gpointer data)
{
  GListModel *model = resource_holder_get_children (RESOURCE_HOLDER (item));

  if (model)
    return g_object_ref (model);

  return NULL;
}

static void
constructed (GObject *object)
{
  GtkInspectorResourceList *rl = GTK_INSPECTOR_RESOURCE_LIST (object);
  GListModel *root_model;
  GListModel *sort_model;
  GtkSorter *sorter;
 
  g_signal_connect (rl->priv->open_details_button, "clicked",
                    G_CALLBACK (open_details), rl);
  g_signal_connect (rl->priv->close_details_button, "clicked",
                    G_CALLBACK (close_details), rl);
 
  root_model = load_resources ();
  rl->priv->tree_model = gtk_tree_list_model_new (FALSE,
                                                  root_model,
                                                  FALSE,
                                                  create_model_for_object,
                                                  NULL,
                                                  NULL);

  sorter = gtk_tree_list_row_sorter_new (gtk_column_view_get_sorter (GTK_COLUMN_VIEW (rl->priv->list)));
  sort_model = G_LIST_MODEL (gtk_sort_list_model_new (G_LIST_MODEL (rl->priv->tree_model), sorter));
  rl->priv->selection = gtk_single_selection_new (sort_model);
  g_object_unref (root_model);
  g_object_unref (sort_model);
  g_object_unref (sorter);

  gtk_column_view_set_model (GTK_COLUMN_VIEW (rl->priv->list), G_LIST_MODEL (rl->priv->selection));

  g_signal_connect (rl->priv->selection, "selection-changed", G_CALLBACK (on_selection_changed), rl);
}

static void
get_property (GObject    *object,
              guint       param_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GtkInspectorResourceList *rl = GTK_INSPECTOR_RESOURCE_LIST (object);

  switch (param_id)
    {
    case PROP_BUTTONS:
      g_value_take_object (value, rl->priv->buttons);
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
  GtkInspectorResourceList *rl = GTK_INSPECTOR_RESOURCE_LIST (object);
  
  switch (param_id)
    {
    case PROP_BUTTONS:
      rl->priv->buttons = g_value_get_object (value);
      rl->priv->open_details_button = gtk_stack_get_child_by_name (GTK_STACK (rl->priv->buttons), "list");
      rl->priv->close_details_button = gtk_stack_get_child_by_name (GTK_STACK (rl->priv->buttons), "details");
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
finalize (GObject *object)
{
  GtkInspectorResourceList *rl = GTK_INSPECTOR_RESOURCE_LIST (object);

  g_object_unref (rl->priv->selection);
  g_object_unref (rl->priv->tree_model);

  G_OBJECT_CLASS (gtk_inspector_resource_list_parent_class)->finalize (object);
}

static void
setup_name_cb (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item)
{
  GtkWidget *expander;
  GtkWidget *label;

  expander = gtk_tree_expander_new ();
  gtk_list_item_set_child (list_item, expander);

  label = gtk_label_new (NULL);
  gtk_widget_set_margin_start (label, 5);
  gtk_widget_set_margin_end (label, 5);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_tree_expander_set_child (GTK_TREE_EXPANDER (expander), label);
}

static void
bind_name_cb (GtkSignalListItemFactory *factory,
              GtkListItem              *list_item)
{
  GtkTreeListRow *list_row;
  GtkWidget *expander;
  GtkWidget *label;
  gpointer item;

  list_row = gtk_list_item_get_item (list_item);
  expander = gtk_list_item_get_child (list_item);
  gtk_tree_expander_set_list_row (GTK_TREE_EXPANDER (expander), list_row);
  item = gtk_tree_list_row_get_item (list_row);
  label = gtk_tree_expander_get_child (GTK_TREE_EXPANDER (expander));

  gtk_label_set_label (GTK_LABEL (label), resource_holder_get_name (RESOURCE_HOLDER (item)));
  g_object_unref (item);
}

static void
setup_size_cb (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_widget_set_margin_start (label, 5);
  gtk_widget_set_margin_end (label, 5);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);
  gtk_list_item_set_child (list_item, label);
}

static void
bind_size_cb (GtkSignalListItemFactory *factory,
              GtkListItem              *list_item)
{
  GObject *item;
  GtkWidget *label;
  gsize size;
  char *text;

  item = gtk_tree_list_row_get_item (gtk_list_item_get_item (list_item));
  label = gtk_list_item_get_child (list_item);

  size = resource_holder_get_size (RESOURCE_HOLDER (item));
  text = g_format_size (size);
  gtk_label_set_label (GTK_LABEL (label), text);
  g_free (text);

  g_object_unref (item);
}

static void
setup_count_cb (GtkSignalListItemFactory *factory,
                GtkListItem              *list_item)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_widget_set_margin_start (label, 5);
  gtk_widget_set_margin_end (label, 5);
  gtk_label_set_xalign (GTK_LABEL (label), 1.0);
  gtk_list_item_set_child (list_item, label);
}

static void
bind_count_cb (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item)
{
  GObject *item;
  GtkWidget *label;
  int count;
  char *text;

  item = gtk_tree_list_row_get_item (gtk_list_item_get_item (list_item));
  label = gtk_list_item_get_child (list_item);

  count = resource_holder_get_count (RESOURCE_HOLDER (item));
  if (count > 0)
    {
      text = g_strdup_printf ("%d", count);
      gtk_label_set_label (GTK_LABEL (label), text);
      g_free (text);
    }
  else
    gtk_label_set_label (GTK_LABEL (label), "");
  g_object_unref (item);
}

static void
gtk_inspector_resource_list_class_init (GtkInspectorResourceListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = get_property;
  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->finalize = finalize;

  widget_class->root = root;
  widget_class->unroot = unroot;

  g_object_class_install_property (object_class, PROP_BUTTONS,
      g_param_spec_object ("buttons", NULL, NULL,
                           GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/resource-list.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, buffer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, content);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, image);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, name_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, type_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, type);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, size_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, info_grid);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, stack);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, search_bar);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, search_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, list);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, path);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, count);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, size);

  gtk_widget_class_bind_template_callback (widget_class, on_search_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, next_match);
  gtk_widget_class_bind_template_callback (widget_class, previous_match);
  gtk_widget_class_bind_template_callback (widget_class, stop_search);
  gtk_widget_class_bind_template_callback (widget_class, setup_name_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_name_cb);
  gtk_widget_class_bind_template_callback (widget_class, setup_count_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_count_cb);
  gtk_widget_class_bind_template_callback (widget_class, setup_size_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_size_cb);
}

// vim: set et sw=2 ts=2:
