/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc. 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gtkstock.h"
#include "gtkintl.h"
#include <gdk/gdkkeysyms.h>

static GHashTable *stock_hash = NULL;
static void init_stock_hash (void);

static void
real_add (const GtkStockItem *items,
          guint               n_items,
          gboolean            copy)
{
  int i;

  init_stock_hash ();

  if (n_items == 0)
    return;

  i = 0;
  while (i < n_items)
    {
      gpointer old_key, old_value;
      const GtkStockItem *item = &items[i];
      if (copy)
        item = gtk_stock_item_copy (item);

      if (g_hash_table_lookup_extended (stock_hash, item->stock_id,
                                        &old_key, &old_value))
        {
          g_hash_table_remove (stock_hash, old_key);
          gtk_stock_item_free (old_value);
        }
      
      g_hash_table_insert (stock_hash,
                           (gchar*)item->stock_id, (GtkStockItem*)item);

      ++i;
    }
}

/**
 * gtk_stock_add:
 * @items: a #GtkStockItem or array of items
 * @n_items: number of #GtkStockItem in @items
 *
 * Registers each of the stock items in @items. If an item already
 * exists with the same stock ID as one of the @items, the old item
 * gets replaced. The stock items are copied, so GTK+ does not hold
 * any pointer into @items and @items can be freed. Use
 * gtk_stock_add_static() if @items is persistent and GTK+ need not
 * copy the array.
 * 
 **/
void
gtk_stock_add (const GtkStockItem *items,
               guint               n_items)
{
  g_return_if_fail (items != NULL);

  real_add (items, n_items, TRUE);
}

/**
 * gtk_stock_add_static:
 * @items: a #GtkStockItem or array of #GtkStockItem
 * @n_items: number of items
 *
 * Same as gtk_stock_add(), but doesn't copy @items, so
 * @items must persist until application exit.
 * 
 **/
void
gtk_stock_add_static (const GtkStockItem *items,
                      guint               n_items)
{
  g_return_if_fail (items != NULL);

  real_add (items, n_items, FALSE);
}

/**
 * gtk_stock_lookup:
 * @stock_id: a stock item name
 * @item: stock item to initialize with values
 * 
 * Fills @item with the registered values for @stock_id, returning %TRUE
 * if @stock_id was known.
 * 
 * 
 * Return value: %TRUE if @item was initialized
 **/
gboolean
gtk_stock_lookup (const gchar  *stock_id,
                  GtkStockItem *item)
{
  const GtkStockItem *found;

  g_return_val_if_fail (stock_id != NULL, FALSE);
  g_return_val_if_fail (item != NULL, FALSE);

  init_stock_hash ();

  found = g_hash_table_lookup (stock_hash, stock_id);

  if (found)
    {
      *item = *found;
      if (item->label)
        item->label = dgettext (item->translation_domain, item->label);
    }

  return found != NULL;
}

static void
listify_foreach (gpointer key, gpointer value, gpointer data)
{
  GSList **list = data;

  *list = g_slist_prepend (*list, value);
}

static GSList *
g_hash_table_get_values (GHashTable *table)
{
  GSList *list = NULL;

  g_hash_table_foreach (table, listify_foreach, &list);

  return list;
}

/**
 * gtk_stock_list_items:
 * 
 * Retrieves a list of all known #GtkStockItem. The items are not copied;
 * they should not be freed. However, the list itself must be freed
 * with g_slist_free().
 * 
 * Return value: a list of registered stock items
 **/
GSList *
gtk_stock_list_items (void)
{
  init_stock_hash ();

  return g_hash_table_get_values (stock_hash);
}

/**
 * gtk_stock_item_copy:
 * @item: a #GtkStockItem
 * 
 * Copies a stock item, mostly useful for language bindings and not in applications.
 * 
 * Return value: a new #GtkStockItem
 **/
GtkStockItem *
gtk_stock_item_copy (const GtkStockItem *item)
{
  GtkStockItem *copy;

  g_return_val_if_fail (item != NULL, NULL);

  copy = g_new (GtkStockItem, 1);

  *copy = *item;

  copy->stock_id = g_strdup (item->stock_id);
  copy->label = g_strdup (item->label);
  copy->translation_domain = g_strdup (item->translation_domain);

  return copy;
}

/**
 * gtk_stock_item_free:
 * @item: a #GtkStockItem
 *
 * Frees a stock item allocated on the heap, such as one returned by
 * gtk_stock_item_copy(). Also frees the fields inside the stock item,
 * if they are not %NULL.
 * 
 **/
void
gtk_stock_item_free (GtkStockItem *item)
{
  g_return_if_fail (item != NULL);

  g_free ((gchar*)item->stock_id);
  g_free ((gchar*)item->label);
  g_free ((gchar*)item->translation_domain);

  g_free (item);
}

static GtkStockItem builtin_items [] =
{
  /* KEEP IN SYNC with gtkiconfactory.c stock icons */ 
 
  { GTK_STOCK_DIALOG_INFO, N_("Information"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_DIALOG_WARNING, N_("Warning"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_DIALOG_ERROR, N_("Error"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_DIALOG_QUESTION, N_("Question"), 0, 0, GETTEXT_PACKAGE },

  { GTK_STOCK_BUTTON_APPLY, N_("_Apply"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_BUTTON_OK, N_("_OK"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_BUTTON_CANCEL, N_("_Cancel"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_BUTTON_CLOSE, N_("_Close"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_BUTTON_YES, N_("_Yes"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_BUTTON_NO, N_("_No"), 0, 0, GETTEXT_PACKAGE },

  { GTK_STOCK_CLOSE, N_("_Close"), GDK_CONTROL_MASK, 'w', GETTEXT_PACKAGE },
  { GTK_STOCK_QUIT, N_("_Quit"), GDK_CONTROL_MASK, 'q', GETTEXT_PACKAGE },
  { GTK_STOCK_HELP, N_("_Help"), GDK_CONTROL_MASK, 'h', GETTEXT_PACKAGE },
  { GTK_STOCK_NEW, N_("_New"), GDK_CONTROL_MASK, 'n', GETTEXT_PACKAGE },
  { GTK_STOCK_OPEN, N_("_Open"), GDK_CONTROL_MASK, 'o', GETTEXT_PACKAGE },
  { GTK_STOCK_SAVE, N_("_Save"), GDK_CONTROL_MASK, 's', GETTEXT_PACKAGE }
};

static void
init_stock_hash (void)
{
  if (stock_hash == NULL)
    {
      stock_hash = g_hash_table_new (g_str_hash, g_str_equal);

      gtk_stock_add_static (builtin_items, G_N_ELEMENTS (builtin_items));
    }
}
