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

#include "highlightoverlay.h"
#include "object-tree.h"

#include "gtkstack.h"
#include "gtkmain.h"
#include "gtkinvisible.h"
#include "gtkwidgetprivate.h"
#include "gtkgesturemultipress.h"
#include "gtkeventcontrollermotion.h"
#include "gtkeventcontrollerkey.h"

static gboolean
inspector_contains (GtkWidget *widget,
                    double     x,
                    double     y)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (!gtk_widget_is_drawable (widget))
    return FALSE;

  return GTK_WIDGET_GET_CLASS (widget)->contains (widget, x, y);
}

static GtkWidget *
inspector_pick (GtkWidget *widget,
                double     x,
                double     y)
{
  /* Like gtk_widget_pick and gtk_widget_contains,
   * but we need to consider insensitive widgets as well. */
  GtkWidget *child;

  for (child = _gtk_widget_get_last_child (widget);
       child;
       child = _gtk_widget_get_prev_sibling (child))
    {
      GtkWidget *picked;
      int dx, dy;

      gtk_widget_get_origin_relative_to_parent (child, &dx, &dy);

      picked = GTK_WIDGET_GET_CLASS (child)->pick (child, x - dx, y - dy);
      if (picked)
        return picked;
    }


  if (!inspector_contains (widget, x, y))
    return NULL;

  return widget;
}

static GtkWidget *
find_widget_at_pointer (GdkDevice *device)
{
  GtkWidget *widget = NULL;
  GdkSurface *pointer_surface;

  pointer_surface = gdk_device_get_surface_at_position (device, NULL, NULL);

  if (pointer_surface)
    {
      gpointer widget_ptr;

      gdk_surface_get_user_data (pointer_surface, &widget_ptr);
      widget = widget_ptr;

      if (!GTK_IS_WINDOW (widget))
        {
          while (TRUE)
            {
              GdkSurface *parent = gdk_surface_get_parent (pointer_surface);

              if (!parent)
                break;

              pointer_surface = parent;
              gdk_surface_get_user_data (pointer_surface, &widget_ptr);
              widget = widget_ptr;

              if (GTK_IS_WINDOW (widget))
                break;
            }

        }
    }

  if (widget)
    {
      double x, y;

      gdk_surface_get_device_position_double (gtk_widget_get_surface (widget),
                                             device, &x, &y, NULL);

      widget = inspector_pick (widget, x, y);
    }

  return widget;
}

static void
clear_flash (GtkInspectorWindow *iw)
{
  if (iw->flash_overlay)
    {
      gtk_inspector_window_remove_overlay (iw, iw->flash_overlay);
      g_clear_object (&iw->flash_overlay);
    }
}

static void
start_flash (GtkInspectorWindow *iw,
             GtkWidget          *widget)
{
  clear_flash (iw);

  iw->flash_count = 1;
  iw->flash_overlay = gtk_highlight_overlay_new (widget);
  gtk_inspector_window_add_overlay (iw, iw->flash_overlay);
}

static void
select_widget (GtkInspectorWindow *iw,
               GtkWidget          *widget)
{
  GtkInspectorObjectTree *wt = GTK_INSPECTOR_OBJECT_TREE (iw->object_tree);

  iw->selected_widget = widget;

  gtk_inspector_object_tree_select_object (wt, G_OBJECT (widget));
}

static void
on_inspect_widget (GtkWidget          *button,
                   GdkEvent           *event,
                   GtkInspectorWindow *iw)
{
  GtkWidget *widget;

  gdk_surface_raise (gtk_widget_get_surface (GTK_WIDGET (iw)));

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

  if (iw->flash_overlay &&
      gtk_highlight_overlay_get_widget (GTK_HIGHLIGHT_OVERLAY (iw->flash_overlay)) == widget)
    {
      /* Already selected */
      return;
    }

  clear_flash (iw);
  start_flash (iw, widget);
}

static void
deemphasize_window (GtkWidget *window)
{
  GdkDisplay *display;

  display = gtk_widget_get_display (window);
  if (gdk_display_is_composited (display))
    {
      cairo_rectangle_int_t rect;
      cairo_region_t *region;

      gtk_widget_set_opacity (window, 0.3);
      rect.x = rect.y = rect.width = rect.height = 0;
      region = cairo_region_create_rectangle (&rect);
      gtk_widget_input_shape_combine_region (window, region);
      cairo_region_destroy (region);
    }
  else
    gdk_surface_lower (gtk_widget_get_surface (window));
}

static void
reemphasize_window (GtkWidget *window)
{
  GdkDisplay *display;

  display = gtk_widget_get_display (window);
  if (gdk_display_is_composited (display))
    {
      gtk_widget_set_opacity (window, 1.0);
      gtk_widget_input_shape_combine_region (window, NULL);
    }
  else
    gdk_surface_raise (gtk_widget_get_surface (window));
}

static void
property_query_pressed (GtkGestureMultiPress *gesture,
                        guint                 n_press,
                        gdouble               x,
                        gdouble               y,
                        GtkInspectorWindow   *iw)
{
  GdkEvent *event;

  gtk_grab_remove (iw->invisible);
  if (iw->grab_seat)
    {
      gdk_seat_ungrab (iw->grab_seat);
      iw->grab_seat = NULL;
    }

  reemphasize_window (GTK_WIDGET (iw));

  event = gtk_get_current_event ();
  on_inspect_widget (iw->invisible, event, iw);
  g_object_unref (event);

  gtk_widget_destroy (iw->invisible);
  iw->invisible = NULL;
}

static void
property_query_motion (GtkEventControllerMotion *controller,
                       gdouble                   x,
                       gdouble                   y,
                       GtkInspectorWindow       *iw)
{
  GdkEvent *event;

  event = gtk_get_current_event ();
  on_highlight_widget (iw->invisible, event, iw);
  g_object_unref (event);
}

static gboolean
property_query_key (GtkEventControllerKey *key,
                    guint                  keyval,
                    guint                  keycode,
                    GdkModifierType        modifiers,
                    GtkInspectorWindow    *iw)
{
  if (keyval == GDK_KEY_Escape)
    {
      gtk_grab_remove (iw->invisible);
      if (iw->grab_seat)
        {
          gdk_seat_ungrab (iw->grab_seat);
          iw->grab_seat = NULL;
        }
      reemphasize_window (GTK_WIDGET (iw));

      clear_flash (iw);

      gtk_widget_destroy (iw->invisible);
      iw->invisible = NULL;

      return TRUE;
    }

  return FALSE;
}

static void
prepare_inspect_func (GdkSeat   *seat,
                      GdkSurface *surface,
                      gpointer   user_data)
{
  gdk_surface_show (surface);
}


void
gtk_inspector_on_inspect (GtkWidget          *button,
                          GtkInspectorWindow *iw)
{
  GdkDisplay *display;
  GdkCursor *cursor;
  GdkGrabStatus status;
  GtkEventController *controller;
  GdkSeat *seat;

  if (!iw->invisible)
    {
      iw->invisible = gtk_invisible_new_for_display (gdk_display_get_default ());
      gtk_widget_realize (iw->invisible);
      gtk_widget_show (iw->invisible);
    }

  display = gdk_display_get_default ();
  cursor = gdk_cursor_new_from_name ("crosshair", NULL);
  seat = gdk_display_get_default_seat (display);
  status = gdk_seat_grab (seat,
                          gtk_widget_get_surface (iw->invisible),
                          GDK_SEAT_CAPABILITY_ALL_POINTING, TRUE,
                          cursor, NULL, prepare_inspect_func, NULL);
  g_object_unref (cursor);
  if (status == GDK_GRAB_SUCCESS)
    iw->grab_seat = seat;

  controller = GTK_EVENT_CONTROLLER (gtk_gesture_multi_press_new ());
  g_signal_connect (controller, "pressed",
		    G_CALLBACK (property_query_pressed), iw);
  gtk_widget_add_controller (iw->invisible, controller);

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "motion",
                    G_CALLBACK (property_query_motion), iw);
  gtk_widget_add_controller (iw->invisible, controller);

  controller = GTK_EVENT_CONTROLLER (gtk_event_controller_key_new ());
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (property_query_key), iw);
  gtk_widget_add_controller (iw->invisible, controller);

  gtk_grab_add (GTK_WIDGET (iw->invisible));
  deemphasize_window (GTK_WIDGET (iw));
}

static gboolean
on_flash_timeout (GtkInspectorWindow *iw)
{
  iw->flash_count++;

  gtk_highlight_overlay_set_color (GTK_HIGHLIGHT_OVERLAY (iw->flash_overlay),
                                   &(GdkRGBA) { 
                                       0.0, 0.0, 1.0,
                                       (iw && iw->flash_count % 2 == 0) ? 0.0 : 0.2
                                   });

  if (iw->flash_count == 6)
    {
      clear_flash (iw);
      iw->flash_cnx = 0;

      return G_SOURCE_REMOVE;
    }

  return G_SOURCE_CONTINUE;
}

void
gtk_inspector_flash_widget (GtkInspectorWindow *iw,
                            GtkWidget          *widget)
{
  if (!gtk_widget_get_visible (widget) || !gtk_widget_get_mapped (widget))
    return;

  if (iw->flash_cnx != 0)
    {
      g_source_remove (iw->flash_cnx);
      iw->flash_cnx = 0;
    }

  start_flash (iw, widget);
  iw->flash_cnx = g_timeout_add (150, (GSourceFunc) on_flash_timeout, iw);
}

void
gtk_inspector_window_select_widget_under_pointer (GtkInspectorWindow *iw)
{
  GdkDisplay *display;
  GdkDevice *device;
  GtkWidget *widget;

  display = gdk_display_get_default ();
  device = gdk_seat_get_pointer (gdk_display_get_default_seat (display));

  widget = find_widget_at_pointer (device);

  if (widget)
    select_widget (iw, widget);
}

/* vim: set et sw=2 ts=2: */
