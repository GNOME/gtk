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

#include "css-editor.h"

#include "gtkcssprovider.h"
#include "gtkcssrbtreeprivate.h"
#include "gtkcsstokenizerprivate.h"
#include "gtkstyleprovider.h"
#include "gtkstylecontext.h"
#include "gtktextview.h"
#include "gtkmessagedialog.h"
#include "gtkfilechooserdialog.h"
#include "gtktogglebutton.h"
#include "gtklabel.h"
#include "gtktooltip.h"
#include "gtktextiter.h"


typedef struct _GtkCssChunk GtkCssChunk;
typedef struct _GtkCssChunkSize GtkCssChunkSize;

struct _GtkCssChunkSize
{
  gsize              bytes;
  gsize              chars;
  gsize              line_breaks;
  gsize              line_bytes;
  gsize              line_chars;
};

struct _GtkCssChunk
{
  GtkCssToken        token;

  GtkCssChunkSize    size;
};

struct _GtkInspectorCssEditorPrivate
{
  GtkWidget *view;
  GtkTextBuffer *text;
  GtkCssRbTree *tokens;
  GtkCssProvider *provider;
  GtkToggleButton *disable_button;
  guint timeout;
  GList *errors;
};

typedef struct {
  GError *error;
  GtkTextIter start;
  GtkTextIter end;
} CssError;

static void
css_error_free (gpointer data)
{
  CssError *error = data;
  g_error_free (error->error);
  g_free (error);
}

static void
gtk_css_chunk_clear (gpointer data)
{
  GtkCssChunk *chunk = data;

  gtk_css_token_clear (&chunk->token);
}

static void
gtk_css_chunk_size_clear (GtkCssChunkSize *size)
{
  size->bytes = 0;
  size->chars = 0;
  size->line_breaks = 0;
  size->line_bytes = 0;
  size->line_chars = 0;
}

static void
gtk_css_chunk_size_add (GtkCssChunkSize *size,
                        GtkCssChunkSize *add)
{
  size->bytes += add->bytes;
  size->chars += add->chars;
  size->line_breaks += add->line_breaks;
  if (add->line_breaks)
    {
      size->line_bytes = add->line_bytes;
      size->line_chars = add->line_chars;
    }
  else
    {
      size->line_bytes += add->bytes;
      size->line_chars += add->chars;
    }
}

static void
gtk_css_chunk_augment (GtkCssRbTree *tree,
                       gpointer      aug,
                       gpointer      node,
                       gpointer      lnode,
                       gpointer      rnode)
{
  GtkCssChunkSize *size = aug;
  GtkCssChunk *chunk = node;

  gtk_css_chunk_size_clear (size);

  if (lnode)
    gtk_css_chunk_size_add (size, gtk_css_rb_tree_get_augment (tree, lnode));
  gtk_css_chunk_size_add (size, &chunk->size);
  if (rnode)
    gtk_css_chunk_size_add (size, gtk_css_rb_tree_get_augment (tree, rnode));
}
 
static gboolean
query_tooltip_cb (GtkWidget             *widget,
                  gint                   x,
                  gint                   y,
                  gboolean               keyboard_tip,
                  GtkTooltip            *tooltip,
                  GtkInspectorCssEditor *ce)
{
  GtkTextIter iter;
  GList *l;

  if (keyboard_tip)
    {
      gint offset;

      g_object_get (ce->priv->text, "cursor-position", &offset, NULL);
      gtk_text_buffer_get_iter_at_offset (ce->priv->text, &iter, offset);
    }
  else
    {
      gint bx, by, trailing;

      gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (ce->priv->view), GTK_TEXT_WINDOW_TEXT,
                                             x, y, &bx, &by);
      gtk_text_view_get_iter_at_position (GTK_TEXT_VIEW (ce->priv->view), &iter, &trailing, bx, by);
    }

  for (l = ce->priv->errors; l; l = l->next)
    {
      CssError *css_error = l->data;

      if (gtk_text_iter_in_range (&iter, &css_error->start, &css_error->end))
        {
          gtk_tooltip_set_text (tooltip, css_error->error->message);
          return TRUE;
        }
    }

  return FALSE;
}

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorCssEditor, gtk_inspector_css_editor, GTK_TYPE_BOX)

static void
set_initial_text (GtkInspectorCssEditor *ce)
{
  const gchar *text = NULL;

  if (text)
    gtk_text_buffer_set_text (GTK_TEXT_BUFFER (ce->priv->text), text, -1);
  else
    {
      gchar *initial_text;
      initial_text = g_strconcat ("/*\n",
                                  _("You can type here any CSS rule recognized by GTK+."), "\n",
                                  _("You can temporarily disable this custom CSS by clicking on the “Pause” button above."), "\n\n",
                                  _("Changes are applied instantly and globally, for the whole application."), "\n",
                                  "*/\n\n", NULL);
      gtk_text_buffer_set_text (GTK_TEXT_BUFFER (ce->priv->text), initial_text, -1);
      g_free (initial_text);
    }
}

static void
disable_toggled (GtkToggleButton       *button,
                 GtkInspectorCssEditor *ce)
{
  if (gtk_toggle_button_get_active (button))
    gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (),
                                                  GTK_STYLE_PROVIDER (ce->priv->provider));
  else
    gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                               GTK_STYLE_PROVIDER (ce->priv->provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_USER);
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
save_to_file (GtkInspectorCssEditor *ce,
              const gchar           *filename)
{
  gchar *text;
  GError *error = NULL;

  text = get_current_text (ce->priv->text);

  if (!g_file_set_contents (filename, text, -1, &error))
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (ce))),
                                       GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_INFO,
                                       GTK_BUTTONS_OK,
                                       _("Saving CSS failed"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                "%s", error->message);
      g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
      gtk_widget_show (dialog);
      g_error_free (error);
    }

  g_free (text);
}

static void
save_response (GtkWidget             *dialog,
               gint                   response,
               GtkInspectorCssEditor *ce)
{
  gtk_widget_hide (dialog);

  if (response == GTK_RESPONSE_ACCEPT)
    {
      gchar *filename;

      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      save_to_file (ce, filename);
      g_free (filename);
    }

  gtk_widget_destroy (dialog);
}

static void
save_clicked (GtkButton             *button,
              GtkInspectorCssEditor *ce)
{
  GtkWidget *dialog;

  dialog = gtk_file_chooser_dialog_new ("",
                                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (ce))),
                                        GTK_FILE_CHOOSER_ACTION_SAVE,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_Save"), GTK_RESPONSE_ACCEPT,
                                        NULL);
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), "custom.css");
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
  g_signal_connect (dialog, "response", G_CALLBACK (save_response), ce);
  gtk_widget_show (dialog);
}

static void
gtk_css_chunk_get_iters (GtkInspectorCssEditor *ce,
                         GtkCssChunk           *chunk,
                         GtkTextIter           *start,
                         GtkTextIter           *end)
{
  GtkInspectorCssEditorPrivate *priv = ce->priv;
  GtkCssChunk *c, *l, *prev = NULL;
  gsize offset;

  offset = 0;
  l = gtk_css_rb_tree_get_left (priv->tokens, chunk);
  if (l)
    {
      GtkCssChunkSize *size = gtk_css_rb_tree_get_augment (priv->tokens, l);
      offset = size->chars;
    }
  else
    {
      offset = 0;
    }
  prev = chunk;
  for (c = gtk_css_rb_tree_get_parent (priv->tokens, chunk);
       c != NULL;
       c = gtk_css_rb_tree_get_parent (priv->tokens, c))
    {
      l = gtk_css_rb_tree_get_left (priv->tokens, c);

      if (l != prev)
        {
          GtkCssChunkSize *size = gtk_css_rb_tree_get_augment (priv->tokens, l);
          offset += size->chars;
          offset += c->size.chars;
        }
      prev = c;
    }

  if (start)
    gtk_text_buffer_get_iter_at_offset (priv->text, start, offset);
  if (end)
    gtk_text_buffer_get_iter_at_offset (priv->text, end, offset + chunk->size.chars);
}

static void
update_token_tags (GtkInspectorCssEditor *ce,
                   GtkCssChunk           *start,
                   GtkCssChunk           *end)
{
  GtkInspectorCssEditorPrivate *priv = ce->priv;
  GtkCssChunk *chunk;

  for (chunk = start;
       chunk != end;
       chunk = gtk_css_rb_tree_get_next (priv->tokens, chunk))
    {
      GtkTextIter start, end;
      const char *tag_name;

      if (chunk->token.type == GTK_CSS_TOKEN_COMMENT)
        tag_name = "comment";
      else if (chunk->token.type == GTK_CSS_TOKEN_STRING)
        tag_name = "string";
      else
        continue;

      gtk_css_chunk_get_iters (ce, chunk, &start, &end);
      gtk_text_buffer_apply_tag_by_name (priv->text, tag_name, &start, &end);
    }
}

static void
update_tokenize (GtkInspectorCssEditor *ce)
{
  GtkInspectorCssEditorPrivate *priv = ce->priv;
  GtkCssTokenizer *tokenizer;
  GtkCssChunk *chunk;
  GBytes *gbytes;
  char *text;
  gsize bytes = 0;
  gsize chars = 0;
  gsize lines = 1;

  if (priv->tokens)
    gtk_css_rb_tree_unref (priv->tokens);
  priv->tokens = gtk_css_rb_tree_new (GtkCssChunk,
                                      GtkCssChunkSize,
                                      gtk_css_chunk_augment,
                                      gtk_css_chunk_clear,
                                      NULL);

  text = get_current_text (priv->text);
  gbytes = g_bytes_new_take (text, strlen (text));
  tokenizer = gtk_css_tokenizer_new (gbytes, NULL, ce, NULL);
  chunk = gtk_css_rb_tree_insert_after (priv->tokens, NULL);

  for (gtk_css_tokenizer_read_token (tokenizer, &chunk->token);
       chunk->token.type != GTK_CSS_TOKEN_EOF;
       gtk_css_tokenizer_read_token (tokenizer, &chunk->token))
    {
      chunk->size.bytes = gtk_css_tokenizer_get_byte (tokenizer) - bytes;
      bytes += chunk->size.bytes;
      chunk->size.chars = gtk_css_tokenizer_get_char (tokenizer) - chars;
      chars += chunk->size.chars;
      chunk->size.line_breaks = gtk_css_tokenizer_get_line (tokenizer) - lines;
      lines += chunk->size.line_breaks;
      if (chunk->size.line_breaks)
        {
          chunk->size.line_bytes = gtk_css_tokenizer_get_line_byte (tokenizer);
          chunk->size.line_chars = gtk_css_tokenizer_get_line_char (tokenizer);
        }
      else
        {
          chunk->size.line_bytes = chunk->size.bytes;
          chunk->size.line_chars = chunk->size.chars;
        }

      chunk = gtk_css_rb_tree_insert_after (priv->tokens, chunk);
    } 

  gtk_css_rb_tree_remove (priv->tokens, chunk);
  gtk_css_tokenizer_unref (tokenizer);
  g_bytes_unref (gbytes);

  update_token_tags (ce, gtk_css_rb_tree_get_first (priv->tokens), NULL);
}

static void
update_style (GtkInspectorCssEditor *ce)
{
  gchar *text;

  g_list_free_full (ce->priv->errors, css_error_free);
  ce->priv->errors = NULL;

  text = get_current_text (ce->priv->text);
  gtk_css_provider_load_from_data (ce->priv->provider, text, -1, NULL);
  g_free (text);
}

static void
clear_all_tags (GtkInspectorCssEditor *ce)
{
  GtkTextIter start, end;

  gtk_text_buffer_get_start_iter (ce->priv->text, &start);
  gtk_text_buffer_get_end_iter (ce->priv->text, &end);
  gtk_text_buffer_remove_all_tags (ce->priv->text, &start, &end);
}

static gboolean
update_timeout (gpointer data)
{
  GtkInspectorCssEditor *ce = data;

  ce->priv->timeout = 0;

  clear_all_tags (ce);
  update_style (ce);
  update_tokenize (ce);

  return G_SOURCE_REMOVE;
}

static void
text_changed (GtkTextBuffer         *buffer,
              GtkInspectorCssEditor *ce)
{
  if (ce->priv->timeout != 0)
    g_source_remove (ce->priv->timeout);

  ce->priv->timeout = g_timeout_add (100, update_timeout, ce); 

  g_list_free_full (ce->priv->errors, css_error_free);
  ce->priv->errors = NULL;
}

static void
show_parsing_error (GtkCssProvider        *provider,
                    GtkCssSection         *section,
                    const GError          *error,
                    GtkInspectorCssEditor *ce)
{
  const char *tag_name;
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (ce->priv->text);
  CssError *css_error;

  css_error = g_new (CssError, 1);
  css_error->error = g_error_copy (error);

  gtk_text_buffer_get_iter_at_line_index (buffer,
                                          &css_error->start,
                                          gtk_css_section_get_start_line (section),
                                          gtk_css_section_get_start_position (section));
  gtk_text_buffer_get_iter_at_line_index (buffer,
                                          &css_error->end,
                                          gtk_css_section_get_end_line (section),
                                          gtk_css_section_get_end_position (section));

  if (g_error_matches (error, GTK_CSS_PROVIDER_ERROR, GTK_CSS_PROVIDER_ERROR_DEPRECATED))
    tag_name = "warning";
  else
    tag_name = "error";

  if (gtk_text_iter_equal (&css_error->start, &css_error->end))
    gtk_text_iter_forward_char (&css_error->end);

  gtk_text_buffer_apply_tag_by_name (buffer, tag_name, &css_error->start, &css_error->end);

  ce->priv->errors = g_list_prepend (ce->priv->errors, css_error);
}

static void
create_provider (GtkInspectorCssEditor *ce)
{
  ce->priv->provider = gtk_css_provider_new ();
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (ce->priv->provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_USER);

  g_signal_connect (ce->priv->provider, "parsing-error",
                    G_CALLBACK (show_parsing_error), ce);
}

static void
destroy_provider (GtkInspectorCssEditor *ce)
{
  gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (),
                                                GTK_STYLE_PROVIDER (ce->priv->provider));
  g_clear_object (&ce->priv->provider);
}

static void
gtk_inspector_css_editor_init (GtkInspectorCssEditor *ce)
{
  ce->priv = gtk_inspector_css_editor_get_instance_private (ce);
  gtk_widget_init_template (GTK_WIDGET (ce));
  ce->priv->tokens = gtk_css_rb_tree_new (GtkCssChunk,
                                          GtkCssChunkSize,
                                          gtk_css_chunk_augment,
                                          gtk_css_chunk_clear,
                                          NULL);
}

static void
constructed (GObject *object)
{
  GtkInspectorCssEditor *ce = GTK_INSPECTOR_CSS_EDITOR (object);

  create_provider (ce);
  set_initial_text (ce);
}

static void
finalize (GObject *object)
{
  GtkInspectorCssEditor *ce = GTK_INSPECTOR_CSS_EDITOR (object);

  if (ce->priv->timeout != 0)
    g_source_remove (ce->priv->timeout);

  if (ce->priv->tokens)
    gtk_css_rb_tree_unref (ce->priv->tokens);

  destroy_provider (ce);

  g_list_free_full (ce->priv->errors, css_error_free);

  G_OBJECT_CLASS (gtk_inspector_css_editor_parent_class)->finalize (object);
}

static void
gtk_inspector_css_editor_class_init (GtkInspectorCssEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = constructed;
  object_class->finalize = finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/css-editor.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssEditor, text);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssEditor, view);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssEditor, disable_button);
  gtk_widget_class_bind_template_callback (widget_class, disable_toggled);
  gtk_widget_class_bind_template_callback (widget_class, save_clicked);
  gtk_widget_class_bind_template_callback (widget_class, text_changed);
  gtk_widget_class_bind_template_callback (widget_class, query_tooltip_cb);
}

// vim: set et sw=2 ts=2:
