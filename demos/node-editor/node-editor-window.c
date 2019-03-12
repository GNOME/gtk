/*
 * Copyright © 2019 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "node-editor-window.h"

#include "gsk/gskrendernodeparserprivate.h"

struct _NodeEditorWindow
{
  GtkApplicationWindow parent;

  guint text_timeout;

  GtkWidget *text_view;
  GtkTextBuffer *text_buffer;
};

struct _NodeEditorWindowClass
{
  GtkApplicationWindowClass parent_class;
};

G_DEFINE_TYPE(NodeEditorWindow, node_editor_window, GTK_TYPE_APPLICATION_WINDOW);

static gchar *
get_current_text (GtkTextBuffer *buffer)
{
  GtkTextIter start, end;

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);
  gtk_text_buffer_remove_all_tags (buffer, &start, &end);

  return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

static void
update_node (NodeEditorWindow *self)
{
  GskRenderNode *node;
  GError *error = NULL;
  char *text;
  GBytes *bytes;

  text = get_current_text (self->text_buffer);
  bytes = g_bytes_new_take (text, strlen (text));

  node = gsk_render_node_deserialize (bytes, &error);
  g_bytes_unref (bytes);
  if (node)
    gsk_render_node_unref (node);
  else
    g_clear_error (&error);
}

static gboolean
update_timeout (gpointer data)
{
  NodeEditorWindow *self = data;

  self->text_timeout = 0;

  update_node (self);

  return G_SOURCE_REMOVE;
}

static void
text_changed (GtkTextBuffer    *buffer,
              NodeEditorWindow *self)
{
  if (self->text_timeout != 0)
    g_source_remove (self->text_timeout);

  self->text_timeout = g_timeout_add (100, update_timeout, self); 
}

static gboolean
query_tooltip_cb (GtkWidget        *widget,
                  gint              x,
                  gint              y,
                  gboolean          keyboard_tip,
                  GtkTooltip       *tooltip,
                  NodeEditorWindow *self)
{
  GtkTextIter iter;
  //GList *l;

  if (keyboard_tip)
    {
      gint offset;

      g_object_get (self->text_buffer, "cursor-position", &offset, NULL);
      gtk_text_buffer_get_iter_at_offset (self->text_buffer, &iter, offset);
    }
  else
    {
      gint bx, by, trailing;

      gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (self->text_view), GTK_TEXT_WINDOW_TEXT,
                                             x, y, &bx, &by);
      gtk_text_view_get_iter_at_position (GTK_TEXT_VIEW (self->text_view), &iter, &trailing, bx, by);
    }

#if 0
  for (l = ce->priv->errors; l; l = l->next)
    {
      CssError *css_error = l->data;

      if (gtk_text_iter_in_range (&iter, &css_error->start, &css_error->end))
        {
          gtk_tooltip_set_text (tooltip, css_error->error->message);
          return TRUE;
        }
    }
#endif

  return FALSE;
}

static void
node_editor_window_finalize (GObject *object)
{
  NodeEditorWindow *self = NODE_EDITOR_WINDOW (object);

  if (self->text_timeout != 0)
    g_source_remove (self->text_timeout);

  G_OBJECT_CLASS (node_editor_window_parent_class)->finalize (object);
}

static void
node_editor_window_class_init (NodeEditorWindowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->finalize = node_editor_window_finalize;

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/gtk4/node-editor/node-editor-window.ui");

  gtk_widget_class_bind_template_child (widget_class, NodeEditorWindow, text_buffer);
  gtk_widget_class_bind_template_child (widget_class, NodeEditorWindow, text_view);

  gtk_widget_class_bind_template_callback (widget_class, text_changed);
  gtk_widget_class_bind_template_callback (widget_class, query_tooltip_cb);
}

static void
node_editor_window_init (NodeEditorWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

NodeEditorWindow *
node_editor_window_new (NodeEditorApplication *application)
{
  return g_object_new (NODE_EDITOR_WINDOW_TYPE,
                       "application", application,
                       NULL);
}
