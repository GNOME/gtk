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
#pragma }
#endif /* __cplusplus */


void	  gtk_rc_init			(void);
void	  gtk_rc_parse			(const gchar *filename);
void	  gtk_rc_parse_string		(const gchar *rc_string);
gboolean  gtk_rc_reparse_all		(void);
GtkStyle* gtk_rc_get_style		(GtkWidget   *widget);
void	  gtk_rc_add_widget_name_style	(GtkStyle    *style,
					 const gchar *pattern);
void	  gtk_rc_add_widget_class_style (GtkStyle    *style,
					 const gchar *pattern);
void	  gtk_rc_add_class_style	(GtkStyle    *style,
					 const gchar *pattern);



/* Tell gtkrc to use a custom routine to load images specified in rc files instead of
 *   the default xpm-only loader
 */
typedef	 GdkPixmap * (*GtkImageLoader) (GdkWindow   *window,
					GdkColormap *colormap,
					GdkBitmap  **mask,
					GdkColor    *transparent_color,
					const gchar *filename);
void	  gtk_rc_set_image_loader(GtkImageLoader loader);


/* private functions/definitions */
typedef enum {
  GTK_RC_TOKEN_INVALID = G_TOKEN_LAST,
  GTK_RC_TOKEN_INCLUDE,
  GTK_RC_TOKEN_NORMAL,
  GTK_RC_TOKEN_ACTIVE,
  GTK_RC_TOKEN_PRELIGHT,
  GTK_RC_TOKEN_SELECTED,
  GTK_RC_TOKEN_INSENSITIVE,
  GTK_RC_TOKEN_FG,
  GTK_RC_TOKEN_BG,
  GTK_RC_TOKEN_BASE,
  GTK_RC_TOKEN_TEXT,
  GTK_RC_TOKEN_FONT,
  GTK_RC_TOKEN_FONTSET,
  GTK_RC_TOKEN_BG_PIXMAP,
  GTK_RC_TOKEN_PIXMAP_PATH,
  GTK_RC_TOKEN_STYLE,
  GTK_RC_TOKEN_BINDING,
  GTK_RC_TOKEN_BIND,
  GTK_RC_TOKEN_WIDGET,
  GTK_RC_TOKEN_WIDGET_CLASS,
  GTK_RC_TOKEN_CLASS,
  GTK_RC_TOKEN_LOWEST,
  GTK_RC_TOKEN_GTK,
  GTK_RC_TOKEN_APPLICATION,
  GTK_RC_TOKEN_RC,
  GTK_RC_TOKEN_HIGHEST,
  GTK_RC_TOKEN_LAST
} GtkRcTokenType;

guint	gtk_rc_parse_color	(GScanner	     *scanner,
				 GdkColor	     *color);
guint	gtk_rc_parse_state	(GScanner	     *scanner,
				 GtkStateType	     *state);
guint	gtk_rc_parse_priority	(GScanner	     *scanner,
				 GtkPathPriorityType *priority);
     


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_RC_H__ */
