/*
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
 *
 * Authors: Cody Russell <crussell@canonical.com>
 *          Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"

#include "gtkoffscreenwindow.h"
#include "gtkwidgetprivate.h"
#include "gtkcontainerprivate.h"
#include "gtkprivate.h"

/**
 * SECTION:gtkoffscreenwindow
 * @short_description: A toplevel to manage offscreen rendering of child widgets
 * @title: GtkOffscreenWindow
 *
 * GtkOffscreenWindow is strictly intended to be used for obtaining
 * snapshots of widgets that are not part of a normal widget hierarchy.
 * Since #GtkOffscreenWindow is a toplevel widget you cannot obtain
 * snapshots of a full window with it since you cannot pack a toplevel
 * widget in another toplevel.
 *
 * The idea is to take a widget and manually set the state of it,
 * add it to a GtkOffscreenWindow and then retrieve the snapshot
 * as a #cairo_surface_t or #GdkPixbuf.
 *
 * GtkOffscreenWindow derives from #GtkWindow only as an implementation
 * detail.  Applications should not use any API specific to #GtkWindow
 * to operate on this object.  It should be treated as a #GtkBin that
 * has no parent widget.
 *
 * When contained offscreen widgets are redrawn, GtkOffscreenWindow
 * will emit a #GtkWidget::damage-event signal.
 */

G_DEFINE_TYPE (GtkOffscreenWindow, gtk_offscreen_window, GTK_TYPE_WINDOW);

static void
gtk_offscreen_window_get_preferred_width (GtkWidget *widget,
					  gint      *minimum,
					  gint      *natural)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkWidget *child;
  gint border_width;
  gint default_width;

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  *minimum = border_width * 2;
  *natural = border_width * 2;

  child = gtk_bin_get_child (bin);

  if (child != NULL && gtk_widget_get_visible (child))
    {
      gint child_min, child_nat;

      gtk_widget_get_preferred_width (child, &child_min, &child_nat);

      *minimum += child_min;
      *natural += child_nat;
    }

  gtk_window_get_default_size (GTK_WINDOW (widget),
                               &default_width, NULL);

  *minimum = MAX (*minimum, default_width);
  *natural = MAX (*natural, default_width);
}

static void
gtk_offscreen_window_get_preferred_height (GtkWidget *widget,
					   gint      *minimum,
					   gint      *natural)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkWidget *child;
  gint border_width;
  gint default_height;

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  *minimum = border_width * 2;
  *natural = border_width * 2;

  child = gtk_bin_get_child (bin);

  if (child != NULL && gtk_widget_get_visible (child))
    {
      gint child_min, child_nat;

      gtk_widget_get_preferred_height (child, &child_min, &child_nat);

      *minimum += child_min;
      *natural += child_nat;
    }

  gtk_window_get_default_size (GTK_WINDOW (widget),
                               NULL, &default_height);

  *minimum = MAX (*minimum, default_height);
  *natural = MAX (*natural, default_height);
}

static void
gtk_offscreen_window_size_allocate (GtkWidget *widget,
                                    GtkAllocation *allocation)
{
  GtkBin *bin = GTK_BIN (widget);
  GtkWidget *child;
  gint border_width;

  gtk_widget_set_allocation (widget, allocation);

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (gtk_widget_get_window (widget),
                            allocation->x,
                            allocation->y,
                            allocation->width,
                            allocation->height);

  child = gtk_bin_get_child (bin);

  if (child != NULL && gtk_widget_get_visible (child))
    {
      GtkAllocation  child_alloc;

      child_alloc.x = border_width;
      child_alloc.y = border_width;
      child_alloc.width = allocation->width - 2 * border_width;
      child_alloc.height = allocation->height - 2 * border_width;

      gtk_widget_size_allocate (child, &child_alloc);
    }

  gtk_widget_queue_draw (widget);
}

static void
gtk_offscreen_window_realize (GtkWidget *widget)
{
  GtkAllocation allocation;
  GtkBin *bin;
  GtkWidget *child;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  bin = GTK_BIN (widget);

  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);

  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.window_type = GDK_WINDOW_OFFSCREEN;
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.wclass = GDK_INPUT_OUTPUT;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  window = gdk_window_new (gtk_widget_get_parent_window (widget),
                           &attributes, attributes_mask);
  gtk_widget_set_window (widget, window);
  gtk_widget_register_window (widget, window);

  child = gtk_bin_get_child (bin);
  if (child)
    gtk_widget_set_parent_window (child, window);

  gtk_style_context_set_background (gtk_widget_get_style_context (widget),
                                    window);
}

static void
gtk_offscreen_window_resize (GtkWidget *widget)
{
  GtkAllocation allocation = { 0, 0 };
  GtkRequisition requisition;

  gtk_widget_get_preferred_size (widget, &requisition, NULL);

  allocation.width  = requisition.width;
  allocation.height = requisition.height;
  gtk_widget_size_allocate (widget, &allocation);
}

static void
move_focus (GtkWidget       *widget,
            GtkDirectionType dir)
{
  gtk_widget_child_focus (widget, dir);

  if (!gtk_container_get_focus_child (GTK_CONTAINER (widget)))
    gtk_window_set_focus (GTK_WINDOW (widget), NULL);
}

static void
gtk_offscreen_window_show (GtkWidget *widget)
{
  gboolean need_resize;

  _gtk_widget_set_visible_flag (widget, TRUE);

  need_resize = _gtk_widget_get_alloc_needed (widget) || !gtk_widget_get_realized (widget);

  if (need_resize)
    gtk_offscreen_window_resize (widget);

  gtk_widget_map (widget);

  /* Try to make sure that we have some focused widget */
  if (!gtk_window_get_focus (GTK_WINDOW (widget)))
    move_focus (widget, GTK_DIR_TAB_FORWARD);
}

static void
gtk_offscreen_window_hide (GtkWidget *widget)
{
  _gtk_widget_set_visible_flag (widget, FALSE);
  gtk_widget_unmap (widget);
}

static void
gtk_offscreen_window_check_resize (GtkContainer *container)
{
  GtkWidget *widget = GTK_WIDGET (container);

  if (gtk_widget_get_visible (widget))
    gtk_offscreen_window_resize (widget);
}

static void
gtk_offscreen_window_class_init (GtkOffscreenWindowClass *class)
{
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  widget_class = GTK_WIDGET_CLASS (class);
  container_class = GTK_CONTAINER_CLASS (class);

  widget_class->realize = gtk_offscreen_window_realize;
  widget_class->show = gtk_offscreen_window_show;
  widget_class->hide = gtk_offscreen_window_hide;
  widget_class->get_preferred_width = gtk_offscreen_window_get_preferred_width;
  widget_class->get_preferred_height = gtk_offscreen_window_get_preferred_height;
  widget_class->size_allocate = gtk_offscreen_window_size_allocate;

  container_class->check_resize = gtk_offscreen_window_check_resize;
}

static void
gtk_offscreen_window_init (GtkOffscreenWindow *window)
{
}

/* --- functions --- */
/**
 * gtk_offscreen_window_new:
 *
 * Creates a toplevel container widget that is used to retrieve
 * snapshots of widgets without showing them on the screen.
 *
 * Returns: A pointer to a #GtkWidget
 *
 * Since: 2.20
 */
GtkWidget *
gtk_offscreen_window_new (void)
{
  return g_object_new (gtk_offscreen_window_get_type (), NULL);
}

/**
 * gtk_offscreen_window_get_surface:
 * @offscreen: the #GtkOffscreenWindow contained widget.
 *
 * Retrieves a snapshot of the contained widget in the form of
 * a #cairo_surface_t.  If you need to keep this around over window
 * resizes then you should add a reference to it.
 *
 * Returns: (transfer none): A #cairo_surface_t pointer to the offscreen
 *     surface, or %NULL.
 *
 * Since: 2.20
 */
cairo_surface_t *
gtk_offscreen_window_get_surface (GtkOffscreenWindow *offscreen)
{
  g_return_val_if_fail (GTK_IS_OFFSCREEN_WINDOW (offscreen), NULL);

  return gdk_offscreen_window_get_surface (gtk_widget_get_window (GTK_WIDGET (offscreen)));
}

/**
 * gtk_offscreen_window_get_pixbuf:
 * @offscreen: the #GtkOffscreenWindow contained widget.
 *
 * Retrieves a snapshot of the contained widget in the form of
 * a #GdkPixbuf.  This is a new pixbuf with a reference count of 1,
 * and the application should unreference it once it is no longer
 * needed.
 *
 * Returns: (transfer full): A #GdkPixbuf pointer, or %NULL.
 *
 * Since: 2.20
 */
GdkPixbuf *
gtk_offscreen_window_get_pixbuf (GtkOffscreenWindow *offscreen)
{
  cairo_surface_t *surface;
  GdkPixbuf *pixbuf = NULL;
  GdkWindow *window;

  g_return_val_if_fail (GTK_IS_OFFSCREEN_WINDOW (offscreen), NULL);

  window = gtk_widget_get_window (GTK_WIDGET (offscreen));
  surface = gdk_offscreen_window_get_surface (window);

  if (surface != NULL)
    {
      pixbuf = gdk_pixbuf_get_from_surface (surface,
                                            0, 0,
                                            gdk_window_get_width (window),
                                            gdk_window_get_height (window));
    }

  return pixbuf;
}
