/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gtkwidgetprivate.h"
#include "gtkwindow.h"

typedef struct _CompareInfo CompareInfo;

enum Axis {
  HORIZONTAL = 0,
  VERTICAL   = 1
};

struct _CompareInfo
{
  GtkWidget *widget;
  int x;
  int y;
  guint reverse : 1;
  guint axis : 1;
};

static inline void
get_axis_info (const GdkRectangle *rect,
               int                 axis,
               int                *origin,
               int                *bounds)
{
  if (axis == HORIZONTAL)
    {
      *origin = rect->x;
      *bounds = rect->width;
    }
  else if (axis == VERTICAL)
    {
      *origin = rect->y;
      *bounds = rect->height;
    }
  else
    g_assert(FALSE);
}

/* Utility function, equivalent to g_list_reverse */
static void
reverse_ptr_array (GPtrArray *arr)
{
  int i;

  for (i = 0; i < arr->len / 2; i ++)
    {
      void *a = g_ptr_array_index (arr, i);
      void *b = g_ptr_array_index (arr, arr->len - 1 - i);

      arr->pdata[i] = b;
      arr->pdata[arr->len - 1 - i] = a;
    }
}

/* Get coordinates of @widget's allocation with respect to
 * allocation of @container.
 */
static gboolean
get_allocation_coords (GtkWidget    *widget,
                       GtkWidget    *child,
                       GdkRectangle *allocation)
{
  gtk_widget_get_allocation (child, allocation);

  return gtk_widget_translate_coordinates (child, widget,
                                           0, 0, &allocation->x, &allocation->y);
}

static int
tab_sort_func (gconstpointer a,
               gconstpointer b,
               gpointer      user_data)
{
  GtkAllocation child1_allocation, child2_allocation;
  const GtkWidget *child1 = *((GtkWidget **)a);
  const GtkWidget *child2 = *((GtkWidget **)b);
  GtkTextDirection text_direction = GPOINTER_TO_INT (user_data);
  int y1, y2;

  _gtk_widget_get_allocation ((GtkWidget *) child1, &child1_allocation);
  _gtk_widget_get_allocation ((GtkWidget *) child2, &child2_allocation);

  y1 = child1_allocation.y + child1_allocation.height / 2;
  y2 = child2_allocation.y + child2_allocation.height / 2;

  if (y1 == y2)
    {
      int x1 = child1_allocation.x + child1_allocation.width / 2;
      int x2 = child2_allocation.x + child2_allocation.width / 2;

      if (text_direction == GTK_TEXT_DIR_RTL)
        return (x1 < x2) ? 1 : ((x1 == x2) ? 0 : -1);
      else
        return (x1 < x2) ? -1 : ((x1 == x2) ? 0 : 1);
    }
  else
    return (y1 < y2) ? -1 : 1;
}

static void
focus_sort_tab (GtkWidget        *widget,
                GtkDirectionType  direction,
                GPtrArray        *focus_order)
{
  GtkTextDirection text_direction = _gtk_widget_get_direction (widget);

  g_ptr_array_sort_with_data (focus_order, tab_sort_func, GINT_TO_POINTER (text_direction));

  if (direction == GTK_DIR_TAB_BACKWARD)
    reverse_ptr_array (focus_order);
}

/* Look for a child in @children that is intermediate between
 * the focus widget and container. This widget, if it exists,
 * acts as the starting widget for focus navigation.
 */
static GtkWidget *
find_old_focus (GtkWidget *widget,
                GPtrArray *children)
{
  int i;

  for (i = 0; i < children->len; i ++)
    {
      GtkWidget *child = g_ptr_array_index (children, i);
      GtkWidget *child_ptr = child;

      while (child_ptr && child_ptr != widget)
        {
          GtkWidget *parent;

          parent = _gtk_widget_get_parent (child_ptr);

          if (parent && (gtk_widget_get_focus_child (parent) != child_ptr))
            {
              child = NULL;
              break;
            }

          child_ptr = parent;
        }

      if (child)
        return child;
    }

  return NULL;
}

static gboolean
old_focus_coords (GtkWidget *widget,
                  GdkRectangle *old_focus_rect)
{
  GtkWidget *toplevel = _gtk_widget_get_toplevel (widget);
  GtkWidget *old_focus;

  if (GTK_IS_WINDOW (toplevel))
    {
      old_focus = gtk_window_get_focus (GTK_WINDOW (toplevel));
      if (old_focus)
        return get_allocation_coords (widget, old_focus, old_focus_rect);
    }

  return FALSE;
}

static int
axis_compare (gconstpointer a,
              gconstpointer b,
              gpointer      user_data)
{
  GdkRectangle allocation1;
  GdkRectangle allocation2;
  CompareInfo *compare = user_data;
  int origin1, origin2;
  int bounds1, bounds2;

  get_allocation_coords (compare->widget, *((GtkWidget **)a), &allocation1);
  get_allocation_coords (compare->widget, *((GtkWidget **)b), &allocation2);

  get_axis_info (&allocation1, compare->axis, &origin1, &bounds1);
  get_axis_info (&allocation2, compare->axis, &origin2, &bounds2);

  origin1 = origin1 + (bounds1 / 2);
  origin2 = origin2 + (bounds2 / 2);

  if (origin1 == origin2)
    {
      /* Now use origin/bounds to compare the 2 widgets on the other axis */
      get_axis_info (&allocation1, 1 - compare->axis, &origin1, &bounds1);
      get_axis_info (&allocation2, 1 - compare->axis, &origin2, &bounds2);

      int x1 = abs (origin1 + (bounds2 / 2) - compare->x);
      int x2 = abs (origin2 + (bounds2 / 2) - compare->x);

      if (compare->reverse)
        return (x1 < x2) ? 1 : ((x1 == x2) ? 0 : -1);
      else
        return (x1 < x2) ? -1 : ((x1 == x2) ? 0 : 1);
    }
  else
    return (origin1 < origin2) ? -1 : 1;
}

static void
focus_sort_left_right (GtkWidget        *widget,
                       GtkDirectionType  direction,
                       GPtrArray        *focus_order)
{
  CompareInfo compare_info;
  GdkRectangle old_allocation;
  GtkWidget *old_focus = gtk_widget_get_focus_child (widget);

  compare_info.widget = widget;
  compare_info.reverse = (direction == GTK_DIR_LEFT);

  if (!old_focus)
    old_focus = find_old_focus (widget, focus_order);

  if (old_focus && get_allocation_coords (widget, old_focus, &old_allocation))
    {
      int compare_y1;
      int compare_y2;
      int compare_x;
      int i;

      /* Delete widgets from list that don't match minimum criteria */

      compare_y1 = old_allocation.y;
      compare_y2 = old_allocation.y + old_allocation.height;

      if (direction == GTK_DIR_LEFT)
        compare_x = old_allocation.x;
      else
        compare_x = old_allocation.x + old_allocation.width;

      for (i = 0; i < focus_order->len; i ++)
        {
          GtkWidget *child = g_ptr_array_index (focus_order, i);
          int child_y1, child_y2;
          GdkRectangle child_allocation;

          if (child != old_focus)
            {
              if (get_allocation_coords (widget, child, &child_allocation))
                {
                  child_y1 = child_allocation.y;
                  child_y2 = child_allocation.y + child_allocation.height;

                  if ((child_y2 <= compare_y1 || child_y1 >= compare_y2) /* No vertical overlap */ ||
                      (direction == GTK_DIR_RIGHT && child_allocation.x + child_allocation.width < compare_x) || /* Not to left */
                      (direction == GTK_DIR_LEFT && child_allocation.x > compare_x)) /* Not to right */
                    {
                      g_ptr_array_remove_index (focus_order, i);
                      i --;
                    }
                }
              else
                {
                  g_ptr_array_remove_index (focus_order, i);
                  i --;
                }
            }
        }

      compare_info.y = (compare_y1 + compare_y2) / 2;
      compare_info.x = old_allocation.x + old_allocation.width / 2;
    }
  else
    {
      /* No old focus widget, need to figure out starting x,y some other way
       */
      GtkAllocation allocation;
      GdkRectangle old_focus_rect;

      _gtk_widget_get_allocation (widget, &allocation);

      if (old_focus_coords (widget, &old_focus_rect))
        {
          compare_info.y = old_focus_rect.y + old_focus_rect.height / 2;
        }
      else
        {
          if (!_gtk_widget_get_has_window (widget))
            compare_info.y = allocation.y + allocation.height / 2;
          else
            compare_info.y = allocation.height / 2;
        }

      if (!_gtk_widget_get_has_window (widget))
        compare_info.x = (direction == GTK_DIR_RIGHT) ? allocation.x : allocation.x + allocation.width;
      else
        compare_info.x = (direction == GTK_DIR_RIGHT) ? 0 : allocation.width;
    }


  compare_info.axis = HORIZONTAL;
  g_ptr_array_sort_with_data (focus_order, axis_compare, &compare_info);

  if (compare_info.reverse)
    reverse_ptr_array (focus_order);
}

static void
focus_sort_up_down (GtkWidget        *widget,
                    GtkDirectionType  direction,
                    GPtrArray        *focus_order)
{
  CompareInfo compare_info;
  GdkRectangle old_allocation;
  GtkWidget *old_focus = gtk_widget_get_focus_child (widget);

  compare_info.widget = widget;
  compare_info.reverse = (direction == GTK_DIR_UP);

  if (!old_focus)
    old_focus = find_old_focus (widget, focus_order);

  if (old_focus && get_allocation_coords (widget, old_focus, &old_allocation))
    {
      int compare_x1;
      int compare_x2;
      int compare_y;
      int i;

      /* Delete widgets from list that don't match minimum criteria */

      compare_x1 = old_allocation.x;
      compare_x2 = old_allocation.x + old_allocation.width;

      if (direction == GTK_DIR_UP)
        compare_y = old_allocation.y;
      else
        compare_y = old_allocation.y + old_allocation.height;

      for (i = 0; i < focus_order->len; i ++)
        {
          GtkWidget *child = g_ptr_array_index (focus_order, i);
          int child_x1, child_x2;
          GdkRectangle child_allocation;

          if (child != old_focus)
            {
              if (get_allocation_coords (widget, child, &child_allocation))
                {
                  child_x1 = child_allocation.x;
                  child_x2 = child_allocation.x + child_allocation.width;

                  if ((child_x2 <= compare_x1 || child_x1 >= compare_x2) /* No horizontal overlap */ ||
                      (direction == GTK_DIR_DOWN && child_allocation.y + child_allocation.height < compare_y) || /* Not below */
                      (direction == GTK_DIR_UP && child_allocation.y > compare_y)) /* Not above */
                    {
                      g_ptr_array_remove_index (focus_order, i);
                      i --;
                    }
                }
              else
                {
                  g_ptr_array_remove_index (focus_order, i);
                  i --;
                }
            }
        }

      compare_info.x = (compare_x1 + compare_x2) / 2;
      compare_info.y = old_allocation.y + old_allocation.height / 2;
    }
  else
    {
      /* No old focus widget, need to figure out starting x,y some other way
       */
      GtkAllocation allocation;
      GdkRectangle old_focus_rect;

      _gtk_widget_get_allocation (widget, &allocation);

      if (old_focus_coords (widget, &old_focus_rect))
        {
          compare_info.x = old_focus_rect.x + old_focus_rect.width / 2;
        }
      else
        {
          if (!_gtk_widget_get_has_window (widget))
            compare_info.x = allocation.x + allocation.width / 2;
          else
            compare_info.x = allocation.width / 2;
        }

      if (!_gtk_widget_get_has_window (widget))
        compare_info.y = (direction == GTK_DIR_DOWN) ? allocation.y : allocation.y + allocation.height;
      else
        compare_info.y = (direction == GTK_DIR_DOWN) ? 0 : + allocation.height;
    }

  compare_info.axis = VERTICAL;
  g_ptr_array_sort_with_data (focus_order, axis_compare, &compare_info);

  if (compare_info.reverse)
    reverse_ptr_array (focus_order);
}

void
gtk_widget_focus_sort (GtkWidget        *widget,
                       GtkDirectionType  direction,
                       GPtrArray        *focus_order)
{
  GtkWidget *child;

  g_assert (focus_order->len == 0);

  /* Initialize the list with all realized child widgets */
  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      if (_gtk_widget_get_realized (child))
        g_ptr_array_add (focus_order, child);
    }

  /* Now sort that list depending on @direction */
  switch (direction)
    {
    case GTK_DIR_TAB_FORWARD:
    case GTK_DIR_TAB_BACKWARD:
      focus_sort_tab (widget, direction, focus_order);
      break;
    case GTK_DIR_UP:
    case GTK_DIR_DOWN:
      focus_sort_up_down (widget, direction, focus_order);
      break;
    case GTK_DIR_LEFT:
    case GTK_DIR_RIGHT:
      focus_sort_left_right (widget, direction, focus_order);
      break;
    default:
      g_assert_not_reached ();
    }
}


gboolean
gtk_widget_focus_move (GtkWidget        *widget,
                       GtkDirectionType  direction,
                       GPtrArray        *focus_order)
{
  GtkWidget *focus_child = gtk_widget_get_focus_child (widget);
  int i;

  for (i = 0; i < focus_order->len; i ++)
    {
      GtkWidget *child = g_ptr_array_index (focus_order, i);

      if (focus_child)
        {
          if (focus_child == child)
            {
              focus_child = NULL;

                if (gtk_widget_child_focus (child, direction))
                  return TRUE;
            }
        }
      else if (_gtk_widget_is_drawable (child) &&
               gtk_widget_is_ancestor (child, widget))
        {
          if (gtk_widget_child_focus (child, direction))
            return TRUE;
        }
    }

  return FALSE;
}
