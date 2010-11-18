/*
 * gtkopenwithdialog.h: an open-with dialog
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

#ifndef __GTK_OPEN_WITH_DIALOG_H__
#define __GTK_OPEN_WITH_DIALOG_H__

#include <gtk/gtkdialog.h>
#include <gio/gio.h>

#define GTK_TYPE_OPEN_WITH_DIALOG\
  (gtk_open_with_dialog_get_type ())
#define GTK_OPEN_WITH_DIALOG(obj)\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_OPEN_WITH_DIALOG, GtkOpenWithDialog))
#define GTK_OPEN_WITH_DIALOG_CLASS(klass)\
  (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_OPEN_WITH_DIALOG, GtkOpenWithDialogClass))
#define GTK_IS_OPEN_WITH_DIALOG(obj)\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_OPEN_WITH_DIALOG))
#define GTK_IS_OPEN_WITH_DIALOG_CLASS(klass)\
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_OPEN_WITH_DIALOG))
#define GTK_OPEN_WITH_DIALOG_GET_CLASS(obj)\
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_OPEN_WITH_DIALOG, GtkOpenWithDialogClass))

typedef struct _GtkOpenWithDialog        GtkOpenWithDialog;
typedef struct _GtkOpenWithDialogClass   GtkOpenWithDialogClass;
typedef struct _GtkOpenWithDialogPrivate GtkOpenWithDialogPrivate;

struct _GtkOpenWithDialog {
  GtkDialog parent;

  /*< private >*/
  GtkOpenWithDialogPrivate *priv;
};

struct _GtkOpenWithDialogClass {
  GtkDialogClass parent_class;

  /* padding for future class expansion */
  gpointer padding[16];
};

GType      gtk_open_with_dialog_get_type (void) G_GNUC_CONST;

GtkWidget * gtk_open_with_dialog_new (GtkWindow *parent,
				      GtkDialogFlags flags,
				      GFile *file);
GtkWidget * gtk_open_with_dialog_new_for_content_type (GtkWindow *parent,
						       GtkDialogFlags flags,
						       const gchar *content_type);

GtkWidget * gtk_open_with_dialog_get_widget (GtkOpenWithDialog *self);

#endif /* __GTK_OPEN_WITH_DIALOG_H__ */
