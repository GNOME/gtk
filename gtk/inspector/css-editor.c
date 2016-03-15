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

#include "gtkcssdeclarationprivate.h"
#include "gtkcssprovider.h"
#include "gtkcssrbtreeprivate.h"
#include "gtkcssstyleruleprivate.h"
#include "gtkcssstylesheetprivate.h"
#include "gtkcsstokenizerprivate.h"
#include "gtkcsstokensourceprivate.h"
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
typedef struct _GtkCssChunkTokenSource GtkCssChunkTokenSource;

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
  /* must be first element so we can cast between them */
  GtkCssToken        token;

  GtkCssChunkSize    size;
  GObject           *consumer;
  GError            *error;
};

struct _GtkCssChunkTokenSource
{
  GtkCssTokenSource parent;
  GtkCssRbTree *tree;
  GtkCssChunk  *chunk;
};

struct _GtkInspectorCssEditorPrivate
{
  GtkWidget *view;
  GtkTextBuffer *text;
  GtkCssRbTree *tokens;
  GtkCssProvider *provider;
  GtkCssStyleSheet *style_sheet;
  GtkCssNode *node;
  GtkToggleButton *disable_button;
  guint timeout;
};

static void
gtk_css_chunk_clear (gpointer data)
{
  GtkCssChunk *chunk = data;

  gtk_css_token_clear (&chunk->token);
  g_clear_object (&chunk->consumer);
  g_clear_error (&chunk->error);
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
 
static void
gtk_css_chunk_token_source_finalize (GtkCssTokenSource *source)
{
  GtkCssChunkTokenSource *chunk = (GtkCssChunkTokenSource *) source;

  gtk_css_rb_tree_unref (chunk->tree);
}

static void
gtk_css_chunk_token_source_consume_token (GtkCssTokenSource *source,
                                          GObject           *consumer)
{
  GtkCssChunkTokenSource *chunk = (GtkCssChunkTokenSource *) source;

  if (chunk->chunk == NULL)
    return;

  chunk->chunk->consumer = g_object_ref (consumer);
  chunk->chunk = gtk_css_rb_tree_get_next (chunk->tree, chunk->chunk);
}

static const GtkCssToken *
gtk_css_chunk_token_source_get_token (GtkCssTokenSource   *source)
{
  GtkCssChunkTokenSource *chunk = (GtkCssChunkTokenSource *) source;
  static const GtkCssToken eof_token = { GTK_CSS_TOKEN_EOF };

  if (chunk->chunk == NULL)
    return &eof_token;

  return &chunk->chunk->token;
}

static void
gtk_css_chunk_token_source_error (GtkCssTokenSource *source,
                                  const GError      *error)
{
  GtkCssChunkTokenSource *chunk = (GtkCssChunkTokenSource *) source;

  if (chunk->chunk == NULL)
    return;

  if (chunk->chunk->error)
    g_warning ("figure out what to do with two errors on the same token");
  else
    chunk->chunk->error = g_error_copy (error);
}

const GtkCssTokenSourceClass GTK_CSS_CHUNK_TOKEN_SOURCE = {
  gtk_css_chunk_token_source_finalize,
  gtk_css_chunk_token_source_consume_token,
  gtk_css_chunk_token_source_get_token,
  gtk_css_chunk_token_source_error,
};

static GtkCssTokenSource *
gtk_css_chunk_token_source_new (GtkCssRbTree *tree)
{
  GtkCssChunkTokenSource *chunk;

  chunk = gtk_css_token_source_new (GtkCssChunkTokenSource, &GTK_CSS_CHUNK_TOKEN_SOURCE);
  chunk->tree = gtk_css_rb_tree_ref (tree);
  chunk->chunk = gtk_css_rb_tree_get_first (tree);

  return &chunk->parent;
}

static gint
compare_chunk_to_offset (GtkCssRbTree *tree,
                         gpointer      node,
                         gpointer      offsetp)
{
  GtkCssChunk *left, *chunk;
  gsize *offset = offsetp;

  chunk = node;

  left = gtk_css_rb_tree_get_left (tree, chunk);
  if (left)
    {
      GtkCssChunkSize *left_size = gtk_css_rb_tree_get_augment (tree, left);
      if (*offset < left_size->chars)
        return 1;
      *offset -= left_size->chars;
    }

  if (*offset < chunk->size.chars)
    return 0;

  *offset -= chunk->size.chars;
  return -1;
}

static GtkCssChunk *
find_chunk_for_offset (GtkInspectorCssEditor *ce,
                       gsize                  offset)
{
  GtkInspectorCssEditorPrivate *priv = ce->priv;

  return gtk_css_rb_tree_find (priv->tokens, NULL, NULL, compare_chunk_to_offset, &offset);
}

static gboolean
query_tooltip_cb (GtkWidget             *widget,
                  gint                   x,
                  gint                   y,
                  gboolean               keyboard_tip,
                  GtkTooltip            *tooltip,
                  GtkInspectorCssEditor *ce)
{
  GtkCssChunk *chunk;
  GString *s;

  if (keyboard_tip)
    {
      gint offset;

      g_object_get (ce->priv->text, "cursor-position", &offset, NULL);
      chunk = find_chunk_for_offset (ce, offset);
    }
  else
    {
      GtkTextIter iter;
      gint bx, by;

      gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (ce->priv->view), GTK_TEXT_WINDOW_TEXT,
                                             x, y, &bx, &by);
      if (!gtk_text_view_get_iter_at_position (GTK_TEXT_VIEW (ce->priv->view), &iter, NULL, bx, by))
        return FALSE;

      chunk = find_chunk_for_offset (ce, gtk_text_iter_get_offset (&iter));
    }

  if (!chunk)
    return FALSE;

  s = g_string_new (NULL);

  gtk_css_token_print (&chunk->token, s);
  g_string_append (s, "\n");
  g_string_append (s, G_OBJECT_TYPE_NAME (chunk->consumer));

  if (chunk->error)
    {
      g_string_append (s, "\n");
      g_string_append (s, chunk->error->message);
    }

  gtk_tooltip_set_text (tooltip, s->str);
  g_string_free (s, TRUE);

  return TRUE;
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
update_style_sheet (GtkInspectorCssEditor *ce)
{
  GtkInspectorCssEditorPrivate *priv = ce->priv;
  GtkCssTokenSource *source;

  source = gtk_css_chunk_token_source_new (priv->tokens);
  g_object_unref (priv->style_sheet);
  priv->style_sheet = gtk_css_style_sheet_new ();
  gtk_css_style_sheet_parse (priv->style_sheet, source);

  gtk_css_token_source_unref (source);
}

static gboolean
apply_comment (GtkInspectorCssEditor *ce,
               GtkCssChunk           *chunk)
{
  return chunk->token.type == GTK_CSS_TOKEN_COMMENT;
}

static gboolean
apply_string (GtkInspectorCssEditor *ce,
               GtkCssChunk           *chunk)
{
  return chunk->token.type == GTK_CSS_TOKEN_STRING
      || chunk->token.type == GTK_CSS_TOKEN_BAD_STRING;
}

static gboolean
apply_declaration (GtkInspectorCssEditor *ce,
                   GtkCssChunk           *chunk)
{
  GtkCssChunk *prev;

  if (chunk->token.type != GTK_CSS_TOKEN_IDENT ||
      !GTK_IS_CSS_DECLARATION (chunk->consumer))
    return FALSE;

  prev = gtk_css_rb_tree_get_previous (ce->priv->tokens, chunk);
  if (prev->consumer == chunk->consumer)
    return FALSE;

  return TRUE;
}

static gboolean
apply_selector (GtkInspectorCssEditor *ce,
                GtkCssChunk           *chunk)
{
  GtkCssChunk *prev;

  if (!GTK_IS_CSS_STYLE_RULE (chunk->consumer))
    return FALSE;

  for (prev = chunk;
       prev->consumer == chunk->consumer;
       prev = gtk_css_rb_tree_get_previous (ce->priv->tokens, prev))
    {
      if (gtk_css_token_is (&chunk->token, GTK_CSS_TOKEN_OPEN_CURLY))
        return FALSE;
    }

  return TRUE;
}

static gboolean
apply_selector_matching (GtkInspectorCssEditor *ce,
                         GtkCssChunk           *chunk)
{
  GtkCssMatcher matcher;
  GtkCssStyleRule *rule;
  gsize i;

  if (ce->priv->node == NULL)
    return FALSE;

  if (!apply_selector (ce, chunk))
    return FALSE;

  rule = GTK_CSS_STYLE_RULE (chunk->consumer);
  gtk_css_node_init_matcher (ce->priv->node, &matcher);

  for (i = 0; i < gtk_css_style_rule_get_n_selectors (rule); i++)
    {
      GtkCssSelector *selector = gtk_css_style_rule_get_selector (rule, i);

      if (_gtk_css_selector_matches (selector, &matcher))
        return TRUE;
    }

  return FALSE;
}

static gboolean
apply_error (GtkInspectorCssEditor *ce,
             GtkCssChunk           *chunk)
{
  return chunk->error
      && !g_error_matches (chunk->error, GTK_CSS_PROVIDER_ERROR, GTK_CSS_PROVIDER_ERROR_DEPRECATED);
}

static gboolean
apply_warning (GtkInspectorCssEditor *ce,
               GtkCssChunk           *chunk)
{
  return chunk->error
      && g_error_matches (chunk->error, GTK_CSS_PROVIDER_ERROR, GTK_CSS_PROVIDER_ERROR_DEPRECATED);
}

static struct {
  const char *tag_name;
  gboolean (* apply_func) (GtkInspectorCssEditor *ce,
                           GtkCssChunk           *chunk);
} tag_list[] = {
  { "comment", apply_comment },
  { "string", apply_string },
  { "declaration", apply_declaration },
  { "selector", apply_selector },
  { "selector-matching", apply_selector_matching },
  { "declaration", apply_declaration },
  { "error", apply_error },
  { "warning", apply_warning },
};

static void
update_token_tags (GtkInspectorCssEditor *ce,
                   GtkCssChunk           *start,
                   GtkCssChunk           *end)
{
  GtkInspectorCssEditorPrivate *priv = ce->priv;
  GtkCssChunk *chunk;
  guint i;

  for (chunk = start;
       chunk != end;
       chunk = gtk_css_rb_tree_get_next (priv->tokens, chunk))
    {
      GtkTextIter start, end;

      gtk_css_chunk_get_iters (ce, chunk, &start, &end);

      for (i = 0; i < G_N_ELEMENTS (tag_list); i++)
        {
          if (!tag_list[i].apply_func (ce, chunk))
            continue;

          gtk_text_buffer_apply_tag_by_name (priv->text, tag_list[i].tag_name, &start, &end);
        }
    }
}

static void
tokenizer_error (GtkCssTokenizer   *tokenizer,
                 const GtkCssToken *token,
                 const GError      *error,
                 gpointer           unused)
{
  GtkCssChunk *chunk = (GtkCssChunk *) token;

  if (chunk->error)
    g_warning ("figure out what to do with two errors on the same token");
  else
    chunk->error = g_error_copy (error);
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
  tokenizer = gtk_css_tokenizer_new (gbytes, tokenizer_error, ce, NULL);
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
}

static void
update_style (GtkInspectorCssEditor *ce)
{
  gchar *text;

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
  update_style_sheet (ce);
  update_token_tags (ce, gtk_css_rb_tree_get_first (ce->priv->tokens), NULL);

  return G_SOURCE_REMOVE;
}

static void
text_changed (GtkTextBuffer         *buffer,
              GtkInspectorCssEditor *ce)
{
  if (ce->priv->timeout != 0)
    g_source_remove (ce->priv->timeout);

  ce->priv->timeout = g_timeout_add (100, update_timeout, ce); 
}

static void
create_provider (GtkInspectorCssEditor *ce)
{
  ce->priv->provider = gtk_css_provider_new ();
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (ce->priv->provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_USER);
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
  ce->priv->style_sheet = gtk_css_style_sheet_new ();
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

  gtk_inspector_css_editor_set_node (ce, NULL);

  if (ce->priv->tokens)
    gtk_css_rb_tree_unref (ce->priv->tokens);

  gtk_inspector_css_editor_set_node (ce, NULL);

  g_object_unref (ce->priv->style_sheet);

  destroy_provider (ce);

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

static void
gtk_inspector_css_editor_node_changed_cb (GtkCssNode            *node,
                                          GtkCssStyleChange     *change,
                                          GtkInspectorCssEditor *ce)
{
  clear_all_tags (ce);
  update_token_tags (ce, gtk_css_rb_tree_get_first (ce->priv->tokens), NULL);
}

void
gtk_inspector_css_editor_set_node (GtkInspectorCssEditor *ce,
                                   GtkCssNode            *node)
{
  GtkInspectorCssEditorPrivate *priv;

  g_return_if_fail (GTK_INSPECTOR_IS_CSS_EDITOR (ce));
  g_return_if_fail (node == NULL || GTK_IS_CSS_NODE (node));

  priv = ce->priv;

  if (priv->node == node)
    return;

  if (priv->node)
    {
      g_signal_handlers_disconnect_by_func (priv->node, gtk_inspector_css_editor_node_changed_cb, ce);
    }

  priv->node = node;

  if (node)
    {
      g_object_ref (node);
      g_signal_connect (priv->node,
                        "style-changed",
                        G_CALLBACK (gtk_inspector_css_editor_node_changed_cb),
                        ce);
    }

  clear_all_tags (ce);
  update_token_tags (ce, gtk_css_rb_tree_get_first (ce->priv->tokens), NULL);
}

// vim: set et sw=2 ts=2:
