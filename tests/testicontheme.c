/* testicontheme.c
 * Copyright (C) 2002, 2003  Red Hat, Inc.
 * Authors: Alexander Larsson, Owen Taylor
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

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

static void
usage (void)
{
  g_print ("usage: test-icon-theme lookup <theme name> <icon name> [size] [scale]\n"
	   " or\n"
	   "usage: test-icon-theme list <theme name> [context]\n"
	   " or\n"
	   "usage: test-icon-theme display <theme name> <icon name> [size] [scale]\n"
	   " or\n"
	   "usage: test-icon-theme display-async <theme name> <icon name> [size] [scale]\n"
	   );
}

static void
icon_loaded_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
  GtkIcon *icon;
  GError *error;

  error = NULL;
  icon = gtk_icon_theme_choose_icon_finish (GTK_ICON_THEME (source_object),
                                           res, &error);

  if (icon == NULL)
    {
      g_print ("%s\n", error->message);
      exit (1);
    }

  gtk_image_set_from_paintable (GTK_IMAGE (user_data), GDK_PAINTABLE (icon));
  g_object_unref (icon);
}

int
main (int argc, char *argv[])
{
  GtkIconTheme *icon_theme;
  GtkIcon *icon_info;
  char *context;
  char *themename;
  GList *list;
  int size = 48;
  int scale = 1;
  GtkIconLookupFlags flags;
  
  gtk_init ();

  if (argc < 3)
    {
      usage ();
      return 1;
    }

  flags = GTK_ICON_LOOKUP_USE_BUILTIN;

  if (g_getenv ("RTL"))
    flags |= GTK_ICON_LOOKUP_DIR_RTL;
  else
    flags |= GTK_ICON_LOOKUP_DIR_LTR;

  themename = argv[2];
  
  icon_theme = gtk_icon_theme_new ();
  
  gtk_icon_theme_set_custom_theme (icon_theme, themename);

  if (strcmp (argv[1], "display") == 0)
    {
      GtkIcon *icon;
      GtkWidget *window, *image;

      if (argc < 4)
	{
	  g_object_unref (icon_theme);
	  usage ();
	  return 1;
	}
      
      if (argc >= 5)
	size = atoi (argv[4]);

      if (argc >= 6)
	scale = atoi (argv[5]);

      icon = gtk_icon_theme_lookup_icon (icon_theme, argv[3], size, scale, flags);
      if (!icon)
        {
          g_print ("Icon '%s' not found\n", argv[3]);
          return 1;
        }

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      image = gtk_image_new ();
      gtk_image_set_from_paintable (GTK_IMAGE (image), GDK_PAINTABLE (icon));
      g_object_unref (icon);
      gtk_container_add (GTK_CONTAINER (window), image);
      g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
      gtk_widget_show (window);

      gtk_main ();
    }
  else if (strcmp (argv[1], "display-async") == 0)
    {
      GtkWidget *window, *image;
      const char *icons[2] = { NULL, NULL };

      if (argc < 4)
	{
	  g_object_unref (icon_theme);
	  usage ();
	  return 1;
	}

      if (argc >= 5)
	size = atoi (argv[4]);

      if (argc >= 6)
	scale = atoi (argv[5]);

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      image = gtk_image_new ();
      gtk_container_add (GTK_CONTAINER (window), image);
      g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
      gtk_widget_show (window);

      icons[0] = argv[3];
      gtk_icon_theme_choose_icon_async (icon_theme, icons, size, scale, flags, NULL, icon_loaded_cb, image);

      gtk_main ();
    }
  else if (strcmp (argv[1], "list") == 0)
    {
      if (argc >= 4)
	context = argv[3];
      else
	context = NULL;

      list = gtk_icon_theme_list_icons (icon_theme,
					   context);
      
      while (list)
	{
	  g_print ("%s\n", (char *)list->data);
	  list = list->next;
	}
    }
  else if (strcmp (argv[1], "lookup") == 0)
    {
      if (argc < 4)
	{
	  g_object_unref (icon_theme);
	  usage ();
	  return 1;
	}

      if (argc >= 5)
	size = atoi (argv[4]);

      if (argc >= 6)
	scale = atoi (argv[5]);

      icon_info = gtk_icon_theme_lookup_icon (icon_theme, argv[3], size, scale, flags);
      g_print ("icon for %s at %dx%d@%dx is %s\n", argv[3], size, size, scale,
               icon_info ? gtk_icon_get_filename (icon_info) : "<none>");

      if (icon_info)
	{
          GdkPaintable *paintable = GDK_PAINTABLE (icon_info);

          g_print ("Base size: %d, Scale: %d\n", gtk_icon_get_base_size (icon_info), gtk_icon_get_base_scale (icon_info));
          g_print ("texture size: %dx%d\n", gdk_paintable_get_intrinsic_width (paintable), gdk_paintable_get_intrinsic_height (paintable));

	  g_object_unref (icon_info);
	}
    }
  else
    {
      g_object_unref (icon_theme);
      usage ();
      return 1;
    }
 

  g_object_unref (icon_theme);
  
  return 0;
}
