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

#include "state-editor.h"
#include "shape-editor.h"
#include "path-paintable.h"

struct _StateEditor
{
  GtkWindow parent_instance;

  GtkGrid *grid;
  PathPaintable *paintable;
  unsigned int max_state;

  gboolean updating;
};

struct _StateEditorClass
{
  GtkWidgetClass parent_class;
};

enum
{
  PROP_PAINTABLE = 1,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE (StateEditor, state_editor, GTK_TYPE_WINDOW)

/* {{{ Utilities, callbacks */

static void repopulate (StateEditor *self);

static GdkPaintable *
get_paintable_for_shape (StateEditor *self,
                         Shape       *shape)
{
  GtkSvg *svg = gtk_svg_new ();
  g_autoptr (GBytes) bytes = NULL;
  Shape *sh;

  svg->width = path_paintable_get_width (self->paintable);
  svg->height = path_paintable_get_height (self->paintable);

  svg_shape_attr_set (svg->content,
                      SHAPE_ATTR_WIDTH,
                      svg_number_new (svg->width));
  svg_shape_attr_set (svg->content,
                      SHAPE_ATTR_HEIGHT,
                      svg_number_new (svg->height));
  svg_shape_attr_set (svg->content,
                      SHAPE_ATTR_VIEW_BOX,
                      svg_view_box_new (&GRAPHENE_RECT_INIT (0, 0, svg->width, svg->height)));

  sh = shape_duplicate (shape);
  svg_shape_attr_set (sh, SHAPE_ATTR_VISIBILITY, NULL);
  svg_shape_attr_set (sh, SHAPE_ATTR_DISPLAY, NULL);
  g_ptr_array_add (svg->content->shapes, sh);

  bytes = gtk_svg_serialize (svg);
  g_object_unref (svg);
  svg = gtk_svg_new_from_bytes (bytes);
  gtk_svg_play (svg);
  return GDK_PAINTABLE (svg);
}

static void
update_states (StateEditor *self)
{
  GtkLayoutManager *mgr = gtk_widget_get_layout_manager (GTK_WIDGET (self->grid));
  uint64_t *states;

  states = g_newa0 (uint64_t, path_paintable_get_n_paths (self->paintable));

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (!GTK_IS_CHECK_BUTTON (child))
        continue;

      if (gtk_check_button_get_active (GTK_CHECK_BUTTON (child)))
        {
          GtkLayoutChild *layout_child;
          int row, col;

          layout_child = gtk_layout_manager_get_layout_child (mgr, child);
          row = gtk_grid_layout_child_get_row (GTK_GRID_LAYOUT_CHILD (layout_child));
          col = gtk_grid_layout_child_get_column (GTK_GRID_LAYOUT_CHILD (layout_child));

          if (col <= self->max_state)
            states[row] |= (G_GUINT64_CONSTANT (1) << (unsigned int) col);
        }
    }

  self->updating = TRUE;

  for (unsigned int i = 0; i < path_paintable_get_n_paths (self->paintable); i++)
    {
      path_paintable_set_path_states (self->paintable, i, states[i]);
    }

  self->updating = FALSE;

  repopulate (self);
}

static void
drop_state (StateEditor *self)
{
  self->max_state--;
  self->max_state = CLAMP (self->max_state, 0, 63);

  update_states (self);
}

static void
add_state (StateEditor *self)
{
  self->max_state++;
  self->max_state = CLAMP (self->max_state, 0, 63);

  update_states (self);
}

static void
clear_paths (StateEditor *self)
{
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->grid))) != NULL)
    gtk_widget_unparent (child);
}

static void
create_paths_for_shape (StateEditor *self,
                        Shape       *shape)
{
  for (unsigned int i = 0; i < shape->shapes->len; i++)
    {
      Shape *sh = g_ptr_array_index (shape->shapes, i);

      if (sh->type == SHAPE_GROUP)
        {
          create_paths_for_shape (self, sh);
          continue;
        }
      else if (shape_is_graphical (sh))
        {
          uint64_t states = sh->gpa.states;
          GdkPaintable *paintable = get_paintable_for_shape (self, sh);
          const char *id = sh->id;
          GtkWidget *child;

          child = gtk_image_new_from_paintable (paintable);
          gtk_image_set_pixel_size (GTK_IMAGE (child), 20);
          g_object_unref (paintable);
          gtk_grid_attach (self->grid, child, -2, i, 1, 1);

          child = gtk_label_new (id);
          gtk_grid_attach (self->grid, child, -1, i, 1, 1);

          for (unsigned int j = 0; j <= self->max_state; j++)
            {
              child = gtk_check_button_new ();
              gtk_check_button_set_active (GTK_CHECK_BUTTON (child),
                                           (states & ((G_GUINT64_CONSTANT (1) << j))) != 0);
              g_signal_connect_swapped (child, "notify::active", G_CALLBACK (update_states), self);
              gtk_grid_attach (self->grid, child, j, i, 1, 1);
            }
        }
    }
}

static void
create_paths (StateEditor *self)
{
  GtkWidget *child;

  for (unsigned int i = 0; i <= self->max_state; i++)
    {
      char *s = g_strdup_printf ("%u", i);
      child = gtk_label_new (s);
      gtk_grid_attach (self->grid, child, i, -1, 1, 1);
      g_free (s);
    }

  create_paths_for_shape (self, path_paintable_get_content (self->paintable));
}

static void
repopulate (StateEditor *self)
{
  if (self->updating)
    return;

  clear_paths (self);
  create_paths (self);
}

static void
paths_changed (StateEditor *self)
{
  self->max_state = MAX (self->max_state, path_paintable_get_n_states (self->paintable) - 1);
  self->max_state = CLAMP (self->max_state, 0, 63);

  repopulate (self);
}

/* }}} */
/* {{{ GObject boilerplate */

static void
state_editor_init (StateEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
state_editor_set_property (GObject      *object,
                           unsigned int  prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  StateEditor *self = STATE_EDITOR (object);

  switch (prop_id)
    {
    case PROP_PAINTABLE:
      state_editor_set_paintable (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
state_editor_get_property (GObject      *object,
                           unsigned int  prop_id,
                           GValue       *value,
                           GParamSpec   *pspec)
{
  StateEditor *self = STATE_EDITOR (object);

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
state_editor_dispose (GObject *object)
{
  StateEditor *self = STATE_EDITOR (object);

  if (self->paintable)
    g_signal_handlers_disconnect_by_func (self->paintable, paths_changed, self);

  g_clear_object (&self->paintable);

  gtk_widget_dispose_template (GTK_WIDGET (object), STATE_EDITOR_TYPE);

  G_OBJECT_CLASS (state_editor_parent_class)->dispose (object);
}

static void
state_editor_finalize (GObject *object)
{
  G_OBJECT_CLASS (state_editor_parent_class)->finalize (object);
}

static void
state_editor_class_init (StateEditorClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  g_type_ensure (SHAPE_EDITOR_TYPE);

  object_class->dispose = state_editor_dispose;
  object_class->finalize = state_editor_finalize;
  object_class->set_property = state_editor_set_property;
  object_class->get_property = state_editor_get_property;

  properties[PROP_PAINTABLE] =
    g_param_spec_object ("paintable", NULL, NULL,
                        PATH_PAINTABLE_TYPE,
                        G_PARAM_READWRITE | G_PARAM_STATIC_NAME);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gtk/Shaper/state-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, StateEditor, grid);
  gtk_widget_class_bind_template_callback (widget_class, drop_state);
  gtk_widget_class_bind_template_callback (widget_class, add_state);
}

/* }}} */
 /* {{{ Public API */

StateEditor *
state_editor_new (void)
{
  return g_object_new (STATE_EDITOR_TYPE, NULL);
}

PathPaintable *
state_editor_get_paintable (StateEditor *self)
{
  g_return_val_if_fail (STATE_IS_EDITOR (self), NULL);

  return self->paintable;
}

void
state_editor_set_paintable (StateEditor *self,
                            PathPaintable   *paintable)
{
  g_return_if_fail (STATE_IS_EDITOR (self));

  if (self->paintable == paintable)
    return;

  if (self->paintable)
    {
      g_signal_handlers_disconnect_by_func (self->paintable, paths_changed, self);
    }

  g_set_object (&self->paintable, paintable);

  if (paintable)
    {
      g_signal_connect_swapped (paintable, "paths-changed",
                                G_CALLBACK (paths_changed), self);
      paths_changed (self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAINTABLE]);
}

/* }}} */

/* vim:set foldmethod=marker: */
