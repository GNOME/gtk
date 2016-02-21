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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"
#include "gtkdrawingarea.h"
#include "gtkintl.h"
#include "gtkstylecontext.h"


/**
 * SECTION:gtkdrawingarea
 * @Short_description: A widget for custom user interface elements
 * @Title: GtkDrawingArea
 * @See_also: #GtkImage
 *
 * The #GtkDrawingArea widget is used for creating custom user interface
 * elements. It’s essentially a blank widget; you can draw on it. After
 * creating a drawing area, the application may want to connect to:
 *
 * - Mouse and button press signals to respond to input from
 *   the user. (Use gtk_widget_add_events() to enable events
 *   you wish to receive.)
 *
 * - The #GtkWidget::realize signal to take any necessary actions
 *   when the widget is instantiated on a particular display.
 *   (Create GDK resources in response to this signal.)
 *
 * - The #GtkWidget::size-allocate signal to take any necessary
 *   actions when the widget changes size.
 *
 * - The #GtkWidget::draw signal to handle redrawing the
 *   contents of the widget.
 *
 * The following code portion demonstrates using a drawing
 * area to display a circle in the normal widget foreground
 * color.
 *
 * Note that GDK automatically clears the exposed area before sending
 * the expose event, and that drawing is implicitly clipped to the exposed
 * area. If you want to have a theme-provided background, you need
 * to call gtk_render_background() in your ::draw method.
 *
 * ## Simple GtkDrawingArea usage
 *
 * |[<!-- language="C" -->
 * gboolean
 * draw_callback (GtkWidget *widget, cairo_t *cr, gpointer data)
 * {
 *   guint width, height;
 *   GdkRGBA color;
 *   GtkStyleContext *context;
 *
 *   context = gtk_widget_get_style_context (widget);
 *
 *   width = gtk_widget_get_allocated_width (widget);
 *   height = gtk_widget_get_allocated_height (widget);
 *
 *   gtk_render_background (context, cr, 0, 0, width, height);
 *
 *   cairo_arc (cr,
 *              width / 2.0, height / 2.0,
 *              MIN (width, height) / 2.0,
 *              0, 2 * G_PI);
 *
 *   gtk_style_context_get_color (context,
 *                                gtk_style_context_get_state (context),
 *                                &color);
 *   gdk_cairo_set_source_rgba (cr, &color);
 *
 *   cairo_fill (cr);
 *
 *  return FALSE;
 * }
 * [...]
 *   GtkWidget *drawing_area = gtk_drawing_area_new ();
 *   gtk_widget_set_size_request (drawing_area, 100, 100);
 *   g_signal_connect (G_OBJECT (drawing_area), "draw",
 *                     G_CALLBACK (draw_callback), NULL);
 * ]|
 *
 * Draw signals are normally delivered when a drawing area first comes
 * onscreen, or when it’s covered by another window and then uncovered.
 * You can also force an expose event by adding to the “damage region”
 * of the drawing area’s window; gtk_widget_queue_draw_area() and
 * gdk_window_invalidate_rect() are equally good ways to do this.
 * You’ll then get a draw signal for the invalid region.
 *
 * The available routines for drawing are documented on the
 * [GDK Drawing Primitives][gdk3-Cairo-Interaction] page
 * and the cairo documentation.
 *
 * To receive mouse events on a drawing area, you will need to enable
 * them with gtk_widget_add_events(). To receive keyboard events, you
 * will need to set the “can-focus” property on the drawing area, and you
 * should probably draw some user-visible indication that the drawing
 * area is focused. Use gtk_widget_has_focus() in your expose event
 * handler to decide whether to draw the focus indicator. See
 * gtk_render_focus() for one way to draw focus.
 */

static void gtk_drawing_area_realize       (GtkWidget           *widget);
static void gtk_drawing_area_style_updated (GtkWidget           *widget);
static void gtk_drawing_area_size_allocate (GtkWidget           *widget,
                                            GtkAllocation       *allocation);
static void gtk_drawing_area_send_configure (GtkDrawingArea     *darea);

G_DEFINE_TYPE (GtkDrawingArea, gtk_drawing_area, GTK_TYPE_WIDGET)

static void
gtk_drawing_area_class_init (GtkDrawingAreaClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  widget_class->realize = gtk_drawing_area_realize;
  widget_class->size_allocate = gtk_drawing_area_size_allocate;
  widget_class->style_updated = gtk_drawing_area_style_updated;

  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_DRAWING_AREA);
}

static void
gtk_drawing_area_init (GtkDrawingArea *darea)
{
}

/**
 * gtk_drawing_area_new:
 *
 * Creates a new drawing area.
 *
 * Returns: a new #GtkDrawingArea
 */
GtkWidget*
gtk_drawing_area_new (void)
{
  return g_object_new (GTK_TYPE_DRAWING_AREA, NULL);
}

static void
set_background (GtkWidget *widget)
{
  if (gtk_widget_get_realized (widget) &&
      gtk_widget_get_has_window (widget))
    {
      /* We still need to call gtk_style_context_set_background() here for
       * GtkDrawingArea, since clients expect backgrounds set on it (e.g. through
       * gtk_widget_override_background_color) to be available even when they
       * don't chain up from draw().
       * This should be revisited next time we have a major API break.
       */
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_style_context_set_background (gtk_widget_get_style_context (widget),
                                        gtk_widget_get_window (widget));
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
}

static void
gtk_drawing_area_style_updated (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_drawing_area_parent_class)->style_updated (widget);

  set_background (widget);
}

static void
gtk_drawing_area_realize (GtkWidget *widget)
{
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  if (!gtk_widget_get_has_window (widget))
    {
      GTK_WIDGET_CLASS (gtk_drawing_area_parent_class)->realize (widget);
    }
  else
    {
      gtk_widget_set_realized (widget, TRUE);

      gtk_widget_get_allocation (widget, &allocation);

      attributes.window_type = GDK_WINDOW_CHILD;
      attributes.x = allocation.x;
      attributes.y = allocation.y;
      attributes.width = allocation.width;
      attributes.height = allocation.height;
      attributes.wclass = GDK_INPUT_OUTPUT;
      attributes.visual = gtk_widget_get_visual (widget);
      attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;

      attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

      window = gdk_window_new (gtk_widget_get_parent_window (widget),
                               &attributes, attributes_mask);
      gtk_widget_register_window (widget, window);
      gtk_widget_set_window (widget, window);

      set_background (widget);
    }

  gtk_drawing_area_send_configure (GTK_DRAWING_AREA (widget));
}

static void
gtk_drawing_area_size_allocate (GtkWidget     *widget,
                                GtkAllocation *allocation)
{
  g_return_if_fail (GTK_IS_DRAWING_AREA (widget));
  g_return_if_fail (allocation != NULL);

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    {
      if (gtk_widget_get_has_window (widget))
        gdk_window_move_resize (gtk_widget_get_window (widget),
                                allocation->x, allocation->y,
                                allocation->width, allocation->height);

      gtk_drawing_area_send_configure (GTK_DRAWING_AREA (widget));
    }
}

static void
gtk_drawing_area_send_configure (GtkDrawingArea *darea)
{
  GtkAllocation allocation;
  GtkWidget *widget;
  GdkEvent *event = gdk_event_new (GDK_CONFIGURE);

  widget = GTK_WIDGET (darea);
  gtk_widget_get_allocation (widget, &allocation);

  event->configure.window = g_object_ref (gtk_widget_get_window (widget));
  event->configure.send_event = TRUE;
  event->configure.x = allocation.x;
  event->configure.y = allocation.y;
  event->configure.width = allocation.width;
  event->configure.height = allocation.height;

  gtk_widget_event (widget, event);
  gdk_event_free (event);
}
