/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
#include "../config.h"
#include "gdk.h"
#include "gdkprivate.h"

#ifdef USE_PTHREADS
#include <pthread.h>

pthread_mutex_t gdk_threads_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif /* USE_PTHREADS */

gboolean
gdk_threads_init (void)
{
#ifdef USE_PTHREADS  
  pipe (gdk_threads_pipe);
  gdk_using_threads = TRUE;
  return TRUE;
#else
  return FALSE;
#endif
}

void
gdk_threads_enter (void)
{
#ifdef USE_PTHREADS
  pthread_mutex_lock (&gdk_threads_mutex);
#endif  
}

void
gdk_threads_leave (void)
{
#ifdef USE_PTHREADS
  pthread_mutex_unlock (&gdk_threads_mutex);
#endif  
}

void          
gdk_threads_wake (void)
{
#ifdef USE_PTHREADS
  if (gdk_select_waiting)
    {
      gdk_select_waiting = FALSE;
      write (gdk_threads_pipe[1], "A", 1);
    }
#endif  
}
