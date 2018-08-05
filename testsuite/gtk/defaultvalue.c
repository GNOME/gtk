/* Gtk+ default value tests
 * Copyright (C) 2007 Christian Persch
 *               2007 Johan Dahlin
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
 */

#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gtkunixprint.h>

static void
check_property (const char *output,
	        GParamSpec *pspec,
		GValue *value)
{
  GValue default_value = G_VALUE_INIT;
  char *v, *dv, *msg;

  if (g_param_value_defaults (pspec, value))
      return;

  g_value_init (&default_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  g_param_value_set_default (pspec, &default_value);

  v = g_strdup_value_contents (value);
  dv = g_strdup_value_contents (&default_value);

  msg = g_strdup_printf ("%s %s.%s: %s != %s\n",
			 output,
			 g_type_name (pspec->owner_type),
			 pspec->name,
			 dv, v);
  g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__,
		       G_STRFUNC, msg);
  g_free (msg);

  g_free (v);
  g_free (dv);
  g_value_unset (&default_value);
}

static void
test_type (gconstpointer data)
{
  GObjectClass *klass;
  GObject *instance;
  GParamSpec **pspecs;
  guint n_pspecs, i;
  GType type;
  GdkDisplay *display;

  type = * (GType *) data;

  display = gdk_display_get_default ();

  if (!G_TYPE_IS_CLASSED (type))
    return;

  if (G_TYPE_IS_ABSTRACT (type))
    return;

  if (!g_type_is_a (type, G_TYPE_OBJECT))
    return;

  /* These can't be freely constructed/destroyed */
  if (g_type_is_a (type, GTK_TYPE_APPLICATION) ||
      g_type_is_a (type, GDK_TYPE_PIXBUF_LOADER) ||
      g_type_is_a (type, GTK_TYPE_LAYOUT_CHILD) ||
#ifdef G_OS_UNIX
      g_type_is_a (type, GTK_TYPE_PRINT_JOB) ||
#endif
      g_type_is_a (type, gdk_pixbuf_simple_anim_iter_get_type ()) ||
      g_str_equal (g_type_name (type), "GdkX11DeviceManagerXI2") ||
      g_str_equal (g_type_name (type), "GdkX11DeviceManagerCore") ||
      g_str_equal (g_type_name (type), "GdkX11Display") ||
      g_str_equal (g_type_name (type), "GdkX11Screen") ||
      g_str_equal (g_type_name (type), "GdkX11GLContext"))
    return;

  /* This throws a critical when the connection is dropped */
  if (g_type_is_a (type, GTK_TYPE_APP_CHOOSER_DIALOG))
    return;

  /* These leak their GDBusConnections */
  if (g_type_is_a (type, GTK_TYPE_FILE_CHOOSER_BUTTON) ||
      g_type_is_a (type, GTK_TYPE_FILE_CHOOSER_DIALOG) ||
      g_type_is_a (type, GTK_TYPE_FILE_CHOOSER_WIDGET) ||
      g_str_equal (g_type_name (type), "GtkPlacesSidebar"))
    return;
 
  klass = g_type_class_ref (type);

  if (g_type_is_a (type, GTK_TYPE_SETTINGS))
    instance = G_OBJECT (g_object_ref (gtk_settings_get_default ()));
  else if (g_type_is_a (type, GDK_TYPE_SURFACE))
    {
      instance = G_OBJECT (g_object_ref (gdk_surface_new_temp (display,
                                                                &(GdkRectangle) { 0, 0, 100, 100 })));
    }
  else if (g_type_is_a (type, GTK_TYPE_FILTER_LIST_MODEL))
    {
      GListStore *list_store = g_list_store_new (G_TYPE_OBJECT);
      instance = g_object_new (type,
                               "model", list_store,
                               NULL);
      g_object_unref (list_store);
    }
  else if (g_type_is_a (type, GDK_TYPE_CLIPBOARD) ||
           g_str_equal (g_type_name (type), "GdkX11Cursor"))
    instance = g_object_new (type, "display", display, NULL);
  else
    instance = g_object_new (type, NULL);

  if (g_type_is_a (type, G_TYPE_INITIALLY_UNOWNED))
    g_object_ref_sink (instance);

  pspecs = g_object_class_list_properties (klass, &n_pspecs);
  for (i = 0; i < n_pspecs; ++i)
    {
      GParamSpec *pspec = pspecs[i];
      GValue value = G_VALUE_INIT;

      if (pspec->owner_type != type)
	continue;

      if ((pspec->flags & G_PARAM_READABLE) == 0)
	continue;

      if (g_type_is_a (type, GDK_TYPE_CLIPBOARD) &&
	  strcmp (pspec->name, "display") == 0)
	continue;

      /* These are set in init() */
      if ((g_type_is_a (type, GDK_TYPE_CLIPBOARD) ||
           g_type_is_a (type, GDK_TYPE_CONTENT_PROVIDER)) &&
	  strcmp (pspec->name, "formats") == 0)
	continue;

      if (g_type_is_a (type, GDK_TYPE_CONTENT_PROVIDER) &&
	  strcmp (pspec->name, "storable-formats") == 0)
	continue;

      /* This one has a special-purpose default value */
      if (g_type_is_a (type, GTK_TYPE_DIALOG) &&
	  (strcmp (pspec->name, "use-header-bar") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_ASSISTANT) &&
	  (strcmp (pspec->name, "use-header-bar") == 0 ||
           strcmp (pspec->name, "pages") == 0)) /* pages always gets a non-NULL value */
	continue;

      if (g_type_is_a (type, GTK_TYPE_STACK) &&
	  (strcmp (pspec->name, "pages") == 0)) /* pages always gets a non-NULL value */
	continue;

      if (g_type_is_a (type, GTK_TYPE_NOTEBOOK) &&
	  (strcmp (pspec->name, "pages") == 0)) /* pages always gets a non-NULL value */
	continue;

      if (g_type_is_a (type, GTK_TYPE_POPOVER) &&
	  (strcmp (pspec->name, "pointing-to") == 0))
	continue;

      if (g_type_is_a (type, GDK_TYPE_DISPLAY_MANAGER) &&
	  (strcmp (pspec->name, "default-display") == 0))
	continue;

      if (g_type_is_a (type, GDK_TYPE_MONITOR) &&
          (strcmp (pspec->name, "geometry") == 0 ||
           strcmp (pspec->name, "workarea") == 0))
        continue;

      if (g_type_is_a (type, GTK_TYPE_ABOUT_DIALOG) &&
	  (strcmp (pspec->name, "program-name") == 0))
	continue;

      /* These are set to the current date */
      if (g_type_is_a (type, GTK_TYPE_CALENDAR) &&
	  (strcmp (pspec->name, "year") == 0 ||
	   strcmp (pspec->name, "month") == 0 ||
	   strcmp (pspec->name, "day") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_CELL_AREA_CONTEXT) &&
	  (strcmp (pspec->name, "minimum-width") == 0 ||
	   strcmp (pspec->name, "minimum-height") == 0 ||
	   strcmp (pspec->name, "natural-width") == 0 ||
	   strcmp (pspec->name, "natural-height") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_CELL_RENDERER_TEXT) &&
	  (strcmp (pspec->name, "background-gdk") == 0 ||
	   strcmp (pspec->name, "foreground-gdk") == 0 ||
	   strcmp (pspec->name, "background-rgba") == 0 ||
	   strcmp (pspec->name, "foreground-rgba") == 0 ||
	   strcmp (pspec->name, "font") == 0 ||
	   strcmp (pspec->name, "font-desc") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_CELL_VIEW) &&
	  (strcmp (pspec->name, "background-gdk") == 0 ||
	   strcmp (pspec->name, "foreground-gdk") == 0 ||
	   strcmp (pspec->name, "foreground-rgba") == 0 ||
	   strcmp (pspec->name, "background-rgba") == 0 ||
           strcmp (pspec->name, "cell-area") == 0 ||
           strcmp (pspec->name, "cell-area-context") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_COLOR_BUTTON) &&
	  (strcmp (pspec->name, "color") == 0 ||
	   strcmp (pspec->name, "rgba") == 0))
	continue;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

      if (g_type_is_a (type, GTK_TYPE_COMBO_BOX) &&
	  (strcmp (pspec->name, "cell-area") == 0 ||
           strcmp (pspec->name, "cell-area-context") == 0))
	continue;

G_GNUC_END_IGNORE_DEPRECATIONS

      /* Default invisible char is determined at runtime,
       * and buffer gets created on-demand
       */
      if (g_type_is_a (type, GTK_TYPE_ENTRY) &&
	  (strcmp (pspec->name, "invisible-char") == 0 ||
           strcmp (pspec->name, "buffer") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_TEXT) &&
	  (strcmp (pspec->name, "invisible-char") == 0 ||
           strcmp (pspec->name, "buffer") == 0))
	continue;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

      if (g_type_is_a (type, GTK_TYPE_ENTRY_COMPLETION) &&
	  (strcmp (pspec->name, "cell-area") == 0 ||
           strcmp (pspec->name, "cell-area-context") == 0))
	continue;

G_GNUC_END_IGNORE_DEPRECATIONS

      if (g_type_is_a (type, GTK_TYPE_FILTER_LIST_MODEL) &&
          strcmp (pspec->name, "model") == 0)
        continue;

      /* This is set in init() */
      if (g_type_is_a (type, GTK_TYPE_FONT_CHOOSER_WIDGET) &&
          strcmp (pspec->name, "tweak-action") == 0)
        continue;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

      if (g_type_is_a (type, GTK_TYPE_ICON_VIEW) &&
	  (strcmp (pspec->name, "cell-area") == 0 ||
           strcmp (pspec->name, "cell-area-context") == 0))
	continue;

G_GNUC_END_IGNORE_DEPRECATIONS

      if (g_type_is_a (type, GTK_TYPE_MESSAGE_DIALOG) &&
          (strcmp (pspec->name, "image") == 0 ||
           strcmp (pspec->name, "message-area") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_PANED) &&
	  strcmp (pspec->name, "max-position") == 0)
	continue;

      if (g_type_is_a (type, GTK_TYPE_PRINT_OPERATION) &&
	  strcmp (pspec->name, "job-name") == 0)
	continue;

#ifdef G_OS_UNIX
      if (g_type_is_a (type, GTK_TYPE_PRINT_UNIX_DIALOG) &&
	  (strcmp (pspec->name, "page-setup") == 0 ||
	   strcmp (pspec->name, "print-settings") == 0))
	continue;
#endif

      if (g_type_is_a (type, GTK_TYPE_PROGRESS_BAR) &&
          strcmp (pspec->name, "adjustment") == 0)
        continue;

      /* filename value depends on $HOME */
      if (g_type_is_a (type, GTK_TYPE_RECENT_MANAGER) &&
          (strcmp (pspec->name, "filename") == 0 ||
	   strcmp (pspec->name, "size") == 0))
        continue;

      if (g_type_is_a (type, GTK_TYPE_SCALE_BUTTON) &&
          strcmp (pspec->name, "adjustment") == 0)
        continue;

      if (g_type_is_a (type, GTK_TYPE_SCROLLED_WINDOW) &&
	  (strcmp (pspec->name, "hadjustment") == 0 ||
           strcmp (pspec->name, "vadjustment") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_SETTINGS))
        continue;

      if (g_type_is_a (type, GTK_TYPE_SHORTCUT) &&
          strcmp (pspec->name, "trigger") == 0)
        continue;

      if (g_type_is_a (type, GTK_TYPE_SPIN_BUTTON) &&
          (strcmp (pspec->name, "adjustment") == 0))
        continue;

      if (g_type_is_a (type, GTK_TYPE_STYLE_CONTEXT) &&
           strcmp (pspec->name, "display") == 0)
        continue;

      if (g_type_is_a (type, GTK_TYPE_TEXT_BUFFER) &&
          (strcmp (pspec->name, "tag-table") == 0 ||
           strcmp (pspec->name, "copy-target-list") == 0 ||
           strcmp (pspec->name, "paste-target-list") == 0))
        continue;

      /* language depends on the current locale */
      if (g_type_is_a (type, GTK_TYPE_TEXT_TAG) &&
          (strcmp (pspec->name, "background-gdk") == 0 ||
           strcmp (pspec->name, "foreground-gdk") == 0 ||
	   strcmp (pspec->name, "language") == 0 ||
	   strcmp (pspec->name, "font") == 0 ||
	   strcmp (pspec->name, "font-desc") == 0))
        continue;

      if (g_type_is_a (type, GTK_TYPE_TEXT_VIEW) &&
          strcmp (pspec->name, "buffer") == 0)
        continue;

      if (g_type_is_a (type, GTK_TYPE_TREE_VIEW) &&
	  (strcmp (pspec->name, "hadjustment") == 0 ||
           strcmp (pspec->name, "vadjustment") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_TREE_VIEW_COLUMN) &&
	  (strcmp (pspec->name, "cell-area") == 0 ||
           strcmp (pspec->name, "cell-area-context") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_VIEWPORT) &&
	  (strcmp (pspec->name, "hadjustment") == 0 ||
           strcmp (pspec->name, "vadjustment") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_WIDGET) &&
	  (strcmp (pspec->name, "name") == 0 ||
	   strcmp (pspec->name, "display") == 0 ||
	   strcmp (pspec->name, "style") == 0))
	continue;

      /* resize-grip-visible is determined at runtime */
      if (g_type_is_a (type, GTK_TYPE_WINDOW) &&
          strcmp (pspec->name, "resize-grip-visible") == 0)
        continue;

      /* show-desktop depends on desktop environment */
      if (g_str_equal (g_type_name (type), "GtkPlacesSidebar") &&
          strcmp (pspec->name, "show-desktop") == 0)
        continue;

      /* GtkRange constructs an adjustment on its own if NULL is set and
       * the property is a CONSTRUCT one, so the returned value is never NULL. */
      if (g_type_is_a (type, GTK_TYPE_RANGE) &&
          strcmp (pspec->name, "adjustment") == 0)
        continue;

      /* ... and GtkScrollbar wraps that property. */
      if (g_type_is_a (type, GTK_TYPE_SCROLLBAR) &&
          strcmp (pspec->name, "adjustment") == 0)
        continue;

      if (g_test_verbose ())
        {
          g_print ("Property %s:%s\n",
                   g_type_name (pspec->owner_type),
                   pspec->name);
        }

      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_object_get_property (instance, pspec->name, &value);
      check_property ("Property", pspec, &value);
      g_value_unset (&value);
    }
  g_free (pspecs);

  if (g_type_is_a (type, GDK_TYPE_SURFACE))
    gdk_surface_destroy (GDK_SURFACE (instance));
  else
    g_object_unref (instance);

  g_type_class_unref (klass);
}

int
main (int argc, char **argv)
{
  const GType *otypes;
  guint i;
  GTestDBus *bus;
  GMainLoop *loop;
  gint result;

  /* These must be set before before gtk_test_init */
  g_setenv ("GIO_USE_VFS", "local", TRUE);
  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);
  g_setenv ("G_ENABLE_DIAGNOSTIC", "0", TRUE);

  gtk_test_init (&argc, &argv);
  gtk_test_register_all_types();

  /* Create one test bus for all tests, as we have a lot of very small
   * and quick tests.
   */
  bus = g_test_dbus_new (G_TEST_DBUS_NONE);
  g_test_dbus_up (bus);

  otypes = gtk_test_list_all_types (NULL);
  for (i = 0; otypes[i]; i++)
    {
      gchar *testname;

      if (otypes[i] == GTK_TYPE_FILE_CHOOSER_NATIVE)
        continue;

      testname = g_strdup_printf ("/Default Values/%s",
				  g_type_name (otypes[i]));
      g_test_add_data_func (testname,
                            &otypes[i],
			    test_type);
      g_free (testname);
    }

  result = g_test_run();

  /* Work around the annoying issue that g_test_dbus_down is giving
   * us an "Error while sending AddMatch" that comes out of an idle
   */
  loop = g_main_loop_new (NULL, FALSE);
  g_timeout_add (1000, (GSourceFunc)g_main_loop_quit, loop);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  g_test_dbus_down (bus);
  g_object_unref (bus);

  return result;
}
