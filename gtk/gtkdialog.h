/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_DIALOG_H__
#define __GTK_DIALOG_H__


#include <gdk/gdk.h>
#include <gtk/gtkwindow.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Parameters for dialog construction */
typedef enum
{
  GTK_DIALOG_MODAL,              /* call gtk_window_set_modal(win, TRUE) */
  GTK_DIALOG_DESTROY_WITH_PARENT /* call gtk_window_set_destroy_with_parent () */

} GtkDialogFlags;

/* Convenience enum to use for action_id's.  Positive values are
   totally user-interpreted. GTK will sometimes return GTK_ACTION_NONE
   if no action_id is available.

   Typical usage is:
      if (gtk_dialog_run(dialog) == GTK_ACTION_ACCEPT)
        blah();
        
*/
typedef enum
{
  /* GTK returns this if an action widget has no action_id,
     or the dialog gets destroyed with no action */
  GTK_ACTION_NONE = 0,
  /* GTK won't return these unless you pass them in
     as the action for an action widget */
  GTK_ACTION_REJECT = 1,
  GTK_ACTION_ACCEPT = 2
} GtkActionType;


#define GTK_TYPE_DIALOG                  (gtk_dialog_get_type ())
#define GTK_DIALOG(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_DIALOG, GtkDialog))
#define GTK_DIALOG_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_DIALOG, GtkDialogClass))
#define GTK_IS_DIALOG(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_DIALOG))
#define GTK_IS_DIALOG_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_DIALOG))
#define GTK_DIALOG_GET_CLASS(obj)        (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_DIALOG, GtkDialogClass))


typedef struct _GtkDialog        GtkDialog;
typedef struct _GtkDialogClass   GtkDialogClass;

struct _GtkDialog
{
  GtkWindow window;

  GtkWidget *vbox;
  GtkWidget *action_area;
};

struct _GtkDialogClass
{
  GtkWindowClass parent_class;

  void (* action) (GtkDialog *dialog, gint action_id);
};


GtkType    gtk_dialog_get_type (void);
GtkWidget* gtk_dialog_new      (void);

GtkWidget* gtk_dialog_new_empty        (const gchar     *title,
                                        GtkWindow       *parent,
                                        GtkDialogFlags   flags);
GtkWidget* gtk_dialog_new_with_buttons (const gchar     *title,
                                        GtkWindow       *parent,
                                        GtkDialogFlags   flags,
                                        const gchar *first_button_text,
                                        gint         first_action_id,
                                        ...);

void gtk_dialog_add_action_widget  (GtkDialog *dialog,
                                    GtkWidget *widget,
                                    gint       action_id);

void gtk_dialog_add_button         (GtkDialog   *dialog,
                                    const gchar *button_text,
                                    gint         action_id);

void gtk_dialog_add_buttons        (GtkDialog   *dialog,
                                    const gchar *first_button_text,
                                    gint         first_action_id,
                                    ...);

/* Emit action signal */
void gtk_dialog_action             (GtkDialog *dialog,
                                    gint       action_id);

/* Returns action_id */
gint gtk_dialog_run                (GtkDialog *dialog);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_DIALOG_H__ */
