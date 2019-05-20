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
#include "gtkintl.h"
#include "gtknative.h"


static void
gtk_drag_dest_realized (GtkWidget *widget)
{
  GtkNative *native = gtk_widget_get_native (widget);

  gdk_surface_register_dnd (gtk_native_get_surface (native));
}

static void
gtk_drag_dest_hierarchy_changed (GtkWidget  *widget,
                                 GParamSpec *pspec,
                                 gpointer    data)
{
  GtkNative *native = gtk_widget_get_native (widget);

  if (native && gtk_widget_get_realized (GTK_WIDGET (native)))
    gdk_surface_register_dnd (gtk_native_get_surface (native));
}

static void
gtk_drag_dest_site_destroy (gpointer data)
{
  GtkDragDestSite *site = data;

  if (site->target_list)
    gdk_content_formats_unref (site->target_list);

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
  g_signal_connect (widget, "notify::root",
                    G_CALLBACK (gtk_drag_dest_hierarchy_changed), site);

  g_object_set_data_full (G_OBJECT (widget), I_("gtk-drag-dest"),
                          site, gtk_drag_dest_site_destroy);
}

/**
 * gtk_drag_dest_set: (method)
 * @widget: a #GtkWidget
 * @flags: which types of default drag behavior to use
 * @targets: (allow-none): the drop types that this @widget will
 *     accept, or %NULL. Later you can access the list with
 *     gtk_drag_dest_get_target_list() and gtk_drag_dest_find_target().
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
 * and invokations of gdk_drag_finish() in #GtkWidget::drag-data-received.
 * Especially the later is dramatic, when your own #GtkWidget::drag-motion
 * handler calls gtk_drag_get_data() to inspect the dragged data.
 *
 * There’s no way to set a default action here, you can use the
 * #GtkWidget::drag-motion callback for that. Here’s an example which selects
 * the action to use depending on whether the control key is pressed or not:
 * |[<!-- language="C" -->
 * static void
 * drag_motion (GtkWidget *widget,
 *              GdkDrag *drag,
 *              gint x,
 *              gint y,
 *              guint time)
 * {
*   GdkModifierType mask;
 *
 *   gdk_surface_get_pointer (gtk_native_get_surface (gtk_widget_get_native (widget)),
 *                           NULL, NULL, &mask);
 *   if (mask & GDK_CONTROL_MASK)
 *     gdk_drag_status (context, GDK_ACTION_COPY, time);
 *   else
 *     gdk_drag_status (context, GDK_ACTION_MOVE, time);
 * }
 * ]|
 */
void
gtk_drag_dest_set (GtkWidget         *widget,
                   GtkDestDefaults    flags,
                   GdkContentFormats *targets,
                   GdkDragAction      actions)
{
  GtkDragDestSite *site;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  site = g_slice_new0 (GtkDragDestSite);

  site->flags = flags;
  site->have_drag = FALSE;
  if (targets)
    site->target_list = gdk_content_formats_ref (targets);
  else
    site->target_list = NULL;
  site->actions = actions;
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
 * Returns: (nullable) (transfer none): the #GdkContentFormats, or %NULL if none
 */
GdkContentFormats *
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
                               GdkContentFormats *target_list)
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
    gdk_content_formats_ref (target_list);

  if (site->target_list)
    gdk_content_formats_unref (site->target_list);

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
 */
void
gtk_drag_dest_add_text_targets (GtkWidget *widget)
{
  GdkContentFormats *target_list;

  target_list = gtk_drag_dest_get_target_list (widget);
  if (target_list)
    gdk_content_formats_ref (target_list);
  else
    target_list = gdk_content_formats_new (NULL, 0);
  target_list = gtk_content_formats_add_text_targets (target_list);
  gtk_drag_dest_set_target_list (widget, target_list);
  gdk_content_formats_unref (target_list);
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
 */
void
gtk_drag_dest_add_image_targets (GtkWidget *widget)
{
  GdkContentFormats *target_list;

  target_list = gtk_drag_dest_get_target_list (widget);
  if (target_list)
    gdk_content_formats_ref (target_list);
  else
    target_list = gdk_content_formats_new (NULL, 0);
  target_list = gtk_content_formats_add_image_targets (target_list, FALSE);
  gtk_drag_dest_set_target_list (widget, target_list);
  gdk_content_formats_unref (target_list);
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
 */
void
gtk_drag_dest_add_uri_targets (GtkWidget *widget)
{
  GdkContentFormats *target_list;

  target_list = gtk_drag_dest_get_target_list (widget);
  if (target_list)
    gdk_content_formats_ref (target_list);
  else
    target_list = gdk_content_formats_new (NULL, 0);
  target_list = gtk_content_formats_add_uri_targets (target_list);
  gtk_drag_dest_set_target_list (widget, target_list);
  gdk_content_formats_unref (target_list);
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
 * @drop: #GdkDrop
 * @target_list: (allow-none): list of droppable targets, or %NULL to use
 *    gtk_drag_dest_get_target_list (@widget).
 *
 * Looks for a match between the supported targets of @drop and the
 * @dest_target_list, returning the first matching target, otherwise
 * returning %NULL. @dest_target_list should usually be the return
 * value from gtk_drag_dest_get_target_list(), but some widgets may
 * have different valid targets for different parts of the widget; in
 * that case, they will have to implement a drag_motion handler that
 * passes the correct target list to this function.
 *
 * Returns: (transfer none) (nullable): first target that the source offers
 *     and the dest can accept, or %NULL
 */
const char *
gtk_drag_dest_find_target (GtkWidget         *widget,
                           GdkDrop           *drop,
                           GdkContentFormats *target_list)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (GDK_IS_DROP (drop), NULL);

  if (target_list == NULL)
    target_list = gtk_drag_dest_get_target_list (widget);

  if (target_list == NULL)
    return NULL;

  return gdk_content_formats_match_mime_type (target_list,
                                              gdk_drop_get_formats (drop));
}

