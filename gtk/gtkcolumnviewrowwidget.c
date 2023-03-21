/*
 * Copyright © 2023 Benjamin Otte
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

#include "gtkcolumnviewrowwidgetprivate.h"

#include "gtkbinlayout.h"
#include "gtkcolumnviewprivate.h"
#include "gtkcolumnviewcellprivate.h"
#include "gtkcolumnviewcolumnprivate.h"
#include "gtkcolumnviewtitleprivate.h"
#include "gtklistitemfactoryprivate.h"
#include "gtklistbaseprivate.h"
#include "gtkwidget.h"
#include "gtkwidgetprivate.h"

G_DEFINE_TYPE (GtkColumnViewRowWidget, gtk_column_view_row_widget, GTK_TYPE_LIST_FACTORY_WIDGET)

static GtkColumnView *
gtk_column_view_row_widget_get_column_view (GtkColumnViewRowWidget *self)
{
  GtkWidget *parent = _gtk_widget_get_parent (GTK_WIDGET (self));

  if (GTK_IS_COLUMN_VIEW (parent))
    return GTK_COLUMN_VIEW (parent);

  parent = _gtk_widget_get_parent (parent);
  g_assert (GTK_IS_COLUMN_VIEW (parent));

  return GTK_COLUMN_VIEW (parent);
}

static gboolean
gtk_column_view_row_widget_is_header (GtkColumnViewRowWidget *self)
{
  return gtk_widget_get_css_name (GTK_WIDGET (self)) == g_intern_static_string ("header");
}

static void
gtk_column_view_row_widget_update (GtkListItemBase *base,
                                   guint            position,
                                   gpointer         item,
                                   gboolean         selected)
{
  GtkColumnViewRowWidget *self = GTK_COLUMN_VIEW_ROW_WIDGET (base);
  GtkListFactoryWidget *fw = GTK_LIST_FACTORY_WIDGET (base);
  gboolean selectable, activatable;
  GtkWidget *child;

  if (gtk_column_view_row_widget_is_header (self))
    return;

  GTK_LIST_ITEM_BASE_CLASS (gtk_column_view_row_widget_parent_class)->update (base, position, item, selected);

  /* This really does not belong here, but doing better
   * requires considerable plumbing that we don't have now,
   * and something like this is needed to fix the filechooser
   * in select_folder mode.
   */
  selectable = TRUE;
  activatable = TRUE;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self));
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      gtk_list_item_base_update (GTK_LIST_ITEM_BASE (child), position, item, selected);

      selectable &= gtk_list_factory_widget_get_selectable (GTK_LIST_FACTORY_WIDGET (child));
      activatable &= gtk_list_factory_widget_get_activatable (GTK_LIST_FACTORY_WIDGET (child));
    }

  gtk_list_factory_widget_set_selectable (fw, selectable);
  gtk_list_factory_widget_set_activatable (fw, activatable);
}

static GtkWidget *
gtk_column_view_next_focus_widget (GtkWidget        *widget,
                                   GtkWidget        *child,
                                   GtkDirectionType  direction)
{
  gboolean forward;

  switch (direction)
    {
      case GTK_DIR_TAB_FORWARD:
        forward = TRUE;
        break;
      case GTK_DIR_TAB_BACKWARD:
        forward = FALSE;
        break;
      case GTK_DIR_LEFT:
        forward = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
        break;
      case GTK_DIR_RIGHT:
        forward = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;
        break;
      case GTK_DIR_UP:
      case GTK_DIR_DOWN:
        return NULL;
      default:
        g_return_val_if_reached (NULL);
    }

  if (forward)
    {
      if (child)
        return gtk_widget_get_next_sibling (child);
      else
        return gtk_widget_get_first_child (widget);
    }
  else
    {
      if (child)
        return gtk_widget_get_prev_sibling (child);
      else
        return gtk_widget_get_last_child (widget);
    }
}

static gboolean
gtk_column_view_row_widget_focus (GtkWidget        *widget,
                                  GtkDirectionType  direction)
{
  GtkWidget *child, *focus_child;

  /* The idea of this function is the following:
   * 1. If any child can take focus, do not ever attempt
   *    to take focus.
   * 2. Otherwise, if this item is selectable or activatable,
   *    allow focusing this widget.
   *
   * This makes sure every item in a list is focusable for
   * activation and selection handling, but no useless widgets
   * get focused and moving focus is as fast as possible.
   */

  focus_child = gtk_widget_get_focus_child (widget);
  if (focus_child && gtk_widget_child_focus (focus_child, direction))
    return TRUE;

  for (child = gtk_column_view_next_focus_widget (widget, focus_child, direction);
       child;
       child = gtk_column_view_next_focus_widget (widget, child, direction))
    {
      if (gtk_widget_child_focus (child, direction))
        return TRUE;
    }

  if (focus_child)
    return FALSE;

  if (gtk_widget_is_focus (widget))
    return FALSE;

  return gtk_widget_grab_focus (widget);
}

static gboolean
gtk_column_view_row_widget_grab_focus (GtkWidget *widget)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (widget);
       child;
       child = gtk_widget_get_next_sibling (child))
    {
      if (gtk_widget_grab_focus (child))
        return TRUE;
    }

  if (!gtk_list_factory_widget_get_selectable (GTK_LIST_FACTORY_WIDGET (widget)))
    return FALSE;

  return GTK_WIDGET_CLASS (gtk_column_view_row_widget_parent_class)->grab_focus (widget);
}

static void
gtk_column_view_row_widget_root (GtkWidget *widget)
{
  GtkColumnViewRowWidget *self = GTK_COLUMN_VIEW_ROW_WIDGET (widget);

  GTK_WIDGET_CLASS (gtk_column_view_row_widget_parent_class)->root (widget);

  if (!gtk_column_view_row_widget_is_header (self))
    {
      GtkListItemBase *base = GTK_LIST_ITEM_BASE (self);
      GListModel *columns;
      guint i;

      columns = gtk_column_view_get_columns (gtk_column_view_row_widget_get_column_view (self));

      for (i = 0; i < g_list_model_get_n_items (columns); i++)
        {
          GtkColumnViewColumn *column = g_list_model_get_item (columns, i);

          if (gtk_column_view_column_get_visible (column))
            {
              GtkWidget *cell;

              cell = gtk_column_view_cell_new (column);
              gtk_column_view_row_widget_add_child (self, cell);
              gtk_list_item_base_update (GTK_LIST_ITEM_BASE (cell),
                                         gtk_list_item_base_get_position (base),
                                         gtk_list_item_base_get_item (base),
                                         gtk_list_item_base_get_selected (base));
            }

          g_object_unref (column);
        }
    }
}

static void
gtk_column_view_row_widget_unroot (GtkWidget *widget)
{
  GtkColumnViewRowWidget *self = GTK_COLUMN_VIEW_ROW_WIDGET (widget);
  GtkWidget *child;

  if (!gtk_column_view_row_widget_is_header (self))
    {
      while ((child = gtk_widget_get_first_child (GTK_WIDGET (self))))
        {
          gtk_column_view_row_widget_remove_child (self, child);
        }
    }

  GTK_WIDGET_CLASS (gtk_column_view_row_widget_parent_class)->unroot (widget);
}

static void
gtk_column_view_row_widget_measure_along (GtkColumnViewRowWidget *self,
                                          int                     for_size,
                                          int                    *minimum,
                                          int                    *natural,
                                          int                    *minimum_baseline,
                                          int                    *natural_baseline)
{
  GtkOrientation orientation = GTK_ORIENTATION_VERTICAL;
  GtkColumnView *view;
  GtkWidget *child;
  guint i, n;
  GtkRequestedSize *sizes = NULL;

  view = gtk_column_view_row_widget_get_column_view (self);

  if (for_size > -1)
    {
      n = g_list_model_get_n_items (gtk_column_view_get_columns (view));
      sizes = g_newa (GtkRequestedSize, n);
      gtk_column_view_distribute_width (view, for_size, sizes);
    }

  for (child = _gtk_widget_get_first_child (GTK_WIDGET (self)), i = 0;
       child != NULL;
       child = _gtk_widget_get_next_sibling (child), i++)
    {
      int child_min = 0;
      int child_nat = 0;
      int child_min_baseline = -1;
      int child_nat_baseline = -1;

      if (!gtk_widget_should_layout (child))
        continue;

      gtk_widget_measure (child, orientation,
                          for_size > -1 ? sizes[i].minimum_size : -1,
                          &child_min, &child_nat,
                          &child_min_baseline, &child_nat_baseline);

      *minimum = MAX (*minimum, child_min);
      *natural = MAX (*natural, child_nat);

      if (child_min_baseline > -1)
        *minimum_baseline = MAX (*minimum_baseline, child_min_baseline);
      if (child_nat_baseline > -1)
        *natural_baseline = MAX (*natural_baseline, child_nat_baseline);
    }
}

static void
gtk_column_view_row_widget_measure (GtkWidget      *widget,
                                    GtkOrientation  orientation,
                                    int             for_size,
                                    int            *minimum,
                                    int            *natural,
                                    int            *minimum_baseline,
                                    int            *natural_baseline)
{
  GtkColumnViewRowWidget *self = GTK_COLUMN_VIEW_ROW_WIDGET (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      gtk_column_view_measure_across (gtk_column_view_row_widget_get_column_view (self),
                                      minimum,
                                      natural);
    }
  else
    {
      gtk_column_view_row_widget_measure_along (self,
                                                for_size,
                                                minimum,
                                                natural,
                                                minimum_baseline,
                                                natural_baseline);
    }
}

static void
gtk_column_view_row_widget_allocate (GtkWidget *widget,
                                     int        width,
                                     int        height,
                                     int        baseline)
{
  GtkWidget *child;

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      GtkColumnViewColumn *column;
      int col_x, col_width, min;

      if (!gtk_widget_should_layout (child))
        continue;

      if (GTK_IS_COLUMN_VIEW_CELL (child))
        column = gtk_column_view_cell_get_column (GTK_COLUMN_VIEW_CELL (child));
      else
        column = gtk_column_view_title_get_column (GTK_COLUMN_VIEW_TITLE (child));
      gtk_column_view_column_get_header_allocation (column, &col_x, &col_width);

      gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, &min, NULL, NULL, NULL);

      gtk_widget_size_allocate (child, &(GtkAllocation) { col_x, 0, MAX (min, col_width), height }, baseline);
    }
}

static void
add_arrow_bindings (GtkWidgetClass   *widget_class,
		    guint             keysym,
		    GtkDirectionType  direction)
{
  guint keypad_keysym = keysym - GDK_KEY_Left + GDK_KEY_KP_Left;

  gtk_widget_class_add_binding_signal (widget_class, keysym, 0,
                                       "move-focus",
                                       "(i)",
                                       direction);
  gtk_widget_class_add_binding_signal (widget_class, keysym, GDK_CONTROL_MASK,
                                       "move-focus",
                                       "(i)",
                                       direction);
  gtk_widget_class_add_binding_signal (widget_class, keypad_keysym, 0,
                                       "move-focus",
                                       "(i)",
                                       direction);
  gtk_widget_class_add_binding_signal (widget_class, keypad_keysym, GDK_CONTROL_MASK,
                                       "move-focus",
                                       "(i)",
                                       direction);
}

static void
gtk_column_view_row_widget_class_init (GtkColumnViewRowWidgetClass *klass)
{
  GtkListItemBaseClass *base_class = GTK_LIST_ITEM_BASE_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  base_class->update = gtk_column_view_row_widget_update;

  widget_class->focus = gtk_column_view_row_widget_focus;
  widget_class->grab_focus = gtk_column_view_row_widget_grab_focus;
  widget_class->measure = gtk_column_view_row_widget_measure;
  widget_class->size_allocate = gtk_column_view_row_widget_allocate;
  widget_class->root = gtk_column_view_row_widget_root;
  widget_class->unroot = gtk_column_view_row_widget_unroot;

  add_arrow_bindings (widget_class, GDK_KEY_Left, GTK_DIR_LEFT);
  add_arrow_bindings (widget_class, GDK_KEY_Right, GTK_DIR_RIGHT);

  /* This gets overwritten by gtk_column_view_row_widget_new() but better safe than sorry */
  gtk_widget_class_set_css_name (widget_class, g_intern_static_string ("row"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_ROW);
}

static void
gtk_column_view_row_widget_init (GtkColumnViewRowWidget *self)
{
  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);
}

GtkWidget *
gtk_column_view_row_widget_new (gboolean is_header)
{
  return g_object_new (GTK_TYPE_COLUMN_VIEW_ROW_WIDGET,
                       "css-name", is_header ? "header" : "row",
                       "selectable", TRUE,
                       "activatable", TRUE,
                       NULL);
}

void
gtk_column_view_row_widget_add_child (GtkColumnViewRowWidget *self,
                                      GtkWidget              *child)
{
  gtk_widget_set_parent (child, GTK_WIDGET (self));
}

void
gtk_column_view_row_widget_reorder_child (GtkColumnViewRowWidget *self,
                                          GtkWidget              *child,
                                          guint                   position)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkWidget *sibling = NULL;

  if (position > 0)
    {
      GtkWidget *c;
      guint i;

      for (c = gtk_widget_get_first_child (widget), i = 0;
           c;
           c = gtk_widget_get_next_sibling (c), i++)
        {
          if (i + 1 == position)
            {
              sibling = c;
              break;
            }
        }
    }

  if (child != sibling)
    gtk_widget_insert_after (child, widget, sibling);
}

void
gtk_column_view_row_widget_remove_child (GtkColumnViewRowWidget *self,
                                         GtkWidget              *child)
{
  gtk_widget_unparent (child);
}

