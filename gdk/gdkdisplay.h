/*
 * gdkdisplay.h
 * 
 * Copyright 2001 Sun Microsystems Inc. 
 *
 * Erwann Chenede <erwann.chenede@sun.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDK_DISPLAY_H__
#define __GDK_DISPLAY_H__

#include <gdk/gdktypes.h>
#include <gobject/gobject.h>

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */

  typedef struct _GdkDisplayClass GdkDisplayClass;

#define GDK_TYPE_DISPLAY              (gdk_display_get_type ())
#define GDK_DISPLAY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DISPLAY, GdkDisplay))
#define GDK_DISPLAY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DISPLAY, GdkDisplayClass))
#define GDK_IS_DISPLAY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DISPLAY))
#define GDK_IS_DISPLAY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DISPLAY))
#define GDK_DISPLAY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DISPLAY, GdkDisplayClass))


struct _GdkDisplay
{
  GObject parent_instance;
};

struct _GdkDisplayClass
{
  GObjectClass parent_class;
  
  gchar *     (*get_display_name)   (GdkDisplay *display);
  gint        (*get_n_screens)      (GdkDisplay *display);
  GdkScreen * (*get_screen)         (GdkDisplay *display,
				     gint        screen_num);
  GdkScreen * (*get_default_screen) (GdkDisplay *display);
};

GType       gdk_display_get_type           (void);
GdkDisplay *gdk_display_init_new           (int          argc,
					    char       **argv,
					    char        *display_name);
GdkDisplay *gdk_display_new                (gchar       *display_name);
gchar *     gdk_display_get_name           (GdkDisplay  *display);
gint        gdk_display_get_n_screens      (GdkDisplay  *display);
GdkScreen * gdk_display_get_screen         (GdkDisplay  *display,
					    gint         screen_num);
GdkScreen * gdk_display_get_default_screen (GdkDisplay  *display);
void        gdk_display_set_use_xshm       (GdkDisplay  *display,
					    gboolean     use_xshm);
gboolean    gdk_display_get_use_xshm       (GdkDisplay  *display);
void        gdk_display_pointer_ungrab     (GdkDisplay  *display,
					    guint32      time);
void        gdk_display_keyboard_ungrab    (GdkDisplay  *display,
					    guint32      time);
gboolean    gdk_display_pointer_is_grabbed (GdkDisplay  *display);
void        gdk_display_beep               (GdkDisplay  *display);
void        gdk_display_sync               (GdkDisplay  *display);


#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* __GDK_DISPLAY_H__ */
