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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

static void
usage (void)
{
  g_print ("usage: test-icon-theme lookup <theme name> <icon name> [size]]\n"
	   " or\n"
	   "usage: test-icon-theme list <theme name> [context]\n"
	   " or\n"
	   "usage: test-icon-theme display <theme name> <icon name> [size]\n"
	   " or\n"
	   "usage: test-icon-theme contexts <theme name>\n"
	   );
}


int
main (int argc, char *argv[])
{
  GtkIconTheme *icon_theme;
  GtkIconInfo *icon_info;
  GdkRectangle embedded_rect;
  GdkPoint *attach_points;
  int n_attach_points;
  const gchar *display_name;
  char *context;
  char *themename;
  GList *list;
  int size = 48;
  int i;
  
  gtk_init (&argc, &argv);

  if (argc < 3)
    {
      usage ();
      return 1;
    }

  themename = argv[2];
  
  icon_theme = gtk_icon_theme_new ();
  
  gtk_icon_theme_set_custom_theme (icon_theme, themename);

  if (strcmp (argv[1], "display") == 0)
    {
      GError *error;
      GdkPixbuf *pixbuf;
      GtkWidget *window, *image;
      GtkIconSize size;

      if (argc < 4)
	{
	  g_object_unref (icon_theme);
	  usage ();
	  return 1;
	}
      
      if (argc >= 5)
	size = atoi (argv[4]);
      else 
	size = GTK_ICON_SIZE_BUTTON;

      error = NULL;
      pixbuf = gtk_icon_theme_load_icon (icon_theme, argv[3], size,
                                         GTK_ICON_LOOKUP_USE_BUILTIN, &error);
      if (!pixbuf)
        {
          g_print ("%s\n", error->message);
          return 1;
        }

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      image = gtk_image_new ();
      gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
      g_object_unref (pixbuf);
      gtk_container_add (GTK_CONTAINER (window), image);
      g_signal_connect (window, "delete-event",
                        G_CALLBACK (gtk_main_quit), window);
      gtk_widget_show_all (window);
      
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
  else if (strcmp (argv[1], "contexts") == 0)
    {
      list = gtk_icon_theme_list_contexts (icon_theme);
      
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
      
      icon_info = gtk_icon_theme_lookup_icon (icon_theme, argv[3], size, GTK_ICON_LOOKUP_USE_BUILTIN);
      g_print ("icon for %s at %dx%d is %s\n", argv[3], size, size,
	       icon_info ? (gtk_icon_info_get_builtin_pixbuf (icon_info) ? "<builtin>" : gtk_icon_info_get_filename (icon_info)) : "<none>");

      if (icon_info) 
	{
	  if (gtk_icon_info_get_embedded_rect (icon_info, &embedded_rect))
	    {
	      g_print ("Embedded rect: %d,%d %dx%d\n",
		       embedded_rect.x, embedded_rect.y,
		       embedded_rect.width, embedded_rect.height);
	    }
	  
	  if (gtk_icon_info_get_attach_points (icon_info, &attach_points, &n_attach_points))
	    {
	      g_print ("Attach Points: ");
	      for (i = 0; i < n_attach_points; i++)
		g_print ("%d, %d; ",
			 attach_points[i].x,
			 attach_points[i].y);
	      g_free (attach_points);
	      g_print ("\n");
	    }
	  
	  display_name = gtk_icon_info_get_display_name (icon_info);
	  
	  if (display_name)
	    g_print ("Display name: %s\n", display_name);
	  
	  gtk_icon_info_free (icon_info);
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
