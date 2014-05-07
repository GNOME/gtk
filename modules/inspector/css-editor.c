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
  GtkStyleContext *selected_context;
  GtkToggleToolButton *disable_button;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorCssEditor, gtk_inspector_css_editor, GTK_TYPE_BOX)

static void
set_initial_text (GtkInspectorCssEditor *editor)
{
  const gchar *text = NULL;

  if (editor->priv->selected_context)
    text = g_object_get_data (G_OBJECT (editor->priv->selected_context), GTK_INSPECTOR_CSS_EDITOR_TEXT);
  if (text)
    gtk_text_buffer_set_text (GTK_TEXT_BUFFER (editor->priv->text), text, -1);
  else
    {
      gchar *initial_text;
      if (editor->priv->global)
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
      gtk_text_buffer_set_text (GTK_TEXT_BUFFER (editor->priv->text), initial_text, -1);
      g_free (initial_text);
    }
}

static void
disable_toggled (GtkToggleToolButton   *button,
                 GtkInspectorCssEditor *editor)
{
  if (gtk_toggle_tool_button_get_active (button))
    {
      if (editor->priv->global)
        gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (),
                                                      GTK_STYLE_PROVIDER (editor->priv->provider));
      else if (editor->priv->selected_context)
        gtk_style_context_remove_provider (editor->priv->selected_context,
                                           GTK_STYLE_PROVIDER (g_object_get_data (G_OBJECT (editor->priv->selected_context), GTK_INSPECTOR_CSS_EDITOR_PROVIDER)));
    }
  else
    {
      if (editor->priv->global)
        gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                   GTK_STYLE_PROVIDER (editor->priv->provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_USER);
      else if (editor->priv->selected_context)
        gtk_style_context_add_provider (editor->priv->selected_context,
                                        GTK_STYLE_PROVIDER (g_object_get_data (G_OBJECT (editor->priv->selected_context), GTK_INSPECTOR_CSS_EDITOR_PROVIDER)),
                                        G_MAXUINT);
    }
}

static void
apply_system_font (GtkInspectorCssEditor *editor)
{
  GSettings *s = g_settings_new ("org.gnome.desktop.interface");
  gchar *font_name = g_settings_get_string (s, "monospace-font-name");
  PangoFontDescription *font_desc = pango_font_description_from_string (font_name);

  gtk_widget_override_font (editor->priv->view, font_desc);

  pango_font_description_free (font_desc);
  g_free (font_name);
  g_object_unref (s);
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
text_changed (GtkTextBuffer         *buffer,
              GtkInspectorCssEditor *editor)
{
  GtkCssProvider *provider;
  gchar *text;

  if (editor->priv->global)
    provider = editor->priv->provider;
  else if (editor->priv->selected_context)
    provider = g_object_get_data (G_OBJECT (editor->priv->selected_context), GTK_INSPECTOR_CSS_EDITOR_PROVIDER);
  else
    return;

  text = get_current_text (buffer);
  gtk_css_provider_load_from_data (provider, text, -1, NULL);
  g_free (text);

  gtk_style_context_reset_widgets (gdk_screen_get_default ());
}

static void
show_parsing_error (GtkCssProvider        *provider,
                    GtkCssSection         *section,
                    const GError          *error,
                    GtkInspectorCssEditor *editor)
{
  GtkTextIter start, end;
  const char *tag_name;
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (editor->priv->text);

  gtk_text_buffer_get_iter_at_line_index (buffer,
                                          &start,
                                          gtk_css_section_get_start_line (section),
                                          gtk_css_section_get_start_position (section));
  gtk_text_buffer_get_iter_at_line_index (buffer,
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
create_provider (GtkInspectorCssEditor *editor)
{
  GtkCssProvider *provider = gtk_css_provider_new ();

  if (editor->priv->global)
    {
      editor->priv->provider = provider;
      gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                 GTK_STYLE_PROVIDER (editor->priv->provider),
                                                 GTK_STYLE_PROVIDER_PRIORITY_USER);
    }
  else if (editor->priv->selected_context)
    {
      gtk_style_context_add_provider (editor->priv->selected_context,
                                      GTK_STYLE_PROVIDER (provider),
                                      G_MAXUINT);
      g_object_set_data (G_OBJECT (editor->priv->selected_context),
                         GTK_INSPECTOR_CSS_EDITOR_PROVIDER, provider);
    }

  g_signal_connect (provider, "parsing-error",
                    G_CALLBACK (show_parsing_error), editor);
}

static void
gtk_inspector_css_editor_init (GtkInspectorCssEditor *editor)
{
  editor->priv = gtk_inspector_css_editor_get_instance_private (editor);
  gtk_widget_init_template (GTK_WIDGET (editor));
}

static void
constructed (GObject *object)
{
  GtkInspectorCssEditor *editor = GTK_INSPECTOR_CSS_EDITOR (object);

  gtk_widget_set_sensitive (GTK_WIDGET (editor), editor->priv->global);
  create_provider (editor);
  apply_system_font (editor);
  set_initial_text (editor);
}

static void
get_property (GObject    *object,
              guint       param_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GtkInspectorCssEditor *editor = GTK_INSPECTOR_CSS_EDITOR (object);

  switch (param_id)
    {
      case PROP_GLOBAL:
        g_value_set_boolean (value, editor->priv->global);
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
  GtkInspectorCssEditor *editor = GTK_INSPECTOR_CSS_EDITOR (object);

  switch (param_id)
    {
      case PROP_GLOBAL:
        editor->priv->global = g_value_get_boolean (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
        break;
    }
}

static void
gtk_inspector_css_editor_class_init (GtkInspectorCssEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = get_property;
  object_class->set_property = set_property;
  object_class->constructed = constructed;

  g_object_class_install_property (object_class, PROP_GLOBAL,
      g_param_spec_boolean ("global", "Global", "Whether this editor changes the whole application or just the selected widget",
                            TRUE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/css-editor.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssEditor, toolbar);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssEditor, text);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssEditor, view);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssEditor, disable_button);
  gtk_widget_class_bind_template_callback (widget_class, disable_toggled);
  gtk_widget_class_bind_template_callback (widget_class, text_changed);
}

GtkWidget *
gtk_inspector_css_editor_new (gboolean global)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_INSPECTOR_CSS_EDITOR,
                                   "global", global,
                                   NULL));
}

void
gtk_inspector_css_editor_set_widget (GtkInspectorCssEditor *editor,
                                     GtkWidget             *widget)
{
  gchar *text;
  GtkCssProvider *provider;

  g_return_if_fail (GTK_INSPECTOR_IS_CSS_EDITOR (editor));
  g_return_if_fail (!editor->priv->global);

  gtk_widget_set_sensitive (GTK_WIDGET (editor), TRUE);

  if (editor->priv->selected_context)
    {
      text = get_current_text (GTK_TEXT_BUFFER (editor->priv->text));
      g_object_set_data_full (G_OBJECT (editor->priv->selected_context),
                              GTK_INSPECTOR_CSS_EDITOR_TEXT,
                              text,
                              g_free);
    }

  editor->priv->selected_context = gtk_widget_get_style_context (widget);

  provider = g_object_get_data (G_OBJECT (editor->priv->selected_context), GTK_INSPECTOR_CSS_EDITOR_PROVIDER);
  if (!provider)
    create_provider (editor);

  set_initial_text (editor);
  disable_toggled (editor->priv->disable_button, editor);
}

// vim: set et sw=2 ts=2:
