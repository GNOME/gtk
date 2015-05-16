/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_DND_H__
#define __GTK_DND_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>
#include <gtk/gtkselection.h>


G_BEGIN_DECLS

/**
 * GtkDestDefaults:
 * @GTK_DEST_DEFAULT_MOTION: If set for a widget, GTK+, during a drag over this
 *   widget will check if the drag matches this widget’s list of possible targets
 *   and actions.
 *   GTK+ will then call gdk_drag_status() as appropriate.
 * @GTK_DEST_DEFAULT_HIGHLIGHT: If set for a widget, GTK+ will draw a highlight on
 *   this widget as long as a drag is over this widget and the widget drag format
 *   and action are acceptable.
 * @GTK_DEST_DEFAULT_DROP: If set for a widget, when a drop occurs, GTK+ will
 *   will check if the drag matches this widget’s list of possible targets and
 *   actions. If so, GTK+ will call gtk_drag_get_data() on behalf of the widget.
 *   Whether or not the drop is successful, GTK+ will call gtk_drag_finish(). If
 *   the action was a move, then if the drag was successful, then %TRUE will be
 *   passed for the @delete parameter to gtk_drag_finish().
 * @GTK_DEST_DEFAULT_ALL: If set, specifies that all default actions should
 *   be taken.
 *
 * The #GtkDestDefaults enumeration specifies the various
 * types of action that will be taken on behalf
 * of the user for a drag destination site.
 */
typedef enum {
  GTK_DEST_DEFAULT_MOTION     = 1 << 0,
  GTK_DEST_DEFAULT_HIGHLIGHT  = 1 << 1,
  GTK_DEST_DEFAULT_DROP       = 1 << 2,
  GTK_DEST_DEFAULT_ALL        = 0x07
} GtkDestDefaults;

/**
 * GtkTargetFlags:
 * @GTK_TARGET_SAME_APP: If this is set, the target will only be selected
 *   for drags within a single application.
 * @GTK_TARGET_SAME_WIDGET: If this is set, the target will only be selected
 *   for drags within a single widget.
 * @GTK_TARGET_OTHER_APP: If this is set, the target will not be selected
 *   for drags within a single application.
 * @GTK_TARGET_OTHER_WIDGET: If this is set, the target will not be selected
 *   for drags withing a single widget.
 *
 * The #GtkTargetFlags enumeration is used to specify
 * constraints on a #GtkTargetEntry.
 */
typedef enum {
  GTK_TARGET_SAME_APP = 1 << 0,    /*< nick=same-app >*/
  GTK_TARGET_SAME_WIDGET = 1 << 1, /*< nick=same-widget >*/
  GTK_TARGET_OTHER_APP = 1 << 2,   /*< nick=other-app >*/
  GTK_TARGET_OTHER_WIDGET = 1 << 3 /*< nick=other-widget >*/
} GtkTargetFlags;

/* Destination side */

GDK_AVAILABLE_IN_ALL
void gtk_drag_get_data (GtkWidget      *widget,
			GdkDragContext *context,
			GdkAtom         target,
			guint32         time_);
GDK_AVAILABLE_IN_ALL
void gtk_drag_finish   (GdkDragContext *context,
			gboolean        success,
			gboolean        del,
			guint32         time_);

GDK_AVAILABLE_IN_ALL
GtkWidget *gtk_drag_get_source_widget (GdkDragContext *context);

GDK_AVAILABLE_IN_ALL
void gtk_drag_highlight   (GtkWidget  *widget);
GDK_AVAILABLE_IN_ALL
void gtk_drag_unhighlight (GtkWidget  *widget);

GDK_AVAILABLE_IN_ALL
void gtk_drag_dest_set   (GtkWidget            *widget,
			  GtkDestDefaults       flags,
  		          const GtkTargetEntry *targets,
			  gint                  n_targets,
			  GdkDragAction         actions);

GDK_AVAILABLE_IN_ALL
void gtk_drag_dest_set_proxy (GtkWidget      *widget,
			      GdkWindow      *proxy_window,
			      GdkDragProtocol protocol,
			      gboolean        use_coordinates);

GDK_AVAILABLE_IN_ALL
void gtk_drag_dest_unset (GtkWidget          *widget);

GDK_AVAILABLE_IN_ALL
GdkAtom        gtk_drag_dest_find_target     (GtkWidget      *widget,
                                              GdkDragContext *context,
                                              GtkTargetList  *target_list);
GDK_AVAILABLE_IN_ALL
GtkTargetList* gtk_drag_dest_get_target_list (GtkWidget      *widget);
GDK_AVAILABLE_IN_ALL
void           gtk_drag_dest_set_target_list (GtkWidget      *widget,
                                              GtkTargetList  *target_list);
GDK_AVAILABLE_IN_ALL
void           gtk_drag_dest_add_text_targets  (GtkWidget    *widget);
GDK_AVAILABLE_IN_ALL
void           gtk_drag_dest_add_image_targets (GtkWidget    *widget);
GDK_AVAILABLE_IN_ALL
void           gtk_drag_dest_add_uri_targets   (GtkWidget    *widget);

GDK_AVAILABLE_IN_ALL
void           gtk_drag_dest_set_track_motion  (GtkWidget *widget,
						gboolean   track_motion);
GDK_AVAILABLE_IN_ALL
gboolean       gtk_drag_dest_get_track_motion  (GtkWidget *widget);

/* Source side */

GDK_AVAILABLE_IN_ALL
void gtk_drag_source_set  (GtkWidget            *widget,
			   GdkModifierType       start_button_mask,
			   const GtkTargetEntry *targets,
			   gint                  n_targets,
			   GdkDragAction         actions);

GDK_AVAILABLE_IN_ALL
void gtk_drag_source_unset (GtkWidget        *widget);

GDK_AVAILABLE_IN_ALL
GtkTargetList* gtk_drag_source_get_target_list (GtkWidget     *widget);
GDK_AVAILABLE_IN_ALL
void           gtk_drag_source_set_target_list (GtkWidget     *widget,
                                                GtkTargetList *target_list);
GDK_AVAILABLE_IN_ALL
void           gtk_drag_source_add_text_targets  (GtkWidget     *widget);
GDK_AVAILABLE_IN_ALL
void           gtk_drag_source_add_image_targets (GtkWidget    *widget);
GDK_AVAILABLE_IN_ALL
void           gtk_drag_source_add_uri_targets   (GtkWidget    *widget);

GDK_AVAILABLE_IN_ALL
void gtk_drag_source_set_icon_pixbuf  (GtkWidget       *widget,
				       GdkPixbuf       *pixbuf);
GDK_DEPRECATED_IN_3_10_FOR(gtk_drag_source_set_icon_name)
void gtk_drag_source_set_icon_stock   (GtkWidget       *widget,
				       const gchar     *stock_id);
GDK_AVAILABLE_IN_ALL
void gtk_drag_source_set_icon_name    (GtkWidget       *widget,
				       const gchar     *icon_name);
GDK_AVAILABLE_IN_3_2
void gtk_drag_source_set_icon_gicon   (GtkWidget       *widget,
				       GIcon           *icon);

/* There probably should be functions for setting the targets
 * as a GtkTargetList
 */

GDK_AVAILABLE_IN_3_10
GdkDragContext *gtk_drag_begin_with_coordinates (GtkWidget         *widget,
                                                 GtkTargetList     *targets,
                                                 GdkDragAction      actions,
                                                 gint               button,
                                                 GdkEvent          *event,
                                                 gint               x,
                                                 gint               y);

GDK_DEPRECATED_IN_3_10_FOR(gtk_drag_begin_with_coordinates)
GdkDragContext *gtk_drag_begin (GtkWidget         *widget,
				GtkTargetList     *targets,
				GdkDragAction      actions,
				gint               button,
				GdkEvent          *event);

GDK_AVAILABLE_IN_3_16
void gtk_drag_cancel           (GdkDragContext *context);

/* Set the image being dragged around
 */
GDK_AVAILABLE_IN_ALL
void gtk_drag_set_icon_widget (GdkDragContext *context,
			       GtkWidget      *widget,
			       gint            hot_x,
			       gint            hot_y);
GDK_AVAILABLE_IN_ALL
void gtk_drag_set_icon_pixbuf (GdkDragContext *context,
			       GdkPixbuf      *pixbuf,
			       gint            hot_x,
			       gint            hot_y);
GDK_DEPRECATED_IN_3_10_FOR(gtk_drag_set_icon_name)
void gtk_drag_set_icon_stock  (GdkDragContext *context,
			       const gchar    *stock_id,
			       gint            hot_x,
			       gint            hot_y);
GDK_AVAILABLE_IN_ALL
void gtk_drag_set_icon_surface(GdkDragContext *context,
			       cairo_surface_t *surface);
GDK_AVAILABLE_IN_ALL
void gtk_drag_set_icon_name   (GdkDragContext *context,
			       const gchar    *icon_name,
			       gint            hot_x,
			       gint            hot_y);
GDK_AVAILABLE_IN_3_2
void gtk_drag_set_icon_gicon  (GdkDragContext *context,
			       GIcon          *icon,
			       gint            hot_x,
			       gint            hot_y);

GDK_AVAILABLE_IN_ALL
void gtk_drag_set_icon_default (GdkDragContext    *context);

GDK_AVAILABLE_IN_ALL
gboolean gtk_drag_check_threshold (GtkWidget *widget,
				   gint       start_x,
				   gint       start_y,
				   gint       current_x,
				   gint       current_y);


G_END_DECLS

#endif /* __GTK_DND_H__ */
