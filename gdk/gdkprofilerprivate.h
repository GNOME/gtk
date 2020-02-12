/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2018 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_PROFILER_PRIVATE_H__
#define __GDK_PROFILER_PRIVATE_H__

#include "gdk/gdkframeclock.h"
#include "gdk/gdkdisplay.h"

G_BEGIN_DECLS

/* Ensure we included config.h as needed for the below HAVE_SYSPROF_CAPTURE check */
#ifndef GETTEXT_PACKAGE
#error "config.h was not included before gdkprofilerprivate.h."
#endif

/* We make this a macro you use as if (GDK_PROFILER_IS_RUNNING) because that
 * way we can ensure all the code is compiled out when not supported, and
 * we can add a G_UNLIKELY() for better codegen if it is.
 */
#ifdef HAVE_SYSPROF_CAPTURE
#define GDK_PROFILER_IS_RUNNING G_UNLIKELY (gdk_profiler_is_running ())
#else
#define GDK_PROFILER_IS_RUNNING FALSE
#endif

void     gdk_profiler_start      (int fd);
void     gdk_profiler_stop       (void);
gboolean gdk_profiler_is_running (void);
void     gdk_profiler_add_mark   (gint64           start,
                                  guint64          duration,
                                  const char      *name,
                                  const char      *message);
void     gdk_profiler_add_markf   (gint64           start,
                                   guint64          duration,
                                   const char      *name,
                                   const char      *format,
                                   ...)  G_GNUC_PRINTF (4, 5);
void     gdk_profiler_end_mark   (gint64           start,
                                  const char      *name,
                                  const char      *message);
void     gdk_profiler_end_markf   (gint64           start,
                                   const char      *name,
                                   const char      *format,
                                   ...)  G_GNUC_PRINTF (3, 4);
guint    gdk_profiler_define_counter (const char *name,
                                      const char *description);
void     gdk_profiler_set_counter    (guint  id,
                                      gint64 time,
                                      double value);
guint    gdk_profiler_define_int_counter (const char *name,
                                          const char *description);
void     gdk_profiler_set_int_counter (guint  id,
                                       gint64 time,
                                       gint64 value);

G_END_DECLS

#endif  /* __GDK_PROFILER_PRIVATE_H__ */
