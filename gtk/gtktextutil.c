/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2001.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gtktextutil.h"
#include "gtkintl.h"
#include "gtkmenuitem.h"

typedef struct _GtkUnicodeMenuEntry GtkUnicodeMenuEntry;
typedef struct _GtkTextUtilCallbackInfo GtkTextUtilCallbackInfo;

struct _GtkUnicodeMenuEntry {
  const char *label;
  gunichar ch;
};

struct _GtkTextUtilCallbackInfo
{
  GtkTextUtilCharChosenFunc func;
  gpointer data;
};

GtkUnicodeMenuEntry bidi_menu_entries[] = {
  { N_("LRM _Left-to-right mark"), 0x200E },
  { N_("RLM _Right-to-left mark"), 0x200F },
  { N_("LRE Left-to-right _embedding"), 0x202A },
  { N_("RLE Right-to-left e_mbedding"), 0x202B },
  { N_("LRO Left-to-right _override"), 0x202D },
  { N_("RLO Right-to-left o_verride"), 0x202E },
  { N_("PDF _Pop directional formatting"), 0x202C },
  { N_("ZWS _Zero width space"), 0x200B },
  { N_("ZWN Zero width _joiner"), 0x200D },
  { N_("ZWNJ Zero width _non-joiner"), 0x200C }
};

static void
activate_cb (GtkWidget *menu_item,
             gpointer   data)
{
  GtkUnicodeMenuEntry *entry;
  GtkTextUtilCallbackInfo *info = data;
  char buf[7];
  
  entry = g_object_get_data (G_OBJECT (menu_item), "gtk-unicode-menu-entry");

  buf[g_unichar_to_utf8 (entry->ch, buf)] = '\0';
  
  (* info->func) (buf, info->data);
}

/**
 * _gtk_text_util_append_special_char_menuitems
 * @menushell: a #GtkMenuShell
 * @callback:  call this when an item is chosen
 * @data: data for callback
 * 
 * Add menuitems for various bidi control characters  to a menu;
 * the menuitems, when selected, will call the given function
 * with the chosen character.
 *
 * This function is private/internal in GTK 2.0, the functionality may
 * become public sometime, but it probably needs more thought first.
 * e.g. maybe there should be a way to just get the list of items,
 * instead of requiring the menu items to be created.
 **/
void
_gtk_text_util_append_special_char_menuitems (GtkMenuShell              *menushell,
                                              GtkTextUtilCharChosenFunc  func,
                                              gpointer                   data)
{
  int i;
  
  for (i = 0; i < G_N_ELEMENTS (bidi_menu_entries); i++)
    {
      GtkWidget *menuitem;
      GtkTextUtilCallbackInfo *info;

      /* wasteful to have a bunch of copies, but simplifies mem management */
      info = g_new (GtkTextUtilCallbackInfo, 1);
      info->func = func;
      info->data = data;
      
      menuitem = gtk_menu_item_new_with_mnemonic (bidi_menu_entries[i].label);
      g_object_set_data (G_OBJECT (menuitem), "gtk-unicode-menu-entry",
                         &bidi_menu_entries[i]);
      
      g_signal_connect_data (G_OBJECT (menuitem), "activate",
                             G_CALLBACK (activate_cb),
                             info, (GClosureNotify) g_free, 0);
      
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (menushell, menuitem);
    }
}

