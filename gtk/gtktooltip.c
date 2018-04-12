/* gtktooltip.c
 *
 * Copyright (C) 2006-2007 Imendio AB
 * Contact: Kristian Rietveld <kris@imendio.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtktooltip.h"
#include "gtktooltipprivate.h"

#include <math.h>
#include <string.h>

#include "gtkintl.h"
#include "gtkwindow.h"
#include "gtkmain.h"
#include "gtksettings.h"
#include "gtksizerequest.h"
#include "gtkstylecontext.h"
#include "gtktooltipwindowprivate.h"
#include "gtkwindowprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkaccessible.h"

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#endif
#ifdef GDK_WINDOWING_MIR
#include "mir/gdkmir.h"
#endif


/**
 * SECTION:gtktooltip
 * @Short_description: Add tips to your widgets
 * @Title: GtkTooltip
 *
 * Basic tooltips can be realized simply by using gtk_widget_set_tooltip_text()
 * or gtk_widget_set_tooltip_markup() without any explicit tooltip object.
 *
 * When you need a tooltip with a little more fancy contents, like adding an
 * image, or you want the tooltip to have different contents per #GtkTreeView
 * row or cell, you will have to do a little more work:
 * 
 * - Set the #GtkWidget:has-tooltip property to %TRUE, this will make GTK+
 *   monitor the widget for motion and related events which are needed to
 *   determine when and where to show a tooltip.
 *
 * - Connect to the #GtkWidget::query-tooltip signal.  This signal will be
 *   emitted when a tooltip is supposed to be shown. One of the arguments passed
 *   to the signal handler is a GtkTooltip object. This is the object that we
 *   are about to display as a tooltip, and can be manipulated in your callback
 *   using functions like gtk_tooltip_set_icon(). There are functions for setting
 *   the tooltip’s markup, setting an image from a named icon, or even putting in
 *   a custom widget.
 *
 *   Return %TRUE from your query-tooltip handler. This causes the tooltip to be
 *   show. If you return %FALSE, it will not be shown.
 *
 * In the probably rare case where you want to have even more control over the
 * tooltip that is about to be shown, you can set your own #GtkWindow which
 * will be used as tooltip window.  This works as follows:
 * 
 * - Set #GtkWidget:has-tooltip and connect to #GtkWidget::query-tooltip as before.
 *   Use gtk_widget_set_tooltip_window() to set a #GtkWindow created by you as
 *   tooltip window.
 *
 * - In the #GtkWidget::query-tooltip callback you can access your window using
 *   gtk_widget_get_tooltip_window() and manipulate as you wish. The semantics of
 *   the return value are exactly as before, return %TRUE to show the window,
 *   %FALSE to not show it.
 */


#define HOVER_TIMEOUT          500
#define BROWSE_TIMEOUT         60
#define BROWSE_DISABLE_TIMEOUT 500

#define GTK_TOOLTIP_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TOOLTIP, GtkTooltipClass))
#define GTK_IS_TOOLTIP_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TOOLTIP))
#define GTK_TOOLTIP_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TOOLTIP, GtkTooltipClass))

typedef struct _GtkTooltipClass   GtkTooltipClass;

struct _GtkTooltip
{
  GObject parent_instance;

  GtkWidget *window;

  GtkWindow *current_window;
  GtkWidget *keyboard_widget;

  GtkWidget *tooltip_widget;

  gdouble last_x;
  gdouble last_y;
  GdkSurface *last_surface;

  guint timeout_id;
  guint browse_mode_timeout_id;

  GdkRectangle tip_area;

  guint browse_mode_enabled : 1;
  guint keyboard_mode_enabled : 1;
  guint tip_area_set : 1;
  guint custom_was_reset : 1;
};

struct _GtkTooltipClass
{
  GObjectClass parent_class;
};

#define GTK_TOOLTIP_VISIBLE(tooltip) ((tooltip)->current_window && gtk_widget_get_visible (GTK_WIDGET((tooltip)->current_window)))

static void       gtk_tooltip_dispose              (GObject         *object);

static void       gtk_tooltip_window_hide          (GtkWidget       *widget,
						    gpointer         user_data);
static void       gtk_tooltip_display_closed       (GdkDisplay      *display,
						    gboolean         was_error,
						    GtkTooltip      *tooltip);
static void       gtk_tooltip_set_last_surface      (GtkTooltip      *tooltip,
						    GdkSurface       *surface);

static void       gtk_tooltip_handle_event_internal (GdkEventType  event_type,
                                                     GdkSurface    *surface,
                                                     gdouble       dx,
                                                     gdouble       dy);

static inline GQuark tooltip_quark (void)
{
  static GQuark quark;

  if G_UNLIKELY (quark == 0)
    quark = g_quark_from_static_string ("gdk-display-current-tooltip");
  return quark;
}

#define quark_current_tooltip tooltip_quark()

G_DEFINE_TYPE (GtkTooltip, gtk_tooltip, G_TYPE_OBJECT);

static void
gtk_tooltip_class_init (GtkTooltipClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_tooltip_dispose;
}

static void
gtk_tooltip_init (GtkTooltip *tooltip)
{
  tooltip->timeout_id = 0;
  tooltip->browse_mode_timeout_id = 0;

  tooltip->browse_mode_enabled = FALSE;
  tooltip->keyboard_mode_enabled = FALSE;

  tooltip->current_window = NULL;
  tooltip->keyboard_widget = NULL;

  tooltip->tooltip_widget = NULL;

  tooltip->last_surface = NULL;

  tooltip->window = gtk_tooltip_window_new ();
  g_signal_connect (tooltip->window, "hide",
                    G_CALLBACK (gtk_tooltip_window_hide),
                    tooltip);
}

static void
gtk_tooltip_dispose (GObject *object)
{
  GtkTooltip *tooltip = GTK_TOOLTIP (object);

  if (tooltip->timeout_id)
    {
      g_source_remove (tooltip->timeout_id);
      tooltip->timeout_id = 0;
    }

  if (tooltip->browse_mode_timeout_id)
    {
      g_source_remove (tooltip->browse_mode_timeout_id);
      tooltip->browse_mode_timeout_id = 0;
    }

  gtk_tooltip_set_custom (tooltip, NULL);
  gtk_tooltip_set_last_surface (tooltip, NULL);

  if (tooltip->window)
    {
      GdkDisplay *display;

      display = gtk_widget_get_display (tooltip->window);
      g_signal_handlers_disconnect_by_func (display,
					    gtk_tooltip_display_closed,
					    tooltip);
      gtk_widget_destroy (tooltip->window);
      tooltip->window = NULL;
    }

  G_OBJECT_CLASS (gtk_tooltip_parent_class)->dispose (object);
}

/* public API */

/**
 * gtk_tooltip_set_markup:
 * @tooltip: a #GtkTooltip
 * @markup: (allow-none): a markup string (see [Pango markup format][PangoMarkupFormat]) or %NULL
 *
 * Sets the text of the tooltip to be @markup, which is marked up
 * with the [Pango text markup language][PangoMarkupFormat].
 * If @markup is %NULL, the label will be hidden.
 */
void
gtk_tooltip_set_markup (GtkTooltip  *tooltip,
			const gchar *markup)
{
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));

  gtk_tooltip_window_set_label_markup (GTK_TOOLTIP_WINDOW (tooltip->window), markup);
}

/**
 * gtk_tooltip_set_text:
 * @tooltip: a #GtkTooltip
 * @text: (allow-none): a text string or %NULL
 *
 * Sets the text of the tooltip to be @text. If @text is %NULL, the label
 * will be hidden. See also gtk_tooltip_set_markup().
 */
void
gtk_tooltip_set_text (GtkTooltip  *tooltip,
                      const gchar *text)
{
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));

  gtk_tooltip_window_set_label_text (GTK_TOOLTIP_WINDOW (tooltip->window), text);
}

/**
 * gtk_tooltip_set_icon:
 * @tooltip: a #GtkTooltip
 * @paintable: (allow-none): a #GdkPaintable, or %NULL
 *
 * Sets the icon of the tooltip (which is in front of the text) to be
 * @paintable.  If @paintable is %NULL, the image will be hidden.
 */
void
gtk_tooltip_set_icon (GtkTooltip   *tooltip,
		      GdkPaintable *paintable)
{
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));
  g_return_if_fail (paintable == NULL || GDK_IS_PAINTABLE (paintable));

  gtk_tooltip_window_set_image_icon (GTK_TOOLTIP_WINDOW (tooltip->window), paintable);
}

/**
 * gtk_tooltip_set_icon_from_icon_name:
 * @tooltip: a #GtkTooltip
 * @icon_name: (allow-none): an icon name, or %NULL
 *
 * Sets the icon of the tooltip (which is in front of the text) to be
 * the icon indicated by @icon_name with the size indicated
 * by @size.  If @icon_name is %NULL, the image will be hidden.
 */
void
gtk_tooltip_set_icon_from_icon_name (GtkTooltip  *tooltip,
				     const gchar *icon_name)
{
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));

  gtk_tooltip_window_set_image_icon_from_name (GTK_TOOLTIP_WINDOW (tooltip->window),
                                               icon_name);
}

/**
 * gtk_tooltip_set_icon_from_gicon:
 * @tooltip: a #GtkTooltip
 * @gicon: (allow-none): a #GIcon representing the icon, or %NULL
 *
 * Sets the icon of the tooltip (which is in front of the text)
 * to be the icon indicated by @gicon with the size indicated
 * by @size. If @gicon is %NULL, the image will be hidden.
 */
void
gtk_tooltip_set_icon_from_gicon (GtkTooltip  *tooltip,
				 GIcon       *gicon)
{
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));

  gtk_tooltip_window_set_image_icon_from_gicon (GTK_TOOLTIP_WINDOW (tooltip->window),
                                                gicon);
}

/**
 * gtk_tooltip_set_custom:
 * @tooltip: a #GtkTooltip
 * @custom_widget: (allow-none): a #GtkWidget, or %NULL to unset the old custom widget.
 *
 * Replaces the widget packed into the tooltip with
 * @custom_widget. @custom_widget does not get destroyed when the tooltip goes
 * away.
 * By default a box with a #GtkImage and #GtkLabel is embedded in 
 * the tooltip, which can be configured using gtk_tooltip_set_markup() 
 * and gtk_tooltip_set_icon().
 */
void
gtk_tooltip_set_custom (GtkTooltip *tooltip,
			GtkWidget  *custom_widget)
{
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));
  g_return_if_fail (custom_widget == NULL || GTK_IS_WIDGET (custom_widget));

  /* The custom widget has been updated from the query-tooltip
   * callback, so we do not want to reset the custom widget later on.
   */
  tooltip->custom_was_reset = TRUE;

  gtk_tooltip_window_set_custom_widget (GTK_TOOLTIP_WINDOW (tooltip->window), custom_widget);
}

/**
 * gtk_tooltip_set_tip_area:
 * @tooltip: a #GtkTooltip
 * @rect: a #GdkRectangle
 *
 * Sets the area of the widget, where the contents of this tooltip apply,
 * to be @rect (in widget coordinates).  This is especially useful for
 * properly setting tooltips on #GtkTreeView rows and cells, #GtkIconViews,
 * etc.
 *
 * For setting tooltips on #GtkTreeView, please refer to the convenience
 * functions for this: gtk_tree_view_set_tooltip_row() and
 * gtk_tree_view_set_tooltip_cell().
 */
void
gtk_tooltip_set_tip_area (GtkTooltip         *tooltip,
			  const GdkRectangle *rect)
{
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));

  if (!rect)
    tooltip->tip_area_set = FALSE;
  else
    {
      tooltip->tip_area_set = TRUE;
      tooltip->tip_area = *rect;
    }
}

/**
 * gtk_tooltip_trigger_tooltip_query:
 * @display: a #GdkDisplay
 *
 * Triggers a new tooltip query on @display, in order to update the current
 * visible tooltip, or to show/hide the current tooltip.  This function is
 * useful to call when, for example, the state of the widget changed by a
 * key press.
 */
void
gtk_tooltip_trigger_tooltip_query (GdkDisplay *display)
{
  gint x, y;
  GdkSurface *surface;
  GdkDevice *device;

  /* Trigger logic as if the mouse moved */
  device = gdk_seat_get_pointer (gdk_display_get_default_seat (display));
  surface = gdk_device_get_surface_at_position (device, &x, &y);
  if (!surface)
    return;

  gtk_tooltip_handle_event_internal (GDK_MOTION_NOTIFY, surface, x, y);
}

static void
gtk_tooltip_window_hide (GtkWidget *widget,
			 gpointer   user_data)
{
  GtkTooltip *tooltip = GTK_TOOLTIP (user_data);

  gtk_tooltip_set_custom (tooltip, NULL);
}

GtkWidget *
_gtk_widget_find_at_coords (GdkSurface *surface,
                            gint       surface_x,
                            gint       surface_y,
                            gint      *widget_x,
                            gint      *widget_y)
{
  GtkWidget *event_widget;
  GtkWidget *picked_widget;

  g_return_val_if_fail (GDK_IS_SURFACE (surface), NULL);

  gdk_surface_get_user_data (surface, (void **)&event_widget);

  if (!event_widget)
    return NULL;

  picked_widget = gtk_widget_pick (event_widget, surface_x, surface_y);

  if (picked_widget != NULL)
    gtk_widget_translate_coordinates (event_widget, picked_widget, surface_x, surface_y, widget_x, widget_y);

  return picked_widget;
}

/* Ignores (x, y) on input, translates event coordinates to
 * allocation relative (x, y) of the returned widget.
 */
static GtkWidget *
find_topmost_widget_coords (GdkSurface *surface,
                            gdouble    dx,
                            gdouble    dy,
                            gint      *x,
                            gint      *y)
{
  GtkAllocation allocation;
  gint tx, ty;
  GtkWidget *tmp;

  /* Returns coordinates relative to tmp's allocation. */
  tmp = _gtk_widget_find_at_coords (surface, dx, dy, &tx, &ty);

  if (!tmp)
    return NULL;

  /* Make sure the pointer can actually be on the widget returned. */
  gtk_widget_get_allocation (tmp, &allocation);
  allocation.x = 0;
  allocation.y = 0;
  if (GTK_IS_WINDOW (tmp))
    {
      GtkBorder border;
      _gtk_window_get_shadow_width (GTK_WINDOW (tmp), &border);
      allocation.x = border.left;
      allocation.y = border.top;
      allocation.width -= border.left + border.right;
      allocation.height -= border.top + border.bottom;
    }

  if (tx < allocation.x || tx >= allocation.width ||
      ty < allocation.y || ty >= allocation.height)
    return NULL;

  if (x)
    *x = tx;
  if (y)
    *y = ty;

  return tmp;
}

static gint
tooltip_browse_mode_expired (gpointer data)
{
  GtkTooltip *tooltip;
  GdkDisplay *display;

  tooltip = GTK_TOOLTIP (data);

  tooltip->browse_mode_enabled = FALSE;
  tooltip->browse_mode_timeout_id = 0;

  if (tooltip->timeout_id)
    {
      g_source_remove (tooltip->timeout_id);
      tooltip->timeout_id = 0;
    }

  /* destroy tooltip */
  display = gtk_widget_get_display (tooltip->window);
  g_object_set_qdata (G_OBJECT (display), quark_current_tooltip, NULL);

  return FALSE;
}

static void
gtk_tooltip_display_closed (GdkDisplay *display,
			    gboolean    was_error,
			    GtkTooltip *tooltip)
{
  if (tooltip->timeout_id)
    {
      g_source_remove (tooltip->timeout_id);
      tooltip->timeout_id = 0;
    }

  g_object_set_qdata (G_OBJECT (display), quark_current_tooltip, NULL);
}

static void
gtk_tooltip_set_last_surface (GtkTooltip *tooltip,
                              GdkSurface  *surface)
{
  GtkWidget *window_widget = NULL;

  if (tooltip->last_surface == surface)
    return;

  if (tooltip->last_surface)
    g_object_remove_weak_pointer (G_OBJECT (tooltip->last_surface),
				  (gpointer *) &tooltip->last_surface);

  tooltip->last_surface = surface;

  if (tooltip->last_surface)
    g_object_add_weak_pointer (G_OBJECT (tooltip->last_surface),
			       (gpointer *) &tooltip->last_surface);

  if (surface)
    gdk_surface_get_user_data (surface, (gpointer *) &window_widget);

  if (window_widget)
    window_widget = gtk_widget_get_toplevel (window_widget);

  if (window_widget &&
      window_widget != tooltip->window &&
      gtk_widget_is_toplevel (window_widget) &&
      GTK_IS_WINDOW (window_widget))
    gtk_window_set_transient_for (GTK_WINDOW (tooltip->window),
                                  GTK_WINDOW (window_widget));
  else
    gtk_window_set_transient_for (GTK_WINDOW (tooltip->window), NULL);
}

static gboolean
gtk_tooltip_run_requery (GtkWidget  **widget,
			 GtkTooltip  *tooltip,
			 gint        *x,
			 gint        *y)
{
  gboolean has_tooltip = FALSE;
  gboolean return_value = FALSE;

  /* Reset tooltip */
  gtk_tooltip_set_markup (tooltip, NULL);
  gtk_tooltip_set_icon (tooltip, NULL);
  gtk_tooltip_set_tip_area (tooltip, NULL);

  /* See if the custom widget is again set from the query-tooltip
   * callback.
   */
  tooltip->custom_was_reset = FALSE;

  do
    {
      has_tooltip = gtk_widget_get_has_tooltip (*widget);

      if (has_tooltip)
        return_value = gtk_widget_query_tooltip (*widget, *x, *y, tooltip->keyboard_mode_enabled, tooltip);

      if (!return_value)
        {
	  GtkWidget *parent = gtk_widget_get_parent (*widget);

	  if (parent)
	    gtk_widget_translate_coordinates (*widget, parent, *x, *y, x, y);

	  *widget = parent;
	}
      else
	break;
    }
  while (*widget);

  /* If the custom widget was not reset in the query-tooltip
   * callback, we clear it here.
   */
  if (!tooltip->custom_was_reset)
    gtk_tooltip_set_custom (tooltip, NULL);

  return return_value;
}

static void
get_bounding_box (GtkWidget    *widget,
                  GdkRectangle *bounds)
{
  GtkWidget *toplevel;
  GtkAllocation allocation;
  GdkSurface *surface;
  gint x, y;
  gint w, h;
  gint x1, y1;
  gint x2, y2;
  gint x3, y3;
  gint x4, y4;

  surface = gtk_widget_get_parent_surface (widget);
  if (surface == NULL)
    surface = gtk_widget_get_surface (widget);

  gtk_widget_get_allocation (widget, &allocation);

  x = allocation.x;
  y = allocation.y;
  w = allocation.width;
  h = allocation.height;

  toplevel = gtk_widget_get_toplevel (widget);
  if (GTK_IS_WINDOW (toplevel) && !GTK_IS_WINDOW (widget))
    {
      GtkWidget *parent = gtk_widget_get_parent (widget);

      gtk_widget_translate_coordinates (parent, toplevel,
                                        x, y,
                                        &x, &y);
    }

  if (GTK_IS_WINDOW (widget))
    {
      GtkBorder border = { 0, };

      _gtk_window_get_shadow_width (GTK_WINDOW (widget), &border);
      x += border.left;
      y += border.right;
      w -= border.left + border.right;
      h -= border.top + border.bottom;
    }

  gdk_surface_get_root_coords (surface, x, y, &x1, &y1);
  gdk_surface_get_root_coords (surface, x + w, y, &x2, &y2);
  gdk_surface_get_root_coords (surface, x, y + h, &x3, &y3);
  gdk_surface_get_root_coords (surface, x + w, y + h, &x4, &y4);

#define MIN4(a,b,c,d) MIN(MIN(a,b),MIN(c,d))
#define MAX4(a,b,c,d) MAX(MAX(a,b),MAX(c,d))

  bounds->x = floor (MIN4 (x1, x2, x3, x4));
  bounds->y = floor (MIN4 (y1, y2, y3, y4));
  bounds->width = ceil (MAX4 (x1, x2, x3, x4)) - bounds->x;
  bounds->height = ceil (MAX4 (y1, y2, y3, y4)) - bounds->y;
}

static void
gtk_tooltip_position (GtkTooltip *tooltip,
		      GdkDisplay *display,
		      GtkWidget  *new_tooltip_widget)
{
  gint x, y, width, height;
  GdkMonitor *monitor;
  GdkRectangle workarea;
  guint cursor_size;
  GdkRectangle bounds;
  GtkBorder border;

#define MAX_DISTANCE 32

  gtk_widget_realize (GTK_WIDGET (tooltip->current_window));
  gtk_widget_set_visible (GTK_WIDGET (tooltip->current_window), TRUE);

  tooltip->tooltip_widget = new_tooltip_widget;

  _gtk_window_get_shadow_width (GTK_WINDOW (tooltip->current_window), &border);

  width = gtk_widget_get_allocated_width (GTK_WIDGET (tooltip->current_window)) - border.left - border.right;
  height = gtk_widget_get_allocated_height (GTK_WIDGET (tooltip->current_window)) - border.top - border.bottom;

  monitor = gdk_display_get_monitor_at_point (display, tooltip->last_x, tooltip->last_y);
  gdk_monitor_get_workarea (monitor, &workarea);

  get_bounding_box (new_tooltip_widget, &bounds);

  /* Position the tooltip */

  g_object_get (gtk_widget_get_settings (GTK_WIDGET (tooltip->current_window)),
                "gtk-cursor-theme-size", &cursor_size,
                NULL);

  /* Try below */
  x = bounds.x + bounds.width / 2 - width / 2;
  y = bounds.y + bounds.height + 4;

  if (y + height <= workarea.y + workarea.height)
    {
      if (tooltip->keyboard_mode_enabled)
        goto found;

      if (y <= tooltip->last_y + cursor_size + MAX_DISTANCE)
        {
          if (tooltip->last_x + cursor_size + MAX_DISTANCE < x)
            x = tooltip->last_x + cursor_size + MAX_DISTANCE;
          else if (x + width < tooltip->last_x - MAX_DISTANCE)
            x = tooltip->last_x - MAX_DISTANCE - width;

          goto found;
        }
   }

  /* Try above */
  x = bounds.x + bounds.width / 2 - width / 2;
  y = bounds.y - height - 4;

  if (y >= workarea.y)
    {
      if (tooltip->keyboard_mode_enabled)
        goto found;

      if (y + height >= tooltip->last_y - MAX_DISTANCE)
        {
          if (tooltip->last_x + cursor_size + MAX_DISTANCE < x)
            x = tooltip->last_x + cursor_size + MAX_DISTANCE;
          else if (x + width < tooltip->last_x - MAX_DISTANCE)
            x = tooltip->last_x - MAX_DISTANCE - width;

          goto found;
        }
    }

  /* Try right FIXME: flip on rtl ? */
  x = bounds.x + bounds.width + 4;
  y = bounds.y + bounds.height / 2 - height / 2;

  if (x + width <= workarea.x + workarea.width)
    {
      if (tooltip->keyboard_mode_enabled)
        goto found;

      if (x <= tooltip->last_x + cursor_size + MAX_DISTANCE)
        {
          if (tooltip->last_y + cursor_size + MAX_DISTANCE < y)
            y = tooltip->last_y + cursor_size + MAX_DISTANCE;
          else if (y + height < tooltip->last_y - MAX_DISTANCE)
            y = tooltip->last_y - MAX_DISTANCE - height;

          goto found;
        }
    }

  /* Try left FIXME: flip on rtl ? */
  x = bounds.x - width - 4;
  y = bounds.y + bounds.height / 2 - height / 2;

  if (x >= workarea.x)
    {
      if (tooltip->keyboard_mode_enabled)
        goto found;

      if (x + width >= tooltip->last_x - MAX_DISTANCE)
        {
          if (tooltip->last_y + cursor_size + MAX_DISTANCE < y)
            y = tooltip->last_y + cursor_size + MAX_DISTANCE;
          else if (y + height < tooltip->last_y - MAX_DISTANCE)
            y = tooltip->last_y - MAX_DISTANCE - height;

          goto found;
        }
    }

   /* Fallback */
  if (tooltip->keyboard_mode_enabled)
    {
      x = bounds.x + bounds.width / 2 - width / 2;
      y = bounds.y + bounds.height + 4;
    }
  else
    {
      /* At cursor */
      x = tooltip->last_x + cursor_size * 3 / 4;
      y = tooltip->last_y + cursor_size * 3 / 4;
    }

found:
  /* Show it */
  if (x + width > workarea.x + workarea.width)
    x -= x - (workarea.x + workarea.width) + width;
  else if (x < workarea.x)
    x = workarea.x;

  if (y + height > workarea.y + workarea.height)
    y -= y - (workarea.y + workarea.height) + height;
  else if (y < workarea.y)
    y = workarea.y;

  if (!tooltip->keyboard_mode_enabled)
    {
      /* don't pop up under the pointer */
      if (x <= tooltip->last_x && tooltip->last_x < x + width &&
          y <= tooltip->last_y && tooltip->last_y < y + height)
        y = tooltip->last_y - height - 2;
    }

#ifdef GDK_WINDOWING_WAYLAND
  /* set the transient parent on the tooltip when running with the Wayland
   * backend to allow correct positioning of the tooltip windows
   */
  if (GDK_IS_WAYLAND_DISPLAY (display))
    {
      GtkWidget *toplevel;

      toplevel = gtk_widget_get_toplevel (tooltip->tooltip_widget);
      if (GTK_IS_WINDOW (toplevel))
        gtk_window_set_transient_for (GTK_WINDOW (tooltip->current_window),
                                      GTK_WINDOW (toplevel));
    }
#endif
#ifdef GDK_WINDOWING_MIR
      /* Set the transient parent on the tooltip when running with the Mir
       * backend to allow correct positioning of the tooltip windows */
      if (GDK_IS_MIR_DISPLAY (display))
        {
          GtkWidget *toplevel;

          toplevel = gtk_widget_get_toplevel (tooltip->tooltip_widget);
          if (GTK_IS_WINDOW (toplevel))
            gtk_window_set_transient_for (GTK_WINDOW (tooltip->current_window),
                                          GTK_WINDOW (toplevel));
        }
#endif

  x -= border.left;
  y -= border.top;

  gtk_window_move (GTK_WINDOW (tooltip->current_window), x, y);
  gtk_widget_show (GTK_WIDGET (tooltip->current_window));
}

static void
gtk_tooltip_show_tooltip (GdkDisplay *display)
{
  gint x, y;
  GdkSurface *surface;
  GtkWidget *tooltip_widget;
  GtkTooltip *tooltip;
  gboolean return_value = FALSE;

  tooltip = g_object_get_qdata (G_OBJECT (display), quark_current_tooltip);

  if (tooltip->keyboard_mode_enabled)
    {
      x = y = -1;
      tooltip_widget = tooltip->keyboard_widget;
    }
  else
    {
      GdkDevice *device;
      gint tx, ty;

      surface = tooltip->last_surface;

      if (!GDK_IS_SURFACE (surface))
        return;

      device = gdk_seat_get_pointer (gdk_display_get_default_seat (display));

      gdk_surface_get_device_position (surface, device, &x, &y, NULL);

      gdk_surface_get_root_coords (surface, x, y, &tx, &ty);
      tooltip->last_x = tx;
      tooltip->last_y = ty;

      tooltip_widget = _gtk_widget_find_at_coords (surface, x, y, &x, &y);
    }

  if (!tooltip_widget)
    return;

  return_value = gtk_tooltip_run_requery (&tooltip_widget, tooltip, &x, &y);
  if (!return_value)
    return;

  if (!tooltip->current_window)
    {
      if (gtk_widget_get_tooltip_window (tooltip_widget))
        tooltip->current_window = gtk_widget_get_tooltip_window (tooltip_widget);
      else
        tooltip->current_window = GTK_WINDOW (GTK_TOOLTIP (tooltip)->window);
    }

  /* FIXME: should use tooltip->current_window iso tooltip->window */
  if (display != gtk_widget_get_display (tooltip->window))
    {
      g_signal_handlers_disconnect_by_func (display,
                                            gtk_tooltip_display_closed,
                                            tooltip);

      gtk_window_set_display (GTK_WINDOW (tooltip->window), display);

      g_signal_connect (display, "closed",
                        G_CALLBACK (gtk_tooltip_display_closed), tooltip);
    }

  gtk_tooltip_position (tooltip, display, tooltip_widget);

  /* Now a tooltip is visible again on the display, make sure browse
   * mode is enabled.
   */
  tooltip->browse_mode_enabled = TRUE;
  if (tooltip->browse_mode_timeout_id)
    {
      g_source_remove (tooltip->browse_mode_timeout_id);
      tooltip->browse_mode_timeout_id = 0;
    }
}

static void
gtk_tooltip_hide_tooltip (GtkTooltip *tooltip)
{
  if (!tooltip)
    return;

  if (tooltip->timeout_id)
    {
      g_source_remove (tooltip->timeout_id);
      tooltip->timeout_id = 0;
    }

  if (!GTK_TOOLTIP_VISIBLE (tooltip))
    return;

  tooltip->tooltip_widget = NULL;

  if (!tooltip->keyboard_mode_enabled)
    {
      guint timeout = BROWSE_DISABLE_TIMEOUT;

      /* The tooltip is gone, after (by default, should be configurable) 500ms
       * we want to turn off browse mode
       */
      if (!tooltip->browse_mode_timeout_id)
        {
	  tooltip->browse_mode_timeout_id =
	    g_timeout_add_full (0, timeout,
                                tooltip_browse_mode_expired,
                                g_object_ref (tooltip),
                                g_object_unref);
	  g_source_set_name_by_id (tooltip->browse_mode_timeout_id, "[gtk+] tooltip_browse_mode_expired");
	}
    }
  else
    {
      if (tooltip->browse_mode_timeout_id)
        {
	  g_source_remove (tooltip->browse_mode_timeout_id);
	  tooltip->browse_mode_timeout_id = 0;
	}
    }

  if (tooltip->current_window)
    {
      gtk_widget_hide (GTK_WIDGET (tooltip->current_window));
      tooltip->current_window = NULL;
    }
}

static gint
tooltip_popup_timeout (gpointer data)
{
  GdkDisplay *display;
  GtkTooltip *tooltip;

  display = GDK_DISPLAY (data);
  tooltip = g_object_get_qdata (G_OBJECT (display), quark_current_tooltip);

  /* This usually does not happen.  However, it does occur in language
   * bindings were reference counting of objects behaves differently.
   */
  if (!tooltip)
    return FALSE;

  gtk_tooltip_show_tooltip (display);

  tooltip->timeout_id = 0;

  return FALSE;
}

static void
gtk_tooltip_start_delay (GdkDisplay *display)
{
  guint timeout;
  GtkTooltip *tooltip;

  tooltip = g_object_get_qdata (G_OBJECT (display), quark_current_tooltip);

  if (!tooltip || GTK_TOOLTIP_VISIBLE (tooltip))
    return;

  if (tooltip->timeout_id)
    g_source_remove (tooltip->timeout_id);

  if (tooltip->browse_mode_enabled)
    timeout = BROWSE_TIMEOUT;
  else
    timeout = HOVER_TIMEOUT;

  tooltip->timeout_id = g_timeout_add_full (0, timeout,
                                            tooltip_popup_timeout,
                                            g_object_ref (display),
                                            g_object_unref);
  g_source_set_name_by_id (tooltip->timeout_id, "[gtk+] tooltip_popup_timeout");
}

void
_gtk_tooltip_focus_in (GtkWidget *widget)
{
  gint x = 0, y = 0;
  gboolean return_value = FALSE;
  GdkDisplay *display;
  GtkTooltip *tooltip;
  GdkDevice *device;

  /* Get current tooltip for this display */
  display = gtk_widget_get_display (widget);
  tooltip = g_object_get_qdata (G_OBJECT (display), quark_current_tooltip);

  /* Check if keyboard mode is enabled at this moment */
  if (!tooltip || !tooltip->keyboard_mode_enabled)
    return;

  device = gtk_get_current_event_device ();

  if (device && gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    device = gdk_device_get_associated_device (device);

  /* This function should be called by either a focus in event,
   * or a key binding. In either case there should be a device.
   */
  if (!device)
    return;

  if (tooltip->keyboard_widget)
    g_object_unref (tooltip->keyboard_widget);

  tooltip->keyboard_widget = g_object_ref (widget);

  gdk_surface_get_device_position (gtk_widget_get_surface (widget),
                                  device, &x, &y, NULL);

  return_value = gtk_tooltip_run_requery (&widget, tooltip, &x, &y);
  if (!return_value)
    {
      gtk_tooltip_hide_tooltip (tooltip);
      return;
    }

  if (!tooltip->current_window)
    {
      if (gtk_widget_get_tooltip_window (widget))
	tooltip->current_window = gtk_widget_get_tooltip_window (widget);
      else
	tooltip->current_window = GTK_WINDOW (GTK_TOOLTIP (tooltip)->window);
    }

  gtk_tooltip_show_tooltip (display);
}

void
_gtk_tooltip_focus_out (GtkWidget *widget)
{
  GdkDisplay *display;
  GtkTooltip *tooltip;

  /* Get current tooltip for this display */
  display = gtk_widget_get_display (widget);
  tooltip = g_object_get_qdata (G_OBJECT (display), quark_current_tooltip);

  if (!tooltip || !tooltip->keyboard_mode_enabled)
    return;

  if (tooltip->keyboard_widget)
    {
      g_object_unref (tooltip->keyboard_widget);
      tooltip->keyboard_widget = NULL;
    }

  gtk_tooltip_hide_tooltip (tooltip);
}

void
_gtk_tooltip_toggle_keyboard_mode (GtkWidget *widget)
{
  GdkDisplay *display;
  GtkTooltip *tooltip;

  display = gtk_widget_get_display (widget);
  tooltip = g_object_get_qdata (G_OBJECT (display), quark_current_tooltip);

  if (!tooltip)
    {
      tooltip = g_object_new (GTK_TYPE_TOOLTIP, NULL);
      g_object_set_qdata_full (G_OBJECT (display),
			       quark_current_tooltip,
			       tooltip,
                               g_object_unref);
      g_signal_connect (display, "closed",
			G_CALLBACK (gtk_tooltip_display_closed),
			tooltip);
    }

  tooltip->keyboard_mode_enabled ^= 1;

  if (tooltip->keyboard_mode_enabled)
    {
      tooltip->keyboard_widget = g_object_ref (widget);
      _gtk_tooltip_focus_in (widget);
    }
  else
    {
      if (tooltip->keyboard_widget)
        {
	  g_object_unref (tooltip->keyboard_widget);
	  tooltip->keyboard_widget = NULL;
	}

      gtk_tooltip_hide_tooltip (tooltip);
    }
}

void
_gtk_tooltip_hide (GtkWidget *widget)
{
  GdkDisplay *display;
  GtkTooltip *tooltip;

  display = gtk_widget_get_display (widget);
  tooltip = g_object_get_qdata (G_OBJECT (display), quark_current_tooltip);

  if (!tooltip || !GTK_TOOLTIP_VISIBLE (tooltip) || !tooltip->tooltip_widget)
    return;

  if (widget == tooltip->tooltip_widget)
    gtk_tooltip_hide_tooltip (tooltip);
}

static gboolean
tooltips_enabled (GdkEvent *event)
{
  GdkDevice *source_device;
  GdkInputSource source;

  source_device = gdk_event_get_source_device (event);

  if (!source_device)
    return FALSE;

  source = gdk_device_get_source (source_device);

  if (source != GDK_SOURCE_TOUCHSCREEN)
    return TRUE;

  return FALSE;
}

void
_gtk_tooltip_handle_event (GdkEvent *event)
{
  GdkEventType event_type;
  GdkSurface *surface;
  gdouble dx, dy;

  if (!tooltips_enabled (event))
    return;

  event_type = gdk_event_get_event_type (event);
  surface = gdk_event_get_surface (event);
  gdk_event_get_coords (event, &dx, &dy);

  gtk_tooltip_handle_event_internal (event_type, surface, dx, dy);
}

static void
gtk_tooltip_handle_event_internal (GdkEventType  event_type,
                                   GdkSurface    *surface,
                                   gdouble       dx,
                                   gdouble       dy)
{
  int x = 0, y = 0;
  GtkWidget *has_tooltip_widget;
  GdkDisplay *display;
  GtkTooltip *current_tooltip;

  has_tooltip_widget = find_topmost_widget_coords (surface, dx, dy, &x, &y);
  display = gdk_surface_get_display (surface);
  current_tooltip = g_object_get_qdata (G_OBJECT (display), quark_current_tooltip);

  if (current_tooltip)
    gtk_tooltip_set_last_surface (current_tooltip, surface);

  if (current_tooltip && current_tooltip->keyboard_mode_enabled)
    {
      gboolean return_value;

      has_tooltip_widget = current_tooltip->keyboard_widget;
      if (!has_tooltip_widget)
	return;

      return_value = gtk_tooltip_run_requery (&has_tooltip_widget,
					      current_tooltip,
					      &x, &y);

      if (!return_value)
	gtk_tooltip_hide_tooltip (current_tooltip);
      else
	gtk_tooltip_start_delay (display);

      return;
    }

  /* Hide the tooltip when there's no new tooltip widget */
  if (!has_tooltip_widget)
    {
      if (current_tooltip)
	gtk_tooltip_hide_tooltip (current_tooltip);

      return;
    }

  switch ((guint) event_type)
    {
      case GDK_BUTTON_PRESS:
      case GDK_KEY_PRESS:
      case GDK_DRAG_ENTER:
      case GDK_GRAB_BROKEN:
      case GDK_SCROLL:
	gtk_tooltip_hide_tooltip (current_tooltip);
	break;

      case GDK_MOTION_NOTIFY:
      case GDK_ENTER_NOTIFY:
      case GDK_LEAVE_NOTIFY:
	if (current_tooltip)
	  {
	    gboolean tip_area_set;
	    GdkRectangle tip_area;
	    gboolean hide_tooltip;

	    tip_area_set = current_tooltip->tip_area_set;
	    tip_area = current_tooltip->tip_area;

	    gtk_tooltip_run_requery (&has_tooltip_widget,
                                     current_tooltip,
                                     &x, &y);

	    /* Leave notify should override the query function */
	    hide_tooltip = (event_type == GDK_LEAVE_NOTIFY);

	    /* Is the pointer above another widget now? */
	    if (GTK_TOOLTIP_VISIBLE (current_tooltip))
	      hide_tooltip |= has_tooltip_widget != current_tooltip->tooltip_widget;

	    /* Did the pointer move out of the previous "context area"? */
	    if (tip_area_set)
	      hide_tooltip |= (x <= tip_area.x
			       || x >= tip_area.x + tip_area.width
			       || y <= tip_area.y
			       || y >= tip_area.y + tip_area.height);

	    if (hide_tooltip)
	      gtk_tooltip_hide_tooltip (current_tooltip);
	    else
	      gtk_tooltip_start_delay (display);
	  }
	else
	  {
	    /* Need a new tooltip for this display */
	    current_tooltip = g_object_new (GTK_TYPE_TOOLTIP, NULL);
	    g_object_set_qdata_full (G_OBJECT (display),
				     quark_current_tooltip,
				     current_tooltip,
                                     g_object_unref);
	    g_signal_connect (display, "closed",
			      G_CALLBACK (gtk_tooltip_display_closed),
			      current_tooltip);

	    gtk_tooltip_set_last_surface (current_tooltip, surface);

	    gtk_tooltip_start_delay (display);
	  }
	break;

      default:
	break;
    }
}
