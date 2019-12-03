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
#include "gtkboxlayout.h"
#include "gtkimage.h"
#include "gtkgestureclick.h"

struct _GtkColumnViewTitle
{
  GtkWidget parent_instance;

  GtkColumnViewColumn *column;

  GtkWidget *title;
  GtkWidget *sort;
};

struct _GtkColumnViewTitleClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkColumnViewTitle, gtk_column_view_title, GTK_TYPE_WIDGET)

static void
gtk_column_view_title_dispose (GObject *object)
{
  GtkColumnViewTitle *self = GTK_COLUMN_VIEW_TITLE (object);

  g_signal_handlers_disconnect_by_func (self->column, gtk_column_view_title_update, self);
  g_clear_pointer (&self->title, gtk_widget_unparent);
  g_clear_pointer (&self->sort, gtk_widget_unparent);

  g_clear_object (&self->column);

  G_OBJECT_CLASS (gtk_column_view_title_parent_class)->dispose (object);
}

static void
gtk_column_view_title_class_init (GtkColumnViewTitleClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_column_view_title_dispose;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
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
click_pressed_cb (GtkGestureClick *gesture,
                  guint            n_press,
                  gdouble          x,
                  gdouble          y,
                  GtkWidget       *widget)
{
  GtkColumnViewTitle *self = GTK_COLUMN_VIEW_TITLE (widget);
  GtkSorter *sorter;
  GtkSorter *active_sorter;
  
  sorter = gtk_column_view_column_get_sorter (self->column);
  active_sorter = gtk_column_view_get_sorter (gtk_column_view_column_get_column_view (self->column));

  if (sorter)
    {
      if (sorter == active_sorter)
        {
          gtk_sorter_set_sort_direction (sorter, 1 - gtk_sorter_get_sort_direction (sorter));
        }
      else
        {
          gtk_sorter_set_sort_direction (sorter, GTK_SORT_ASCENDING);
          gtk_column_view_set_sorter (gtk_column_view_column_get_column_view (self->column), sorter);
        }
    }

  gtk_column_view_title_update (self);
}

static void
gtk_column_view_title_init (GtkColumnViewTitle *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkGesture *gesture;

  widget->priv->resize_func = gtk_column_view_title_resize_func;

  self->title = gtk_label_new (NULL);
  gtk_widget_set_parent (self->title, widget);

  self->sort = gtk_image_new ();
  gtk_widget_set_parent (self->sort, widget);

  gesture = gtk_gesture_click_new ();
  g_signal_connect (gesture, "pressed", G_CALLBACK (click_pressed_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));
}

GtkWidget *
gtk_column_view_title_new (GtkColumnViewColumn *column)
{
  GtkColumnViewTitle *title;

  title = g_object_new (GTK_TYPE_COLUMN_VIEW_TITLE,
                        NULL);

  title->column = g_object_ref (column);
  gtk_column_view_title_update (title);

  g_signal_connect_swapped (gtk_column_view_column_get_column_view (column), "notify::sorter",
                            G_CALLBACK (gtk_column_view_title_update), title);

  return GTK_WIDGET (title);
}

void
gtk_column_view_title_update (GtkColumnViewTitle *self)
{
  GtkSorter *sorter;
  GtkSorter *active_sorter;

  gtk_label_set_label (GTK_LABEL (self->title), gtk_column_view_column_get_title (self->column));

  sorter = gtk_column_view_column_get_sorter (self->column);
  active_sorter = gtk_column_view_get_sorter (gtk_column_view_column_get_column_view (self->column));

  if (sorter)
    {
      gtk_widget_show (self->sort);
      if (sorter == active_sorter)
        {
          if (gtk_sorter_get_sort_direction (sorter) == GTK_SORT_ASCENDING)
            gtk_image_set_from_icon_name (GTK_IMAGE (self->sort), "pan-down-symbolic");
          else          
            gtk_image_set_from_icon_name (GTK_IMAGE (self->sort), "pan-up-symbolic");
        }
      else
        gtk_image_clear (GTK_IMAGE (self->sort));
    }
  else
    gtk_widget_hide (self->sort);
}

GtkColumnViewColumn *
gtk_column_view_title_get_column (GtkColumnViewTitle *self)
{
  return self->column;
}
