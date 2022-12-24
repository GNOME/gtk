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
#include "gsk/gl/gskglrenderer.h"
#ifdef GDK_WINDOWING_BROADWAY
#include "gsk/broadway/gskbroadwayrenderer.h"
#endif
#ifdef GDK_RENDERING_VULKAN
#include "gsk/vulkan/gskvulkanrenderer.h"
#endif

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

  GtkWidget *testcase_popover;
  GtkWidget *testcase_error_label;
  GtkWidget *testcase_cairo_checkbutton;
  GtkWidget *testcase_name_entry;
  GtkWidget *testcase_save_button;
  GtkWidget *scale_scale;

  GtkWidget *renderer_listbox;
  GListStore *renderers;
  GskRenderNode *node;

  GFileMonitor *file_monitor;

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

static char *
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
deserialize_error_func (const GskParseLocation *start_location,
                        const GskParseLocation *end_location,
                        const GError           *error,
                        gpointer                user_data)
{
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
  char *text;
  GBytes *bytes;
  GtkTextIter iter;
  GtkTextIter start, end;
  float scale;

  g_array_remove_range (self->errors, 0, self->errors->len);
  text = get_current_text (self->text_buffer);
  text_buffer_remove_all_tags (self->text_buffer);
  bytes = g_bytes_new_take (text, strlen (text));

  g_clear_pointer (&self->node, gsk_render_node_unref);

  /* If this is too slow, go fix the parser performance */
  self->node = gsk_render_node_deserialize (bytes, deserialize_error_func, self);

  scale = gtk_scale_button_get_value (GTK_SCALE_BUTTON (self->scale_scale));
  if (self->node && scale != 1.0)
    {
      GskRenderNode *node;

      node = gsk_transform_node_new (self->node, gsk_transform_scale (NULL, scale, scale));
      gsk_render_node_unref (self->node);
      self->node = node;
    }

  g_bytes_unref (bytes);
  if (self->node)
    {
      /* XXX: Is this code necessary or can we have API to turn nodes into paintables? */
      GtkSnapshot *snapshot;
      GdkPaintable *paintable;
      graphene_rect_t bounds;
      guint i;

      snapshot = gtk_snapshot_new ();
      gsk_render_node_get_bounds (self->node, &bounds);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (- bounds.origin.x, - bounds.origin.y));
      gtk_snapshot_append_node (snapshot, self->node);
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

  gtk_text_buffer_get_bounds (self->text_buffer, &start, &end);
  gtk_text_buffer_apply_tag_by_name (self->text_buffer, "no-hyphens",
                                     &start, &end);
}

static void
scale_changed (GObject          *object,
               GParamSpec       *pspec,
               NodeEditorWindow *self)
{
  text_changed (self->text_buffer, self);
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
      int offset;

      g_object_get (self->text_buffer, "cursor-position", &offset, NULL);
      gtk_text_buffer_get_iter_at_offset (self->text_buffer, &iter, offset);
    }
  else
    {
      int bx, by, trailing;

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

static gboolean
load_bytes (NodeEditorWindow *self,
            GBytes           *bytes);

static void
load_error (NodeEditorWindow *self,
            const char        *error_message)
{
  PangoLayout *layout;
  GtkSnapshot *snapshot;
  GskRenderNode *node;
  GBytes *bytes;

  layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), error_message);
  pango_layout_set_width (layout, 300 * PANGO_SCALE);
  snapshot = gtk_snapshot_new ();
  gtk_snapshot_append_layout (snapshot, layout, &(GdkRGBA) { 0.7, 0.13, 0.13, 1.0 });
  node = gtk_snapshot_free_to_node (snapshot);
  bytes = gsk_render_node_serialize (node);

  load_bytes (self, bytes);

  gsk_render_node_unref (node);
  g_object_unref (layout);
}

static gboolean
load_bytes (NodeEditorWindow *self,
            GBytes           *bytes)
{
  if (!g_utf8_validate (g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes), NULL))
    {
      load_error (self, "Invalid UTF-8");
      g_bytes_unref (bytes);
      return FALSE;
    }

  gtk_text_buffer_set_text (self->text_buffer,
                            g_bytes_get_data (bytes, NULL),
                            g_bytes_get_size (bytes));

  g_bytes_unref (bytes);

  return TRUE;
}

static gboolean
load_file_contents (NodeEditorWindow *self,
                    GFile            *file)
{
  GError *error = NULL;
  GBytes *bytes;

  bytes = g_file_load_bytes (file, NULL, NULL, &error);
  if (bytes == NULL)
    {
      load_error (self, error->message);
      g_clear_error (&error);
      return FALSE;
    }

  return load_bytes (self, bytes);
}

static GdkContentProvider *
on_picture_drag_prepare_cb (GtkDragSource    *source,
                            double            x,
                            double            y,
                            NodeEditorWindow *self)
{
  if (self->node == NULL)
    return NULL;

  return gdk_content_provider_new_typed (GSK_TYPE_RENDER_NODE, self->node);
}

static void
on_picture_drop_read_done_cb (GObject      *source,
                              GAsyncResult *res,
                              gpointer      data)
{
  NodeEditorWindow *self = data;
  GOutputStream *stream = G_OUTPUT_STREAM (source);
  GdkDrop *drop = g_object_get_data (source, "drop");
  GdkDragAction action = 0;
  GBytes *bytes;

  if (g_output_stream_splice_finish (stream, res, NULL) >= 0)
    {
      bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (stream));
      if (load_bytes (self, bytes))
        action = GDK_ACTION_COPY;
    }

  g_object_unref (self);
  gdk_drop_finish (drop, action);
  g_object_unref (drop);
  return;
}

static void
on_picture_drop_read_cb (GObject      *source,
                         GAsyncResult *res,
                         gpointer      data)
{
  NodeEditorWindow *self = data;
  GdkDrop *drop = GDK_DROP (source);
  GInputStream *input;
  GOutputStream *output;

  input = gdk_drop_read_finish (drop, res, NULL, NULL);
  if (input == NULL)
    {
      g_object_unref (self);
      gdk_drop_finish (drop, 0);
      return;
    }

  output = g_memory_output_stream_new_resizable ();
  g_object_set_data (G_OBJECT (output), "drop", drop);
  g_object_ref (drop);

  g_output_stream_splice_async (output,
                                input,
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                G_PRIORITY_DEFAULT,
                                NULL,
                                on_picture_drop_read_done_cb,
                                self);
  g_object_unref (output);
  g_object_unref (input);
}

static gboolean
on_picture_drop_cb (GtkDropTargetAsync *dest,
                    GdkDrop            *drop,
                    double              x,
                    double              y,
                    NodeEditorWindow   *self)
{
  gdk_drop_read_async (drop,
                       (const char *[2]) { "application/x-gtk-render-node", NULL },
                       G_PRIORITY_DEFAULT,
                       NULL,
                       on_picture_drop_read_cb,
                       g_object_ref (self));

  return TRUE;
}

static void
file_changed_cb (GFileMonitor      *monitor,
                 GFile             *file,
                 GFile             *other_file,
                 GFileMonitorEvent  event_type,
                 gpointer           user_data)
{
  NodeEditorWindow *self = user_data;

  if (event_type == G_FILE_MONITOR_EVENT_CHANGED)
    load_file_contents (self, file);
}

gboolean
node_editor_window_load (NodeEditorWindow *self,
                         GFile            *file)
{
  GError *error = NULL;

  g_clear_object (&self->file_monitor);

  if (!load_file_contents (self, file))
    return FALSE;

  self->file_monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, &error);

  if (error)
    {
      g_warning ("couldn't monitor file: %s", error->message);
      g_error_free (error);
      g_clear_object (&self->file_monitor);
    }
  else
    {
      g_signal_connect (self->file_monitor, "changed", G_CALLBACK (file_changed_cb), self);
    }

  return TRUE;
}

static void
open_response_cb (GObject *source,
                  GAsyncResult *result,
                  void *user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  NodeEditorWindow *self = user_data;
  GFile *file;

  file = gtk_file_dialog_open_finish (dialog, result, NULL);
  if (file)
    {
      node_editor_window_load (self, file);
      g_object_unref (file);
    }
}

static void
show_open_filechooser (NodeEditorWindow *self)
{
  GtkFileDialog *dialog;
  GFile *cwd;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, "Open node file");
  cwd = g_file_new_for_path (".");
  gtk_file_dialog_set_initial_folder (dialog, cwd);
  g_object_unref (cwd);
  gtk_file_dialog_open (dialog, GTK_WINDOW (self),
                        NULL, open_response_cb, self);
  g_object_unref (dialog);
}

static void
open_cb (GtkWidget        *button,
         NodeEditorWindow *self)
{
  show_open_filechooser (self);
}

static void
save_response_cb (GObject *source,
                  GAsyncResult *result,
                  void *user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  NodeEditorWindow *self = user_data;
  GFile *file;

  file = gtk_file_dialog_save_finish (dialog, result, NULL);
  if (file)
    {
      char *text;
      GError *error = NULL;

      text = get_current_text (self->text_buffer);

      g_file_replace_contents (file, text, strlen (text),
                               NULL, FALSE,
                               G_FILE_CREATE_NONE,
                               NULL,
                               NULL,
                               &error);
      if (error != NULL)
        {
          GtkAlertDialog *alert;

          alert = gtk_alert_dialog_new ("Saving failed");
          gtk_alert_dialog_set_detail (alert, error->message);
          gtk_alert_dialog_show (alert,
                                 GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self))));
          g_object_unref (alert);
          g_error_free (error);
        }

      g_free (text);
      g_object_unref (file);
    }
}

static void
save_cb (GtkWidget        *button,
         NodeEditorWindow *self)
{
  GtkFileDialog *dialog;
  GFile *cwd;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, "Save node");
  cwd = g_file_new_for_path (".");
  gtk_file_dialog_set_initial_folder (dialog, cwd);
  gtk_file_dialog_set_initial_name (dialog, "demo.node");
  g_object_unref (cwd);
  gtk_file_dialog_save (dialog,
                        GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (button))),
                        NULL,
                        save_response_cb, self);
  g_object_unref (dialog);
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

static GdkTexture *
create_cairo_texture (NodeEditorWindow *self)
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

  renderer = gsk_cairo_renderer_new ();
  gsk_renderer_realize (renderer, NULL, NULL);

  texture = gsk_renderer_render_texture (renderer, node, NULL);
  gsk_render_node_unref (node);
  gsk_renderer_unrealize (renderer);
  g_object_unref (renderer);

  return texture;
}

static void
export_image_response_cb (GObject *source,
                          GAsyncResult *result,
                          void *user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  GdkTexture *texture = user_data;
  GFile *file;

  file = gtk_file_dialog_save_finish (dialog, result, NULL);
  if (file)
    {
      if (!gdk_texture_save_to_png (texture, g_file_peek_path (file)))
        {
          GtkAlertDialog *alert;

          alert = gtk_alert_dialog_new ("Exporting to image failed");
          gtk_alert_dialog_show (alert, GTK_WINDOW (gtk_window_get_transient_for (GTK_WINDOW (dialog))));
          g_object_unref (alert);
        }

      g_object_unref (file);
    }

  g_object_unref (texture);
}

static void
export_image_cb (GtkWidget        *button,
                 NodeEditorWindow *self)
{
  GdkTexture *texture;
  GtkFileDialog *dialog;

  texture = create_texture (self);
  if (texture == NULL)
    return;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, "");
  gtk_file_dialog_set_initial_name (dialog, "example.png");
  gtk_file_dialog_save (dialog,
                        GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (button))),
                        NULL,
                        export_image_response_cb, texture);
  g_object_unref (dialog);
}


static void
clip_image_cb (GtkWidget        *button,
               NodeEditorWindow *self)
{
  GdkTexture *texture;
  GdkClipboard *clipboard;

  texture = create_texture (self);
  if (texture == NULL)
    return;

  clipboard = gtk_widget_get_clipboard (GTK_WIDGET (self));

  gdk_clipboard_set_texture (clipboard, texture);

  g_object_unref (texture);
}

static void
testcase_name_entry_changed_cb (GtkWidget        *button,
                                GParamSpec       *pspec,
                                NodeEditorWindow *self)

{
  const char *text = gtk_editable_get_text (GTK_EDITABLE (self->testcase_name_entry));

  if (strlen (text) > 0)
    gtk_widget_set_sensitive (self->testcase_save_button, TRUE);
  else
    gtk_widget_set_sensitive (self->testcase_save_button, FALSE);
}

/* Returns the location where gsk test cases are stored in
 * the GTK testsuite, if we can determine it.
 *
 * When running node editor outside of a GTK build, you can
 * set GTK_SOURCE_DIR to point it at the checkout.
 */
static char *
get_source_dir (void)
{
  const char *subdir = "testsuite/gsk/compare";
  const char *source_dir;
  char *current_dir;
  char *dir;

  source_dir = g_getenv ("GTK_SOURCE_DIR");
  current_dir = g_get_current_dir ();

  if (source_dir)
    {
      char *abs_source_dir = g_canonicalize_filename (source_dir, NULL);
      dir = g_canonicalize_filename (subdir, abs_source_dir);
      g_free (abs_source_dir);
    }
  else
    {
      dir = g_canonicalize_filename (subdir, current_dir);
    }

  if (g_file_test (dir, G_FILE_TEST_EXISTS))
    {
      g_free (current_dir);

      return dir;
    }

  g_free (dir);

  return current_dir;
}

static void
testcase_save_clicked_cb (GtkWidget        *button,
                          NodeEditorWindow *self)
{
  const char *testcase_name = gtk_editable_get_text (GTK_EDITABLE (self->testcase_name_entry));
  char *source_dir = get_source_dir ();
  char *node_file_name;
  char *node_file;
  char *png_file_name;
  char *png_file;
  char *text = NULL;
  GdkTexture *texture;
  GError *error = NULL;

  node_file_name = g_strconcat (testcase_name, ".node", NULL);
  node_file = g_build_filename (source_dir, node_file_name, NULL);
  g_free (node_file_name);

  g_debug ("Saving testcase in %s", node_file);

  png_file_name = g_strconcat (testcase_name, ".png", NULL);
  png_file = g_build_filename (source_dir, png_file_name, NULL);
  g_free (png_file_name);

  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (self->testcase_cairo_checkbutton)))
    texture = create_cairo_texture (self);
  else
    texture = create_texture (self);

  if (!gdk_texture_save_to_png (texture, png_file))
    {
      gtk_label_set_label (GTK_LABEL (self->testcase_error_label),
                           "Could not save texture file");
      goto out;
    }

  text = get_current_text (self->text_buffer);
  if (!g_file_set_contents (node_file, text, -1, &error))
    {
      gtk_label_set_label (GTK_LABEL (self->testcase_error_label), error->message);
      /* TODO: Remove texture file again? */
      goto out;
    }

  gtk_editable_set_text (GTK_EDITABLE (self->testcase_name_entry), "");
  gtk_popover_popdown (GTK_POPOVER (self->testcase_popover));

out:
  g_free (text);
  g_free (png_file);
  g_free (node_file);
  g_free (source_dir);
}

static void
dark_mode_cb (GtkToggleButton *button,
              GParamSpec      *pspec,
              NodeEditorWindow *self)
{
  g_object_set (gtk_widget_get_settings (GTK_WIDGET (self)),
                "gtk-application-prefer-dark-theme", gtk_toggle_button_get_active (button),
                NULL);
}

static void
node_editor_window_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), NODE_EDITOR_WINDOW_TYPE);

  G_OBJECT_CLASS (node_editor_window_parent_class)->dispose (object);
}

static void
node_editor_window_finalize (GObject *object)
{
  NodeEditorWindow *self = (NodeEditorWindow *)object;

  g_array_free (self->errors, TRUE);

  g_clear_pointer (&self->node, gsk_render_node_unref);
  g_clear_object (&self->renderers);

  G_OBJECT_CLASS (node_editor_window_parent_class)->finalize (object);
}

static void
node_editor_window_add_renderer (NodeEditorWindow *self,
                                 GskRenderer      *renderer,
                                 const char       *description)
{
  GdkPaintable *paintable;

  if (!gsk_renderer_realize (renderer, NULL, NULL))
    {
      GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (self));
      g_assert (surface != NULL);

      if (!gsk_renderer_realize (renderer, surface, NULL))
        {
          g_object_unref (renderer);
          return;
        }
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
  guint i;

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->renderers)); i ++)
    {
      gpointer item = g_list_model_get_item (G_LIST_MODEL (self->renderers), i);
      gsk_renderer_unrealize (gtk_renderer_paintable_get_renderer (item));
      g_object_unref (item);
    }

  g_list_store_remove_all (self->renderers);

  GTK_WIDGET_CLASS (node_editor_window_parent_class)->unrealize (widget);
}

static void
node_editor_window_class_init (NodeEditorWindowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = node_editor_window_dispose;
  object_class->finalize = node_editor_window_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/gtk4/node-editor/node-editor-window.ui");

  widget_class->realize = node_editor_window_realize;
  widget_class->unrealize = node_editor_window_unrealize;

  gtk_widget_class_bind_template_child (widget_class, NodeEditorWindow, text_view);
  gtk_widget_class_bind_template_child (widget_class, NodeEditorWindow, picture);
  gtk_widget_class_bind_template_child (widget_class, NodeEditorWindow, renderer_listbox);
  gtk_widget_class_bind_template_child (widget_class, NodeEditorWindow, testcase_popover);
  gtk_widget_class_bind_template_child (widget_class, NodeEditorWindow, testcase_error_label);
  gtk_widget_class_bind_template_child (widget_class, NodeEditorWindow, testcase_cairo_checkbutton);
  gtk_widget_class_bind_template_child (widget_class, NodeEditorWindow, testcase_name_entry);
  gtk_widget_class_bind_template_child (widget_class, NodeEditorWindow, testcase_save_button);
  gtk_widget_class_bind_template_child (widget_class, NodeEditorWindow, scale_scale);

  gtk_widget_class_bind_template_callback (widget_class, text_view_query_tooltip_cb);
  gtk_widget_class_bind_template_callback (widget_class, open_cb);
  gtk_widget_class_bind_template_callback (widget_class, save_cb);
  gtk_widget_class_bind_template_callback (widget_class, export_image_cb);
  gtk_widget_class_bind_template_callback (widget_class, clip_image_cb);
  gtk_widget_class_bind_template_callback (widget_class, testcase_save_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, testcase_name_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, dark_mode_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_picture_drag_prepare_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_picture_drop_cb);
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
  gtk_widget_add_css_class (label, "title-4");
  gtk_box_append (GTK_BOX (box), label);

  picture = gtk_picture_new_for_paintable (paintable);
  /* don't ever scale up, we want to be as accurate as possible */
  gtk_widget_set_halign (picture, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (picture, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (box), picture);

  row = gtk_list_box_row_new ();
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
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
  gtk_text_tag_table_add (self->tag_table,
                          g_object_new (GTK_TYPE_TEXT_TAG,
                                        "name", "no-hyphens",
                                        "insert-hyphens", FALSE,
                                        NULL));

  self->text_buffer = gtk_text_buffer_new (self->tag_table);
  g_signal_connect (self->text_buffer, "changed", G_CALLBACK (text_changed), self);
  g_signal_connect (self->scale_scale, "notify::value", G_CALLBACK (scale_changed), self);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (self->text_view), self->text_buffer);

  /* Default */
  gtk_text_buffer_set_text (self->text_buffer,
         "shadow {\n"
         "  child: texture {\n"
         "    bounds: 0 0 128 128;\n"
         "    texture: url(\"resource:///org/gtk/gtk4/node-editor/icons/apps/org.gtk.gtk4.NodeEditor.svg\");\n"
         "  }\n"
         "  shadows: rgba(0,0,0,0.5) 0 1 12;\n"
         "}\n"
         "\n"
         "transform {\n"
         "  child: text {\n"
         "    color: rgb(46,52,54);\n"
         "    font: \"Cantarell Bold 11\";\n"
         "    glyphs: \"GTK Node Editor\";\n"
         "    offset: 8 14.418;\n"
         "  }\n"
         "  transform: translate(0, 140);\n"
         "}", -1);
}

NodeEditorWindow *
node_editor_window_new (NodeEditorApplication *application)
{
  return g_object_new (NODE_EDITOR_WINDOW_TYPE,
                       "application", application,
                       NULL);
}
