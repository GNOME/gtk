/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * Massively updated for Pango by Owen Taylor, May 2000
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "gdk/gdk.h"
#include "gdk/gdkkeysyms.h"

#include "gtkfontsel.h"

#include "gtkbutton.h"
#include "gtkclist.h"
#include "gtkentry.h"
#include "gtkframe.h"
#include "gtkhbbox.h"
#include "gtkhbox.h"
#include "gtklabel.h"
#include "gtkrc.h"
#include "gtksignal.h"
#include "gtkstock.h"
#include "gtktable.h"
#include "gtkvbox.h"
#include "gtkscrolledwindow.h"
#include "gtkintl.h"

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

/* These are what we use as the standard font sizes, for the size clist.
 */
static const guint16 font_sizes[] = {
  8, 9, 10, 11, 12, 13, 14, 16, 18, 20, 22, 24, 26, 28,
  32, 36, 40, 48, 56, 64, 72
};

static void    gtk_font_selection_class_init	     (GtkFontSelectionClass *klass);
static void    gtk_font_selection_init		     (GtkFontSelection      *fontsel);
static void    gtk_font_selection_finalize	     (GObject               *object);

/* These are the callbacks & related functions. */
static void     gtk_font_selection_select_font           (GtkWidget        *w,
							  gint              row,
							  gint              column,
							  GdkEventButton   *bevent,
							  gpointer          data);
static void     gtk_font_selection_show_available_fonts  (GtkFontSelection *fs);

static void     gtk_font_selection_show_available_styles (GtkFontSelection *fs);
static void     gtk_font_selection_select_best_style     (GtkFontSelection *fs,
							  gboolean          use_first);
static void     gtk_font_selection_select_style          (GtkWidget        *w,
							  gint              row,
							  gint              column,
							  GdkEventButton   *bevent,
							  gpointer          data);

static void     gtk_font_selection_select_best_size      (GtkFontSelection *fs);
static void     gtk_font_selection_show_available_sizes  (GtkFontSelection *fs);
static void     gtk_font_selection_size_activate         (GtkWidget        *w,
							  gpointer          data);
static void     gtk_font_selection_select_size           (GtkWidget        *w,
							  gint              row,
							  gint              column,
							  GdkEventButton   *bevent,
							  gpointer          data);

static void     gtk_font_selection_expose_list           (GtkWidget        *w,
							  GdkEventExpose   *event,
							  gpointer          data);


/* Misc. utility functions. */
static void     gtk_font_selection_load_font         (GtkFontSelection *fs);
static void    gtk_font_selection_update_preview     (GtkFontSelection *fs);

/* FontSelectionDialog */
static void    gtk_font_selection_dialog_class_init  (GtkFontSelectionDialogClass *klass);
static void    gtk_font_selection_dialog_init	     (GtkFontSelectionDialog *fontseldiag);

static gint    gtk_font_selection_dialog_on_configure(GtkWidget      *widget,
						      GdkEventConfigure *event,
						      GtkFontSelectionDialog *fsd);

static GtkWindowClass *font_selection_parent_class = NULL;
static GtkVBoxClass *font_selection_dialog_parent_class = NULL;

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
      
      font_selection_type = gtk_type_unique (GTK_TYPE_VBOX,
					     &fontsel_type_info);
    }
  
  return font_selection_type;
}

static void
gtk_font_selection_class_init(GtkFontSelectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  font_selection_parent_class = gtk_type_class (GTK_TYPE_VBOX);
  
  gobject_class->finalize = gtk_font_selection_finalize;
}

static void
gtk_font_selection_init(GtkFontSelection *fontsel)
{
  GtkWidget *scrolled_win;
  GtkWidget *text_frame;
  GtkWidget *text_box;
  GtkWidget *table, *label;

  fontsel->font_desc = pango_font_description_from_string ("sans 12");
  
  /* Create the table of font, style & size. */
  table = gtk_table_new (3, 3, FALSE);
  gtk_widget_show (table);
  gtk_table_set_col_spacings(GTK_TABLE(table), 8);
  gtk_box_pack_start (GTK_BOX (fontsel), table, TRUE, TRUE, 0);

  fontsel->font_entry = gtk_entry_new ();
  gtk_entry_set_editable(GTK_ENTRY(fontsel->font_entry), FALSE);
  gtk_widget_set_usize (fontsel->font_entry, 20, -1);
  gtk_widget_show (fontsel->font_entry);
  gtk_table_attach (GTK_TABLE (table), fontsel->font_entry, 0, 1, 1, 2,
		    GTK_FILL, 0, 0, 0);
  
  fontsel->font_style_entry = gtk_entry_new ();
  gtk_entry_set_editable (GTK_ENTRY(fontsel->font_style_entry), FALSE);
  gtk_widget_set_usize (fontsel->font_style_entry, 20, -1);
  gtk_widget_show (fontsel->font_style_entry);
  gtk_table_attach (GTK_TABLE (table), fontsel->font_style_entry, 1, 2, 1, 2,
		    GTK_FILL, 0, 0, 0);
  
  fontsel->size_entry = gtk_entry_new ();
  gtk_widget_set_usize (fontsel->size_entry, 20, -1);
  gtk_widget_show (fontsel->size_entry);
  gtk_table_attach (GTK_TABLE (table), fontsel->size_entry, 2, 3, 1, 2,
		    GTK_FILL, 0, 0, 0);
  gtk_signal_connect (GTK_OBJECT (fontsel->size_entry), "activate",
		      (GtkSignalFunc) gtk_font_selection_size_activate,
		      fontsel);
  
  fontsel->font_label = gtk_label_new_with_mnemonic (_("_Family:"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (fontsel->font_label),
                                 fontsel->font_entry);
  gtk_misc_set_alignment (GTK_MISC (fontsel->font_label), 0.0, 0.5);
  gtk_widget_show (fontsel->font_label);
  gtk_table_attach (GTK_TABLE (table), fontsel->font_label, 0, 1, 0, 1,
		    GTK_FILL, 0, 0, 0);  
  label = gtk_label_new_with_mnemonic (_("_Style:"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),
                                 fontsel->font_style_entry);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, 0, 1,
		    GTK_FILL, 0, 0, 0);
  label = gtk_label_new_with_mnemonic (_("Si_ze:"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),
                                 fontsel->size_entry);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table), label, 2, 3, 0, 1,
		    GTK_FILL, 0, 0, 0);
  
  
  /* Create the clists  */
  fontsel->font_clist = gtk_clist_new (1);
  gtk_clist_column_titles_hide (GTK_CLIST (fontsel->font_clist));
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
  
  fontsel->font_style_clist = gtk_clist_new (1);
  gtk_clist_column_titles_hide (GTK_CLIST (fontsel->font_style_clist));
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
  
  fontsel->size_clist = gtk_clist_new (1);
  gtk_clist_column_titles_hide (GTK_CLIST(fontsel->size_clist));
  gtk_clist_set_column_width (GTK_CLIST(fontsel->size_clist), 0, 20);
  scrolled_win = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_usize (scrolled_win, FONT_SIZE_LIST_WIDTH, -1);
  gtk_container_add (GTK_CONTAINER (scrolled_win), fontsel->size_clist);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_widget_show (fontsel->size_clist);
  gtk_widget_show (scrolled_win);
  gtk_table_attach (GTK_TABLE (table), scrolled_win, 2, 3, 2, 3,
		    GTK_FILL, GTK_FILL, 0, 0);
  
  /* Insert the fonts. If there exist fonts with the same family but
     different foundries, then the foundry name is appended in brackets. */
  gtk_font_selection_show_available_fonts(fontsel);
  
  gtk_signal_connect (GTK_OBJECT (fontsel->font_clist), "select_row",
		      GTK_SIGNAL_FUNC(gtk_font_selection_select_font),
		      fontsel);
  GTK_WIDGET_SET_FLAGS (fontsel->font_clist, GTK_CAN_FOCUS);

  gtk_signal_connect_after (GTK_OBJECT (fontsel->font_clist), "expose_event",
			    GTK_SIGNAL_FUNC(gtk_font_selection_expose_list),
			    fontsel);
  
  gtk_font_selection_show_available_styles (fontsel);
  
  gtk_signal_connect (GTK_OBJECT (fontsel->font_style_clist), "select_row",
		      GTK_SIGNAL_FUNC(gtk_font_selection_select_style),
		      fontsel);
  GTK_WIDGET_SET_FLAGS (fontsel->font_style_clist, GTK_CAN_FOCUS);

  gtk_font_selection_show_available_sizes (fontsel);
  
  gtk_signal_connect (GTK_OBJECT (fontsel->size_clist), "select_row",
		      GTK_SIGNAL_FUNC(gtk_font_selection_select_size),
		      fontsel);
  GTK_WIDGET_SET_FLAGS (fontsel->size_clist, GTK_CAN_FOCUS);
  
  /* create the text entry widget */
  text_frame = gtk_frame_new (_("Preview:"));
  gtk_widget_show (text_frame);
  gtk_frame_set_shadow_type(GTK_FRAME(text_frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start (GTK_BOX (fontsel), text_frame,
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

  gtk_font_selection_update_preview (fontsel);
}

GtkWidget *
gtk_font_selection_new()
{
  GtkFontSelection *fontsel;
  
  fontsel = gtk_type_new (GTK_TYPE_FONT_SELECTION);
  
  return GTK_WIDGET (fontsel);
}

static void
gtk_font_selection_finalize (GObject *object)
{
  GtkFontSelection *fontsel;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_FONT_SELECTION (object));
  
  fontsel = GTK_FONT_SELECTION (object);

  pango_font_description_free (fontsel->font_desc);

  if (fontsel->font)
    gdk_font_unref (fontsel->font);
  
  if (G_OBJECT_CLASS (font_selection_parent_class)->finalize)
    (* G_OBJECT_CLASS (font_selection_parent_class)->finalize) (object);
}


/* This is called when the clist is exposed. Here we scroll to the current
   font if necessary. */
static void
gtk_font_selection_expose_list (GtkWidget		*widget,
				GdkEventExpose		*event,
				gpointer		 data)
{
  GtkFontSelection *fontsel;
  GList *selection;
  gint index;
  
#ifdef FONTSEL_DEBUG
  g_message("In expose_list\n");
#endif
  fontsel = GTK_FONT_SELECTION(data);
  
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

/* This is called when a family is selected in the list. */
static void
gtk_font_selection_select_font (GtkWidget      *w,
				gint	        row,
				gint	        column,
				GdkEventButton *bevent,
				gpointer        data)
{
  GtkFontSelection *fontsel;
  gchar *family_name;
  gint index;
  
  fontsel = GTK_FONT_SELECTION (data);

  if (GTK_CLIST (fontsel->font_clist)->selection)
    {
      index = GPOINTER_TO_INT (GTK_CLIST (fontsel->font_clist)->selection->data);

      if (gtk_clist_get_text (GTK_CLIST (fontsel->font_clist), index, 0, &family_name) &&
	  strcasecmp (fontsel->font_desc->family_name, family_name) != 0)
	{
	  g_free (fontsel->font_desc->family_name);
	  fontsel->font_desc->family_name  = g_strdup (family_name);

	  gtk_entry_set_text (GTK_ENTRY (fontsel->font_entry), family_name);
	  
	  gtk_font_selection_show_available_styles (fontsel);
	  gtk_font_selection_select_best_style (fontsel, TRUE);
	}
    }
}

static int
cmp_strings (const void *a, const void *b)
{
  return strcasecmp (*(const char **)a, *(const char **)b);
}

static void
gtk_font_selection_show_available_fonts (GtkFontSelection *fontsel)
{
  gchar **families;
  int n_families, i;

  pango_context_list_families (gtk_widget_get_pango_context (GTK_WIDGET (fontsel)),
			       &families, &n_families);
  qsort (families, n_families, sizeof(char *), cmp_strings);

  gtk_clist_freeze (GTK_CLIST (fontsel->font_clist));
  gtk_clist_clear (GTK_CLIST (fontsel->font_clist));

  for (i=0; i<n_families; i++)
    {
      gtk_clist_append (GTK_CLIST (fontsel->font_clist), &families[i]);

      if (!strcasecmp (families[i], fontsel->font_desc->family_name))
	{
	  gtk_clist_select_row (GTK_CLIST(fontsel->font_clist), i, 0);
	  gtk_entry_set_text(GTK_ENTRY(fontsel->font_entry), families[i]);
	}
    }
  
  gtk_clist_thaw (GTK_CLIST(fontsel->font_clist));

  pango_font_map_free_families (families, n_families);
}

static int
compare_font_descriptions (const PangoFontDescription *a, const PangoFontDescription *b)
{
  int val = strcasecmp (a->family_name, b->family_name);
  if (val != 0)
    return val;

  if (a->weight != b->weight)
    return a->weight - b->weight;

  if (a->style != b->style)
    return a->style - b->style;
  
  if (a->stretch != b->stretch)
    return a->stretch - b->stretch;

  if (a->variant != b->variant)
    return a->variant - b->variant;

  return 0;
}

static int
font_description_sort_func (const void *a, const void *b)
{
  return compare_font_descriptions (*(PangoFontDescription **)a, *(PangoFontDescription **)b);
}

/* This fills the font style clist with all the possible style combinations
   for the current font family. */
static void
gtk_font_selection_show_available_styles (GtkFontSelection *fontsel)
{
  PangoFontDescription **descs;
  int n_descs, i;
  gint match_row = -1;
  gchar *str;
  
  pango_context_list_fonts (gtk_widget_get_pango_context (GTK_WIDGET (fontsel)),
			    fontsel->font_desc->family_name, &descs, &n_descs);
  qsort (descs, n_descs, sizeof(PangoFontDescription *), font_description_sort_func);

  gtk_clist_freeze (GTK_CLIST (fontsel->font_style_clist));
  gtk_clist_clear (GTK_CLIST (fontsel->font_style_clist));

  for (i=0; i<n_descs; i++)
    {
      PangoFontDescription tmp_desc;

      tmp_desc = *descs[i];
      tmp_desc.family_name = NULL;
      tmp_desc.size = 0;

      str = pango_font_description_to_string (&tmp_desc);
      gtk_clist_append (GTK_CLIST (fontsel->font_style_clist), &str);

      if (descs[i]->weight == fontsel->font_desc->weight &&
	  descs[i]->style == fontsel->font_desc->style &&
	  descs[i]->stretch == fontsel->font_desc->stretch &&
	  descs[i]->variant == fontsel->font_desc->variant)
	match_row = i;
      
      g_free (str);
    }

  gtk_clist_select_row (GTK_CLIST (fontsel->font_style_clist), match_row, 0);
  if (match_row >= 0)
    {
      gtk_clist_get_text (GTK_CLIST (fontsel->font_style_clist), match_row, 0, &str);
      gtk_entry_set_text (GTK_ENTRY (fontsel->font_style_entry), str);
    }
  
  gtk_clist_thaw (GTK_CLIST(fontsel->font_style_clist));

  pango_font_descriptions_free (descs, n_descs);
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
  gint best_row = 0;
  
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
  GtkFontSelection *fontsel = GTK_FONT_SELECTION (data);
  PangoFontDescription *tmp_desc;
  gchar *text;
  gint index;
  
  if (bevent && !GTK_WIDGET_HAS_FOCUS (w))
    gtk_widget_grab_focus (w);
  
  if (GTK_CLIST (fontsel->font_style_clist)->selection)
    {
      index = GPOINTER_TO_INT (GTK_CLIST (fontsel->font_style_clist)->selection->data);

      if (gtk_clist_get_text (GTK_CLIST (fontsel->font_style_clist), index, 0, &text))
	{
	  tmp_desc = pango_font_description_from_string (text);
	  
	  fontsel->font_desc->style = tmp_desc->style;
	  fontsel->font_desc->variant = tmp_desc->variant;
	  fontsel->font_desc->weight = tmp_desc->weight;
	  fontsel->font_desc->stretch = tmp_desc->stretch;
	  
	  pango_font_description_free (tmp_desc);
	}
    }

  gtk_font_selection_show_available_sizes (fontsel);
  gtk_font_selection_select_best_size (fontsel);
}

static void
gtk_font_selection_show_available_sizes (GtkFontSelection *fontsel)
{
  gint i;
  gchar buffer[128];
  gchar *size;

  /* Insert the standard font sizes */
  gtk_clist_freeze (GTK_CLIST (fontsel->size_clist));
  gtk_clist_clear (GTK_CLIST (fontsel->size_clist));

  for (i = 0; i < G_N_ELEMENTS (font_sizes); i++)
    {
      sprintf(buffer, "%i", font_sizes[i]);
      size = buffer;
      gtk_clist_append (GTK_CLIST(fontsel->size_clist), &size);
      if (font_sizes[i] * PANGO_SCALE == fontsel->font_desc->size)
	gtk_clist_select_row(GTK_CLIST(fontsel->size_clist), i, 0);
    }
  gtk_clist_thaw (GTK_CLIST(fontsel->size_clist));

  sprintf (buffer, "%i", fontsel->font_desc->size / PANGO_SCALE);
  gtk_entry_set_text (GTK_ENTRY(fontsel->size_entry), buffer);
}

static void
gtk_font_selection_select_best_size (GtkFontSelection *fontsel)
{
  gtk_font_selection_load_font (fontsel);  
}

/* If the user hits return in the font size entry, we change to the new font
   size. */
static void
gtk_font_selection_size_activate (GtkWidget   *w,
                                  gpointer     data)
{
  GtkFontSelection *fontsel;
  gint new_size;
  gchar *text;
  
  fontsel = GTK_FONT_SELECTION (data);

  text = gtk_entry_get_text (GTK_ENTRY (fontsel->size_entry));
  new_size = atoi (text) * PANGO_SCALE;
  
  if (fontsel->font_desc->size != new_size)
    {
      fontsel->font_desc->size = new_size;
      gtk_font_selection_load_font (fontsel);
    }
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
  gint new_size;
  gchar *text;
  
  fontsel = GTK_FONT_SELECTION (data);
  
  if (bevent && !GTK_WIDGET_HAS_FOCUS (w))
    gtk_widget_grab_focus (w);
  
  gtk_clist_get_text(GTK_CLIST(fontsel->size_clist), row, 0, &text);
  new_size = atoi (text) * PANGO_SCALE;
  
  if (fontsel->font_desc->size != new_size)
    {
      /* If the size was selected by the user we set the selected_size. */
      fontsel->font_desc->size = new_size;

      gtk_font_selection_load_font (fontsel);
    }
}

static void
gtk_font_selection_load_font (GtkFontSelection *fontsel)
{
  if (fontsel->font)
    gdk_font_unref (fontsel->font);
  fontsel->font = NULL;

  gtk_font_selection_update_preview (fontsel);
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
  GtkRcStyle *rc_style;
  gint new_height;
  GtkRequisition old_requisition;
  GtkWidget *preview_entry = fontsel->preview_entry;
  gchar *text;

  gtk_widget_get_child_requisition (preview_entry, &old_requisition);
  
  rc_style = gtk_rc_style_new ();
  rc_style->font_desc = pango_font_description_copy (fontsel->font_desc);
  gtk_widget_modify_style (preview_entry, rc_style);
  gtk_rc_style_unref (rc_style);

  gtk_widget_size_request (preview_entry, NULL);
  
  /* We don't ever want to be over MAX_PREVIEW_HEIGHT pixels high. */
  new_height = CLAMP (preview_entry->requisition.height, INITIAL_PREVIEW_HEIGHT, MAX_PREVIEW_HEIGHT);

  if (new_height > old_requisition.height || new_height < old_requisition.height - 30)
    gtk_widget_set_usize(preview_entry, -1, new_height);
  
  /* This sets the preview text, if it hasn't been set already. */
  text = gtk_entry_get_text(GTK_ENTRY(preview_entry));
  if (strlen(text) == 0)
    gtk_entry_set_text(GTK_ENTRY(preview_entry), PREVIEW_TEXT);
  gtk_entry_set_position(GTK_ENTRY(preview_entry), 0);
}

/*****************************************************************************
 * These functions are the main public interface for getting/setting the font.
 *****************************************************************************/

GdkFont*
gtk_font_selection_get_font (GtkFontSelection *fontsel)
{
  if (!fontsel->font)
    fontsel->font = gdk_font_from_description (fontsel->font_desc);
  
  return fontsel->font;
}


gchar *
gtk_font_selection_get_font_name (GtkFontSelection *fontsel)
{
  return pango_font_description_to_string (fontsel->font_desc);
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
  PangoFontDescription *new_desc;
  PangoFontDescription **descs;
  int n_descs, i;
  gboolean found = FALSE;

  g_return_val_if_fail (fontsel != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_FONT_SELECTION (fontsel), FALSE);
  
  new_desc = pango_font_description_from_string (fontname);

  /* Check to make sure that this is in the list of allowed fonts */

  pango_context_list_fonts (gtk_widget_get_pango_context (GTK_WIDGET (fontsel)),
			    new_desc->family_name, &descs, &n_descs);

  for (i=0; i<n_descs; i++)
    {
      if (descs[i]->weight == new_desc->weight &&
	  descs[i]->style == new_desc->style &&
	  descs[i]->stretch == new_desc->stretch &&
	  descs[i]->variant == new_desc->variant)
	{
	  found = TRUE;
	  break;
	}
    }

  pango_font_descriptions_free (descs, n_descs);

  if (!found)
    return FALSE;

  pango_font_description_free (fontsel->font_desc);
  fontsel->font_desc = new_desc;

  return TRUE;
}


/* This returns the text in the preview entry. You should copy the returned
   text if you need it. */
gchar*
gtk_font_selection_get_preview_text  (GtkFontSelection *fontsel)
{
  return gtk_entry_get_text (GTK_ENTRY (fontsel->preview_entry));
}


/* This sets the text in the preview entry. */
void
gtk_font_selection_set_preview_text  (GtkFontSelection *fontsel,
				      const gchar	  *text)
{
  gtk_entry_set_text (GTK_ENTRY (fontsel->preview_entry), text);
}

/*****************************************************************************
 * GtkFontSelectionDialog
 *****************************************************************************/

GtkType
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
      
      font_selection_dialog_type = gtk_type_unique (GTK_TYPE_DIALOG,
                                                    &fontsel_diag_info);
    }
  
  return font_selection_dialog_type;
}

static void
gtk_font_selection_dialog_class_init (GtkFontSelectionDialogClass *klass)
{
  GtkObjectClass *object_class;
  
  object_class = (GtkObjectClass*) klass;
  
  font_selection_dialog_parent_class = gtk_type_class (GTK_TYPE_DIALOG);
}

static void
gtk_font_selection_dialog_init (GtkFontSelectionDialog *fontseldiag)
{
  GtkDialog *dialog;

  dialog = GTK_DIALOG (fontseldiag);
  
  fontseldiag->dialog_width = -1;
  fontseldiag->auto_resize = TRUE;
  
  gtk_widget_set_events(GTK_WIDGET(fontseldiag), GDK_STRUCTURE_MASK);
  gtk_signal_connect (GTK_OBJECT (fontseldiag), "configure_event",
		      (GtkSignalFunc) gtk_font_selection_dialog_on_configure,
		      fontseldiag);
  
  gtk_container_set_border_width (GTK_CONTAINER (fontseldiag), 4);
  gtk_window_set_policy(GTK_WINDOW(fontseldiag), FALSE, TRUE, TRUE);
  
  fontseldiag->main_vbox = dialog->vbox;
  
  fontseldiag->fontsel = gtk_font_selection_new();
  gtk_container_set_border_width (GTK_CONTAINER (fontseldiag->fontsel), 4);
  gtk_widget_show (fontseldiag->fontsel);
  gtk_box_pack_start (GTK_BOX (fontseldiag->main_vbox),
		      fontseldiag->fontsel, TRUE, TRUE, 0);
  
  /* Create the action area */
  fontseldiag->action_area = dialog->action_area;
  
  fontseldiag->ok_button = gtk_dialog_add_button (dialog,
                                                  GTK_STOCK_BUTTON_OK,
                                                  GTK_RESPONSE_OK);
  gtk_widget_grab_default (fontseldiag->ok_button);
  
  fontseldiag->apply_button = gtk_dialog_add_button (dialog,
                                                     GTK_STOCK_BUTTON_APPLY,
                                                     GTK_RESPONSE_APPLY);
  gtk_widget_hide (fontseldiag->apply_button);

  
  fontseldiag->cancel_button = gtk_dialog_add_button (dialog,
                                                      GTK_STOCK_BUTTON_CANCEL,
                                                      GTK_RESPONSE_CANCEL);

  gtk_window_set_title (GTK_WINDOW (fontseldiag),
                        _("Font Selection"));

}

GtkWidget*
gtk_font_selection_dialog_new	(const gchar	  *title)
{
  GtkFontSelectionDialog *fontseldiag;
  
  fontseldiag = gtk_type_new (GTK_TYPE_FONT_SELECTION_DIALOG);

  if (title)
    gtk_window_set_title (GTK_WINDOW (fontseldiag), title);
  
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
