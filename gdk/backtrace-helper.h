/* backtrace-helper.h
 *
 * Copyright 2020-2023 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <sysprof-capture.h>

#ifdef HAVE_EXECINFO_H
# include <execinfo.h>
#endif
#ifdef ENABLE_LIBUNWIND
# define UNW_LOCAL_ONLY
# include <libunwind.h>
#endif

static void
backtrace_init (void)
{
#ifdef ENABLE_LIBUNWIND
  unw_set_caching_policy (unw_local_addr_space, UNW_CACHE_PER_THREAD);
# ifdef HAVE_UNW_SET_CACHE_SIZE
  unw_set_cache_size (unw_local_addr_space, 1024, 0);
#endif
#endif
}

static int
backtrace_func (SysprofCaptureAddress  *addrs,
                guint                   n_addrs,
                G_GNUC_UNUSED gpointer  user_data)
{
#if defined(ENABLE_LIBUNWIND)
# if GLIB_SIZEOF_VOID_P == 8
  /* We know that collector will overwrite fields *AFTER* it
   * has called the backtrace function allowing us to cheat
   * and subtract an offset from addrs to avoid having to
   * copy frame pointers around.
   */
  return unw_backtrace ((void **)addrs - 2, n_addrs) - 2;
# else
  static const int skip = 2;
  void **stack = alloca (n_addrs * sizeof (gpointer));
  int n = unw_backtrace (stack, n_addrs);
  for (guint i = skip; i < n; i++)
    addrs[i-skip] = GPOINTER_TO_SIZE (stack[i]);
  return MAX (0, n - skip);
# endif
#elif defined(HAVE_EXECINFO_H)
# if GLIB_SIZEOF_VOID_P == 8
  /* See note on unw_backtrace() */
  return backtrace ((void **)addrs - 2, n_addrs) - 2;
# else /* GLIB_SIZEOF_VOID_P != 8 */
  static const int skip = 2;
  void **stack = alloca (n_addrs * sizeof (gpointer));
  int n = backtrace (stack, n_addrs);
  for (guint i = skip; i < n; i++)
    addrs[i-skip] = GPOINTER_TO_SIZE (stack[i]);
  return MAX (0, n - skip);
# endif /* GLIB_SIZEOF_VOID_P */
#else
  return 0;
#endif
}

