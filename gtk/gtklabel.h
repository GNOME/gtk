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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __GTK_LABEL_H__
#define __GTK_LABEL_H__


#include <gdk/gdk.h>
#include <gtk/gtkmisc.h>


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define GTK_TYPE_LABEL			(gtk_label_get_type ())
#define GTK_LABEL(obj)			(GTK_CHECK_CAST ((obj), GTK_TYPE_LABEL, GtkLabel))
#define GTK_LABEL_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_LABEL, GtkLabelClass))
#define GTK_IS_LABEL(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_LABEL))
#define GTK_IS_LABEL_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_LABEL))


typedef struct _GtkLabel       GtkLabel;
typedef struct _GtkLabelClass  GtkLabelClass;

struct _GtkLabel
{
  GtkMisc misc;
  
  gchar  *label;
  gchar  *pattern;

  GSList *row;
  guint	  max_width : 16;
  guint   jtype : 2;
  guint	  needs_clear : 1;
};

struct _GtkLabelClass
{
  GtkMiscClass parent_class;
};


GtkType	   gtk_label_get_type	 (void);
GtkWidget* gtk_label_new	 (const gchar	    *string);
void	   gtk_label_set	 (GtkLabel	    *label,
				  const gchar	    *string);
void	   gtk_label_set_pattern (GtkLabel	    *label,
				  const gchar	    *pattern);
void	   gtk_label_set_justify (GtkLabel	    *label,
				  GtkJustification   jtype);
void	   gtk_label_get	 (GtkLabel	    *label,
				  gchar		   **string);


/* Convenience function to set the name and pattern by parsing
 * a string with embedded underscores, and return the appropriate
 * key symbol for the accelerator.
 */

guint      gtk_label_parse_uline    (GtkLabel         *label,
				     const gchar      *string);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_LABEL_H__ */
