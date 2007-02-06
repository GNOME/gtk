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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "gtktooltip.h"
#include "gtkintl.h"
#include "gtkwindow.h"
#include "gtkmain.h"
#include "gtklabel.h"
#include "gtkimage.h"
#include "gtkhbox.h"
#include "gtkalignment.h"

#include <string.h>


#define GTK_TOOLTIP(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TOOLTIP, GtkTooltip))
#define GTK_TOOLTIP_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TOOLTIP, GtkTooltipClass))
#define GTK_IS_TOOLTIP(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TOOLTIP))
#define GTK_IS_TOOLTIP_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TOOLTIP))
#define GTK_TOOLTIP_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TOOLTIP, GtkTooltipClass))

typedef struct _GtkTooltipClass   GtkTooltipClass;

struct _GtkTooltip
{
  GObject parent_instance;

  GtkWidget *window;
  GtkWidget *alignment;
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *custom_widget;

  GtkWindow *current_window;
  GtkWidget *keyboard_widget;

  GtkWidget *tooltip_widget;
  GdkWindow *toplevel_window;

  gdouble last_x;
  gdouble last_y;
  GdkWindow *last_window;

  guint timeout_id;
  guint browse_mode_timeout_id;

  guint browse_mode_enabled : 1;
  guint keyboard_mode_enabled : 1;
};

struct _GtkTooltipClass
{
  GObjectClass parent_class;
};

#define GTK_TOOLTIP_VISIBLE(tooltip) ((tooltip)->current_window && GTK_WIDGET_VISIBLE ((tooltip)->current_window))


static void       gtk_tooltip_class_init           (GtkTooltipClass *klass);
static void       gtk_tooltip_init                 (GtkTooltip      *tooltip);
static void       gtk_tooltip_finalize             (GObject         *object);

static gboolean   gtk_tooltip_paint_window         (GtkTooltip      *tooltip);
static void       gtk_tooltip_window_hide          (GtkWidget       *widget,
						    gpointer         user_data);
static void       gtk_tooltip_display_closed       (GdkDisplay      *display,
						    gboolean         was_error,
						    GtkTooltip      *tooltip);


G_DEFINE_TYPE (GtkTooltip, gtk_tooltip, G_TYPE_OBJECT);

static void
gtk_tooltip_class_init (GtkTooltipClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_tooltip_finalize;
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
  tooltip->toplevel_window = NULL;

  tooltip->last_window = NULL;

  tooltip->window = g_object_ref (gtk_window_new (GTK_WINDOW_POPUP));
  gtk_window_set_type_hint (GTK_WINDOW (tooltip->window),
			    GDK_WINDOW_TYPE_HINT_TOOLTIP);
  gtk_widget_set_app_paintable (tooltip->window, TRUE);
  gtk_window_set_resizable (GTK_WINDOW (tooltip->window), FALSE);
  gtk_widget_set_name (tooltip->window, "gtk-tooltip");
  g_signal_connect (tooltip->window, "hide",
		    G_CALLBACK (gtk_tooltip_window_hide), tooltip);

  tooltip->alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding (GTK_ALIGNMENT (tooltip->alignment),
			     tooltip->window->style->ythickness,
			     tooltip->window->style->ythickness,
			     tooltip->window->style->xthickness,
			     tooltip->window->style->xthickness);
  gtk_container_add (GTK_CONTAINER (tooltip->window), tooltip->alignment);
  gtk_widget_show (tooltip->alignment);

  g_signal_connect_swapped (tooltip->window, "expose_event",
			    G_CALLBACK (gtk_tooltip_paint_window), tooltip);

  tooltip->box = gtk_hbox_new (FALSE, tooltip->window->style->xthickness);
  gtk_container_add (GTK_CONTAINER (tooltip->alignment), tooltip->box);
  gtk_widget_show (tooltip->box);

  tooltip->image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (tooltip->box), tooltip->image,
		      FALSE, FALSE, 0);

  tooltip->label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (tooltip->box), tooltip->label,
		      FALSE, FALSE, 0);

  tooltip->custom_widget = NULL;
}

static void
gtk_tooltip_finalize (GObject *object)
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

  if (tooltip->window)
    {
      GdkDisplay *display;

      display = gtk_widget_get_display (tooltip->window);
      g_signal_handlers_disconnect_by_func (display,
					    gtk_tooltip_display_closed,
					    tooltip);
      gtk_widget_destroy (tooltip->window);
    }

  G_OBJECT_CLASS (gtk_tooltip_parent_class)->finalize (object);
}

/* public API */

/**
 * gtk_tooltip_set_markup:
 * @label: a #GtkTooltip
 * @markup: a markup string (see <link linkend="PangoMarkupFormat">Pango markup format</link>) or %NULL
 *
 * Sets the text of the tooltip to be @str, which is marked up
 * with the <link
 * linkend="PangoMarkupFormat">Pango text markup language</link>.
 * If @markup is %NULL, the label will be hidden.
 *
 * Since: 2.12
 */
void
gtk_tooltip_set_markup (GtkTooltip  *tooltip,
			const gchar *markup)
{
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));

  gtk_label_set_markup (GTK_LABEL (tooltip->label), markup);

  if (markup)
    gtk_widget_show (tooltip->label);
  else
    gtk_widget_hide (tooltip->label);
}

/**
 * gtk_tooltip_set_icon:
 * @tooltip: a #GtkTooltip
 * @pixbuf: a #GdkPixbuf, or %NULL
 *
 * Sets the icon of the tooltip (which is in front of the text) to be
 * @pixbuf.  If @pixbuf is %NULL, the image will be hidden.
 *
 * Since: 2.12
 */
void
gtk_tooltip_set_icon (GtkTooltip *tooltip,
		      GdkPixbuf  *pixbuf)
{
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));
  if (pixbuf)
    g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

  gtk_image_set_from_pixbuf (GTK_IMAGE (tooltip->image), pixbuf);

  if (pixbuf)
    gtk_widget_show (tooltip->image);
  else
    gtk_widget_hide (tooltip->image);
}

/**
 * gtk_tooltip_set_icon_from_stock:
 * @tooltip: a #GtkTooltip
 * @stock_id: a stock icon name, or %NULL
 * @size: a stock icon size
 *
 * Sets the icon of the tooltip (which is in front of the text) to be
 * the stock item indicated by @stock_id with the size indicated
 * by @size.  If @stock_id is %NULL, the image will be hidden.
 *
 * Since: 2.12
 */
void
gtk_tooltip_set_icon_from_stock (GtkTooltip  *tooltip,
				 const gchar *stock_id,
				 GtkIconSize  size)
{
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));

  gtk_image_set_from_stock (GTK_IMAGE (tooltip->image), stock_id, size);

  if (stock_id)
    gtk_widget_show (tooltip->image);
  else
    gtk_widget_hide (tooltip->image);
}

/**
 * gtk_tooltip_set_custom:
 * tooltip: a #GtkTooltip
 * custom_widget: a #GtkWidget
 *
 * Replaces the widget packed into the tooltip with @custom_widget.  By
 * default a box with a #GtkImage and #GtkLabel is embedded in the tooltip,
 * which can be configured using gtk_tooltip_set_markup() and
 * gtk_tooltip_set_icon().
 *
 * Since: 2.12
 */
void
gtk_tooltip_set_custom (GtkTooltip *tooltip,
			GtkWidget  *custom_widget)
{
  g_return_if_fail (GTK_IS_TOOLTIP (tooltip));
  if (custom_widget)
    g_return_if_fail (GTK_IS_WIDGET (custom_widget));

  if (tooltip->custom_widget)
    {
      gtk_container_remove (GTK_CONTAINER (tooltip->box),
			    tooltip->custom_widget);
      g_object_unref (tooltip->custom_widget);
    }

  if (custom_widget)
    {
      tooltip->custom_widget = g_object_ref (custom_widget);

      gtk_container_add (GTK_CONTAINER (tooltip->box), custom_widget);
      gtk_widget_show (custom_widget);
    }
  else
    tooltip->custom_widget = NULL;
}

/**
 * gtk_tooltip_trigger_tooltip_query:
 * @display: a #GtkDisplay
 *
 * Triggers a new tooltip query on @display, in order to update the current
 * visible tooltip, or to show/hide the current tooltip.  This function is
 * useful to call when, for example, the state of the widget changed by a
 * key press.
 *
 * Since: 2.12
 */
void
gtk_tooltip_trigger_tooltip_query (GdkDisplay *display)
{
  gint x, y;
  GdkWindow *window;
  GdkEvent event;

  /* Trigger logic as if the mouse moved */
  window = gdk_display_get_window_at_pointer (display, &x, &y);
  if (!window)
    return;

  event.type = GDK_MOTION_NOTIFY;
  event.motion.window = window;
  event.motion.x = x;
  event.motion.y = y;
  event.motion.is_hint = FALSE;

  _gtk_tooltip_handle_event (&event);
}

/* private functions */

static void
gtk_tooltip_reset (GtkTooltip *tooltip)
{
  gtk_tooltip_set_markup (tooltip, NULL);
  gtk_tooltip_set_icon (tooltip, NULL);
  gtk_tooltip_set_custom (tooltip, NULL);
}

static gboolean
gtk_tooltip_paint_window (GtkTooltip *tooltip)
{
  GtkRequisition req;

  gtk_widget_size_request (tooltip->window, &req);
  gtk_paint_flat_box (tooltip->window->style,
		      tooltip->window->window,
		      GTK_STATE_NORMAL,
		      GTK_SHADOW_OUT,
		      NULL,
		      tooltip->window,
		      "tooltip",
		      0, 0,
		      tooltip->window->allocation.width,
		      tooltip->window->allocation.height);

  return FALSE;
}

static void
gtk_tooltip_window_hide (GtkWidget *widget,
			 gpointer   user_data)
{
  GtkTooltip *tooltip = GTK_TOOLTIP (user_data);

  if (tooltip->custom_widget)
    gtk_tooltip_set_custom (tooltip, NULL);
}

/* event handling, etc */

struct ChildLocation
{
  GtkWidget *child;
  GtkWidget *container;

  gint x;
  gint y;
};

static void
child_location_foreach (GtkWidget *child,
			gpointer   data)
{
  struct ChildLocation *child_loc = data;

  if (!child_loc->child)
    {
      gint x, y;

      gtk_widget_translate_coordinates (child_loc->container, child,
					child_loc->x, child_loc->y,
					&x, &y);

      if (x >= 0 && x < child->allocation.width
	  && y >= 0 && y < child->allocation.height)
        {
	  if (GTK_IS_CONTAINER (child))
	    {
	      struct ChildLocation tmp = { NULL, NULL, 0, 0 };

	      tmp.x = x;
	      tmp.y = y;
	      tmp.container = child;

	      gtk_container_foreach (GTK_CONTAINER (child),
				     child_location_foreach, &tmp);

	      if (tmp.child)
		child_loc->child = tmp.child;
	      else
		child_loc->child = child;
	    }
	  else
	    child_loc->child = child;
	}
    }
}

static void
window_to_alloc (GtkWidget *dest_widget,
		 gint       src_x,
		 gint       src_y,
		 gint      *dest_x,
		 gint      *dest_y)
{
  /* Translate from window relative to allocation relative */
  if (!GTK_WIDGET_NO_WINDOW (dest_widget) && dest_widget->parent)
    {
      gint wx, wy;
      gdk_window_get_position (dest_widget->window, &wx, &wy);

      src_x += wx - dest_widget->allocation.x;
      src_y += wy - dest_widget->allocation.y;
    }
  else
    {
      src_x -= dest_widget->allocation.x;
      src_y -= dest_widget->allocation.y;
    }

  if (dest_x)
    *dest_x = src_x;
  if (dest_y)
    *dest_y = src_y;
}

static GtkWidget *
find_widget_under_pointer (GdkWindow *window,
			   gint      *x,
			   gint      *y)
{
  GtkWidget *event_widget;
  struct ChildLocation child_loc = { NULL, NULL, 0, 0 };

  child_loc.x = *x;
  child_loc.y = *y;

  gdk_window_get_user_data (window, (void **)&event_widget);
  if (GTK_IS_CONTAINER (event_widget))
    {
      window_to_alloc (event_widget,
		       child_loc.x, child_loc.y,
		       &child_loc.x, &child_loc.y);

      child_loc.container = event_widget;
      child_loc.child = NULL;

      gtk_container_foreach (GTK_CONTAINER (event_widget),
			     child_location_foreach, &child_loc);

      if (child_loc.child)
	event_widget = child_loc.child;
      else if (child_loc.container)
	event_widget = child_loc.container;
    }

  if (x)
    *x = child_loc.x;
  if (y)
    *y = child_loc.y;

  return event_widget;
}

static GtkWidget *
find_topmost_widget_coords_from_event (GdkEvent *event,
				       gint     *x,
				       gint     *y)
{
  gint tx, ty;
  gdouble dx, dy;
  GtkWidget *tmp;
  GtkWidget *orig;

  gdk_event_get_coords (event, &dx, &dy);
  tx = dx;
  ty = dy;

  orig = tmp = find_widget_under_pointer (event->any.window, &tx, &ty);

  if (tmp && (x != NULL || y != NULL))
    {
      if (tmp != orig)
	gtk_widget_translate_coordinates (orig, tmp, tx, ty, x, y);
      else
        {
	  if (x)
	    *x = tx;
	  if (y)
	    *y = ty;
	}
    }

  return tmp;
}

static gint
tooltip_browse_mode_expired (gpointer data)
{
  GtkTooltip *tooltip;

  GDK_THREADS_ENTER ();

  tooltip = GTK_TOOLTIP (data);

  tooltip->browse_mode_enabled = FALSE;
  tooltip->browse_mode_timeout_id = 0;

  /* destroy tooltip */
  g_object_set_data (G_OBJECT (gtk_widget_get_display (tooltip->window)),
		     "gdk-display-current-tooltip", NULL);

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static void
gtk_tooltip_display_closed (GdkDisplay *display,
			    gboolean    was_error,
			    GtkTooltip *tooltip)
{
  g_object_set (display, "gdk-display-current-tooltip", NULL);
}

static gboolean
gtk_tooltip_run_requery (GtkWidget  **widget,
			 GtkTooltip  *tooltip,
			 gint        *x,
			 gint        *y)
{
  gboolean has_tooltip = FALSE;
  gboolean return_value = FALSE;

  gtk_tooltip_reset (tooltip);

  do
    {
      g_object_get (*widget,
		    "has-tooltip", &has_tooltip,
		    NULL);

      if (has_tooltip)
	g_signal_emit_by_name (*widget,
			       "query-tooltip",
			       *x, *y,
			       tooltip->keyboard_mode_enabled,
			       tooltip,
			       &return_value);

      if (!return_value)
        {
	  GtkWidget *parent = (*widget)->parent;

	  if (parent)
	    gtk_widget_translate_coordinates (*widget, parent, *x, *y, x, y);

	  *widget = parent;
	}
      else
	break;
    }
  while (*widget);

  return return_value;
}

static void
gtk_tooltip_show_tooltip (GdkDisplay *display)
{
  gint x, y;
  GdkScreen *screen;

  GdkWindow *window;
  GtkWidget *tooltip_widget;
  GtkWidget *pointer_widget;
  GtkTooltip *tooltip;
  gboolean has_tooltip;
  gboolean return_value = FALSE;

  tooltip = g_object_get_data (G_OBJECT (display),
			       "gdk-display-current-tooltip");

  if (tooltip->keyboard_mode_enabled)
    {
      pointer_widget = tooltip_widget = tooltip->keyboard_widget;
    }
  else
    {
      window = tooltip->last_window;

      gdk_window_get_origin (window, &x, &y);
      x = tooltip->last_x - x;
      y = tooltip->last_y - y;

      if (!window)
	return;

      pointer_widget = tooltip_widget = find_widget_under_pointer (window,
								   &x, &y);
    }

  if (!tooltip_widget)
    return;

  g_object_get (tooltip_widget, "has-tooltip", &has_tooltip, NULL);

  g_assert (tooltip != NULL);

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

  /* Position the tooltip */
  /* FIXME: should we swap this when RTL is enabled? */
  if (tooltip->keyboard_mode_enabled)
    {
      gdk_window_get_origin (tooltip_widget->window, &x, &y);
      if (GTK_WIDGET_NO_WINDOW (tooltip_widget))
        {
	  x += tooltip_widget->allocation.x;
	  y += tooltip_widget->allocation.y;
	}

      /* For keyboard mode we position the tooltip below the widget,
       * right of the center of the widget.
       */
      x += tooltip_widget->allocation.width / 2;
      y += tooltip_widget->allocation.height + 4;
    }
  else
    {
      guint cursor_size;

      x = tooltip->last_x;
      y = tooltip->last_y;

      /* For mouse mode, we position the tooltip right of the cursor,
       * a little below the cursor's center.
       */
      cursor_size = gdk_display_get_default_cursor_size (display);
      x += cursor_size / 2;
      y += cursor_size / 2;
    }

  screen = gtk_widget_get_screen (tooltip_widget);

  if (screen != gtk_widget_get_screen (tooltip->window))
    {
      g_signal_handlers_disconnect_by_func (display,
					    gtk_tooltip_display_closed,
					    tooltip);

      gtk_window_set_screen (GTK_WINDOW (tooltip->window), screen);

      g_signal_connect (display, "closed",
			G_CALLBACK (gtk_tooltip_display_closed), tooltip);
    }

  tooltip->tooltip_widget = tooltip_widget;

  /* Show it */
  if (tooltip->current_window)
    {
      gint monitor_num;
      GdkRectangle monitor;
      GtkRequisition requisition;

      gtk_widget_size_request (GTK_WIDGET (tooltip->current_window),
                               &requisition);

      monitor_num = gdk_screen_get_monitor_at_point (screen, x, y);
      gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

      if (x + requisition.width > monitor.x + monitor.width)
        x -= x - (monitor.x + monitor.width) + requisition.width;
      else if (x < monitor.x)
        x = monitor.x;

      if (y + requisition.height > monitor.y + monitor.height)
        y -= y - (monitor.y + monitor.height) + requisition.height;

      gtk_window_move (GTK_WINDOW (tooltip->current_window), x, y);
      gtk_widget_show (GTK_WIDGET (tooltip->current_window));
    }

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
  if (!tooltip || !GTK_TOOLTIP_VISIBLE (tooltip))
    return;

  tooltip->tooltip_widget = NULL;

  if (tooltip->timeout_id)
    {
      g_source_remove (tooltip->timeout_id);
      tooltip->timeout_id = 0;
    }

  if (!tooltip->keyboard_mode_enabled)
    {
      guint timeout;
      GtkSettings *settings;

      settings = gtk_widget_get_settings (GTK_WIDGET (tooltip->window));

      g_object_get (settings,
		    "gtk-tooltip-browse-mode-timeout", &timeout,
		    NULL);

      /* The tooltip is gone, after (by default, should be configurable) 500ms
       * we want to turn off browse mode
       */
      if (!tooltip->browse_mode_timeout_id)
	tooltip->browse_mode_timeout_id =
	  g_timeout_add_full (0, timeout,
			      tooltip_browse_mode_expired,
			      g_object_ref (tooltip),
			      g_object_unref);
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

  GDK_THREADS_ENTER ();

  display = GDK_DISPLAY_OBJECT (data);

  gtk_tooltip_show_tooltip (display);

  tooltip = g_object_get_data (G_OBJECT (display),
			       "gdk-display-current-tooltip");
  tooltip->timeout_id = 0;

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static void
gtk_tooltip_start_delay (GdkDisplay *display)
{
  guint timeout;
  GtkTooltip *tooltip;
  GtkSettings *settings;

  tooltip = g_object_get_data (G_OBJECT (display),
			       "gdk-display-current-tooltip");

  if (tooltip && GTK_TOOLTIP_VISIBLE (tooltip))
    return;

  if (tooltip->timeout_id)
    g_source_remove (tooltip->timeout_id);

  settings = gtk_widget_get_settings (GTK_WIDGET (tooltip->window));

  if (tooltip->browse_mode_enabled)
    g_object_get (settings, "gtk-tooltip-browse-timeout", &timeout, NULL);
  else
    g_object_get (settings, "gtk-tooltip-timeout", &timeout, NULL);

  tooltip->timeout_id = g_timeout_add_full (0, timeout,
					    tooltip_popup_timeout,
					    g_object_ref (display),
					    g_object_unref);
}

void
_gtk_tooltip_focus_in (GtkWidget *widget)
{
  gint x, y;
  gboolean return_value = FALSE;
  GdkDisplay *display;
  GtkTooltip *tooltip;

  /* Get current tooltip for this display */
  display = gtk_widget_get_display (widget);
  tooltip = g_object_get_data (G_OBJECT (display),
			       "gdk-display-current-tooltip");

  /* Check if keyboard mode is enabled at this moment */
  if (!tooltip || !tooltip->keyboard_mode_enabled)
    return;

  if (tooltip->keyboard_widget)
    g_object_unref (tooltip->keyboard_widget);

  tooltip->keyboard_widget = g_object_ref (widget);

  gdk_window_get_pointer (widget->window, &x, &y, NULL);

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
  tooltip = g_object_get_data (G_OBJECT (display),
			       "gdk-display-current-tooltip");

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
  tooltip = g_object_get_data (G_OBJECT (display),
			       "gdk-display-current-tooltip");

  if (!tooltip)
    {
      tooltip = g_object_new (GTK_TYPE_TOOLTIP, NULL);
      g_object_set_data_full (G_OBJECT (display),
			      "gdk-display-current-tooltip",
			      tooltip, g_object_unref);
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
  GtkWidget *toplevel;
  GdkDisplay *display;
  GtkTooltip *tooltip;

  display = gtk_widget_get_display (widget);
  tooltip = g_object_get_data (G_OBJECT (display),
			       "gdk-display-current-tooltip");

  if (!tooltip || !GTK_TOOLTIP_VISIBLE (tooltip) || !tooltip->tooltip_widget)
    return;

  toplevel = gtk_widget_get_toplevel (widget);

  if (widget == tooltip->tooltip_widget
      || toplevel->window == tooltip->toplevel_window)
    gtk_tooltip_hide_tooltip (tooltip);
}

void
_gtk_tooltip_handle_event (GdkEvent *event)
{
  gint x, y;
  gboolean return_value = FALSE;
  GtkWidget *has_tooltip_widget = NULL;
  GdkDisplay *display;
  GtkTooltip *current_tooltip;

  has_tooltip_widget = find_topmost_widget_coords_from_event (event, &x, &y);
  display = gdk_drawable_get_display (event->any.window);
  current_tooltip = g_object_get_data (G_OBJECT (display),
				       "gdk-display-current-tooltip");

  if (current_tooltip)
    {
      current_tooltip->last_window = event->any.window;
      gdk_event_get_root_coords (event,
				&current_tooltip->last_x,
				&current_tooltip->last_y);
    }

  if (current_tooltip && current_tooltip->keyboard_mode_enabled)
    {
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

  /* Always poll for a next motion event */
  if (event->type == GDK_MOTION_NOTIFY && event->motion.is_hint)
    gdk_window_get_pointer (event->any.window, NULL, NULL, NULL);

  /* Hide the tooltip when there's no new tooltip widget */
  if (!has_tooltip_widget)
    {
      if (current_tooltip && GTK_TOOLTIP_VISIBLE (current_tooltip))
	gtk_tooltip_hide_tooltip (current_tooltip);

      return;
    }

  switch (event->type)
    {
      case GDK_BUTTON_PRESS:
      case GDK_2BUTTON_PRESS:
      case GDK_3BUTTON_PRESS:
      case GDK_KEY_PRESS:
      case GDK_DRAG_ENTER:
      case GDK_GRAB_BROKEN:
	gtk_tooltip_hide_tooltip (current_tooltip);
	break;

      case GDK_MOTION_NOTIFY:
      case GDK_ENTER_NOTIFY:
      case GDK_LEAVE_NOTIFY:
      case GDK_SCROLL:
	if (current_tooltip)
	  {
	    return_value = gtk_tooltip_run_requery (&has_tooltip_widget,
						    current_tooltip,
						    &x, &y);

	    if (!return_value)
	      gtk_tooltip_hide_tooltip (current_tooltip);
	    else
	      gtk_tooltip_start_delay (display);
	  }
	else
	  {
	    /* Need a new tooltip for this display */
	    current_tooltip = g_object_new (GTK_TYPE_TOOLTIP, NULL);
	    g_object_set_data_full (G_OBJECT (display),
				    "gdk-display-current-tooltip",
				    current_tooltip, g_object_unref);

	    current_tooltip->last_window = event->any.window;
	    gdk_event_get_root_coords (event,
				       &current_tooltip->last_x,
				       &current_tooltip->last_y);

	    gtk_tooltip_start_delay (display);
	  }
	break;

      default:
	break;
    }
}
