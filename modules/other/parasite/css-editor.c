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

#include "css-editor.h"
#include "parasite.h"

#define PARASITE_CSSEDITOR_TEXT "parasite-csseditor-text"
#define PARASITE_CSSEDITOR_PROVIDER "parasite-csseditor-provider"

enum
{
  COLUMN_ENABLED,
  COLUMN_NAME,
  COLUMN_USER,
  NUM_COLUMNS
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
} ParasiteCssEditorByContext;

struct _ParasiteCssEditorPrivate
{
  GtkWidget *toolbar;
  GtkTextBuffer *text;
  GtkCssProvider *provider;
  gboolean global;
  GtkStyleContext *selected_context;
  GtkToggleToolButton *disable_button;
};

G_DEFINE_TYPE_WITH_PRIVATE (ParasiteCssEditor, parasite_csseditor, GTK_TYPE_BOX)

static const gchar *initial_text_global =
          "/*\n"
          "You can type here any CSS rule recognized by GTK+.\n"
          "You can temporarily disable this custom CSS by clicking on the \"Pause\" button above.\n\n"
          "Changes are applied instantly and globally, for the whole application.\n"
          "*/\n\n";
static const gchar *initial_text_widget =
          "/*\n"
          "You can type here any CSS rule recognized by GTK+.\n"
          "You can temporarily disable this custom CSS by clicking on the \"Pause\" button above.\n\n"
          "Changes are applied instantly, only for this selected widget.\n"
          "*/\n\n";

static void
disable_toggled (GtkToggleToolButton *button, ParasiteCssEditor *editor)
{
  if (gtk_toggle_tool_button_get_active (button))
    {
      if (editor->priv->global)
        {
          gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (),
                                                        GTK_STYLE_PROVIDER (editor->priv->provider));
        }
      else if (editor->priv->selected_context)
        {
          gtk_style_context_remove_provider (editor->priv->selected_context,
                                             GTK_STYLE_PROVIDER (g_object_get_data (G_OBJECT (editor->priv->selected_context), PARASITE_CSSEDITOR_PROVIDER)));
        }
    }
  else
    {
      if (editor->priv->global)
        {
          gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                     GTK_STYLE_PROVIDER (editor->priv->provider),
                                                     GTK_STYLE_PROVIDER_PRIORITY_USER);
        }
      else if (editor->priv->selected_context)
        {
          gtk_style_context_add_provider (editor->priv->selected_context,
                                          GTK_STYLE_PROVIDER (g_object_get_data (G_OBJECT (editor->priv->selected_context), PARASITE_CSSEDITOR_PROVIDER)),
                                          G_MAXUINT);
        }
    }
}

static void
create_toolbar (ParasiteCssEditor *editor)
{
  editor->priv->toolbar = g_object_new (GTK_TYPE_TOOLBAR,
                                        "icon-size", GTK_ICON_SIZE_SMALL_TOOLBAR,
                                        NULL);
  gtk_container_add (GTK_CONTAINER (editor), editor->priv->toolbar);

  editor->priv->disable_button = g_object_new (GTK_TYPE_TOGGLE_TOOL_BUTTON,
                                               "icon-name", "media-playback-pause",
                                               "tooltip-text", "Disable this custom css",
                                               NULL);
  g_signal_connect (editor->priv->disable_button,
                    "toggled",
                    G_CALLBACK (disable_toggled),
                    editor);
  gtk_container_add (GTK_CONTAINER (editor->priv->toolbar),
                     GTK_WIDGET (editor->priv->disable_button));
}

static void
apply_system_font (GtkWidget *widget)
{
  GSettings *s = g_settings_new ("org.gnome.desktop.interface");
  gchar *font_name = g_settings_get_string (s, "monospace-font-name");
  PangoFontDescription *font_desc = pango_font_description_from_string (font_name);

  gtk_widget_override_font (widget, font_desc);

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
text_changed (GtkTextBuffer *buffer, ParasiteCssEditor *editor)
{
  GtkCssProvider *provider;
  char *text;

  if (editor->priv->global)
    provider = editor->priv->provider;
  else if (editor->priv->selected_context)
    provider = g_object_get_data (G_OBJECT (editor->priv->selected_context), PARASITE_CSSEDITOR_PROVIDER);
  else
    return;

  text = get_current_text (buffer);
  gtk_css_provider_load_from_data (provider, text, -1, NULL);
  g_free (text);

  gtk_style_context_reset_widgets (gdk_screen_get_default ());
}

static void
show_parsing_error (GtkCssProvider    *provider,
                    GtkCssSection     *section,
                    const GError      *error,
                    ParasiteCssEditor *editor)
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
create_text_widget (ParasiteCssEditor *editor)
{
  GtkWidget *sw, *view;

  editor->priv->text = gtk_text_buffer_new (NULL);

  if (editor->priv->global)
    gtk_text_buffer_set_text (GTK_TEXT_BUFFER (editor->priv->text), initial_text_global, -1);
  else
    gtk_text_buffer_set_text (GTK_TEXT_BUFFER (editor->priv->text), initial_text_widget, -1);

  g_signal_connect (editor->priv->text, "changed", G_CALLBACK (text_changed), editor);

  gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (editor->priv->text),
                              "warning",
                              "underline", PANGO_UNDERLINE_SINGLE,
                              NULL);
  gtk_text_buffer_create_tag (GTK_TEXT_BUFFER (editor->priv->text),
                              "error",
                              "underline", PANGO_UNDERLINE_ERROR,
                              NULL);

  sw = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                     "expand", TRUE,
                     NULL);
  gtk_container_add (GTK_CONTAINER (editor), sw);

  view = g_object_new (GTK_TYPE_TEXT_VIEW,
                       "buffer", editor->priv->text,
                       "wrap-mode", GTK_WRAP_WORD,
                       NULL);
  apply_system_font (view);
  gtk_container_add (GTK_CONTAINER (sw), view);
}

static void
create_provider (ParasiteCssEditor *editor)
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
                         PARASITE_CSSEDITOR_PROVIDER,
                         provider);
    }

  g_signal_connect (provider,
                    "parsing-error",
                    G_CALLBACK (show_parsing_error),
                    editor);
}

static void
parasite_csseditor_init (ParasiteCssEditor *editor)
{
  editor->priv = parasite_csseditor_get_instance_private (editor);
}

static void
constructed (GObject *object)
{
  ParasiteCssEditor *editor = PARASITE_CSSEDITOR (object);

  g_object_set (editor,
                "orientation", GTK_ORIENTATION_VERTICAL,
                "sensitive", editor->priv->global,
                NULL);

  create_toolbar (editor);
  create_provider (editor);
  create_text_widget (editor);
}

static void
get_property (GObject    *object,
              guint      param_id,
              GValue     *value,
              GParamSpec *pspec)
{
  ParasiteCssEditor *editor = PARASITE_CSSEDITOR (object);

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
              guint        param_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  ParasiteCssEditor *editor = PARASITE_CSSEDITOR (object);

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
parasite_csseditor_class_init (ParasiteCssEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = get_property;
  object_class->set_property = set_property;
  object_class->constructed  = constructed;

  g_object_class_install_property (object_class,
                                   PROP_GLOBAL,
                                   g_param_spec_boolean ("global",
                                                         "Global",
                                                         "Whether this editor changes the whole application or just the selected widget",
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

GtkWidget *
parasite_csseditor_new (gboolean global)
{
    return GTK_WIDGET (g_object_new (PARASITE_TYPE_CSSEDITOR,
                                     "global", global,
                                     NULL));
}

void
parasite_csseditor_set_widget (ParasiteCssEditor *editor, GtkWidget *widget)
{
  gchar *text;
  GtkCssProvider *provider;

  g_return_if_fail (PARASITE_IS_CSSEDITOR (editor));
  g_return_if_fail (!editor->priv->global);

  gtk_widget_set_sensitive (GTK_WIDGET (editor), TRUE);

  if (editor->priv->selected_context)
    {
      text = get_current_text (GTK_TEXT_BUFFER (editor->priv->text));
      g_object_set_data_full (G_OBJECT (editor->priv->selected_context),
                              PARASITE_CSSEDITOR_TEXT,
                              text,
                              g_free);
    }

  editor->priv->selected_context = gtk_widget_get_style_context (widget);

  provider = g_object_get_data (G_OBJECT (editor->priv->selected_context), PARASITE_CSSEDITOR_PROVIDER);
  if (!provider)
    {
      create_provider (editor);
    }

  text = g_object_get_data (G_OBJECT (editor->priv->selected_context), PARASITE_CSSEDITOR_TEXT);
  if (text)
    gtk_text_buffer_set_text (GTK_TEXT_BUFFER (editor->priv->text), text, -1);
  else
    gtk_text_buffer_set_text (GTK_TEXT_BUFFER (editor->priv->text), initial_text_widget, -1);

  disable_toggled (editor->priv->disable_button, editor);
}

// vim: set et sw=4 ts=4:
