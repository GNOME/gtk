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

#ifndef __GTK_WIDGET_PRIVATE_H__
#define __GTK_WIDGET_PRIVATE_H__

G_BEGIN_DECLS

/* With GtkWidget, a widget may be requested
 * its width for 2 or maximum 3 heights in one resize
 * (Note this define is limited by the bitfield sizes
 * defined on the SizeRequestCache structure).
 */
#define GTK_SIZE_REQUEST_CACHED_SIZES   (3)

typedef struct
{
  /* the size this request is for */
  gint for_size;

  gint minimum_size;
  gint natural_size;
} SizeRequest;

typedef struct {
  SizeRequest widths[GTK_SIZE_REQUEST_CACHED_SIZES];
  SizeRequest heights[GTK_SIZE_REQUEST_CACHED_SIZES];

  guint       cached_widths      : 2;
  guint       cached_heights     : 2;
  guint       last_cached_width  : 2;
  guint       last_cached_height : 2;
} SizeRequestCache;

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

gboolean     _gtk_widget_get_sizegroup_visited (GtkWidget    *widget);
void         _gtk_widget_set_sizegroup_visited (GtkWidget    *widget,
						gboolean      visited);
gboolean     _gtk_widget_get_sizegroup_bumping (GtkWidget    *widget);
void         _gtk_widget_set_sizegroup_bumping (GtkWidget    *widget,
						gboolean      bumping);
void         _gtk_widget_add_sizegroup         (GtkWidget    *widget,
						gpointer      group);
void         _gtk_widget_remove_sizegroup      (GtkWidget    *widget,
						gpointer      group);
GSList      *_gtk_widget_get_sizegroups        (GtkWidget    *widget);

void _gtk_widget_override_size_request (GtkWidget *widget,
                                        int        width,
                                        int        height,
                                        int       *old_width,
                                        int       *old_height);
void _gtk_widget_restore_size_request  (GtkWidget *widget,
                                        int        old_width,
                                        int        old_height);

gboolean _gtk_widget_get_translation_to_window (GtkWidget      *widget,
                                                GdkWindow      *window,
                                                int            *x,
                                                int            *y);

G_END_DECLS

#endif /* __GTK_WIDGET_PRIVATE_H__ */
