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

#include "gtkrendererpaintableprivate.h"

#include "gsk/gskrendernodeparserprivate.h"

typedef struct
{
  gsize  start_chars;
  gsize  end_chars;
  char  *message;
} TextViewError;

struct _NodeEditorWindow
{
  GtkApplicationWindow parent;

  GtkWidget *picture;
  GtkWidget *text_view;
  GtkTextBuffer *text_buffer;
  GtkTextTagTable *tag_table;

  GtkWidget *renderer_listbox;
  GListStore *renderers;
  GdkPaintable *paintable;

  GArray *errors;
};

struct _NodeEditorWindowClass
{
  GtkApplicationWindowClass parent_class;
};

G_DEFINE_TYPE(NodeEditorWindow, node_editor_window, GTK_TYPE_APPLICATION_WINDOW);

static void
text_view_error_free (TextViewError *e)
{
  g_free (e->message);
}

static gchar *
get_current_text (GtkTextBuffer *buffer)
{
  GtkTextIter start, end;

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);

  return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

static void
text_buffer_remove_all_tags (GtkTextBuffer *buffer)
{
  GtkTextIter start, end;

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);
  gtk_text_buffer_remove_all_tags (buffer, &start, &end);
}

static void
deserialize_error_func (const GtkCssSection *section,
                        const GError        *error,
                        gpointer             user_data)
{
  const GtkCssLocation *start_location = gtk_css_section_get_start_location (section);
  const GtkCssLocation *end_location = gtk_css_section_get_end_location (section);
  NodeEditorWindow *self = user_data;
  GtkTextIter start_iter, end_iter;
  TextViewError text_view_error;

  gtk_text_buffer_get_iter_at_line_offset (self->text_buffer, &start_iter,
                                           start_location->lines,
                                           start_location->line_chars);
  gtk_text_buffer_get_iter_at_line_offset (self->text_buffer, &end_iter,
                                           end_location->lines,
                                           end_location->line_chars);

  gtk_text_buffer_apply_tag_by_name (self->text_buffer, "error",
                                     &start_iter, &end_iter);

  text_view_error.start_chars = start_location->chars;
  text_view_error.end_chars = end_location->chars;
  text_view_error.message = g_strdup (error->message);
  g_array_append_val (self->errors, text_view_error);
}

static void
text_iter_skip_alpha_backward (GtkTextIter *iter)
{
  /* Just skip to the previous non-whitespace char */

  while (!gtk_text_iter_is_start (iter))
    {
      gunichar c = gtk_text_iter_get_char (iter);

      if (g_unichar_isspace (c))
        {
          gtk_text_iter_forward_char (iter);
          break;
        }

      gtk_text_iter_backward_char (iter);
    }
}

static void
text_iter_skip_whitespace_backward (GtkTextIter *iter)
{
  while (!gtk_text_iter_is_start (iter))
    {
      gunichar c = gtk_text_iter_get_char (iter);

      if (g_unichar_isalpha (c))
        {
          gtk_text_iter_forward_char (iter);
          break;
        }

      gtk_text_iter_backward_char (iter);
    }
}

static void
text_changed (GtkTextBuffer    *buffer,
              NodeEditorWindow *self)
{
  GskRenderNode *node;
  char *text;
  GBytes *bytes;

  g_array_remove_range (self->errors, 0, self->errors->len);
  text = get_current_text (self->text_buffer);
  text_buffer_remove_all_tags (self->text_buffer);
  bytes = g_bytes_new_take (text, strlen (text));

  /* If this is too slow, go fix the parser performance */
  node = gsk_render_node_deserialize (bytes, deserialize_error_func, self);
  g_bytes_unref (bytes);
  if (node)
    {
      /* XXX: Is this code necessary or can we have API to turn nodes into paintables? */
      GtkSnapshot *snapshot;
      GdkPaintable *paintable;
      graphene_rect_t bounds;
      guint i;

      snapshot = gtk_snapshot_new ();
      gsk_render_node_get_bounds (node, &bounds);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (- bounds.origin.x, - bounds.origin.y));
      gtk_snapshot_append_node (snapshot, node);
      gsk_render_node_unref (node);
      paintable = gtk_snapshot_free_to_paintable (snapshot, &bounds.size);
      gtk_picture_set_paintable (GTK_PICTURE (self->picture), paintable);
      for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->renderers)); i++)
        {
          gpointer item = g_list_model_get_item (G_LIST_MODEL (self->renderers), i);
          gtk_renderer_paintable_set_paintable (item, paintable);
          g_object_unref (item);
        }
      g_clear_object (&paintable);
    }
  else
    {
      gtk_picture_set_paintable (GTK_PICTURE (self->picture), NULL);
    }

  GtkTextIter iter;

  gtk_text_buffer_get_start_iter (self->text_buffer, &iter);

  while (!gtk_text_iter_is_end (&iter))
    {
      gunichar c = gtk_text_iter_get_char (&iter);

      if (c == '{')
        {
          GtkTextIter word_end = iter;
          GtkTextIter word_start;

          gtk_text_iter_backward_char (&word_end);
          text_iter_skip_whitespace_backward (&word_end);

          word_start = word_end;
          gtk_text_iter_backward_word_start (&word_start);
          text_iter_skip_alpha_backward (&word_start);

          gtk_text_buffer_apply_tag_by_name (self->text_buffer, "nodename",
                                             &word_start, &word_end);
        }
      else if (c == ':')
        {
          GtkTextIter word_end = iter;
          GtkTextIter word_start;

          gtk_text_iter_backward_char (&word_end);
          text_iter_skip_whitespace_backward (&word_end);

          word_start = word_end;
          gtk_text_iter_backward_word_start (&word_start);
          text_iter_skip_alpha_backward (&word_start);

          gtk_text_buffer_apply_tag_by_name (self->text_buffer, "propname",
                                             &word_start, &word_end);
        }
      else if (c == '"')
        {
          GtkTextIter string_start = iter;
          GtkTextIter string_end = iter;

          gtk_text_iter_forward_char (&iter);
          while (!gtk_text_iter_is_end (&iter))
            {
              c = gtk_text_iter_get_char (&iter);

              if (c == '"')
                {
                  gtk_text_iter_forward_char (&iter);
                  string_end = iter;
                  break;
                }

              gtk_text_iter_forward_char (&iter);
            }

          gtk_text_buffer_apply_tag_by_name (self->text_buffer, "string",
                                             &string_start, &string_end);
        }

      gtk_text_iter_forward_char (&iter);
    }
}

static gboolean
text_view_query_tooltip_cb (GtkWidget        *widget,
                            int               x,
                            int               y,
                            gboolean          keyboard_tip,
                            GtkTooltip       *tooltip,
                            NodeEditorWindow *self)
{
  GtkTextIter iter;
  guint i;
  GString *text;

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

  text = g_string_new ("");

  for (i = 0; i < self->errors->len; i ++)
    {
      const TextViewError *e = &g_array_index (self->errors, TextViewError, i);
      GtkTextIter start_iter, end_iter;

      gtk_text_buffer_get_iter_at_offset (self->text_buffer, &start_iter, e->start_chars);
      gtk_text_buffer_get_iter_at_offset (self->text_buffer, &end_iter, e->end_chars);

      if (gtk_text_iter_in_range (&iter, &start_iter, &end_iter))
        {
          if (text->len > 0)
            g_string_append (text, "\n");
          g_string_append (text, e->message);
        }
    }

  if (text->len > 0)
    {
      gtk_tooltip_set_text (tooltip, text->str);
      g_string_free (text, TRUE);
      return TRUE;
    }
  else
    {
      g_string_free (text, TRUE);
      return FALSE;
    }
}

gboolean
node_editor_window_load (NodeEditorWindow *self,
                         GFile            *file)
{
  GtkTextIter end;
  GBytes *bytes;

  bytes = g_file_load_bytes (file, NULL, NULL, NULL);
  if (bytes == NULL)
    return FALSE;

  if (!g_utf8_validate (g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), NULL))
    {
      g_bytes_unref (bytes);
      return FALSE;
    }

  gtk_text_buffer_get_end_iter (self->text_buffer, &end);
  gtk_text_buffer_insert (self->text_buffer,
                          &end,
                          g_bytes_get_data (bytes, NULL),
                          g_bytes_get_size (bytes));

  g_bytes_unref (bytes);

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
show_open_filechooser (NodeEditorWindow *self)
{
  GtkWidget *dialog;

  dialog = gtk_file_chooser_dialog_new ("Open node file",
                                        GTK_WINDOW (self),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        "_Cancel", GTK_RESPONSE_CANCEL,
                                        "_Load", GTK_RESPONSE_ACCEPT,
                                        NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), ".");
  g_signal_connect (dialog, "response", G_CALLBACK (open_response_cb), self);
  gtk_widget_show (dialog);
}

static void
open_cb (GtkWidget        *button,
         NodeEditorWindow *self)
{
  show_open_filechooser (self);
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

  dialog = gtk_file_chooser_dialog_new ("Save node",
                                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (button))),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        "_Cancel", GTK_RESPONSE_CANCEL,
                                        "_Save", GTK_RESPONSE_ACCEPT,
                                        NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), ".");
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

  renderer = gtk_native_get_renderer (gtk_widget_get_native (GTK_WIDGET (self)));
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
  NodeEditorWindow *self = (NodeEditorWindow *)object;

  g_array_free (self->errors, TRUE);

  g_clear_object (&self->renderers);

  G_OBJECT_CLASS (node_editor_window_parent_class)->finalize (object);
}

static void
node_editor_window_add_renderer (NodeEditorWindow *self,
                                 GskRenderer      *renderer,
                                 const char       *description)
{
  GdkSurface *surface;
  GdkPaintable *paintable;

  surface = gtk_widget_get_surface (GTK_WIDGET (self));
  g_assert (surface != NULL);

  if (renderer != NULL && !gsk_renderer_realize (renderer, surface, NULL))
    {
      g_object_unref (renderer);
      return;
    }

  paintable = gtk_renderer_paintable_new (renderer, gtk_picture_get_paintable (GTK_PICTURE (self->picture)));
  g_object_set_data_full (G_OBJECT (paintable), "description", g_strdup (description), g_free);
  g_clear_object (&renderer);

  g_list_store_append (self->renderers, paintable);
  g_object_unref (paintable);
}

static void
node_editor_window_realize (GtkWidget *widget)
{
  NodeEditorWindow *self = NODE_EDITOR_WINDOW (widget);

  GTK_WIDGET_CLASS (node_editor_window_parent_class)->realize (widget);

#if 0
  node_editor_window_add_renderer (self,
                                   NULL,
                                   "Default");
#endif
  node_editor_window_add_renderer (self,
                                   gsk_gl_renderer_new (),
                                   "OpenGL");
#ifdef GDK_RENDERING_VULKAN
  node_editor_window_add_renderer (self,
                                   gsk_vulkan_renderer_new (),
                                   "Vulkan");
#endif
#ifdef GDK_WINDOWING_BROADWAY
  node_editor_window_add_renderer (self,
                                   gsk_broadway_renderer_new (),
                                   "Broadway");
#endif
  node_editor_window_add_renderer (self,
                                   gsk_cairo_renderer_new (),
                                   "Cairo");
}

static void
node_editor_window_unrealize (GtkWidget *widget)
{
  NodeEditorWindow *self = NODE_EDITOR_WINDOW (widget);

  g_list_store_remove_all (self->renderers);

  GTK_WIDGET_CLASS (node_editor_window_parent_class)->unrealize (widget);
}

static void
node_editor_window_class_init (NodeEditorWindowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->finalize = node_editor_window_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/gtk4/node-editor/node-editor-window.ui");

  widget_class->realize = node_editor_window_realize;
  widget_class->unrealize = node_editor_window_unrealize;

  gtk_widget_class_bind_template_child (widget_class, NodeEditorWindow, text_view);
  gtk_widget_class_bind_template_child (widget_class, NodeEditorWindow, picture);
  gtk_widget_class_bind_template_child (widget_class, NodeEditorWindow, renderer_listbox);

  gtk_widget_class_bind_template_callback (widget_class, text_view_query_tooltip_cb);
  gtk_widget_class_bind_template_callback (widget_class, open_cb);
  gtk_widget_class_bind_template_callback (widget_class, save_cb);
  gtk_widget_class_bind_template_callback (widget_class, export_image_cb);
}

static GtkWidget *
node_editor_window_create_renderer_widget (gpointer item,
                                           gpointer user_data)
{
  GdkPaintable *paintable = item;
  GtkWidget *box, *label, *picture;
  GtkWidget *row;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_size_request (box, 120, 90);

  label = gtk_label_new (g_object_get_data (G_OBJECT (paintable), "description"));
  gtk_container_add (GTK_CONTAINER (box), label);

  picture = gtk_picture_new_for_paintable (paintable);
  /* don't ever scale up, we want to be as accurate as possible */
  gtk_widget_set_halign (picture, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (picture, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (box), picture);

  row = gtk_list_box_row_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);

  return row;
}

static void
window_open (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  NodeEditorWindow *self = user_data;

  show_open_filechooser (self);
}

static GActionEntry win_entries[] = {
  { "open", window_open, NULL, NULL, NULL },
};

static void
node_editor_window_init (NodeEditorWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->renderers = g_list_store_new (GDK_TYPE_PAINTABLE);
  gtk_list_box_bind_model (GTK_LIST_BOX (self->renderer_listbox),
                           G_LIST_MODEL (self->renderers),
                           node_editor_window_create_renderer_widget,
                           self,
                           NULL);

  self->errors = g_array_new (FALSE, TRUE, sizeof (TextViewError));
  g_array_set_clear_func (self->errors, (GDestroyNotify)text_view_error_free);

  g_action_map_add_action_entries (G_ACTION_MAP (self), win_entries, G_N_ELEMENTS (win_entries), self);

  self->tag_table = gtk_text_tag_table_new ();
  gtk_text_tag_table_add (self->tag_table,
                          g_object_new (GTK_TYPE_TEXT_TAG,
                                        "name", "error",
                                        "underline", PANGO_UNDERLINE_ERROR,
                                        NULL));
  gtk_text_tag_table_add (self->tag_table,
                          g_object_new (GTK_TYPE_TEXT_TAG,
                                        "name", "nodename",
                                        "foreground-rgba", &(GdkRGBA) { 0.9, 0.78, 0.53, 1},
                                        NULL));
  gtk_text_tag_table_add (self->tag_table,
                          g_object_new (GTK_TYPE_TEXT_TAG,
                                        "name", "propname",
                                        "foreground-rgba", &(GdkRGBA) { 0.7, 0.55, 0.67, 1},
                                        NULL));
  gtk_text_tag_table_add (self->tag_table,
                          g_object_new (GTK_TYPE_TEXT_TAG,
                                        "name", "string",
                                        "foreground-rgba", &(GdkRGBA) { 0.63, 0.73, 0.54, 1},
                                        NULL));
  gtk_text_tag_table_add (self->tag_table,
                          g_object_new (GTK_TYPE_TEXT_TAG,
                                        "name", "number",
                                        "foreground-rgba", &(GdkRGBA) { 0.8, 0.52, 0.43, 1},
                                        NULL));

  self->text_buffer = gtk_text_buffer_new (self->tag_table);
  g_signal_connect (self->text_buffer, "changed", G_CALLBACK (text_changed), self);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (self->text_view), self->text_buffer);
}

NodeEditorWindow *
node_editor_window_new (NodeEditorApplication *application)
{
  return g_object_new (NODE_EDITOR_WINDOW_TYPE,
                       "application", application,
                       NULL);
}
