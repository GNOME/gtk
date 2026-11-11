/* toplevelsize.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <gdk/gdk.h>

#include "gdk/gdktoplevelsizeprivate.h"

static void
test_proposal_shadows (void)
{
  GdkToplevelSize size;
  int width, height;

  gdk_toplevel_size_init (&size, 1920, 1080);
  size.proposed_width = 801;
  size.proposed_height = 603;

  gdk_toplevel_size_set_shadow_width (&size, 7, 9, 11, 13);
  g_assert_true (gdk_toplevel_size_get_proposed_size (&size, &width, &height));
  g_assert_cmpint (width, ==, 817);
  g_assert_cmpint (height, ==, 627);

  gdk_toplevel_size_set_shadow_width (&size, 2, 3, 4, 5);
  g_assert_true (gdk_toplevel_size_get_proposed_size (&size, &width, &height));
  g_assert_cmpint (width, ==, 806);
  g_assert_cmpint (height, ==, 612);

  size.proposed_width = G_MAXINT;
  g_assert_true (gdk_toplevel_size_get_proposed_size (&size, &width, NULL));
  g_assert_cmpint (width, ==, G_MAXINT);
}

static void
test_unspecified_axes (void)
{
  GdkToplevelSize size;
  int width, height;

  gdk_toplevel_size_init (&size, 1920, 1080);
  gdk_toplevel_size_set_shadow_width (&size, 7, 9, 11, 13);

  g_assert_false (gdk_toplevel_size_get_proposed_size (&size, &width, &height));
  g_assert_cmpint (width, ==, 0);
  g_assert_cmpint (height, ==, 0);

  size.proposed_width = 801;
  g_assert_true (gdk_toplevel_size_get_proposed_size (&size, &width, &height));
  g_assert_cmpint (width, ==, 817);
  g_assert_cmpint (height, ==, 0);

  size.proposed_width = 0;
  size.proposed_height = 603;
  g_assert_true (gdk_toplevel_size_get_proposed_size (&size, &width, &height));
  g_assert_cmpint (width, ==, 0);
  g_assert_cmpint (height, ==, 627);
}

static void
test_explicit_reply (void)
{
  GdkToplevelSize size;
  int width, height;

  gdk_toplevel_size_init (&size, 1920, 1080);
  size.proposed_width = 803;
  size.proposed_height = 607;

  g_assert_true (gdk_toplevel_size_get_proposed_size (&size, NULL, NULL));
  gdk_toplevel_size_set_size (&size, 400, 300);
  g_assert_false (size.reply_to_proposal);

  gdk_toplevel_size_set_size_for_proposal (&size, 800, 600);
  g_assert_true (size.reply_to_proposal);

  gdk_toplevel_size_set_size (&size, 790, 580);
  gdk_toplevel_size_get_size (&size, &width, &height);
  g_assert_true (size.reply_to_proposal);
  g_assert_cmpint (width, ==, 790);
  g_assert_cmpint (height, ==, 580);

  gdk_toplevel_size_init (&size, 1920, 1080);
  g_assert_false (size.reply_to_proposal);
  g_assert_false (gdk_toplevel_size_get_proposed_size (&size, NULL, NULL));
}

int
main (int argc, char **argv)
{
  (g_test_init) (&argc, &argv, NULL);
  g_test_add_func ("/toplevel-size/proposal-shadows", test_proposal_shadows);
  g_test_add_func ("/toplevel-size/unspecified-axes", test_unspecified_axes);
  g_test_add_func ("/toplevel-size/explicit-reply", test_explicit_reply);
  return g_test_run ();
}
