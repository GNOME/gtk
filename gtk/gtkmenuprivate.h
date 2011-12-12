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

G_BEGIN_DECLS

/* Directions for submenus */
typedef enum
{
  GTK_DIRECTION_LEFT,
  GTK_DIRECTION_RIGHT
} GtkSubmenuDirection;

/* Placement of submenus */
typedef enum
{
  GTK_TOP_BOTTOM,
  GTK_LEFT_RIGHT
} GtkSubmenuPlacement;


struct _GtkMenuPrivate
{
  GtkWidget *parent_menu_item;
  GtkWidget *old_active_menu_item;

  GtkAccelGroup *accel_group;
  gchar         *accel_path;

  GtkMenuPositionFunc position_func;
  gpointer            position_func_data;
  GDestroyNotify      position_func_data_destroy;
  gint                position_x;
  gint                position_y;

  guint toggle_size;
  guint accel_size;

  /* Do _not_ touch these widgets directly. We hide the reference
   * count from the toplevel to the menu, so it must be restored
   * before operating on these widgets
   */
  GtkWidget *toplevel;

  GtkWidget     *tearoff_window;
  GtkWidget     *tearoff_hbox;
  GtkWidget     *tearoff_scrollbar;
  GtkAdjustment *tearoff_adjustment;

  GdkWindow *view_window;
  GdkWindow *bin_window;

  gint scroll_offset;
  gint saved_scroll_offset;
  gint scroll_step;

  guint scroll_timeout;

  guint needs_destruction_ref : 1;
  guint torn_off              : 1;
  /* The tearoff is active when it is torn off and the not-torn-off
   * menu is not popped up.
   */
  guint tearoff_active        : 1;
  guint scroll_fast           : 1;

  guint upper_arrow_visible   : 1;
  guint lower_arrow_visible   : 1;
  guint upper_arrow_prelight  : 1;
  guint lower_arrow_prelight  : 1;

  guint have_position         : 1;
  guint have_layout           : 1;
  guint seen_item_enter       : 1;
  guint ignore_button_release : 1;
  guint no_toggle_size        : 1;
  guint drag_already_pressed  : 1;
  guint drag_scroll_started   : 1;

  /* info used for the table */
  guint *heights;
  gint heights_length;
  gint requested_height;

  gboolean initially_pushed_in;
  gint monitor_num;

  /* Cached layout information */
  gint n_rows;
  gint n_columns;

  gchar *title;

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

G_END_DECLS

#endif /* __GTK_MENU_PRIVATE_H__ */
