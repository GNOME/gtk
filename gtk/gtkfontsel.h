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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_FONTSEL_H__
#define __GTK_FONTSEL_H__


#include <gdk/gdk.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtknotebook.h>


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define GTK_FONT_SELECTION(obj)		GTK_CHECK_CAST(obj, gtk_font_selection_get_type(), GtkFontSelection)
#define GTK_FONT_SELECTION_CLASS(klass) GTK_CHECK_CLASS_CAST(klass, gtk_font_selection_get_type(), GtkFontSelectionClass)
#define GTK_IS_FONT_SELECTION(obj)      GTK_CHECK_TYPE(obj, gtk_font_selection_get_type())

#define GTK_FONT_SELECTION_DIALOG(obj)          GTK_CHECK_CAST (obj, gtk_font_selection_dialog_get_type (), GtkFontSelectionDialog)
#define GTK_FONT_SELECTION_DIALOG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_font_selection_dialog_get_type (), GtkFontSelectionDialogClass)
#define GTK_IS_FONT_SELECTION_DIALOG(obj)       GTK_CHECK_TYPE (obj, gtk_font_selection_dialog_get_type ())

typedef struct _GtkFontSelection	     GtkFontSelection;
typedef struct _GtkFontSelectionClass	     GtkFontSelectionClass;

typedef struct _GtkFontSelectionDialog	     GtkFontSelectionDialog;
typedef struct _GtkFontSelectionDialogClass  GtkFontSelectionDialogClass;


typedef struct _FontInfo FontInfo;
typedef struct _FontStyle FontStyle;

/* This struct represents one family of fonts (with one foundry), e.g. adobe
   courier or sony fixed. It stores the family name, the index of the foundry
   name, and the index of and number of available styles. */
struct _FontInfo
{
  gchar   *family;
  guint16  foundry;
  gint	   style_index;
  guint16  nstyles;
};


/* This is the number of properties which we keep in the properties array,
   i.e. Foundry, Weight, Slant, Set Width & Spacing. */
#define GTK_NUM_FONT_PROPERTIES  6

/* This is the number of properties each style has i.e. Weight, Slant,
   Set Width & Spacing. Note that Foundry is not included. */
#define GTK_NUM_STYLE_PROPERTIES 5


/* Used for the flags field in FontStyle. Note that they can be combined,
   e.g. a style can have multiple bitmaps and a true scalable version.
   The displayed flag is used when displaying styles to remember which
   styles have already been displayed. */
enum
{
  BITMAP_FONT		= 1 << 0,
  SCALABLE_FONT		= 1 << 1,
  SCALABLE_BITMAP_FONT	= 1 << 2,
  DISPLAYED		= 1 << 3
};

/* This represents one style, as displayed in the Font Style clist. It can
   have a number of available pixel sizes and point sizes. The indexes point
   into the two big klass->pixel_sizes & klass->point_sizes arrays. */
struct _FontStyle
{
  guint16  properties[GTK_NUM_STYLE_PROPERTIES];
  gint	   pixel_sizes_index;
  guint16  npixel_sizes;
  gint	   point_sizes_index;
  guint16  npoint_sizes;
  guint8   flags;
};


/* Used to determine whether we are using point or pixel sizes. */
typedef enum
{
  PIXELS_METRIC,
  POINTS_METRIC
} GtkFontMetricType;


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
  GtkWidget *scaled_bitmaps_button;
  GtkWidget *preview_entry;
  GtkWidget *message_label;

  /* These are on the font info page. */
  GtkWidget *info_vbox;
  GtkWidget *info_clist;
  GtkWidget *requested_font_name;
  GtkWidget *actual_font_name;

  /* These are on the filter page. */
  GtkWidget *filter_vbox;
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

  /* This flag determines if scaled bitmapped fonts are acceptable. */
  gboolean scale_bitmapped_fonts;

  /* These are the current property settings. They are indexes into the
     strings in the class' properties array. */
  guint16 property_values[GTK_NUM_STYLE_PROPERTIES];

  /* These hold the arrays of current filter settings for each property.
     If nfilters is 0 then all values of the property are OK. If not the
     filters array contains the indexes of the valid property values. */
  guint16 *property_filters[GTK_NUM_FONT_PROPERTIES];
  guint16 property_nfilters[GTK_NUM_FONT_PROPERTIES];

  /* This flags is set to scroll the clist to the selected value as soon as
     it appears. There might be a better way of doing this. */
  gboolean scroll_on_expose;
};


struct _GtkFontSelectionClass
{
  GtkWindowClass parent_class;

  /* This is a table with each FontInfo representing one font family+foundry */
  FontInfo *font_info;
  gint nfonts;

  /* This stores all the valid combinations of properties for every family.
     Each FontInfo holds an index into its own space in this one big array. */
  FontStyle *font_styles;
  gint nstyles;

  /* This stores all the font sizes available for every style.
     Each style holds an index into these arrays. */
  guint16 *pixel_sizes;
  guint16 *point_sizes;

  /* These are the arrays of all possible weights/slants/set widths/spacings
     and the amount of space allocated for each array. The extra array is
     used for the foundries strings. */
  gchar **properties[GTK_NUM_FONT_PROPERTIES];
  guint16 nproperties[GTK_NUM_FONT_PROPERTIES];
  guint16 space_allocated[GTK_NUM_FONT_PROPERTIES];

  /* Whether any scalable bitmap fonts are available. */
  gboolean scaled_bitmaps_available;
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



/* FontSelection */

guint      gtk_font_selection_get_type		(void);
GtkWidget* gtk_font_selection_new		(void);

/* This returns the X Logical Font Description fontname, or NULL if no font
   is selected. Note that there is a slight possibility that the font might not
   have been loaded OK. You should call gtk_font_selection_get_font() to see
   if it has been loaded OK.
   You should g_free() the returned font name after you're done with it. */
gchar*	   gtk_font_selection_get_font_name	(GtkFontSelection *fontsel);

/* This will return the current GdkFont, or NULL if none is selected or there
   was a problem loading it. Remember to use gdk_font_ref/unref() if you want
   to use the font (in a style, for example). */
GdkFont*   gtk_font_selection_get_font		(GtkFontSelection *fontsel);

/* This sets the currently displayed font. It should be a valid X Logical
   Font Description font name (anything else will be ignored), e.g.
   "-adobe-courier-bold-o-normal--25-*-*-*-*-*-*-*" 
   It returns TRUE on success. */
gboolean   gtk_font_selection_set_font_name     (GtkFontSelection *fontsel,
						 const gchar	  *fontname);

/* This returns the text in the preview entry. You should copy the returned
   text if you need it. */
gchar*     gtk_font_selection_get_preview_text  (GtkFontSelection *fontsel);

/* This sets the text in the preview entry. It will be copied by the entry,
   so there's no need to g_strdup() it first. */
void       gtk_font_selection_set_preview_text  (GtkFontSelection *fontsel,
						 const gchar	  *text);



/* FontSelectionDialog */

guint      gtk_font_selection_dialog_get_type	(void);
GtkWidget* gtk_font_selection_dialog_new	(const gchar	  *title);

/* These simply call the corresponding FontSelection function. */
gchar*	   gtk_font_selection_dialog_get_font_name
						(GtkFontSelectionDialog *fsd);
GdkFont*   gtk_font_selection_dialog_get_font	(GtkFontSelectionDialog *fsd);
gboolean   gtk_font_selection_dialog_set_font_name
						(GtkFontSelectionDialog *fsd,
						 const gchar	  *fontname);
gchar*     gtk_font_selection_dialog_get_preview_text
						(GtkFontSelectionDialog *fsd);
void       gtk_font_selection_dialog_set_preview_text
						(GtkFontSelectionDialog *fsd,
						 const gchar	  *text);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_FONTSEL_H__ */
