/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkFontSelection widget for Gtk+, by Damon Chaplin, May 1998.
 * Based on the GnomeFontSelector widget, by Elliot Lee, but major changes.
 * The GnomeFontSelector was derived from app/text_tool.c in the GIMP.
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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_FONTSEL_H__
#define __GTK_FONTSEL_H__


#include <gdk/gdk.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtknotebook.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GTK_TYPE_FONT_SELECTION		     (gtk_font_selection_get_type ())
#define GTK_FONT_SELECTION(obj)		     (GTK_CHECK_CAST ((obj), GTK_TYPE_FONT_SELECTION, GtkFontSelection))
#define GTK_FONT_SELECTION_CLASS(klass)	     (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_FONT_SELECTION, GtkFontSelectionClass))
#define GTK_IS_FONT_SELECTION(obj)	     (GTK_CHECK_TYPE ((obj), GTK_TYPE_FONT_SELECTION))
#define GTK_IS_FONT_SELECTION_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FONT_SELECTION))

#define GTK_TYPE_FONT_SELECTION_DIALOG		    (gtk_font_selection_dialog_get_type ())
#define GTK_FONT_SELECTION_DIALOG(obj)		    (GTK_CHECK_CAST ((obj), GTK_TYPE_FONT_SELECTION_DIALOG, GtkFontSelectionDialog))
#define GTK_FONT_SELECTION_DIALOG_CLASS(klass)	    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_FONT_SELECTION_DIALOG, GtkFontSelectionDialogClass))
#define GTK_IS_FONT_SELECTION_DIALOG(obj)	    (GTK_CHECK_TYPE ((obj), GTK_TYPE_FONT_SELECTION_DIALOG))
#define GTK_IS_FONT_SELECTION_DIALOG_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_FONT_SELECTION_DIALOG))

typedef struct _GtkFontSelection	     GtkFontSelection;
typedef struct _GtkFontSelectionClass	     GtkFontSelectionClass;

typedef struct _GtkFontSelectionDialog	     GtkFontSelectionDialog;
typedef struct _GtkFontSelectionDialogClass  GtkFontSelectionDialogClass;




/* This is the number of properties which we keep in the properties array,
   i.e. Weight, Slant, Set Width, Spacing, Charset & Foundry. */
#define GTK_NUM_FONT_PROPERTIES	 6

/* This is the number of properties each style has i.e. Weight, Slant,
   Set Width, Spacing & Charset. Note that Foundry is not included,
   since it is the same for all styles of the same FontInfo. */
#define GTK_NUM_STYLE_PROPERTIES 5


/* Used to determine whether we are using point or pixel sizes. */
typedef enum
{
  GTK_FONT_METRIC_PIXELS,
  GTK_FONT_METRIC_POINTS
} GtkFontMetricType;

/* Used for determining the type of a font style, and also for setting filters.
   These can be combined if a style has bitmaps and scalable fonts available.*/
typedef enum
{
  GTK_FONT_BITMAP		= 1 << 0,
  GTK_FONT_SCALABLE		= 1 << 1,
  GTK_FONT_SCALABLE_BITMAP	= 1 << 2,

  GTK_FONT_ALL			= 0x07
} GtkFontType;

/* These are the two types of filter available - base and user. The base
   filter is set by the application and can't be changed by the user. */
#define	GTK_NUM_FONT_FILTERS	2
typedef enum
{
  GTK_FONT_FILTER_BASE,
  GTK_FONT_FILTER_USER
} GtkFontFilterType;

/* These hold the arrays of current filter settings for each property.
   If nfilters is 0 then all values of the property are OK. If not the
   filters array contains the indexes of the valid property values. */
typedef struct _GtkFontFilter	GtkFontFilter;
struct _GtkFontFilter
{
  gint font_type;
  guint16 *property_filters[GTK_NUM_FONT_PROPERTIES];
  guint16 property_nfilters[GTK_NUM_FONT_PROPERTIES];
};


struct _GtkFontSelection
{
  GtkNotebook notebook;
  
  /* These are on the font page. */
  GtkWidget *main_vbox;
  GtkWidget *font_label;
  GtkWidget *font_entry;
  GtkWidget *font_clist;
  GtkWidget *font_style_entry;
  GtkWidget *font_style_clist;
  GtkWidget *size_entry;
  GtkWidget *size_clist;
  GtkWidget *pixels_button;
  GtkWidget *points_button;
  GtkWidget *filter_button;
  GtkWidget *preview_entry;
  GtkWidget *message_label;
  
  /* These are on the font info page. */
  GtkWidget *info_vbox;
  GtkWidget *info_clist;
  GtkWidget *requested_font_name;
  GtkWidget *actual_font_name;
  
  /* These are on the filter page. */
  GtkWidget *filter_vbox;
  GtkWidget *type_bitmaps_button;
  GtkWidget *type_scalable_button;
  GtkWidget *type_scaled_bitmaps_button;
  GtkWidget *filter_clists[GTK_NUM_FONT_PROPERTIES];
  
  GdkFont *font;
  gint font_index;
  gint style;
  GtkFontMetricType metric;
  /* The size is either in pixels or deci-points, depending on the metric. */
  gint size;
  
  /* This is the last size explicitly selected. When the user selects different
     fonts we try to find the nearest size to this. */
  gint selected_size;
  
  /* These are the current property settings. They are indexes into the
     strings in the GtkFontSelInfo properties array. */
  guint16 property_values[GTK_NUM_STYLE_PROPERTIES];
  
  /* These are the base and user font filters. */
  GtkFontFilter filters[GTK_NUM_FONT_FILTERS];
};


struct _GtkFontSelectionClass
{
  GtkNotebookClass parent_class;
};


struct _GtkFontSelectionDialog
{
  GtkWindow window;
  
  GtkWidget *fontsel;
  
  GtkWidget *main_vbox;
  GtkWidget *action_area;
  GtkWidget *ok_button;
  /* The 'Apply' button is not shown by default but you can show/hide it. */
  GtkWidget *apply_button;
  GtkWidget *cancel_button;
  
  /* If the user changes the width of the dialog, we turn auto-shrink off. */
  gint dialog_width;
  gboolean auto_resize;
};

struct _GtkFontSelectionDialogClass
{
  GtkWindowClass parent_class;
};



/*****************************************************************************
 * GtkFontSelection functions.
 *   see the comments in the GtkFontSelectionDialog functions.
 *****************************************************************************/

GtkType	   gtk_font_selection_get_type		(void);
GtkWidget* gtk_font_selection_new		(void);
gchar*	   gtk_font_selection_get_font_name	(GtkFontSelection *fontsel);
GdkFont*   gtk_font_selection_get_font		(GtkFontSelection *fontsel);
gboolean   gtk_font_selection_set_font_name	(GtkFontSelection *fontsel,
						 const gchar	  *fontname);
void	   gtk_font_selection_set_filter	(GtkFontSelection *fontsel,
						 GtkFontFilterType filter_type,
						 GtkFontType	   font_type,
						 gchar		 **foundries,
						 gchar		 **weights,
						 gchar		 **slants,
						 gchar		 **setwidths,
						 gchar		 **spacings,
						 gchar		 **charsets);
gchar*	   gtk_font_selection_get_preview_text	(GtkFontSelection *fontsel);
void	   gtk_font_selection_set_preview_text	(GtkFontSelection *fontsel,
						 const gchar	  *text);



/*****************************************************************************
 * GtkFontSelectionDialog functions.
 *   most of these functions simply call the corresponding function in the
 *   GtkFontSelection.
 *****************************************************************************/

GtkType	   gtk_font_selection_dialog_get_type	(void);
GtkWidget* gtk_font_selection_dialog_new	(const gchar	  *title);

/* This returns the X Logical Font Description fontname, or NULL if no font
   is selected. Note that there is a slight possibility that the font might not
   have been loaded OK. You should call gtk_font_selection_dialog_get_font()
   to see if it has been loaded OK.
   You should g_free() the returned font name after you're done with it. */
gchar*	 gtk_font_selection_dialog_get_font_name    (GtkFontSelectionDialog *fsd);

/* This will return the current GdkFont, or NULL if none is selected or there
   was a problem loading it. Remember to use gdk_font_ref/unref() if you want
   to use the font (in a style, for example). */
GdkFont* gtk_font_selection_dialog_get_font	    (GtkFontSelectionDialog *fsd);

/* This sets the currently displayed font. It should be a valid X Logical
   Font Description font name (anything else will be ignored), e.g.
   "-adobe-courier-bold-o-normal--25-*-*-*-*-*-*-*" 
   It returns TRUE on success. */
gboolean gtk_font_selection_dialog_set_font_name    (GtkFontSelectionDialog *fsd,
						     const gchar	*fontname);

/* This sets one of the font filters, to limit the fonts shown. The filter_type
   is GTK_FONT_FILTER_BASE or GTK_FONT_FILTER_USER. The font type is a
   combination of the bit flags GTK_FONT_BITMAP, GTK_FONT_SCALABLE and
   GTK_FONT_SCALABLE_BITMAP (or GTK_FONT_ALL for all font types).
   The foundries, weights etc. are arrays of strings containing property
   values, e.g. 'bold', 'demibold', and *MUST* finish with a NULL.
   Standard long names are also accepted, e.g. 'italic' instead of 'i'.

   e.g. to allow only fixed-width fonts ('char cell' or 'monospaced') to be
   selected use:

  gchar *spacings[] = { "c", "m", NULL };
  gtk_font_selection_dialog_set_filter (GTK_FONT_SELECTION_DIALOG (fontsel),
				       GTK_FONT_FILTER_BASE, GTK_FONT_ALL,
				       NULL, NULL, NULL, NULL, spacings, NULL);

  to allow only true scalable fonts to be selected use:

  gtk_font_selection_dialog_set_filter (GTK_FONT_SELECTION_DIALOG (fontsel),
				       GTK_FONT_FILTER_BASE, GTK_FONT_SCALABLE,
				       NULL, NULL, NULL, NULL, NULL, NULL);
*/
void	   gtk_font_selection_dialog_set_filter	(GtkFontSelectionDialog *fsd,
						 GtkFontFilterType filter_type,
						 GtkFontType	   font_type,
						 gchar		 **foundries,
						 gchar		 **weights,
						 gchar		 **slants,
						 gchar		 **setwidths,
						 gchar		 **spacings,
						 gchar		 **charsets);

/* This returns the text in the preview entry. You should copy the returned
   text if you need it. */
gchar*	 gtk_font_selection_dialog_get_preview_text (GtkFontSelectionDialog *fsd);

/* This sets the text in the preview entry. It will be copied by the entry,
   so there's no need to g_strdup() it first. */
void	 gtk_font_selection_dialog_set_preview_text (GtkFontSelectionDialog *fsd,
						     const gchar	    *text);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_FONTSEL_H__ */
