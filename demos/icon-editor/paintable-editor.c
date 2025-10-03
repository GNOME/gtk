/*
 * Copyright © 2025 Red Hat, Inc
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

#include "paintable-editor.h"
#include "path-editor.h"
#include "path-paintable.h"

static void size_changed (PaintableEditor *self);
static void paintable_editor_set_initial_state (PaintableEditor *self,
                                                unsigned int     state);

struct _PaintableEditor
{
  GtkWidget parent_instance;

  PathPaintable *paintable;
  unsigned int state;

  GtkScrolledWindow *swin;
  GtkEntry *keywords;
  GtkEntry *width;
  GtkEntry *height;
  GtkLabel *compat;
  GtkLabel *summary1;
  GtkLabel *summary2;
  GtkSpinButton *initial_state;

  GtkBox *path_elts;
};

struct _PaintableEditorClass
{
  GtkWidgetClass parent_class;
};

enum
{
  PROP_PAINTABLE = 1,
  PROP_INITIAL_STATE,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE (PaintableEditor, paintable_editor, GTK_TYPE_WIDGET)

/* {{{ Utilities, callbacks */

static void
clear_path_editors (PaintableEditor *self)
{
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->path_elts))) != NULL)
    gtk_box_remove (self->path_elts, child);
}

static PathEditor *
append_path_editor (PaintableEditor *self,
                    gsize           idx)
{
  PathEditor *pe;

  pe = path_editor_new (self->paintable, idx);
  gtk_box_append (self->path_elts, GTK_WIDGET (pe));
  gtk_box_append (self->path_elts, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  return pe;
}

static void
create_path_editors (PaintableEditor *self)
{
  gtk_box_append (self->path_elts, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
  for (size_t i = 0; i < path_paintable_get_n_paths (self->paintable); i++)
    append_path_editor (self, i);
}

static void
update_size (PaintableEditor *self)
{
  g_autofree char *text = NULL;

  text = g_strdup_printf ("%g", path_paintable_get_width (self->paintable));
  gtk_editable_set_text (GTK_EDITABLE (self->width), text);
  g_set_str (&text, g_strdup_printf ("%g", path_paintable_get_height (self->paintable)));
  gtk_editable_set_text (GTK_EDITABLE (self->height), text);
}

static void
update_summary (PaintableEditor *self)
{
  if (self->paintable)
    {
      unsigned int state = path_paintable_get_state (self->paintable);
      g_autofree char *summary1 = NULL;
      g_autofree char *summary2 = NULL;
      unsigned int n;


      n = 0;
      for (size_t i = 0; i < path_paintable_get_n_paths (self->paintable); i++)
        {
          uint64_t states = path_paintable_get_path_states (self->paintable, i);
          if (states & (1ul << state))
            n++;
        }

      if (state == STATE_UNSET)
        {
          summary1 = g_strdup ("Current state: -1");
          summary2 = g_strdup_printf ("%lu path elements", path_paintable_get_n_paths (self->paintable));
        }
      else
        {
          summary1 = g_strdup_printf ("Current state: %u", state);
          summary2 = g_strdup_printf ("%lu path elements, %u in current state", path_paintable_get_n_paths (self->paintable), n);
        }

      gtk_label_set_label (self->summary1, summary1);
      gtk_label_set_label (self->summary2, summary2);
    }
  else
    {
      gtk_label_set_label (self->summary1, "");
      gtk_label_set_label (self->summary2, "");
    }
}

static void
update_compat (PaintableEditor *self)
{
  switch (path_paintable_get_compatibility (self->paintable))
    {
    case GTK_4_0:
      gtk_label_set_label (self->compat, "GTK 4.0");
      break;
    case GTK_4_20:
      gtk_label_set_label (self->compat, "GTK 4.20");
      break;
    case GTK_4_22:
      gtk_label_set_label (self->compat, "GTK 4.22");
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
paths_changed (PaintableEditor *self)
{
  clear_path_editors (self);
  create_path_editors (self);
  update_size (self);
  update_summary (self);
}

static void
changed (PaintableEditor *self)
{
  update_compat (self);
}

static void
size_changed (PaintableEditor *self)
{
  const char *text;
  double width, height;
  int res;

  text = gtk_editable_get_text (GTK_EDITABLE (self->width));
  res = sscanf (text, "%lf", &width);
  text = gtk_editable_get_text (GTK_EDITABLE (self->height));
  res += sscanf (text, "%lf", &height);
  if (res == 2 && width > 0 && height > 0)
    path_paintable_set_size (self->paintable, width, height);
}

static void
keywords_changed (PaintableEditor *self)
{
  const char *text;
  g_auto (GStrv) keywords = NULL;

  text = gtk_editable_get_text (GTK_EDITABLE (self->keywords));
  keywords = g_strsplit (text, " ", 0);
  path_paintable_set_keywords (self->paintable, keywords);
}

/* }}} */
/* {{{ GObject boilerplate */

static void
paintable_editor_init (PaintableEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
paintable_editor_set_property (GObject      *object,
                               unsigned int  prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  PaintableEditor *self = PAINTABLE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      paintable_editor_set_paintable (self, g_value_get_object (value));
      break;

    case PROP_INITIAL_STATE:
      paintable_editor_set_initial_state (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
paintable_editor_get_property (GObject      *object,
                               unsigned int  prop_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  PaintableEditor *self = PAINTABLE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      g_value_set_object (value, self->paintable);
      break;

    case PROP_INITIAL_STATE:
      g_value_set_uint (value, self->state);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
paintable_editor_dispose (GObject *object)
{
  PaintableEditor *self = PAINTABLE_EDITOR (object);

  if (self->paintable)
    g_signal_handlers_disconnect_by_func (self->paintable, paths_changed, self);

  g_clear_object (&self->paintable);

  clear_path_editors (self);

  gtk_widget_dispose_template (GTK_WIDGET (object), PAINTABLE_EDITOR_TYPE);

  G_OBJECT_CLASS (paintable_editor_parent_class)->dispose (object);
}

static void
paintable_editor_finalize (GObject *object)
{
  G_OBJECT_CLASS (paintable_editor_parent_class)->finalize (object);
}

static void
paintable_editor_class_init (PaintableEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  g_type_ensure (PATH_EDITOR_TYPE);

  object_class->dispose = paintable_editor_dispose;
  object_class->finalize = paintable_editor_finalize;
  object_class->set_property = paintable_editor_set_property;
  object_class->get_property = paintable_editor_get_property;

  properties[PROP_PAINTABLE] =
    g_param_spec_object ("paintable", NULL, NULL,
                        PATH_PAINTABLE_TYPE,
                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_INITIAL_STATE] =
    g_param_spec_uint ("initial-state", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/Shaper/paintable-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, swin);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, keywords);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, width);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, height);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, compat);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, summary1);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, summary2);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, initial_state);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, path_elts);

  gtk_widget_class_bind_template_callback (widget_class, size_changed);
  gtk_widget_class_bind_template_callback (widget_class, keywords_changed);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, "PaintableEditor");
}

/* }}} */
/* {{{ Public API */

PaintableEditor *
paintable_editor_new (void)
{
  return g_object_new (PAINTABLE_EDITOR_TYPE, NULL);
}

PathPaintable *
paintable_editor_get_paintable (PaintableEditor *self)
{
  g_return_val_if_fail (PAINTABLE_IS_EDITOR (self), NULL);

  return self->paintable;
}

void
paintable_editor_set_paintable (PaintableEditor *self,
                                PathPaintable   *paintable)
{
  g_return_if_fail (PAINTABLE_IS_EDITOR (self));

  if (self->paintable == paintable)
    return;

  if (self->paintable)
    {
      g_signal_handlers_disconnect_by_func (self->paintable, paths_changed, self);
      g_signal_handlers_disconnect_by_func (self->paintable, changed, self);
      g_signal_handlers_disconnect_by_func (self->paintable, update_summary, self);
    }

  clear_path_editors (self);

  g_set_object (&self->paintable, paintable);

  if (paintable)
    {
      g_autofree char *width = NULL;
      g_autofree char *height = NULL;

      if (path_paintable_get_keywords (paintable))
        {
          g_autofree char *keywords = NULL;

          keywords = g_strjoinv (" ", path_paintable_get_keywords (paintable));
          gtk_editable_set_text (GTK_EDITABLE (self->keywords), keywords);
        }
      else
        {
          gtk_editable_set_text (GTK_EDITABLE (self->keywords), "");
        }

      width = g_strdup_printf ("%g", path_paintable_get_width (paintable));
      gtk_editable_set_text (GTK_EDITABLE (self->width), width);
      height = g_strdup_printf ("%g", path_paintable_get_height (paintable));
      gtk_editable_set_text (GTK_EDITABLE (self->height), height);

      g_signal_connect_swapped (paintable, "paths-changed",
                                G_CALLBACK (paths_changed), self);
      g_signal_connect_swapped (paintable, "changed",
                                G_CALLBACK (changed), self);
      g_signal_connect_swapped (paintable, "notify::state",
                                G_CALLBACK (update_summary), self);

      create_path_editors (self);
      update_summary (self);
      update_compat (self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAINTABLE]);
}

static void
paintable_editor_set_initial_state (PaintableEditor *self,
                                    unsigned int     state)
{
  g_return_if_fail (PAINTABLE_IS_EDITOR (self));

  if (self->state == state)
    return;

  self->state = state;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_INITIAL_STATE]);
}

void
paintable_editor_add_path (PaintableEditor *self)
{
  g_autoptr (GskPath) path = NULL;
  char buffer[128];
  PathEditor *pe;
  float shape_params[6] = { 0, };

  if (path_paintable_get_n_paths (self->paintable) == 0)
    path_paintable_set_size (self->paintable, 100.f, 100.f);

  g_snprintf (buffer, sizeof (buffer), "M 0 0 L ");
  g_ascii_formatd (buffer + strlen (buffer), sizeof (buffer) - strlen (buffer),
                   "%g ",
                   path_paintable_get_width (self->paintable));
  g_ascii_formatd (buffer + strlen (buffer), sizeof (buffer) - strlen (buffer),
                   "%g ",
                   path_paintable_get_width (self->paintable));
  path = gsk_path_parse (buffer);
  g_signal_handlers_block_by_func (self->paintable, paths_changed, self);
  path_paintable_add_path (self->paintable, path, SHAPE_PATH, shape_params);
  pe = append_path_editor (self, path_paintable_get_n_paths (self->paintable) - 1);
  g_signal_handlers_unblock_by_func (self->paintable, paths_changed, self);

  path_editor_edit_path (pe);
}

/* }}} */

/* vim:set foldmethod=marker: */
