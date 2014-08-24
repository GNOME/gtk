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
#include "gtktoggletoolbutton.h"

#define GTK_INSPECTOR_CSS_EDITOR_TEXT "inspector-css-editor-text"
#define GTK_INSPECTOR_CSS_EDITOR_PROVIDER "inspector-css-editor-provider"

enum
{
  COLUMN_ENABLED,
  COLUMN_NAME,
  COLUMN_USER
};

enum
{
  PROP_0,
  PROP_GLOBAL
};

typedef struct
{
  gboolean enabled;
  gboolean user;
} GtkInspectorCssEditorByContext;

struct _GtkInspectorCssEditorPrivate
{
  GtkWidget *toolbar;
  GtkWidget *view;
  GtkTextBuffer *text;
  GtkCssProvider *provider;
  gboolean global;
  GtkStyleContext *context;
  GtkToggleToolButton *disable_button;
  guint timeout;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorCssEditor, gtk_inspector_css_editor, GTK_TYPE_BOX)

static void
set_initial_text (GtkInspectorCssEditor *ce)
{
  const gchar *text = NULL;

  if (ce->priv->context)
    text = g_object_get_data (G_OBJECT (ce->priv->context), GTK_INSPECTOR_CSS_EDITOR_TEXT);
  if (text)
    gtk_text_buffer_set_text (GTK_TEXT_BUFFER (ce->priv->text), text, -1);
  else
    {
      gchar *initial_text;
      if (ce->priv->global)
        initial_text = g_strconcat ("/*\n",
                                    _("You can type here any CSS rule recognized by GTK+."), "\n",
                                    _("You can temporarily disable this custom CSS by clicking on the \"Pause\" button above."), "\n\n",
                                    _("Changes are applied instantly and globally, for the whole application."), "\n",
                                    "*/\n\n", NULL);
      else
        initial_text = g_strconcat ("/*\n",
                                    _("You can type here any CSS rule recognized by GTK+."), "\n",
                                    _("You can temporarily disable this custom CSS by clicking on the \"Pause\" button above."), "\n\n",
                                    _("Changes are applied instantly, only for this selected widget."), "\n",
                                    "*/\n\n", NULL);
      gtk_text_buffer_set_text (GTK_TEXT_BUFFER (ce->priv->text), initial_text, -1);
      g_free (initial_text);
    }
}

static void
disable_toggled (GtkToggleToolButton   *button,
                 GtkInspectorCssEditor *ce)
{
  if (gtk_toggle_tool_button_get_active (button))
    {
      if (ce->priv->global)
        gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (),
                                                      GTK_STYLE_PROVIDER (ce->priv->provider));
      else if (ce->priv->context)
        gtk_style_context_remove_provider (ce->priv->context,
                                           GTK_STYLE_PROVIDER (g_object_get_data (G_OBJECT (ce->priv->context), GTK_INSPECTOR_CSS_EDITOR_PROVIDER)));
    }
  else
    {
      if (ce->priv->global)
        gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                   GTK_STYLE_PROVIDER (ce->priv->provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_USER);
      else if (ce->priv->context)
        gtk_style_context_add_provider (ce->priv->context,
                                        GTK_STYLE_PROVIDER (g_object_get_data (G_OBJECT (ce->priv->context), GTK_INSPECTOR_CSS_EDITOR_PROVIDER)),
                                        G_MAXUINT);
    }
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
save_clicked (GtkToolButton         *button,
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
apply_system_font (GtkInspectorCssEditor *ce)
{
  GSettings *s = g_settings_new ("org.gnome.desktop.interface");
  gchar *font_name = g_settings_get_string (s, "monospace-font-name");
  PangoFontDescription *font_desc = pango_font_description_from_string (font_name);

  gtk_widget_override_font (ce->priv->view, font_desc);

  pango_font_description_free (font_desc);
  g_free (font_name);
  g_object_unref (s);
}

static void
update_style (GtkInspectorCssEditor *ce)
{
  GtkCssProvider *provider;
  gchar *text;

  if (ce->priv->global)
    provider = ce->priv->provider;
  else if (ce->priv->context)
    provider = g_object_get_data (G_OBJECT (ce->priv->context), GTK_INSPECTOR_CSS_EDITOR_PROVIDER);
  else
    return;

  text = get_current_text (ce->priv->text);
  gtk_css_provider_load_from_data (provider, text, -1, NULL);
  g_free (text);

  gtk_style_context_reset_widgets (gdk_screen_get_default ());
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
}

/* Safe version of gtk_text_buffer_get_iter_at_line_index(). */
static void
safe_get_iter_at_line_index (GtkTextBuffer *buffer,
                             GtkTextIter   *iter,
                             gint           line_number,
                             gint           byte_index)
{
  if (line_number >= gtk_text_buffer_get_line_count (buffer))
    {
      gtk_text_buffer_get_end_iter (buffer, iter);
      return;
    }

  gtk_text_buffer_get_iter_at_line (buffer, iter, line_number);

  if (byte_index < gtk_text_iter_get_bytes_in_line (iter))
    gtk_text_iter_set_line_index (iter, byte_index);
  else
    gtk_text_iter_forward_to_line_end (iter);
}

static void
show_parsing_error (GtkCssProvider        *provider,
                    GtkCssSection         *section,
                    const GError          *error,
                    GtkInspectorCssEditor *ce)
{
  GtkTextIter start, end;
  const char *tag_name;
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (ce->priv->text);

  safe_get_iter_at_line_index (buffer,
                               &start,
                               gtk_css_section_get_start_line (section),
                               gtk_css_section_get_start_position (section));
  safe_get_iter_at_line_index (buffer,
                               &end,
                               gtk_css_section_get_end_line (section),
                               gtk_css_section_get_end_position (section));

  if (g_error_matches (error, GTK_CSS_PROVIDER_ERROR, GTK_CSS_PROVIDER_ERROR_DEPRECATED))
    tag_name = "warning";
  else
    tag_name = "error";

  gtk_text_buffer_apply_tag_by_name (buffer, tag_name, &start, &end);
}

static void
create_provider (GtkInspectorCssEditor *ce)
{
  GtkCssProvider *provider = gtk_css_provider_new ();

  if (ce->priv->global)
    {
      ce->priv->provider = provider;
      gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                 GTK_STYLE_PROVIDER (ce->priv->provider),
                                                 GTK_STYLE_PROVIDER_PRIORITY_USER);
    }
  else if (ce->priv->context)
    {
      gtk_style_context_add_provider (ce->priv->context,
                                      GTK_STYLE_PROVIDER (provider),
                                      G_MAXUINT);
      g_object_set_data_full (G_OBJECT (ce->priv->context),
                              GTK_INSPECTOR_CSS_EDITOR_PROVIDER, provider,
                              g_object_unref);
    }

  g_signal_connect (provider, "parsing-error",
                    G_CALLBACK (show_parsing_error), ce);
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
  apply_system_font (ce);
  set_initial_text (ce);
}

static void
get_property (GObject    *object,
              guint       param_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GtkInspectorCssEditor *ce = GTK_INSPECTOR_CSS_EDITOR (object);

  switch (param_id)
    {
      case PROP_GLOBAL:
        g_value_set_boolean (value, ce->priv->global);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
        break;
    }
}

static void
set_property (GObject      *object,
              guint         param_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GtkInspectorCssEditor *ce = GTK_INSPECTOR_CSS_EDITOR (object);

  switch (param_id)
    {
      case PROP_GLOBAL:
        ce->priv->global = g_value_get_boolean (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
        break;
    }
}

static void
finalize (GObject *object)
{
  GtkInspectorCssEditor *ce = GTK_INSPECTOR_CSS_EDITOR (object);

  if (ce->priv->timeout != 0)
    g_source_remove (ce->priv->timeout);

  G_OBJECT_CLASS (gtk_inspector_css_editor_parent_class)->finalize (object);
}

static void
gtk_inspector_css_editor_class_init (GtkInspectorCssEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = get_property;
  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->finalize = finalize;

  g_object_class_install_property (object_class, PROP_GLOBAL,
      g_param_spec_boolean ("global", "Global", "Whether this editor changes the whole application or just the selected widget",
                            TRUE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/css-editor.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssEditor, toolbar);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssEditor, text);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssEditor, view);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssEditor, disable_button);
  gtk_widget_class_bind_template_callback (widget_class, disable_toggled);
  gtk_widget_class_bind_template_callback (widget_class, save_clicked);
  gtk_widget_class_bind_template_callback (widget_class, text_changed);
}

static void
remove_dead_object (gpointer data, GObject *dead_object)
{
  GtkInspectorCssEditor *ce = data;

  ce->priv->context = NULL;
  gtk_widget_hide (GTK_WIDGET (ce));
}

void
gtk_inspector_css_editor_set_object (GtkInspectorCssEditor *ce,
                                     GObject               *object)
{
  gchar *text;
  GtkCssProvider *provider;

  g_return_if_fail (GTK_INSPECTOR_IS_CSS_EDITOR (ce));
  g_return_if_fail (!ce->priv->global);

  if (ce->priv->context)
    {
      g_object_weak_unref (G_OBJECT (ce->priv->context), remove_dead_object, ce);
      text = get_current_text (GTK_TEXT_BUFFER (ce->priv->text));
      g_object_set_data_full (G_OBJECT (ce->priv->context),
                              GTK_INSPECTOR_CSS_EDITOR_TEXT,
                              text, g_free);
      ce->priv->context = NULL;
    }

  if (!GTK_IS_WIDGET (object))
    {
      gtk_widget_hide (GTK_WIDGET (ce));
      return;
    }

  gtk_widget_show (GTK_WIDGET (ce));

  ce->priv->context = gtk_widget_get_style_context (GTK_WIDGET (object));

  provider = g_object_get_data (G_OBJECT (ce->priv->context), GTK_INSPECTOR_CSS_EDITOR_PROVIDER);
  if (!provider)
    create_provider (ce);

  set_initial_text (ce);
  disable_toggled (ce->priv->disable_button, ce);

  g_object_weak_ref (G_OBJECT (ce->priv->context), remove_dead_object, ce);
}

// vim: set et sw=2 ts=2:
