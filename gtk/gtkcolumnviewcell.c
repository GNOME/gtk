/*
 * Copyright © 2019 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkcolumnviewcellprivate.h"

#include "gtkcolumnviewcolumnprivate.h"
#include "gtkintl.h"
#include "gtklistitemwidgetprivate.h"
#include "gtkwidgetprivate.h"

struct _GtkColumnViewCell
{
  GtkListItemWidget parent_instance;

  GtkColumnViewColumn *column;

  /* This list isn't sorted - next/prev refer to list elements, not rows in the list */
  GtkColumnViewCell *next_cell;
  GtkColumnViewCell *prev_cell;
};

struct _GtkColumnViewCellClass
{
  GtkListItemWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkColumnViewCell, gtk_column_view_cell, GTK_TYPE_LIST_ITEM_WIDGET)

static void
gtk_column_view_cell_measure (GtkWidget      *widget,
                              GtkOrientation  orientation,
                              int             for_size,
                              int            *minimum,
                              int            *natural,
                              int            *minimum_baseline,
                              int            *natural_baseline)
{
  GtkWidget *child = gtk_widget_get_first_child (widget);

  if (child)
    gtk_widget_measure (child, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);
}

static void
gtk_column_view_cell_size_allocate (GtkWidget *widget,
                                    int        width,
                                    int        height,
                                    int        baseline)
{
  GtkWidget *child = gtk_widget_get_first_child (widget);

  if (child)
    gtk_widget_allocate (child, width, height, baseline, NULL);
}

static void
gtk_column_view_cell_root (GtkWidget *widget)
{
  GtkColumnViewCell *self = GTK_COLUMN_VIEW_CELL (widget);

  GTK_WIDGET_CLASS (gtk_column_view_cell_parent_class)->root (widget);

  self->next_cell = gtk_column_view_column_get_first_cell (self->column);
  if (self->next_cell)
    self->next_cell->prev_cell = self;

  gtk_column_view_column_add_cell (self->column, self);
}

static void
gtk_column_view_cell_unroot (GtkWidget *widget)
{
  GtkColumnViewCell *self = GTK_COLUMN_VIEW_CELL (widget);

  gtk_column_view_column_remove_cell (self->column, self);

  if (self->prev_cell)
    self->prev_cell->next_cell = self->next_cell;
  if (self->next_cell)
    self->next_cell->prev_cell = self->prev_cell;

  self->prev_cell = NULL;
  self->next_cell = NULL;

  GTK_WIDGET_CLASS (gtk_column_view_cell_parent_class)->unroot (widget);
}

static void
gtk_column_view_cell_dispose (GObject *object)
{
  GtkColumnViewCell *self = GTK_COLUMN_VIEW_CELL (object);

  g_clear_object (&self->column);

  G_OBJECT_CLASS (gtk_column_view_cell_parent_class)->dispose (object);
}

static void
gtk_column_view_cell_class_init (GtkColumnViewCellClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  widget_class->root = gtk_column_view_cell_root;
  widget_class->unroot = gtk_column_view_cell_unroot;
  widget_class->measure = gtk_column_view_cell_measure;
  widget_class->size_allocate = gtk_column_view_cell_size_allocate;

  gobject_class->dispose = gtk_column_view_cell_dispose;

  gtk_widget_class_set_css_name (widget_class, I_("cell"));
}

static void
gtk_column_view_cell_resize_func (GtkWidget *widget)
{
  GtkColumnViewCell *self = GTK_COLUMN_VIEW_CELL (widget);

  if (self->column)
    gtk_column_view_column_queue_resize (self->column);
}

static void
gtk_column_view_cell_init (GtkColumnViewCell *self)
{
  GtkWidget *widget = GTK_WIDGET (self);

  gtk_widget_set_can_focus (widget, FALSE);
  /* FIXME: Figure out if settting the manager class to INVALID should work */
  gtk_widget_set_layout_manager (widget, NULL);
  widget->priv->resize_func = gtk_column_view_cell_resize_func;
}

GtkWidget *
gtk_column_view_cell_new (GtkColumnViewColumn *column)
{
  GtkColumnViewCell *cell;

  cell = g_object_new (GTK_TYPE_COLUMN_VIEW_CELL,
                       "factory", gtk_column_view_column_get_factory (column),
                       NULL);

  cell->column = g_object_ref (column);

  return GTK_WIDGET (cell);
}

void
gtk_column_view_cell_remove (GtkColumnViewCell *self)
{
  GtkWidget *widget = GTK_WIDGET (self);

  gtk_list_item_widget_remove_child (GTK_LIST_ITEM_WIDGET (gtk_widget_get_parent (widget)), widget);
}

GtkColumnViewCell *
gtk_column_view_cell_get_next (GtkColumnViewCell *self)
{
  return self->next_cell;
}

GtkColumnViewCell *
gtk_column_view_cell_get_prev (GtkColumnViewCell *self)
{
  return self->prev_cell;
}

GtkColumnViewColumn *
gtk_column_view_cell_get_column (GtkColumnViewCell *self)
{
  return self->column;
}
