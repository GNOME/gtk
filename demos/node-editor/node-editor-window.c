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

  GtkWidget *picture;
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
    {
      /* XXX: Is this code neceeary or can we have API to turn nodes into paintables? */
      GtkSnapshot *snapshot;
      GdkPaintable *paintable;

      snapshot = gtk_snapshot_new ();
      gtk_snapshot_append_node (snapshot, node);
      gsk_render_node_unref (node);
      paintable = gtk_snapshot_free_to_paintable (snapshot, NULL);
      gtk_picture_set_paintable (GTK_PICTURE (self->picture), paintable);
      g_clear_object (&paintable);
    }
  else
    {
      gtk_picture_set_paintable (GTK_PICTURE (self->picture), NULL);
      g_clear_error (&error);
    }
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

gboolean
node_editor_window_load (NodeEditorWindow *self,
                         GFile            *file)
{
  GtkTextIter start, end;
  GBytes *bytes;

  bytes = g_file_load_bytes (file, NULL, NULL, NULL);
  if (bytes == NULL)
    return FALSE;

  if (!g_utf8_validate (g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), NULL))
    {
      g_bytes_unref (bytes);
      return FALSE;
    }

  gtk_text_buffer_get_start_iter (self->text_buffer, &start);
  gtk_text_buffer_get_end_iter (self->text_buffer, &end);
  gtk_text_buffer_insert (self->text_buffer,
                          &end,
                          g_bytes_get_data (bytes, NULL),
                          g_bytes_get_size (bytes));

  return TRUE;
}

static void
open_response_cb (GtkWidget        *dialog,
                  gint              response,
                  NodeEditorWindow *self)
{
  gtk_widget_hide (dialog);

  if (response == GTK_RESPONSE_ACCEPT)
    {
      GFile *file;

      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
      node_editor_window_load (self, file);
      g_object_unref (file);
    }

  gtk_widget_destroy (dialog);
}

static void
open_cb (GtkWidget        *button,
         NodeEditorWindow *self)
{
  GtkWidget *dialog;

  dialog = gtk_file_chooser_dialog_new ("",
                                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (button))),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        "_Cancel", GTK_RESPONSE_CANCEL,
                                        "_Load", GTK_RESPONSE_ACCEPT,
                                        NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  g_signal_connect (dialog, "response", G_CALLBACK (open_response_cb), self);
  gtk_widget_show (dialog);
}

static void
save_response_cb (GtkWidget        *dialog,
                  gint              response,
                  NodeEditorWindow *self)
{
  gtk_widget_hide (dialog);

  if (response == GTK_RESPONSE_ACCEPT)
    {
      char *text, *filename;
      GError *error = NULL;

      text = get_current_text (self->text_buffer);

      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      if (!g_file_set_contents (filename, text, -1, &error))
        {
          GtkWidget *dialog;

          dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))),
                                           GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_INFO,
                                           GTK_BUTTONS_OK,
                                           "Saving failed");
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                    "%s", error->message);
          g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
          gtk_widget_show (dialog);
          g_error_free (error);
        }
      g_free (filename);
    }

  gtk_widget_destroy (dialog);
}

static void
save_cb (GtkWidget        *button,
         NodeEditorWindow *self)
{
  GtkWidget *dialog;

  dialog = gtk_file_chooser_dialog_new ("",
                                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (button))),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        "_Cancel", GTK_RESPONSE_CANCEL,
                                        "_Save", GTK_RESPONSE_ACCEPT,
                                        NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  g_signal_connect (dialog, "response", G_CALLBACK (save_response_cb), self);
  gtk_widget_show (dialog);
}

static GdkTexture *
create_texture (NodeEditorWindow *self)
{
  GdkPaintable *paintable;
  GtkSnapshot *snapshot;
  GskRenderer *renderer;
  GskRenderNode *node;
  GdkTexture *texture;

  paintable = gtk_picture_get_paintable (GTK_PICTURE (self->picture));
  if (paintable == NULL ||
      gdk_paintable_get_intrinsic_width (paintable) <= 0 ||
      gdk_paintable_get_intrinsic_height (paintable) <= 0)
    return NULL;
  snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (paintable, snapshot, gdk_paintable_get_intrinsic_width (paintable), gdk_paintable_get_intrinsic_height (paintable));
  node = gtk_snapshot_free_to_node (snapshot);
  if (node == NULL)
    return NULL;

  /* ahem */
  renderer = GTK_ROOT_GET_IFACE (gtk_widget_get_root (GTK_WIDGET (self)))->get_renderer (gtk_widget_get_root (GTK_WIDGET (self)));
  texture = gsk_renderer_render_texture (renderer, node, NULL);
  gsk_render_node_unref (node);

  return texture;
}

static void
export_image_response_cb (GtkWidget  *dialog,
                          gint        response,
                          GdkTexture *texture)
{
  gtk_widget_hide (dialog);

  if (response == GTK_RESPONSE_ACCEPT)
    {
      char *filename;

      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      if (!gdk_texture_save_to_png (texture, filename))
        {
          GtkWidget *message_dialog;

          message_dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_window_get_transient_for (GTK_WINDOW (dialog))),
                                                   GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_INFO,
                                                   GTK_BUTTONS_OK,
                                                   "Exporting to image failed");
          g_signal_connect (message_dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
          gtk_widget_show (message_dialog);
        }
      g_free (filename);
    }

  gtk_widget_destroy (dialog);
  g_object_unref (texture);
}

static void
export_image_cb (GtkWidget        *button,
                 NodeEditorWindow *self)
{
  GdkTexture *texture;
  GtkWidget *dialog;

  texture = create_texture (self);
  if (texture == NULL)
    return;

  dialog = gtk_file_chooser_dialog_new ("",
                                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (button))),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        "_Cancel", GTK_RESPONSE_CANCEL,
                                        "_Save", GTK_RESPONSE_ACCEPT,
                                        NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  g_signal_connect (dialog, "response", G_CALLBACK (export_image_response_cb), texture);
  gtk_widget_show (dialog);
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
  gtk_widget_class_bind_template_child (widget_class, NodeEditorWindow, picture);

  gtk_widget_class_bind_template_callback (widget_class, text_changed);
  gtk_widget_class_bind_template_callback (widget_class, query_tooltip_cb);
  gtk_widget_class_bind_template_callback (widget_class, open_cb);
  gtk_widget_class_bind_template_callback (widget_class, save_cb);
  gtk_widget_class_bind_template_callback (widget_class, export_image_cb);
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
