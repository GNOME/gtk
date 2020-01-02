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

#ifndef __GTK_DRAG_DEST_H__
#define __GTK_DRAG_DEST_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkselection.h>
#include <gtk/gtkwidget.h>


G_BEGIN_DECLS

typedef struct _GtkDropTarget GtkDropTarget;

/**
 * GtkDestDefaults:
 * @GTK_DEST_DEFAULT_MOTION: If set for a widget, GTK+, during a drag over this
 *   widget will check if the drag matches this widget’s list of possible formats
 *   and actions.
 *   GTK+ will then call gdk_drag_status() as appropriate.
 * @GTK_DEST_DEFAULT_HIGHLIGHT: If set for a widget, GTK+ will draw a highlight on
 *   this widget as long as a drag is over this widget and the widget drag format
 *   and action are acceptable.
 * @GTK_DEST_DEFAULT_OP: If set for a widget, when a drop occurs, GTK+ will
 *   will check if the drag matches this widget’s list of possible formats and
 *   actions. If so, GTK+ will call gtk_drag_get_data() on behalf of the widget.
 *   Whether or not the drop is successful, GTK+ will call gdk_drag_finish(). If
 *   the action was a move, then if the drag was successful, then %TRUE will be
 *   passed for the @delete parameter to gdk_drag_finish().
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

GDK_AVAILABLE_IN_ALL
GtkDropTarget *gtk_drag_dest_set   (GtkWidget            *widget,
                                    GtkDestDefaults       flags,
                                    GdkContentFormats    *targets,
                                    GdkDragAction         actions);

GDK_AVAILABLE_IN_ALL
void gtk_drag_dest_unset (GtkWidget          *widget);

GDK_AVAILABLE_IN_ALL
const char *            gtk_drag_dest_find_target       (GtkWidget              *widget,
                                                         GdkDrop                *drop,
                                                         GdkContentFormats      *target_list);
GDK_AVAILABLE_IN_ALL
GdkContentFormats*      gtk_drag_dest_get_target_list   (GtkWidget              *widget);
GDK_AVAILABLE_IN_ALL
void           gtk_drag_dest_set_target_list (GtkWidget      *widget,
                                              GdkContentFormats  *target_list);
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


#define GTK_TYPE_DROP_TARGET         (gtk_drop_target_get_type ())
#define GTK_DROP_TARGET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_DROP_TARGET, GtkDropTarget))
#define GTK_DROP_TARGET_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_DROP_TARGET, GtkDropTargetClass))
#define GTK_IS_DROP_TARGET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_DROP_TARGET))
#define GTK_IS_DROP_TARGET_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_DROP_TARGET))
#define GTK_DROP_TARGET_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_DROP_TARGET, GtkDropTargetClass))

typedef struct _GtkDropTargetClass GtkDropTargetClass;

GDK_AVAILABLE_IN_ALL
GType              gtk_drop_target_get_type         (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkDropTarget       *gtk_drop_target_new            (GtkDestDefaults    defaults,
                                                     GdkContentFormats *formats,
                                                     GdkDragAction      actions);

GDK_AVAILABLE_IN_ALL
void               gtk_drop_target_set_formats      (GtkDropTarget     *dest,
                                                     GdkContentFormats *formats);
GDK_AVAILABLE_IN_ALL
GdkContentFormats *gtk_drop_target_get_formats      (GtkDropTarget     *dest);

GDK_AVAILABLE_IN_ALL
void               gtk_drop_target_set_actions      (GtkDropTarget     *dest,
                                                     GdkDragAction      actions);
GDK_AVAILABLE_IN_ALL
GdkDragAction      gtk_drop_target_get_actions      (GtkDropTarget     *dest);

GDK_AVAILABLE_IN_ALL
void               gtk_drop_target_set_defaults     (GtkDropTarget     *dest,
                                                     GtkDestDefaults    defaults);
GDK_AVAILABLE_IN_ALL
GtkDestDefaults    gtk_drop_target_get_defaults     (GtkDropTarget     *dest);

GDK_AVAILABLE_IN_ALL
void               gtk_drop_target_set_track_motion (GtkDropTarget     *dest,
                                                     gboolean           track_motion);
GDK_AVAILABLE_IN_ALL
gboolean           gtk_drop_target_get_track_motion (GtkDropTarget     *dest);

GDK_AVAILABLE_IN_ALL
void               gtk_drop_target_attach           (GtkDropTarget     *dest,
                                                     GtkWidget         *widget);
GDK_AVAILABLE_IN_ALL
void               gtk_drop_target_detach           (GtkDropTarget     *dest);

GDK_AVAILABLE_IN_ALL
GdkDrop           *gtk_drop_target_get_drop         (GtkDropTarget     *dest);

GDK_AVAILABLE_IN_ALL
GtkWidget         *gtk_drop_target_get_target       (GtkDropTarget     *dest);

GDK_AVAILABLE_IN_ALL
const char        *gtk_drop_target_find_mimetype    (GtkDropTarget     *dest);

GDK_AVAILABLE_IN_ALL
void               gtk_drag_highlight            (GtkWidget  *widget);

GDK_AVAILABLE_IN_ALL
void               gtk_drag_unhighlight          (GtkWidget  *widget);


G_END_DECLS

#endif /* __GTK_DRAG_DEST_H__ */
