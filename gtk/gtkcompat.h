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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#ifndef __GTK_COMPAT_H__
#define __GTK_COMPAT_H__


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* this file contains aliases that have to be kept for historical
 * reasons, because a wide code base depends on them.
 */

#ifndef	GTK_DISABLE_COMPAT_H

#define	gtk_accel_label_accelerator_width	gtk_accel_label_get_accel_width
#define	gtk_container_border_width		gtk_container_set_border_width
#define	gtk_notebook_current_page               gtk_notebook_get_current_page
#define	gtk_packer_configure                    gtk_packer_set_child_packing
#define	gtk_paned_gutter_size			gtk_paned_set_gutter_size
#define	gtk_paned_handle_size			gtk_paned_set_handle_size
#define	gtk_scale_value_width                   gtk_scale_get_value_width
#define	gtk_window_position			gtk_window_set_position
#define	gtk_toggle_button_set_state		gtk_toggle_button_set_active
#define	gtk_check_menu_item_set_state		gtk_check_menu_item_set_active

/* strongly deprecated: */
#define	gtk_ctree_set_reorderable(t,r)		gtk_clist_set_reorderable((GtkCList*) (t),(r))
#define	gtk_style_apply_default_pixmap(s,gw,st,a,x,y,w,h) \
    gtk_style_apply_default_background (s,gw,TRUE,st,a,x,y,w,h)

#endif	/* GTK_DISABLE_COMPAT_H */

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_COMPAT_H__ */
