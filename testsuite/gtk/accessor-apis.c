/*
 * Copyright © 2020 Benjamin Otte
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

#include <gtk/gtk.h>

static struct
{
  const char *expected;
  const char *alternative;
} exceptions[] = {
  /* keep this sorted please */
  { "gtk_window_get_display", "gtk_widget_get_display" },
};

static GModule *module;

static gboolean
function_exists (const char *function_name)
{
  gpointer func;
  guint i;

  if (g_module_symbol (module, function_name, &func) && func)
    return TRUE;

  for (i = 0; i < G_N_ELEMENTS(exceptions); i++)
    {
      if (g_str_equal (function_name, exceptions[i].expected))
        {
          if (exceptions[i].alternative)
            return function_exists (exceptions[i].alternative);
          else
            return TRUE;
        }
    }

  return FALSE;
}

/* Keep in sync with gtkbuilder.c ! */
static char *
type_name_mangle (const char *name,
                  gboolean    split_first_cap)
{
  GString *symbol_name = g_string_new ("");
  gint i;

  for (i = 0; name[i] != '\0'; i++)
    {
      /* skip if uppercase, first or previous is uppercase */
      if ((name[i] == g_ascii_toupper (name[i]) &&
             ((i > 0 && name[i-1] != g_ascii_toupper (name[i-1])) ||
              (i == 1 && name[0] == g_ascii_toupper (name[0]) && split_first_cap))) ||
           (i > 2 && name[i]  == g_ascii_toupper (name[i]) &&
           name[i-1] == g_ascii_toupper (name[i-1]) &&
           name[i-2] == g_ascii_toupper (name[i-2])))
        g_string_append_c (symbol_name, '_');
      g_string_append_c (symbol_name, g_ascii_tolower (name[i]));
    }

  return g_string_free (symbol_name, FALSE);
}

static void
property_name_mangle (GString    *symbol_name,
                      const char *name)
{
  guint i;

  for (i = 0; name[i] != '\0'; i++)
    {
      if (g_ascii_isalnum (name[i]))
        g_string_append_c (symbol_name, g_ascii_tolower (name[i]));
      else
        g_string_append_c (symbol_name, '_');
    }
}

const char *getters[] = { "get", "is", "ref" };
const char *setters[] = { "set" };

static char **
get_potential_names (GType       type,
                     gboolean    get,
                     const char *property_name)
{
  GPtrArray *options;
  char *type_name_options[2];
  guint n_type_name_options, n_verbs;
  const char **verbs;
  guint i, j;

  if (get)
    {
      verbs = getters;
      n_verbs = G_N_ELEMENTS (getters);
    }
  else
    {
      verbs = setters;
      n_verbs = G_N_ELEMENTS (setters);
    }

  type_name_options[0] = type_name_mangle (g_type_name (type), FALSE);
  type_name_options[1] = type_name_mangle (g_type_name (type), TRUE);
  if (g_str_equal (type_name_options[0], type_name_options[1]))
    n_type_name_options = 1;
  else
    n_type_name_options = 2;

  options = g_ptr_array_new ();

  for (i = 0; i < n_type_name_options; i++)
    {
      for (j = 0; j < n_verbs; j++)
        {
          GString *str;

          str = g_string_new (type_name_options[i]);
          g_string_append_c (str, '_');
          g_string_append (str, verbs[j]);
          g_string_append_c (str, '_');
          property_name_mangle (str, property_name);

          g_ptr_array_add (options, g_string_free (str, FALSE));
        }
    }

  g_free (type_name_options[0]);
  g_free (type_name_options[1]);

  g_ptr_array_add (options, NULL);

  return (char **) g_ptr_array_free (options, FALSE);
}

static void
check_function_name (GType       type,
                     gboolean    get,
                     const char *property_name)
{
  guint i;
  char **names;

  names = get_potential_names (type, get, property_name);

  for (i = 0; names[i] != NULL; i++)
    {
      if (function_exists (names[i]))
        {
          g_strfreev (names);
          return;
        }
    }

  g_test_message ("No %s for property %s::%s", get ? "getter" : "setter", g_type_name (type), property_name);
  for (i = 0; names[i] != NULL; i++)
    {
      g_test_message ("    %s", names[i]);
    }

  g_test_fail ();

  g_strfreev (names);
}

static void
check_property (GParamSpec *pspec)
{
  if (pspec->flags & G_PARAM_READABLE)
    {
      check_function_name (pspec->owner_type,
                           TRUE,
                           pspec->name);
    }
  if (pspec->flags & G_PARAM_WRITABLE &&
      !(pspec->flags & G_PARAM_CONSTRUCT_ONLY))
    {
      check_function_name (pspec->owner_type,
                           FALSE,
                           pspec->name);
    }
}

static void
test_accessors (gconstpointer data)
{
  GType type = GPOINTER_TO_SIZE (data);
  GObjectClass *klass;
  GParamSpec **pspecs;
  guint i, n_pspecs;

  klass = g_type_class_ref (type);
  pspecs = g_object_class_list_properties (klass, &n_pspecs);

  for (i = 0; i < n_pspecs; ++i)
    {
      GParamSpec *pspec = pspecs[i];

      if (pspec->owner_type != type)
	continue;

      check_property (pspec);
    }

  g_free (pspecs);
  g_type_class_unref (klass);
}

#if 0
static gboolean
dbind_warning_handler (const char     *log_domain,
                       GLogLevelFlags  log_level,
                       const char     *message,
                       gpointer        user_data)
{
  if (strcmp (log_domain, "dbind") == 0 &&
      log_level == (G_LOG_LEVEL_WARNING|G_LOG_FLAG_FATAL))
    return FALSE;

  return TRUE;
}
#endif

int
main (int argc, char **argv)
{
  const GType *all_types;
  guint n_types = 0, i;
  gint result;
#if 0
  GTestDBus *bus;
  const char *display, *x_r_d;
#endif

  /* These must be set before before gtk_test_init */
  g_setenv ("GIO_USE_VFS", "local", TRUE);
  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);

#if 0
  /* g_test_dbus_up() helpfully clears these, so we have to re-set it */
  display = g_getenv ("DISPLAY");
  x_r_d = g_getenv ("XDG_RUNTIME_DIR");

  /* Create one test bus for all tests, as we have a lot of very small
   * and quick tests.
   */
  bus = g_test_dbus_new (G_TEST_DBUS_NONE);
  g_test_dbus_up (bus);

  if (display)
    g_setenv ("DISPLAY", display, TRUE);
  if (x_r_d)
    g_setenv ("XDG_RUNTIME_DIR", x_r_d, TRUE);

  g_test_log_set_fatal_handler (dbind_warning_handler, NULL);
#endif

  /* initialize test program */
  gtk_test_init (&argc, &argv);
  gtk_test_register_all_types ();

  module = g_module_open (NULL, G_MODULE_BIND_LAZY);

  all_types = gtk_test_list_all_types (&n_types);

  for (i = 0; i < n_types; i++)
    {
      char *test_path;

      if (!G_TYPE_IS_INSTANTIATABLE (all_types[i]) ||
          !g_type_is_a (all_types[i], G_TYPE_OBJECT))
        continue;

      test_path = g_strdup_printf ("/accessor-apis/%s", g_type_name (all_types[i]));

      g_test_add_data_func (test_path, GSIZE_TO_POINTER (all_types[i]), test_accessors);

      g_free (test_path);
    }

  result = g_test_run();

  g_module_close (module);
#if 0
  g_test_dbus_down (bus);
  g_object_unref (bus);
#endif

  return result;
}
