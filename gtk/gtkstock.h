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

#ifndef __GTK_STOCK_H__
#define __GTK_STOCK_H__


#include <gdk/gdk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _GtkStockItem GtkStockItem;

struct _GtkStockItem
{
  gchar *stock_id;
  gchar *label;
  GdkModifierType modifier;
  guint keyval;
  gchar *translation_domain;
};

void     gtk_stock_add        (const GtkStockItem  *item,
                               guint                n_items);
void     gtk_stock_add_static (const GtkStockItem  *item,
                               guint                n_items);
gboolean gtk_stock_lookup     (const gchar         *stock_id,
                               GtkStockItem        *item);

/* Should free the list, but DO NOT modify the items in the list.
 * This function is only useful for GUI builders and such.
 */
GSList*  gtk_stock_list_items  (void);

GtkStockItem *gtk_stock_item_copy (const GtkStockItem *item);
void          gtk_stock_item_free (GtkStockItem       *item);


/* Stock IDs */
#define GTK_STOCK_DIALOG_INFO      "gtk-dialog-info"
#define GTK_STOCK_DIALOG_WARNING   "gtk-dialog-warning"
#define GTK_STOCK_DIALOG_ERROR     "gtk-dialog-error"
#define GTK_STOCK_DIALOG_QUESTION  "gtk-dialog-question"

#define GTK_STOCK_BUTTON_APPLY     "gtk-button-apply"
#define GTK_STOCK_BUTTON_OK        "gtk-button-ok"
#define GTK_STOCK_BUTTON_CANCEL    "gtk-button-cancel"
#define GTK_STOCK_BUTTON_CLOSE     "gtk-button-close"
#define GTK_STOCK_BUTTON_YES       "gtk-button-yes"
#define GTK_STOCK_BUTTON_NO        "gtk-button-no"

#define GTK_STOCK_CLOSE            "gtk-close"
#define GTK_STOCK_QUIT             "gtk-quit"
#define GTK_STOCK_HELP             "gtk-help"
#define GTK_STOCK_NEW              "gtk-new"
#define GTK_STOCK_OPEN             "gtk-open"
#define GTK_STOCK_SAVE             "gtk-save"

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_STOCK_H__ */
