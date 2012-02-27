/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * GtkToolbar copyright (C) Federico Mena
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@gnome.org>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
 * Copyright (C) 2003 Soeren Sandmann <sandmann@daimi.au.dk>
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

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_TOOLBAR_H__
#define __GTK_TOOLBAR_H__


#include <gtk/gtkcontainer.h>
#include <gtk/gtktoolitem.h>

G_BEGIN_DECLS


#define GTK_TYPE_TOOLBAR            (gtk_toolbar_get_type ())
#define GTK_TOOLBAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TOOLBAR, GtkToolbar))
#define GTK_TOOLBAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TOOLBAR, GtkToolbarClass))
#define GTK_IS_TOOLBAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TOOLBAR))
#define GTK_IS_TOOLBAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_TOOLBAR))
#define GTK_TOOLBAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_TOOLBAR, GtkToolbarClass))

typedef enum
{
  GTK_TOOLBAR_SPACE_EMPTY,
  GTK_TOOLBAR_SPACE_LINE
} GtkToolbarSpaceStyle;

typedef struct _GtkToolbar              GtkToolbar;
typedef struct _GtkToolbarPrivate       GtkToolbarPrivate;
typedef struct _GtkToolbarClass         GtkToolbarClass;

struct _GtkToolbar
{
  GtkContainer container;

  GtkToolbarPrivate *priv;
};

struct _GtkToolbarClass
{
  GtkContainerClass parent_class;

  /* signals */
  void     (* orientation_changed) (GtkToolbar       *toolbar,
				    GtkOrientation    orientation);
  void     (* style_changed)       (GtkToolbar       *toolbar,
				    GtkToolbarStyle   style);
  gboolean (* popup_context_menu)  (GtkToolbar       *toolbar,
				    gint              x,
				    gint              y,
				    gint              button_number);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GType           gtk_toolbar_get_type                (void) G_GNUC_CONST;
GtkWidget *     gtk_toolbar_new                     (void);

void            gtk_toolbar_insert                  (GtkToolbar      *toolbar,
						     GtkToolItem     *item,
						     gint             pos);

gint            gtk_toolbar_get_item_index          (GtkToolbar      *toolbar,
						     GtkToolItem     *item);
gint            gtk_toolbar_get_n_items             (GtkToolbar      *toolbar);
GtkToolItem *   gtk_toolbar_get_nth_item            (GtkToolbar      *toolbar,
						     gint             n);

gboolean        gtk_toolbar_get_show_arrow          (GtkToolbar      *toolbar);
void            gtk_toolbar_set_show_arrow          (GtkToolbar      *toolbar,
						     gboolean         show_arrow);

GtkToolbarStyle gtk_toolbar_get_style               (GtkToolbar      *toolbar);
void            gtk_toolbar_set_style               (GtkToolbar      *toolbar,
						     GtkToolbarStyle  style);
void            gtk_toolbar_unset_style             (GtkToolbar      *toolbar);

GtkIconSize     gtk_toolbar_get_icon_size           (GtkToolbar      *toolbar);
void            gtk_toolbar_set_icon_size           (GtkToolbar      *toolbar,
                                                     GtkIconSize      icon_size);
void            gtk_toolbar_unset_icon_size         (GtkToolbar      *toolbar);

GtkReliefStyle  gtk_toolbar_get_relief_style        (GtkToolbar      *toolbar);
gint            gtk_toolbar_get_drop_index          (GtkToolbar      *toolbar,
						     gint             x,
						     gint             y);
void            gtk_toolbar_set_drop_highlight_item (GtkToolbar      *toolbar,
						     GtkToolItem     *tool_item,
						     gint             index_);


/* internal functions */
gchar *         _gtk_toolbar_elide_underscores      (const gchar         *original);
void            _gtk_toolbar_paint_space_line       (GtkWidget           *widget,
						     GtkToolbar          *toolbar,
                                                     cairo_t             *cr);
gint            _gtk_toolbar_get_default_space_size (void);


G_END_DECLS

#endif /* __GTK_TOOLBAR_H__ */
