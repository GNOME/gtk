/* GtkCustomPaperUnixDialog
 * Copyright (C) 2006 Alexander Larsson <alexl@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_CUSTOM_PAPER_UNIX_DIALOG_H__
#define __GTK_CUSTOM_PAPER_UNIX_DIALOG_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_CUSTOM_PAPER_UNIX_DIALOG                  (gtk_custom_paper_unix_dialog_get_type ())
#define GTK_CUSTOM_PAPER_UNIX_DIALOG(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CUSTOM_PAPER_UNIX_DIALOG, GtkCustomPaperUnixDialog))
#define GTK_CUSTOM_PAPER_UNIX_DIALOG_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CUSTOM_PAPER_UNIX_DIALOG, GtkCustomPaperUnixDialogClass))
#define GTK_IS_CUSTOM_PAPER_UNIX_DIALOG(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CUSTOM_PAPER_UNIX_DIALOG))
#define GTK_IS_CUSTOM_PAPER_UNIX_DIALOG_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CUSTOM_PAPER_UNIX_DIALOG))
#define GTK_CUSTOM_PAPER_UNIX_DIALOG_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CUSTOM_PAPER_UNIX_DIALOG, GtkCustomPaperUnixDialogClass))


typedef struct _GtkCustomPaperUnixDialog         GtkCustomPaperUnixDialog;
typedef struct _GtkCustomPaperUnixDialogClass    GtkCustomPaperUnixDialogClass;
typedef struct _GtkCustomPaperUnixDialogPrivate  GtkCustomPaperUnixDialogPrivate;

struct _GtkCustomPaperUnixDialog
{
  GtkDialog parent_instance;

  GtkCustomPaperUnixDialogPrivate *priv;
};

struct _GtkCustomPaperUnixDialogClass
{
  GtkDialogClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

GDK_AVAILABLE_IN_ALL
GType             gtk_custom_paper_unix_dialog_get_type           (void) G_GNUC_CONST;
GtkWidget *       _gtk_custom_paper_unix_dialog_new                (GtkWindow   *parent,
								   const gchar *title);
GtkUnit           _gtk_print_get_default_user_units                (void);
void              _gtk_print_load_custom_papers                    (GtkListStore *store);
void              _gtk_print_save_custom_papers                    (GtkListStore *store);
GList *           _gtk_load_custom_papers                          (void);


G_END_DECLS

#endif /* __GTK_CUSTOM_PAPER_UNIX_DIALOG_H__ */
