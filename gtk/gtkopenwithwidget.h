/*
 * gtkopenwithwidget.h: an open-with widget
 *
 * Copyright (C) 2004 Novell, Inc.
 * Copyright (C) 2007, 2010 Red Hat, Inc.
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
 * Authors: Dave Camp <dave@novell.com>
 *          Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <ccecchi@redhat.com>
 */

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_OPEN_WITH_WIDGET_H__
#define __GTK_OPEN_WITH_WIDGET_H__

#include <gtk/gtkbox.h>
#include <gio/gio.h>

#define GTK_TYPE_OPEN_WITH_WIDGET\
  (gtk_open_with_widget_get_type ())
#define GTK_OPEN_WITH_WIDGET(obj)\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_OPEN_WITH_WIDGET, GtkOpenWithWidget))
#define GTK_OPEN_WITH_WIDGET_CLASS(klass)\
  (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_OPEN_WITH_WIDGET, GtkOpenWithWidgetClass))
#define GTK_IS_OPEN_WITH_WIDGET(obj)\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_OPEN_WITH_WIDGET))
#define GTK_IS_OPEN_WITH_WIDGET_CLASS(klass)\
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_OPEN_WITH_WIDGET))
#define GTK_OPEN_WITH_WIDGET_GET_CLASS(obj)\
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_OPEN_WITH_WIDGET, GtkOpenWithWidgetClass))

typedef struct _GtkOpenWithWidget        GtkOpenWithWidget;
typedef struct _GtkOpenWithWidgetClass   GtkOpenWithWidgetClass;
typedef struct _GtkOpenWithWidgetPrivate GtkOpenWithWidgetPrivate;

struct _GtkOpenWithWidget {
  GtkBox parent;

  /*< private >*/
  GtkOpenWithWidgetPrivate *priv;
};

struct _GtkOpenWithWidgetClass {
  GtkBoxClass parent_class;

  void (* application_selected) (GtkOpenWithWidget *self,
				 GAppInfo *app_info);

  void (* application_activated) (GtkOpenWithWidget *self,
				  GAppInfo *app_info);

  /* padding for future class expansion */
  gpointer padding[16];
};

typedef enum {
  GTK_OPEN_WITH_WIDGET_SHOW_RECOMMENDED = 1 << 0,
  GTK_OPEN_WITH_WIDGET_SHOW_FALLBACK = 1 << 1,
  GTK_OPEN_WITH_WIDGET_SHOW_OTHER = 1 << 2,
  GTK_OPEN_WITH_WIDGET_SHOW_ALL = 1 << 3,
} GtkOpenWithWidgetShowFlags;

GType      gtk_open_with_widget_get_type (void) G_GNUC_CONST;

GtkWidget * gtk_open_with_widget_new (const gchar *content_type);

void gtk_open_with_widget_set_show_default (GtkOpenWithWidget *self,
					    gboolean setting);
gboolean gtk_open_with_widget_get_show_default (GtkOpenWithWidget *self);

void gtk_open_with_widget_set_show_recommended (GtkOpenWithWidget *self,
						gboolean setting);
gboolean gtk_open_with_widget_get_show_recommended (GtkOpenWithWidget *self);

void gtk_open_with_widget_set_show_fallback (GtkOpenWithWidget *self,
					     gboolean setting);
gboolean gtk_open_with_widget_get_show_fallback (GtkOpenWithWidget *self);

void gtk_open_with_widget_set_show_other (GtkOpenWithWidget *self,
					  gboolean setting);
gboolean gtk_open_with_widget_get_show_other (GtkOpenWithWidget *self);

void gtk_open_with_widget_set_show_all (GtkOpenWithWidget *self,
					gboolean show_all);
gboolean gtk_open_with_widget_get_show_all (GtkOpenWithWidget *self);

void gtk_open_with_widget_set_default_text (GtkOpenWithWidget *self,
					    const gchar *text);
const gchar * gtk_open_with_widget_get_default_text (GtkOpenWithWidget *self);

#endif /* __GTK_OPEN_WITH_WIDGET_H__ */
