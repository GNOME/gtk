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

#include <math.h>
#include <string.h>
#include "gtklabel.h"
#include "gdk/gdkkeysyms.h"
#include "gdk/gdki18n.h"

enum {
  ARG_0,
  ARG_LABEL,
  ARG_PATTERN,
  ARG_JUSTIFY,
  ARG_WRAP
};

typedef struct _GtkLabelULine GtkLabelULine;
struct _GtkLabelWord
{
  GdkWChar *beginning;
  gint length;
  
  /* FIXME:
   * We need (space,width) only before we've set (x,y), so to save
   * memory, these pairs should really be wrapped in a union.
   * I haven't yet figured out how to do this without making the code
   * look ugly. 
   */
  gint space;
  gint width;
  gint x;
  gint y;
  GtkLabelWord *next;
  gint uline_y;
  GtkLabelULine *uline;
};

struct _GtkLabelULine
{
  gint x1;
  gint x2;
  gint y;
  GtkLabelULine *next;
};

static void gtk_label_class_init   (GtkLabelClass  *klass);
static void gtk_label_init	   (GtkLabel	   *label);
static void gtk_label_set_arg	   (GtkObject	   *object,
				    GtkArg	   *arg,
				    guint	    arg_id);
static void gtk_label_get_arg	   (GtkObject      *object,
				    GtkArg	   *arg,
				    guint	    arg_id);
static void gtk_label_finalize	   (GtkObject	   *object);
static void gtk_label_size_request (GtkWidget	   *widget,
				    GtkRequisition *requisition);
static void gtk_label_style_set    (GtkWidget      *widget,
				    GtkStyle       *previous_style);
static gint gtk_label_expose	   (GtkWidget	   *widget,
				    GdkEventExpose *event);

static GtkLabelWord*  gtk_label_word_alloc          (void);
static GtkLabelULine* gtk_label_uline_alloc         (void);
static void           gtk_label_free_words          (GtkLabel     *label);
static void           gtk_label_free_ulines         (GtkLabelWord *word);
static gint           gtk_label_split_text          (GtkLabel     *label);


static GtkMiscClass *parent_class = NULL;

static GMemChunk *word_chunk = NULL;
static GMemChunk *uline_chunk = NULL;

GtkType
gtk_label_get_type (void)
{
  static GtkType label_type = 0;
  
  if (!label_type)
    {
      static const GtkTypeInfo label_info =
      {
	"GtkLabel",
	sizeof (GtkLabel),
	sizeof (GtkLabelClass),
	(GtkClassInitFunc) gtk_label_class_init,
	(GtkObjectInitFunc) gtk_label_init,
        /* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };
      
      label_type = gtk_type_unique (GTK_TYPE_MISC, &label_info);
      gtk_type_set_chunk_alloc (label_type, 32);
    }
  
  return label_type;
}

static void
gtk_label_class_init (GtkLabelClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  
  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;
  
  parent_class = gtk_type_class (GTK_TYPE_MISC);
  
  gtk_object_add_arg_type ("GtkLabel::label", GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_LABEL);
  gtk_object_add_arg_type ("GtkLabel::pattern", GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_PATTERN);
  gtk_object_add_arg_type ("GtkLabel::justify", GTK_TYPE_JUSTIFICATION, GTK_ARG_READWRITE, ARG_JUSTIFY);
  gtk_object_add_arg_type ("GtkLabel::wrap", GTK_TYPE_BOOL, GTK_ARG_READWRITE, ARG_WRAP);
  
  object_class->set_arg = gtk_label_set_arg;
  object_class->get_arg = gtk_label_get_arg;
  object_class->finalize = gtk_label_finalize;
  
  widget_class->size_request = gtk_label_size_request;
  widget_class->style_set = gtk_label_style_set;
  widget_class->expose_event = gtk_label_expose;
}

static void
gtk_label_set_arg (GtkObject	  *object,
		   GtkArg	  *arg,
		   guint	   arg_id)
{
  GtkLabel *label;
  
  label = GTK_LABEL (object);
  
  switch (arg_id)
    {
    case ARG_LABEL:
      gtk_label_set_text (label, GTK_VALUE_STRING (*arg));
      break;
    case ARG_PATTERN:
      gtk_label_set_pattern (label, GTK_VALUE_STRING (*arg));
      break;
    case ARG_JUSTIFY:
      gtk_label_set_justify (label, GTK_VALUE_ENUM (*arg));
      break;
    case ARG_WRAP:
      gtk_label_set_line_wrap (label, GTK_VALUE_BOOL (*arg));
      break;	  
    default:
      break;
    }
}

static void
gtk_label_get_arg (GtkObject	  *object,
		   GtkArg	  *arg,
		   guint	   arg_id)
{
  GtkLabel *label;
  
  label = GTK_LABEL (object);
  
  switch (arg_id)
    {
    case ARG_LABEL:
      GTK_VALUE_STRING (*arg) = g_strdup (label->label);
      break;
    case ARG_PATTERN:
      GTK_VALUE_STRING (*arg) = g_strdup (label->pattern);
      break;
    case ARG_JUSTIFY:
      GTK_VALUE_ENUM (*arg) = label->jtype;
      break;
    case ARG_WRAP:
      GTK_VALUE_BOOL (*arg) = label->wrap;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_label_init (GtkLabel *label)
{
  GTK_WIDGET_SET_FLAGS (label, GTK_NO_WINDOW);
  
  label->label = NULL;
  label->label_wc = NULL;
  label->pattern = NULL;

  label->words = NULL;

  label->max_width = 0;
  label->jtype = GTK_JUSTIFY_CENTER;
  label->wrap = FALSE;
  
  gtk_label_set_text (label, "");
}

GtkWidget*
gtk_label_new (const gchar *str)
{
  GtkLabel *label;
  
  label = gtk_type_new (GTK_TYPE_LABEL);

  if (str && *str)
    gtk_label_set_text (label, str);
  
  return GTK_WIDGET (label);
}

static inline void
gtk_label_set_text_internal (GtkLabel *label,
			     gchar    *str,
			     GdkWChar *str_wc)
{
  gtk_label_free_words (label);
      
  g_free (label->label);
  g_free (label->label_wc);
  
  label->label = str;
  label->label_wc = str_wc;
  
  gtk_widget_queue_resize (GTK_WIDGET (label));
}

void
gtk_label_set_text (GtkLabel    *label,
		    const gchar *str)
{
  GdkWChar *str_wc;
  gint len;
  gint wc_len;
  
  g_return_if_fail (GTK_IS_LABEL (label));
  if (!str)
    str = "";

  if (!label->label || strcmp (label->label, str))
    {
      /* Convert text to wide characters */
      len = strlen (str);
      str_wc = g_new (GdkWChar, len + 1);
      wc_len = gdk_mbstowcs (str_wc, str, len + 1);
      if (wc_len >= 0)
	{
	  str_wc[wc_len] = '\0';
	  gtk_label_set_text_internal (label, g_strdup (str), str_wc);
	}
      else
	g_free (str_wc);
    }
}

void
gtk_label_set_pattern (GtkLabel	   *label,
		       const gchar *pattern)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  gtk_label_free_words (label);
  
  g_free (label->pattern);
  label->pattern = g_strdup (pattern);

  gtk_widget_queue_resize (GTK_WIDGET (label));
}

void
gtk_label_set_justify (GtkLabel        *label,
		       GtkJustification jtype)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (jtype >= GTK_JUSTIFY_LEFT && jtype <= GTK_JUSTIFY_FILL);
  
  if ((GtkJustification) label->jtype != jtype)
    {
      gtk_label_free_words (label);
      
      label->jtype = jtype;

      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

void
gtk_label_set_line_wrap (GtkLabel *label,
			 gboolean  wrap)
{
  g_return_if_fail (GTK_IS_LABEL (label));
  
  wrap = wrap != FALSE;
  
  if (label->wrap != wrap)
    {
      gtk_label_free_words (label);

      label->wrap = wrap;

      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

void
gtk_label_get (GtkLabel *label,
	       gchar   **str)
{
  g_return_if_fail (label != NULL);
  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (str != NULL);
  
  *str = label->label;
}

static void
gtk_label_finalize (GtkObject *object)
{
  GtkLabel *label;
  
  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_LABEL (object));
  
  label = GTK_LABEL (object);
  
  g_free (label->label);
  g_free (label->label_wc);
  g_free (label->pattern);

  gtk_label_free_words (label);

  GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static GtkLabelULine*
gtk_label_uline_alloc (void)
{
  GtkLabelULine *uline;
  
  if (!uline_chunk)
    uline_chunk = g_mem_chunk_create (GtkLabelWord, 32, G_ALLOC_AND_FREE);

  uline = g_chunk_new0 (GtkLabelULine, uline_chunk);
  
  uline->next = NULL;
  
  return uline;
}

static void
gtk_label_free_ulines (GtkLabelWord *word)
{
  while (word->uline)
    {
      GtkLabelULine *uline = word->uline;

      word->uline = uline->next;
      g_chunk_free (uline, uline_chunk);
    }
}

static GtkLabelWord*
gtk_label_word_alloc (void)
{
  GtkLabelWord * word;
  
  if (!word_chunk)
    word_chunk = g_mem_chunk_create (GtkLabelWord, 32, G_ALLOC_AND_FREE);
  
  word = g_chunk_new0 (GtkLabelWord, word_chunk);
  
  word->beginning = NULL;
  word->next = NULL;
  word->uline = NULL;

  return word;
}

static void
gtk_label_free_words (GtkLabel *label)
{
  while (label->words)
    {
      GtkLabelWord *word = label->words;

      label->words = word->next;

      gtk_label_free_ulines (word);

      g_chunk_free (word, word_chunk);
    }
}

static gint
gtk_label_split_text (GtkLabel *label)
{
  GtkLabelWord *word, **tailp;
  gint space_width, line_width, max_line_width;
  GdkWChar *str, *p;
  
  gtk_label_free_words (label);
  if (label->label == NULL)
    return 0;
  
  /* Split text at new-lines. */
  space_width = gdk_string_width (GTK_WIDGET (label)->style->font, " ");
  
  line_width = 0;
  max_line_width = 0;
  tailp = &label->words;
  str = label->label_wc;
  
  while (*str)
    {
      word = gtk_label_word_alloc ();
      
      if (str == label->label_wc || str[-1] == '\n')
	{
	  /* Paragraph break */
	  word->space = 0;
	  
	  max_line_width = MAX (line_width, max_line_width);
	  line_width = 0;
	}
      else if (str[0] == ' ')
	{
	  while (str[0] == ' ')
	    {
	      str++;
	      word->space += space_width;
	    }
	}
      else
	{
	  /* Regular inter-word space */
	  word->space = space_width;
	}
      
      word->beginning = str;
      
      word->length = 0;
      p = word->beginning;
      while (*p && *p != '\n')
	{
	  word->length++;
	  p++;
	}
      
      word->width = gdk_text_width_wc (GTK_WIDGET (label)->style->font, str, word->length);
      
      str += word->length;
      if (*str)
	str++;
      
      line_width += word->space + word->width;
      
      *tailp = word;
      tailp = &word->next;
    }
  
  /* Add an empty word to represent an empty line
   */
  if (str == label->label_wc || str[-1] == '\n')
    {
      word = gtk_label_word_alloc ();
      
      word->space = 0;
      word->beginning = str;
      word->length = 0;
      word->width = 0;
      
      *tailp = word;
      tailp = &word->next;
    }
  
  return MAX (line_width, max_line_width);
}

/* this needs to handle white space better. */
static gint
gtk_label_split_text_wrapped (GtkLabel *label)
{
  GtkLabelWord *word, **tailp;
  gint space_width, line_width, max_line_width;
  GdkWChar *str, *p;
  
  gtk_label_free_words (label);
  if (label->label == NULL)
    return 0;
  
  /* Split text at new-lines.  (Or at spaces in the case of paragraphs). */
  space_width = gdk_string_width (GTK_WIDGET (label)->style->font, " ");
  
  line_width = 0;
  max_line_width = 0;
  tailp = &label->words;
  str = label->label_wc;
  while (*str)
    {
      word = gtk_label_word_alloc ();
      
      if (str == label->label_wc || str[-1] == '\n')
	{
	  /* Paragraph break */
	  word->space = 0;
	  
	  max_line_width = MAX (line_width, max_line_width);
	  line_width = 0;
	}
      else if (str[0] == ' ')
	{
	  gint nspaces = 0;
	  
	  while (str[0] == ' ')
	    {
	      nspaces++;
	      str++;
	    }
	  
	  if (label->jtype == GTK_JUSTIFY_FILL)
	    word->space = (space_width * 3 + 1) / 2;
	  else
	    word->space = space_width * nspaces;
	}
      else
	{
	  /* Regular inter-word space */
	  word->space = space_width;
	}
      
      word->beginning = str;
      word->length = 0;
      p = word->beginning;
      while (*p && !gdk_iswspace (*p))
	{
	  word->length++;
	  p++;
	}
      word->width = gdk_text_width_wc (GTK_WIDGET (label)->style->font, str, word->length);
      
      str += word->length;
      if (*str)
	str++;
      
      line_width += word->space + word->width;
      
      *tailp = word;
      tailp = &word->next;
    }
  
  return MAX (line_width, max_line_width);
}

/* gtk_label_pick_width
 *
 * Split paragraphs, trying to make each line at least min_width,
 * and trying even harder to make each line no longer than max_width.
 *
 * Returns the length of the longest resulting line.
 *
 * (The reason we go to all this effort to pick a paragraph width is to
 * try to avoid the lame look of a short paragraph with a
 * short final line.)
 */
static gint
gtk_label_pick_width (GtkLabel *label,
		      gint      min_width,
		      gint      max_width)
{
  GtkLabelWord *word;
  gint width, line_width;
  
  g_return_val_if_fail (label->wrap, min_width);
  
  line_width = 0;
  width = 0;
  for (word = label->words; word; word = word->next)
    {
      if (word->space == 0
	  || (line_width
	      && (line_width >= min_width
		  || line_width + word->width + word->space > max_width)))
	{
	  /* New line */
	  width = MAX (width, line_width);
	  line_width = 0;
	}
      line_width += word->space + word->width;
    }
  
  return MAX (width, line_width);
}

/* Here, we finalize the lines.
 * This is only for non-wrap labels.  Wrapped labels
 * use gtk_label_finalize_wrap instead.
 */
static void
gtk_label_finalize_lines (GtkLabel       *label,
			  GtkRequisition *requisition,
			  gint            max_line_width)
{
  GtkLabelWord *line;
  gint y, baseline_skip, y_max;
  gint i, j;
  gchar *ptrn;
  
  g_return_if_fail (!label->wrap);
  ptrn = label->pattern;
  
  y = 0;
  baseline_skip = (GTK_WIDGET (label)->style->font->ascent +
		   GTK_WIDGET (label)->style->font->descent + 2);
  
  for (line = label->words; line; line = line->next)
    {
      if (label->jtype == GTK_JUSTIFY_CENTER)
	line->x = (max_line_width - line->width) / 2;
      else if (label->jtype == GTK_JUSTIFY_RIGHT)
	line->x = max_line_width - line->width;
      else
	line->x = 0;
      
      line->y = y + GTK_WIDGET (label)->style->font->ascent + 1;
      y_max = 0;
      
      /* now we deal with the underline stuff; */
      if (ptrn && ptrn[0] != '\0')
	{
	  for (i = 0; i < line->length; i++)
	    {
	      if (ptrn[i] == '\0')
		break;
	      else if (ptrn[i] == '_')
		{
		  gint descent;
		  gint rbearing;
		  gint lbearing;
		  gint width;
		  gint offset;
		  GtkLabelULine *uline;
		  
		  for (j = i + 1; j < line->length; j++)
		    {
		      if (ptrn[j] == '\0')
			break;
		      else if (ptrn[j] == ' ')
			break;
		    }
		  
		  /* good.  Now we have an underlined segment.
		   * let's measure it and record it.
		   */
		  offset = gdk_text_width_wc (GTK_WIDGET (label)->style->font,
					      line->beginning,
					      i);
		  gdk_text_extents_wc (GTK_WIDGET (label)->style->font,
				       line->beginning+i,
				       j-i, &lbearing,
				       &rbearing, &width, NULL,
				       &descent);
		  y_max = MAX (descent + 2, y_max);
		  uline = gtk_label_uline_alloc ();
		  uline->x1 = offset + line->x + lbearing - 1;
		  uline->x2 = offset + line->x + rbearing;
		  uline->y = line->y + descent + 2;
		  uline->next = line->uline;
		  line->uline = uline;
		  i = j - 1;
		}
	    }
	  if (strlen (ptrn) > line->length)
	    /* the + 1 is for line breaks. */
	    ptrn += line->length + 1;
	  else
	    ptrn = NULL;
	}
      y += (baseline_skip + y_max);
    }
  
  label->max_width = max_line_width;
  requisition->width = max_line_width + 2 * label->misc.xpad;
  requisition->height = y + 2 * label->misc.ypad;
}

/* this finalizes word-wrapped words */
static void
gtk_label_finalize_lines_wrap (GtkLabel       *label,
			       GtkRequisition *requisition,
			       gint            max_line_width)
{
  GtkLabelWord *word, *line, *next_line;
  GtkWidget *widget;
  gchar *ptrn;
  gint x, y, space, extra_width, add_space, baseline_skip;
  
  g_return_if_fail (label->wrap);
  
  ptrn = label->pattern;
  y = 0;
  baseline_skip = (GTK_WIDGET (label)->style->font->ascent +
		   GTK_WIDGET (label)->style->font->descent + 1);
  
  for (line = label->words; line != 0; line = next_line)
    {
      space = 0;
      extra_width = max_line_width - line->width;
      
      for (next_line = line->next; next_line; next_line = next_line->next)
	{
	  if (next_line->space == 0)
	    break;		/* New paragraph */
	  if (next_line->space + next_line->width > extra_width)
	    break;
	  extra_width -= next_line->space + next_line->width;
	  space += next_line->space;
	}
      
      line->x = 0;
      line->y = y + GTK_WIDGET (label)->style->font->ascent + 1;
      x = line->width;
      add_space = 0;
      
      for (word = line->next; word != next_line; word = word->next)
	{
	  if (next_line && next_line->space)
	    {
	      /* Not last line of paragraph --- fill line if needed */
	      if (label->jtype == GTK_JUSTIFY_FILL) {
		add_space = (extra_width * word->space + space / 2) / space;
		extra_width -= add_space;
		space -= word->space;
	      }
	    }
	  
	  word->x = x + word->space + add_space;
	  word->y = line->y;
	  x = word->x + word->width;
	}
      
      y += (baseline_skip);
    }
  
  label->max_width = max_line_width;
  widget = GTK_WIDGET (label);
  requisition->width = max_line_width + 2 * label->misc.xpad;
  requisition->height = y + 2 * label->misc.ypad + 1;
}

static void
gtk_label_size_request (GtkWidget      *widget,
			GtkRequisition *requisition)
{
  GtkLabel *label;
  
  g_return_if_fail (GTK_IS_LABEL (widget));
  g_return_if_fail (requisition != NULL);
  
  label = GTK_LABEL (widget);

  /*
   * There are a number of conditions which will necessitate re-filling
   * our text:
   *
   *     1. text changed.
   *     2. justification changed either from to to GTK_JUSTIFY_FILL.
   *     3. font changed.
   *
   * These have been detected elsewhere, and label->words will be zero,
   * if one of the above has occured.
   *
   * Additionally, though, if GTK_JUSTIFY_FILL, we need to re-fill if:
   *
   *     4. gtk_widget_set_usize has changed the requested width.
   *     5. gtk_misc_set_padding has changed xpad.
   *     6.  maybe others?...
   *
   * Too much of a pain to detect all these case, so always re-fill.  I
   * don't think it's really that slow.
   */
  
  if (label->wrap)
    {
      GtkWidgetAuxInfo *aux_info;
      gint longest_paragraph;
      
      longest_paragraph = gtk_label_split_text_wrapped (label);
      
      aux_info = gtk_object_get_data (GTK_OBJECT (widget), "gtk-aux-info");
      if (aux_info && aux_info->width > 0)
	{
	  label->max_width = MAX (aux_info->width - 2 * label->misc.xpad, 1);
	  gtk_label_split_text_wrapped (label);
	}
      else
	{
	  label->max_width = gdk_string_width (GTK_WIDGET (label)->style->font,
					       "This is a good enough length for any line to have.");
	  label->max_width = MIN (label->max_width, (gdk_screen_width () + 1) / 2);
	  label->max_width = MIN (label->max_width, longest_paragraph);
	  if (longest_paragraph > 0)
	    {
	      gint nlines, perfect_width;
	      
	      nlines = (longest_paragraph + label->max_width - 1) / label->max_width;
	      perfect_width = (longest_paragraph + nlines - 1) / nlines;
	      label->max_width = gtk_label_pick_width (label,
						       perfect_width,
						       label->max_width);
	    }
	}
      gtk_label_finalize_lines_wrap (label, requisition, label->max_width);
    }
  else if (!label->words)
    {
      label->max_width = gtk_label_split_text (label);
      gtk_label_finalize_lines (label, requisition, label->max_width);
    }
}

static void 
gtk_label_style_set (GtkWidget *widget,
		     GtkStyle  *previous_style)
{
  GtkLabel *label;

  g_return_if_fail (GTK_IS_LABEL (widget));
  
  label = GTK_LABEL (widget);
  
  /* Clear the list of words so that they are recomputed on
   * size_request
   */
  if (previous_style && label->words)
    gtk_label_free_words (label);
}

static void
gtk_label_paint_word (GtkLabel     *label,
		      gint          x,
		      gint          y,
		      GtkLabelWord *word,
		      GdkRectangle *area)
{
  GtkWidget *widget = GTK_WIDGET (label);
  GtkLabelULine *uline;
  gchar *tmp_str;
  
  tmp_str = gdk_wcstombs (word->beginning);
  if (tmp_str)
    {
      gtk_paint_string (widget->style, widget->window, widget->state,
			area, widget, "label", 
			x + word->x,
			y + word->y,
			tmp_str);
      g_free (tmp_str);
    }
  
  for (uline = word->uline; uline; uline = uline->next)
    gtk_paint_hline (widget->style, widget->window, 
		     widget->state, area,
		     widget, "label", 
		     x + uline->x1, x + uline->x2, y + uline->y);
}

static gint
gtk_label_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkLabel *label;
  GtkMisc *misc;
  GtkLabelWord *word;
  gint x, y;
  
  g_return_val_if_fail (GTK_IS_LABEL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  label = GTK_LABEL (widget);
  
  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget) &&
      label->label && (*label->label != '\0'))
    {
      misc = GTK_MISC (widget);
      
      /*
       * GC Clipping
       */
      gdk_gc_set_clip_rectangle (widget->style->white_gc, &event->area);
      gdk_gc_set_clip_rectangle (widget->style->fg_gc[widget->state], &event->area);
      
      x = floor (widget->allocation.x + (gint)misc->xpad
		 + (((gint)widget->allocation.width - 
		    (gint)label->max_width - 2 * (gint)misc->xpad)
		    * misc->xalign) + 0.5);
      
      y = floor (widget->allocation.y + (gint)misc->ypad 
		 + (((gint)widget->allocation.height
		     - (gint)widget->requisition.height)
		    * misc->yalign) + 0.5);

      for (word = label->words; word; word = word->next)
	{
	  gchar save = word->beginning[word->length];
	  word->beginning[word->length] = '\0';
	  gtk_label_paint_word (label, x, y, word, &event->area);
	  word->beginning[word->length] = save;
	}
      
      gdk_gc_set_clip_mask (widget->style->white_gc, NULL);
      gdk_gc_set_clip_mask (widget->style->fg_gc[widget->state], NULL);
    }

  return TRUE;
}

guint      
gtk_label_parse_uline (GtkLabel    *label,
		       const gchar *string)
{
  guint accel_key = GDK_VoidSymbol;
  GdkWChar *p, *q, *string_wc;
  gchar *r;
  gchar *pattern;
  gchar *result_str;
  gint length, wc_length;
  gboolean underscore;
  
  g_return_val_if_fail (GTK_IS_LABEL (label), GDK_VoidSymbol);
  g_return_val_if_fail (string != NULL, GDK_VoidSymbol);

  /* Convert text to wide characters */
  length = strlen (string);
  string_wc = g_new (GdkWChar, length + 1);
  wc_length = gdk_mbstowcs (string_wc, string, length + 1);
  if (wc_length < 0)
    {
      g_free (string_wc);
      return GDK_VoidSymbol;
    }

  string_wc[wc_length] = '\0';
  
  pattern = g_new (gchar, length+1);
  
  underscore = FALSE;
  
  p = q = string_wc;
  r = pattern;
  
  while (*p)
    {
      if (underscore)
	{
	  if (*p == '_')
	    *r++ = ' ';
	  else
	    {
	      *r++ = '_';
	      if (accel_key == GDK_VoidSymbol)
		accel_key = gdk_keyval_to_lower (*p);
	    }
	  
	  *q++ = *p;
	  underscore = FALSE;
	}
      else
	{
	  if (*p == '_')
	    underscore = TRUE;
	  else
	    {
	      *q++ = *p;
	      *r++ = ' ';
	    }
	}
      p++;
    }
  *q = 0;
  *r = 0;

  result_str = gdk_wcstombs (string_wc);
  if (string)
    {
      gtk_label_set_text_internal (label, result_str, string_wc);
      gtk_label_set_pattern (label, pattern);
    }
  
  g_free (pattern);
  
  return accel_key;
}
