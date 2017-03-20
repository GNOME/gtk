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
#include "gtkstyleprovider.h"
#include "gtkstylecontext.h"
#include "gtktextview.h"
#include "gtkmessagedialog.h"
#include "gtkfilechooserdialog.h"
#include "gtktogglebutton.h"
#include "gtklabel.h"
#include "gtktooltip.h"
#include "gtktextiter.h"


struct _GtkInspectorCssEditorPrivate
{
  GtkWidget *view;
  GtkTextBuffer *text;
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
  gchar *initial_text;
  initial_text = g_strconcat ("/*\n",
                              _("You can type here any CSS rule recognized by GTK+."), "\n",
                              _("You can temporarily disable this custom CSS by clicking on the “Pause” button above."), "\n\n",
                              _("Changes are applied instantly and globally, for the whole application."), "\n",
                              "*/\n\n", NULL);
  gtk_text_buffer_set_text (GTK_TEXT_BUFFER (ce->priv->text), initial_text, -1);
  g_free (initial_text);
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
  gtk_text_buffer_remove_all_tags (buffer, &start, &end);

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
update_style (GtkInspectorCssEditor *ce)
{
  gchar *text;

  g_list_free_full (ce->priv->errors, css_error_free);
  ce->priv->errors = NULL;

  text = get_current_text (ce->priv->text);
  gtk_css_provider_load_from_data (ce->priv->provider, text, -1, NULL);
  g_free (text);
}

static gboolean
update_timeout (gpointer data)
{
  GtkInspectorCssEditor *ce = data;

  ce->priv->timeout = 0;

  update_style (ce);

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
