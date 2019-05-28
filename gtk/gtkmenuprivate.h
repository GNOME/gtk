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
#include <gtk/gtkgesturemultipress.h>

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

  gint                position_x;
  gint                position_y;

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

  GtkWidget *top_arrow_widget;
  GtkWidget *bottom_arrow_widget;

  gint scroll_offset;
  gint saved_scroll_offset;
  gint scroll_step;

  guint scroll_timeout;

  guint needs_destruction_ref : 1;
  guint scroll_fast           : 1;

  guint upper_arrow_prelight  : 1;
  guint lower_arrow_prelight  : 1;

  guint have_layout           : 1;
  guint ignore_button_release : 1;
  guint no_toggle_size        : 1;
  guint drag_already_pressed  : 1;
  guint drag_scroll_started   : 1;

  /* info used for the table */
  guint *heights;
  gint heights_length;
  gint requested_height;

  gint monitor_num;

  /* Cached layout information */
  gint n_rows;
  gint n_columns;

 /* Arrow states */
  GtkStateFlags lower_arrow_state;
  GtkStateFlags upper_arrow_state;

  /* navigation region */
  gint navigation_x;
  gint navigation_y;
  gint navigation_width;
  gint navigation_height;

  guint navigation_timeout;

  gdouble drag_start_y;
  gint initial_drag_offset;
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
