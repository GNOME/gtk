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

#include "gtkcolumnviewtitleprivate.h"

#include "gtkcolumnviewcolumnprivate.h"
#include "gtkintl.h"
#include "gtklabel.h"
#include "gtkwidgetprivate.h"

struct _GtkColumnViewTitle
{
  GtkWidget parent_instance;

  GtkColumnViewColumn *column;

  GtkWidget *title;
};

struct _GtkColumnViewTitleClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkColumnViewTitle, gtk_column_view_title, GTK_TYPE_WIDGET)

static void
gtk_column_view_title_measure (GtkWidget      *widget,
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
gtk_column_view_title_size_allocate (GtkWidget *widget,
                                     int        width,
                                     int        height,
                                     int        baseline)
{
  GtkWidget *child = gtk_widget_get_first_child (widget);

  if (child)
    gtk_widget_allocate (child, width, height, baseline, NULL);
}

static void
gtk_column_view_title_dispose (GObject *object)
{
  GtkColumnViewTitle *self = GTK_COLUMN_VIEW_TITLE (object);

  g_clear_pointer(&self->title, gtk_widget_unparent);

  g_clear_object (&self->column);

  G_OBJECT_CLASS (gtk_column_view_title_parent_class)->dispose (object);
}

static void
gtk_column_view_title_class_init (GtkColumnViewTitleClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  widget_class->measure = gtk_column_view_title_measure;
  widget_class->size_allocate = gtk_column_view_title_size_allocate;

  gobject_class->dispose = gtk_column_view_title_dispose;

  gtk_widget_class_set_css_name (widget_class, I_("button"));
}

static void
gtk_column_view_title_resize_func (GtkWidget *widget)
{
  GtkColumnViewTitle *self = GTK_COLUMN_VIEW_TITLE (widget);

  if (self->column)
    gtk_column_view_column_queue_resize (self->column);
}

static void
gtk_column_view_title_init (GtkColumnViewTitle *self)
{
  GtkWidget *widget = GTK_WIDGET (self);

  widget->priv->resize_func = gtk_column_view_title_resize_func;

  self->title = gtk_label_new (NULL);
  gtk_widget_set_parent (self->title, widget);
}

GtkWidget *
gtk_column_view_title_new (GtkColumnViewColumn *column)
{
  GtkColumnViewTitle *title;

  title = g_object_new (GTK_TYPE_COLUMN_VIEW_TITLE,
                        NULL);

  title->column = g_object_ref (column);
  gtk_column_view_title_update (title);

  return GTK_WIDGET (title);
}

void
gtk_column_view_title_update (GtkColumnViewTitle *self)
{
  gtk_label_set_label (GTK_LABEL (self->title), gtk_column_view_column_get_title (self->column));
}

GtkColumnViewColumn *
gtk_column_view_title_get_column (GtkColumnViewTitle *self)
{
  return self->column;
}
