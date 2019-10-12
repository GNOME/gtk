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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_MENU_PRIVATE_H__
#define __GTK_MENU_PRIVATE_H__

#include <gtk/gtkmenu.h>
#include <gtk/gtkcssnodeprivate.h>
#include <gtk/gtkgestureclick.h>

G_BEGIN_DECLS

typedef struct _GtkMenuClass   GtkMenuClass;
typedef struct _GtkMenuPrivate GtkMenuPrivate;

struct _GtkMenu
{
  GtkMenuShell menu_shell;

  GtkMenuPrivate *priv;
};

struct _GtkMenuClass
{
  GtkMenuShellClass parent_class;
};

struct _GtkMenuPrivate
{
  GtkWidget *parent_menu_item;
  GtkWidget *old_active_menu_item;

  GtkAccelGroup *accel_group;
  const char    *accel_path;

  GdkSurface         *rect_surface;
  GdkRectangle       rect;
  GtkWidget         *widget;
  GdkGravity         rect_anchor;
  GdkGravity         menu_anchor;
  GdkAnchorHints     anchor_hints;
  gint               rect_anchor_dx;
  gint               rect_anchor_dy;
  GdkSurfaceTypeHint  menu_type_hint;

  guint toggle_size;
  guint accel_size;

  /* Do _not_ touch these widgets directly. We hide the reference
   * count from the toplevel to the menu, so it must be restored
   * before operating on these widgets
   */
  GtkWidget *toplevel;
  GtkWidget *swin;
  GtkWidget *box;

  guint needs_destruction_ref : 1;

  guint ignore_button_release : 1;
  guint no_toggle_size        : 1;

  gint monitor_num;
};

G_GNUC_INTERNAL
void gtk_menu_update_scroll_offset (GtkMenu            *menu,
                                    const GdkRectangle *flipped_rect,
                                    const GdkRectangle *final_rect,
                                    gboolean            flipped_x,
                                    gboolean            flipped_y,
                                    gpointer            user_data);

G_END_DECLS

#endif /* __GTK_MENU_PRIVATE_H__ */
