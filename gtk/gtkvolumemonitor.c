/* GTK - The GIMP Toolkit
 * Copyright (C) 2020 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen
 */

#include "config.h"

#include "gtkvolumemonitor.h"

static GVolumeMonitor *the_volume_monitor;
static GList *pending_tasks;

static void
get_volume_monitor_thread (GTask        *running_task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  GList *l;

  the_volume_monitor = g_volume_monitor_get ();

  g_object_add_weak_pointer (G_OBJECT (the_volume_monitor), (gpointer *)&the_volume_monitor);

  for (l = pending_tasks; l; l = l->next)
    {
      GTask *task = l->data;

      if (!g_task_return_error_if_cancelled (task))
        g_task_return_pointer (task, g_object_ref (the_volume_monitor), g_object_unref);
    }

  g_list_free_full (pending_tasks, g_object_unref);
  pending_tasks = NULL;

  g_object_unref (the_volume_monitor);
}

void
gtk_volume_monitor_get (GAsyncReadyCallback  callback,
                        gpointer             data,
                        GCancellable        *cancellable)
{
  GTask *task;

  task = g_task_new (NULL, cancellable, callback, data);
  g_task_set_return_on_cancel (task, TRUE);

  if (the_volume_monitor)
    {
      g_task_return_pointer (task, g_object_ref (the_volume_monitor), g_object_unref);
      g_object_unref (task);
    }
  else
    {
      pending_tasks = g_list_prepend (pending_tasks, task);
      if (pending_tasks->next == NULL)
        g_task_run_in_thread (task, get_volume_monitor_thread);
    }
}
