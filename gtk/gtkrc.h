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

#ifndef __GTK_RC_H__
#define __GTK_RC_H__


#include <gtk/gtkstyle.h>
#include <gtk/gtkwidget.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_RC_STYLE              (gtk_rc_style_get_type ())
#define GTK_RC_STYLE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GTK_TYPE_RC_STYLE, GtkRcStyle))
#define GTK_RC_STYLE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_RC_STYLE, GtkRcStyleClass))
#define GTK_IS_RC_STYLE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GTK_TYPE_RC_STYLE))
#define GTK_IS_RC_STYLE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_RC_STYLE))
#define GTK_RC_STYLE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_RC_STYLE, GtkRcStyleClass))

typedef enum
{
  GTK_RC_FG		= 1 << 0,
  GTK_RC_BG		= 1 << 1,
  GTK_RC_TEXT		= 1 << 2,
  GTK_RC_BASE		= 1 << 3
} GtkRcFlags;

typedef struct _GtkRcStyleClass GtkRcStyleClass;

struct _GtkRcStyle
{
  GObject parent_instance;

  /*< public >*/
  
  gchar *name;
  gchar *bg_pixmap_name[5];
  PangoFontDescription *font_desc;

  GtkRcFlags color_flags[5];
  GdkColor   fg[5];
  GdkColor   bg[5];
  GdkColor   text[5];
  GdkColor   base[5];

  gint xthickness;
  gint ythickness;

  /*< private >*/
  
  /* list of RC style lists including this RC style */
  GSList *rc_style_lists;
};

struct _GtkRcStyleClass
{
  GObjectClass parent_class;

  /* Create an empty RC style of the same type as this RC style.
   * The default implementation, which does
   * g_object_new (G_OBJECT_TYPE (style), NULL);
   * should work in most cases.
   */
  GtkRcStyle *(*clone) (GtkRcStyle *rc_style);

  /* Fill in engine specific parts of GtkRcStyle by parsing contents
   * of brackets. Returns G_TOKEN_NONE if succesful, otherwise returns
   * the token it expected but didn't get.
   */
  guint     (*parse)  (GtkRcStyle *rc_style, GScanner *scanner);
  
  /* Combine RC style data from src into dest. If overriden, this
   * function should chain to the parent.
   */
  void      (*merge)  (GtkRcStyle *dest, GtkRcStyle *src);

  /* Create an empty style suitable to this RC style
   */
  GtkStyle *(*create_style) (GtkRcStyle *rc_style);
};

void	  gtk_rc_init			(void);
void      gtk_rc_add_default_file    (const gchar *filename);
void      gtk_rc_set_default_files      (gchar **filenames);
gchar**   gtk_rc_get_default_files      (void);
void	  gtk_rc_parse			(const gchar *filename);
void	  gtk_rc_parse_string		(const gchar *rc_string);
gboolean  gtk_rc_reparse_all		(void);
GtkStyle* gtk_rc_get_style		(GtkWidget   *widget);
void	  gtk_rc_add_widget_name_style	(GtkRcStyle  *rc_style,
					 const gchar *pattern);
void	  gtk_rc_add_widget_class_style (GtkRcStyle  *rc_style,
					 const gchar *pattern);
void	  gtk_rc_add_class_style	(GtkRcStyle  *rc_style,
					 const gchar *pattern);

GType       gtk_rc_style_get_type   (void);
GtkRcStyle* gtk_rc_style_new        (void);
void        gtk_rc_style_ref        (GtkRcStyle *rc_style);
void        gtk_rc_style_unref      (GtkRcStyle *rc_style);

/* Tell gtkrc to use a custom routine to load images specified in rc files instead of
 *   the default xpm-only loader
 */
typedef	GdkPixmap*  (*GtkImageLoader) 		(GdkWindow   	*window,
						 GdkColormap 	*colormap,
						 GdkBitmap     **mask,
						 GdkColor    	*transparent_color,
						 const gchar 	*filename);
void		gtk_rc_set_image_loader      	(GtkImageLoader	 loader);

GdkPixmap*	gtk_rc_load_image		(GdkColormap 	*colormap,
						 GdkColor    	*transparent_color,
						 const gchar 	*filename);
gchar*		gtk_rc_find_pixmap_in_path	(GScanner    	*scanner,
						 const gchar	*pixmap_file);
gchar*		gtk_rc_find_module_in_path	(const gchar 	*module_file);
gchar*		gtk_rc_get_theme_dir		(void);
gchar*		gtk_rc_get_module_dir		(void);

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
  GTK_RC_TOKEN_TEXT,
  GTK_RC_TOKEN_BASE,
  GTK_RC_TOKEN_XTHICKNESS,
  GTK_RC_TOKEN_YTHICKNESS,
  GTK_RC_TOKEN_FONT,
  GTK_RC_TOKEN_FONTSET,
  GTK_RC_TOKEN_FONT_NAME,
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
  GTK_RC_TOKEN_ENGINE,
  GTK_RC_TOKEN_MODULE_PATH,
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
