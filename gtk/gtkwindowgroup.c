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

#include "gtkmain.h"
#include "gtkwindowprivate.h"
#include "gtkwindowgroup.h"


/**
 * SECTION:gtkwindowgroup
 * @Short_description: Limit the effect of grabs
 * @Title: GtkWindowGroup
 *
 * A #GtkWindowGroup restricts the effect of grabs to windows
 * in the same group, thereby making window groups almost behave
 * like separate applications. 
 *
 * A window can be a member in at most one window group at a time.
 * Windows that have not been explicitly assigned to a group are
 * implicitly treated like windows of the default window group.
 *
 * GtkWindowGroup objects are referenced by each window in the group,
 * so once you have added all windows to a GtkWindowGroup, you can drop
 * the initial reference to the window group with g_object_unref(). If the
 * windows in the window group are subsequently destroyed, then they will
 * be removed from the window group and drop their references on the window
 * group; when all window have been removed, the window group will be
 * freed.
 */

typedef struct _GtkDeviceGrabInfo GtkDeviceGrabInfo;
struct _GtkDeviceGrabInfo
{
  GtkWidget *widget;
  GdkDevice *device;
  guint block_others : 1;
};

struct _GtkWindowGroupPrivate
{
  GSList *grabs;
  GSList *device_grabs;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkWindowGroup, gtk_window_group, G_TYPE_OBJECT)

static void
gtk_window_group_init (GtkWindowGroup *group)
{
  group->priv = gtk_window_group_get_instance_private (group);
}

static void
gtk_window_group_class_init (GtkWindowGroupClass *klass)
{
}


/**
 * gtk_window_group_new:
 * 
 * Creates a new #GtkWindowGroup object. Grabs added with
 * gtk_grab_add() only affect windows within the same #GtkWindowGroup.
 * 
 * Returns: a new #GtkWindowGroup. 
 **/
GtkWindowGroup *
gtk_window_group_new (void)
{
  return g_object_new (GTK_TYPE_WINDOW_GROUP, NULL);
}

static void
window_group_cleanup_grabs (GtkWindowGroup *group,
                            GtkWindow      *window)
{
  GtkWindowGroupPrivate *priv;
  GtkDeviceGrabInfo *info;
  GSList *tmp_list;
  GSList *to_remove = NULL;

  priv = group->priv;

  tmp_list = priv->grabs;
  while (tmp_list)
    {
      if (gtk_widget_get_toplevel (tmp_list->data) == (GtkWidget*) window)
        to_remove = g_slist_prepend (to_remove, g_object_ref (tmp_list->data));
      tmp_list = tmp_list->next;
    }

  while (to_remove)
    {
      gtk_grab_remove (to_remove->data);
      g_object_unref (to_remove->data);
      to_remove = g_slist_delete_link (to_remove, to_remove);
    }

  tmp_list = priv->device_grabs;

  while (tmp_list)
    {
      info = tmp_list->data;

      if (gtk_widget_get_toplevel (info->widget) == (GtkWidget *) window)
        to_remove = g_slist_prepend (to_remove, info);

      tmp_list = tmp_list->next;
    }

  while (to_remove)
    {
      info = to_remove->data;

      gtk_device_grab_remove (info->widget, info->device);
      to_remove = g_slist_delete_link (to_remove, to_remove);
    }
}

/**
 * gtk_window_group_add_window:
 * @window_group: a #GtkWindowGroup
 * @window: the #GtkWindow to add
 * 
 * Adds a window to a #GtkWindowGroup. 
 **/
void
gtk_window_group_add_window (GtkWindowGroup *window_group,
                             GtkWindow      *window)
{
  GtkWindowGroup *old_group;

  g_return_if_fail (GTK_IS_WINDOW_GROUP (window_group));
  g_return_if_fail (GTK_IS_WINDOW (window));

  old_group = _gtk_window_get_window_group (window);

  if (old_group != window_group)
    {
      g_object_ref (window);
      g_object_ref (window_group);

      if (old_group)
        gtk_window_group_remove_window (old_group, window);
      else
        window_group_cleanup_grabs (gtk_window_get_group (NULL), window);

      _gtk_window_set_window_group (window, window_group);

      g_object_unref (window);
    }
}

/**
 * gtk_window_group_remove_window:
 * @window_group: a #GtkWindowGroup
 * @window: the #GtkWindow to remove
 * 
 * Removes a window from a #GtkWindowGroup.
 **/
void
gtk_window_group_remove_window (GtkWindowGroup *window_group,
                                GtkWindow      *window)
{
  g_return_if_fail (GTK_IS_WINDOW_GROUP (window_group));
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (_gtk_window_get_window_group (window) == window_group);

  g_object_ref (window);

  window_group_cleanup_grabs (window_group, window);
  _gtk_window_set_window_group (window, NULL);

  g_object_unref (window_group);
  g_object_unref (window);
}

/**
 * gtk_window_group_list_windows:
 * @window_group: a #GtkWindowGroup
 *
 * Returns a list of the #GtkWindows that belong to @window_group.
 *
 * Returns: (element-type GtkWindow) (transfer container): A
 *   newly-allocated list of windows inside the group.
 *
 * Since: 2.14
 **/
GList *
gtk_window_group_list_windows (GtkWindowGroup *window_group)
{
  GList *toplevels, *toplevel, *group_windows;

  g_return_val_if_fail (GTK_IS_WINDOW_GROUP (window_group), NULL);

  group_windows = NULL;
  toplevels = gtk_window_list_toplevels ();

  for (toplevel = toplevels; toplevel; toplevel = toplevel->next)
    {
      GtkWindow *window = toplevel->data;

      if (window_group == _gtk_window_get_window_group (window))
        group_windows = g_list_prepend (group_windows, window);
    }

  g_list_free (toplevels);

  return g_list_reverse (group_windows);
}

/**
 * gtk_window_group_get_current_grab:
 * @window_group: a #GtkWindowGroup
 *
 * Gets the current grab widget of the given group,
 * see gtk_grab_add().
 *
 * Returns: (transfer none): the current grab widget of the group
 *
 * Since: 2.22
 */
GtkWidget *
gtk_window_group_get_current_grab (GtkWindowGroup *window_group)
{
  g_return_val_if_fail (GTK_IS_WINDOW_GROUP (window_group), NULL);

  if (window_group->priv->grabs)
    return GTK_WIDGET (window_group->priv->grabs->data);
  return NULL;
}

void
_gtk_window_group_add_grab (GtkWindowGroup *window_group,
                            GtkWidget      *widget)
{
  GtkWindowGroupPrivate *priv;

  priv = window_group->priv;
  priv->grabs = g_slist_prepend (priv->grabs, widget);
}

void
_gtk_window_group_remove_grab (GtkWindowGroup *window_group,
                               GtkWidget      *widget)
{
  GtkWindowGroupPrivate *priv;

  priv = window_group->priv;
  priv->grabs = g_slist_remove (priv->grabs, widget);
}

void
_gtk_window_group_add_device_grab (GtkWindowGroup *window_group,
                                   GtkWidget      *widget,
                                   GdkDevice      *device,
                                   gboolean        block_others)
{
  GtkWindowGroupPrivate *priv;
  GtkDeviceGrabInfo *info;

  priv = window_group->priv;

  info = g_slice_new0 (GtkDeviceGrabInfo);
  info->widget = widget;
  info->device = device;
  info->block_others = block_others;

  priv->device_grabs = g_slist_prepend (priv->device_grabs, info);
}

void
_gtk_window_group_remove_device_grab (GtkWindowGroup *window_group,
                                      GtkWidget      *widget,
                                      GdkDevice      *device)
{
  GtkWindowGroupPrivate *priv;
  GtkDeviceGrabInfo *info;
  GSList *list, *node = NULL;
  GdkDevice *other_device;

  priv = window_group->priv;
  other_device = gdk_device_get_associated_device (device);
  list = priv->device_grabs;

  while (list)
    {
      info = list->data;

      if (info->widget == widget &&
          (info->device == device ||
           info->device == other_device))
        {
          node = list;
          break;
        }

      list = list->next;
    }

  if (node)
    {
      info = node->data;

      priv->device_grabs = g_slist_delete_link (priv->device_grabs, node);
      g_slice_free (GtkDeviceGrabInfo, info);
    }
}

/**
 * gtk_window_group_get_current_device_grab:
 * @window_group: a #GtkWindowGroup
 * @device: a #GdkDevice
 *
 * Returns the current grab widget for @device, or %NULL if none.
 *
 * Returns: (transfer none): The grab widget, or %NULL
 *
 * Since: 3.0
 */
GtkWidget *
gtk_window_group_get_current_device_grab (GtkWindowGroup *window_group,
                                          GdkDevice      *device)
{
  GtkWindowGroupPrivate *priv;
  GtkDeviceGrabInfo *info;
  GdkDevice *other_device;
  GSList *list;

  g_return_val_if_fail (GTK_IS_WINDOW_GROUP (window_group), NULL);
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  priv = window_group->priv;
  list = priv->device_grabs;
  other_device = gdk_device_get_associated_device (device);

  while (list)
    {
      info = list->data;
      list = list->next;

      if (info->device == device ||
          info->device == other_device)
        return info->widget;
    }

  return NULL;
}

gboolean
_gtk_window_group_widget_is_blocked_for_device (GtkWindowGroup *window_group,
                                                GtkWidget      *widget,
                                                GdkDevice      *device)
{
  GtkWindowGroupPrivate *priv;
  GtkDeviceGrabInfo *info;
  GdkDevice *other_device;
  GSList *list;

  priv = window_group->priv;
  other_device = gdk_device_get_associated_device (device);
  list = priv->device_grabs;

  while (list)
    {
      info = list->data;
      list = list->next;

      /* Look for blocking grabs on other device pairs
       * that have the passed widget within the GTK+ grab.
       */
      if (info->block_others &&
          info->device != device &&
          info->device != other_device &&
          (info->widget == widget ||
           gtk_widget_is_ancestor (widget, info->widget)))
        return TRUE;
    }

  return FALSE;
}
