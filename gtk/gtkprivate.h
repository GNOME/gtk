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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_PRIVATE_H__
#define __GTK_PRIVATE_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

void         _gtk_widget_set_visible_flag   (GtkWidget *widget,
                                             gboolean   visible);
gboolean     _gtk_widget_get_resize_pending (GtkWidget *widget);
void         _gtk_widget_set_resize_pending (GtkWidget *widget,
                                             gboolean   resize_pending);
gboolean     _gtk_widget_get_in_reparent    (GtkWidget *widget);
void         _gtk_widget_set_in_reparent    (GtkWidget *widget,
                                             gboolean   in_reparent);
gboolean     _gtk_widget_get_anchored       (GtkWidget *widget);
void         _gtk_widget_set_anchored       (GtkWidget *widget,
                                             gboolean   anchored);
gboolean     _gtk_widget_get_shadowed       (GtkWidget *widget);
void         _gtk_widget_set_shadowed       (GtkWidget *widget,
                                             gboolean   shadowed);
gboolean     _gtk_widget_get_alloc_needed   (GtkWidget *widget);
void         _gtk_widget_set_alloc_needed   (GtkWidget *widget,
                                             gboolean   alloc_needed);
gboolean     _gtk_widget_get_width_request_needed  (GtkWidget *widget);
void         _gtk_widget_set_width_request_needed  (GtkWidget *widget,
                                                    gboolean   width_request_needed);
gboolean     _gtk_widget_get_height_request_needed (GtkWidget *widget);
void         _gtk_widget_set_height_request_needed (GtkWidget *widget,
                                                    gboolean   height_request_needed);

void _gtk_widget_override_size_request (GtkWidget *widget,
					int        width,
					int        height,
					int       *old_width,
					int       *old_height);
void _gtk_widget_restore_size_request  (GtkWidget *widget,
					int        old_width,
					int        old_height);

#ifdef G_OS_WIN32

const gchar *_gtk_get_datadir ();
const gchar *_gtk_get_libdir ();
const gchar *_gtk_get_sysconfdir ();
const gchar *_gtk_get_localedir ();
const gchar *_gtk_get_data_prefix ();

#undef GTK_DATADIR
#define GTK_DATADIR _gtk_get_datadir ()
#undef GTK_LIBDIR
#define GTK_LIBDIR _gtk_get_libdir ()
#undef GTK_LOCALEDIR
#define GTK_LOCALEDIR _gtk_get_localedir ()
#undef GTK_SYSCONFDIR
#define GTK_SYSCONFDIR _gtk_get_sysconfdir ()
#undef GTK_DATA_PREFIX
#define GTK_DATA_PREFIX _gtk_get_data_prefix ()

#endif /* G_OS_WIN32 */

gboolean _gtk_fnmatch (const char *pattern,
		       const char *string,
		       gboolean    no_leading_period);

#define GTK_PARAM_READABLE G_PARAM_READABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_WRITABLE G_PARAM_WRITABLE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB
#define GTK_PARAM_READWRITE G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB

/* Many keyboard shortcuts for Mac are the same as for X
 * except they use Command key instead of Control (e.g. Cut,
 * Copy, Paste). This symbol is for those simple cases. */
#ifndef GDK_WINDOWING_QUARTZ
#define GTK_DEFAULT_ACCEL_MOD_MASK GDK_CONTROL_MASK
#else
#define GTK_DEFAULT_ACCEL_MOD_MASK GDK_META_MASK
#endif


/* With GtkWidget     , a widget may be requested
 * its width for 2 or maximum 3 heights in one resize
 * (Note this define is limited by the bitfield sizes
 * defined on the SizeRequestCache structure).
 */
#define GTK_SIZE_REQUEST_CACHED_SIZES 3

typedef struct
{
  gint   for_size;
  gint   minimum_size;
  gint   natural_size;
} SizeRequest;

typedef struct {
  SizeRequest widths[GTK_SIZE_REQUEST_CACHED_SIZES];
  SizeRequest heights[GTK_SIZE_REQUEST_CACHED_SIZES];
  guint       cached_widths : 2;
  guint       cached_heights : 2;
  guint       last_cached_width : 2;
  guint       last_cached_height : 2;
} SizeRequestCache;


G_END_DECLS

#endif /* __GTK_PRIVATE_H__ */
