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
#include "path-paintable-private.h"

static void size_changed (PaintableEditor *self);

struct _PaintableEditor
{
  GtkWidget parent_instance;

  PathPaintable *paintable;

  GtkScrolledWindow *swin;
  GtkEntry *width;
  GtkEntry *height;
  GtkAdjustment *duration_adj;
  GBinding *duration_binding;
  GtkAdjustment *delay_adj;
  GBinding *delay_binding;
  GtkDropDown *easing;
  GBinding *easing_binding;

  GtkBox *path_elts;
};

struct _PaintableEditorClass
{
  GtkWidgetClass parent_class;
};

enum
{
  PROP_PAINTABLE = 1,
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

static void
create_path_editors (PaintableEditor *self)
{
  for (gsize i = 0; i < path_paintable_get_n_paths (self->paintable); i++)
    {
      PathEditor *pe = path_editor_new (self->paintable, i);
      gtk_box_append (self->path_elts, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
      gtk_box_append (self->path_elts, GTK_WIDGET (pe));
    }
  gtk_box_append (self->path_elts, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
}

static void
maybe_update_size (PaintableEditor *self)
{
  guint symbolic;
  GdkRGBA color;
  g_autoptr (GskStroke) stroke = gsk_stroke_new (0);
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 0, 0);
  g_autofree char *text = NULL;

  if (path_paintable_get_width (self->paintable) != 0 &&
      path_paintable_get_height (self->paintable) != 0)
    return;

  for (gsize i = 0; i < path_paintable_get_n_paths (self->paintable); i++)
    {
      GskPath *path = path_paintable_get_path (self->paintable, i);
      graphene_rect_t b;

      path_paintable_get_path_stroke (self->paintable, i, stroke, &symbolic, &color);

      gsk_path_get_stroke_bounds (path, stroke, &b);
      graphene_rect_union (&b, &bounds, &bounds);
    }

  path_paintable_set_size (self->paintable,
                           bounds.origin.x + bounds.size.width,
                           bounds.origin.y + bounds.size.height);

  text = g_strdup_printf ("%g", path_paintable_get_width (self->paintable));
  gtk_editable_set_text (GTK_EDITABLE (self->width), text);
  text = g_strdup_printf ("%g", path_paintable_get_height (self->paintable));
  gtk_editable_set_text (GTK_EDITABLE (self->height), text);
}

static void
paths_changed (PaintableEditor *self)
{
  clear_path_editors (self);
  create_path_editors (self);
  maybe_update_size (self);
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

/* }}} */
/* {{{ GObject boilerplate */

static void
paintable_editor_init (PaintableEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
paintable_editor_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  PaintableEditor *self = PAINTABLE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      paintable_editor_set_paintable (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
paintable_editor_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  PaintableEditor *self = PAINTABLE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      g_value_set_object (value, self->paintable);
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
  g_clear_object (&self->duration_binding);
  g_clear_object (&self->delay_binding);
  g_clear_object (&self->easing_binding);

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

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/icon-editor/paintable-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, swin);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, width);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, height);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, duration_adj);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, delay_adj);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, easing);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, path_elts);

  gtk_widget_class_bind_template_callback (widget_class, size_changed);

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
    g_signal_handlers_disconnect_by_func (self->paintable, paths_changed, self);

  g_clear_object (&self->duration_binding);
  g_clear_object (&self->delay_binding);
  g_clear_object (&self->easing_binding);

  clear_path_editors (self);

  g_set_object (&self->paintable, paintable);

  if (paintable)
    {
      g_autofree char *width = NULL;
      g_autofree char *height = NULL;

      width = g_strdup_printf ("%g", path_paintable_get_width (paintable));
      gtk_editable_set_text (GTK_EDITABLE (self->width), width);
      height = g_strdup_printf ("%g", path_paintable_get_height (paintable));
      gtk_editable_set_text (GTK_EDITABLE (self->height), height);

      self->duration_binding =
        g_object_ref (g_object_bind_property (paintable, "duration",
                                              self->duration_adj, "value",
                                              G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE));
      self->delay_binding =
        g_object_ref (g_object_bind_property (paintable, "delay",
                                              self->delay_adj, "value",
                                              G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE));
      self->easing_binding =
        g_object_ref (g_object_bind_property (paintable, "easing",
                                              self->easing, "selected",
                                              G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE));

      g_signal_connect_swapped (paintable, "paths-changed",
                                G_CALLBACK (paths_changed), self);

      create_path_editors (self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAINTABLE]);
}

/* }}} */

/* vim:set foldmethod=marker: */
