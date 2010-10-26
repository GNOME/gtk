/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkFontSelection widget for Gtk+, by Damon Chaplin, May 1998.
 * Based on the GnomeFontSelector widget, by Elliot Lee, but major changes.
 * The GnomeFontSelector was derived from app/text_tool.c in the GIMP.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_FONTSEL_H__
#define __GTK_FONTSEL_H__


#include <gtk/gtkdialog.h>
#include <gtk/gtkvbox.h>


G_BEGIN_DECLS

#define GTK_TYPE_FONT_SELECTION              (gtk_font_selection_get_type ())
#define GTK_FONT_SELECTION(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_FONT_SELECTION, GtkFontSelection))
#define GTK_FONT_SELECTION_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FONT_SELECTION, GtkFontSelectionClass))
#define GTK_IS_FONT_SELECTION(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_FONT_SELECTION))
#define GTK_IS_FONT_SELECTION_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FONT_SELECTION))
#define GTK_FONT_SELECTION_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FONT_SELECTION, GtkFontSelectionClass))


#define GTK_TYPE_FONT_SELECTION_DIALOG              (gtk_font_selection_dialog_get_type ())
#define GTK_FONT_SELECTION_DIALOG(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_FONT_SELECTION_DIALOG, GtkFontSelectionDialog))
#define GTK_FONT_SELECTION_DIALOG_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FONT_SELECTION_DIALOG, GtkFontSelectionDialogClass))
#define GTK_IS_FONT_SELECTION_DIALOG(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_FONT_SELECTION_DIALOG))
#define GTK_IS_FONT_SELECTION_DIALOG_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FONT_SELECTION_DIALOG))
#define GTK_FONT_SELECTION_DIALOG_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FONT_SELECTION_DIALOG, GtkFontSelectionDialogClass))


typedef struct _GtkFontSelection              GtkFontSelection;
typedef struct _GtkFontSelectionPrivate       GtkFontSelectionPrivate;
typedef struct _GtkFontSelectionClass         GtkFontSelectionClass;

typedef struct _GtkFontSelectionDialog              GtkFontSelectionDialog;
typedef struct _GtkFontSelectionDialogPrivate       GtkFontSelectionDialogPrivate;
typedef struct _GtkFontSelectionDialogClass         GtkFontSelectionDialogClass;

struct _GtkFontSelection
{
  GtkVBox parent_instance;

  /*< private >*/
  GtkFontSelectionPrivate *priv;
};

struct _GtkFontSelectionClass
{
  GtkVBoxClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};


struct _GtkFontSelectionDialog
{
  GtkDialog parent_instance;

  /*< private >*/
  GtkFontSelectionDialogPrivate *priv;
};

struct _GtkFontSelectionDialogClass
{
  GtkDialogClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};



/*****************************************************************************
 * GtkFontSelection functions.
 *   see the comments in the GtkFontSelectionDialog functions.
 *****************************************************************************/

GType	     gtk_font_selection_get_type	  (void) G_GNUC_CONST;
GtkWidget *  gtk_font_selection_new               (void);
GtkWidget *  gtk_font_selection_get_family_list   (GtkFontSelection *fontsel);
GtkWidget *  gtk_font_selection_get_face_list     (GtkFontSelection *fontsel);
GtkWidget *  gtk_font_selection_get_size_entry    (GtkFontSelection *fontsel);
GtkWidget *  gtk_font_selection_get_size_list     (GtkFontSelection *fontsel);
GtkWidget *  gtk_font_selection_get_preview_entry (GtkFontSelection *fontsel);
PangoFontFamily *
             gtk_font_selection_get_family        (GtkFontSelection *fontsel);
PangoFontFace *
             gtk_font_selection_get_face          (GtkFontSelection *fontsel);
gint         gtk_font_selection_get_size          (GtkFontSelection *fontsel);
gchar*       gtk_font_selection_get_font_name     (GtkFontSelection *fontsel);

gboolean     gtk_font_selection_set_font_name     (GtkFontSelection *fontsel,
                                                   const gchar      *fontname);
const gchar* gtk_font_selection_get_preview_text  (GtkFontSelection *fontsel);
void         gtk_font_selection_set_preview_text  (GtkFontSelection *fontsel,
                                                   const gchar      *text);

/*****************************************************************************
 * GtkFontSelectionDialog functions.
 *   most of these functions simply call the corresponding function in the
 *   GtkFontSelection.
 *****************************************************************************/

GType	   gtk_font_selection_dialog_get_type	       (void) G_GNUC_CONST;
GtkWidget *gtk_font_selection_dialog_new	       (const gchar            *title);

GtkWidget *gtk_font_selection_dialog_get_ok_button     (GtkFontSelectionDialog *fsd);
GtkWidget *gtk_font_selection_dialog_get_cancel_button (GtkFontSelectionDialog *fsd);
GtkWidget *gtk_font_selection_dialog_get_font_selection (GtkFontSelectionDialog *fsd);

/* This returns the X Logical Font Description fontname, or NULL if no font
   is selected. Note that there is a slight possibility that the font might not
   have been loaded OK. You should call gtk_font_selection_dialog_get_font()
   to see if it has been loaded OK.
   You should g_free() the returned font name after you're done with it. */
gchar*	   gtk_font_selection_dialog_get_font_name     (GtkFontSelectionDialog *fsd);

/* This sets the currently displayed font. It should be a valid X Logical
   Font Description font name (anything else will be ignored), e.g.
   "-adobe-courier-bold-o-normal--25-*-*-*-*-*-*-*" 
   It returns TRUE on success. */
gboolean   gtk_font_selection_dialog_set_font_name     (GtkFontSelectionDialog *fsd,
                                                        const gchar	       *fontname);

/* This returns the text in the preview entry. You should copy the returned
   text if you need it. */
G_CONST_RETURN gchar* 
          gtk_font_selection_dialog_get_preview_text   (GtkFontSelectionDialog *fsd);

/* This sets the text in the preview entry. It will be copied by the entry,
   so there's no need to g_strdup() it first. */
void	  gtk_font_selection_dialog_set_preview_text   (GtkFontSelectionDialog *fsd,
                                                        const gchar	       *text);


G_END_DECLS


#endif /* __GTK_FONTSEL_H__ */
