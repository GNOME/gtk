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
#ifndef __GTK_RC_H__
#define __GTK_RC_H__


#include <gtk/gtkstyle.h>
#include <gtk/gtkwidget.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


void	  gtk_rc_init			(void);
void	  gtk_rc_parse			(const gchar *filename);
void	  gtk_rc_parse_string		(const gchar *rc_string);
GtkStyle* gtk_rc_get_style		(GtkWidget   *widget);
void	  gtk_rc_add_widget_name_style	(GtkStyle    *style,
					 const gchar *pattern);
void	  gtk_rc_add_widget_class_style (GtkStyle    *style,
					 const gchar *pattern);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_RC_H__ */
