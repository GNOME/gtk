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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Global clipboard abstraction. 
 */

#ifndef __GTK_CLIPBOARD_H__
#define __GTK_CLIPBOARD_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <gtk/gtkselection.h>

typedef struct _GtkClipboard GtkClipboard;

typedef void (* GtkClipboardReceivedFunc)        (GtkClipboard     *clipboard,
					          GtkSelectionData *selection_data,
					          gpointer          data);
typedef void (* GtkClipboardTextReceivedFunc)    (GtkClipboard     *clipboard,
					          const gchar      *text,
					          gpointer          data);

/* Should these functions have GtkClipboard *clipboard as the first argument?
 * right now for ClearFunc, you may have trouble determining _which_ clipboard
 * was cleared, if you reuse your ClearFunc for multiple clipboards.
 */
typedef void (* GtkClipboardGetFunc)          (GtkClipboard     *clipboard,
					       GtkSelectionData *selection_data,
					       guint             info,
					       gpointer          user_data_or_owner);
typedef void (* GtkClipboardClearFunc)        (GtkClipboard     *clipboard,
					       gpointer          user_data_or_owner);
  
GtkClipboard *gtk_clipboard_get (GdkAtom selection);

gboolean gtk_clipboard_set_with_data  (GtkClipboard          *clipboard,
				       const GtkTargetEntry  *targets,
				       guint                  n_targets,
				       GtkClipboardGetFunc    get_func,
				       GtkClipboardClearFunc  clear_func,
				       gpointer               user_data);
gboolean gtk_clipboard_set_with_owner (GtkClipboard          *clipboard,
				       const GtkTargetEntry  *targets,
				       guint                  n_targets,
				       GtkClipboardGetFunc    get_func,
				       GtkClipboardClearFunc  clear_func,
				       GObject               *owner);
GObject *gtk_clipboard_get_owner      (GtkClipboard          *clipboard);
void     gtk_clipboard_clear          (GtkClipboard          *clipboard);
void     gtk_clipboard_set_text       (GtkClipboard          *clipboard,
				       const gchar           *text,
				       gint                   len);

void gtk_clipboard_request_contents (GtkClipboard                    *clipboard,
				     GdkAtom                          target,
				     GtkClipboardReceivedFunc         callback,
				     gpointer                         user_data);
void gtk_clipboard_request_text     (GtkClipboard                    *clipboard,
				     GtkClipboardTextReceivedFunc     callback,
				     gpointer                         user_data);

GtkSelectionData *gtk_clipboard_wait_for_contents (GtkClipboard *clipboard,
						   GdkAtom       target);
gchar *           gtk_clipboard_wait_for_text     (GtkClipboard *clipboard);

gboolean gtk_clipboard_wait_is_text_available   (GtkClipboard         *clipboard);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_CLIPBOARD_H__ */
