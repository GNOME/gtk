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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_STOCK_H__
#define __GTK_STOCK_H__


#include <gdk/gdk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GtkStockItem GtkStockItem;

struct _GtkStockItem
{
  const char *stock_id;
  const char *label;
  GdkModifierType modifier;
  guint keyval;
  const char *translation_domain;
};

void     gtk_stock_add        (const GtkStockItem *item,
                               guint               n_items);
void     gtk_stock_add_static (const GtkStockItem *item,
                               guint               n_items);
gboolean gtk_stock_lookup     (const gchar        *stock_id,
                               GtkStockItem       *item);



GtkStockItem *gtk_stock_item_copy (const GtkStockItem *item);
void          gtk_stock_item_free (GtkStockItem       *item);


/* Stock IDs */
#define GTK_STOCK_DIALOG_INFO      "Gtk_Info_Dialog"
#define GTK_STOCK_DIALOG_WARNING   "Gtk_Warning_Dialog"
#define GTK_STOCK_DIALOG_ERROR     "Gtk_Error_Dialog"
#define GTK_STOCK_DIALOG_QUESTION  "Gtk_Question_Dialog"

#define GTK_STOCK_BUTTON_APPLY     "Gtk_Apply_Button"
#define GTK_STOCK_BUTTON_OK        "Gtk_OK_Button"
#define GTK_STOCK_BUTTON_CANCEL    "Gtk_Cancel_Button"
#define GTK_STOCK_BUTTON_CLOSE     "Gtk_Close_Button"
#define GTK_STOCK_BUTTON_YES       "Gtk_Yes_Button"
#define GTK_STOCK_BUTTON_NO        "Gtk_No_Button"

#define GTK_STOCK_CLOSE            "Gtk_Close"
#define GTK_STOCK_QUIT             "Gtk_Quit"
#define GTK_STOCK_HELP             "Gtk_Help"
#define GTK_STOCK_NEW              "Gtk_New"
#define GTK_STOCK_OPEN             "Gtk_Open"
#define GTK_STOCK_SAVE             "Gtk_Save"

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_STOCK_H__ */
