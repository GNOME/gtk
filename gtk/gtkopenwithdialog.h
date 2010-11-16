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

  void (*application_selected) (GtkOpenWithDialog *dialog,
				GAppInfo *application);

  /* padding for future class expansion */
  gpointer padding[16];
};

typedef enum {
  GTK_OPEN_WITH_DIALOG_MODE_OPEN_FILE = 0,
  GTK_OPEN_WITH_DIALOG_MODE_SELECT_DEFAULT
} GtkOpenWithDialogMode;

GType      gtk_open_with_dialog_get_type (void);

GtkWidget * gtk_open_with_dialog_new (GtkWindow *parent,
				      GtkDialogFlags flags,
				      GtkOpenWithDialogMode mode,
				      GFile *file);
GtkWidget * gtk_open_with_dialog_new_for_content_type (GtkWindow *parent,
						       GtkDialogFlags flags,
						       GtkOpenWithDialogMode mode,
						       const gchar *content_type);

void gtk_open_with_dialog_set_show_other_applications (GtkOpenWithDialog *self,
						       gboolean show_other_applications);
gboolean gtk_open_with_dialog_get_show_other_applications (GtkOpenWithDialog *self);

void gtk_open_with_dialog_set_show_set_as_default_button (GtkOpenWithDialog *self,
							  gboolean show_button);
gboolean gtk_open_with_get_show_set_as_default_button (GtkOpenWithDialog *self);

GAppInfo * gtk_open_with_dialog_get_selected_application (GtkOpenWithDialog *self);

#endif /* __GTK_OPEN_WITH_DIALOG_H__ */
