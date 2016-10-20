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

#include "gtkdragdest.h"

#include "gtkdnd.h"
#include "gtkdndprivate.h"
#include "gtkselectionprivate.h"
#include "gtkintl.h"


static void
gtk_drag_dest_realized (GtkWidget *widget)
{
  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);

  if (gtk_widget_is_toplevel (toplevel))
    gdk_window_register_dnd (gtk_widget_get_window (toplevel));
}

static void
gtk_drag_dest_hierarchy_changed (GtkWidget *widget,
                                 GtkWidget *previous_toplevel)
{
  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);

  if (gtk_widget_is_toplevel (toplevel) && gtk_widget_get_realized (toplevel))
    gdk_window_register_dnd (gtk_widget_get_window (toplevel));
}

static void
gtk_drag_dest_site_destroy (gpointer data)
{
  GtkDragDestSite *site = data;

  if (site->proxy_window)
    g_object_unref (site->proxy_window);

  if (site->target_list)
    gtk_target_list_unref (site->target_list);

  g_slice_free (GtkDragDestSite, site);
}

static void
gtk_drag_dest_set_internal (GtkWidget       *widget,
                            GtkDragDestSite *site)
{
  GtkDragDestSite *old_site;

  old_site = g_object_get_data (G_OBJECT (widget), I_("gtk-drag-dest"));
  if (old_site)
    {
      g_signal_handlers_disconnect_by_func (widget,
                                            gtk_drag_dest_realized,
                                            old_site);
      g_signal_handlers_disconnect_by_func (widget,
                                            gtk_drag_dest_hierarchy_changed,
                                            old_site);

      site->track_motion = old_site->track_motion;
    }

  if (gtk_widget_get_realized (widget))
    gtk_drag_dest_realized (widget);

  g_signal_connect (widget, "realize",
                    G_CALLBACK (gtk_drag_dest_realized), site);
  g_signal_connect (widget, "hierarchy-changed",
                    G_CALLBACK (gtk_drag_dest_hierarchy_changed), site);

  g_object_set_data_full (G_OBJECT (widget), I_("gtk-drag-dest"),
                          site, gtk_drag_dest_site_destroy);
}

/**
 * gtk_drag_dest_set: (method)
 * @widget: a #GtkWidget
 * @flags: which types of default drag behavior to use
 * @targets: (allow-none) (array length=n_targets): a pointer to an array of
 *     #GtkTargetEntrys indicating the drop types that this @widget will
 *     accept, or %NULL. Later you can access the list with
 *     gtk_drag_dest_get_target_list() and gtk_drag_dest_find_target().
 * @n_targets: the number of entries in @targets
 * @actions: a bitmask of possible actions for a drop onto this @widget.
 *
 * Sets a widget as a potential drop destination, and adds default behaviors.
 *
 * The default behaviors listed in @flags have an effect similar
 * to installing default handlers for the widget’s drag-and-drop signals
 * (#GtkWidget::drag-motion, #GtkWidget::drag-drop, ...). They all exist
 * for convenience. When passing #GTK_DEST_DEFAULT_ALL for instance it is
 * sufficient to connect to the widget’s #GtkWidget::drag-data-received
 * signal to get primitive, but consistent drag-and-drop support.
 *
 * Things become more complicated when you try to preview the dragged data,
 * as described in the documentation for #GtkWidget::drag-motion. The default
 * behaviors described by @flags make some assumptions, that can conflict
 * with your own signal handlers. For instance #GTK_DEST_DEFAULT_DROP causes
 * invokations of gdk_drag_status() in the context of #GtkWidget::drag-motion,
 * and invokations of gtk_drag_finish() in #GtkWidget::drag-data-received.
 * Especially the later is dramatic, when your own #GtkWidget::drag-motion
 * handler calls gtk_drag_get_data() to inspect the dragged data.
 *
 * There’s no way to set a default action here, you can use the
 * #GtkWidget::drag-motion callback for that. Here’s an example which selects
 * the action to use depending on whether the control key is pressed or not:
 * |[<!-- language="C" -->
 * static void
 * drag_motion (GtkWidget *widget,
 *              GdkDragContext *context,
 *              gint x,
 *              gint y,
 *              guint time)
 * {
*   GdkModifierType mask;
 *
 *   gdk_window_get_pointer (gtk_widget_get_window (widget),
 *                           NULL, NULL, &mask);
 *   if (mask & GDK_CONTROL_MASK)
 *     gdk_drag_status (context, GDK_ACTION_COPY, time);
 *   else
 *     gdk_drag_status (context, GDK_ACTION_MOVE, time);
 * }
 * ]|
 */
void
gtk_drag_dest_set (GtkWidget            *widget,
                   GtkDestDefaults       flags,
                   const GtkTargetEntry *targets,
                   gint                  n_targets,
                   GdkDragAction         actions)
{
  GtkDragDestSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  site = g_slice_new0 (GtkDragDestSite);

  site->flags = flags;
  site->have_drag = FALSE;
  if (targets)
    site->target_list = gtk_target_list_new (targets, n_targets);
  else
    site->target_list = NULL;
  site->actions = actions;
  site->do_proxy = FALSE;
  site->proxy_window = NULL;
  site->track_motion = FALSE;

  gtk_drag_dest_set_internal (widget, site);
}

/**
 * gtk_drag_dest_set_proxy: (method)
 * @widget: a #GtkWidget
 * @proxy_window: the window to which to forward drag events
 * @protocol: the drag protocol which the @proxy_window accepts
 *   (You can use gdk_drag_get_protocol() to determine this)
 * @use_coordinates: If %TRUE, send the same coordinates to the
 *   destination, because it is an embedded
 *   subwindow.
 *
 * Sets this widget as a proxy for drops to another window.
 *
 * Deprecated: 3.22
 */
void
gtk_drag_dest_set_proxy (GtkWidget       *widget,
                         GdkWindow       *proxy_window,
                         GdkDragProtocol  protocol,
                         gboolean         use_coordinates)
{
  GtkDragDestSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (!proxy_window || GDK_IS_WINDOW (proxy_window));

  site = g_slice_new (GtkDragDestSite);

  site->flags = 0;
  site->have_drag = FALSE;
  site->target_list = NULL;
  site->actions = 0;
  site->proxy_window = proxy_window;
  if (proxy_window)
    g_object_ref (proxy_window);
  site->do_proxy = TRUE;
  site->proxy_protocol = protocol;
  site->proxy_coords = use_coordinates;
  site->track_motion = FALSE;

  gtk_drag_dest_set_internal (widget, site);
}

/**
 * gtk_drag_dest_unset: (method)
 * @widget: a #GtkWidget
 *
 * Clears information about a drop destination set with
 * gtk_drag_dest_set(). The widget will no longer receive
 * notification of drags.
 */
void
gtk_drag_dest_unset (GtkWidget *widget)
{
  GtkDragDestSite *old_site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  old_site = g_object_get_data (G_OBJECT (widget), I_("gtk-drag-dest"));
  if (old_site)
    {
      g_signal_handlers_disconnect_by_func (widget,
                                            gtk_drag_dest_realized,
                                            old_site);
      g_signal_handlers_disconnect_by_func (widget,
                                            gtk_drag_dest_hierarchy_changed,
                                            old_site);
    }

  g_object_set_data (G_OBJECT (widget), I_("gtk-drag-dest"), NULL);
}

/**
 * gtk_drag_dest_get_target_list: (method)
 * @widget: a #GtkWidget
 *
 * Returns the list of targets this widget can accept from
 * drag-and-drop.
 *
 * Returns: (nullable) (transfer none): the #GtkTargetList, or %NULL if none
 */
GtkTargetList *
gtk_drag_dest_get_target_list (GtkWidget *widget)
{
  GtkDragDestSite *site;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);

  site = g_object_get_data (G_OBJECT (widget), I_("gtk-drag-dest"));

  return site ? site->target_list : NULL;
}

/**
 * gtk_drag_dest_set_target_list: (method)
 * @widget: a #GtkWidget that’s a drag destination
 * @target_list: (allow-none): list of droppable targets, or %NULL for none
 *
 * Sets the target types that this widget can accept from drag-and-drop.
 * The widget must first be made into a drag destination with
 * gtk_drag_dest_set().
 */
void
gtk_drag_dest_set_target_list (GtkWidget     *widget,
                               GtkTargetList *target_list)
{
  GtkDragDestSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  site = g_object_get_data (G_OBJECT (widget), I_("gtk-drag-dest"));

  if (!site)
    {
      g_warning ("Can't set a target list on a widget until you've called gtk_drag_dest_set() "
                 "to make the widget into a drag destination");
      return;
    }

  if (target_list)
    gtk_target_list_ref (target_list);

  if (site->target_list)
    gtk_target_list_unref (site->target_list);

  site->target_list = target_list;
}

/**
 * gtk_drag_dest_add_text_targets: (method)
 * @widget: a #GtkWidget that’s a drag destination
 *
 * Add the text targets supported by #GtkSelectionData to
 * the target list of the drag destination. The targets
 * are added with @info = 0. If you need another value,
 * use gtk_target_list_add_text_targets() and
 * gtk_drag_dest_set_target_list().
 *
 * Since: 2.6
 */
void
gtk_drag_dest_add_text_targets (GtkWidget *widget)
{
  GtkTargetList *target_list;

  target_list = gtk_drag_dest_get_target_list (widget);
  if (target_list)
    gtk_target_list_ref (target_list);
  else
    target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_text_targets (target_list, 0);
  gtk_drag_dest_set_target_list (widget, target_list);
  gtk_target_list_unref (target_list);
}

/**
 * gtk_drag_dest_add_image_targets: (method)
 * @widget: a #GtkWidget that’s a drag destination
 *
 * Add the image targets supported by #GtkSelectionData to
 * the target list of the drag destination. The targets
 * are added with @info = 0. If you need another value,
 * use gtk_target_list_add_image_targets() and
 * gtk_drag_dest_set_target_list().
 *
 * Since: 2.6
 */
void
gtk_drag_dest_add_image_targets (GtkWidget *widget)
{
  GtkTargetList *target_list;

  target_list = gtk_drag_dest_get_target_list (widget);
  if (target_list)
    gtk_target_list_ref (target_list);
  else
    target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_image_targets (target_list, 0, FALSE);
  gtk_drag_dest_set_target_list (widget, target_list);
  gtk_target_list_unref (target_list);
}

/**
 * gtk_drag_dest_add_uri_targets: (method)
 * @widget: a #GtkWidget that’s a drag destination
 *
 * Add the URI targets supported by #GtkSelectionData to
 * the target list of the drag destination. The targets
 * are added with @info = 0. If you need another value,
 * use gtk_target_list_add_uri_targets() and
 * gtk_drag_dest_set_target_list().
 *
 * Since: 2.6
 */
void
gtk_drag_dest_add_uri_targets (GtkWidget *widget)
{
  GtkTargetList *target_list;

  target_list = gtk_drag_dest_get_target_list (widget);
  if (target_list)
    gtk_target_list_ref (target_list);
  else
    target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_uri_targets (target_list, 0);
  gtk_drag_dest_set_target_list (widget, target_list);
  gtk_target_list_unref (target_list);
}

/**
 * gtk_drag_dest_set_track_motion: (method)
 * @widget: a #GtkWidget that’s a drag destination
 * @track_motion: whether to accept all targets
 *
 * Tells the widget to emit #GtkWidget::drag-motion and
 * #GtkWidget::drag-leave events regardless of the targets and the
 * %GTK_DEST_DEFAULT_MOTION flag.
 *
 * This may be used when a widget wants to do generic
 * actions regardless of the targets that the source offers.
 *
 * Since: 2.10
 */
void
gtk_drag_dest_set_track_motion (GtkWidget *widget,
                                gboolean   track_motion)
{
  GtkDragDestSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  site = g_object_get_data (G_OBJECT (widget), I_("gtk-drag-dest"));

  g_return_if_fail (site != NULL);

  site->track_motion = track_motion != FALSE;
}

/**
 * gtk_drag_dest_get_track_motion: (method)
 * @widget: a #GtkWidget that’s a drag destination
 *
 * Returns whether the widget has been configured to always
 * emit #GtkWidget::drag-motion signals.
 *
 * Returns: %TRUE if the widget always emits
 *   #GtkWidget::drag-motion events
 *
 * Since: 2.10
 */
gboolean
gtk_drag_dest_get_track_motion (GtkWidget *widget)
{
  GtkDragDestSite *site;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  site = g_object_get_data (G_OBJECT (widget), I_("gtk-drag-dest"));

  if (site)
    return site->track_motion;

  return FALSE;
}

/**
 * gtk_drag_dest_find_target: (method)
 * @widget: drag destination widget
 * @context: drag context
 * @target_list: (allow-none): list of droppable targets, or %NULL to use
 *    gtk_drag_dest_get_target_list (@widget).
 *
 * Looks for a match between the supported targets of @context and the
 * @dest_target_list, returning the first matching target, otherwise
 * returning %GDK_NONE. @dest_target_list should usually be the return
 * value from gtk_drag_dest_get_target_list(), but some widgets may
 * have different valid targets for different parts of the widget; in
 * that case, they will have to implement a drag_motion handler that
 * passes the correct target list to this function.
 *
 * Returns: (transfer none): first target that the source offers
 *     and the dest can accept, or %GDK_NONE
 */
GdkAtom
gtk_drag_dest_find_target (GtkWidget      *widget,
                           GdkDragContext *context,
                           GtkTargetList  *target_list)
{
  GList *tmp_target;
  GList *tmp_source = NULL;
  GtkWidget *source_widget;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), GDK_NONE);
  g_return_val_if_fail (GDK_IS_DRAG_CONTEXT (context), GDK_NONE);

  source_widget = gtk_drag_get_source_widget (context);
  if (target_list == NULL)
    target_list = gtk_drag_dest_get_target_list (widget);

  if (target_list == NULL)
    return GDK_NONE;

  tmp_target = target_list->list;
  while (tmp_target)
    {
      GtkTargetPair *pair = tmp_target->data;
      tmp_source = gdk_drag_context_list_targets (context);
      while (tmp_source)
        {
          if (tmp_source->data == GUINT_TO_POINTER (pair->target))
            {
              if ((!(pair->flags & GTK_TARGET_SAME_APP) || source_widget) &&
                  (!(pair->flags & GTK_TARGET_SAME_WIDGET) || (source_widget == widget)) &&
                  (!(pair->flags & GTK_TARGET_OTHER_APP) || !source_widget) &&
                  (!(pair->flags & GTK_TARGET_OTHER_WIDGET) || (source_widget != widget)))
                return pair->target;
              else
                break;
            }
          tmp_source = tmp_source->next;
        }
      tmp_target = tmp_target->next;
    }

  return GDK_NONE;
}

