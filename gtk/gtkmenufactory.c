/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
#include <string.h>
#include "gtkcheckmenuitem.h"
#include "gtkmenu.h"
#include "gtkmenubar.h"
#include "gtkmenufactory.h"
#include "gtkmenuitem.h"
#include "gtksignal.h"


enum
{
  CREATE  = 1 << 0,
  DESTROY = 1 << 1,
  CHECK   = 1 << 2
};


static void         gtk_menu_factory_create            (GtkMenuFactory *factory,
							GtkMenuEntry   *entry,
							GtkWidget      *parent,
							const char     *path);
static void         gtk_menu_factory_remove            (GtkMenuFactory *factory,
							GtkWidget      *parent,
							const char     *path);
static GtkWidget*   gtk_menu_factory_make_widget       (GtkMenuFactory *factory);
static GtkMenuPath* gtk_menu_factory_get               (GtkWidget      *parent,
							const char     *path,
							int             flags);
static GtkMenuPath* gtk_menu_factory_find_recurse      (GtkMenuFactory *factory,
							GtkWidget      *parent,
							const char     *path);
static void         gtk_menu_factory_parse_accelerator (const char     *accelerator,
							char     *accelerator_key,
							guint8         *accelerator_mods);


GtkMenuFactory*
gtk_menu_factory_new (GtkMenuFactoryType type)
{
  GtkMenuFactory *factory;

  factory = g_new (GtkMenuFactory, 1);
  factory->path = NULL;
  factory->type = type;
  factory->table = NULL;
  factory->widget = NULL;
  factory->subfactories = NULL;

  return factory;
}

void
gtk_menu_factory_destroy (GtkMenuFactory *factory)
{
  GtkMenuFactory *subfactory;
  GList *tmp_list;

  g_return_if_fail (factory != NULL);

  if (factory->path)
    g_free (factory->path);

  tmp_list = factory->subfactories;
  while (tmp_list)
    {
      subfactory = tmp_list->data;
      tmp_list = tmp_list->next;

      gtk_menu_factory_destroy (subfactory);
    }

  if (factory->table)
    {
      gtk_accelerator_table_unref (factory->table);
      factory->table = NULL;
    }
  
  if (factory->widget)
    gtk_widget_unref (factory->widget);
}

void
gtk_menu_factory_add_entries (GtkMenuFactory *factory,
			      GtkMenuEntry   *entries,
			      int             nentries)
{
  int i;

  g_return_if_fail (factory != NULL);
  g_return_if_fail (entries != NULL);
  g_return_if_fail (nentries > 0);

  if (!factory->widget)
    {
      factory->widget = gtk_menu_factory_make_widget (factory);
      gtk_widget_ref  (factory->widget);
      gtk_object_sink (GTK_OBJECT (factory->widget));
    }

  for (i = 0; i < nentries; i++)
    gtk_menu_factory_create (factory, &entries[i], factory->widget, entries[i].path);
}

void
gtk_menu_factory_add_subfactory (GtkMenuFactory *factory,
				 GtkMenuFactory *subfactory,
				 const char     *path)
{
  g_return_if_fail (factory != NULL);
  g_return_if_fail (subfactory != NULL);
  g_return_if_fail (path != NULL);

  if (subfactory->path)
    g_free (subfactory->path);
  subfactory->path = g_strdup (path);

  factory->subfactories = g_list_append (factory->subfactories, subfactory);
}

void
gtk_menu_factory_remove_paths (GtkMenuFactory  *factory,
			       char           **paths,
			       int              npaths)
{
  int i;

  g_return_if_fail (factory != NULL);
  g_return_if_fail (paths != NULL);
  g_return_if_fail (npaths > 0);

  if (factory->widget)
    {
      for (i = 0; i < npaths; i++)
	gtk_menu_factory_remove (factory, factory->widget, paths[i]);
    }
}

void
gtk_menu_factory_remove_entries (GtkMenuFactory *factory,
				 GtkMenuEntry   *entries,
				 int             nentries)
{
  int i;

  g_return_if_fail (factory != NULL);
  g_return_if_fail (entries != NULL);
  g_return_if_fail (nentries > 0);

  if (factory->widget)
    {
      for (i = 0; i < nentries; i++)
	gtk_menu_factory_remove (factory, factory->widget, entries[i].path);
    }
}

void
gtk_menu_factory_remove_subfactory (GtkMenuFactory *factory,
				    GtkMenuFactory *subfactory,
				    const char     *path)
{
  g_return_if_fail (factory != NULL);
  g_return_if_fail (subfactory != NULL);
  g_return_if_fail (path != NULL);

  g_warning ("FIXME: gtk_menu_factory_remove_subfactory");
}

GtkMenuPath*
gtk_menu_factory_find (GtkMenuFactory *factory,
		       const char     *path)
{
  g_return_val_if_fail (factory != NULL, NULL);
  g_return_val_if_fail (path != NULL, NULL);

  return gtk_menu_factory_find_recurse (factory, factory->widget, path);
}


static void
gtk_menu_factory_create (GtkMenuFactory *factory,
			 GtkMenuEntry   *entry,
			 GtkWidget      *parent,
			 const char     *path)
{
  GtkMenuFactory *subfactory;
  GtkMenuPath *menu_path;
  GtkWidget *menu;
  GList *tmp_list;
  char tmp_path[256];
  char accelerator_key;
  guint8 accelerator_mods;
  char *p;

  g_return_if_fail (factory != NULL);
  g_return_if_fail (entry != NULL);

  /* If 'path' is empty, then simply return.
   */
  if (!path || path[0] == '\0')
    return;

  /* Strip off the next part of the path.
   */
  p = strchr (path, '/');

  /* If this is the last part of the path ('p' is
   *  NULL), then we create an item.
   */
  if (!p)
    {
      /* Check to see if this item is a separator.
       */
      if (strcmp (path, "<separator>") == 0)
	{
	  entry->widget = gtk_menu_item_new ();
	  gtk_container_add (GTK_CONTAINER (parent), entry->widget);
	  gtk_widget_show (entry->widget);
	}
      else
	{
	  if (strncmp (path, "<check>", 7) == 0)
	    menu_path = gtk_menu_factory_get (parent, path + 7, CREATE | CHECK);
	  else
	    menu_path = gtk_menu_factory_get (parent, path, CREATE);
	  entry->widget = menu_path->widget;

	  if (strcmp (path, "<nothing>") == 0)
	    gtk_widget_hide (entry->widget);

	  if (entry->accelerator)
	    {
	      gtk_menu_factory_parse_accelerator (entry->accelerator,
						  &accelerator_key,
						  &accelerator_mods);
	      if (!factory->table)
		factory->table = gtk_accelerator_table_new ();
	      
	      gtk_widget_install_accelerator (menu_path->widget,
					      factory->table,
					      "activate",
					      accelerator_key,
					      accelerator_mods);
	    }

	  if (entry->callback)
	    gtk_signal_connect (GTK_OBJECT (menu_path->widget), "activate",
				(GtkSignalFunc) entry->callback,
				entry->callback_data);
	}
    }
  else
    {
      strncpy (tmp_path, path, (unsigned int) ((long) p - (long) path));
      tmp_path[(long) p - (long) path] = '\0';

      menu_path = gtk_menu_factory_get (parent, tmp_path, 0);
      if (!menu_path)
	{
	  tmp_list = factory->subfactories;
	  while (tmp_list)
	    {
	      subfactory = tmp_list->data;
	      tmp_list = tmp_list->next;

	      if (subfactory->path &&
		  (strcmp (subfactory->path, tmp_path) == 0))
		{
		  if (!subfactory->widget) 
		    {
		      subfactory->widget = gtk_menu_factory_make_widget (subfactory);
		      gtk_widget_ref  (subfactory->widget);
		      gtk_object_sink (GTK_OBJECT (subfactory->widget));
		    }

		  gtk_menu_factory_create (subfactory, entry, subfactory->widget, p + 1);
		  return;
		}
	    }

	  menu_path = gtk_menu_factory_get (parent, tmp_path, CREATE);
	}

      entry->widget = menu_path->widget;
      menu = GTK_MENU_ITEM (menu_path->widget)->submenu;

      if (!menu)
	{
	  menu = gtk_menu_new ();
	  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_path->widget), menu);

	  if (!factory->table)
	    factory->table = gtk_accelerator_table_new ();
	  gtk_menu_set_accelerator_table (GTK_MENU (menu), factory->table);
	}

      gtk_menu_factory_create (factory, entry, menu, p + 1);
    }
}

static void
gtk_menu_factory_remove (GtkMenuFactory *factory,
			 GtkWidget      *parent,
			 const char     *path)
{
  GtkMenuFactory *subfactory;
  GtkMenuPath *menu_path;
  GtkWidget *menu;
  GList *tmp_list;
  char tmp_path[256];
  char *p;

  if (!path || path[0] == '\0')
    return;

  p = strchr (path, '/');

  if (!p)
    {
      if (parent)
	gtk_menu_factory_get (parent, path, DESTROY);
    }
  else
    {
      strncpy (tmp_path, path, (unsigned int) ((long) p - (long) path));
      tmp_path[(long) p - (long) path] = '\0';

      menu_path = gtk_menu_factory_get (parent, tmp_path, 0);
      if (!menu_path)
	{
	  tmp_list = factory->subfactories;
	  while (tmp_list)
	    {
	      subfactory = tmp_list->data;
	      tmp_list = tmp_list->next;

	      if (subfactory->path &&
		  (strcmp (subfactory->path, tmp_path) == 0))
		{
		  if (!subfactory->widget)
		    return;
		  gtk_menu_factory_remove (subfactory, subfactory->widget, p + 1);
		}
	    }
	}
      else
	{
	  menu = GTK_MENU_ITEM (menu_path->widget)->submenu;
	  if (menu)
	    gtk_menu_factory_remove (factory, menu, p + 1);
	}
    }
}

static GtkWidget*
gtk_menu_factory_make_widget (GtkMenuFactory *factory)
{
  GtkWidget *widget;

  g_return_val_if_fail (factory != NULL, NULL);

  switch (factory->type)
    {
    case GTK_MENU_FACTORY_MENU:
      widget = gtk_menu_new ();

      if (!factory->table)
	factory->table = gtk_accelerator_table_new ();
      gtk_menu_set_accelerator_table (GTK_MENU (widget), factory->table);
      return widget;
    case GTK_MENU_FACTORY_MENU_BAR:
      return gtk_menu_bar_new ();
    case GTK_MENU_FACTORY_OPTION_MENU:
      g_error ("not implemented");
      break;
    }

  return NULL;
}

static GtkMenuPath*
gtk_menu_factory_get (GtkWidget *parent,
		      const char *path,
		      int        flags)
{
  GtkMenuPath *menu_path;
  GList *tmp_list;

  tmp_list = gtk_object_get_user_data (GTK_OBJECT (parent));
  while (tmp_list)
    {
      menu_path = tmp_list->data;
      tmp_list = tmp_list->next;

      if (strcmp (menu_path->path, path) == 0)
	{
	  if (flags & DESTROY)
	    {
	      tmp_list = gtk_object_get_user_data (GTK_OBJECT (parent));
	      tmp_list = g_list_remove (tmp_list, menu_path);
	      gtk_object_set_user_data (GTK_OBJECT (parent), tmp_list);

	      gtk_widget_destroy (menu_path->widget);
	      g_free (menu_path->path);
	      g_free (menu_path);

	      return NULL;
	    }
	  else
	    {
	      return menu_path;
	    }
	}
    }

  if (flags & CREATE)
    {
      menu_path = g_new (GtkMenuPath, 1);
      menu_path->path = g_strdup (path);

      if (flags & CHECK)
	menu_path->widget = gtk_check_menu_item_new_with_label (path);
      else
	menu_path->widget = gtk_menu_item_new_with_label (path);

      gtk_container_add (GTK_CONTAINER (parent), menu_path->widget);
      gtk_object_set_user_data (GTK_OBJECT (menu_path->widget), NULL);
      gtk_widget_show (menu_path->widget);

      tmp_list = gtk_object_get_user_data (GTK_OBJECT (parent));
      tmp_list = g_list_prepend (tmp_list, menu_path);
      gtk_object_set_user_data (GTK_OBJECT (parent), tmp_list);

      return menu_path;
    }

  return NULL;
}

static GtkMenuPath*
gtk_menu_factory_find_recurse (GtkMenuFactory *factory,
			       GtkWidget      *parent,
			       const char     *path)
{
  GtkMenuFactory *subfactory;
  GtkMenuPath *menu_path;
  GtkWidget *menu;
  GList *tmp_list;
  char tmp_path[256];
  char *p;

  if (!path || path[0] == '\0')
    return NULL;

  p = strchr (path, '/');

  if (!p)
    {
      if (parent)
	return gtk_menu_factory_get (parent, path, 0);
    }
  else
    {
      strncpy (tmp_path, path, (unsigned int) ((long) p - (long) path));
      tmp_path[(long) p - (long) path] = '\0';

      menu_path = gtk_menu_factory_get (parent, tmp_path, 0);
      if (!menu_path)
	{
	  tmp_list = factory->subfactories;
	  while (tmp_list)
	    {
	      subfactory = tmp_list->data;
	      tmp_list = tmp_list->next;

	      if (subfactory->path &&
		  (strcmp (subfactory->path, tmp_path) == 0))
		{
		  if (!subfactory->widget)
		    return NULL;
		  return gtk_menu_factory_find_recurse (subfactory, subfactory->widget, p + 1);
		}
	    }

	  return NULL;
	}

      menu = GTK_MENU_ITEM (menu_path->widget)->submenu;
      if (menu)
	return gtk_menu_factory_find_recurse (factory, menu, p + 1);
    }

  return NULL;
}

static void
gtk_menu_factory_parse_accelerator (const char   *accelerator,
                                    char   *accelerator_key,
				    guint8 *accelerator_mods)
{
  int done;

  g_return_if_fail (accelerator != NULL);
  g_return_if_fail (accelerator_key != NULL);
  g_return_if_fail (accelerator_mods != NULL);

  *accelerator_key = 0;
  *accelerator_mods = 0;

  done = FALSE;
  while (!done)
    {
      if (strncmp (accelerator, "<shift>", 7) == 0)
        {
          accelerator += 7;
          *accelerator_mods |= GDK_SHIFT_MASK;
        }
      else if (strncmp (accelerator, "<alt>", 5) == 0)
        {
          accelerator += 5;
          *accelerator_mods |= GDK_MOD1_MASK;
        }
      else if (strncmp (accelerator, "<control>", 9) == 0)
        {
          accelerator += 9;
          *accelerator_mods |= GDK_CONTROL_MASK;
        }
      else
        {
          done = TRUE;
          *accelerator_key = accelerator[0];
        }
    }
}
