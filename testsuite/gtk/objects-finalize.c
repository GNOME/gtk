/* objects-finalize.c
 * Copyright (C) 2013 Openismus GmbH
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Tristan Van Berkom <tristanvb@openismus.com>
 */
#include <gtk/gtk.h>
#include <string.h>

#ifdef GDK_WINDOWING_X11
# include <gdk/gdkx.h>
#endif


typedef GType (*GTypeGetFunc) (void);

static gboolean finalized = FALSE;

static gboolean
main_loop_quit_cb (gpointer data)
{
  gtk_main_quit ();

  g_assert (finalized);
  return FALSE;
}

static void
check_finalized (gpointer data,
		 GObject *where_the_object_was)
{
  gboolean *did_finalize = (gboolean *)data;

  *did_finalize = TRUE;
}

static void
test_finalize_object (gconstpointer data)
{
  GType test_type = GPOINTER_TO_SIZE (data);
  GObject *object;

  object = g_object_new (test_type, NULL);
  g_assert (G_IS_OBJECT (object));

  /* Make sure we have the only reference */
  if (g_object_is_floating (object))
    g_object_ref_sink (object);

  /* Assert that the object finalizes properly */
  g_object_weak_ref (object, check_finalized, &finalized);

  /* Toplevels are owned by GTK+, just tell GTK+ to destroy it */
  if (GTK_IS_WINDOW (object) || GTK_IS_INVISIBLE (object))
    gtk_widget_destroy (GTK_WIDGET (object));
  else
    g_object_unref (object);

  /* Even if the object did finalize, it may have left some dangerous stuff in the GMainContext */
  g_timeout_add (50, main_loop_quit_cb, NULL);
  gtk_main();
}

int
main (int argc, char **argv)
{
  const GType *all_types;
  guint n_types = 0, i;
  gchar *schema_dir;
  GTestDBus *bus;
  gint result;

  /* These must be set before before gtk_test_init */
  g_setenv ("GIO_USE_VFS", "local", TRUE);
  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);

  /* initialize test program */
  gtk_test_init (&argc, &argv);
  gtk_test_register_all_types ();

  /* g_test_build_filename must be called after gtk_test_init */
  schema_dir = g_test_build_filename (G_TEST_BUILT, "", NULL);
  g_setenv ("GSETTINGS_SCHEMA_DIR", schema_dir, TRUE);

  /* Create one test bus for all tests, as we have a lot of very small
   * and quick tests.
   */
  bus = g_test_dbus_new (G_TEST_DBUS_NONE);
  g_test_dbus_up (bus);

  all_types = gtk_test_list_all_types (&n_types);

  for (i = 0; i < n_types; i++)
    {
      if (g_type_is_a (all_types[i], G_TYPE_OBJECT) &&
	  G_TYPE_IS_INSTANTIATABLE (all_types[i]) &&
	  !G_TYPE_IS_ABSTRACT (all_types[i]) &&
#ifdef GDK_WINDOWING_X11
	  all_types[i] != GDK_TYPE_X11_WINDOW &&
	  all_types[i] != GDK_TYPE_X11_CURSOR &&
	  all_types[i] != GDK_TYPE_X11_SCREEN &&
	  all_types[i] != GDK_TYPE_X11_DISPLAY &&
	  all_types[i] != GDK_TYPE_X11_DEVICE_MANAGER_XI2 &&
	  all_types[i] != GDK_TYPE_X11_DISPLAY_MANAGER &&
#endif
	  /* Not allowed to finalize a GdkPixbufLoader without calling gdk_pixbuf_loader_close() */
	  all_types[i] != GDK_TYPE_PIXBUF_LOADER &&
	  all_types[i] != gdk_pixbuf_simple_anim_iter_get_type())
	{
	  gchar *test_path = g_strdup_printf ("/FinalizeObject/%s", g_type_name (all_types[i]));

	  g_test_add_data_func (test_path, GSIZE_TO_POINTER (all_types[i]), test_finalize_object);

	  g_free (test_path);
	}
    }

  result = g_test_run();

  g_test_dbus_down (bus);
  g_object_unref (bus);
  g_free (schema_dir);

  return result;
}
