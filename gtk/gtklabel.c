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
#include <string.h>
#include "gtklabel.h"


enum {
  ARG_0,
  ARG_LABEL,
  ARG_JUSTIFY
};

static void gtk_label_class_init   (GtkLabelClass  *klass);
static void gtk_label_init         (GtkLabel       *label);
static void gtk_label_set_arg	   (GtkLabel       *label,
				    GtkArg         *arg,
				    guint           arg_id);
static void gtk_label_get_arg	   (GtkLabel       *label,
				    GtkArg         *arg,
				    guint           arg_id);
static void gtk_label_finalize     (GtkObject      *object);
static void gtk_label_size_request (GtkWidget      *widget,
				    GtkRequisition *requisition);
static gint gtk_label_expose       (GtkWidget      *widget,
				    GdkEventExpose *event);
static void gtk_label_state_changed (GtkWidget      *widget,
				     guint	     previous_state);
static void gtk_label_style_set	    (GtkWidget      *widget,
				     GtkStyle       *previous_style);



static GtkMiscClass *parent_class = NULL;


guint
gtk_label_get_type ()
{
  static guint label_type = 0;

  if (!label_type)
    {
      GtkTypeInfo label_info =
      {
	"GtkLabel",
	sizeof (GtkLabel),
	sizeof (GtkLabelClass),
	(GtkClassInitFunc) gtk_label_class_init,
	(GtkObjectInitFunc) gtk_label_init,
	(GtkArgSetFunc) gtk_label_set_arg,
        (GtkArgGetFunc) gtk_label_get_arg,
      };

      label_type = gtk_type_unique (gtk_misc_get_type (), &label_info);
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
  gtk_object_add_arg_type ("GtkLabel::justify", GTK_TYPE_ENUM, GTK_ARG_READWRITE, ARG_JUSTIFY);

  object_class->finalize = gtk_label_finalize;

  widget_class->size_request = gtk_label_size_request;
  widget_class->expose_event = gtk_label_expose;
  widget_class->style_set    = gtk_label_style_set;
  widget_class->state_changed = gtk_label_state_changed;
}

static void
gtk_label_set_arg (GtkLabel       *label,
		   GtkArg         *arg,
		   guint           arg_id)
{
  switch (arg_id)
    {
    case ARG_LABEL:
      gtk_label_set (label, GTK_VALUE_STRING (*arg));
      break;
    case ARG_JUSTIFY:
      gtk_label_set_justify (label, GTK_VALUE_ENUM (*arg));
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void gtk_label_get_arg      (GtkLabel       *label,
				    GtkArg         *arg,
				    guint           arg_id)
{
  switch (arg_id)
    {
    case ARG_LABEL:
      GTK_VALUE_STRING (*arg) = g_strdup (label->label);
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
  label->jtype = GTK_JUSTIFY_CENTER;

  gtk_label_set (label, "");
}

GtkWidget*
gtk_label_new (const char *str)
{
  GtkLabel *label;

  g_return_val_if_fail (str != NULL, NULL);

  label = gtk_type_new (gtk_label_get_type ());

  gtk_label_set (label, str);

  return GTK_WIDGET (label);
}

void
gtk_label_set (GtkLabel *label,
	       const char *str)
{
  char* p;

  g_return_if_fail (label != NULL);
  g_return_if_fail (GTK_IS_LABEL (label));
  g_return_if_fail (str != NULL);

  if (label->label)
    g_free (label->label);
  label->label = g_strdup (str);

  if (label->row)
    g_slist_free (label->row);
  label->row = NULL;
  label->row = g_slist_append (label->row, label->label);
  p = label->label;
  while ((p = strchr(p, '\n')))
    label->row = g_slist_append (label->row, ++p);

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
gtk_label_set_justify (GtkLabel *label, GtkJustification jtype)
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
gtk_label_get (GtkLabel  *label,
	       char     **str)
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
  g_slist_free (label->row);
  
  (* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
gtk_label_size_request (GtkWidget      *widget,
			GtkRequisition *requisition)
{
  GtkLabel *label;
  GSList *row;
  gint width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_LABEL (widget));
  g_return_if_fail (requisition != NULL);

  label = GTK_LABEL (widget);

  row = label->row;
  width = 0;
  while (row)
    {
      if (row->next)
  	  width = MAX (width,
                       gdk_text_width (GTK_WIDGET (label)->style->font, row->data,
                                       (gchar*) row->next->data - (gchar*) row->data));
      else
        width = MAX (width, gdk_string_width (GTK_WIDGET (label)->style->font, row->data));
      row = row->next;
    }
  
  requisition->width = width + label->misc.xpad * 2;
  requisition->height = ((GTK_WIDGET (label)->style->font->ascent +
                          GTK_WIDGET (label)->style->font->descent + 2) *
                         g_slist_length(label->row) +
                         label->misc.ypad * 2);
}

static gint
gtk_label_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkLabel *label;
  GtkMisc *misc;
  GSList *row;
  gint state;
  gint offset;
  gint len;
  gint maxl;
  gint x, y;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_LABEL (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget))
    {
      label = GTK_LABEL (widget);
      misc = GTK_MISC (widget);

      state = widget->state;

      maxl = widget->requisition.width - misc->xpad * 2;

      /*
       * GC Clipping
       */
      gdk_gc_set_clip_rectangle (widget->style->white_gc, &event->area);
      gdk_gc_set_clip_rectangle (widget->style->fg_gc[state], &event->area);

      x = widget->allocation.x + misc->xpad +
	(widget->allocation.width - widget->requisition.width) 
	* misc->xalign + 0.5;

      y = (widget->allocation.y * (1.0 - misc->yalign) +
	   (widget->allocation.y + widget->allocation.height -
	    (widget->requisition.height - misc->ypad * 2)) *
	   misc->yalign + widget->style->font->ascent) + 1.5;
          
      row = label->row;
      while (row && row->next)
	{
	  len = (gchar*) row->next->data - (gchar*) row->data;
	  offset = 0;

	  if (label->jtype == GTK_JUSTIFY_CENTER)
	    offset = (maxl - gdk_text_width (widget->style->font, row->data, len)) / 2;

	  else if (label->jtype == GTK_JUSTIFY_RIGHT)
	    offset = (maxl - gdk_text_width (widget->style->font, row->data, len));

	  if (state == GTK_STATE_INSENSITIVE)
	    gdk_draw_text (widget->window, widget->style->font,
			   widget->style->white_gc,
			   offset + x + 1, y + 1, row->data, len);
	  
	  gdk_draw_text (widget->window, widget->style->font,
			 widget->style->fg_gc[state],
			 offset + x, y, row->data, len);
	  row = row->next;
	  y += widget->style->font->ascent + widget->style->font->descent + 2;
	}
      
      /* 
       * COMMENT: we can avoid gdk_text_width() calls here storing in label->row
       * the widths of the rows calculated in gtk_label_set.
       * Once we have a wrapping interface we can support GTK_JUSTIFY_FILL.
       */
      offset = 0;

      if (label->jtype == GTK_JUSTIFY_CENTER)
	offset = (maxl - gdk_string_width (widget->style->font, row->data)) / 2;

      else if (label->jtype == GTK_JUSTIFY_RIGHT)
	offset = (maxl - gdk_string_width (widget->style->font, row->data));

      if (state == GTK_STATE_INSENSITIVE)
	gdk_draw_string (widget->window, widget->style->font,
			 widget->style->white_gc,
			 offset + x + 1, y + 1, row->data);
      
      gdk_draw_string (widget->window, widget->style->font,
		       widget->style->fg_gc[state],
		       offset + x, y, row->data);

      gdk_gc_set_clip_mask (widget->style->white_gc, NULL);
      gdk_gc_set_clip_mask (widget->style->fg_gc[state], NULL);

    }
  return TRUE;
}

static void 
gtk_label_state_changed (GtkWidget      *widget,
			 guint	         previous_state)
{
  if (GTK_WIDGET_NO_WINDOW (widget) &&
      GTK_WIDGET_DRAWABLE (widget))
    gdk_window_clear_area (widget->window,
			   widget->allocation.x,
			   widget->allocation.y,
			   widget->allocation.width,
			   widget->allocation.height);
}

static void 
gtk_label_style_set	(GtkWidget      *widget,
			 GtkStyle       *previous_style)
{
  if (GTK_WIDGET_NO_WINDOW (widget) &&
      GTK_WIDGET_DRAWABLE (widget))
    gdk_window_clear_area (widget->window,
			   widget->allocation.x,
			   widget->allocation.y,
			   widget->allocation.width,
			   widget->allocation.height);
}
