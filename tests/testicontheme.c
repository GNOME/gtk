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
	   );
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char *argv[])
{
  GtkIconTheme *icon_theme;
  char *themename;
  int size = 48;
  int scale = 1;
  GtkIconLookupFlags flags;
  GtkTextDirection direction;
  
  gtk_init ();

  if (argc < 3)
    {
      usage ();
      return 1;
    }

  flags = 0;

  if (g_getenv ("RTL"))
    direction = GTK_TEXT_DIR_RTL;
  else if (g_getenv ("LTR"))
    direction = GTK_TEXT_DIR_LTR;
  else
    direction = GTK_TEXT_DIR_NONE;

  themename = argv[2];
  
  icon_theme = gtk_icon_theme_new ();
  
  gtk_icon_theme_set_theme_name (icon_theme, themename);

  if (strcmp (argv[1], "display") == 0)
    {
      GtkIconPaintable *icon;
      GtkWidget *window, *image;
      gboolean done = FALSE;

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

      icon = gtk_icon_theme_lookup_icon (icon_theme, argv[3], NULL, size, scale, direction, flags);
      if (!icon)
        {
          g_print ("Icon '%s' not found\n", argv[3]);
          return 1;
        }

      window = gtk_window_new ();
      image = gtk_image_new ();
      gtk_image_set_from_paintable (GTK_IMAGE (image), GDK_PAINTABLE (icon));
      g_object_unref (icon);
      gtk_container_add (GTK_CONTAINER (window), image);
      g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);
      gtk_widget_show (window);

      while (!done)
        g_main_context_iteration (NULL, TRUE);
    }
  else if (strcmp (argv[1], "list") == 0)
    {
      char **icons;
      int i;

      icons = gtk_icon_theme_get_icon_names (icon_theme);
      for (i = 0; icons[i]; i++)
	g_print ("%s\n", icons[i]);
      g_strfreev (icons);
    }
  else if (strcmp (argv[1], "lookup") == 0)
    {
      GFile *file;
      GtkIconPaintable *icon;

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

      icon = gtk_icon_theme_lookup_icon (icon_theme, argv[3], NULL, size, scale, direction, flags);
      file = gtk_icon_paintable_get_file (icon);
      g_print ("icon for %s at %dx%d@%dx is %s\n", argv[3], size, size, scale,
               file ? g_file_get_uri (file) : "<none>");

      g_print ("texture size: %dx%d\n", gdk_paintable_get_intrinsic_width (GDK_PAINTABLE (icon)), gdk_paintable_get_intrinsic_height (GDK_PAINTABLE (icon)));
      g_object_unref (icon);
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
