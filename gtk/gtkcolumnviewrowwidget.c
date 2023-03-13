/*
 * Copyright © 2018 Benjamin Otte
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
#include "gtklistitemfactoryprivate.h"
#include "gtklistbaseprivate.h"
#include "gtkwidget.h"
#include "gtkwidgetprivate.h"

G_DEFINE_TYPE (GtkColumnViewRowWidget, gtk_column_view_row_widget, GTK_TYPE_LIST_FACTORY_WIDGET)

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

  for (child = focus_child ? gtk_widget_get_next_sibling (focus_child)
                           : gtk_widget_get_first_child (widget);
       child;
       child = gtk_widget_get_next_sibling (child))
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
gtk_column_view_row_widget_class_init (GtkColumnViewRowWidgetClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->focus = gtk_column_view_row_widget_focus;
  widget_class->grab_focus = gtk_column_view_row_widget_grab_focus;

  /* This gets overwritten by gtk_column_view_row_widget_new() but better safe than sorry */
  gtk_widget_class_set_css_name (widget_class, I_("row"));
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static void
gtk_column_view_row_widget_init (GtkColumnViewRowWidget *self)
{
  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);
}

GtkWidget *
gtk_column_view_row_widget_new (GtkListItemFactory *factory,
                                const char         *css_name,
                                GtkAccessibleRole   role)
{
  g_return_val_if_fail (css_name != NULL, NULL);

  return g_object_new (GTK_TYPE_COLUMN_VIEW_ROW_WIDGET,
                       "css-name", css_name,
                       "accessible-role", role,
                       "factory", factory,
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

