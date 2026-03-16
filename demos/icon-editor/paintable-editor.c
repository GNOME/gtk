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
#include "shape-editor.h"
#include "path-paintable.h"

static void size_changed (PaintableEditor *self);
static void paintable_editor_set_compat_classes (PaintableEditor *self,
                                                 gboolean         compat_classes);

struct _PaintableEditor
{
  GtkWidget parent_instance;

  PathPaintable *paintable;
  unsigned int state;
  gboolean compat_classes;

  GtkScrolledWindow *swin;
  GtkEntry *author;
  GtkEntry *license;
  GtkEntry *description;
  GtkEntry *keywords;
  GtkEntry *width;
  GtkEntry *height;
  GtkEntry *viewbox_x;
  GtkEntry *viewbox_y;
  GtkEntry *viewbox_w;
  GtkEntry *viewbox_h;
  GtkLabel *compat;
  GtkLabel *summary1;
  GtkLabel *summary2;
  GtkImage *icon_image;
  GtkCheckButton *compat_check;

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
  PROP_COMPAT_CLASSES,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE (PaintableEditor, paintable_editor, GTK_TYPE_WIDGET)

/* {{{ Utilities, callbacks */

static void
clear_shape_editors (PaintableEditor *self)
{
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->path_elts))) != NULL)
    gtk_box_remove (self->path_elts, child);
}

static void
append_shape_editor (PaintableEditor *self,
                     Shape           *shape)
{
  ShapeEditor *pe;

  pe = shape_editor_new (self->paintable, shape);
  if (pe)
    {
      gtk_box_append (self->path_elts, GTK_WIDGET (pe));
      gtk_box_append (self->path_elts, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));
    }
}

static void
create_shape_editors (PaintableEditor *self)
{
  Shape *content;

  gtk_box_append (self->path_elts, gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

  content = path_paintable_get_content (self->paintable);
  for (unsigned int i = 0; i < content->shapes->len; i++)
    {
      Shape *shape = g_ptr_array_index (content->shapes, i);
      append_shape_editor (self, shape);
    }
}

static void
update_size (PaintableEditor *self)
{
  GtkSvg *svg = path_paintable_get_svg (self->paintable);
  g_autofree char *text = NULL;

  text = g_strdup_printf ("%g", svg->width);
  gtk_editable_set_text (GTK_EDITABLE (self->width), text);
  g_set_str (&text, g_strdup_printf ("%g", svg->height));
  gtk_editable_set_text (GTK_EDITABLE (self->height), text);
}

static void
update_viewbox (PaintableEditor *self)
{
  GtkSvg *svg = path_paintable_get_svg (self->paintable);
  graphene_rect_t rect;

  if (svg_shape_attr_get_viewbox (svg->content, SHAPE_ATTR_VIEW_BOX, &rect))
    {
      g_autofree char *text = g_strdup_printf ("%g", rect.origin.x);
      gtk_editable_set_text (GTK_EDITABLE (self->viewbox_x), text);
      g_set_str (&text, g_strdup_printf ("%g", rect.origin.y));
      gtk_editable_set_text (GTK_EDITABLE (self->viewbox_y), text);
      g_set_str (&text, g_strdup_printf ("%g", rect.size.width));
      gtk_editable_set_text (GTK_EDITABLE (self->viewbox_w), text);
      g_set_str (&text, g_strdup_printf ("%g", rect.size.height));
      gtk_editable_set_text (GTK_EDITABLE (self->viewbox_h), text);
    }
  else
    {
      gtk_editable_set_text (GTK_EDITABLE (self->viewbox_x), "");
      gtk_editable_set_text (GTK_EDITABLE (self->viewbox_y), "");
      gtk_editable_set_text (GTK_EDITABLE (self->viewbox_w), "");
      gtk_editable_set_text (GTK_EDITABLE (self->viewbox_h), "");
    }
}

typedef struct
{
  unsigned int all;
  unsigned int graphical;
  unsigned int current;

  unsigned int state;
} ShapeCountData;

static void
count_shapes (Shape    *shape,
              gpointer  data)
{
  ShapeCountData *d = data;

  d->all++;

  if (!shape_is_graphical (shape))
    return;

  d->graphical++;

  if ((shape->gpa.states & (G_GUINT64_CONSTANT (1) << d->state)) == 0)
    return;

  d->current++;
}

static void
update_summary (PaintableEditor *self)
{
  if (self->paintable)
    {
      GtkSvg *svg = path_paintable_get_svg (self->paintable);
      unsigned int state, n_names;
      const char **names;
      g_autofree char *summary1 = NULL;
      g_autofree char *summary2 = NULL;
      ShapeCountData counts;

      state = gtk_svg_get_state (svg);
      names = gtk_svg_get_state_names (svg, &n_names);

      counts.state = state;
      counts.all = counts.graphical = counts.current = 0;
      svg_foreach_shape (svg->content, count_shapes, &counts);

      if (state < n_names)
        summary1 = g_strdup_printf ("Current state: %u (%s)", state, names[state]);
      else
        summary1 = g_strdup_printf ("Current state: %u", state);

      summary2 = g_strdup_printf ("%u graphical shapes, %u in current state", counts.graphical, counts.current);

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
update_icon_paintable (PaintableEditor *self)
{
  GdkPaintable *paintable = GDK_PAINTABLE (path_paintable_get_icon_paintable (self->paintable));

  gtk_image_set_from_paintable (self->icon_image, paintable);

  g_object_unref (paintable);
}

static void
paths_changed (PaintableEditor *self)
{
  clear_shape_editors (self);
  create_shape_editors (self);
  update_summary (self);
  update_icon_paintable (self);
}

static void
changed (PaintableEditor *self)
{
  update_compat (self);
  update_size (self);
  update_viewbox (self);
  update_summary (self);
  update_icon_paintable (self);
}

static void
set_size (PaintableEditor *self,
          double           width,
          double           height)
{
  GtkSvg *svg = path_paintable_get_svg (self->paintable);
  graphene_rect_t rect;

  svg->width = width;
  svg->height = height;

  svg_shape_attr_set (svg->content, SHAPE_ATTR_WIDTH, svg_number_new (width));
  svg_shape_attr_set (svg->content, SHAPE_ATTR_HEIGHT, svg_number_new (height));

  if (!svg_shape_attr_get_viewbox (svg->content, SHAPE_ATTR_VIEW_BOX, &rect))
    svg_shape_attr_set (svg->content, SHAPE_ATTR_VIEW_BOX,
                        svg_view_box_new (&GRAPHENE_RECT_INIT (0, 0, width, height)));

  path_paintable_changed (self->paintable);
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self->paintable));
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
    set_size (self, width, height);
}

static void
viewbox_changed (PaintableEditor *self)
{
  GtkSvg *svg = path_paintable_get_svg (self->paintable);
  const char *text;
  double x, y, w, h;
  int res;

  text = gtk_editable_get_text (GTK_EDITABLE (self->viewbox_x));
  res = sscanf (text, "%lf", &x);
  text = gtk_editable_get_text (GTK_EDITABLE (self->viewbox_y));
  res += sscanf (text, "%lf", &y);
  text = gtk_editable_get_text (GTK_EDITABLE (self->viewbox_w));
  res += sscanf (text, "%lf", &w);
  text = gtk_editable_get_text (GTK_EDITABLE (self->viewbox_h));
  res += sscanf (text, "%lf", &h);
  if (res == 4 && w > 0 && h > 0)
    {
      svg_shape_attr_set (svg->content, SHAPE_ATTR_VIEW_BOX,
                          svg_view_box_new (&GRAPHENE_RECT_INIT (x, y, w, h)));

      path_paintable_changed (self->paintable);
      gdk_paintable_invalidate_size (GDK_PAINTABLE (self->paintable));
    }
}

static void
author_changed (PaintableEditor *self)
{
  const char *text = gtk_editable_get_text (GTK_EDITABLE (self->author));
  path_paintable_set_author (self->paintable, text);
}

static void
license_changed (PaintableEditor *self)
{
  const char *text = gtk_editable_get_text (GTK_EDITABLE (self->license));
  path_paintable_set_license (self->paintable, text);
}

static void
description_changed (PaintableEditor *self)
{
  const char *text = gtk_editable_get_text (GTK_EDITABLE (self->description));
  path_paintable_set_description (self->paintable, text);
}

static void
keywords_changed (PaintableEditor *self)
{
  const char *text = gtk_editable_get_text (GTK_EDITABLE (self->keywords));
  path_paintable_set_keywords (self->paintable, text);
}

/* }}} */
/* {{{ GObject boilerplate */

static void
paintable_editor_init (PaintableEditor *self)
{
  self->compat_classes = TRUE;
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

    case PROP_COMPAT_CLASSES:
      paintable_editor_set_compat_classes (self, g_value_get_boolean (value));
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

    case PROP_COMPAT_CLASSES:
      g_value_set_boolean (value, self->compat_classes);
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

  clear_shape_editors (self);

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

  g_type_ensure (SHAPE_EDITOR_TYPE);

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
                       0, 64, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  properties[PROP_COMPAT_CLASSES] =
    g_param_spec_boolean ("compat-classes", NULL, NULL,
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/Shaper/paintable-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, swin);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, author);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, license);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, description);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, keywords);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, width);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, height);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, viewbox_x);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, viewbox_y);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, viewbox_w);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, viewbox_h);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, compat);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, summary1);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, summary2);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, icon_image);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, path_elts);
  gtk_widget_class_bind_template_child (widget_class, PaintableEditor, compat_check);

  gtk_widget_class_bind_template_callback (widget_class, size_changed);
  gtk_widget_class_bind_template_callback (widget_class, viewbox_changed);
  gtk_widget_class_bind_template_callback (widget_class, author_changed);
  gtk_widget_class_bind_template_callback (widget_class, license_changed);
  gtk_widget_class_bind_template_callback (widget_class, description_changed);
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

  clear_shape_editors (self);

  g_set_object (&self->paintable, paintable);

  if (paintable)
    {
      g_signal_connect_swapped (paintable, "paths-changed",
                                G_CALLBACK (paths_changed), self);
      g_signal_connect_swapped (paintable, "changed",
                                G_CALLBACK (changed), self);
      g_signal_connect_swapped (paintable, "notify::state",
                                G_CALLBACK (update_summary), self);

      create_shape_editors (self);
      update_summary (self);
      update_size (self);
      update_viewbox (self);
      update_compat (self);
      update_icon_paintable (self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAINTABLE]);
}

static void
paintable_editor_set_compat_classes (PaintableEditor *self,
                                     gboolean         compat_classes)
{
  g_return_if_fail (PAINTABLE_IS_EDITOR (self));

  if (self->compat_classes == compat_classes)
    return;

  self->compat_classes = compat_classes;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMPAT_CLASSES]);
}

void
paintable_editor_add_path (PaintableEditor *self)
{
  GtkSvg *svg = path_paintable_get_svg (self->paintable);
  GskPathBuilder *builder;
  g_autoptr (GskPath) path = NULL;
  Shape *content;
  size_t idx;
  Shape *shape;
  graphene_rect_t rect;

  if (svg->content->shapes->len == 0 && svg->width == 0 && svg->height == 0)
    set_size (self, 100, 100);

  svg_shape_attr_get_viewbox (svg->content, SHAPE_ATTR_VIEW_BOX, &rect);

  builder = gsk_path_builder_new ();
  gsk_path_builder_move_to (builder, rect.origin.x, rect.origin.y);
  gsk_path_builder_rel_line_to (builder, rect.size.width, rect.size.height);
  path = gsk_path_builder_free_to_path (builder);
  g_signal_handlers_block_by_func (self->paintable, paths_changed, self);
  idx = path_paintable_add_path (self->paintable, path);

  shape = path_paintable_get_shape (self->paintable, idx);
  shape->id = path_paintable_find_unused_id (self->paintable, "path");

  content = path_paintable_get_content (self->paintable);
  append_shape_editor (self, g_ptr_array_index (content->shapes, content->shapes->len - 1));
  g_signal_handlers_unblock_by_func (self->paintable, paths_changed, self);
}

/* }}} */

/* vim:set foldmethod=marker: */
