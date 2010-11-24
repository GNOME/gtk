/*
 * gtkappchooseronlinedummy.h: an extension point for online integration
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

#ifndef __GTK_APP_CHOOSER_ONLINE_DUMMY_H__
#define __GTK_APP_CHOOSER_ONLINE_DUMMY_H__

#include <gtk/gtkappchooseronline.h>
#include <glib.h>

#define GTK_TYPE_APP_CHOOSER_ONLINE_DUMMY            (_gtk_app_chooser_online_dummy_get_type ())
#define GTK_APP_CHOOSER_ONLINE_DUMMY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_APP_CHOOSER_ONLINE_DUMMY, GtkAppChooserOnlineDummy))
#define GTK_APP_CHOOSER_ONLINE_DUMMY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_APP_CHOOSER_ONLINE_DUMMY, GtkAppChooserOnlineDummyClass))
#define GTK_IS_APP_CHOOSER_ONLINE_DUMMY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_APP_CHOOSER_ONLINE_DUMMY))
#define GTK_IS_APP_CHOOSER_ONLINE_DUMMY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_APP_CHOOSER_ONLINE_DUMMY))
#define GTK_APP_CHOOSER_ONLINE_DUMMY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_APP_CHOOSER_ONLINE_DUMMY, GtkAppChooserOnlineDummyClass))

typedef struct _GtkAppChooserOnlineDummy        GtkAppChooserOnlineDummy;
typedef struct _GtkAppChooserOnlineDummyClass   GtkAppChooserOnlineDummyClass;
typedef struct _GtkAppChooserOnlineDummyPrivate GtkAppChooserOnlineDummyPrivate;

struct _GtkAppChooserOnlineDummy {
  GObject parent;
};

struct _GtkAppChooserOnlineDummyClass {
  GObjectClass parent_class;

  GtkAppChooserOnlineDummy *priv;
};

GType _gtk_app_chooser_online_dummy_get_type (void);

#endif /* __GTK_APP_CHOOSER_ONLINE_DUMMY_H__ */
