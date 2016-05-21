/* gtktabstrip.h
 *
 * Copyright (C) 2016 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(__GTK_H_INSIDE__) && !defined(GTK_COMPILATION)
# error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_TAB_STRIP_H__
#define __GTK_TAB_STRIP_H__

#include <gtk/gtkbox.h>
#include <gtk/gtkstack.h>
#include <gtk/gtktypebuiltins.h>

G_BEGIN_DECLS

#define GTK_TYPE_TAB_STRIP                 (gtk_tab_strip_get_type ())
#define GTK_TAB_STRIP(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TAB_STRIP, GtkTabStrip))
#define GTK_IS_TAB_STRIP(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TAB_STRIP))
#define GTK_TAB_STRIP_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TAB_STRIP, GtkTabStripClass))
#define GTK_IS_TAB_STRIP_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TAB_STRIP))
#define GTK_TAB_STRIP_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TAB_STRIP, GtkTabStripClass))

typedef struct _GtkTabStrip      GtkTabStrip;
typedef struct _GtkTabStripClass GtkTabStripClass;

struct _GtkTabStrip
{
  GtkBox parent;
};

struct _GtkTabStripClass
{
  GtkBoxClass parent_class;
};

GDK_AVAILABLE_IN_3_22
GType            gtk_tab_strip_get_type        (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_22
GtkWidget       *gtk_tab_strip_new             (void);
GDK_AVAILABLE_IN_3_22
GtkStack        *gtk_tab_strip_get_stack       (GtkTabStrip     *self);
GDK_AVAILABLE_IN_3_22
void             gtk_tab_strip_set_stack       (GtkTabStrip     *self,
                                                GtkStack        *stack);
GDK_AVAILABLE_IN_3_22
gboolean         gtk_tab_strip_get_closable    (GtkTabStrip     *self);
GDK_AVAILABLE_IN_3_22
void             gtk_tab_strip_set_closable    (GtkTabStrip     *self,
                                                gboolean         closable);
GDK_AVAILABLE_IN_3_22
gboolean         gtk_tab_strip_get_scrollable  (GtkTabStrip     *self);
GDK_AVAILABLE_IN_3_22
void             gtk_tab_strip_set_scrollable  (GtkTabStrip     *self,
                                                gboolean         scrollable);

G_END_DECLS

#endif /* __GTK_TAB_STRIP_H__ */
