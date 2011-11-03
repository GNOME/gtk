/* accel.c - test accel parsing
 * Copyright (C) 2011 Bastien Nocera <hadess@hadess.net>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <gtk/gtk.h>
#include <locale.h>

static void
test_one_accel (const char *accel,
		const char *exp_label)
{
  guint accel_key;
  GdkModifierType mods;
  guint *keycodes;
  char *label, *name;

  gtk_accelerator_parse_with_keycode (accel,
				      &accel_key,
				      &keycodes,
				      &mods);

  g_assert (accel_key != 0);
  g_assert (keycodes);
  g_assert (keycodes[0] != 0);

  label = gtk_accelerator_get_label_with_keycode (NULL,
						  accel_key,
						  *keycodes,
						  mods);

  g_assert_cmpstr (label, ==, exp_label);

  name = gtk_accelerator_name_with_keycode (NULL,
					    accel_key,
					    *keycodes,
					    mods);
  g_assert_cmpstr (name, ==, accel);

  g_free (keycodes);
  g_free (label);
  g_free (name);
}

static void
accel (void)
{
  test_one_accel ("<Primary><Alt>z", "Ctrl+Alt+Z");
}

int
main (int   argc,
      char *argv[])
{
  setlocale (LC_ALL, "en_GB.UTF-8");

  gtk_test_init (&argc, &argv);
  g_test_add_func ("/accel", accel);
  return g_test_run();
}
