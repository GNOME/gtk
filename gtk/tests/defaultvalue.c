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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#undef GTK_DISABLE_DEPRECATED
#define GTK_ENABLE_BROKEN
#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gtkunixprint.h>

static void
check_property (const char *output,
	        GParamSpec *pspec,
		GValue *value)
{
  GValue default_value = { 0, };
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

  type = * (GType *) data;

  if (!G_TYPE_IS_CLASSED (type))
    return;

  if (G_TYPE_IS_ABSTRACT (type))
    return;

  if (!g_type_is_a (type, G_TYPE_OBJECT))
    return;

  /* These can't be freely constructed/destroyed */
  if (g_type_is_a (type, GTK_TYPE_PRINT_JOB) ||
      g_type_is_a (type, GDK_TYPE_PIXBUF_LOADER) ||
      g_type_is_a (type, gdk_pixbuf_simple_anim_iter_get_type ()))
    return;

  /* The gtk_arg compat wrappers can't set up default values */
  if (g_type_is_a (type, GTK_TYPE_CLIST) ||
      g_type_is_a (type, GTK_TYPE_CTREE) ||
      g_type_is_a (type, GTK_TYPE_LIST) ||
      g_type_is_a (type, GTK_TYPE_TIPS_QUERY)) 
    return;

  klass = g_type_class_ref (type);
  
  if (g_type_is_a (type, GTK_TYPE_SETTINGS))
    instance = g_object_ref (gtk_settings_get_default ());
  else if (g_type_is_a (type, GDK_TYPE_PANGO_RENDERER))
    instance = g_object_ref (gdk_pango_renderer_get_default (gdk_screen_get_default ()));
  else if (g_type_is_a (type, GDK_TYPE_PIXMAP))
    instance = g_object_ref (gdk_pixmap_new (NULL, 1, 1, 1));
  else if (g_type_is_a (type, GDK_TYPE_COLORMAP))
    instance = g_object_ref (gdk_colormap_new (gdk_visual_get_best (), TRUE));
  else if (g_type_is_a (type, GDK_TYPE_WINDOW))
    {
      GdkWindowAttr attributes;
      attributes.window_type = GDK_WINDOW_TEMP;
      attributes.event_mask = 0;
      attributes.width = 100;
      attributes.height = 100;
      instance = g_object_ref (gdk_window_new (NULL, &attributes, 0));
    }
  else
    instance = g_object_new (type, NULL);

  if (g_type_is_a (type, G_TYPE_INITIALLY_UNOWNED))
    g_object_ref_sink (instance);

  pspecs = g_object_class_list_properties (klass, &n_pspecs);
  for (i = 0; i < n_pspecs; ++i)
    {
      GParamSpec *pspec = pspecs[i];
      GValue value = { 0, };
      
      if (pspec->owner_type != type)
	continue;

      if ((pspec->flags & G_PARAM_READABLE) == 0)
	continue;

      if (g_type_is_a (type, GDK_TYPE_DISPLAY_MANAGER) &&
	  (strcmp (pspec->name, "default-display") == 0))
	continue;

      if (g_type_is_a (type, GDK_TYPE_PANGO_RENDERER) &&
	  (strcmp (pspec->name, "screen") == 0))
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

      if (g_type_is_a (type, GTK_TYPE_CELL_RENDERER_TEXT) &&
	  (strcmp (pspec->name, "background-gdk") == 0 ||
	   strcmp (pspec->name, "foreground-gdk") == 0 ||
	   strcmp (pspec->name, "font") == 0 ||
	   strcmp (pspec->name, "font-desc") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_CELL_VIEW) &&
	  (strcmp (pspec->name, "background-gdk") == 0 ||
	   strcmp (pspec->name, "foreground-gdk") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_COLOR_BUTTON) &&
	  strcmp (pspec->name, "color") == 0)
	continue;

      if (g_type_is_a (type, GTK_TYPE_COLOR_SELECTION) &&
	  strcmp (pspec->name, "current-color") == 0)
	continue;

      if (g_type_is_a (type, GTK_TYPE_COLOR_SELECTION_DIALOG) &&
	  (strcmp (pspec->name, "color-selection") == 0 ||
	   strcmp (pspec->name, "ok-button") == 0 ||
	   strcmp (pspec->name, "help-button") == 0 ||
	   strcmp (pspec->name, "cancel-button") == 0))
	continue;

      /* Default invisible char is determined at runtime */
      if (g_type_is_a (type, GTK_TYPE_ENTRY) &&
	  (strcmp (pspec->name, "invisible-char") == 0 ||
           strcmp (pspec->name, "buffer") == 0))
	continue;

      /* Gets set to the cwd */
      if (g_type_is_a (type, GTK_TYPE_FILE_SELECTION) &&
	  strcmp (pspec->name, "filename") == 0)
	continue;

      if (g_type_is_a (type, GTK_TYPE_FONT_SELECTION) &&
	  strcmp (pspec->name, "font") == 0)
	continue;

      if (g_type_is_a (type, GTK_TYPE_LAYOUT) &&
	  (strcmp (pspec->name, "hadjustment") == 0 ||
           strcmp (pspec->name, "vadjustment") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_MESSAGE_DIALOG) &&
          (strcmp (pspec->name, "image") == 0 ||
           strcmp (pspec->name, "message-area") == 0))

	continue;

      if (g_type_is_a (type, GTK_TYPE_PRINT_OPERATION) &&
	  strcmp (pspec->name, "job-name") == 0)
	continue;

      if (g_type_is_a (type, GTK_TYPE_PRINT_UNIX_DIALOG) &&
	  (strcmp (pspec->name, "page-setup") == 0 ||
	   strcmp (pspec->name, "print-settings") == 0))
	continue;

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

      /* these defaults come from XResources */
      if (g_type_is_a (type, GTK_TYPE_SETTINGS) &&
          strncmp (pspec->name, "gtk-xft-", 8) == 0)
        continue;

      if (g_type_is_a (type, GTK_TYPE_SETTINGS) &&
          (strcmp (pspec->name, "color-hash") == 0 ||
	   strcmp (pspec->name, "gtk-cursor-theme-name") == 0 ||
	   strcmp (pspec->name, "gtk-cursor-theme-size") == 0 ||
	   strcmp (pspec->name, "gtk-dnd-drag-threshold") == 0 ||
	   strcmp (pspec->name, "gtk-double-click-time") == 0 ||
	   strcmp (pspec->name, "gtk-fallback-icon-theme") == 0 ||
	   strcmp (pspec->name, "gtk-file-chooser-backend") == 0 ||
	   strcmp (pspec->name, "gtk-icon-theme-name") == 0 ||
	   strcmp (pspec->name, "gtk-im-module") == 0 ||
	   strcmp (pspec->name, "gtk-key-theme-name") == 0 ||
	   strcmp (pspec->name, "gtk-theme-name") == 0 ||
           strcmp (pspec->name, "gtk-sound-theme-name") == 0 ||
           strcmp (pspec->name, "gtk-enable-input-feedback-sounds") == 0 ||
           strcmp (pspec->name, "gtk-enable-event-sounds") == 0))
        continue;

      if (g_type_is_a (type, GTK_TYPE_SPIN_BUTTON) &&
          (strcmp (pspec->name, "adjustment") == 0))
        continue;

      if (g_type_is_a (type, GTK_TYPE_STATUS_ICON) &&
          (strcmp (pspec->name, "size") == 0 ||
           strcmp (pspec->name, "screen") == 0))
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

      if (g_type_is_a (type, GTK_TYPE_TEXT) &&
	  (strcmp (pspec->name, "hadjustment") == 0 ||
           strcmp (pspec->name, "vadjustment") == 0))
        continue;

      if (g_type_is_a (type, GTK_TYPE_TEXT_VIEW) &&
          strcmp (pspec->name, "buffer") == 0)
        continue;

      if (g_type_is_a (type, GTK_TYPE_TOOL_ITEM_GROUP) &&
          strcmp (pspec->name, "label-widget") == 0)
        continue;

      if (g_type_is_a (type, GTK_TYPE_TREE_VIEW) &&
	  (strcmp (pspec->name, "hadjustment") == 0 ||
           strcmp (pspec->name, "vadjustment") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_VIEWPORT) &&
	  (strcmp (pspec->name, "hadjustment") == 0 ||
           strcmp (pspec->name, "vadjustment") == 0))
	continue;

      if (g_type_is_a (type, GTK_TYPE_WIDGET) &&
	  (strcmp (pspec->name, "name") == 0 ||
	   strcmp (pspec->name, "screen") == 0 ||
	   strcmp (pspec->name, "style") == 0))
	continue;

      if (g_test_verbose ())
      g_print ("Property %s.%s\n", 
	     g_type_name (pspec->owner_type),
	     pspec->name);
      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_object_get_property (instance, pspec->name, &value);
      check_property ("Property", pspec, &value);
      g_value_unset (&value);
    }
  g_free (pspecs);

  if (g_type_is_a (type, GTK_TYPE_WIDGET))
    {
      pspecs = gtk_widget_class_list_style_properties (GTK_WIDGET_CLASS (klass), &n_pspecs);
      
      for (i = 0; i < n_pspecs; ++i)
	{
	  GParamSpec *pspec = pspecs[i];
	  GValue value = { 0, };
	  
	  if (pspec->owner_type != type)
	    continue;

	  if ((pspec->flags & G_PARAM_READABLE) == 0)
	    continue;
	  
	  g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
	  gtk_widget_style_get_property (GTK_WIDGET (instance), pspec->name, &value);
	  check_property ("Style property", pspec, &value);
	  g_value_unset (&value);
	}

      g_free (pspecs);
    }
  
  if (g_type_is_a (type, GDK_TYPE_WINDOW))
    gdk_window_destroy (GDK_WINDOW (instance));
  else
    g_object_unref (instance);
  
  g_type_class_unref (klass);
}

extern void pixbuf_init (void);

int
main (int argc, char **argv)
{
  const GType *otypes;
  guint i;

  gtk_test_init (&argc, &argv);
  pixbuf_init ();
  gtk_test_register_all_types();
  
  otypes = gtk_test_list_all_types (NULL);
  for (i = 0; otypes[i]; i++)
    {
      gchar *testname;
      
      testname = g_strdup_printf ("/Default Values/%s",
				  g_type_name (otypes[i]));
      g_test_add_data_func (testname,
                            &otypes[i],
			    test_type);
      g_free (testname);
    }
  
  return g_test_run();
}
