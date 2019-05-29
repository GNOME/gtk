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
#include "gtkwidgetprivate.h"
#include "gtkeventcontrollermotion.h"
#include "gtkeventcontrollerkey.h"
#include "gtknative.h"

static GtkWidget *
find_widget_at_pointer (GdkDevice *device)
{
  GtkWidget *widget = NULL;
  GdkSurface *pointer_surface;

  pointer_surface = gdk_device_get_surface_at_position (device, NULL, NULL);

  if (pointer_surface)
    widget = gtk_native_get_for_surface (pointer_surface);

  if (widget)
    {
      double x, y;

      gdk_surface_get_device_position (gtk_native_get_surface (GTK_NATIVE (widget)),
                                       device, &x, &y, NULL);

      widget = gtk_widget_pick (widget, x, y, GTK_PICK_INSENSITIVE|GTK_PICK_NON_TARGETABLE);
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
on_inspect_widget (GtkInspectorWindow *iw,
                   GdkEvent           *event)
{
  GtkWidget *widget;

  gdk_surface_raise (gtk_native_get_surface (GTK_NATIVE (iw)));

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

  if (gtk_widget_get_root (widget) == GTK_ROOT (iw))
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
    gdk_surface_lower (gtk_native_get_surface (GTK_NATIVE (window)));
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
    gdk_surface_raise (gtk_native_get_surface (GTK_NATIVE (window)));
}

static gboolean handle_event (GtkInspectorWindow *iw, GdkEvent *event);

static void
handle_button_event (GtkInspectorWindow *iw,
                     GdkEvent           *event)
{
  g_signal_handlers_disconnect_by_func (iw, handle_event, NULL);
  reemphasize_window (GTK_WIDGET (iw));
  on_inspect_widget (iw, event);
}

static void
handle_motion_event (GtkInspectorWindow *iw,
                     GdkEvent           *event)
{
  on_highlight_widget (NULL, event, iw);
}

static void
handle_key_event (GtkInspectorWindow *iw,
                  GdkEvent           *event)
{
  guint keyval = 0;

  gdk_event_get_keyval (event, &keyval);
  if (keyval == GDK_KEY_Escape)
    {
      g_signal_handlers_disconnect_by_func (iw, handle_event, NULL);
      reemphasize_window (GTK_WIDGET (iw));
      clear_flash (iw);
    }
}

static gboolean
handle_event (GtkInspectorWindow *iw, GdkEvent *event)
{
  switch ((int)gdk_event_get_event_type (event))
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      handle_key_event (iw, event);
      break;

    case GDK_MOTION_NOTIFY:
      handle_motion_event (iw, event);
      break;

    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      handle_button_event (iw, event);
      break;

    default:;
    }

  return TRUE;
}

void
gtk_inspector_on_inspect (GtkWidget          *button,
                          GtkInspectorWindow *iw)
{
  g_signal_connect (iw, "event", G_CALLBACK (handle_event), NULL);
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
