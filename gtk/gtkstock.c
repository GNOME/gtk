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

#include <string.h>

#include "gtkstock.h"
#include "gtkiconfactory.h"
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

  *list = g_slist_prepend (*list, key);
}

static GSList *
g_hash_table_get_keys (GHashTable *table)
{
  GSList *list = NULL;

  g_hash_table_foreach (table, listify_foreach, &list);

  return list;
}

/**
 * gtk_stock_list_ids:
 * 
 * Retrieves a list of all known stock IDs added to a #GtkIconFactory
 * or registered with gtk_stock_add(). The list must be freed with g_slist_free(),
 * and each string in the list must be freed with g_free().
 * 
 * Return value: a list of known stock IDs
 **/
GSList*
gtk_stock_list_ids (void)
{
  GSList *ids;
  GSList *icon_ids;
  GSList *retval;
  GSList *tmp_list;
  const gchar *last_id;
  
  init_stock_hash ();

  ids = g_hash_table_get_keys (stock_hash);
  icon_ids = _gtk_icon_factory_list_ids ();
  ids = g_slist_concat (ids, icon_ids);

  ids = g_slist_sort (ids, (GCompareFunc)strcmp);

  last_id = NULL;
  retval = NULL;
  tmp_list = ids;
  while (tmp_list != NULL)
    {
      GSList *next;

      next = g_slist_next (tmp_list);

      if (last_id && strcmp (tmp_list->data, last_id) == 0)
        {
          /* duplicate, ignore */
        }
      else
        {
          retval = g_slist_prepend (retval, g_strdup (tmp_list->data));
          last_id = tmp_list->data;
        }

      g_slist_free_1 (tmp_list);
      
      tmp_list = next;
    }

  return retval;
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
  /* KEEP IN SYNC with gtkiconfactory.c stock icons, when appropriate */ 
 
  { GTK_STOCK_DIALOG_INFO, N_("Information"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_DIALOG_WARNING, N_("Warning"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_DIALOG_ERROR, N_("Error"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_DIALOG_QUESTION, N_("Question"), 0, 0, GETTEXT_PACKAGE },

  /*  FIXME these need accelerators when appropriate, and
   * need the mnemonics to be rationalized
   */
  { GTK_STOCK_ADD, N_("_Add"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_APPLY, N_("_Apply"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_BOLD, N_("_Bold"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_CANCEL, N_("_Cancel"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_CDROM, N_("_CD-Rom"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_CLEAR, N_("_Clear"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_CLOSE, N_("_Close"), GDK_CONTROL_MASK, 'w', GETTEXT_PACKAGE },
  { GTK_STOCK_CONVERT, N_("_Convert"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_COPY, N_("_Copy"), GDK_CONTROL_MASK, 'c', GETTEXT_PACKAGE },
  { GTK_STOCK_CUT, N_("C_ut"), GDK_CONTROL_MASK, 'x', GETTEXT_PACKAGE },
  { GTK_STOCK_DELETE, N_("_Delete"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_EXECUTE, N_("_Execute"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_FIND, N_("_Find"), GDK_CONTROL_MASK, 'f', GETTEXT_PACKAGE },
  { GTK_STOCK_FIND_AND_REPLACE, N_("Find and _Replace"), GDK_CONTROL_MASK, 'r', GETTEXT_PACKAGE },
  { GTK_STOCK_FLOPPY, N_("_Floppy"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_GOTO_BOTTOM, N_("_Bottom"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_GOTO_FIRST, N_("_First"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_GOTO_LAST, N_("_Last"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_GOTO_TOP, N_("_Top"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_GO_BACK, N_("_Back"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_GO_DOWN, N_("_Down"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_GO_FORWARD, N_("_Forward"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_GO_Up, N_("_Up"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_HELP, N_("_Help"), GDK_CONTROL_MASK, 'h', GETTEXT_PACKAGE },
  { GTK_STOCK_HOME, N_("_Home"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_INDEX, N_("_Index"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_ITALIC, N_("_Italic"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_JUMP_TO, N_("_Jump to"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_JUSTIFY_CENTER, N_("_Center"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_JUSTIFY_FILL, N_("_Fill"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_JUSTIFY_LEFT, N_("_Left"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_JUSTIFY_RIGHT, N_("_Right"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_NEW, N_("_New"), GDK_CONTROL_MASK, 'n', GETTEXT_PACKAGE },
  { GTK_STOCK_NO, N_("_No"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_OK, N_("_OK"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_OPEN, N_("_Open"), GDK_CONTROL_MASK, 'o', GETTEXT_PACKAGE },
  { GTK_STOCK_PASTE, N_("_Paste"), GDK_CONTROL_MASK, 'v', GETTEXT_PACKAGE },
  { GTK_STOCK_PREFERENCES, N_("_Preferences"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_PRINT, N_("_Print"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_PRINT_PREVIEW, N_("Print Pre_view"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_PROPERTIES, N_("_Properties"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_QUIT, N_("_Quit"), GDK_CONTROL_MASK, 'q', GETTEXT_PACKAGE },
  { GTK_STOCK_REDO, N_("_Redo"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_REFRESH, N_("_Refresh"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_REMOVE, N_("_Remove"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_REVERT_TO_SAVED, N_("_Revert"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_SAVE, N_("_Save"), GDK_CONTROL_MASK, 's', GETTEXT_PACKAGE },
  { GTK_STOCK_SAVE_AS, N_("Save _As"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_SELECT_COLOR, N_("_Color"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_SELECT_FONT, N_("_Font"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_SORT_ASCENDING, N_("_Ascending"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_SORT_DESCENDING, N_("_Descending"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_SPELL_CHECK, N_("_Spell Check"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_STOP, N_("_Stop"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_STRIKETHROUGH, N_("_Strikethrough"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_UNDELETE, N_("_Undelete"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_UNDERLINE, N_("_Underline"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_UNDO, N_("_Undo"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_YES, N_("_Yes"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_ZOOM_100, N_("Zoom _100%"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_ZOOM_FIT, N_("Zoom to _Fit"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_ZOOM_IN, N_("Zoom _In"), 0, 0, GETTEXT_PACKAGE },
  { GTK_STOCK_ZOOM_OUT, N_("Zoom _Out"), 0, 0, GETTEXT_PACKAGE }
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
