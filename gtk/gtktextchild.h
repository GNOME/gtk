/* GTK - The GIMP Toolkit
 * gtktextchild.h Copyright (C) 2000 Red Hat, Inc.
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

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <glib-object.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS


/**
 * GtkTextChildAnchor:
 *
 * Marks a spot in a `GtkTextBuffer` where child widgets can be “anchored”.
 *
 * The anchor can have multiple widgets anchored, to allow for multiple views.
 */
typedef struct _GtkTextChildAnchor      GtkTextChildAnchor;
typedef struct _GtkTextChildAnchorClass GtkTextChildAnchorClass;

#define GTK_TYPE_TEXT_CHILD_ANCHOR              (gtk_text_child_anchor_get_type ())
#define GTK_TEXT_CHILD_ANCHOR(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GTK_TYPE_TEXT_CHILD_ANCHOR, GtkTextChildAnchor))
#define GTK_TEXT_CHILD_ANCHOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TEXT_CHILD_ANCHOR, GtkTextChildAnchorClass))
#define GTK_IS_TEXT_CHILD_ANCHOR(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GTK_TYPE_TEXT_CHILD_ANCHOR))
#define GTK_IS_TEXT_CHILD_ANCHOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TEXT_CHILD_ANCHOR))
#define GTK_TEXT_CHILD_ANCHOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TEXT_CHILD_ANCHOR, GtkTextChildAnchorClass))

struct _GtkTextChildAnchor
{
  GObject parent_instance;

  /*< private >*/
  gpointer segment;
};

struct _GtkTextChildAnchorClass
{
  GObjectClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GDK_AVAILABLE_IN_ALL
GType gtk_text_child_anchor_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
GtkTextChildAnchor *gtk_text_child_anchor_new (void);

GDK_AVAILABLE_IN_4_6
GtkTextChildAnchor *gtk_text_child_anchor_new_with_replacement (const char *character);

GDK_AVAILABLE_IN_ALL
GtkWidget **gtk_text_child_anchor_get_widgets (GtkTextChildAnchor *anchor,
                                               guint *out_len);
GDK_AVAILABLE_IN_ALL
gboolean gtk_text_child_anchor_get_deleted (GtkTextChildAnchor *anchor);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtkTextChildAnchor, g_object_unref)

G_END_DECLS

