/*
 * gdkdisplay-x11.h
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

#ifndef __GDK_DISPLAY_X11__
#define __GDK_DISPLAY_X11__

#include <X11/X.h>
#include <X11/Xlib.h>
#include <glib.h>
#include <gdk/gdkdisplay.h>
#include <gdk/gdkkeys.h>
#include "gdk/gdkwindow.h"

G_BEGIN_DECLS

typedef struct _GdkDisplayImplX11 GdkDisplayImplX11;
typedef struct _GdkDisplayImplX11Class GdkDisplayImplX11Class;

#define GDK_TYPE_DISPLAY_IMPL_X11              (gdk_x11_display_impl_get_type())
#define GDK_DISPLAY_IMPL_X11(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DISPLAY_IMPL_X11, GdkDisplayImplX11))
#define GDK_DISPLAY_IMPL_X11_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DISPLAY_IMPL_X11, GdkDisplayImplX11Class))
#define GDK_IS_DISPLAY_IMPL_X11(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DISPLAY_IMPL_X11))
#define GDK_IS_DISPLAY_IMPL_X11_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DISPLAY_IMPL_X11))
#define GDK_DISPLAY_IMPL_X11_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DISPLAY_IMPL_X11, GdkDisplayImplX11Class))

struct _GdkDisplayImplX11
{
  GdkDisplay parent_instance;
  Display *xdisplay;
  GdkScreen *default_screen;
  GSList *screen_list;

  gint grab_count;

  /* Keyboard related information */

  gint xkb_event_type;
  guint keymap_serial;
  gboolean use_xkb;
  /* Whether we were able to turn on detectable-autorepeat using
   * XkbSetDetectableAutorepeat. If FALSE, we'll fall back
   * to checking the next event with XPending(). */

  GdkKeymap *keymap;

  gboolean have_xkb_autorepeat;
  gboolean gdk_use_xshm;

  /* Window that currently holds the x pointer grab */

  GdkWindowObject *gdk_xgrab_window;

  /* drag and drop information */
  /* I assume each display needs only info on a default  display */

  GdkScreen *dnd_default_screen;
  GdkDragContext *current_dest_drag;

  /* data needed for MOTIF DnD */

  Window motif_drag_window;
  GdkWindow *motif_drag_gdk_window;
  GList **motif_target_lists;
  gint motif_n_target_lists;

  /* Mapping to/from virtual atoms */

  GHashTable *atom_from_virtual;
  GHashTable *atom_to_virtual;

  /* Session Management leader window see ICCCM */
  Window leader_window;
  
  /* list of filters for client messages */
  GList *client_filters;	            
  
  /* X ID hashtable */
  GHashTable *xid_ht;

  /* translation queue */
  GSList *translate_queue;
};

struct _GdkDisplayImplX11Class
{
  GdkDisplayClass parent_class;
};

GType       gdk_x11_display_impl_get_type     (void);
GdkDisplay *_gdk_x11_display_impl_display_new (gchar *display_name);
gboolean    gdk_x11_display_is_root_window    (GdkDisplay *display,
					       Window xroot_window);
void	    gdk_x11_display_grab	      (GdkDisplay *display);
void	    gdk_x11_display_ungrab	      (GdkDisplay *display);

#define DEFAULT_X_DISPLAY   GDK_DISPLAY_IMPL_X11(gdk_get_default_display())->xdisplay
#define GDK_DISPLAY_XDISPLAY(display)  (GDK_DISPLAY_IMPL_X11(display)->xdisplay)

G_END_DECLS

#endif				/* __GDK_DISPLAY_X11__ */
