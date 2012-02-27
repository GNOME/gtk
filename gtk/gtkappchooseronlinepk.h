/*
 * gtkappchooseronlinepk.h: an extension point for online integration
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Cosimo Cecchi <ccecchi@redhat.com>
 */

#ifndef __GTK_APP_CHOOSER_ONLINE_PK_H__
#define __GTK_APP_CHOOSER_ONLINE_PK_H__

#include <gtk/gtkappchooseronline.h>
#include <glib.h>

#define GTK_TYPE_APP_CHOOSER_ONLINE_PK            (_gtk_app_chooser_online_pk_get_type ())
#define GTK_APP_CHOOSER_ONLINE_PK(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_APP_CHOOSER_ONLINE_PK, GtkAppChooserOnlinePk))
#define GTK_APP_CHOOSER_ONLINE_PK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_APP_CHOOSER_ONLINE_PK, GtkAppChooserOnlinePkClass))
#define GTK_IS_APP_CHOOSER_ONLINE_PK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_APP_CHOOSER_ONLINE_PK))
#define GTK_IS_APP_CHOOSER_ONLINE_PK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_APP_CHOOSER_ONLINE_PK))
#define GTK_APP_CHOOSER_ONLINE_PK_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_APP_CHOOSER_ONLINE_PK, GtkAppChooserOnlinePkClass))

typedef struct _GtkAppChooserOnlinePk        GtkAppChooserOnlinePk;
typedef struct _GtkAppChooserOnlinePkClass   GtkAppChooserOnlinePkClass;
typedef struct _GtkAppChooserOnlinePkPrivate GtkAppChooserOnlinePkPrivate;

struct _GtkAppChooserOnlinePk {
  GObject parent;

  GtkAppChooserOnlinePkPrivate *priv;
};

struct _GtkAppChooserOnlinePkClass {
  GObjectClass parent_class;
};

GType _gtk_app_chooser_online_pk_get_type (void);

#endif /* __GTK_APP_CHOOSER_ONLINE_PK_H__ */
