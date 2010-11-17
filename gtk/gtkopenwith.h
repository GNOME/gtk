/*
 * gtkopenwith.h: open-with interface
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

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_OPEN_WITH_H__
#define __GTK_OPEN_WITH_H__

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GTK_TYPE_OPEN_WITH\
  (gtk_open_with_get_type ())
#define GTK_OPEN_WITH(obj)\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_OPEN_WITH, GtkOpenWith))
#define GTK_IS_OPEN_WITH(obj)\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_OPEN_WITH))

typedef struct _GtkOpenWith GtkOpenWith;

GType gtk_open_with_get_type () G_GNUC_CONST;

GAppInfo * gtk_open_with_get_app_info (GtkOpenWith *self);
gchar * gtk_open_with_get_content_type (GtkOpenWith *self);

G_END_DECLS

#endif /* __GTK_OPEN_WITH_H__ */

