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

#include "gtkcolumnviewprivate.h"
#include "gtkcolumnviewcolumnprivate.h"
#include "gtkcolumnviewsorterprivate.h"
#include "gtkprivate.h"
#include "gtklabel.h"
#include "gtkwidgetprivate.h"
#include "gtkbox.h"
#include "gtkbuiltiniconprivate.h"
#include "gtkgestureclick.h"
#include "gtkpopovermenu.h"
#include "gtknative.h"
#include "gtkcssnodeprivate.h"
#include "gtkcssnumbervalueprivate.h"

struct _GtkColumnViewTitle
{
  GtkWidget parent_instance;

  GtkColumnViewColumn *column;

  GtkWidget *box;
  GtkWidget *title;
  GtkWidget *sort;
  GtkWidget *popup_menu;
};

struct _GtkColumnViewTitleClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkColumnViewTitle, gtk_column_view_title, GTK_TYPE_WIDGET)

static int
get_number (GtkCssValue *value)
{
  double d = _gtk_css_number_value_get (value, 100);

  if (d < 1)
    return ceil (d);
  else
    return floor (d);
}

static int
unadjust_width (GtkWidget *widget,
                int        width)
{
  GtkCssStyle *style;
  int widget_margins;
  int css_extra;

  style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));
  css_extra = get_number (style->size->margin_left) +
              get_number (style->size->margin_right) +
              get_number (style->border->border_left_width) +
              get_number (style->border->border_right_width) +
              get_number (style->size->padding_left) +
              get_number (style->size->padding_right);
  widget_margins = widget->priv->margin.left + widget->priv->margin.right;

  return MAX (0, width - widget_margins - css_extra);
}

static void
gtk_column_view_title_measure (GtkWidget      *widget,
                               GtkOrientation  orientation,
                               int             for_size,
                               int            *minimum,
                               int            *natural,
                               int            *minimum_baseline,
                               int            *natural_baseline)
{
  GtkColumnViewTitle *self = GTK_COLUMN_VIEW_TITLE (widget);
  GtkWidget *child = gtk_widget_get_first_child (widget);
  int fixed_width = gtk_column_view_column_get_fixed_width (self->column);
  int unadj_width;

  unadj_width = unadjust_width (widget, fixed_width);

  if (orientation == GTK_ORIENTATION_VERTICAL)
    {
      if (fixed_width > -1)
        {
          if (for_size == -1)
            for_size = unadj_width;
          else
            for_size = MIN (for_size, unadj_width);
        }
    }

  if (child)
    gtk_widget_measure (child, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      if (fixed_width > -1)
        {
          *minimum = 0;
          *natural = unadj_width;
        }
    }
}

static void
gtk_column_view_title_size_allocate (GtkWidget *widget,
                                     int        width,
                                     int        height,
                                     int        baseline)
{
  GtkColumnViewTitle *self = GTK_COLUMN_VIEW_TITLE (widget);
  GtkWidget *child = gtk_widget_get_first_child (widget);

  if (child)
    {
      int min;

      gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, height, &min, NULL, NULL, NULL);

      gtk_widget_allocate (child, MAX (min, width), height, baseline, NULL);
    }

  if (self->popup_menu)
    gtk_popover_present (GTK_POPOVER (self->popup_menu));
}

static void
gtk_column_view_title_dispose (GObject *object)
{
  GtkColumnViewTitle *self = GTK_COLUMN_VIEW_TITLE (object);

  g_clear_pointer (&self->box, gtk_widget_unparent);
  g_clear_pointer (&self->popup_menu, gtk_widget_unparent);

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
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_COLUMN_HEADER);
}

static void
gtk_column_view_title_resize_func (GtkWidget *widget)
{
  GtkColumnViewTitle *self = GTK_COLUMN_VIEW_TITLE (widget);

  if (self->column)
    gtk_column_view_column_queue_resize (self->column);
}

static void
activate_sort (GtkColumnViewTitle *self)
{
  GtkSorter *sorter;
  GtkColumnView *view;
  GtkSorter *view_sorter;

  sorter = gtk_column_view_column_get_sorter (self->column);
  if (sorter == NULL)
    return;

  view = gtk_column_view_column_get_column_view (self->column);
  view_sorter = gtk_column_view_get_sorter (view);
  gtk_column_view_sorter_activate_column (view_sorter, self->column);
}

static void
show_menu (GtkColumnViewTitle *self,
           double              x,
           double              y)
{
  if (!self->popup_menu)
    {
      GMenuModel *model;

      model = gtk_column_view_column_get_header_menu (self->column);
      if (!model)
        return;

      self->popup_menu = gtk_popover_menu_new_from_model (model);
      gtk_widget_set_parent (self->popup_menu, GTK_WIDGET (self));
      gtk_popover_set_position (GTK_POPOVER (self->popup_menu), GTK_POS_BOTTOM);

      gtk_popover_set_has_arrow (GTK_POPOVER (self->popup_menu), FALSE);
      gtk_widget_set_halign (self->popup_menu, GTK_ALIGN_START);
    }

  if (x != -1 && y != -1)
    {
      GdkRectangle rect = { x, y, 1, 1 };
      gtk_popover_set_pointing_to (GTK_POPOVER (self->popup_menu), &rect);
    }
  else
    gtk_popover_set_pointing_to (GTK_POPOVER (self->popup_menu), NULL);

  gtk_popover_popup (GTK_POPOVER (self->popup_menu));
}

static void
click_released_cb (GtkGestureClick *gesture,
                   guint            n_press,
                   double           x,
                   double           y,
                   GtkWidget       *widget)
{
  GtkColumnViewTitle *self = GTK_COLUMN_VIEW_TITLE (widget);
  guint button;

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  if (button == GDK_BUTTON_PRIMARY)
    activate_sort (self);
  else if (button == GDK_BUTTON_SECONDARY)
    show_menu (self, x, y);
}

static void
gtk_column_view_title_init (GtkColumnViewTitle *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkGesture *gesture;

  widget->priv->resize_func = gtk_column_view_title_resize_func;

  gtk_widget_set_overflow (widget, GTK_OVERFLOW_HIDDEN);

  self->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_parent (self->box, widget);

  self->title = gtk_label_new (NULL);
  gtk_box_append (GTK_BOX (self->box), self->title);

  self->sort = gtk_builtin_icon_new ("sort-indicator");
  gtk_box_append (GTK_BOX (self->box), self->sort);

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "released", G_CALLBACK (click_released_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));
}

GtkWidget *
gtk_column_view_title_new (GtkColumnViewColumn *column)
{
  GtkColumnViewTitle *title;

  title = g_object_new (GTK_TYPE_COLUMN_VIEW_TITLE, NULL);

  title->column = g_object_ref (column);
  gtk_column_view_title_update (title);

  return GTK_WIDGET (title);
}

void
gtk_column_view_title_update (GtkColumnViewTitle *self)
{
  GtkInvertibleSorter *sorter;

  gtk_label_set_label (GTK_LABEL (self->title), gtk_column_view_column_get_title (self->column));

  sorter = gtk_column_view_column_get_invertible_sorter (self->column);

  if (sorter)
    {
      GtkColumnView *view;
      GtkSorter *view_sorter;
      GtkInvertibleSorter *active = NULL;
      GtkSortType direction = GTK_SORT_ASCENDING;

      view = gtk_column_view_column_get_column_view (self->column);
      view_sorter = gtk_column_view_get_sorter (view);
      if (g_list_model_get_n_items (G_LIST_MODEL (view_sorter)) > 0)
        {
          active = g_list_model_get_item (G_LIST_MODEL (view_sorter), 0);
          g_object_unref (active);
          direction = gtk_invertible_sorter_get_sort_order (active);
        }

      gtk_widget_show (self->sort);
      gtk_widget_remove_css_class (self->sort, "ascending");
      gtk_widget_remove_css_class (self->sort, "descending");
      gtk_widget_remove_css_class (self->sort, "unsorted");
      if (sorter != active)
        gtk_widget_add_css_class (self->sort, "unsorted");
      else if (direction == GTK_SORT_DESCENDING)
        gtk_widget_add_css_class (self->sort, "descending");
      else
        gtk_widget_add_css_class (self->sort, "ascending");
    }
  else
    gtk_widget_hide (self->sort);

  g_clear_pointer (&self->popup_menu, gtk_widget_unparent);
}

GtkColumnViewColumn *
gtk_column_view_title_get_column (GtkColumnViewTitle *self)
{
  return self->column;
}
