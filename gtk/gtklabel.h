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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_LABEL_H__
#define __GTK_LABEL_H__


#include <gdk/gdk.h>
#include <gtk/gtkmisc.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_LABEL		  (gtk_label_get_type ())
#define GTK_LABEL(obj)		  (GTK_CHECK_CAST ((obj), GTK_TYPE_LABEL, GtkLabel))
#define GTK_LABEL_CLASS(klass)	  (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_LABEL, GtkLabelClass))
#define GTK_IS_LABEL(obj)	  (GTK_CHECK_TYPE ((obj), GTK_TYPE_LABEL))
#define GTK_IS_LABEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_LABEL))


typedef struct _GtkLabel       GtkLabel;
typedef struct _GtkLabelClass  GtkLabelClass;

typedef struct _GtkLabelWord   GtkLabelWord;

struct _GtkLabel
{
  GtkMisc misc;

  gchar    *label;
  GdkWChar *label_wc;
  gchar    *pattern;

  GtkLabelWord *words;

  guint	  max_width : 16;
  guint   jtype : 2;
  gboolean wrap;
};

struct _GtkLabelClass
{
  GtkMiscClass parent_class;
};


GtkType    gtk_label_get_type      (void);
GtkWidget* gtk_label_new           (const gchar       *str);
void       gtk_label_set_text      (GtkLabel          *label,
                                    const gchar       *str);
void       gtk_label_set_justify   (GtkLabel          *label,
                                    GtkJustification   jtype);
void	   gtk_label_set_pattern   (GtkLabel	      *label,
				    const gchar	      *pattern);
void	   gtk_label_set_line_wrap (GtkLabel	      *label,
				    gboolean           wrap);
void       gtk_label_get           (GtkLabel          *label,
                                    gchar            **str);

/* Convenience function to set the name and pattern by parsing
 * a string with embedded underscores, and return the appropriate
 * key symbol for the accelerator.
 */
guint      gtk_label_parse_uline    (GtkLabel         *label,
				     const gchar      *string);

#ifndef	GTK_DISABLE_COMPAT_H
#  define gtk_label_set				gtk_label_set_text
#endif	/* GTK_DISABLE_COMPAT_H */

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_LABEL_H__ */
