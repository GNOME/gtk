/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * Themes added by The Rasterman <raster@redhat.com>
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __GTK_THEMES_H__
#define __GTK_THEMES_H__


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>

struct _GtkThemesData
{
   gchar *theme_name;
   void  *theme_lib;
   void (*init)(int     *argc, char ***argv);
   void (*exit)(void);
   struct
     {
	GtkStyle *(*gtk_style_new) (void);
	void (*gtk_style_set_background) (GtkStyle     *style,
					  GdkWindow    *window,
					  GtkStateType  state_type);
     } functions;
   void *data;
};

typedef struct _GtkThemesData          GtkThemesData;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Initialization, exit, mainloop and miscellaneous routines
 */
void	   gtk_themes_init	 (int	       *argc,
				  char	     ***argv);
void	   gtk_themes_exit	 (gint		error_code);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_THEMES_H__ */
