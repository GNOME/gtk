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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_THEMES_H__
#define __GTK_THEMES_H__

#include <gdk/gdk.h>
#include <gtk/gtkstyle.h>
#include <gtk/gtkwidget.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


struct _GtkThemeEngine {
  /* Fill in engine_data pointer in a GtkRcStyle by parsing contents
   * of brackets. Returns G_TOKEN_NONE if succesfull, otherwise returns
   * the token it expected but didn't get.
   */
  guint (*parse_rc_style)    (GScanner *scanner, GtkRcStyle *rc_style);
  
  /* Combine RC style data from src into dest. If 
   * dest->engine_data is NULL, it should be initialized to default
   * values.
   */
  void (*merge_rc_style)    (GtkRcStyle *dest,     GtkRcStyle *src);

  /* Fill in style->engine_data from rc_style->engine_data */
  void (*rc_style_to_style) (GtkStyle   *style, GtkRcStyle *rc_style);

  /* Duplicate engine_data from src to dest. The engine_data will
   * not subsequently be modified except by a call to realize_style()
   * so if realize_style() does nothing, refcounting is appropriate.
   */
  void (*duplicate_style)   (GtkStyle *dest,       GtkStyle *src);

  /* If style needs to initialize for a particular colormap/depth
   * combination, do it here. style->colormap/style->depth will have
   * been set at this point, and style itself initialized for 
   * the colormap
   */
  void (*realize_style) (GtkStyle   *new_style);

  /* If style needs to clean up for a particular colormap/depth
   * combination, do it here. 
   */
  void (*unrealize_style) (GtkStyle   *new_style);

  /* Clean up rc_style->engine_data before rc_style is destroyed */
  void (*destroy_rc_style)  (GtkRcStyle *rc_style);

  /* Clean up style->engine_data before style is destroyed */
  void (*destroy_style)     (GtkStyle   *style);

  void (*set_background) (GtkStyle     *style,
			  GdkWindow    *window,
			  GtkStateType  state_type);
};

GtkThemeEngine *gtk_theme_engine_get   (const gchar    *name);
void            gtk_theme_engine_ref   (GtkThemeEngine *engine);
void            gtk_theme_engine_unref (GtkThemeEngine *engine);



#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_THEMES_H__ */
