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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/*
 * Limits:
 *
 *  Fontnames	 - A maximum of MAX_FONTS (32767) fontnames will be retrieved
 *		   from X Windows with XListFonts(). Any more are ignored.
 *		   I think this limit may have been set because of a limit in
 *		   GtkList. It could possibly be increased since we are using
 *		   GtkClists now, but I'd be surprised if it was reached.
 *  Field length - XLFD_MAX_FIELD_LEN is the maximum length that any field of a
 *		   fontname can be for it to be considered valid. Others are
 *		   ignored.
 *  Properties   - Maximum of 65535 choices for each font property - guint16's
 *		   are used as indices, e.g. in the FontInfo struct.
 *  Combinations - Maximum of 65535 combinations of properties for each font
 *		   family - a guint16 is used in the FontInfo struct.
 *  Font size    - Minimum font size of 2 pixels/points, since trying to load
 *		   some fonts with a size of 1 can cause X to hang.
 *		   (e.g. the Misc Fixed fonts).
 */

/*
 * Possible Improvements:
 *
 *  Font Styles  - could sort the styles into a reasonable order - regular
 *		   first, then bold, bold italic etc.
 *
 *  I18N	 - the default preview text is not useful for international
 *		   fonts. Maybe the first few characters of the font could be
 *		   displayed instead.
 *		 - fontsets? should these be handled by the font dialog?
 */

/*
 * Debugging: compile with -DFONTSEL_DEBUG for lots of debugging output.
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <X11/Xlib.h>

#include "gdk/gdkx.h"
#include "gdk/gdkkeysyms.h"

#include "gtkbutton.h"
#include "gtkcheckbutton.h"
#include "gtkclist.h"
#include "gtkentry.h"
#include "gtkfontsel.h"
#include "gtkframe.h"
#include "gtkhbbox.h"
#include "gtkhbox.h"
#include "gtklabel.h"
#include "gtknotebook.h"
#include "gtkradiobutton.h"
#include "gtksignal.h"
#include "gtktable.h"
#include "gtkvbox.h"
#include "gtkscrolledwindow.h"
#include "gtkintl.h"

/* The maximum number of fontnames requested with XListFonts(). */
#define MAX_FONTS 32767

/* This is the largest field length we will accept. If a fontname has a field
   larger than this we will skip it. */
#define XLFD_MAX_FIELD_LEN 64

/* These are what we use as the standard font sizes, for the size clist.
   Note that when using points we still show these integer point values but
   we work internally in decipoints (and decipoint values can be typed in). */
static const guint16 font_sizes[] = {
  8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 22, 24, 26, 28,
  32, 36, 40, 48, 56, 64, 72
};

/* Initial font metric & size (Remember point sizes are in decipoints).
   The font size should match one of those in the font_sizes array. */
#define INITIAL_METRIC		  GTK_FONT_METRIC_POINTS
#define INITIAL_FONT_SIZE	  140

/* This is the default text shown in the preview entry, though the user
   can set it. Remember that some fonts only have capital letters. */
#define PREVIEW_TEXT "abcdefghijk ABCDEFGHIJK"

/* This is the initial and maximum height of the preview entry (it expands
   when large font sizes are selected). Initial height is also the minimum. */
#define INITIAL_PREVIEW_HEIGHT 44
#define MAX_PREVIEW_HEIGHT 300

/* These are the sizes of the font, style & size clists. */
#define FONT_LIST_HEIGHT	136
#define FONT_LIST_WIDTH		190
#define FONT_STYLE_LIST_WIDTH	170
#define FONT_SIZE_LIST_WIDTH	60

/* This is the number of fields in an X Logical Font Description font name.
   Note that we count the registry & encoding as 1. */
#define GTK_XLFD_NUM_FIELDS 13

typedef struct _GtkFontSelInfo GtkFontSelInfo;
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

/* This represents one style, as displayed in the Font Style clist. It can
   have a number of available pixel sizes and point sizes. The indexes point
   into the two big fontsel_info->pixel_sizes & fontsel_info->point_sizes
   arrays. The displayed flag is used when displaying styles to remember which
   styles have already been displayed. Note that it is combined with the
   GtkFontType in the flags field. */
#define  GTK_FONT_DISPLAYED	(1 << 7)
struct _FontStyle
{
  guint16  properties[GTK_NUM_STYLE_PROPERTIES];
  gint	   pixel_sizes_index;
  guint16  npixel_sizes;
  gint	   point_sizes_index;
  guint16  npoint_sizes;
  guint8   flags;
};

struct _GtkFontSelInfo {
  
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
  
  /* These are the arrays of strings of all possible weights, slants, 
     set widths, spacings, charsets & foundries, and the amount of space
     allocated for each array. */
  gchar **properties[GTK_NUM_FONT_PROPERTIES];
  guint16 nproperties[GTK_NUM_FONT_PROPERTIES];
  guint16 space_allocated[GTK_NUM_FONT_PROPERTIES];
};

/* These are the field numbers in the X Logical Font Description fontnames,
   e.g. -adobe-courier-bold-o-normal--25-180-100-100-m-150-iso8859-1 */
typedef enum
{
  XLFD_FOUNDRY		= 0,
  XLFD_FAMILY		= 1,
  XLFD_WEIGHT		= 2,
  XLFD_SLANT		= 3,
  XLFD_SET_WIDTH	= 4,
  XLFD_ADD_STYLE	= 5,
  XLFD_PIXELS		= 6,
  XLFD_POINTS		= 7,
  XLFD_RESOLUTION_X	= 8,
  XLFD_RESOLUTION_Y	= 9,
  XLFD_SPACING		= 10,
  XLFD_AVERAGE_WIDTH	= 11,
  XLFD_CHARSET		= 12
} FontField;

/* These are the names of the fields, used on the info & filter page. */
static const gchar* xlfd_field_names[GTK_XLFD_NUM_FIELDS] = {
  N_("Foundry:"),
  N_("Family:"),
  N_("Weight:"),
  N_("Slant:"),
  N_("Set Width:"),
  N_("Add Style:"),
  N_("Pixel Size:"),
  N_("Point Size:"),
  N_("Resolution X:"),
  N_("Resolution Y:"),
  N_("Spacing:"),
  N_("Average Width:"),
  N_("Charset:"),
};

/* These are the array indices of the font properties used in several arrays,
   and should match the xlfd_index array below. */
typedef enum
{
  WEIGHT	= 0,
  SLANT		= 1,
  SET_WIDTH	= 2,
  SPACING	= 3,
  CHARSET	= 4,
  FOUNDRY	= 5
} PropertyIndexType;

/* This is used to look up a field in a fontname given one of the above
   property indices. */
static const FontField xlfd_index[GTK_NUM_FONT_PROPERTIES] = {
  XLFD_WEIGHT,
  XLFD_SLANT,
  XLFD_SET_WIDTH,
  XLFD_SPACING,
  XLFD_CHARSET,
  XLFD_FOUNDRY
};

/* These are the positions of the properties in the filter table - x, y. */
static const gint filter_positions[GTK_NUM_FONT_PROPERTIES][2] = {
  { 1, 0 }, { 0, 2 }, { 1, 2 }, { 2, 2 }, { 2, 0 }, { 0, 0 }
};
static const gint filter_heights[GTK_NUM_FONT_PROPERTIES] = {
  100, 70, 70, 40, 100, 100
};

/* This is returned by gtk_font_selection_filter_state to describe if a
   property value is filtered. e.g. if 'bold' has been selected on the filter
   page, then that will return 'FILTERED' and 'black' will be 'NOT_FILTERED'.
   If none of the weight values are selected, they all return 'NOT_SET'. */
typedef enum
{
  FILTERED,
  NOT_FILTERED,
  NOT_SET
} GtkFontPropertyFilterState;

static GtkFontSelInfo *fontsel_info = NULL;

/* The initial size and increment of each of the arrays of property values. */
#define PROPERTY_ARRAY_INCREMENT	16

static void    gtk_font_selection_class_init	     (GtkFontSelectionClass *klass);
static void    gtk_font_selection_init		     (GtkFontSelection *fontsel);
static void    gtk_font_selection_destroy	     (GtkObject      *object);

/* These are all used for class initialization - loading in the fonts etc. */
static void    gtk_font_selection_get_fonts          (void);
static void    gtk_font_selection_insert_font        (GSList         *fontnames[],
						      gint           *ntable,
						      gchar          *fontname);
static gint    gtk_font_selection_insert_field       (gchar          *fontname,
						      gint            prop);

/* These are the callbacks & related functions. */
static void    gtk_font_selection_select_font	     (GtkWidget      *w,
						      gint	      row,
						      gint	      column,
						      GdkEventButton *bevent,
						      gpointer        data);
static gint    gtk_font_selection_on_clist_key_press (GtkWidget      *clist,
						      GdkEventKey    *event,
						      GtkFontSelection *fs);
static gboolean gtk_font_selection_select_next	     (GtkFontSelection *fs,
						      GtkWidget        *clist,
						      gint		step);
static void    gtk_font_selection_show_available_styles
(GtkFontSelection *fs);
static void    gtk_font_selection_select_best_style  (GtkFontSelection *fs,
						      gboolean	       use_first);

static void    gtk_font_selection_select_style	     (GtkWidget      *w,
						      gint	      row,
						      gint	      column,
						      GdkEventButton *bevent,
						      gpointer        data);
static void    gtk_font_selection_show_available_sizes
(GtkFontSelection *fs);
static void     gtk_font_selection_size_activate  (GtkWidget *w,
						   gpointer   data);

static void    gtk_font_selection_select_best_size   (GtkFontSelection *fs);
static void    gtk_font_selection_select_size	     (GtkWidget      *w,
						      gint	      row,
						      gint	      column,
						      GdkEventButton *bevent,
						      gpointer        data);

static void    gtk_font_selection_metric_callback    (GtkWidget      *w,
						      gpointer        data);
static void    gtk_font_selection_expose_list	     (GtkWidget	     *w,
						      GdkEventExpose *event,
						      gpointer        data);
static void    gtk_font_selection_realize_list	     (GtkWidget	     *widget,
						      gpointer	      data);

static void    gtk_font_selection_switch_page	     (GtkWidget      *w,
						      GtkNotebookPage *page,
						      gint             page_num,
						      gpointer         data);
static void    gtk_font_selection_show_font_info     (GtkFontSelection *fs);

static void    gtk_font_selection_select_filter	     (GtkWidget      *w,
						      gint	      row,
						      gint	      column,
						      GdkEventButton *bevent,
						      GtkFontSelection *fs);
static void    gtk_font_selection_unselect_filter    (GtkWidget      *w,
						      gint	      row,
						      gint	      column,
						      GdkEventButton *bevent,
						      GtkFontSelection *fs);
static void    gtk_font_selection_update_filter	     (GtkFontSelection *fs);
static gboolean gtk_font_selection_style_visible     (GtkFontSelection *fs,
						      FontInfo       *font,
						      gint	      style);
static void    gtk_font_selection_reset_filter	     (GtkWidget      *w,
						      GtkFontSelection *fs);
static void    gtk_font_selection_on_clear_filter    (GtkWidget      *w,
						      GtkFontSelection *fs);
static void    gtk_font_selection_show_available_fonts
						     (GtkFontSelection *fs);
static void    gtk_font_selection_clear_filter       (GtkFontSelection *fs);
static void    gtk_font_selection_update_filter_lists(GtkFontSelection *fs);
static GtkFontPropertyFilterState gtk_font_selection_filter_state
						     (GtkFontSelection *fs,
						      GtkFontFilterType filter_type,
						      gint		property,
						      gint		index);

/* Misc. utility functions. */
static gboolean gtk_font_selection_load_font         (GtkFontSelection *fs);
static void    gtk_font_selection_update_preview     (GtkFontSelection *fs);

static gint    gtk_font_selection_find_font	     (GtkFontSelection *fs,
						      gchar          *family,
						      guint16	      foundry);
static guint16 gtk_font_selection_field_to_index     (gchar         **table,
						      gint            ntable,
						      gchar          *field);

static gchar*  gtk_font_selection_expand_slant_code  (gchar          *slant);
static gchar*  gtk_font_selection_expand_spacing_code(gchar          *spacing);

/* Functions for handling X Logical Font Description fontnames. */
static gboolean gtk_font_selection_is_xlfd_font_name (const gchar    *fontname);
static char*   gtk_font_selection_get_xlfd_field     (const gchar    *fontname,
						      FontField       field_num,
						      gchar          *buffer);
static gchar * gtk_font_selection_create_xlfd        (gint            size,
						      GtkFontMetricType metric,
						      gchar          *foundry,
						      gchar          *family,
						      gchar          *weight,
						      gchar          *slant,
						      gchar          *set_width,
						      gchar          *spacing,
						      gchar	     *charset);


/* FontSelectionDialog */
static void    gtk_font_selection_dialog_class_init  (GtkFontSelectionDialogClass *klass);
static void    gtk_font_selection_dialog_init	     (GtkFontSelectionDialog *fontseldiag);

static gint    gtk_font_selection_dialog_on_configure(GtkWidget      *widget,
						      GdkEventConfigure *event,
						      GtkFontSelectionDialog *fsd);

static GtkWindowClass *font_selection_parent_class = NULL;
static GtkNotebookClass *font_selection_dialog_parent_class = NULL;

GtkType
gtk_font_selection_get_type()
{
  static GtkType font_selection_type = 0;
  
  if(!font_selection_type)
    {
      static const GtkTypeInfo fontsel_type_info =
      {
	"GtkFontSelection",
	sizeof (GtkFontSelection),
	sizeof (GtkFontSelectionClass),
	(GtkClassInitFunc) gtk_font_selection_class_init,
	(GtkObjectInitFunc) gtk_font_selection_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };
      
      font_selection_type = gtk_type_unique (GTK_TYPE_NOTEBOOK,
					     &fontsel_type_info);
    }
  
  return font_selection_type;
}

static void
gtk_font_selection_class_init(GtkFontSelectionClass *klass)
{
  GtkObjectClass *object_class;
  
  object_class = (GtkObjectClass *) klass;
  
  font_selection_parent_class = gtk_type_class (GTK_TYPE_NOTEBOOK);
  
  object_class->destroy = gtk_font_selection_destroy;
  
  gtk_font_selection_get_fonts ();
}

static void
gtk_font_selection_init(GtkFontSelection *fontsel)
{
  GtkWidget *scrolled_win;
  GtkWidget *text_frame;
  GtkWidget *text_box, *frame;
  GtkWidget *table, *label, *hbox, *hbox2, *clist, *button, *vbox, *alignment;
  gint i, prop, row;
  gchar *titles[] = { NULL, NULL, NULL };
  gchar buffer[128];
  gchar *size;
  gint size_to_match;
  gchar *row_text[3];
  gchar *property, *text;
  gboolean inserted;
  
  /* Number of internationalized titles here must match number
     of NULL initializers above */
  titles[0] = _("Font Property");
  titles[1] = _("Requested Value");
  titles[2] = _("Actual Value");

  /* Initialize the GtkFontSelection struct. We do this here in case any
     callbacks are triggered while creating the interface. */
  fontsel->font = NULL;
  fontsel->font_index = -1;
  fontsel->style = -1;
  fontsel->metric = INITIAL_METRIC;
  fontsel->size = INITIAL_FONT_SIZE;
  fontsel->selected_size = INITIAL_FONT_SIZE;

  fontsel->filters[GTK_FONT_FILTER_BASE].font_type = GTK_FONT_ALL;
  fontsel->filters[GTK_FONT_FILTER_USER].font_type = GTK_FONT_BITMAP
    | GTK_FONT_SCALABLE;

  
  for (prop = 0; prop < GTK_NUM_FONT_PROPERTIES; prop++)
    {
      fontsel->filters[GTK_FONT_FILTER_BASE].property_filters[prop] = NULL;
      fontsel->filters[GTK_FONT_FILTER_BASE].property_nfilters[prop] = 0;
      fontsel->filters[GTK_FONT_FILTER_USER].property_filters[prop] = NULL;
      fontsel->filters[GTK_FONT_FILTER_USER].property_nfilters[prop] = 0;
    }
  
  for (prop = 0; prop < GTK_NUM_STYLE_PROPERTIES; prop++)
    fontsel->property_values[prop] = 0;
  
  /* Create the main notebook page. */
  gtk_notebook_set_homogeneous_tabs (GTK_NOTEBOOK (fontsel), TRUE);
  gtk_notebook_set_tab_hborder (GTK_NOTEBOOK (fontsel), 8);
  fontsel->main_vbox = gtk_vbox_new (FALSE, 4);
  gtk_widget_show (fontsel->main_vbox);
  gtk_container_set_border_width (GTK_CONTAINER (fontsel->main_vbox), 6);
  label = gtk_label_new(_("Font"));
  gtk_notebook_append_page (GTK_NOTEBOOK (fontsel),
			    fontsel->main_vbox, label);
  
  /* Create the table of font, style & size. */
  table = gtk_table_new (3, 3, FALSE);
  gtk_widget_show (table);
  gtk_table_set_col_spacings(GTK_TABLE(table), 8);
  gtk_box_pack_start (GTK_BOX (fontsel->main_vbox), table, TRUE, TRUE, 0);
  
  fontsel->font_label = gtk_label_new(_("Font:"));
  gtk_misc_set_alignment (GTK_MISC (fontsel->font_label), 0.0, 0.5);
  gtk_widget_show (fontsel->font_label);
  gtk_table_attach (GTK_TABLE (table), fontsel->font_label, 0, 1, 0, 1,
		    GTK_FILL, 0, 0, 0);
  label = gtk_label_new(_("Font Style:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, 0, 1,
		    GTK_FILL, 0, 0, 0);
  label = gtk_label_new(_("Size:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 2, 3, 0, 1,
		    GTK_FILL, 0, 0, 0);
  
  fontsel->font_entry = gtk_entry_new();
  gtk_entry_set_editable(GTK_ENTRY(fontsel->font_entry), FALSE);
  gtk_widget_set_usize (fontsel->font_entry, 20, -1);
  gtk_widget_show (fontsel->font_entry);
  gtk_table_attach (GTK_TABLE (table), fontsel->font_entry, 0, 1, 1, 2,
		    GTK_FILL, 0, 0, 0);
  fontsel->font_style_entry = gtk_entry_new();
  gtk_entry_set_editable(GTK_ENTRY(fontsel->font_style_entry), FALSE);
  gtk_widget_set_usize (fontsel->font_style_entry, 20, -1);
  gtk_widget_show (fontsel->font_style_entry);
  gtk_table_attach (GTK_TABLE (table), fontsel->font_style_entry, 1, 2, 1, 2,
		    GTK_FILL, 0, 0, 0);
  fontsel->size_entry = gtk_entry_new();
  gtk_widget_set_usize (fontsel->size_entry, 20, -1);
  gtk_widget_show (fontsel->size_entry);
  gtk_table_attach (GTK_TABLE (table), fontsel->size_entry, 2, 3, 1, 2,
		    GTK_FILL, 0, 0, 0);
  gtk_signal_connect (GTK_OBJECT (fontsel->size_entry), "activate",
		      GTK_SIGNAL_FUNC (gtk_font_selection_size_activate),
		      fontsel);
  
  /* Create the clists  */
  fontsel->font_clist = gtk_clist_new(1);
  gtk_clist_column_titles_hide (GTK_CLIST(fontsel->font_clist));
  gtk_clist_set_column_auto_resize (GTK_CLIST (fontsel->font_clist), 0, TRUE);
  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_usize (scrolled_win, FONT_LIST_WIDTH, FONT_LIST_HEIGHT);
  gtk_container_add (GTK_CONTAINER (scrolled_win), fontsel->font_clist);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_widget_show(fontsel->font_clist);
  gtk_widget_show(scrolled_win);

  gtk_table_attach (GTK_TABLE (table), scrolled_win, 0, 1, 2, 3,
		    GTK_EXPAND | GTK_FILL,
		    GTK_EXPAND | GTK_FILL, 0, 0);
  
  fontsel->font_style_clist = gtk_clist_new(1);
  gtk_clist_column_titles_hide (GTK_CLIST(fontsel->font_style_clist));
  gtk_clist_set_column_auto_resize (GTK_CLIST (fontsel->font_style_clist),
				    0, TRUE);
  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_usize (scrolled_win, FONT_STYLE_LIST_WIDTH, -1);
  gtk_container_add (GTK_CONTAINER (scrolled_win), fontsel->font_style_clist);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_widget_show(fontsel->font_style_clist);
  gtk_widget_show(scrolled_win);
  gtk_table_attach (GTK_TABLE (table), scrolled_win, 1, 2, 2, 3,
		    GTK_EXPAND | GTK_FILL,
		    GTK_EXPAND | GTK_FILL, 0, 0);
  
  fontsel->size_clist = gtk_clist_new(1);
  gtk_clist_column_titles_hide (GTK_CLIST(fontsel->size_clist));
  gtk_clist_set_column_width (GTK_CLIST(fontsel->size_clist), 0, 20);
  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_usize (scrolled_win, FONT_SIZE_LIST_WIDTH, -1);
  gtk_container_add (GTK_CONTAINER (scrolled_win), fontsel->size_clist);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_widget_show(fontsel->size_clist);
  gtk_widget_show(scrolled_win);
  gtk_table_attach (GTK_TABLE (table), scrolled_win, 2, 3, 2, 3,
		    GTK_FILL, GTK_FILL, 0, 0);
  
  
  /* Insert the fonts. If there exist fonts with the same family but
     different foundries, then the foundry name is appended in brackets. */
  gtk_font_selection_show_available_fonts(fontsel);
  
  gtk_signal_connect (GTK_OBJECT (fontsel->font_clist), "select_row",
		      GTK_SIGNAL_FUNC(gtk_font_selection_select_font),
		      fontsel);
  GTK_WIDGET_SET_FLAGS (fontsel->font_clist, GTK_CAN_FOCUS);
  gtk_signal_connect (GTK_OBJECT (fontsel->font_clist), "key_press_event",
		      GTK_SIGNAL_FUNC(gtk_font_selection_on_clist_key_press),
		      fontsel);
  gtk_signal_connect_after (GTK_OBJECT (fontsel->font_clist), "expose_event",
			    GTK_SIGNAL_FUNC(gtk_font_selection_expose_list),
			    fontsel);
  
  gtk_signal_connect (GTK_OBJECT (fontsel->font_style_clist), "select_row",
		      GTK_SIGNAL_FUNC(gtk_font_selection_select_style),
		      fontsel);
  GTK_WIDGET_SET_FLAGS (fontsel->font_style_clist, GTK_CAN_FOCUS);
  gtk_signal_connect (GTK_OBJECT (fontsel->font_style_clist),
		      "key_press_event",
		      GTK_SIGNAL_FUNC(gtk_font_selection_on_clist_key_press),
		      fontsel);
  gtk_signal_connect_after (GTK_OBJECT (fontsel->font_style_clist),
			    "realize",
			    GTK_SIGNAL_FUNC(gtk_font_selection_realize_list),
			    fontsel);
  
  /* Insert the standard font sizes */
  gtk_clist_freeze (GTK_CLIST(fontsel->size_clist));
  size_to_match = INITIAL_FONT_SIZE;
  if (INITIAL_METRIC == GTK_FONT_METRIC_POINTS)
    size_to_match = size_to_match / 10;
  for (i = 0; i < sizeof(font_sizes) / sizeof(font_sizes[0]); i++)
    {
      sprintf(buffer, "%i", font_sizes[i]);
      size = buffer;
      gtk_clist_append(GTK_CLIST(fontsel->size_clist), &size);
      if (font_sizes[i] == size_to_match)
	{
	  gtk_clist_select_row(GTK_CLIST(fontsel->size_clist), i, 0);
	  gtk_entry_set_text(GTK_ENTRY(fontsel->size_entry), buffer);
	}
    }
  gtk_clist_thaw (GTK_CLIST(fontsel->size_clist));
  
  gtk_signal_connect (GTK_OBJECT (fontsel->size_clist), "select_row",
		      GTK_SIGNAL_FUNC(gtk_font_selection_select_size),
		      fontsel);
  GTK_WIDGET_SET_FLAGS (fontsel->size_clist, GTK_CAN_FOCUS);
  gtk_signal_connect (GTK_OBJECT (fontsel->size_clist), "key_press_event",
		      GTK_SIGNAL_FUNC(gtk_font_selection_on_clist_key_press),
		      fontsel);
  
  
  /* create the Reset Filter & Metric buttons */
  hbox = gtk_hbox_new(FALSE, 8);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (fontsel->main_vbox), hbox, FALSE, TRUE, 0);
  
  fontsel->filter_button = gtk_button_new_with_label(_("Reset Filter"));
  gtk_misc_set_padding (GTK_MISC (GTK_BIN (fontsel->filter_button)->child),
			16, 0);
  gtk_widget_show(fontsel->filter_button);
  gtk_box_pack_start (GTK_BOX (hbox), fontsel->filter_button, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (fontsel->filter_button, FALSE);
  gtk_signal_connect (GTK_OBJECT (fontsel->filter_button), "clicked",
		      GTK_SIGNAL_FUNC(gtk_font_selection_on_clear_filter),
		      fontsel);
  
  hbox2 = gtk_hbox_new(FALSE, 0);
  gtk_widget_show (hbox2);
  gtk_box_pack_end (GTK_BOX (hbox), hbox2, FALSE, FALSE, 0);
  
  label = gtk_label_new(_("Metric:"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, TRUE, 8);
  
  fontsel->points_button = gtk_radio_button_new_with_label(NULL, _("Points"));
  gtk_widget_show (fontsel->points_button);
  gtk_box_pack_start (GTK_BOX (hbox2), fontsel->points_button, FALSE, TRUE, 0);
  if (INITIAL_METRIC == GTK_FONT_METRIC_POINTS)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fontsel->points_button),
				TRUE);
  
  fontsel->pixels_button = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(fontsel->points_button), _("Pixels"));
  gtk_widget_show (fontsel->pixels_button);
  gtk_box_pack_start (GTK_BOX (hbox2), fontsel->pixels_button, FALSE, TRUE, 0);
  if (INITIAL_METRIC == GTK_FONT_METRIC_PIXELS)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fontsel->pixels_button),
				TRUE);
  
  gtk_signal_connect(GTK_OBJECT(fontsel->points_button), "toggled",
		     (GtkSignalFunc) gtk_font_selection_metric_callback,
		     fontsel);
  gtk_signal_connect(GTK_OBJECT(fontsel->pixels_button), "toggled",
		     (GtkSignalFunc) gtk_font_selection_metric_callback,
		     fontsel);
  
  
  /* create the text entry widget */
  text_frame = gtk_frame_new (_("Preview:"));
  gtk_widget_show (text_frame);
  gtk_frame_set_shadow_type(GTK_FRAME(text_frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (fontsel->main_vbox), text_frame,
		      FALSE, TRUE, 0);
  
  /* This is just used to get a 4-pixel space around the preview entry. */
  text_box = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (text_box);
  gtk_container_add (GTK_CONTAINER (text_frame), text_box);
  gtk_container_set_border_width (GTK_CONTAINER (text_box), 4);
  
  fontsel->preview_entry = gtk_entry_new ();
  gtk_widget_show (fontsel->preview_entry);
  gtk_widget_set_usize (fontsel->preview_entry, -1, INITIAL_PREVIEW_HEIGHT);
  gtk_box_pack_start (GTK_BOX (text_box), fontsel->preview_entry,
		      TRUE, TRUE, 0);
  
  /* Create the message area */
  fontsel->message_label = gtk_label_new("");
  gtk_widget_show (fontsel->message_label);
  gtk_box_pack_start (GTK_BOX (fontsel->main_vbox), fontsel->message_label, 
		      FALSE, FALSE, 0);
  
  
  /* Create the font info page */
  fontsel->info_vbox = gtk_vbox_new (FALSE, 4);
  gtk_widget_show (fontsel->info_vbox);
  gtk_container_set_border_width (GTK_CONTAINER (fontsel->info_vbox), 2);
  label = gtk_label_new(_("Font Information"));
  gtk_notebook_append_page (GTK_NOTEBOOK (fontsel),
			    fontsel->info_vbox, label);
  
  fontsel->info_clist = gtk_clist_new_with_titles (3, titles);
  gtk_widget_set_usize (fontsel->info_clist, 390, 150);
  gtk_clist_set_column_width(GTK_CLIST(fontsel->info_clist), 0, 130);
  gtk_clist_set_column_width(GTK_CLIST(fontsel->info_clist), 1, 130);
  gtk_clist_set_column_width(GTK_CLIST(fontsel->info_clist), 2, 130);
  gtk_clist_column_titles_passive(GTK_CLIST(fontsel->info_clist));
  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_add (GTK_CONTAINER (scrolled_win), fontsel->info_clist);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_widget_show(fontsel->info_clist);
  gtk_widget_show(scrolled_win);
  gtk_box_pack_start (GTK_BOX (fontsel->info_vbox), scrolled_win,
		      TRUE, TRUE, 0);
  
  /* Insert the property names */
  gtk_clist_freeze (GTK_CLIST(fontsel->info_clist));
  row_text[1] = "";
  row_text[2] = "";
  for (i = 0; i < GTK_XLFD_NUM_FIELDS; i++)
    {
      row_text[0] = _(xlfd_field_names[i]);
      gtk_clist_append(GTK_CLIST(fontsel->info_clist), row_text);
      gtk_clist_set_shift(GTK_CLIST(fontsel->info_clist), i, 0, 0, 4);
      gtk_clist_set_shift(GTK_CLIST(fontsel->info_clist), i, 1, 0, 4);
      gtk_clist_set_shift(GTK_CLIST(fontsel->info_clist), i, 2, 0, 4);
    }
  gtk_clist_thaw (GTK_CLIST(fontsel->info_clist));
  
  label = gtk_label_new(_("Requested Font Name:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (fontsel->info_vbox), label, FALSE, TRUE, 0);
  
  fontsel->requested_font_name = gtk_entry_new();
  gtk_entry_set_editable(GTK_ENTRY(fontsel->requested_font_name), FALSE);
  gtk_widget_show (fontsel->requested_font_name);
  gtk_box_pack_start (GTK_BOX (fontsel->info_vbox),
		      fontsel->requested_font_name, FALSE, TRUE, 0);
  
  label = gtk_label_new(_("Actual Font Name:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (fontsel->info_vbox), label, FALSE, TRUE, 0);
  
  fontsel->actual_font_name = gtk_entry_new();
  gtk_entry_set_editable(GTK_ENTRY(fontsel->actual_font_name), FALSE);
  gtk_widget_show (fontsel->actual_font_name);
  gtk_box_pack_start (GTK_BOX (fontsel->info_vbox),
		      fontsel->actual_font_name, FALSE, TRUE, 0);
  
  sprintf(buffer, _("%i fonts available with a total of %i styles."),
	  fontsel_info->nfonts, fontsel_info->nstyles);
  label = gtk_label_new(buffer);
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (fontsel->info_vbox), label, FALSE, FALSE, 0);
  
  gtk_signal_connect (GTK_OBJECT (fontsel), "switch_page",
		      GTK_SIGNAL_FUNC(gtk_font_selection_switch_page),
		      fontsel);
  
  
  /* Create the Filter page. */
  fontsel->filter_vbox = gtk_vbox_new (FALSE, 4);
  gtk_widget_show (fontsel->filter_vbox);
  gtk_container_set_border_width (GTK_CONTAINER (fontsel->filter_vbox), 2);
  label = gtk_label_new(_("Filter"));
  gtk_notebook_append_page (GTK_NOTEBOOK (fontsel),
			    fontsel->filter_vbox, label);
  
  /* Create the font type checkbuttons. */
  frame = gtk_frame_new (NULL);
  gtk_widget_show (frame);
  gtk_box_pack_start (GTK_BOX (fontsel->filter_vbox), frame, FALSE, TRUE, 0);

  hbox = gtk_hbox_new (FALSE, 20);
  gtk_widget_show (hbox);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  label = gtk_label_new(_("Font Types:"));
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 10);

  hbox2 = gtk_hbox_new (TRUE, 0);
  gtk_widget_show (hbox2);
  gtk_box_pack_start (GTK_BOX (hbox), hbox2, FALSE, TRUE, 0);

  fontsel->type_bitmaps_button = gtk_check_button_new_with_label (_("Bitmap"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fontsel->type_bitmaps_button), TRUE);
  gtk_widget_show (fontsel->type_bitmaps_button);
  gtk_box_pack_start (GTK_BOX (hbox2), fontsel->type_bitmaps_button,
		      FALSE, TRUE, 0);

  fontsel->type_scalable_button = gtk_check_button_new_with_label (_("Scalable"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fontsel->type_scalable_button), TRUE);
  gtk_widget_show (fontsel->type_scalable_button);
  gtk_box_pack_start (GTK_BOX (hbox2), fontsel->type_scalable_button,
		      FALSE, TRUE, 0);

  fontsel->type_scaled_bitmaps_button = gtk_check_button_new_with_label (_("Scaled Bitmap"));
  gtk_widget_show (fontsel->type_scaled_bitmaps_button);
  gtk_box_pack_start (GTK_BOX (hbox2), fontsel->type_scaled_bitmaps_button,
		      FALSE, TRUE, 0);

  table = gtk_table_new (4, 3, FALSE);
  gtk_table_set_col_spacings(GTK_TABLE(table), 2);
  gtk_widget_show (table);
  gtk_box_pack_start (GTK_BOX (fontsel->filter_vbox), table, TRUE, TRUE, 0);
  
  for (prop = 0; prop < GTK_NUM_FONT_PROPERTIES; prop++)
    {
      gint left = filter_positions[prop][0];
      gint top = filter_positions[prop][1];
      
      label = gtk_label_new(_(xlfd_field_names[xlfd_index[prop]]));
      gtk_misc_set_alignment (GTK_MISC (label), 0.0, 1.0);
      gtk_misc_set_padding (GTK_MISC (label), 0, 2);
      gtk_widget_show(label);
      gtk_table_attach (GTK_TABLE (table), label, left, left + 1,
			top, top + 1, GTK_FILL, GTK_FILL, 0, 0);
      
      clist = gtk_clist_new(1);
      gtk_widget_set_usize (clist, 100, filter_heights[prop]);
      gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_MULTIPLE);
      gtk_clist_column_titles_hide(GTK_CLIST(clist));
      gtk_clist_set_column_auto_resize (GTK_CLIST (clist), 0, TRUE);
      scrolled_win = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_add (GTK_CONTAINER (scrolled_win), clist);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				      GTK_POLICY_AUTOMATIC,
				      GTK_POLICY_AUTOMATIC);
      gtk_widget_show(clist);
      gtk_widget_show(scrolled_win);
      
      /* For the bottom-right cell we add the 'Reset Filter' button. */
      if (top == 2 && left == 2)
	{
	  vbox = gtk_vbox_new(FALSE, 0);
	  gtk_widget_show(vbox);
	  gtk_table_attach (GTK_TABLE (table), vbox, left, left + 1,
			    top + 1, top + 2, GTK_FILL, GTK_FILL, 0, 0);
	  
	  gtk_box_pack_start (GTK_BOX (vbox), scrolled_win, TRUE, TRUE, 0);
	  
	  alignment = gtk_alignment_new(0.5, 0.0, 0.8, 0.0);
	  gtk_widget_show(alignment);
	  gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, TRUE, 4);
	  
	  button = gtk_button_new_with_label(_("Reset Filter"));
	  gtk_widget_show(button);
	  gtk_container_add(GTK_CONTAINER(alignment), button);
	  gtk_signal_connect (GTK_OBJECT (button), "clicked",
			      GTK_SIGNAL_FUNC(gtk_font_selection_reset_filter),
			      fontsel);
	}
      else
	gtk_table_attach (GTK_TABLE (table), scrolled_win,
			  left, left + 1, top + 1, top + 2,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      
      gtk_signal_connect (GTK_OBJECT (clist), "select_row",
			  GTK_SIGNAL_FUNC(gtk_font_selection_select_filter),
			  fontsel);
      gtk_signal_connect (GTK_OBJECT (clist), "unselect_row",
			  GTK_SIGNAL_FUNC(gtk_font_selection_unselect_filter),
			  fontsel);
      
      /* Insert the property names, expanded, and in sorted order.
	 But we make sure that the wildcard '*' is first. */
      gtk_clist_freeze (GTK_CLIST(clist));
      property = N_("*");
      gtk_clist_append(GTK_CLIST(clist), &property);
      
      for (i = 1; i < fontsel_info->nproperties[prop]; i++) {
	property = _(fontsel_info->properties[prop][i]);
	if (prop == SLANT)
	  property = gtk_font_selection_expand_slant_code(property);
	else if (prop == SPACING)
	  property = gtk_font_selection_expand_spacing_code(property);
	
	inserted = FALSE;
	for (row = 1; row < GTK_CLIST(clist)->rows; row++)
	  {
	    gtk_clist_get_text(GTK_CLIST(clist), row, 0, &text);
	    if (strcmp(property, text) < 0)
	      {
		inserted = TRUE;
		gtk_clist_insert(GTK_CLIST(clist), row, &property);
		break;
	      }
	  }
	if (!inserted)
	  row = gtk_clist_append(GTK_CLIST(clist), &property);
	gtk_clist_set_row_data(GTK_CLIST(clist), row, GINT_TO_POINTER (i));
      }
      gtk_clist_select_row(GTK_CLIST(clist), 0, 0);
      gtk_clist_thaw (GTK_CLIST(clist));
      fontsel->filter_clists[prop] = clist;
    }
}

GtkWidget *
gtk_font_selection_new()
{
  GtkFontSelection *fontsel;
  
  fontsel = gtk_type_new (GTK_TYPE_FONT_SELECTION);
  
  return GTK_WIDGET (fontsel);
}

static void
gtk_font_selection_destroy (GtkObject *object)
{
  GtkFontSelection *fontsel;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_FONT_SELECTION (object));
  
  fontsel = GTK_FONT_SELECTION (object);
  
  /* All we have to do is unref the font, if we have one. */
  if (fontsel->font)
    gdk_font_unref (fontsel->font);
  
  if (GTK_OBJECT_CLASS (font_selection_parent_class)->destroy)
    (* GTK_OBJECT_CLASS (font_selection_parent_class)->destroy) (object);
}


/* This is called when the clist is exposed. Here we scroll to the current
   font if necessary. */
static void
gtk_font_selection_expose_list (GtkWidget		*widget,
				GdkEventExpose		*event,
				gpointer		 data)
{
  GtkFontSelection *fontsel;
  FontInfo *font_info;
  GList *selection;
  gint index;
  
#ifdef FONTSEL_DEBUG
  g_message("In expose_list\n");
#endif
  fontsel = GTK_FONT_SELECTION(data);
  
  font_info = fontsel_info->font_info;
      
  /* Try to scroll the font family clist to the selected item */
  selection = GTK_CLIST(fontsel->font_clist)->selection;
  if (selection)
    {
      index = GPOINTER_TO_INT (selection->data);
      if (gtk_clist_row_is_visible(GTK_CLIST(fontsel->font_clist), index)
	  != GTK_VISIBILITY_FULL)
	gtk_clist_moveto(GTK_CLIST(fontsel->font_clist), index, -1, 0.5, 0);
    }
      
  /* Try to scroll the font style clist to the selected item */
  selection = GTK_CLIST(fontsel->font_style_clist)->selection;
  if (selection)
    {
      index = GPOINTER_TO_INT (selection->data);
      if (gtk_clist_row_is_visible(GTK_CLIST(fontsel->font_style_clist), index)
	  != GTK_VISIBILITY_FULL)
	gtk_clist_moveto(GTK_CLIST(fontsel->font_style_clist), index, -1,
			 0.5, 0);
    }
      
  /* Try to scroll the font size clist to the selected item */
  selection = GTK_CLIST(fontsel->size_clist)->selection;
  if (selection)
    {
      index = GPOINTER_TO_INT (selection->data);
      if (gtk_clist_row_is_visible(GTK_CLIST(fontsel->size_clist), index)
	  != GTK_VISIBILITY_FULL)
      gtk_clist_moveto(GTK_CLIST(fontsel->size_clist), index, -1, 0.5, 0);
    }
}


/* This is called when the style clist is realized. We need to set any
   charset rows to insensitive colours. */
static void
gtk_font_selection_realize_list (GtkWidget		*widget,
				 gpointer		 data)
{
  GtkFontSelection *fontsel;
  gint row;
  GdkColor *inactive_fg, *inactive_bg;

#ifdef FONTSEL_DEBUG
  g_message("In realize_list\n");
#endif
  fontsel = GTK_FONT_SELECTION (data);

  /* Set the colours for any charset rows to insensitive. */
  inactive_fg = &fontsel->font_style_clist->style->fg[GTK_STATE_INSENSITIVE];
  inactive_bg = &fontsel->font_style_clist->style->bg[GTK_STATE_INSENSITIVE];

  for (row = 0; row < GTK_CLIST (fontsel->font_style_clist)->rows; row++)
    {
      if (GPOINTER_TO_INT (gtk_clist_get_row_data (GTK_CLIST (fontsel->font_style_clist), row)) == -1)
	{
	  gtk_clist_set_foreground (GTK_CLIST (fontsel->font_style_clist),
				    row, inactive_fg);
	  gtk_clist_set_background (GTK_CLIST (fontsel->font_style_clist),
				    row, inactive_bg);
	}
    }
}


/* This is called when a family is selected in the list. */
static void
gtk_font_selection_select_font (GtkWidget      *w,
				gint	        row,
				gint	        column,
				GdkEventButton *bevent,
				gpointer        data)
{
  GtkFontSelection *fontsel;
  FontInfo *font_info;
  FontInfo *font;
  
#ifdef FONTSEL_DEBUG
  g_message("In select_font\n");
#endif
  fontsel = GTK_FONT_SELECTION(data);
  font_info = fontsel_info->font_info;
  
  if (bevent && !GTK_WIDGET_HAS_FOCUS (w))
    gtk_widget_grab_focus (w);
  
  row = GPOINTER_TO_INT (gtk_clist_get_row_data (GTK_CLIST (fontsel->font_clist), row));
  font = &font_info[row];
  gtk_entry_set_text(GTK_ENTRY(fontsel->font_entry), font->family);
  
  /* If it is already the current font, just return. */
  if (fontsel->font_index == row)
    return;
  
  fontsel->font_index = row;
  gtk_font_selection_show_available_styles (fontsel);
  gtk_font_selection_select_best_style (fontsel, TRUE);
}


static gint
gtk_font_selection_on_clist_key_press (GtkWidget        *clist,
				       GdkEventKey      *event,
				       GtkFontSelection *fontsel)
{
#ifdef FONTSEL_DEBUG
  g_message("In on_clist_key_press\n");
#endif
  if (event->keyval == GDK_Up)
    return gtk_font_selection_select_next (fontsel, clist, -1);
  else if (event->keyval == GDK_Down)
    return gtk_font_selection_select_next (fontsel, clist, 1);
  else
    return FALSE;
}


static gboolean
gtk_font_selection_select_next (GtkFontSelection *fontsel,
				GtkWidget        *clist,
				gint		  step)
{
  GList *selection;
  gint current_row, row;
  
  selection = GTK_CLIST(clist)->selection;
  if (!selection)
    return FALSE;
  current_row = GPOINTER_TO_INT (selection->data);
  
  /* Stop the normal clist key handler from being run. */
  gtk_signal_emit_stop_by_name (GTK_OBJECT (clist), "key_press_event");

  for (row = current_row + step;
       row >= 0 && row < GTK_CLIST(clist)->rows;
       row += step)
    {
      /* If this is the style clist, make sure that the item is not a charset
	 entry. */
      if (clist == fontsel->font_style_clist)
	if (GPOINTER_TO_INT (gtk_clist_get_row_data(GTK_CLIST(clist), row)) == -1)
	  continue;
      
      /* Now we've found the row to select. */
      if (gtk_clist_row_is_visible(GTK_CLIST(clist), row)
	  != GTK_VISIBILITY_FULL)
	gtk_clist_moveto(GTK_CLIST(clist), row, -1, (step < 0) ? 0 : 1, 0);
      gtk_clist_select_row(GTK_CLIST(clist), row, 0);
      break;
    }
  return TRUE;
}


/* This fills the font style clist with all the possible style combinations
   for the current font family. */
static void
gtk_font_selection_show_available_styles (GtkFontSelection *fontsel)
{
  FontInfo *font;
  FontStyle *styles;
  gint style, tmpstyle, row;
  gint weight_index, slant_index, set_width_index, spacing_index;
  gint charset_index;
  gchar *weight, *slant, *set_width, *spacing;
  gchar *charset = NULL;
  gchar *new_item;
  gchar buffer[XLFD_MAX_FIELD_LEN * 6 + 2];
  GdkColor *inactive_fg, *inactive_bg;
  gboolean show_charset;
  
#ifdef FONTSEL_DEBUG
  g_message("In show_available_styles\n");
#endif
  font = &fontsel_info->font_info[fontsel->font_index];
  styles = &fontsel_info->font_styles[font->style_index];
  
  gtk_clist_freeze (GTK_CLIST(fontsel->font_style_clist));
  gtk_clist_clear (GTK_CLIST(fontsel->font_style_clist));
  
  /* First we mark all visible styles as not having been displayed yet,
     and check if every style has the same charset. If not then we will
     display the charset in the list before the styles. */
  show_charset = FALSE;
  charset_index = -1;
  for (style = 0; style < font->nstyles; style++)
    {
      if (gtk_font_selection_style_visible(fontsel, font, style))
	{
	  styles[style].flags &= ~GTK_FONT_DISPLAYED;
	  
	  if (charset_index == -1)
	    charset_index  = styles[style].properties[CHARSET];
	  else if (charset_index != styles[style].properties[CHARSET])
	    show_charset = TRUE;
	}
      else
	styles[style].flags |= GTK_FONT_DISPLAYED;
    }
  
  /* Step through the undisplayed styles, finding the next charset which
     hasn't been displayed yet. Then display the charset on one line, if
     necessary, and the visible styles indented beneath it. */
  inactive_fg = &fontsel->font_style_clist->style->fg[GTK_STATE_INSENSITIVE];
  inactive_bg = &fontsel->font_style_clist->style->bg[GTK_STATE_INSENSITIVE];
  
  for (style = 0; style < font->nstyles; style++)
    {
      if (styles[style].flags & GTK_FONT_DISPLAYED)
	continue;
      
      if (show_charset)
	{
	  charset_index  = styles[style].properties[CHARSET];
	  charset  = fontsel_info->properties[CHARSET] [charset_index];
	  row = gtk_clist_append(GTK_CLIST(fontsel->font_style_clist),
				 &charset);
	  gtk_clist_set_row_data(GTK_CLIST(fontsel->font_style_clist), row,
				 (gpointer) -1);
	  if (GTK_WIDGET_REALIZED (fontsel->font_style_clist))
	    {
	      gtk_clist_set_foreground(GTK_CLIST(fontsel->font_style_clist),
				       row, inactive_fg);
	      gtk_clist_set_background(GTK_CLIST(fontsel->font_style_clist),
				       row, inactive_bg);
	    }
	}
      
      for (tmpstyle = style; tmpstyle < font->nstyles; tmpstyle++)
	{
	  if (styles[tmpstyle].flags & GTK_FONT_DISPLAYED
	      || charset_index != styles[tmpstyle].properties[CHARSET])
	    continue;
	  
	  styles[tmpstyle].flags |= GTK_FONT_DISPLAYED;
	  
	  weight_index    = styles[tmpstyle].properties[WEIGHT];
	  slant_index     = styles[tmpstyle].properties[SLANT];
	  set_width_index = styles[tmpstyle].properties[SET_WIDTH];
	  spacing_index   = styles[tmpstyle].properties[SPACING];
	  weight    = fontsel_info->properties[WEIGHT]   [weight_index];
	  slant     = fontsel_info->properties[SLANT]    [slant_index];
	  set_width = fontsel_info->properties[SET_WIDTH][set_width_index];
	  spacing   = fontsel_info->properties[SPACING]  [spacing_index];
	  
	  /* Convert '(nil)' weights to 'regular', since it looks nicer. */
	  if      (!g_strcasecmp(weight, N_("(nil)")))	weight = N_("regular");
	  
	  /* We don't show default values or (nil) in the other properties. */
	  if      (!g_strcasecmp(slant, "r"))        slant = NULL;
	  else if (!g_strcasecmp(slant, "(nil)"))    slant = NULL;
	  else if (!g_strcasecmp(slant, "i"))        slant = N_("italic");
	  else if (!g_strcasecmp(slant, "o"))        slant = N_("oblique");
	  else if (!g_strcasecmp(slant, "ri"))       slant = N_("reverse italic");
	  else if (!g_strcasecmp(slant, "ro"))       slant = N_("reverse oblique");
	  else if (!g_strcasecmp(slant, "ot"))       slant = N_("other");
	  
	  if      (!g_strcasecmp(set_width, "normal")) set_width = NULL;
	  else if (!g_strcasecmp(set_width, "(nil)"))  set_width = NULL;
	  
	  if      (!g_strcasecmp(spacing, "p"))        spacing = NULL;
	  else if (!g_strcasecmp(spacing, "(nil)"))    spacing = NULL;
	  else if (!g_strcasecmp(spacing, "m"))        spacing = N_("[M]");
	  else if (!g_strcasecmp(spacing, "c"))        spacing = N_("[C]");
	  
	  /* Add the strings together, making sure there is 1 space between
	     them */
	  strcpy(buffer, _(weight));
	  if (slant)
	    {
	      strcat(buffer, " ");
	      strcat(buffer, _(slant));
	    }
	  if (set_width)
	    {
	      strcat(buffer, " ");
	      strcat(buffer, _(set_width));
	    }
	  if (spacing)
	    {
	      strcat(buffer, " ");
	      strcat(buffer, _(spacing));
	    }
	  
	  new_item = buffer;
	  row = gtk_clist_append(GTK_CLIST(fontsel->font_style_clist),
				 &new_item);
	  if (show_charset)
	    gtk_clist_set_shift(GTK_CLIST(fontsel->font_style_clist), row, 0,
				0, 4);
	  gtk_clist_set_row_data(GTK_CLIST(fontsel->font_style_clist), row,
				 GINT_TO_POINTER (tmpstyle));
	}
    }
  
  gtk_clist_thaw (GTK_CLIST(fontsel->font_style_clist));
}


/* This selects a style when the user selects a font. It just uses the first
   available style at present. I was thinking of trying to maintain the
   selected style, e.g. bold italic, when the user selects different fonts.
   However, the interface is so easy to use now I'm not sure it's worth it.
   Note: This will load a font. */
static void
gtk_font_selection_select_best_style(GtkFontSelection *fontsel,
				     gboolean	       use_first)
{
  FontInfo *font;
  FontStyle *styles;
  gint row, prop, style, matched;
  gint best_matched = -1, best_style = -1, best_row = -1;
  
#ifdef FONTSEL_DEBUG
  g_message("In select_best_style\n");
#endif
  font = &fontsel_info->font_info[fontsel->font_index];
  styles = &fontsel_info->font_styles[font->style_index];
  
  for (row = 0; row < GTK_CLIST(fontsel->font_style_clist)->rows; row++)
    {
      style = GPOINTER_TO_INT (gtk_clist_get_row_data (GTK_CLIST (fontsel->font_style_clist), row));
      /* Skip charset rows. */
      if (style == -1)
	continue;

      /* If we just want the first style, we've got it. */
      if (use_first)
	{
	  best_style = style;
	  best_row = row;
	  break;
	}

      matched = 0;
      for (prop = 0; prop < GTK_NUM_STYLE_PROPERTIES; prop++)
	{
	  if (fontsel->property_values[prop] == styles[style].properties[prop])
	    matched++;
	}
      if (matched > best_matched)
	{
	  best_matched = matched;
	  best_style = style;
	  best_row = row;
	}
    }
  g_return_if_fail (best_style != -1);
  g_return_if_fail (best_row != -1);

  fontsel->style = best_style;

  for (prop = 0; prop < GTK_NUM_STYLE_PROPERTIES; prop++)
    fontsel->property_values[prop] = styles[fontsel->style].properties[prop];

  gtk_clist_select_row(GTK_CLIST(fontsel->font_style_clist), best_row, 0);
  if (gtk_clist_row_is_visible(GTK_CLIST(fontsel->font_style_clist), best_row)
      != GTK_VISIBILITY_FULL)
    gtk_clist_moveto(GTK_CLIST(fontsel->font_style_clist), best_row, -1,
		     0.5, 0);
  gtk_font_selection_show_available_sizes (fontsel);
  gtk_font_selection_select_best_size (fontsel);
}


/* This is called when a style is selected in the list. */
static void
gtk_font_selection_select_style (GtkWidget      *w,
				 gint	        row,
				 gint	        column,
				 GdkEventButton *bevent,
				 gpointer        data)
{
  GtkFontSelection *fontsel;
  FontInfo *font_info;
  FontInfo *font;
  FontStyle *styles;
  gint style, prop;
  gchar *text;
  
#ifdef FONTSEL_DEBUG
  g_message("In select_style\n");
#endif
  fontsel = GTK_FONT_SELECTION(data);
  font_info = fontsel_info->font_info;
  font = &font_info[fontsel->font_index];
  styles = &fontsel_info->font_styles[font->style_index];
  
  if (bevent && !GTK_WIDGET_HAS_FOCUS (w))
    gtk_widget_grab_focus (w);
  
  /* The style index is stored in the row data, so we just need to copy
     the style values into the fontsel and reload the font. */
  style = GPOINTER_TO_INT (gtk_clist_get_row_data(GTK_CLIST(fontsel->font_style_clist), row));
  
  /* Don't allow selection of charset rows. */
  if (style == -1)
    {
      gtk_clist_unselect_row(GTK_CLIST(fontsel->font_style_clist), row, 0);
      return;
    }
  
  gtk_clist_get_text(GTK_CLIST(fontsel->font_style_clist), row, 0, &text);
  gtk_entry_set_text(GTK_ENTRY(fontsel->font_style_entry), text);
  
  for (prop = 0; prop < GTK_NUM_STYLE_PROPERTIES; prop++)
    fontsel->property_values[prop] = styles[style].properties[prop];
  
  if (fontsel->style == style)
    return;
  
  fontsel->style = style;
  gtk_font_selection_show_available_sizes (fontsel);
  gtk_font_selection_select_best_size (fontsel);
}


/* This shows all the available sizes in the size clist, according to the
   current metric and the current font & style. */
static void
gtk_font_selection_show_available_sizes (GtkFontSelection *fontsel)
{
  FontInfo *font;
  FontStyle *styles, *style;
  const guint16 *standard_sizes;
  guint16 *bitmapped_sizes;
  gint nstandard_sizes, nbitmapped_sizes;
  gchar buffer[16], *size;
  gfloat bitmap_size_float = 0.;
  guint16 bitmap_size = 0;
  gboolean can_match;
  gint type_filter;
  
#ifdef FONTSEL_DEBUG
  g_message("In show_available_sizes\n");
#endif
  font = &fontsel_info->font_info[fontsel->font_index];
  styles = &fontsel_info->font_styles[font->style_index];
  style = &styles[fontsel->style];
  
  standard_sizes = font_sizes;
  nstandard_sizes = sizeof(font_sizes) / sizeof(font_sizes[0]);
  if (fontsel->metric == GTK_FONT_METRIC_POINTS)
    {
      bitmapped_sizes = &fontsel_info->point_sizes[style->point_sizes_index];
      nbitmapped_sizes = style->npoint_sizes;
    }
  else
    {
      bitmapped_sizes = &fontsel_info->pixel_sizes[style->pixel_sizes_index];
      nbitmapped_sizes = style->npixel_sizes;
    }
  
  /* Only show the standard sizes if a scalable font is available. */
  type_filter = fontsel->filters[GTK_FONT_FILTER_BASE].font_type
    & fontsel->filters[GTK_FONT_FILTER_USER].font_type;

  if (!((style->flags & GTK_FONT_SCALABLE_BITMAP
	 && type_filter & GTK_FONT_SCALABLE_BITMAP)
	|| (style->flags & GTK_FONT_SCALABLE
	    && type_filter & GTK_FONT_SCALABLE)))
    nstandard_sizes = 0;
  
  gtk_clist_freeze (GTK_CLIST(fontsel->size_clist));
  gtk_clist_clear (GTK_CLIST(fontsel->size_clist));
  
  /* Interleave the standard sizes with the bitmapped sizes so we get a list
     of ascending sizes. If the metric is points, we have to convert the
     decipoints to points. */
  while (nstandard_sizes || nbitmapped_sizes)
    {
      can_match = TRUE;

      if (nbitmapped_sizes)
	{
	  if (fontsel->metric == GTK_FONT_METRIC_POINTS)
	    {
	      if (*bitmapped_sizes % 10 != 0)
		can_match = FALSE;
	      bitmap_size = *bitmapped_sizes / 10;
	      bitmap_size_float = *bitmapped_sizes / 10;
	    }
	  else
	    {
	      bitmap_size = *bitmapped_sizes;
	      bitmap_size_float = *bitmapped_sizes;
	    }
	}

      if (can_match && nstandard_sizes && nbitmapped_sizes
	  && *standard_sizes == bitmap_size)
	{
	  sprintf(buffer, "%i *", *standard_sizes);
	  standard_sizes++;
	  nstandard_sizes--;
	  bitmapped_sizes++;
	  nbitmapped_sizes--;
	}
      else if (nstandard_sizes
	       && (!nbitmapped_sizes
		   || (gfloat)*standard_sizes < bitmap_size_float))
	{
	  sprintf(buffer, "%i", *standard_sizes);
	  standard_sizes++;
	  nstandard_sizes--;
	}
      else
	{
	  if (fontsel->metric == GTK_FONT_METRIC_POINTS)
	    {
	      if (*bitmapped_sizes % 10 == 0)
		sprintf(buffer, "%i *", *bitmapped_sizes / 10);
	      else
		sprintf(buffer, "%i.%i *", *bitmapped_sizes / 10,
			*bitmapped_sizes % 10);
	    }
	  else
	    {
	      sprintf(buffer, "%i *", *bitmapped_sizes);
	    }
	  bitmapped_sizes++;
	  nbitmapped_sizes--;
	}
      size = buffer;
      gtk_clist_append(GTK_CLIST(fontsel->size_clist), &size);
    }
  gtk_clist_thaw (GTK_CLIST(fontsel->size_clist));
}

static void
gtk_font_selection_update_size (GtkFontSelection *fontsel)
{
  gint new_size;
  gfloat new_size_float;
  gchar *text;
  
#ifdef FONTSEL_DEBUG
  g_message("In update_size\n");
#endif
  
  text = gtk_entry_get_text (GTK_ENTRY (fontsel->size_entry));
  if (fontsel->metric == GTK_FONT_METRIC_PIXELS)
    {
      new_size = atoi (text);
      if (new_size < 2)
	new_size = 2;
    }
  else
    {
      new_size_float = atof (text) * 10;
      new_size = (gint) new_size_float;
      if (new_size < 20)
	new_size = 20;
    }
  
  /* Remember that this size was set explicitly. */
  fontsel->selected_size = new_size;
  
  /* Check if the font size has changed, and return if it hasn't. */
  if (fontsel->size == new_size)
    return;
  
  fontsel->size = new_size;
  gtk_font_selection_select_best_size (fontsel);
}

/* If the user hits return in the font size entry, we change to the new font
   size. */
static void
gtk_font_selection_size_activate (GtkWidget *w,
				  gpointer   data)
{
  gtk_font_selection_update_size (data);
}

/* This tries to select the closest size to the current size, though it
   may have to change the size if only unscaled bitmaps are available.
   Note: this will load a font. */
static void
gtk_font_selection_select_best_size(GtkFontSelection *fontsel)
{
  FontInfo *font;
  FontStyle *styles, *style;
  gchar *text;
  gint row, best_row = 0, size, size_fraction, best_size = 0, nmatched;
  gboolean found = FALSE;
  gchar buffer[32];
  GList *selection;
  gint type_filter;

#ifdef FONTSEL_DEBUG
  g_message("In select_best_size\n");
#endif

  if (fontsel->font_index == -1)
    return;
  
  font = &fontsel_info->font_info[fontsel->font_index];
  styles = &fontsel_info->font_styles[font->style_index];
  style = &styles[fontsel->style];
  
  /* Find the closest size available in the size clist. If the exact size is
     in the list set found to TRUE. */
  for (row = 0; row < GTK_CLIST(fontsel->size_clist)->rows; row++)
    {
      gtk_clist_get_text(GTK_CLIST(fontsel->size_clist), row, 0, &text);
      nmatched = sscanf(text, "%i.%i", &size, &size_fraction);
      if (fontsel->metric == GTK_FONT_METRIC_POINTS)
	{
	  size *= 10;
	  if (nmatched == 2)
	    size += size_fraction;
	}
      
      if (size == fontsel->selected_size)
	{
	  found = TRUE;
	  best_size = size;
	  best_row = row;
	  break;
	}
      else if (best_size == 0
	       || abs(size - fontsel->selected_size)
	       < (abs(best_size - fontsel->selected_size)))
	{
	  best_size = size;
	  best_row = row;
	}
    }
  
  /* If we aren't scaling bitmapped fonts and this is a bitmapped font, we
     need to use the closest size found. */
  type_filter = fontsel->filters[GTK_FONT_FILTER_BASE].font_type
    & fontsel->filters[GTK_FONT_FILTER_USER].font_type;

  if (!((style->flags & GTK_FONT_SCALABLE_BITMAP
	 && type_filter & GTK_FONT_SCALABLE_BITMAP)
	|| (style->flags & GTK_FONT_SCALABLE
	    && type_filter & GTK_FONT_SCALABLE)))
    found = TRUE;
  
  if (found)
    {
      fontsel->size = best_size;
      gtk_clist_moveto(GTK_CLIST(fontsel->size_clist), best_row, -1, 0.5, 0);
      gtk_clist_select_row(GTK_CLIST(fontsel->size_clist), best_row, 0);
    }
  else
    {
      fontsel->size = fontsel->selected_size;
      selection = GTK_CLIST(fontsel->size_clist)->selection;
      if (selection)
	gtk_clist_unselect_row(GTK_CLIST(fontsel->size_clist),
			       GPOINTER_TO_INT (selection->data), 0);
      gtk_clist_moveto(GTK_CLIST(fontsel->size_clist), best_row, -1, 0.5, 0);
      
      /* Show the size in the size entry. */
      if (fontsel->metric == GTK_FONT_METRIC_PIXELS)
	sprintf(buffer, "%i", fontsel->size);
      else
	{
	  if (fontsel->size % 10 == 0)
	    sprintf(buffer, "%i", fontsel->size / 10);
	  else
	    sprintf(buffer, "%i.%i", fontsel->size / 10, fontsel->size % 10);
	}
      gtk_entry_set_text (GTK_ENTRY (fontsel->size_entry), buffer);
    }
  gtk_font_selection_load_font (fontsel);
}


/* This is called when a size is selected in the list. */
static void
gtk_font_selection_select_size (GtkWidget      *w,
				gint	        row,
				gint	        column,
				GdkEventButton *bevent,
				gpointer        data)
{
  GtkFontSelection *fontsel;
  gdouble new_size;
  gchar *text;
  gchar buffer[16];
  gint i;
  
#ifdef FONTSEL_DEBUG
  g_message("In select_size\n");
#endif
  fontsel = GTK_FONT_SELECTION(data);
  
  if (bevent && !GTK_WIDGET_HAS_FOCUS (w))
    gtk_widget_grab_focus (w);
  
  /* Copy the size from the clist to the size entry, but without the bitmapped
     marker ('*'). */
  gtk_clist_get_text(GTK_CLIST(fontsel->size_clist), row, 0, &text);
  i = 0;
  while (i < 15 && (text[i] == '.' || (text[i] >= '0' && text[i] <= '9')))
    {
      buffer[i] = text[i];
      i++;
    }
  buffer[i] = '\0';
  gtk_entry_set_text(GTK_ENTRY(fontsel->size_entry), buffer);
  
  /* Check if the font size has changed, and return if it hasn't. */
  new_size = atof(text);
  if (fontsel->metric == GTK_FONT_METRIC_POINTS)
    new_size *= 10;
  
  if (fontsel->size == (gint)new_size)
    return;
  
  /* If the size was selected by the user we set the selected_size. */
  fontsel->selected_size = new_size;
  
  fontsel->size = new_size;
  gtk_font_selection_load_font (fontsel);
}


/* This is called when the pixels or points radio buttons are pressed. */
static void
gtk_font_selection_metric_callback (GtkWidget *w,
				    gpointer   data)
{
  GtkFontSelection *fontsel = GTK_FONT_SELECTION(data);
  
#ifdef FONTSEL_DEBUG
  g_message("In metric_callback\n");
#endif
  if (GTK_TOGGLE_BUTTON(fontsel->pixels_button)->active)
    {
      if (fontsel->metric == GTK_FONT_METRIC_PIXELS)
	return;
      fontsel->metric = GTK_FONT_METRIC_PIXELS;
      fontsel->size = (fontsel->size + 5) / 10;
      fontsel->selected_size = (fontsel->selected_size + 5) / 10;
    }
  else
    {
      if (fontsel->metric == GTK_FONT_METRIC_POINTS)
	return;
      fontsel->metric = GTK_FONT_METRIC_POINTS;
      fontsel->size *= 10;
      fontsel->selected_size *= 10;
    }
  if (fontsel->font_index != -1)
    {
      gtk_font_selection_show_available_sizes (fontsel);
      gtk_font_selection_select_best_size (fontsel);
    }
}


/* This searches the given property table and returns the index of the given
   string, or 0, which is the wildcard '*' index, if it's not found. */
static guint16
gtk_font_selection_field_to_index (gchar **table,
				   gint    ntable,
				   gchar  *field)
{
  gint i;
  
  for (i = 0; i < ntable; i++)
    if (strcmp (field, table[i]) == 0)
      return i;
  
  return 0;
}



/* This attempts to load the current font, and returns TRUE if it succeeds. */
static gboolean
gtk_font_selection_load_font (GtkFontSelection *fontsel)
{
  GdkFont *font;
  gchar *fontname, *label_text;
  XFontStruct *xfs;
  
  if (fontsel->font)
    gdk_font_unref (fontsel->font);
  fontsel->font = NULL;
  
  /* If no family has been selected yet, just return FALSE. */
  if (fontsel->font_index == -1)
    return FALSE;
  
  fontname = gtk_font_selection_get_font_name (fontsel);
  if (fontname)
    {
#ifdef FONTSEL_DEBUG
      g_message("Loading: %s\n", fontname);
#endif
      font = gdk_font_load (fontname);
      xfs = font ? GDK_FONT_XFONT (font) : NULL;
      if (xfs && (xfs->min_byte1 != 0 || xfs->max_byte1 != 0))
	{
	  gchar *tmp_name;
	  
	  gdk_font_unref (font);
	  tmp_name = g_strconcat (fontname, ",*", NULL);
	  font = gdk_fontset_load (tmp_name);
	  g_free (tmp_name);
	}
      g_free (fontname);
      
      if (font)
	{
	  fontsel->font = font;
	  /* Make sure the message label is empty, but don't change it unless
	     it's necessary as it results in a resize of the whole window! */
	  gtk_label_get(GTK_LABEL(fontsel->message_label), &label_text);
	  if (strcmp(label_text, ""))
	    gtk_label_set_text(GTK_LABEL(fontsel->message_label), "");
	  gtk_font_selection_update_preview (fontsel);
	  return TRUE;
	}
      else 
	{
	  gtk_label_set_text(GTK_LABEL(fontsel->message_label),
			     _("The selected font is not available."));
	}
    }
  else
    {
      gtk_label_set_text(GTK_LABEL(fontsel->message_label),
			 _("The selected font is not a valid font."));
    }
  
  return FALSE;
}


/* This sets the font in the preview entry to the selected font, and tries to
   make sure that the preview entry is a reasonable size, i.e. so that the
   text can be seen with a bit of space to spare. But it tries to avoid
   resizing the entry every time the font changes.
   This also used to shrink the preview if the font size was decreased, but
   that made it awkward if the user wanted to resize the window themself. */
static void
gtk_font_selection_update_preview (GtkFontSelection *fontsel)
{
  GtkWidget *preview_entry;
  GtkStyle *style;
  gint text_height, new_height;
  gchar *text;
  XFontStruct *xfs;
  
#ifdef FONTSEL_DEBUG
  g_message("In update_preview\n");
#endif
  style = gtk_style_new ();
  gdk_font_unref (style->font);
  style->font = fontsel->font;
  gdk_font_ref (style->font);
  
  preview_entry = fontsel->preview_entry;
  gtk_widget_set_style (preview_entry, style);
  gtk_style_unref(style);
  
  text_height = preview_entry->style->font->ascent
    + preview_entry->style->font->descent;
  /* We don't ever want to be over MAX_PREVIEW_HEIGHT pixels high. */
  new_height = text_height + 20;
  if (new_height < INITIAL_PREVIEW_HEIGHT)
    new_height = INITIAL_PREVIEW_HEIGHT;
  if (new_height > MAX_PREVIEW_HEIGHT)
    new_height = MAX_PREVIEW_HEIGHT;
  
  if ((preview_entry->requisition.height < text_height + 10)
      || (preview_entry->requisition.height > text_height + 40))
    gtk_widget_set_usize(preview_entry, -1, new_height);
  
  /* This sets the preview text, if it hasn't been set already. */
  text = gtk_entry_get_text(GTK_ENTRY(fontsel->preview_entry));
  if (strlen(text) == 0)
    gtk_entry_set_text(GTK_ENTRY(fontsel->preview_entry), PREVIEW_TEXT);
  gtk_entry_set_position(GTK_ENTRY(fontsel->preview_entry), 0);
  
  /* If this is a 2-byte font display a message to say it may not be
     displayed properly. */
  xfs = GDK_FONT_XFONT(fontsel->font);
  if (xfs->min_byte1 != 0 || xfs->max_byte1 != 0)
    gtk_label_set_text(GTK_LABEL(fontsel->message_label),
		       _("This is a 2-byte font and may not be displayed correctly."));
}


static void
gtk_font_selection_switch_page (GtkWidget       *w,
				GtkNotebookPage *page,
				gint             page_num,
				gpointer         data)
{
  GtkFontSelection *fontsel = GTK_FONT_SELECTION(data);
  
  /* This function strangely gets called when the window is destroyed,
     so we check here to see if the notebook is visible. */
  if (!GTK_WIDGET_VISIBLE(w))
    return;
  
  if (page_num == 0)
    gtk_font_selection_update_filter(fontsel);
  else if (page_num == 1)
    gtk_font_selection_show_font_info(fontsel);
}


static void
gtk_font_selection_show_font_info (GtkFontSelection *fontsel)
{
  Atom font_atom, atom;
  Bool status;
  char *name;
  gchar *fontname;
  gchar field_buffer[XLFD_MAX_FIELD_LEN];
  gchar *field;
  gint i;
  gboolean shown_actual_fields = FALSE;
  
  fontname = gtk_font_selection_get_font_name(fontsel);
  gtk_entry_set_text(GTK_ENTRY(fontsel->requested_font_name),
		     fontname ? fontname : "");
  
  gtk_clist_freeze (GTK_CLIST(fontsel->info_clist));
  for (i = 0; i < GTK_XLFD_NUM_FIELDS; i++)
    {
      if (fontname)
	field = gtk_font_selection_get_xlfd_field (fontname, i, field_buffer);
      else
	field = NULL;
      if (field)
	{
	  if (i == XLFD_SLANT)
	    field = gtk_font_selection_expand_slant_code(field);
	  else if (i == XLFD_SPACING)
	    field = gtk_font_selection_expand_spacing_code(field);
	}
      gtk_clist_set_text(GTK_CLIST(fontsel->info_clist), i, 1,
			 field ? field : "");
    }
  
  if (fontsel->font)
    {
      font_atom = gdk_atom_intern ("FONT", FALSE);

      if (fontsel->font->type == GDK_FONT_FONTSET)
	{
	  XFontStruct **font_structs;
	  gint num_fonts;
	  gchar **font_names;
	  
	  num_fonts = XFontsOfFontSet (GDK_FONT_XFONT(fontsel->font),
				       &font_structs, &font_names);
	  status = XGetFontProperty(font_structs[0], font_atom, &atom);
	}
      else
	{
	  status = XGetFontProperty(GDK_FONT_XFONT(fontsel->font), font_atom,
				    &atom);
	}

      if (status == True)
	{
	  name = gdk_atom_name (atom);
	  gtk_entry_set_text(GTK_ENTRY(fontsel->actual_font_name), name);
	  
	  for (i = 0; i < GTK_XLFD_NUM_FIELDS; i++)
	    {
	      field = gtk_font_selection_get_xlfd_field (name, i,
							 field_buffer);
	      if (i == XLFD_SLANT)
		field = gtk_font_selection_expand_slant_code(field);
	      else if (i == XLFD_SPACING)
		field = gtk_font_selection_expand_spacing_code(field);
	      gtk_clist_set_text(GTK_CLIST(fontsel->info_clist), i, 2,
				 field ? field : "");
	    }
	  shown_actual_fields = TRUE;
	  g_free (name);
	}
    }
  if (!shown_actual_fields)
    {
      gtk_entry_set_text(GTK_ENTRY(fontsel->actual_font_name), "");
      for (i = 0; i < GTK_XLFD_NUM_FIELDS; i++)
	{
	  gtk_clist_set_text(GTK_CLIST(fontsel->info_clist), i, 2,
			     fontname ? _("(unknown)") : "");
	}
    }
  gtk_clist_thaw (GTK_CLIST(fontsel->info_clist));
  g_free(fontname);
}


static gchar*
gtk_font_selection_expand_slant_code(gchar *slant)
{
  if      (!g_strcasecmp(slant, "r"))   return(_("roman"));
  else if (!g_strcasecmp(slant, "i"))   return(_("italic"));
  else if (!g_strcasecmp(slant, "o"))   return(_("oblique"));
  else if (!g_strcasecmp(slant, "ri"))  return(_("reverse italic"));
  else if (!g_strcasecmp(slant, "ro"))  return(_("reverse oblique"));
  else if (!g_strcasecmp(slant, "ot"))  return(_("other"));
  return slant;
}

static gchar*
gtk_font_selection_expand_spacing_code(gchar *spacing)
{
  if      (!g_strcasecmp(spacing, "p")) return(_("proportional"));
  else if (!g_strcasecmp(spacing, "m")) return(_("monospaced"));
  else if (!g_strcasecmp(spacing, "c")) return(_("char cell"));
  return spacing;
}


/*****************************************************************************
 * These functions all deal with the Filter page and filtering the fonts.
 *****************************************************************************/

/* This is called when an item is selected in one of the filter clists.
   We make sure that the first row of the clist, i.e. the wildcard '*', is
   selected if and only if none of the other items are selected.
   Also doesn't allow selections of values filtered out by base filter.
   We may need to be careful about triggering other signals. */
static void
gtk_font_selection_select_filter	     (GtkWidget      *w,
					      gint	      row,
					      gint	      column,
					      GdkEventButton *bevent,
					      GtkFontSelection *fontsel)
{
  gint i, prop, index;
  
  if (row == 0)
    {
      for (i = 1; i < GTK_CLIST(w)->rows; i++)
	gtk_clist_unselect_row(GTK_CLIST(w), i, 0);
    }
  else
    {
      /* Find out which property this is. */
      for (prop = 0; prop < GTK_NUM_FONT_PROPERTIES; prop++)
	if (fontsel->filter_clists[prop] == w)
	  break;
      index = GPOINTER_TO_INT (gtk_clist_get_row_data(GTK_CLIST(w), row));
      if (gtk_font_selection_filter_state (fontsel, GTK_FONT_FILTER_BASE,
					   prop, index) == NOT_FILTERED)
	gtk_clist_unselect_row(GTK_CLIST(w), row, 0);
      else
	gtk_clist_unselect_row(GTK_CLIST(w), 0, 0);
    }
}


/* Here a filter item is being deselected. If there are now no items selected
   we select the first '*' item, unless that it is the item being deselected,
   in which case we select all of the other items. This makes it easy to
   select all items in the list except one or two. */
static void
gtk_font_selection_unselect_filter	     (GtkWidget      *w,
					      gint	      row,
					      gint	      column,
					      GdkEventButton *bevent,
					      GtkFontSelection *fontsel)
{
  gint i, prop, index;

  if (!GTK_CLIST(w)->selection)
    {
      if (row == 0)
	{
	  /* Find out which property this is. */
	  for (prop = 0; prop < GTK_NUM_FONT_PROPERTIES; prop++)
	    if (fontsel->filter_clists[prop] == w)
	      break;

	  for (i = 1; i < GTK_CLIST(w)->rows; i++)
	    {
	      index = GPOINTER_TO_INT (gtk_clist_get_row_data(GTK_CLIST(w),
							      i));
	      if (gtk_font_selection_filter_state (fontsel,
						   GTK_FONT_FILTER_BASE,
						   prop, index)
		  != NOT_FILTERED)
		gtk_clist_select_row(GTK_CLIST(w), i, 0);
	    }
	}
      else
	{
	  gtk_clist_select_row(GTK_CLIST(w), 0, 0);
	}
    }
}


/* This is called when the main notebook page is selected. It checks if the
   filter has changed, an if so it creates the filter settings, and filters the
   fonts shown. If an empty filter (all '*'s) is applied, then filtering is
   turned off. */
static void
gtk_font_selection_update_filter     (GtkFontSelection *fontsel)
{
  GtkWidget *clist;
  GList *selection;
  gboolean default_filter = TRUE, filter_changed = FALSE;
  gint prop, nselected, i, row, index;
  GtkFontFilter *filter = &fontsel->filters[GTK_FONT_FILTER_USER];
  gint base_font_type, user_font_type, new_font_type;
  
#ifdef FONTSEL_DEBUG
  g_message("In update_filter\n");
#endif
  
  /* Check if the user filter has changed, and also if it is the default
     filter, i.e. bitmap & scalable fonts and all '*'s selected.
     We only look at the bits which are not already filtered out by the base
     filter, since that overrides the user filter. */
  base_font_type = fontsel->filters[GTK_FONT_FILTER_BASE].font_type
    & GTK_FONT_ALL;
  user_font_type = fontsel->filters[GTK_FONT_FILTER_USER].font_type
    & GTK_FONT_ALL;
  new_font_type = GTK_TOGGLE_BUTTON(fontsel->type_bitmaps_button)->active
    ? GTK_FONT_BITMAP : 0;
  new_font_type |= (GTK_TOGGLE_BUTTON(fontsel->type_scalable_button)->active
    ? GTK_FONT_SCALABLE : 0);
  new_font_type |= (GTK_TOGGLE_BUTTON(fontsel->type_scaled_bitmaps_button)->active ? GTK_FONT_SCALABLE_BITMAP : 0);
  new_font_type &= base_font_type;
  new_font_type |= (~base_font_type & user_font_type);
  if (new_font_type != (GTK_FONT_BITMAP | GTK_FONT_SCALABLE))
    default_filter = FALSE;

  if (new_font_type != user_font_type)
    filter_changed = TRUE;
  fontsel->filters[GTK_FONT_FILTER_USER].font_type = new_font_type;

  for (prop = 0; prop < GTK_NUM_FONT_PROPERTIES; prop++)
    {
      clist = fontsel->filter_clists[prop];
      selection = GTK_CLIST(clist)->selection;
      nselected = g_list_length(selection);
      if (nselected != 1 || GPOINTER_TO_INT (selection->data) != 0)
	{
	  default_filter = FALSE;
	  
	  if (filter->property_nfilters[prop] != nselected)
	    filter_changed = TRUE;
	  else
	    {
	      for (i = 0; i < nselected; i++)
		{
		  row = GPOINTER_TO_INT (selection->data);
		  index = GPOINTER_TO_INT (gtk_clist_get_row_data (GTK_CLIST (clist), row));
		  if (filter->property_filters[prop][i] != index)
		    filter_changed = TRUE;
		  selection = selection->next;
		}
	    }
	}
      else
	{
	  if (filter->property_nfilters[prop] != 0)
	    filter_changed = TRUE;
	}
    }
  
  /* If the filter hasn't changed we just return. */
  if (!filter_changed)
    return;
  
#ifdef FONTSEL_DEBUG
  g_message("   update_fonts: filter has changed\n");
#endif
  
  /* Free the old filter data and create the new arrays. */
  for (prop = 0; prop < GTK_NUM_FONT_PROPERTIES; prop++)
    {
      g_free(filter->property_filters[prop]);

      clist = fontsel->filter_clists[prop];
      selection = GTK_CLIST(clist)->selection;
      nselected = g_list_length(selection);
      if (nselected == 1 && GPOINTER_TO_INT (selection->data) == 0)
	{
	  filter->property_filters[prop] = NULL;
	  filter->property_nfilters[prop] = 0;
	}
      else
	{
	  filter->property_filters[prop] = g_new(guint16, nselected);
	  filter->property_nfilters[prop] = nselected;
	  for (i = 0; i < nselected; i++)
	    {
	      row = GPOINTER_TO_INT (selection->data);
	      index = GPOINTER_TO_INT (gtk_clist_get_row_data (GTK_CLIST (clist), row));
	      filter->property_filters[prop][i] = index;
	      selection = selection->next;
	    }
	}
    }

  /* Set the 'Reset Filter' button sensitive if a filter is in effect, and
     also set the label above the font list to show this as well. */
  if (default_filter)
    {
      gtk_widget_set_sensitive(fontsel->filter_button, FALSE);
      gtk_label_set_text(GTK_LABEL(fontsel->font_label), _("Font:"));
    }
  else
    {
      gtk_widget_set_sensitive(fontsel->filter_button, TRUE);
      gtk_label_set_text(GTK_LABEL(fontsel->font_label), _("Font: (Filter Applied)"));
    }
  gtk_font_selection_show_available_fonts(fontsel);
}  


/* This shows all the available fonts in the font clist. */
static void
gtk_font_selection_show_available_fonts     (GtkFontSelection *fontsel)
{
  FontInfo *font_info, *font;
  GtkFontFilter *filter;
  gint nfonts, i, j, k, row, style, font_row = -1;
  gchar font_buffer[XLFD_MAX_FIELD_LEN * 2 + 4];
  gchar *font_item;
  gboolean matched, matched_style;
  
#ifdef FONTSEL_DEBUG
  g_message("In show_available_fonts\n");
#endif
  font_info = fontsel_info->font_info;
  nfonts = fontsel_info->nfonts;
  
  /* Filter the list of fonts. */
  gtk_clist_freeze (GTK_CLIST(fontsel->font_clist));
  gtk_clist_clear (GTK_CLIST(fontsel->font_clist));
  for (i = 0; i < nfonts; i++)
    {
      font = &font_info[i];
      
      /* Check if the foundry passes through all filters. */
      matched = TRUE;
      for (k = 0; k < GTK_NUM_FONT_FILTERS; k++)
	{
	  filter = &fontsel->filters[k];

	  if (filter->property_nfilters[FOUNDRY] != 0)
	    {
	      matched = FALSE;
	      for (j = 0; j < filter->property_nfilters[FOUNDRY]; j++)
		{
		  if (font->foundry == filter->property_filters[FOUNDRY][j])
		    {
		      matched = TRUE;
		      break;
		    }
		}
	      if (!matched)
		break;
	    }
	}
      
      if (!matched)
	continue;


      /* Now check if the other properties are matched in at least one style.*/
      matched_style = FALSE;
      for (style = 0; style < font->nstyles; style++)
	{
	  if (gtk_font_selection_style_visible(fontsel, font, style))
	    {
	      matched_style = TRUE;
	      break;
	    }
	}
      if (!matched_style)
	continue;
      
      /* Insert the font in the clist. */
      if ((i > 0 && font->family == font_info[i-1].family)
	  || (i < nfonts - 1 && font->family == font_info[i+1].family))
	{
	  sprintf(font_buffer, "%s (%s)", font->family,
		  fontsel_info->properties[FOUNDRY][font->foundry]);
	  font_item = font_buffer;
	  row = gtk_clist_append(GTK_CLIST(fontsel->font_clist), &font_item);
	}
      else
	{
	  row = gtk_clist_append(GTK_CLIST(fontsel->font_clist),
				 &font->family);
	}
      gtk_clist_set_row_data(GTK_CLIST(fontsel->font_clist), row,
			     GINT_TO_POINTER (i));
      if (fontsel->font_index == i)
	font_row = row;
    }
  gtk_clist_thaw (GTK_CLIST(fontsel->font_clist));
  
  /* If the currently-selected font isn't in the new list, reset the
     selection. */
  if (font_row == -1)
    {
      fontsel->font_index = -1;
      if (fontsel->font)
	gdk_font_unref(fontsel->font);
      fontsel->font = NULL;
      gtk_entry_set_text(GTK_ENTRY(fontsel->font_entry), "");
      gtk_clist_clear (GTK_CLIST(fontsel->font_style_clist));
      gtk_entry_set_text(GTK_ENTRY(fontsel->font_style_entry), "");
      return;
    }

  gtk_clist_select_row(GTK_CLIST(fontsel->font_clist), font_row, 0);
  if (gtk_clist_row_is_visible(GTK_CLIST(fontsel->font_clist), font_row)
      != GTK_VISIBILITY_FULL)
    gtk_clist_moveto(GTK_CLIST(fontsel->font_clist), font_row, -1, 0.5, 0);

  gtk_font_selection_show_available_styles (fontsel);
  gtk_font_selection_select_best_style (fontsel, FALSE);
}


/* Returns TRUE if the style is not currently filtered out. */
static gboolean
gtk_font_selection_style_visible(GtkFontSelection *fontsel,
				 FontInfo         *font,
				 gint              style_index)
{
  FontStyle *styles, *style;
  GtkFontFilter *filter;
  guint16 value;
  gint prop, i, j;
  gboolean matched;
  gint type_filter;

  styles = &fontsel_info->font_styles[font->style_index];
  style = &styles[style_index];

  /* Check if font_type of style is filtered. */
  type_filter = fontsel->filters[GTK_FONT_FILTER_BASE].font_type
    & fontsel->filters[GTK_FONT_FILTER_USER].font_type;
  if (!(style->flags & type_filter))
    return FALSE;
  
  for (prop = 0; prop < GTK_NUM_STYLE_PROPERTIES; prop++)
    {
      value = style->properties[prop];
      
      /* Check each filter. */
      for (i = 0; i < GTK_NUM_FONT_FILTERS; i++)
	{
	  filter = &fontsel->filters[i];

	  if (filter->property_nfilters[prop] != 0)
	    {
	      matched = FALSE;
	      for (j = 0; j < filter->property_nfilters[prop]; j++)
		{
		  if (value == filter->property_filters[prop][j])
		    {
		      matched = TRUE;
		      break;
		    }
		}
	      if (!matched)
		return FALSE;
	    }
	}
    }
  return TRUE;
}


/* This resets the font type to bitmap or scalable, and sets all the filter
   clists to the wildcard '*' options. */
static void
gtk_font_selection_reset_filter	     (GtkWidget      *w,
				      GtkFontSelection *fontsel)
{
  gint prop, base_font_type;
  
  fontsel->filters[GTK_FONT_FILTER_USER].font_type = GTK_FONT_BITMAP
    | GTK_FONT_SCALABLE;

  base_font_type = fontsel->filters[GTK_FONT_FILTER_BASE].font_type;
  if (base_font_type & GTK_FONT_BITMAP)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fontsel->type_bitmaps_button), TRUE);
  if (base_font_type & GTK_FONT_SCALABLE)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fontsel->type_scalable_button), TRUE);
  if (base_font_type & GTK_FONT_SCALABLE_BITMAP)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fontsel->type_scaled_bitmaps_button), FALSE);
  
  for (prop = 0; prop < GTK_NUM_FONT_PROPERTIES; prop++)
    gtk_clist_select_row(GTK_CLIST(fontsel->filter_clists[prop]), 0, 0);
}


/* This clears the filter, showing all fonts and styles again. */
static void
gtk_font_selection_on_clear_filter     (GtkWidget      *w,
					GtkFontSelection *fontsel)
{
  gtk_font_selection_clear_filter(fontsel);
}


/* This resets the user filter, showing all fonts and styles which pass the
   base filter again. Note that the font type is set to bitmaps and scalable
   fonts - scaled bitmaps are not shown. */
static void
gtk_font_selection_clear_filter     (GtkFontSelection *fontsel)
{
  GtkFontFilter *filter;
  gint prop;
  
#ifdef FONTSEL_DEBUG
  g_message("In clear_filter\n");
#endif
  /* Clear the filter data. */
  filter = &fontsel->filters[GTK_FONT_FILTER_USER];
  filter->font_type = GTK_FONT_BITMAP | GTK_FONT_SCALABLE;
  for (prop = 0; prop < GTK_NUM_FONT_PROPERTIES; prop++)
    {
      g_free(filter->property_filters[prop]);
      filter->property_filters[prop] = NULL;
      filter->property_nfilters[prop] = 0;
    }
  
  /* Select all the '*'s on the filter page. */
  gtk_font_selection_reset_filter(NULL, fontsel);
  
  /* Update the main notebook page. */
  gtk_widget_set_sensitive(fontsel->filter_button, FALSE);
  gtk_label_set_text(GTK_LABEL(fontsel->font_label), _("Font:"));
  
  gtk_font_selection_show_available_fonts(fontsel);
}
  
  
void
gtk_font_selection_set_filter	(GtkFontSelection *fontsel,
				 GtkFontFilterType filter_type,
				 GtkFontType	   font_type,
				 gchar		 **foundries,
				 gchar		 **weights,
				 gchar		 **slants,
				 gchar		 **setwidths,
				 gchar		 **spacings,
				 gchar		 **charsets)
{
  GtkFontFilter *filter;
  gchar **filter_strings [GTK_NUM_FONT_PROPERTIES];
  gchar *filter_string;
  gchar *property, *property_alt;
  gint prop, nfilters, i, j, num_found;
  gint base_font_type, user_font_type;
  gboolean filter_set;

  /* Put them into an array so we can use a simple loop. */
  filter_strings[FOUNDRY]   = foundries;
  filter_strings[WEIGHT]    = weights;
  filter_strings[SLANT]     = slants;
  filter_strings[SET_WIDTH] = setwidths;
  filter_strings[SPACING]   = spacings;
  filter_strings[CHARSET]   = charsets;

  filter = &fontsel->filters[filter_type];
  filter->font_type = font_type;
      
  /* Free the old filter data, and insert the new. */
  for (prop = 0; prop < GTK_NUM_FONT_PROPERTIES; prop++)
    {
      g_free(filter->property_filters[prop]);
      filter->property_filters[prop] = NULL;
      filter->property_nfilters[prop] = 0;
      
      if (filter_strings[prop])
	{
	  /* Count how many items in the new array. */
	  nfilters = 0;
	  while (filter_strings[prop][nfilters])
	    nfilters++;

	  filter->property_filters[prop] = g_new(guint16, nfilters);
	  filter->property_nfilters[prop] = 0;

	  /* Now convert the strings to property indices. */
	  num_found = 0;
	  for (i = 0; i < nfilters; i++)
	    {
	      filter_string = filter_strings[prop][i];
	      for (j = 0; j < fontsel_info->nproperties[prop]; j++)
		{
		  property = _(fontsel_info->properties[prop][j]);
		  property_alt = NULL;
		  if (prop == SLANT)
		    property_alt = gtk_font_selection_expand_slant_code(property);
		  else if (prop == SPACING)
		    property_alt = gtk_font_selection_expand_spacing_code(property);
		  if (!strcmp (filter_string, property)
		      || (property_alt && !strcmp (filter_string, property_alt)))
		    {
		      filter->property_filters[prop][num_found] = j;
		      num_found++;
		      break;
		    }
		}
	    }
	  filter->property_nfilters[prop] = num_found;
	}
    }

  /* Now set the clists on the filter page according to the new filter. */
  gtk_font_selection_update_filter_lists (fontsel);

  if (filter_type == GTK_FONT_FILTER_BASE)
    {
      user_font_type = fontsel->filters[GTK_FONT_FILTER_USER].font_type;
      if (font_type & GTK_FONT_BITMAP)
	{
	  gtk_widget_set_sensitive (fontsel->type_bitmaps_button, TRUE);
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fontsel->type_bitmaps_button), user_font_type & GTK_FONT_BITMAP);
	}
      else
	{
	  gtk_widget_set_sensitive (fontsel->type_bitmaps_button, FALSE);
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fontsel->type_bitmaps_button), FALSE);
	}
      
      if (font_type & GTK_FONT_SCALABLE)
	{
	  gtk_widget_set_sensitive (fontsel->type_scalable_button, TRUE);
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fontsel->type_scalable_button), user_font_type & GTK_FONT_SCALABLE);
	}
      else
	{
	  gtk_widget_set_sensitive (fontsel->type_scalable_button, FALSE);
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fontsel->type_scalable_button), FALSE);
	}

      if (font_type & GTK_FONT_SCALABLE_BITMAP)
	{
	  gtk_widget_set_sensitive (fontsel->type_scaled_bitmaps_button, TRUE);
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fontsel->type_scaled_bitmaps_button), user_font_type & GTK_FONT_SCALABLE_BITMAP);
	}
      else
	{
	  gtk_widget_set_sensitive (fontsel->type_scaled_bitmaps_button, FALSE);
	  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fontsel->type_scaled_bitmaps_button), FALSE);
	}
    }
  else
    {
      base_font_type = fontsel->filters[GTK_FONT_FILTER_BASE].font_type;
      if (base_font_type & GTK_FONT_BITMAP)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fontsel->type_bitmaps_button), font_type & GTK_FONT_BITMAP);

      if (base_font_type & GTK_FONT_SCALABLE)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fontsel->type_scalable_button), font_type & GTK_FONT_SCALABLE);

      if (base_font_type & GTK_FONT_SCALABLE_BITMAP)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fontsel->type_scaled_bitmaps_button), font_type & GTK_FONT_SCALABLE_BITMAP);

      /* If the user filter is not the default, make the 'Reset Filter' button
	 sensitive. */
      filter_set = FALSE;
      if (font_type != (GTK_FONT_BITMAP | GTK_FONT_SCALABLE))
	filter_set = TRUE;
      for (prop = 0; prop < GTK_NUM_FONT_PROPERTIES; prop++)
	{
	  if (filter->property_nfilters[prop] != 0)
	    filter_set = TRUE;
	}
      if (filter_set)
	gtk_widget_set_sensitive (fontsel->filter_button, TRUE);
    }

  gtk_font_selection_show_available_fonts (fontsel);
}


/* This sets the colour of each property in the filter clists according to
   the base filter. i.e. Filtered properties are shown as insensitive. */
static void
gtk_font_selection_update_filter_lists (GtkFontSelection *fontsel)
{
  GtkWidget *clist;
  GdkColor *inactive_fg, *inactive_bg, *fg, *bg;
  gint prop, row, index;

  /* We have to make sure the clist is realized to use the colours. */
  for (prop = 0; prop < GTK_NUM_FONT_PROPERTIES; prop++)
    {
      clist = fontsel->filter_clists[prop];
      gtk_widget_realize (clist);
      inactive_fg = &clist->style->fg[GTK_STATE_INSENSITIVE];
      inactive_bg = &clist->style->bg[GTK_STATE_INSENSITIVE];
      for (row = 1; row < GTK_CLIST(clist)->rows; row++)
	{
	  index = GPOINTER_TO_INT (gtk_clist_get_row_data(GTK_CLIST(clist),
							   row));
	  /* Set the colour according to the base filter. */
	  if (gtk_font_selection_filter_state (fontsel, GTK_FONT_FILTER_BASE,
					       prop, index) == NOT_FILTERED)
	    {
	      fg = inactive_fg;
	      bg = inactive_bg;
	    }
	  else
	    {
	      fg = NULL;
	      bg = NULL;
	    }
	  gtk_clist_set_foreground(GTK_CLIST(clist), row, fg);
	  gtk_clist_set_background(GTK_CLIST(clist), row, bg);

	  /* Set the selection state according to the user filter. */
	  if (gtk_font_selection_filter_state (fontsel, GTK_FONT_FILTER_USER,
					       prop, index) == FILTERED
	      && fg == NULL)
	    gtk_clist_select_row (GTK_CLIST (clist), row, 0);
	  else
	    gtk_clist_unselect_row (GTK_CLIST (clist), row, 0);
	}
    }
}


/* Returns whether a property value is in the filter or not, or if the
   property has no filter set. */
static GtkFontPropertyFilterState
gtk_font_selection_filter_state (GtkFontSelection *fontsel,
				 GtkFontFilterType filter_type,
				 gint		  property,
				 gint		  index)
{
  GtkFontFilter *filter;
  gint i;

  filter = &fontsel->filters[filter_type];
  if (filter->property_nfilters[property] == 0)
    return NOT_SET;

  for (i = 0; i < filter->property_nfilters[property]; i++)
    {
      if (filter->property_filters[property][i] == index)
	return FILTERED;
    }
  return NOT_FILTERED;
}


/*****************************************************************************
 * These functions all deal with creating the main class arrays containing
 * the data about all available fonts.
 *****************************************************************************/
static void
gtk_font_selection_get_fonts (void)
{
  gchar **xfontnames;
  GSList **fontnames;
  gchar *fontname;
  GSList * temp_list;
  gint num_fonts;
  gint i, prop, style, size;
  gint npixel_sizes = 0, npoint_sizes = 0;
  FontInfo *font;
  FontStyle *current_style, *prev_style, *tmp_style;
  gboolean matched_style, found_size;
  gint pixels, points, res_x, res_y;
  gchar field_buffer[XLFD_MAX_FIELD_LEN];
  gchar *field;
  guint8 flags;
  guint16 *pixel_sizes, *point_sizes, *tmp_sizes;
  
  fontsel_info = g_new (GtkFontSelInfo, 1);
  
  /* Get a maximum of MAX_FONTS fontnames from the X server.
     Use "-*" as the pattern rather than "-*-*-*-*-*-*-*-*-*-*-*-*-*-*" since
     the latter may result in fonts being returned which don't actually exist.
     xlsfonts also uses "*" so I think it's OK. "-*" gets rid of aliases. */
  xfontnames = XListFonts (GDK_DISPLAY(), "-*", MAX_FONTS, &num_fonts);
  /* Output a warning if we actually get MAX_FONTS fonts. */
  if (num_fonts == MAX_FONTS)
    g_warning(_("MAX_FONTS exceeded. Some fonts may be missing."));
  
  /* The maximum size of all these tables is the number of font names
     returned. We realloc them later when we know exactly how many
     unique entries there are. */
  fontsel_info->font_info = g_new (FontInfo, num_fonts);
  fontsel_info->font_styles = g_new (FontStyle, num_fonts);
  fontsel_info->pixel_sizes = g_new (guint16, num_fonts);
  fontsel_info->point_sizes = g_new (guint16, num_fonts);
  
  fontnames = g_new (GSList*, num_fonts);
  
  /* Create the initial arrays for the property value strings, though they
     may be realloc'ed later. Put the wildcard '*' in the first elements. */
  for (prop = 0; prop < GTK_NUM_FONT_PROPERTIES; prop++)
    {
      fontsel_info->properties[prop] = g_new(gchar*, PROPERTY_ARRAY_INCREMENT);
      fontsel_info->space_allocated[prop] = PROPERTY_ARRAY_INCREMENT;
      fontsel_info->nproperties[prop] = 1;
      fontsel_info->properties[prop][0] = "*";
    }
  
  
  /* Insert the font families into the main table, sorted by family and
     foundry (fonts with different foundries are placed in seaparate FontInfos.
     All fontnames in each family + foundry are placed into the fontnames
     array of lists. */
  fontsel_info->nfonts = 0;
  for (i = 0; i < num_fonts; i++)
    {
#ifdef FONTSEL_DEBUG
      g_message("%s\n", xfontnames[i]);
#endif
      if (gtk_font_selection_is_xlfd_font_name (xfontnames[i]))
	gtk_font_selection_insert_font (fontnames, &fontsel_info->nfonts, xfontnames[i]);
      else
	{
#ifdef FONTSEL_DEBUG
	  g_warning("Skipping invalid font: %s", xfontnames[i]);
#endif
	}
    }
  
  
  /* Since many font names will be in the same FontInfo not all of the
     allocated FontInfo table will be used, so we will now reallocate it
     with the real size. */
  fontsel_info->font_info = g_realloc(fontsel_info->font_info,
				      sizeof(FontInfo) * fontsel_info->nfonts);
  
  
  /* Now we work out which choices of weight/slant etc. are valid for each
     font. */
  fontsel_info->nstyles = 0;
  current_style = fontsel_info->font_styles;
  for (i = 0; i < fontsel_info->nfonts; i++)
    {
      font = &fontsel_info->font_info[i];
      
      /* Use the next free position in the styles array. */
      font->style_index = fontsel_info->nstyles;
      
      /* Now step through each of the fontnames with this family, and create
	 a style for each fontname. Each style contains the index into the
	 weights/slants etc. arrays, and a number of pixel/point sizes. */
      style = 0;
      temp_list = fontnames[i];
      while (temp_list)
	{
	  fontname = temp_list->data;
	  temp_list = temp_list->next;
	  
	  for (prop = 0; prop < GTK_NUM_STYLE_PROPERTIES; prop++)
	    {
	      current_style->properties[prop]
		= gtk_font_selection_insert_field (fontname, prop);
	    }
	  current_style->pixel_sizes_index = npixel_sizes;
	  current_style->npixel_sizes = 0;
	  current_style->point_sizes_index = npoint_sizes;
	  current_style->npoint_sizes = 0;
	  current_style->flags = 0;
	  
	  
	  field = gtk_font_selection_get_xlfd_field (fontname, XLFD_PIXELS,
						     field_buffer);
	  pixels = atoi(field);
	  
	  field = gtk_font_selection_get_xlfd_field (fontname, XLFD_POINTS,
						     field_buffer);
	  points = atoi(field);
	  
	  field = gtk_font_selection_get_xlfd_field (fontname,
						     XLFD_RESOLUTION_X,
						     field_buffer);
	  res_x = atoi(field);
	  
	  field = gtk_font_selection_get_xlfd_field (fontname,
						     XLFD_RESOLUTION_Y,
						     field_buffer);
	  res_y = atoi(field);
	  
	  if (pixels == 0 && points == 0)
	    {
	      if (res_x == 0 && res_y == 0)
		flags = GTK_FONT_SCALABLE;
	      else
		flags = GTK_FONT_SCALABLE_BITMAP;
	    }
	  else
	    flags = GTK_FONT_BITMAP;
	  
	  /* Now we check to make sure that the style is unique. If it isn't
	     we forget it. */
	  prev_style = fontsel_info->font_styles + font->style_index;
	  matched_style = FALSE;
	  while (prev_style < current_style)
	    {
	      matched_style = TRUE;
	      for (prop = 0; prop < GTK_NUM_STYLE_PROPERTIES; prop++)
		{
		  if (prev_style->properties[prop]
		      != current_style->properties[prop])
		    {
		      matched_style = FALSE;
		      break;
		    }
		}
	      if (matched_style)
		break;
	      prev_style++;
	    }
	  
	  /* If we matched an existing style, we need to add the pixels &
	     point sizes to the style. If not, we insert the pixel & point
	     sizes into our new style. Note that we don't add sizes for
	     scalable fonts. */
	  if (matched_style)
	    {
	      prev_style->flags |= flags;
	      if (flags == GTK_FONT_BITMAP)
		{
		  pixel_sizes = fontsel_info->pixel_sizes
		    + prev_style->pixel_sizes_index;
		  found_size = FALSE;
		  for (size = 0; size < prev_style->npixel_sizes; size++)
		    {
		      if (pixels == *pixel_sizes)
			{
			  found_size = TRUE;
			  break;
			}
		      else if (pixels < *pixel_sizes)
			break;
		      pixel_sizes++;
		    }
		  /* We need to move all the following pixel sizes up, and also
		     update the indexes of any following styles. */
		  if (!found_size)
		    {
		      for (tmp_sizes = fontsel_info->pixel_sizes + npixel_sizes;
			   tmp_sizes > pixel_sizes; tmp_sizes--)
			*tmp_sizes = *(tmp_sizes - 1);
		      
		      *pixel_sizes = pixels;
		      npixel_sizes++;
		      prev_style->npixel_sizes++;
		      
		      tmp_style = prev_style + 1;
		      while (tmp_style < current_style)
			{
			  tmp_style->pixel_sizes_index++;
			  tmp_style++;
			}
		    }
		  
		  point_sizes = fontsel_info->point_sizes
		    + prev_style->point_sizes_index;
		  found_size = FALSE;
		  for (size = 0; size < prev_style->npoint_sizes; size++)
		    {
		      if (points == *point_sizes)
			{
			  found_size = TRUE;
			  break;
			}
		      else if (points < *point_sizes)
			break;
		      point_sizes++;
		    }
		  /* We need to move all the following point sizes up, and also
		     update the indexes of any following styles. */
		  if (!found_size)
		    {
		      for (tmp_sizes = fontsel_info->point_sizes + npoint_sizes;
			   tmp_sizes > point_sizes; tmp_sizes--)
			*tmp_sizes = *(tmp_sizes - 1);
		      
		      *point_sizes = points;
		      npoint_sizes++;
		      prev_style->npoint_sizes++;
		      
		      tmp_style = prev_style + 1;
		      while (tmp_style < current_style)
			{
			  tmp_style->point_sizes_index++;
			  tmp_style++;
			}
		    }
		}
	    }
	  else
	    {
	      current_style->flags = flags;
	      if (flags == GTK_FONT_BITMAP)
		{
		  fontsel_info->pixel_sizes[npixel_sizes++] = pixels;
		  current_style->npixel_sizes = 1;
		  fontsel_info->point_sizes[npoint_sizes++] = points;
		  current_style->npoint_sizes = 1;
		}
	      style++;
	      fontsel_info->nstyles++;
	      current_style++;
	    }
	}
      g_slist_free(fontnames[i]);
      
      /* Set nstyles to the real value, minus duplicated fontnames.
	 Note that we aren't using all the allocated memory if fontnames are
	 duplicated. */
      font->nstyles = style;
    }
  
  /* Since some repeated styles may be skipped we won't have used all the
     allocated space, so we will now reallocate it with the real size. */
  fontsel_info->font_styles = g_realloc(fontsel_info->font_styles,
					sizeof(FontStyle) * fontsel_info->nstyles);
  fontsel_info->pixel_sizes = g_realloc(fontsel_info->pixel_sizes,
					sizeof(guint16) * npixel_sizes);
  fontsel_info->point_sizes = g_realloc(fontsel_info->point_sizes,
					sizeof(guint16) * npoint_sizes);
  g_free(fontnames);
  XFreeFontNames (xfontnames);
  
  
  /* Debugging Output */
  /* This outputs all FontInfos. */
#ifdef FONTSEL_DEBUG
  g_message("\n\n Font Family           Weight    Slant     Set Width Spacing   Charset\n\n");
  for (i = 0; i < fontsel_info->nfonts; i++)
    {
      FontInfo *font = &fontsel_info->font_info[i];
      FontStyle *styles = fontsel_info->font_styles + font->style_index;
      for (style = 0; style < font->nstyles; style++)
	{
	  g_message("%5i %-16.16s ", i, font->family);
	  for (prop = 0; prop < GTK_NUM_STYLE_PROPERTIES; prop++)
	    g_message("%-9.9s ",
		      fontsel_info->properties[prop][styles->properties[prop]]);
	  g_message("\n      ");
	  
	  if (styles->flags & GTK_FONT_BITMAP)
	    g_message("Bitmapped font  ");
	  if (styles->flags & GTK_FONT_SCALABLE)
	    g_message("Scalable font  ");
	  if (styles->flags & GTK_FONT_SCALABLE_BITMAP)
	    g_message("Scalable-Bitmapped font  ");
	  g_message("\n");
	  
	  if (styles->npixel_sizes)
	    {
	      g_message("      Pixel sizes: ");
	      tmp_sizes = fontsel_info->pixel_sizes + styles->pixel_sizes_index;
	      for (size = 0; size < styles->npixel_sizes; size++)
		g_message("%i ", *tmp_sizes++);
	      g_message("\n");
	    }
	  
	  if (styles->npoint_sizes)
	    {
	      g_message("      Point sizes: ");
	      tmp_sizes = fontsel_info->point_sizes + styles->point_sizes_index;
	      for (size = 0; size < styles->npoint_sizes; size++)
		g_message("%i ", *tmp_sizes++);
	      g_message("\n");
	    }
	  
	  g_message("\n");
	  styles++;
	}
    }
  /* This outputs all available properties. */
  for (prop = 0; prop < GTK_NUM_FONT_PROPERTIES; prop++)
    {
      g_message("Property: %s\n", xlfd_field_names[xlfd_index[prop]]);
      for (i = 0; i < fontsel_info->nproperties[prop]; i++)
        g_message("  %s\n", fontsel_info->properties[prop][i]);
    }
#endif
}

/* This inserts the given fontname into the FontInfo table.
   If a FontInfo already exists with the same family and foundry, then the
   fontname is added to the FontInfos list of fontnames, else a new FontInfo
   is created and inserted in alphabetical order in the table. */
static void
gtk_font_selection_insert_font (GSList		      *fontnames[],
				gint		      *ntable,
				gchar		      *fontname)
{
  FontInfo *table;
  FontInfo temp_info;
  GSList *temp_fontname;
  gchar *family;
  gboolean family_exists = FALSE;
  gint foundry;
  gint lower, upper;
  gint middle, cmp;
  gchar family_buffer[XLFD_MAX_FIELD_LEN];
  
  table = fontsel_info->font_info;
  
  /* insert a fontname into a table */
  family = gtk_font_selection_get_xlfd_field (fontname, XLFD_FAMILY,
					      family_buffer);
  if (!family)
    return;
  
  foundry = gtk_font_selection_insert_field (fontname, FOUNDRY);
  
  lower = 0;
  if (*ntable > 0)
    {
      /* Do a binary search to determine if we have already encountered
       *  a font with this family & foundry. */
      upper = *ntable;
      while (lower < upper)
	{
	  middle = (lower + upper) >> 1;
	  
	  cmp = strcmp (family, table[middle].family);
	  /* If the family matches we sort by the foundry. */
	  if (cmp == 0)
	    {
	      family_exists = TRUE;
	      family = table[middle].family;
	      cmp = strcmp(fontsel_info->properties[FOUNDRY][foundry],
			   fontsel_info->properties[FOUNDRY][table[middle].foundry]);
	    }
	  
	  if (cmp == 0)
	    {
	      fontnames[middle] = g_slist_prepend (fontnames[middle],
						   fontname);
	      return;
	    }
	  else if (cmp < 0)
	    upper = middle;
	  else
	    lower = middle+1;
	}
    }
  
  /* Add another entry to the table for this new font family */
  temp_info.family = family_exists ? family : g_strdup(family);
  temp_info.foundry = foundry;
  temp_fontname = g_slist_prepend (NULL, fontname);
  
  (*ntable)++;
  
  /* Quickly insert the entry into the table in sorted order
   *  using a modification of insertion sort and the knowledge
   *  that the entries proper position in the table was determined
   *  above in the binary search and is contained in the "lower"
   *  variable. */
  if (*ntable > 1)
    {
      upper = *ntable - 1;
      while (lower != upper)
	{
	  table[upper] = table[upper-1];
	  fontnames[upper] = fontnames[upper-1];
	  upper--;
	}
    }
  table[lower] = temp_info;
  fontnames[lower] = temp_fontname;
}


/* This checks that the specified field of the given fontname is in the
   appropriate properties array. If not it is added. Thus eventually we get
   arrays of all possible weights/slants etc. It returns the array index. */
static gint
gtk_font_selection_insert_field (gchar		       *fontname,
				 gint			prop)
{
  gchar field_buffer[XLFD_MAX_FIELD_LEN];
  gchar *field;
  guint16 index;
  
  field = gtk_font_selection_get_xlfd_field (fontname, xlfd_index[prop],
					     field_buffer);
  if (!field)
    return 0;
  
  /* If the field is already in the array just return its index. */
  for (index = 0; index < fontsel_info->nproperties[prop]; index++)
    if (!strcmp(field, fontsel_info->properties[prop][index]))
      return index;
  
  /* Make sure we have enough space to add the field. */
  if (fontsel_info->nproperties[prop] == fontsel_info->space_allocated[prop])
    {
      fontsel_info->space_allocated[prop] += PROPERTY_ARRAY_INCREMENT;
      fontsel_info->properties[prop] = g_realloc(fontsel_info->properties[prop],
						 sizeof(gchar*)
						 * fontsel_info->space_allocated[prop]);
    }
  
  /* Add the new field. */
  index = fontsel_info->nproperties[prop];
  fontsel_info->properties[prop][index] = g_strdup(field);
  fontsel_info->nproperties[prop]++;
  return index;
}


/*****************************************************************************
 * These functions are the main public interface for getting/setting the font.
 *****************************************************************************/

GdkFont*
gtk_font_selection_get_font (GtkFontSelection *fontsel)
{
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), NULL);

  gtk_font_selection_update_size (fontsel);
  
  return fontsel->font;
}


gchar *
gtk_font_selection_get_font_name (GtkFontSelection *fontsel)
{
  FontInfo *font;
  gchar *family_str, *foundry_str;
  gchar *property_str[GTK_NUM_STYLE_PROPERTIES];
  gint prop;
  
  g_return_val_if_fail (fontsel != NULL, NULL);
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), NULL);
  
  gtk_font_selection_update_size (fontsel);
  
  /* If no family has been selected return NULL. */
  if (fontsel->font_index == -1)
    return NULL;
  
  font = &fontsel_info->font_info[fontsel->font_index];
  family_str = font->family;
  foundry_str = fontsel_info->properties[FOUNDRY][font->foundry];
  /* some fonts have a (nil) foundry */
  if (strcmp (foundry_str, "(nil)") == 0)
    foundry_str = "";
    
  for (prop = 0; prop < GTK_NUM_STYLE_PROPERTIES; prop++)
    {
      property_str[prop] = fontsel_info->properties[prop][fontsel->property_values[prop]];
      if (strcmp (property_str[prop], "(nil)") == 0)
	property_str[prop] = "";
    }
  
  return gtk_font_selection_create_xlfd (fontsel->size,
					 fontsel->metric,
					 foundry_str,
					 family_str,
					 property_str[WEIGHT],
					 property_str[SLANT],
					 property_str[SET_WIDTH],
					 property_str[SPACING],
					 property_str[CHARSET]);
}


/* This sets the current font, selecting the appropriate clist rows.
   First we check the fontname is valid and try to find the font family
   - i.e. the name in the main list. If we can't find that, then just return.
   Next we try to set each of the properties according to the fontname.
   Finally we select the font family & style in the clists. */
gboolean
gtk_font_selection_set_font_name (GtkFontSelection *fontsel,
				  const gchar      *fontname)
{
  gchar *family, *field;
  gint index, prop, size, row;
  guint16 foundry, value;
  gchar family_buffer[XLFD_MAX_FIELD_LEN];
  gchar field_buffer[XLFD_MAX_FIELD_LEN];
  gchar buffer[16];
  
  g_return_val_if_fail (fontsel != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), FALSE);
  g_return_val_if_fail (fontname != NULL, FALSE);
  
  /* Check it is a valid fontname. */
  if (!gtk_font_selection_is_xlfd_font_name(fontname))
    return FALSE;
  
  family = gtk_font_selection_get_xlfd_field (fontname, XLFD_FAMILY,
					      family_buffer);
  if (!family)
    return FALSE;
  
  field = gtk_font_selection_get_xlfd_field (fontname, XLFD_FOUNDRY,
					     field_buffer);
  foundry = gtk_font_selection_field_to_index (fontsel_info->properties[FOUNDRY],
					       fontsel_info->nproperties[FOUNDRY],
					       field);
  
  index = gtk_font_selection_find_font(fontsel, family, foundry);
  if (index == -1)
    return FALSE;
  
  /* Convert the property fields into indices and set them. */
  for (prop = 0; prop < GTK_NUM_STYLE_PROPERTIES; prop++) 
    {
      field = gtk_font_selection_get_xlfd_field (fontname, xlfd_index[prop],
						 field_buffer);
      value = gtk_font_selection_field_to_index (fontsel_info->properties[prop],
						 fontsel_info->nproperties[prop],
						 field);
      fontsel->property_values[prop] = value;
    }
  
  field = gtk_font_selection_get_xlfd_field (fontname, XLFD_POINTS,
					     field_buffer);
  size = atoi(field);
  if (size > 0)
    {
      if (size < 20)
	size = 20;
      fontsel->size = fontsel->selected_size = size;
      fontsel->metric = GTK_FONT_METRIC_POINTS;
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fontsel->points_button),
				  TRUE);
      if (size % 10 == 0)
	sprintf (buffer, "%i", size / 10);
      else
	sprintf (buffer, "%i.%i", size / 10, size % 10);
    }
  else
    {
      field = gtk_font_selection_get_xlfd_field (fontname, XLFD_PIXELS,
						 field_buffer);
      size = atoi(field);
      if (size < 2)
	size = 2;
      fontsel->size = fontsel->selected_size = size;
      fontsel->metric = GTK_FONT_METRIC_PIXELS;
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fontsel->pixels_button),
				  TRUE);
      sprintf (buffer, "%i", size);
    }
  gtk_entry_set_text (GTK_ENTRY (fontsel->size_entry), buffer);
  
  /* Clear any current filter. */
  gtk_font_selection_clear_filter(fontsel);
  
  /* Now find the best style match. */
  fontsel->font_index = index;
  row = gtk_clist_find_row_from_data (GTK_CLIST (fontsel->font_clist),
				      GINT_TO_POINTER (index));
  if (row != -1)
    {
      gtk_clist_select_row (GTK_CLIST (fontsel->font_clist), row, 0);
      if (GTK_WIDGET_MAPPED (fontsel->font_clist))
	gtk_clist_moveto (GTK_CLIST (fontsel->font_clist), row, -1, 0.5, 0);
    }
  
  gtk_font_selection_show_available_styles (fontsel);
  /* This will load the font. */
  gtk_font_selection_select_best_style (fontsel, FALSE);
  
  return TRUE;
}


/* Returns the index of the given family, or -1 if not found */
static gint
gtk_font_selection_find_font (GtkFontSelection *fontsel,
			      gchar	       *family,
			      guint16		foundry)
{
  FontInfo *font_info;
  gint lower, upper, middle = -1, cmp, nfonts;
  gint found_family = -1;
  
  font_info = fontsel_info->font_info;
  nfonts = fontsel_info->nfonts;
  if (nfonts == 0)
    return -1;
  
  /* Do a binary search to find the font family. */
  lower = 0;
  upper = nfonts;
  while (lower < upper)
    {
      middle = (lower + upper) >> 1;
      
      cmp = strcmp (family, font_info[middle].family);
      if (cmp == 0)
	{
	  found_family = middle;
	  cmp = strcmp(fontsel_info->properties[FOUNDRY][foundry],
		       fontsel_info->properties[FOUNDRY][font_info[middle].foundry]);
	}

      if (cmp == 0)
	return middle;
      else if (cmp < 0)
	upper = middle;
      else if (cmp > 0)
	lower = middle+1;
    }
  
  /* We couldn't find the family and foundry, but we may have just found the
     family, so we return that. */
  return found_family;
}


/* This returns the text in the preview entry. You should copy the returned
   text if you need it. */
gchar*
gtk_font_selection_get_preview_text  (GtkFontSelection *fontsel)
{
  return gtk_entry_get_text(GTK_ENTRY(fontsel->preview_entry));
}


/* This sets the text in the preview entry. */
void
gtk_font_selection_set_preview_text  (GtkFontSelection *fontsel,
				      const gchar	  *text)
{
  gtk_entry_set_text(GTK_ENTRY(fontsel->preview_entry), text);
}


/*****************************************************************************
 * These functions all deal with X Logical Font Description (XLFD) fontnames.
 * See the freely available documentation about this.
 *****************************************************************************/

/*
 * Returns TRUE if the fontname is a valid XLFD.
 * (It just checks if the number of dashes is 14, and that each
 * field < XLFD_MAX_FIELD_LEN  characters long - that's not in the XLFD but it
 * makes it easier for me).
 */
static gboolean
gtk_font_selection_is_xlfd_font_name (const gchar *fontname)
{
  gint i = 0;
  gint field_len = 0;
  
  while (*fontname)
    {
      if (*fontname++ == '-')
	{
	  if (field_len > XLFD_MAX_FIELD_LEN) return FALSE;
	  field_len = 0;
	  i++;
	}
      else
	field_len++;
    }
  
  return (i == 14) ? TRUE : FALSE;
}

/*
 * This fills the buffer with the specified field from the X Logical Font
 * Description name, and returns it. If fontname is NULL or the field is
 * longer than XFLD_MAX_FIELD_LEN it returns NULL.
 * Note: For the charset field, we also return the encoding, e.g. 'iso8859-1'.
 */
static gchar*
gtk_font_selection_get_xlfd_field (const gchar *fontname,
				   FontField    field_num,
				   gchar       *buffer)
{
  const gchar *t1, *t2;
  gint countdown, len, num_dashes;
  
  if (!fontname)
    return NULL;
  
  /* we assume this is a valid fontname...that is, it has 14 fields */
  
  countdown = field_num;
  t1 = fontname;
  while (*t1 && (countdown >= 0))
    if (*t1++ == '-')
      countdown--;
  
  num_dashes = (field_num == XLFD_CHARSET) ? 2 : 1;
  for (t2 = t1; *t2; t2++)
    { 
      if (*t2 == '-' && --num_dashes == 0)
	break;
    }
  
  if (t1 != t2)
    {
      /* Check we don't overflow the buffer */
      len = (long) t2 - (long) t1;
      if (len > XLFD_MAX_FIELD_LEN - 1)
	return NULL;
      strncpy (buffer, t1, len);
      buffer[len] = 0;

      /* Convert to lower case. */
      g_strdown (buffer);
    }
  else
    strcpy(buffer, "(nil)");
  
  return buffer;
}

/*
 * This returns a X Logical Font Description font name, given all the pieces.
 * Note: this retval must be freed by the caller.
 */
static gchar *
gtk_font_selection_create_xlfd (gint		  size,
				GtkFontMetricType metric,
				gchar		  *foundry,
				gchar		  *family,
				gchar		  *weight,
				gchar		  *slant,
				gchar		  *set_width,
				gchar		  *spacing,
				gchar		  *charset)
{
  gchar buffer[16];
  gchar *pixel_size = "*", *point_size = "*", *fontname;
  
  if (size <= 0)
    return NULL;
  
  sprintf (buffer, "%d", (int) size);
  if (metric == GTK_FONT_METRIC_PIXELS)
    pixel_size = buffer;
  else
    point_size = buffer;
  
  fontname = g_strdup_printf("-%s-%s-%s-%s-%s-*-%s-%s-*-*-%s-*-%s",
			     foundry, family, weight, slant, set_width,
			     pixel_size, point_size, spacing, charset);
  return fontname;
}



/*****************************************************************************
 * GtkFontSelectionDialog
 *****************************************************************************/

guint
gtk_font_selection_dialog_get_type (void)
{
  static guint font_selection_dialog_type = 0;
  
  if (!font_selection_dialog_type)
    {
      GtkTypeInfo fontsel_diag_info =
      {
	"GtkFontSelectionDialog",
	sizeof (GtkFontSelectionDialog),
	sizeof (GtkFontSelectionDialogClass),
	(GtkClassInitFunc) gtk_font_selection_dialog_class_init,
	(GtkObjectInitFunc) gtk_font_selection_dialog_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };
      
      font_selection_dialog_type = gtk_type_unique (GTK_TYPE_WINDOW, &fontsel_diag_info);
    }
  
  return font_selection_dialog_type;
}

static void
gtk_font_selection_dialog_class_init (GtkFontSelectionDialogClass *klass)
{
  GtkObjectClass *object_class;
  
  object_class = (GtkObjectClass*) klass;
  
  font_selection_dialog_parent_class = gtk_type_class (GTK_TYPE_WINDOW);
}

static void
gtk_font_selection_dialog_init (GtkFontSelectionDialog *fontseldiag)
{
  fontseldiag->dialog_width = -1;
  fontseldiag->auto_resize = TRUE;
  
  gtk_widget_set_events(GTK_WIDGET(fontseldiag), GDK_STRUCTURE_MASK);
  gtk_signal_connect (GTK_OBJECT (fontseldiag), "configure_event",
		      (GtkSignalFunc) gtk_font_selection_dialog_on_configure,
		      fontseldiag);
  
  gtk_container_set_border_width (GTK_CONTAINER (fontseldiag), 4);
  gtk_window_set_policy(GTK_WINDOW(fontseldiag), FALSE, TRUE, TRUE);
  
  fontseldiag->main_vbox = gtk_vbox_new (FALSE, 4);
  gtk_widget_show (fontseldiag->main_vbox);
  gtk_container_add (GTK_CONTAINER (fontseldiag), fontseldiag->main_vbox);
  
  fontseldiag->fontsel = gtk_font_selection_new();
  gtk_widget_show (fontseldiag->fontsel);
  gtk_box_pack_start (GTK_BOX (fontseldiag->main_vbox),
		      fontseldiag->fontsel, TRUE, TRUE, 0);
  
  /* Create the action area */
  fontseldiag->action_area = gtk_hbutton_box_new ();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(fontseldiag->action_area),
			    GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing(GTK_BUTTON_BOX(fontseldiag->action_area), 5);
  gtk_box_pack_start (GTK_BOX (fontseldiag->main_vbox),
		      fontseldiag->action_area, FALSE, FALSE, 0);
  gtk_widget_show (fontseldiag->action_area);
  
  fontseldiag->ok_button = gtk_button_new_with_label(_("OK"));
  GTK_WIDGET_SET_FLAGS (fontseldiag->ok_button, GTK_CAN_DEFAULT);
  gtk_widget_show(fontseldiag->ok_button);
  gtk_box_pack_start (GTK_BOX (fontseldiag->action_area),
		      fontseldiag->ok_button, TRUE, TRUE, 0);
  gtk_widget_grab_default (fontseldiag->ok_button);
  
  fontseldiag->apply_button = gtk_button_new_with_label(_("Apply"));
  GTK_WIDGET_SET_FLAGS (fontseldiag->apply_button, GTK_CAN_DEFAULT);
  /*gtk_widget_show(fontseldiag->apply_button);*/
  gtk_box_pack_start (GTK_BOX(fontseldiag->action_area),
		      fontseldiag->apply_button, TRUE, TRUE, 0);
  
  fontseldiag->cancel_button = gtk_button_new_with_label(_("Cancel"));
  GTK_WIDGET_SET_FLAGS (fontseldiag->cancel_button, GTK_CAN_DEFAULT);
  gtk_widget_show(fontseldiag->cancel_button);
  gtk_box_pack_start (GTK_BOX(fontseldiag->action_area),
		      fontseldiag->cancel_button, TRUE, TRUE, 0);
  
  
}

GtkWidget*
gtk_font_selection_dialog_new	(const gchar	  *title)
{
  GtkFontSelectionDialog *fontseldiag;
  
  fontseldiag = gtk_type_new (GTK_TYPE_FONT_SELECTION_DIALOG);
  gtk_window_set_title (GTK_WINDOW (fontseldiag),
			title ? title : _("Font Selection"));
  
  return GTK_WIDGET (fontseldiag);
}

gchar*
gtk_font_selection_dialog_get_font_name	(GtkFontSelectionDialog *fsd)
{
  return gtk_font_selection_get_font_name(GTK_FONT_SELECTION(fsd->fontsel));
}

GdkFont*
gtk_font_selection_dialog_get_font	(GtkFontSelectionDialog *fsd)
{
  return gtk_font_selection_get_font(GTK_FONT_SELECTION(fsd->fontsel));
}

gboolean
gtk_font_selection_dialog_set_font_name	(GtkFontSelectionDialog *fsd,
					 const gchar	  *fontname)
{
  return gtk_font_selection_set_font_name(GTK_FONT_SELECTION(fsd->fontsel),
					  fontname);
}

void
gtk_font_selection_dialog_set_filter	(GtkFontSelectionDialog *fsd,
					 GtkFontFilterType filter_type,
					 GtkFontType	   font_type,
					 gchar		 **foundries,
					 gchar		 **weights,
					 gchar		 **slants,
					 gchar		 **setwidths,
					 gchar		 **spacings,
					 gchar		 **charsets)
{
  gtk_font_selection_set_filter (GTK_FONT_SELECTION (fsd->fontsel),
				 filter_type, font_type,
				 foundries, weights, slants, setwidths,
				 spacings, charsets);
}

gchar*
gtk_font_selection_dialog_get_preview_text (GtkFontSelectionDialog *fsd)
{
  return gtk_font_selection_get_preview_text(GTK_FONT_SELECTION(fsd->fontsel));
}

void
gtk_font_selection_dialog_set_preview_text (GtkFontSelectionDialog *fsd,
					    const gchar	  *text)
{
  gtk_font_selection_set_preview_text(GTK_FONT_SELECTION(fsd->fontsel), text);
}


/* This turns auto-shrink off if the user resizes the width of the dialog.
   It also turns it back on again if the user resizes it back to its normal
   width. */
static gint
gtk_font_selection_dialog_on_configure (GtkWidget         *widget,
					GdkEventConfigure *event,
					GtkFontSelectionDialog *fsd)
{
  /* This sets the initial width. */
  if (fsd->dialog_width == -1)
    fsd->dialog_width = event->width;
  else if (fsd->auto_resize && fsd->dialog_width != event->width)
    {
      fsd->auto_resize = FALSE;
      gtk_window_set_policy(GTK_WINDOW(fsd), FALSE, TRUE, FALSE);
    }
  else if (!fsd->auto_resize && fsd->dialog_width == event->width)
    {
      fsd->auto_resize = TRUE;
      gtk_window_set_policy(GTK_WINDOW(fsd), FALSE, TRUE, TRUE);
    }
  
  return FALSE;
}
