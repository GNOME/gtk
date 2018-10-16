/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include "gtkdragsource.h"

#include "gtkdnd.h"
#include "gtkdndprivate.h"
#include "gtkgesturedrag.h"
#include "gtkimagedefinitionprivate.h"
#include "gtkintl.h"


typedef struct _GtkDragSourceSite GtkDragSourceSite;

struct _GtkDragSourceSite 
{
  GdkModifierType    start_button_mask;
  GtkTargetList     *target_list;        /* Targets for drag data */
  GdkDragAction      actions;            /* Possible actions */

  GtkImageDefinition *image_def;
  GtkGesture        *drag_gesture;
};
  
static void
gtk_drag_source_gesture_begin (GtkGesture       *gesture,
                               GdkEventSequence *sequence,
                               gpointer          data)
{
  GtkDragSourceSite *site = data;
  guint button;

  if (gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture)))
    button = 1;
  else
    button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

  g_assert (button >= 1);

  if (!site->start_button_mask ||
      !(site->start_button_mask & (GDK_BUTTON1_MASK << (button - 1))))
    gtk_gesture_set_state (gesture, GTK_EVENT_SEQUENCE_DENIED);
}

static gboolean
gtk_drag_source_event_cb (GtkWidget *widget,
                          GdkEvent  *event,
                          gpointer   data)
{
  gdouble start_x, start_y, offset_x, offset_y;
  GtkDragSourceSite *site = data;

  gtk_event_controller_handle_event (GTK_EVENT_CONTROLLER (site->drag_gesture), event);

  if (gtk_gesture_is_recognized (site->drag_gesture))
    {
      gtk_gesture_drag_get_start_point (GTK_GESTURE_DRAG (site->drag_gesture),
                                        &start_x, &start_y);
      gtk_gesture_drag_get_offset (GTK_GESTURE_DRAG (site->drag_gesture),
                                   &offset_x, &offset_y);

      if (gtk_drag_check_threshold (widget, start_x, start_y,
                                    start_x + offset_x, start_y + offset_y))
        {
          GdkEventSequence *sequence;
          GdkEvent *last_event;
          guint button;
          gboolean needs_icon;
          GdkDragContext *context;

          sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (site->drag_gesture));
          last_event = gdk_event_copy (gtk_gesture_get_last_event (site->drag_gesture, sequence));

          button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (site->drag_gesture));
          gtk_event_controller_reset (GTK_EVENT_CONTROLLER (site->drag_gesture));

          context = gtk_drag_begin_internal (widget, &needs_icon, site->target_list,
                                             site->actions, button, last_event,
                                             start_x, start_y);

          if (context != NULL && needs_icon)
            gtk_drag_set_icon_definition (context, site->image_def, 0, 0);

          gdk_event_free (last_event);

          return TRUE;
        }
    }

  return FALSE;
}

static void 
gtk_drag_source_site_destroy (gpointer data)
{
  GtkDragSourceSite *site = data;

  if (site->target_list)
    gtk_target_list_unref (site->target_list);

  gtk_image_definition_unref (site->image_def);
  g_clear_object (&site->drag_gesture);
  g_slice_free (GtkDragSourceSite, site);
}

/**
 * gtk_drag_source_set: (method)
 * @widget: a #GtkWidget
 * @start_button_mask: the bitmask of buttons that can start the drag
 * @targets: (allow-none) (array length=n_targets): the table of targets
 *     that the drag will support, may be %NULL
 * @n_targets: the number of items in @targets
 * @actions: the bitmask of possible actions for a drag from this widget
 *
 * Sets up a widget so that GTK+ will start a drag operation when the user
 * clicks and drags on the widget. The widget must have a window.
 */
void
gtk_drag_source_set (GtkWidget            *widget,
                     GdkModifierType       start_button_mask,
                     const GtkTargetEntry *targets,
                     gint                  n_targets,
                     GdkDragAction         actions)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");

  gtk_widget_add_events (widget,
                         gtk_widget_get_events (widget) |
                         GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                         GDK_BUTTON_MOTION_MASK);

  if (site)
    {
      if (site->target_list)
        gtk_target_list_unref (site->target_list);
    }
  else
    {
      site = g_slice_new0 (GtkDragSourceSite);
      site->image_def = gtk_image_definition_new_empty ();
      site->drag_gesture = gtk_gesture_drag_new (widget);
      gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (site->drag_gesture),
                                                  GTK_PHASE_NONE);
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (site->drag_gesture), 0);
      g_signal_connect (site->drag_gesture, "begin",
                        G_CALLBACK (gtk_drag_source_gesture_begin),
                        site);

      g_signal_connect (widget, "button-press-event",
                        G_CALLBACK (gtk_drag_source_event_cb),
                        site);
      g_signal_connect (widget, "button-release-event",
                        G_CALLBACK (gtk_drag_source_event_cb),
                        site);
      g_signal_connect (widget, "motion-notify-event",
                        G_CALLBACK (gtk_drag_source_event_cb),
                        site);
      g_object_set_data_full (G_OBJECT (widget),
                              I_("gtk-site-data"), 
                              site, gtk_drag_source_site_destroy);
    }

  site->start_button_mask = start_button_mask;

  site->target_list = gtk_target_list_new (targets, n_targets);

  site->actions = actions;
}

/**
 * gtk_drag_source_unset: (method)
 * @widget: a #GtkWidget
 *
 * Undoes the effects of gtk_drag_source_set().
 */
void 
gtk_drag_source_unset (GtkWidget *widget)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");

  if (site)
    {
      g_signal_handlers_disconnect_by_func (widget,
                                            gtk_drag_source_event_cb,
                                            site);
      g_object_set_data (G_OBJECT (widget), I_("gtk-site-data"), NULL);
    }
}

/**
 * gtk_drag_source_get_target_list: (method)
 * @widget: a #GtkWidget
 *
 * Gets the list of targets this widget can provide for
 * drag-and-drop.
 *
 * Returns: (nullable) (transfer none): the #GtkTargetList, or %NULL if none
 *
 * Since: 2.4
 */
GtkTargetList *
gtk_drag_source_get_target_list (GtkWidget *widget)
{
  GtkDragSourceSite *site;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");

  return site ? site->target_list : NULL;
}

/**
 * gtk_drag_source_set_target_list: (method)
 * @widget: a #GtkWidget that’s a drag source
 * @target_list: (allow-none): list of draggable targets, or %NULL for none
 *
 * Changes the target types that this widget offers for drag-and-drop.
 * The widget must first be made into a drag source with
 * gtk_drag_source_set().
 *
 * Since: 2.4
 */
void
gtk_drag_source_set_target_list (GtkWidget     *widget,
                                 GtkTargetList *target_list)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");
  if (site == NULL)
    {
      g_warning ("gtk_drag_source_set_target_list() requires the widget "
                 "to already be a drag source.");
      return;
    }

  if (target_list)
    gtk_target_list_ref (target_list);

  if (site->target_list)
    gtk_target_list_unref (site->target_list);

  site->target_list = target_list;
}

/**
 * gtk_drag_source_add_text_targets: (method)
 * @widget: a #GtkWidget that’s is a drag source
 *
 * Add the text targets supported by #GtkSelectionData to
 * the target list of the drag source.  The targets
 * are added with @info = 0. If you need another value, 
 * use gtk_target_list_add_text_targets() and
 * gtk_drag_source_set_target_list().
 * 
 * Since: 2.6
 */
void
gtk_drag_source_add_text_targets (GtkWidget *widget)
{
  GtkTargetList *target_list;

  target_list = gtk_drag_source_get_target_list (widget);
  if (target_list)
    gtk_target_list_ref (target_list);
  else
    target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_text_targets (target_list, 0);
  gtk_drag_source_set_target_list (widget, target_list);
  gtk_target_list_unref (target_list);
}

/**
 * gtk_drag_source_add_image_targets: (method)
 * @widget: a #GtkWidget that’s is a drag source
 *
 * Add the writable image targets supported by #GtkSelectionData to
 * the target list of the drag source. The targets
 * are added with @info = 0. If you need another value, 
 * use gtk_target_list_add_image_targets() and
 * gtk_drag_source_set_target_list().
 * 
 * Since: 2.6
 */
void
gtk_drag_source_add_image_targets (GtkWidget *widget)
{
  GtkTargetList *target_list;

  target_list = gtk_drag_source_get_target_list (widget);
  if (target_list)
    gtk_target_list_ref (target_list);
  else
    target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_image_targets (target_list, 0, TRUE);
  gtk_drag_source_set_target_list (widget, target_list);
  gtk_target_list_unref (target_list);
}

/**
 * gtk_drag_source_add_uri_targets: (method)
 * @widget: a #GtkWidget that’s is a drag source
 *
 * Add the URI targets supported by #GtkSelectionData to
 * the target list of the drag source.  The targets
 * are added with @info = 0. If you need another value, 
 * use gtk_target_list_add_uri_targets() and
 * gtk_drag_source_set_target_list().
 * 
 * Since: 2.6
 */
void
gtk_drag_source_add_uri_targets (GtkWidget *widget)
{
  GtkTargetList *target_list;

  target_list = gtk_drag_source_get_target_list (widget);
  if (target_list)
    gtk_target_list_ref (target_list);
  else
    target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_uri_targets (target_list, 0);
  gtk_drag_source_set_target_list (widget, target_list);
  gtk_target_list_unref (target_list);
}

/**
 * gtk_drag_source_set_icon_pixbuf: (method)
 * @widget: a #GtkWidget
 * @pixbuf: the #GdkPixbuf for the drag icon
 * 
 * Sets the icon that will be used for drags from a particular widget
 * from a #GdkPixbuf. GTK+ retains a reference for @pixbuf and will 
 * release it when it is no longer needed.
 */
void 
gtk_drag_source_set_icon_pixbuf (GtkWidget *widget,
                                 GdkPixbuf *pixbuf)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");
  g_return_if_fail (site != NULL); 

  g_clear_pointer (&site->image_def, gtk_image_definition_unref);
  site->image_def = gtk_image_definition_new_pixbuf (pixbuf, 1);
}

/**
 * gtk_drag_source_set_icon_stock: (method)
 * @widget: a #GtkWidget
 * @stock_id: the ID of the stock icon to use
 *
 * Sets the icon that will be used for drags from a particular source
 * to a stock icon.
 *
 * Deprecated: 3.10: Use gtk_drag_source_set_icon_name() instead.
 */
void
gtk_drag_source_set_icon_stock (GtkWidget   *widget,
                                const gchar *stock_id)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (stock_id != NULL);

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");
  g_return_if_fail (site != NULL);

  gtk_image_definition_unref (site->image_def);
  site->image_def = gtk_image_definition_new_stock (stock_id);
}

/**
 * gtk_drag_source_set_icon_name: (method)
 * @widget: a #GtkWidget
 * @icon_name: name of icon to use
 * 
 * Sets the icon that will be used for drags from a particular source
 * to a themed icon. See the docs for #GtkIconTheme for more details.
 *
 * Since: 2.8
 */
void 
gtk_drag_source_set_icon_name (GtkWidget   *widget,
                               const gchar *icon_name)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (icon_name != NULL);

  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");
  g_return_if_fail (site != NULL);

  gtk_image_definition_unref (site->image_def);
  site->image_def = gtk_image_definition_new_icon_name (icon_name);
}

/**
 * gtk_drag_source_set_icon_gicon: (method)
 * @widget: a #GtkWidget
 * @icon: A #GIcon
 * 
 * Sets the icon that will be used for drags from a particular source
 * to @icon. See the docs for #GtkIconTheme for more details.
 *
 * Since: 3.2
 */
void
gtk_drag_source_set_icon_gicon (GtkWidget *widget,
                                GIcon     *icon)
{
  GtkDragSourceSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (icon != NULL);
  
  site = g_object_get_data (G_OBJECT (widget), "gtk-site-data");
  g_return_if_fail (site != NULL);

  gtk_image_definition_unref (site->image_def);
  site->image_def = gtk_image_definition_new_gicon (icon);
}

