/*
 * gtkopenwithonlinedummy.h: an extension point for online integration
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

#ifndef __GTK_OPEN_WITH_ONLINE_DUMMY_H__
#define __GTK_OPEN_WITH_ONLINE_DUMMY_H__

#include <gtk/gtkopenwithonline.h>
#include <glib.h>

#define GTK_TYPE_OPEN_WITH_ONLINE_DUMMY\
  (_gtk_open_with_online_dummy_get_type ())
#define GTK_OPEN_WITH_ONLINE_DUMMY(obj)\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_OPEN_WITH_ONLINE_DUMMY, GtkOpenWithOnlineDummy))
#define GTK_OPEN_WITH_ONLINE_DUMMY_CLASS(klass)\
  (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_OPEN_WITH_ONLINE_DUMMY, GtkOpenWithOnlineDummyClass))
#define GTK_IS_OPEN_WITH_ONLINE_DUMMY(obj)\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_OPEN_WITH_ONLINE_DUMMY))
#define GTK_IS_OPEN_WITH_ONLINE_DUMMY_CLASS(klass)\
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_OPEN_WITH_ONLINE_DUMMY))
#define GTK_OPEN_WITH_ONLINE_DUMMY_GET_CLASS(obj)\
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_OPEN_WITH_ONLINE_DUMMY, GtkOpenWithOnlineDummyClass))

typedef struct _GtkOpenWithOnlineDummy        GtkOpenWithOnlineDummy;
typedef struct _GtkOpenWithOnlineDummyClass   GtkOpenWithOnlineDummyClass;
typedef struct _GtkOpenWithOnlineDummyPrivate GtkOpenWithOnlineDummyPrivate;

struct _GtkOpenWithOnlineDummy {
  GObject parent;
};

struct _GtkOpenWithOnlineDummyClass {
  GObjectClass parent_class;

  GtkOpenWithOnlineDummy *priv;
};

GType _gtk_open_with_online_dummy_get_type (void);

#endif /* __GTK_OPEN_WITH_ONLINE_DUMMY_H__ */
