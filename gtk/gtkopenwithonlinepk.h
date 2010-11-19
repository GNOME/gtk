/*
 * gtkopenwithonlinepk.h: an extension point for online integration
 *
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Cosimo Cecchi <ccecchi@redhat.com>
 */

#ifndef __GTK_OPEN_WITH_ONLINE_PK_H__
#define __GTK_OPEN_WITH_ONLINE_PK_H__

#include <gtk/gtkopenwithonline.h>
#include <glib.h>

#define GTK_TYPE_OPEN_WITH_ONLINE_PK\
  (_gtk_open_with_online_pk_get_type ())
#define GTK_OPEN_WITH_ONLINE_PK(obj)\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_OPEN_WITH_ONLINE_PK, GtkOpenWithOnlinePk))
#define GTK_OPEN_WITH_ONLINE_PK_CLASS(klass)\
  (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_OPEN_WITH_ONLINE_PK, GtkOpenWithOnlinePkClass))
#define GTK_IS_OPEN_WITH_ONLINE_PK(obj)\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_OPEN_WITH_ONLINE_PK))
#define GTK_IS_OPEN_WITH_ONLINE_PK_CLASS(klass)\
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_OPEN_WITH_ONLINE_PK))
#define GTK_OPEN_WITH_ONLINE_PK_GET_CLASS(obj)\
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_OPEN_WITH_ONLINE_PK, GtkOpenWithOnlinePkClass))

typedef struct _GtkOpenWithOnlinePk        GtkOpenWithOnlinePk;
typedef struct _GtkOpenWithOnlinePkClass   GtkOpenWithOnlinePkClass;
typedef struct _GtkOpenWithOnlinePkPrivate GtkOpenWithOnlinePkPrivate;

struct _GtkOpenWithOnlinePk {
  GObject parent;

  GtkOpenWithOnlinePkPrivate *priv;
};

struct _GtkOpenWithOnlinePkClass {
  GObjectClass parent_class;
};

GType _gtk_open_with_online_pk_get_type (void);

#endif /* __GTK_OPEN_WITH_ONLINE_PK_H__ */
