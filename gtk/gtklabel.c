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
#include <string.h>
#include "gtklabel.h"
#include "gdk/gdkkeysyms.h"

enum {
  ARG_0,
  ARG_LABEL,
  ARG_PATTERN,
  ARG_JUSTIFY
};

typedef struct _GtkLabelRow GtkLabelRow;

struct _GtkLabelRow {
  gint index;
  gint width;
  gint height;
  gint len;
};

GMemChunk *row_mem_chunk = NULL;

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
static gint gtk_label_expose	   (GtkWidget	   *widget,
				    GdkEventExpose *event);
static void gtk_label_state_changed (GtkWidget	    *widget,
				     guint	     previous_state);
static void gtk_label_style_set	    (GtkWidget	    *widget,
				     GtkStyle	    *previous_style);
static void gtk_label_free_rows     (GtkLabel       *label);



static GtkMiscClass *parent_class = NULL;


GtkType
gtk_label_get_type (void)
{
  static GtkType label_type = 0;
  
  if (!label_type)
    {
      GtkTypeInfo label_info =
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
      
      label_type = gtk_type_unique (gtk_misc_get_type (), &label_info);
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
  
  parent_class = gtk_type_class (gtk_misc_get_type ());
  
  gtk_object_add_arg_type ("GtkLabel::label", GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_LABEL);
  gtk_object_add_arg_type ("GtkLabel::pattern", GTK_TYPE_STRING, GTK_ARG_READWRITE, ARG_PATTERN);
  gtk_object_add_arg_type ("GtkLabel::justify", GTK_TYPE_JUSTIFICATION, GTK_ARG_READWRITE, ARG_JUSTIFY);

  object_class->set_arg = gtk_label_set_arg;
  object_class->get_arg = gtk_label_get_arg;
  object_class->finalize = gtk_label_finalize;
  
  widget_class->size_request = gtk_label_size_request;
  widget_class->expose_event = gtk_label_expose;
  widget_class->style_set    = gtk_label_style_set;
  widget_class->state_changed = gtk_label_state_changed;
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
      gtk_label_set (label, GTK_VALUE_STRING (*arg) ? GTK_VALUE_STRING (*arg) : "");
      break;
    case ARG_PATTERN:
      gtk_label_set_pattern (label, GTK_VALUE_STRING (*arg));
      break;
    case ARG_JUSTIFY:
      gtk_label_set_justify (label, GTK_VALUE_ENUM (*arg));
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
  label->row = NULL;
  label->max_width = 0;
  label->jtype = GTK_JUSTIFY_CENTER;
  label->needs_clear = FALSE;
  label->pattern = NULL;
  
  gtk_label_set (label, "");
}

GtkWidget*
gtk_label_new (const gchar *str)
{
  GtkLabel *label;
  
  g_return_val_if_fail (str != NULL, NULL);
  
  label = gtk_type_new (gtk_label_get_type ());
  
  gtk_label_set (label, str);
  
  return GTK_WIDGET (label);
}

void
gtk_label_set (GtkLabel	   *label,
	       const gchar *str)
{
  char* p;
  
  g_return_if_fail (label != NULL);
  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (str != NULL);

  if (!row_mem_chunk)
    row_mem_chunk = g_mem_chunk_create (GtkLabelRow, 64, G_ALLOC_AND_FREE);
  
  if (label->label)
    g_free (label->label);
  label->label = g_strdup (str);
  
  if (label->row)
    gtk_label_free_rows (label);

  p = label->label;
  do {
    GtkLabelRow *row = g_chunk_new (GtkLabelRow, row_mem_chunk);
    label->row = g_slist_append (label->row, row);

    row->index = p - label->label;
    
    p = strchr(p, '\n');
    if (p)
      {
	p++;
	row->len = (p - label->label) - row->index;
      }
    else
      row->len = strlen (label->label) - row->index;
  } while (p);

  if (GTK_WIDGET_VISIBLE (label))
    {
      if (GTK_WIDGET_MAPPED (label))
	gdk_window_clear_area (GTK_WIDGET (label)->window,
			       GTK_WIDGET (label)->allocation.x,
			       GTK_WIDGET (label)->allocation.y,
			       GTK_WIDGET (label)->allocation.width,
			       GTK_WIDGET (label)->allocation.height);
      
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

void
gtk_label_set_pattern (GtkLabel	   *label,
		       const gchar *pattern)
{
  g_return_if_fail (label != NULL);
  g_return_if_fail (GTK_IS_LABEL (label));
  
  if (label->pattern)
    g_free (label->pattern);
  label->pattern = g_strdup (pattern);
  
  if (GTK_WIDGET_VISIBLE (label))
    {
      if (GTK_WIDGET_MAPPED (label))
	gdk_window_clear_area (GTK_WIDGET (label)->window,
			       GTK_WIDGET (label)->allocation.x,
			       GTK_WIDGET (label)->allocation.y,
			       GTK_WIDGET (label)->allocation.width,
			       GTK_WIDGET (label)->allocation.height);
      
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

void
gtk_label_set_justify (GtkLabel	       *label,
		       GtkJustification jtype)
{
  g_return_if_fail (label != NULL);
  g_return_if_fail (GTK_IS_LABEL (label));
  
  if ((GtkJustification) label->jtype != jtype)
    {
      label->jtype = jtype;
      
      if (GTK_WIDGET_VISIBLE (label))
	{
	  if (GTK_WIDGET_MAPPED (label))
	    gdk_window_clear_area (GTK_WIDGET (label)->window,
				   GTK_WIDGET (label)->allocation.x,
				   GTK_WIDGET (label)->allocation.y,
				   GTK_WIDGET (label)->allocation.width,
				   GTK_WIDGET (label)->allocation.height);
	  
	  gtk_widget_queue_resize (GTK_WIDGET (label));
	}
    }
}

void
gtk_label_get (GtkLabel	 *label,
	       gchar	**str)
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
  gtk_label_free_rows (label);
  
  (* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

static gint
gtk_label_process_row (GtkLabel    *label,
		       GtkLabelRow *row,
		       gint         x, gint y,
		       gboolean     draw)
{
  GtkWidget *widget = GTK_WIDGET (label);
  
  char lastc;
  gint i, j;
  gint offset;
  gint height;
  gint pattern_length;

  if (label->pattern) 
    pattern_length = strlen (label->pattern);
  else
    pattern_length = 0;

  offset = 0;
  height = widget->style->font->ascent + widget->style->font->descent;
  
  if (draw)
    {
      if (label->jtype == GTK_JUSTIFY_CENTER)
	offset = (label->max_width - row->width) / 2;
      else if (label->jtype == GTK_JUSTIFY_RIGHT)
	offset = (label->max_width - row->width);
    }
      
  if (label->pattern && (row->index < pattern_length))
    lastc = label->pattern[0];
  else
    lastc = ' ';
  
  j = 0;
  for (i=1; i<=row->len; i++)
    {
      char c;

      if (label->pattern && (row->index + i < pattern_length))
	c = label->pattern[row->index+i];
      else
	c = ' ';

      if ((i == row->len) || (lastc != c))
	{
	  gint width = 0;

	  if (lastc == '_')
	    {
	      gint descent;
	      gint rbearing;
	      gint lbearing;

	      gdk_text_extents (widget->style->font,
				&label->label[row->index+j], i - j,
				&lbearing, &rbearing, &width, NULL, &descent);

	      if (draw)
		{
		  if (widget->state == GTK_STATE_INSENSITIVE)
		    gdk_draw_line (widget->window,
				   widget->style->white_gc,
				   offset + x + lbearing, y + descent + 2, 
				   offset + x + rbearing + 1, y + descent + 2);
	      
		  gdk_draw_line (widget->window,
				 widget->style->fg_gc[widget->state],
				 offset + x + lbearing - 1, y + descent + 1, 
				 offset + x + rbearing, y + descent + 1);
		}

	      height = MAX (height, 
			    widget->style->font->ascent + descent + 2);
	    }
	  else if (i != row->len)
	    {
	      width = gdk_text_width (widget->style->font, 
				      &label->label[row->index+j], 
				      i - j);
	    }

	  if (draw)
	    {
	      if (widget->state == GTK_STATE_INSENSITIVE)
		gdk_draw_text (widget->window, widget->style->font,
			       widget->style->white_gc,
			       offset + x + 1, y + 1,
			       &label->label[row->index+j], i - j);
	      
	      gdk_draw_text (widget->window, widget->style->font,
			     widget->style->fg_gc[widget->state],
			     offset + x, y, 
			     &label->label[row->index+j], i - j);
	    }

		  
	  offset += width;

	  if (i != row->len)
	    {
	      lastc = c;
	      j = i;
	    }
	}
    }

  return height;
}

static void
gtk_label_size_request (GtkWidget      *widget,
			GtkRequisition *requisition)
{
  GtkLabel *label;
  GSList *tmp_list;
  gint width;
  
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LABEL (widget));
  g_return_if_fail (requisition != NULL);
  
  label = GTK_LABEL (widget);
  
  requisition->height = label->misc.ypad * 2;

  tmp_list = label->row;
  width = 0;
  while (tmp_list)
    {
      GtkLabelRow *row = tmp_list->data;

      row->width = gdk_text_width (GTK_WIDGET (label)->style->font,
				   &label->label [row->index],
				   row->len);
      width = MAX (width, row->width);

      requisition->height += gtk_label_process_row (label, row, 0, 0, FALSE) + 2;

      tmp_list = tmp_list->next;
    }

  label->max_width = width;
  requisition->width = width + label->misc.xpad * 2;
}
  
static gint
gtk_label_expose (GtkWidget	 *widget,
		  GdkEventExpose *event)
{
  GtkLabel *label;
  GtkMisc *misc;
  GSList *tmp_list;
  gint x, y;
  
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_LABEL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);
  
  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget))
    {
      label = GTK_LABEL (widget);
      misc = GTK_MISC (widget);
      
      /*
       * GC Clipping
       */
      gdk_gc_set_clip_rectangle (widget->style->white_gc, &event->area);
      gdk_gc_set_clip_rectangle (widget->style->fg_gc[widget->state], &event->area);
      
      /* We clear the whole allocation here so that if a partial
       * expose is triggered we don't just clear part and mess up
       * when the queued redraw comes along. (There will always
       * be a complete queued redraw when the needs_clear flag
       * is set.)
       */
      if (label->needs_clear)
	{
	  gdk_window_clear_area (widget->window,
				 widget->allocation.x,
				 widget->allocation.y,
				 widget->allocation.width,
				 widget->allocation.height);
	  
	  label->needs_clear = FALSE;
	}
      
      x = widget->allocation.x + misc->xpad +
	(widget->allocation.width - (label->max_width + label->misc.xpad * 2))
	* misc->xalign + 0.5;
      
      y = (widget->allocation.y * (1.0 - misc->yalign) +
	   (widget->allocation.y + widget->allocation.height -
	    (widget->requisition.height - misc->ypad * 2)) *
	   misc->yalign + widget->style->font->ascent) + 1.5;
      
      /* 
       * COMMENT: we can avoid gdk_text_width() calls here storing in label->row
       * the widths of the rows calculated in gtk_label_set.
       * Once we have a wrapping interface we can support GTK_JUSTIFY_FILL.
       */

      tmp_list = label->row;
      while (tmp_list)
	{
	  y += gtk_label_process_row (label, tmp_list->data, x, y, TRUE) + 2;
	  tmp_list = tmp_list->next;
	}
      
      gdk_gc_set_clip_mask (widget->style->white_gc, NULL);
      gdk_gc_set_clip_mask (widget->style->fg_gc[widget->state], NULL);
      
    }
  return TRUE;
}

static void
gtk_label_free_rows (GtkLabel *label)
{
  GSList *tmp_list;

  tmp_list = label->row;
  while (tmp_list)
    {
      g_mem_chunk_free (row_mem_chunk, tmp_list->data);
      tmp_list = tmp_list->next;
    }
  g_slist_free (label->row);
  label->row = NULL;
}

static void 
gtk_label_state_changed (GtkWidget	*widget,
			 guint		 previous_state)
{
  if (GTK_WIDGET_DRAWABLE (widget))
    GTK_LABEL (widget)->needs_clear = TRUE;
}

static void 
gtk_label_style_set (GtkWidget	*widget,
		     GtkStyle	*previous_style)
{
  if (GTK_WIDGET_DRAWABLE (widget))
    GTK_LABEL (widget)->needs_clear = TRUE;
}

guint      
gtk_label_parse_uline (GtkLabel         *label,
		       const gchar      *string)
{
  guint accel_key = GDK_VoidSymbol;
  const gchar *p;
  gchar *q, *r;
  gchar *name, *pattern;

  gint length;
  gboolean underscore;

  length = strlen (string);
  
  name = g_new (gchar, length+1);
  pattern = g_new (gchar, length+1);

  underscore = FALSE;

  p = string;
  q = name;
  r = pattern;
  underscore = FALSE;

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

  gtk_label_set (label, name);
  gtk_label_set_pattern (label, pattern);
  
  g_free (name);
  g_free (pattern);

  return accel_key;
}


