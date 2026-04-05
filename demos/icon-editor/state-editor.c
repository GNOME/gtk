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
#include "gtk/svg/gtksvgelementprivate.h"

struct _StateEditor
{
  GtkWindow parent_instance;

  GtkGrid *grid;
  GtkSpinButton *initial_state;
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
                         SvgElement       *shape)
{
  return shape_get_path_image (shape, path_paintable_get_svg (self->paintable));
}

static gboolean
valid_state_name (const char *name)
{
  if (strcmp (name, "all") == 0 ||
      strcmp (name, "none") == 0 ||
      g_ascii_isdigit (name[0]))
    return FALSE;

  return TRUE;
}

static unsigned int
find_max_state (SvgElement *shape)
{
  uint64_t states = svg_element_get_states (shape);

  if (svg_element_get_type (shape) == SVG_ELEMENT_SVG ||
      svg_element_get_type (shape) == SVG_ELEMENT_GROUP)
    {
      unsigned int state = 0;
      for (unsigned int i = 0; i < svg_element_get_n_children (shape); i++)
        {
          SvgElement *sh = svg_element_get_child (shape, i);
          state = MAX (state, find_max_state (sh));
        }
      return state;
    }
  else if (states == 0 || states == G_MAXUINT64)
    return 0;
  else
    return g_bit_nth_msf (states, -1);
}

static void
update_state_names (StateEditor *self)
{
  GtkSvg *svg = path_paintable_get_svg (self->paintable);
  const char *names[65] = { NULL, };
  unsigned int i;

  for (i = 0; i <= self->max_state; i++)
    {
      GtkEditable *e;
      const char *text;

      e = GTK_EDITABLE (gtk_grid_get_child_at (self->grid, i, -1));
      text = gtk_editable_get_text (e);
      if (text && valid_state_name (text))
        {
          names[i] = text;
        }
      else
        {
          char num[64];
          g_snprintf (num, sizeof (num), "%u", i);
          if (strcmp (num, text) == 0)
            {
              names[i] = NULL;
              break;
            }
          else
            {
              gtk_editable_set_text (e, num);
              return;
            }
        }
    }

  names[i + 1] = NULL;

  gtk_svg_set_state_names (svg, names);
  path_paintable_changed (self->paintable);
}

static unsigned int
count_shapes (SvgElement *shape)
{
  SvgElementType type = svg_element_get_type (shape);
  unsigned int count = 1;

  if (svg_element_type_is_container (type))
    {
      for (unsigned int i = 0; i < svg_element_get_n_children (shape); i++)
        {
          SvgElement *sh = svg_element_get_child (shape, i);

          count += count_shapes (sh);
        }
    }

  return count;
}

static void
update_states (StateEditor *self)
{
  GtkLayoutManager *mgr = gtk_widget_get_layout_manager (GTK_WIDGET (self->grid));
  uint64_t *states;
  unsigned int n;

  n = count_shapes (path_paintable_get_svg (self->paintable)->content);

  states = g_newa0 (uint64_t, n);

  for (unsigned int i = 0; i < n; i++)
    {
      GtkLayoutChild *layout_child;
      int row;

      GtkWidget *child = gtk_grid_get_child_at (self->grid, -2, i);
      GtkWidget *toggle = gtk_grid_get_child_at (self->grid, -1, i);

      if (!GTK_IS_LABEL (child))
        break;

      layout_child = gtk_layout_manager_get_layout_child (mgr, child);
      row = gtk_grid_layout_child_get_row (GTK_GRID_LAYOUT_CHILD (layout_child));

      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)))
        states[row] = G_MAXUINT64;
    }

  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (self->grid));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkLayoutChild *layout_child;
      int row, col;

      layout_child = gtk_layout_manager_get_layout_child (mgr, child);
      row = gtk_grid_layout_child_get_row (GTK_GRID_LAYOUT_CHILD (layout_child));
      col = gtk_grid_layout_child_get_column (GTK_GRID_LAYOUT_CHILD (layout_child));

      if (GTK_IS_CHECK_BUTTON (child))
        {
          if (gtk_check_button_get_active (GTK_CHECK_BUTTON (child)))
            states[row] |= G_GUINT64_CONSTANT (1) << (unsigned int) col;
          else
            states[row] &= ~(G_GUINT64_CONSTANT (1) << (unsigned int) col);
        }
    }

  self->updating = TRUE;

  for (unsigned int i = 0; i < n; i++)
    {
      GtkWidget *child = gtk_grid_get_child_at (self->grid, -2, i);
      const char *id;

      if (!GTK_IS_LABEL (child))
        break;

      id = gtk_label_get_label (GTK_LABEL (child));
      path_paintable_set_path_states_by_id (self->paintable, id, states[i]);
    }

  self->updating = FALSE;

  repopulate (self);
}

static void
update_one (GtkWidget   *check,
            GParamSpec  *pspec,
            StateEditor *self)
{
  GtkLayoutManager *mgr;
  GtkLayoutChild *layout_child;
  int row;
  GtkWidget *label;
  const char *id;
  uint64_t states;

  mgr = gtk_widget_get_layout_manager (GTK_WIDGET (self->grid));
  layout_child = gtk_layout_manager_get_layout_child (mgr, check);
  row = gtk_grid_layout_child_get_row (GTK_GRID_LAYOUT_CHILD (layout_child));

  label = gtk_grid_get_child_at (self->grid, -2, row);
  id = gtk_label_get_label (GTK_LABEL (label));

  states = 0;
  for (unsigned int i = 0; i < self->max_state; i++)
    {
      GtkWidget *child;

      child = gtk_grid_get_child_at (self->grid, i, row);
      if (gtk_check_button_get_active (GTK_CHECK_BUTTON (child)))
        states |= (G_GUINT64_CONSTANT (1) << (unsigned int) i);
    }

  self->updating = TRUE;
  path_paintable_set_path_states_by_id (self->paintable, id, states);
  self->updating = FALSE;

  if (!gtk_check_button_get_active (GTK_CHECK_BUTTON (check)))
    {
      GtkWidget *toggle = gtk_grid_get_child_at (self->grid, -1, row);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), FALSE);
    }
}

static void
update_all (GtkWidget   *toggle,
            GParamSpec  *pspec,
            StateEditor *self)
{
  GtkLayoutManager *mgr;
  GtkLayoutChild *layout_child;
  int row;
  GtkWidget *label;
  const char *id;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)))
    return;

  mgr = gtk_widget_get_layout_manager (GTK_WIDGET (self->grid));
  layout_child = gtk_layout_manager_get_layout_child (mgr, toggle);
  row = gtk_grid_layout_child_get_row (GTK_GRID_LAYOUT_CHILD (layout_child));

  label = gtk_grid_get_child_at (self->grid, -2, row);
  id = gtk_label_get_label (GTK_LABEL (label));

  self->updating = TRUE;
  path_paintable_set_path_states_by_id (self->paintable, id, G_MAXUINT64);
  self->updating = FALSE;

  repopulate (self);
}

static void
drop_state (StateEditor *self)
{
  if (self->max_state == 0)
    return;

  self->max_state--;
  self->max_state = CLAMP (self->max_state, 0, 63);

  update_state_names (self);
  update_states (self);
}

static void
add_state (StateEditor *self)
{
  if (self->max_state == 63)
    return;

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
create_paths_for_shape (StateEditor  *self,
                        SvgElement   *shape,
                        unsigned int *row)
{
  for (unsigned int i = 0; i < svg_element_get_n_children (shape); i++)
    {
      SvgElement *sh = svg_element_get_child (shape, i);

      if (svg_element_get_type (sh) == SVG_ELEMENT_GROUP)
        {
          create_paths_for_shape (self, sh, row);
          continue;
        }
      else if (svg_element_type_is_graphical (svg_element_get_type (sh)))
        {
          uint64_t states = svg_element_get_states (sh);
          const char *id = svg_element_get_id (sh);
          GdkPaintable *paintable = get_paintable_for_shape (self, sh);
          GtkWidget *child;

          child = gtk_image_new_from_paintable (paintable);
          gtk_image_set_pixel_size (GTK_IMAGE (child), 20);
          g_object_unref (paintable);
          gtk_grid_attach (self->grid, child, -3, *row, 1, 1);

          child = gtk_label_new (id);
          gtk_grid_attach (self->grid, child, -2, *row, 1, 1);

          child = gtk_toggle_button_new_with_label ("All");
          gtk_grid_attach (self->grid, child, -1, *row, 1, 1);

          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (child), states == G_MAXUINT64);

          g_signal_connect (child, "notify::active", G_CALLBACK (update_all), self);

          for (unsigned int j = 0; j <= self->max_state; j++)
            {
              child = gtk_check_button_new ();
              gtk_widget_set_halign (child, GTK_ALIGN_CENTER);
              gtk_check_button_set_active (GTK_CHECK_BUTTON (child),
                                           (states & ((G_GUINT64_CONSTANT (1) << j))) != 0);
              g_signal_connect (child, "notify::active", G_CALLBACK (update_one), self);
              gtk_grid_attach (self->grid, child, j, *row, 1, 1);
            }

          (*row)++;
        }
    }
}

static void
state_name_changed (GtkEditable *editable,
                    GParamSpec  *pspec,
                    gpointer     data)
{
  StateEditor *self = (StateEditor *) data;

  if (self->updating)
    return;

  if (gtk_editable_label_get_editing (GTK_EDITABLE_LABEL (editable)))
    return;

  update_state_names (self);
}

static void
create_paths (StateEditor *self)
{
  GtkSvg *svg = path_paintable_get_svg (self->paintable);
  GtkWidget *child;
  const char **names;
  unsigned int n_names;
  unsigned int row;

  names = gtk_svg_get_state_names (svg, &n_names);

  for (unsigned int i = 0; i <= self->max_state; i++)
    {
      if (i < n_names)
        child = gtk_editable_label_new (names[i]);
      else
        {
          char *s = g_strdup_printf ("%u", i);
          child = gtk_editable_label_new (s);
          g_free (s);
        }
      gtk_editable_set_width_chars (GTK_EDITABLE (child), 6);
      gtk_grid_attach (self->grid, child, i, -1, 1, 1);
      g_signal_connect (child, "notify::editing", G_CALLBACK (state_name_changed), self);
    }

  row = 0;
  create_paths_for_shape (self, svg->content, &row);
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
  GtkSvg *svg = path_paintable_get_svg (self->paintable);
  self->max_state = MAX (self->max_state, find_max_state (svg->content));
  self->max_state = CLAMP (self->max_state, 0, 63);

  repopulate (self);
}

static void
initial_state_changed (StateEditor *self)
{
  GtkSvg *svg = path_paintable_get_svg (self->paintable);
  svg->initial_state = (unsigned int) gtk_spin_button_get_value_as_int (self->initial_state);
  path_paintable_changed (self->paintable);
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
  gtk_widget_class_bind_template_child (widget_class, StateEditor, initial_state);
  gtk_widget_class_bind_template_callback (widget_class, drop_state);
  gtk_widget_class_bind_template_callback (widget_class, add_state);
  gtk_widget_class_bind_template_callback (widget_class, initial_state_changed);
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
      gtk_spin_button_set_value (self->initial_state, path_paintable_get_svg (paintable)->initial_state);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PAINTABLE]);
}

/* }}} */

/* vim:set foldmethod=marker: */
