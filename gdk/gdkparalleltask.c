/*
 * Copyright © 2024 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gdkparalleltaskprivate.h"
#include "gdkdebugprivate.h"

typedef struct _TaskData TaskData;

struct _TaskData
{
  GdkTaskFunc task_func;
  gpointer task_data;
  int n_running_tasks;
};

static void
gdk_parallel_task_thread_func (gpointer data,
                               gpointer unused)
{
  TaskData *task = data;

  task->task_func (task->task_data);

  g_atomic_int_add (&task->n_running_tasks, -1);
}

/**
 * gdk_parallel_task_run:
 * @task_func: the function to spawn
 * @task_data: data to pass to the function
 * @max_tasks: maximum number of tasks to spawn
 *
 * Spawns the given function in many threads.
 * Once all functions have exited, this function returns.
 **/
void
gdk_parallel_task_run (GdkTaskFunc task_func,
                       gpointer    task_data,
                       guint       max_tasks)
{
  static GThreadPool *pool;
  static guint nproc;
  TaskData task = {
    .task_func = task_func,
    .task_data = task_data,
  };
  int i, n_tasks;

  if (max_tasks == 1 || !gdk_has_feature (GDK_FEATURE_THREADS))
    {
      task_func (task_data);
      return;
    }

  if (g_once_init_enter (&pool))
    {
      nproc = g_get_num_processors ();
      guint num_threads = CLAMP (2, nproc - 1, 32);
      GThreadPool *the_pool = g_thread_pool_new (gdk_parallel_task_thread_func,
                                                 NULL,
                                                 num_threads,
                                                 FALSE,
                                                 NULL);
      g_thread_pool_set_max_unused_threads (num_threads);
      g_once_init_leave (&pool, the_pool);
    }

  n_tasks = MIN (max_tasks, nproc);
  task.n_running_tasks = n_tasks;
  /* Start with 1 because we run 1 task ourselves */
  for (i = 1; i < n_tasks; i++)
    {
      g_thread_pool_push (pool, &task, NULL);
    }

  gdk_parallel_task_thread_func (&task, NULL);

  while (g_atomic_int_get (&task.n_running_tasks) > 0)
    g_thread_yield ();
}

