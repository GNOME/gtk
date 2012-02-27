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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_MENU_SHELL_PRIVATE_H__
#define __GTK_MENU_SHELL_PRIVATE_H__


#include <gtk/gtkmenushell.h>
#include <gtk/gtkmnemonichash.h>
#include <gtk/gtkkeyhash.h>


G_BEGIN_DECLS

struct _GtkMenuShellPrivate
{
  GList *children;
  GtkWidget *active_menu_item;
  GtkWidget *parent_menu_shell;

  guint button;
  guint32 activate_time;

  guint active               : 1;
  guint have_grab            : 1;
  guint have_xgrab           : 1;
  guint ignore_enter         : 1;
  guint keyboard_mode        : 1;

  guint take_focus           : 1;
  guint activated_submenu    : 1;
  guint in_unselectable_item : 1; /* This flag is a crutch to keep
                                   * mnemonics in the same menu if
                                   * the user moves the mouse over
                                   * an unselectable menuitem.
                                   */
  GtkMnemonicHash *mnemonic_hash;
  GtkKeyHash *key_hash;

  GdkDevice *grab_pointer;
};

void        _gtk_menu_shell_select_last      (GtkMenuShell *menu_shell,
                                              gboolean      search_sensitive);
gint        _gtk_menu_shell_get_popup_delay  (GtkMenuShell *menu_shell);
void        _gtk_menu_shell_set_grab_device  (GtkMenuShell *menu_shell,
                                              GdkDevice    *device);
GdkDevice *_gtk_menu_shell_get_grab_device   (GtkMenuShell *menu_shell);

void       _gtk_menu_shell_add_mnemonic      (GtkMenuShell *menu_shell,
                                              guint         keyval,
                                              GtkWidget    *target);
void       _gtk_menu_shell_remove_mnemonic   (GtkMenuShell *menu_shell,
                                              guint         keyval,
                                              GtkWidget    *target);

void       _gtk_menu_shell_update_mnemonics  (GtkMenuShell *menu_shell);
void       _gtk_menu_shell_set_keyboard_mode (GtkMenuShell *menu_shell,
                                              gboolean      keyboard_mode);
gboolean   _gtk_menu_shell_get_keyboard_mode (GtkMenuShell *menu_shell);


G_END_DECLS

#endif  /* __GTK_MENU_SHELL_PRIVATE_H__ */
