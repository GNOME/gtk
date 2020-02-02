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

int
main (int argc, char *argv[])
{
  GtkIconTheme *icon_theme;
  GtkIcon *icon;
  char *themename;
  GList *list;
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

      icon = gtk_icon_theme_lookup_icon (icon_theme, argv[3], size, scale, direction, flags);
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
  else if (strcmp (argv[1], "list") == 0)
    {
      list = gtk_icon_theme_list_icons (icon_theme);
      
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

      icon = gtk_icon_theme_lookup_icon (icon_theme, argv[3], size, scale, direction, flags);
      g_print ("icon for %s at %dx%d@%dx is %s\n", argv[3], size, size, scale,
               icon ? gtk_icon_get_filename (icon) : "<none>");

      if (icon)
	{
          GdkPaintable *paintable = GDK_PAINTABLE (icon);

          g_print ("texture size: %dx%d\n", gdk_paintable_get_intrinsic_width (paintable), gdk_paintable_get_intrinsic_height (paintable));

	  g_object_unref (icon);
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
