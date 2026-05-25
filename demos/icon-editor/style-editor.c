/*
 * Copyright © 2026 Red Hat, Inc
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "style-editor.h"
#include "gtk/svg/gtksvgelementprivate.h"

struct _StyleEditor
{
  GtkWidget parent_instance;

  GtkLabel *title;
  GtkTextBuffer *content;
  char *style;

  guint timeout;
};

enum
{
  PROP_STYLE = 1,
  NUM_PROPERTIES,
};

static GParamSpec *props[NUM_PROPERTIES];

static char *
get_current_text (GtkTextBuffer *buffer)
{
  GtkTextIter start, end;

  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);
  gtk_text_buffer_remove_all_tags (buffer, &start, &end);

  return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

static void
update_timeout (gpointer data)
{
  StyleEditor *self = data;
  self->timeout = 0;
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STYLE]);
}

static void
text_changed (StyleEditor *self)
{
  if (self->timeout != 0)
    g_source_remove (self->timeout);

  self->timeout = g_timeout_add_once (100, update_timeout, self);
}

/* {{{ GObject  b oilerplate */

struct _StyleEditorClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (StyleEditor, style_editor, GTK_TYPE_WIDGET)

static void
style_editor_init (StyleEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
style_editor_dispose (GObject *object)
{
  StyleEditor *self = STYLE_EDITOR (object);

  gtk_widget_dispose_template (GTK_WIDGET (object), style_editor_get_type ());

  if (self->timeout != 0)
    g_source_remove (self->timeout);

  G_OBJECT_CLASS (style_editor_parent_class)->dispose (object);
}

static void
style_editor_finalize (GObject *object)
{
  StyleEditor *self = STYLE_EDITOR (object);

  g_free (self->style);

  G_OBJECT_CLASS (style_editor_parent_class)->finalize (object);
}

static void
style_editor_get_property (GObject      *object,
                           unsigned int  prop_id,
                           GValue       *value,
                           GParamSpec   *pspec)
{
  StyleEditor *self = STYLE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_STYLE:
      g_value_set_string (value, style_editor_get_style (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
style_editor_set_property (GObject      *object,
                           unsigned int  prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  StyleEditor *self = STYLE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_STYLE:
      style_editor_set_style (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
style_editor_class_init (StyleEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = style_editor_dispose;
  object_class->finalize = style_editor_finalize;
  object_class->get_property = style_editor_get_property;
  object_class->set_property = style_editor_set_property;

  props[PROP_STYLE] = g_param_spec_string ("style", NULL, NULL,
                                           NULL,
                                           G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/Shaper/style-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, StyleEditor, title);
  gtk_widget_class_bind_template_child (widget_class, StyleEditor, content);

  gtk_widget_class_bind_template_callback (widget_class, text_changed);

  gtk_widget_class_set_css_name (widget_class, "StyleEditor");
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
}

 /* }}} */
/* {{{ Public API */

StyleEditor *
style_editor_new (void)
{
  return g_object_new (style_editor_get_type (), NULL);
}

void
style_editor_set_style (StyleEditor *self,
                        const char  *style)
{
  g_return_if_fail (STYLE_IS_EDITOR (self));
  g_return_if_fail (style != NULL);

  gtk_text_buffer_set_text (self->content, style, strlen (style));
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_STYLE]);
}

const char *
style_editor_get_style (StyleEditor *self)
{
  g_autofree char *style = get_current_text (self->content);

  g_return_val_if_fail (STYLE_IS_EDITOR (self), NULL);

  g_set_str (&self->style, style);

  return self->style;
}

/* }}} */
/* vim:set foldmethod=marker: */
