/*
 * Copyright (c) 2008-2009  Christian Hammond
 * Copyright (c) 2008-2009  David Trowbridge
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#include "window.h"
#include "widget-tree.h"

#include "gtknotebook.h"
#include "gtkmain.h"

typedef struct
{
  gint x;
  gint y;
  gboolean found;
  gboolean first;
  GtkWidget *res_widget;
} FindWidgetData;

static void
find_widget (GtkWidget      *widget,
             FindWidgetData *data)
{
  GtkAllocation new_allocation;
  gint x_offset = 0;
  gint y_offset = 0;

  gtk_widget_get_allocation (widget, &new_allocation);

  if (data->found || !gtk_widget_get_mapped (widget))
    return;

  /* Note that in the following code, we only count the
   * position as being inside a WINDOW widget if it is inside
   * widget->window; points that are outside of widget->window
   * but within the allocation are not counted. This is consistent
   * with the way we highlight drag targets.
   */
  if (gtk_widget_get_has_window (widget))
    {
      new_allocation.x = 0;
      new_allocation.y = 0;
    }

  if (gtk_widget_get_parent (widget) && !data->first)
    {
      GdkWindow *window;

      window = gtk_widget_get_window (widget);
      while (window != gtk_widget_get_window (gtk_widget_get_parent (widget)))
        {
          gint tx, ty, twidth, theight;

          if (window == NULL)
            return;

          twidth = gdk_window_get_width (window);
          theight = gdk_window_get_height (window);

          if (new_allocation.x < 0)
            {
              new_allocation.width += new_allocation.x;
              new_allocation.x = 0;
            }
          if (new_allocation.y < 0)
            {
              new_allocation.height += new_allocation.y;
              new_allocation.y = 0;
            }
          if (new_allocation.x + new_allocation.width > twidth)
            new_allocation.width = twidth - new_allocation.x;
          if (new_allocation.y + new_allocation.height > theight)
            new_allocation.height = theight - new_allocation.y;

          gdk_window_get_position (window, &tx, &ty);
          new_allocation.x += tx;
          x_offset += tx;
          new_allocation.y += ty;
          y_offset += ty;

          window = gdk_window_get_parent (window);
        }
    }

  if ((data->x >= new_allocation.x) && (data->y >= new_allocation.y) &&
      (data->x < new_allocation.x + new_allocation.width) &&
      (data->y < new_allocation.y + new_allocation.height))
    {
      /* First, check if the drag is in a valid drop site in
       * one of our children 
       */
      if (GTK_IS_CONTAINER (widget))
        {
          FindWidgetData new_data = *data;

          new_data.x -= x_offset;
          new_data.y -= y_offset;
          new_data.found = FALSE;
          new_data.first = FALSE;

          gtk_container_forall (GTK_CONTAINER (widget),
                                (GtkCallback)find_widget,
                                &new_data);

          data->found = new_data.found;
          if (data->found)
            data->res_widget = new_data.res_widget;
        }

      /* If not, and this widget is registered as a drop site, check to
       * emit "drag_motion" to check if we are actually in
       * a drop site.
       */
      if (!data->found)
        {
          data->found = TRUE;
          data->res_widget = widget;
        }
    }
}

static GtkWidget *
find_widget_at_pointer (GdkDevice *device)
{
  GtkWidget *widget = NULL;
  GdkWindow *pointer_window;
  gint x, y;
  FindWidgetData data;

  pointer_window = gdk_device_get_window_at_position (device, NULL, NULL);

  if (pointer_window)
    {
      gpointer widget_ptr;

      gdk_window_get_user_data (pointer_window, &widget_ptr);
      widget = widget_ptr;
    }

  if (widget)
    {
      gdk_window_get_device_position (gtk_widget_get_window (widget),
                                      device, &x, &y, NULL);

      data.x = x;
      data.y = y;
      data.found = FALSE;
      data.first = TRUE;

      find_widget (widget, &data);
      if (data.found)
        return data.res_widget;

      return widget;
    }

  return NULL;
}

static gboolean draw_flash (GtkWidget          *widget,
                            cairo_t            *cr,
                            GtkInspectorWindow *iw);

static void
clear_flash (GtkInspectorWindow *iw)
{
  if (iw->flash_widget)
    {
      gtk_widget_queue_draw (iw->flash_widget);
      g_signal_handlers_disconnect_by_func (iw->flash_widget, draw_flash, iw);
      iw->flash_widget = NULL;
    }
}

static void
start_flash (GtkInspectorWindow *iw,
             GtkWidget          *widget)
{
  iw->flash_count = 1;
  iw->flash_widget = widget;
  g_signal_connect_after (widget, "draw", G_CALLBACK (draw_flash), iw);
  gtk_widget_queue_draw (widget);
}

static void
select_widget (GtkInspectorWindow *iw,
               GtkWidget          *widget)
{
  iw->selected_widget = widget;

  gtk_notebook_set_current_page (GTK_NOTEBOOK (iw->top_notebook), 0);

  gtk_inspector_widget_tree_scan (GTK_INSPECTOR_WIDGET_TREE (iw->widget_tree),
                                  gtk_widget_get_toplevel (widget));

  gtk_inspector_widget_tree_select_object (GTK_INSPECTOR_WIDGET_TREE (iw->widget_tree),
                                           G_OBJECT (widget));
}

static void
on_inspect_widget (GtkWidget          *button,
                   GdkEvent           *event,
                   GtkInspectorWindow *iw)
{
  GtkWidget *widget;

  gdk_window_raise (gtk_widget_get_window (GTK_WIDGET (iw)));

  clear_flash (iw);

  widget = find_widget_at_pointer (gdk_event_get_device (event));

  if (widget)
    select_widget (iw, widget);
}

static void
on_highlight_widget (GtkWidget          *button,
                     GdkEvent           *event,
                     GtkInspectorWindow *iw)
{
  GtkWidget *widget;

  widget = find_widget_at_pointer (gdk_event_get_device (event));

  if (widget == NULL)
    {
      /* This window isn't in-process. Ignore it. */
      return;
    }

  if (gtk_widget_get_toplevel (widget) == GTK_WIDGET (iw))
    {
      /* Don't hilight things in the inspector window */
      return;
    }

  if (iw->flash_widget == widget)
    {
      /* Already selected */
      return;
    }

  clear_flash (iw);
  start_flash (iw, widget);
}

static gboolean
property_query_event (GtkWidget *widget,
                      GdkEvent  *event,
                      gpointer   data)
{
  GtkInspectorWindow *iw = (GtkInspectorWindow *)data;

  if (event->type == GDK_BUTTON_RELEASE)
    {
      g_signal_handlers_disconnect_by_func (widget, property_query_event, data);
      gtk_grab_remove (widget);
      gdk_device_ungrab (gdk_event_get_device (event), GDK_CURRENT_TIME);
      on_inspect_widget (widget, event, data);
    }
  else if (event->type == GDK_MOTION_NOTIFY)
    {
      on_highlight_widget (widget, event, data);
    }
  else if (event->type == GDK_KEY_PRESS)
    {
      GdkEventKey *ke = (GdkEventKey*)event;
      GdkDevice *device;

      if (ke->keyval == GDK_KEY_Escape)
        {
          g_signal_handlers_disconnect_by_func (widget, property_query_event, data);
          gtk_grab_remove (widget);
          device = gdk_device_get_associated_device (gdk_event_get_device (event));
          gdk_device_ungrab (device, GDK_CURRENT_TIME);
          gdk_window_raise (gtk_widget_get_window (GTK_WIDGET (iw)));
          clear_flash (iw);
        }
    }

  return FALSE;
}

void
on_inspect (GtkWidget          *button,
            GtkInspectorWindow *iw)
{
  GdkDisplay *display;
  GdkDevice *device;
  GdkCursor *cursor;

  g_signal_connect (button, "event",
                    G_CALLBACK (property_query_event), iw);

  display = gtk_widget_get_display (button);
  cursor = gdk_cursor_new_for_display (display, GDK_CROSSHAIR);
  device = gdk_device_manager_get_client_pointer (gdk_display_get_device_manager (display));
  gdk_device_grab (device,
                   gtk_widget_get_window (GTK_WIDGET (button)),
                   GDK_OWNERSHIP_NONE, TRUE,
                   GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK,
                   cursor, GDK_CURRENT_TIME);
  g_object_unref (cursor);
  gtk_grab_add (GTK_WIDGET (button));

  gdk_window_lower (gtk_widget_get_window (GTK_WIDGET (iw)));
}

static gboolean
draw_flash (GtkWidget          *widget,
            cairo_t            *cr,
            GtkInspectorWindow *iw)
{
  GtkAllocation alloc;

  if (iw && iw->flash_count % 2 == 0)
    return FALSE;

  if (GTK_IS_WINDOW (widget))
    {
      /* We don't want to draw the drag highlight around the
       * CSD window decorations
       */
      gtk_widget_get_allocation (gtk_bin_get_child (GTK_BIN (widget)), &alloc);
    }
  else
    {
      alloc.x = 0;
      alloc.y = 0;
      alloc.width = gtk_widget_get_allocated_width (widget);
      alloc.height = gtk_widget_get_allocated_height (widget);
    }

  cairo_set_source_rgba (cr, 0.0, 0.0, 1.0, 0.2);
  cairo_rectangle (cr,
                   alloc.x + 0.5, alloc.y + 0.5,
                   alloc.width - 1, alloc.height - 1);
  cairo_fill (cr);

  return FALSE;
}

static gboolean
on_flash_timeout (GtkInspectorWindow *iw)
{
  gtk_widget_queue_draw (iw->flash_widget);

  iw->flash_count++;

  if (iw->flash_count == 6)
    {
      g_signal_handlers_disconnect_by_func (iw->flash_widget, draw_flash, iw);
      iw->flash_widget = NULL;
      iw->flash_cnx = 0;

      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

void
gtk_inspector_flash_widget (GtkInspectorWindow *iw,
                            GtkWidget          *widget)
{
  if (iw->flash_cnx != 0)
    return;

  if (!gtk_widget_get_visible (widget) || !gtk_widget_get_mapped (widget))
    return;

  start_flash (iw, widget);
  iw->flash_cnx = g_timeout_add (150, (GSourceFunc) on_flash_timeout, iw);
}

void
gtk_inspector_start_highlight (GtkWidget *widget)
{
  g_signal_connect_after (widget, "draw", G_CALLBACK (draw_flash), NULL);
  gtk_widget_queue_draw (widget);
}

void
gtk_inspector_stop_highlight (GtkWidget *widget)
{
  g_signal_handlers_disconnect_by_func (widget, draw_flash, NULL);
  gtk_widget_queue_draw (widget);
}

void
gtk_inspector_window_select_widget_under_pointer (GtkInspectorWindow *iw)
{
  GdkDisplay *display;
  GdkDeviceManager *dm;
  GdkDevice *device;
  GtkWidget *widget;

  display = gtk_widget_get_display (GTK_WIDGET (iw));
  dm = gdk_display_get_device_manager (display);
  device = gdk_device_manager_get_client_pointer (dm);

  widget = find_widget_at_pointer (device);

  if (widget)
    select_widget (iw, widget);
}

/* vim: set et sw=2 ts=2: */
