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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include "gtkprogressbar.h"
#include "gtksignal.h"


#define MIN_HORIZONTAL_BAR_WIDTH   150
#define MIN_HORIZONTAL_BAR_HEIGHT  20
#define MIN_VERTICAL_BAR_WIDTH     22
#define MIN_VERTICAL_BAR_HEIGHT    80
#define MAX_TEXT_LENGTH            80
#define TEXT_SPACING               2


static void gtk_progress_bar_class_init    (GtkProgressBarClass *klass);
static void gtk_progress_bar_init          (GtkProgressBar      *pbar);
static void gtk_progress_bar_size_request  (GtkWidget           *widget,
					    GtkRequisition      *requisition);
static void gtk_progress_bar_real_update   (GtkProgress         *progress);
static void gtk_progress_bar_paint         (GtkProgress         *progress);
static void gtk_progress_bar_act_mode_enter (GtkProgress        *progress);


guint
gtk_progress_bar_get_type (void)
{
  static guint progress_bar_type = 0;

  if (!progress_bar_type)
    {
      GtkTypeInfo progress_bar_info =
      {
	"GtkProgressBar",
	sizeof (GtkProgressBar),
	sizeof (GtkProgressBarClass),
	(GtkClassInitFunc) gtk_progress_bar_class_init,
	(GtkObjectInitFunc) gtk_progress_bar_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL
      };

      progress_bar_type = gtk_type_unique (gtk_progress_get_type (),
					   &progress_bar_info);
    }

  return progress_bar_type;
}

static void
gtk_progress_bar_class_init (GtkProgressBarClass *class)
{
  GtkWidgetClass *widget_class;
  GtkProgressClass *progress_class;

  widget_class = (GtkWidgetClass *) class;
  progress_class = (GtkProgressClass *) class;

  widget_class->size_request = gtk_progress_bar_size_request;

  progress_class->paint = gtk_progress_bar_paint;
  progress_class->update = gtk_progress_bar_real_update;
  progress_class->act_mode_enter = gtk_progress_bar_act_mode_enter;
}

static void
gtk_progress_bar_init (GtkProgressBar *pbar)
{
  GTK_WIDGET_SET_FLAGS (pbar, GTK_BASIC);

  pbar->bar_style = GTK_PROGRESS_CONTINUOUS;
  pbar->blocks = 10;
  pbar->in_block = -1;
  pbar->orientation = GTK_PROGRESS_LEFT_TO_RIGHT;
  pbar->activity_pos = 0;
  pbar->activity_dir = 1;
  pbar->activity_step = 3;
}


GtkWidget *
gtk_progress_bar_new (void)
{
  GtkProgressBar *pbar;

  pbar = gtk_type_new (gtk_progress_bar_get_type ());

  gtk_progress_bar_construct (pbar, NULL);

  return GTK_WIDGET (pbar);
}

GtkWidget *
gtk_progress_bar_new_with_adjustment (GtkAdjustment *adjustment)
{
  GtkProgressBar *pbar;

  pbar = gtk_type_new (gtk_progress_bar_get_type ());

  gtk_progress_bar_construct (pbar, adjustment);

  return GTK_WIDGET (pbar);
}

void
gtk_progress_bar_construct (GtkProgressBar *pbar,
			    GtkAdjustment  *adjustment)
{
  g_return_if_fail (pbar != NULL);
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  if (!adjustment)
    adjustment = (GtkAdjustment *) gtk_adjustment_new (0, 0, 100, 0, 0, 0);

  gtk_progress_set_adjustment (GTK_PROGRESS (pbar), adjustment);
}

static void
gtk_progress_bar_real_update (GtkProgress *progress)
{
  GtkProgressBar *pbar;
  GtkWidget *widget;

  g_return_if_fail (progress != NULL);
  g_return_if_fail (GTK_IS_PROGRESS (progress));

  pbar = GTK_PROGRESS_BAR (progress);
  widget = GTK_WIDGET (progress);
 
  if (pbar->bar_style == GTK_PROGRESS_CONTINUOUS ||
      GTK_PROGRESS (pbar)->activity_mode)
    {
      if (GTK_PROGRESS (pbar)->activity_mode)
	{
	  guint size;

	  /* advance the block */

	  if (pbar->orientation == GTK_PROGRESS_LEFT_TO_RIGHT ||
	      pbar->orientation == GTK_PROGRESS_RIGHT_TO_LEFT)
	    {
	      size = widget->allocation.height - 
		widget->style->klass->ythickness * 2;

	      if (pbar->activity_dir == 0)
		{
		  pbar->activity_pos += pbar->activity_step;
		  if (pbar->activity_pos + size >=
		      widget->allocation.width -
		      widget->style->klass->xthickness)
		    {
		      pbar->activity_pos = widget->allocation.width -
			widget->style->klass->xthickness - size;
		      pbar->activity_dir = 1;
		    }
		}
	      else
		{
		  pbar->activity_pos -= pbar->activity_step;
		  if (pbar->activity_pos <= widget->style->klass->xthickness)
		    {
		      pbar->activity_pos = widget->style->klass->xthickness;
		      pbar->activity_dir = 0;
		    }
		}
	    }
	  else
	    {
	      size = widget->allocation.width - 
		widget->style->klass->xthickness * 2;

	      if (pbar->activity_dir == 0)
		{
		  pbar->activity_pos += pbar->activity_step;
		  if (pbar->activity_pos + size >=
		      widget->allocation.height -
		      widget->style->klass->ythickness)
		    {
		      pbar->activity_pos = widget->allocation.height -
			widget->style->klass->ythickness - size;
		      pbar->activity_dir = 1;
		    }
		}
	      else
		{
		  pbar->activity_pos -= pbar->activity_step;
		  if (pbar->activity_pos <= widget->style->klass->ythickness)
		    {
		      pbar->activity_pos = widget->style->klass->ythickness;
		      pbar->activity_dir = 0;
		    }
		}
	    }
	}
      gtk_progress_bar_paint (progress);
      gtk_widget_queue_draw (GTK_WIDGET (progress));
    }
  else
    {
      gint in_block;
      
      in_block = -1 + (gint)(gtk_progress_get_current_percentage (progress) *
			     (gfloat)pbar->blocks);
      
      if (pbar->in_block != in_block)
	{
	  pbar->in_block = in_block;
	  gtk_progress_bar_paint (progress);
	  gtk_widget_queue_draw (GTK_WIDGET (progress));
	}
    }
}

static void
gtk_progress_bar_size_request (GtkWidget      *widget,
			       GtkRequisition *requisition)
{
  GtkProgress *progress;
  GtkProgressBar *pbar;
  gchar *buf;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PROGRESS_BAR (widget));
  g_return_if_fail (requisition != NULL);

  progress = GTK_PROGRESS (widget);
  pbar = GTK_PROGRESS_BAR (widget);

  if (pbar->orientation == GTK_PROGRESS_LEFT_TO_RIGHT ||
      pbar->orientation == GTK_PROGRESS_RIGHT_TO_LEFT)
    {
      if (progress->show_text && pbar->bar_style != GTK_PROGRESS_DISCRETE)
	{
	  buf = gtk_progress_get_text_from_value (progress,
						  progress->adjustment->upper);

	  requisition->width = MAX (MIN_HORIZONTAL_BAR_WIDTH,
				    2 * widget->style->klass->xthickness + 3 +
				    gdk_text_width (widget->style->font, 
						    buf, strlen (buf)) +
				    2 * TEXT_SPACING);

	  requisition->height = MAX (MIN_HORIZONTAL_BAR_HEIGHT,
				     2 * widget->style->klass->ythickness + 3 +
				     gdk_text_height (widget->style->font, 
						      buf, strlen (buf)) +
				     2 * TEXT_SPACING);
	  g_free (buf);
	}
      else
	{
	  requisition->width = MIN_HORIZONTAL_BAR_WIDTH;
	  requisition->height = MIN_HORIZONTAL_BAR_HEIGHT;
	}
    }
  else
    {
      if (progress->show_text && pbar->bar_style != GTK_PROGRESS_DISCRETE)
	{	  
	  buf = gtk_progress_get_text_from_value (progress,
						  progress->adjustment->upper);

	  requisition->width = MAX (MIN_VERTICAL_BAR_WIDTH,
				    2 * widget->style->klass->xthickness + 3 +
				    gdk_text_width (widget->style->font, 
						    buf, strlen (buf)) +
				    2 * TEXT_SPACING);

	  requisition->height = MAX (MIN_VERTICAL_BAR_HEIGHT,
				     2 * widget->style->klass->ythickness + 3 +
				     gdk_text_height (widget->style->font, 
						      buf, strlen (buf)) +
				     2 * TEXT_SPACING);
	  g_free (buf);
	}
      else
	{
	  requisition->width = MIN_VERTICAL_BAR_WIDTH;
	  requisition->height = MIN_VERTICAL_BAR_HEIGHT;
	}
    }
}

static void
gtk_progress_bar_act_mode_enter (GtkProgress *progress)
{
  GtkProgressBar *pbar;
  GtkWidget *widget;

  pbar = GTK_PROGRESS_BAR (progress);
  widget = GTK_WIDGET (progress);

  /* calculate start pos */

  if (pbar->orientation == GTK_PROGRESS_LEFT_TO_RIGHT ||
      pbar->orientation == GTK_PROGRESS_RIGHT_TO_LEFT)
    {
      if (pbar->orientation == GTK_PROGRESS_LEFT_TO_RIGHT)
	{
	  pbar->activity_pos = widget->style->klass->xthickness;
	  pbar->activity_dir = 0;
	}
      else
	{
	  pbar->activity_pos = widget->allocation.width - 
	    widget->style->klass->xthickness - (widget->allocation.height - 
		widget->style->klass->ythickness * 2);
	  pbar->activity_dir = 1;
	}
    }
  else
    {
      if (pbar->orientation == GTK_PROGRESS_TOP_TO_BOTTOM)
	{
	  pbar->activity_pos = widget->style->klass->ythickness;
	  pbar->activity_dir = 0;
	}
      else
	{
	  pbar->activity_pos = widget->allocation.height -
	    widget->style->klass->ythickness - (widget->allocation.width - 
		widget->style->klass->xthickness * 2);
	  pbar->activity_dir = 1;
	}
    }
}

static void
gtk_progress_bar_paint (GtkProgress *progress)
{
  GtkProgressBar *pbar;
  GtkWidget *widget;
  gint amount;
  gint block_delta = 0;
  gint space = 0;
  gint i;
  gint x;
  gint y;
  gfloat percentage;

  g_return_if_fail (progress != NULL);
  g_return_if_fail (GTK_IS_PROGRESS_BAR (progress));

  pbar = GTK_PROGRESS_BAR (progress);
  widget = GTK_WIDGET (progress);

  if (pbar->orientation == GTK_PROGRESS_LEFT_TO_RIGHT ||
      pbar->orientation == GTK_PROGRESS_RIGHT_TO_LEFT)
    space = widget->allocation.width -
      2 * widget->style->klass->xthickness;
  else
    space = widget->allocation.height -
      2 * widget->style->klass->ythickness;

  percentage = gtk_progress_get_current_percentage (progress);

  if (progress->offscreen_pixmap)
    {
      gtk_draw_shadow (widget->style,
		       progress->offscreen_pixmap,
		       GTK_STATE_NORMAL, GTK_SHADOW_IN, 0, 0,
		       widget->allocation.width,
		       widget->allocation.height);
	  
      gdk_draw_rectangle (progress->offscreen_pixmap,
			  widget->style->bg_gc[GTK_STATE_ACTIVE], TRUE,
			  widget->style->klass->xthickness,
			  widget->style->klass->ythickness,
			  widget->allocation.width -
			  widget->style->klass->xthickness * 2,
			  widget->allocation.height -
			  widget->style->klass->ythickness * 2);

      if (progress->activity_mode)
	{
	  if (pbar->orientation == GTK_PROGRESS_LEFT_TO_RIGHT ||
	      pbar->orientation == GTK_PROGRESS_RIGHT_TO_LEFT)
	    {
	      gdk_draw_rectangle (progress->offscreen_pixmap,
				  widget->style->bg_gc[GTK_STATE_PRELIGHT],
				  TRUE,
				  pbar->activity_pos,
				  widget->style->klass->ythickness,
				  widget->allocation.height - 
				  widget->style->klass->ythickness * 2,
				  widget->allocation.height - 
				  widget->style->klass->ythickness * 2);
	      
	      gtk_draw_shadow (widget->style,
			       progress->offscreen_pixmap,
			       GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
			       pbar->activity_pos,
			       widget->style->klass->ythickness,
			       widget->allocation.height - 
			       widget->style->klass->ythickness * 2,
			       widget->allocation.height -
			       widget->style->klass->ythickness * 2);
	      return;
	    }
	  else
	    {
	      gdk_draw_rectangle (progress->offscreen_pixmap,
				  widget->style->bg_gc[GTK_STATE_PRELIGHT],
				  TRUE,
				  widget->style->klass->xthickness,
				  pbar->activity_pos,
				  widget->allocation.width - 
				  widget->style->klass->xthickness * 2,
				  widget->allocation.width - 
				  widget->style->klass->xthickness * 2);
	      
	      gtk_draw_shadow (widget->style,
			       progress->offscreen_pixmap,
			       GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
			       widget->style->klass->xthickness,
			       pbar->activity_pos,
			       widget->allocation.width -
			       widget->style->klass->xthickness * 2,
			       widget->allocation.width - 
			       widget->style->klass->xthickness * 2);
	      return;
	    }
	}

      amount = percentage * space;
      
      if (amount > 0)
	{
	  switch (pbar->orientation)
	    {
	      
	    case GTK_PROGRESS_LEFT_TO_RIGHT:
	      
	      if (pbar->bar_style == GTK_PROGRESS_CONTINUOUS)
		{
		  gdk_draw_rectangle (progress->offscreen_pixmap,
				      widget->style->bg_gc[GTK_STATE_PRELIGHT],
				      TRUE,
				      widget->style->klass->xthickness,
				      widget->style->klass->ythickness,
				      amount,
				      widget->allocation.height - 
				      widget->style->klass->ythickness * 2);
		  gtk_draw_shadow (widget->style,
				   progress->offscreen_pixmap,
				   GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
				   widget->style->klass->xthickness,
				   widget->style->klass->ythickness,
				   amount,
				   widget->allocation.height -
				   widget->style->klass->ythickness * 2);
		}
	      else
		{
		  x = widget->style->klass->xthickness;
		  
		  for (i = 0; i <= pbar->in_block; i++)
		    {
		      block_delta = (((i + 1) * space) / pbar->blocks)
			- ((i * space) / pbar->blocks);
		      
		      gdk_draw_rectangle 
			(progress->offscreen_pixmap,
			 widget->style->bg_gc[GTK_STATE_PRELIGHT],
			 TRUE,
			 x,
			 widget->style->klass->ythickness,
			 block_delta,
			 widget->allocation.height - 
			 widget->style->klass->ythickness * 2);

		      gtk_draw_shadow (widget->style,
				       progress->offscreen_pixmap,
				       GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
				       x,
				       widget->style->klass->ythickness,
				       block_delta,
				       widget->allocation.height -
				       widget->style->klass->ythickness * 2);

		      x +=  block_delta;
		    }
		}
	      break;

	    case GTK_PROGRESS_RIGHT_TO_LEFT:

	      if (pbar->bar_style == GTK_PROGRESS_CONTINUOUS)
		{
		  gdk_draw_rectangle (progress->offscreen_pixmap,
				      widget->style->bg_gc[GTK_STATE_PRELIGHT],
				      TRUE,
				      widget->allocation.width - 
				      widget->style->klass->xthickness - amount,
				      widget->style->klass->ythickness,
				      amount,
				      widget->allocation.height - 
				      widget->style->klass->ythickness * 2);
		  gtk_draw_shadow (widget->style,
				   progress->offscreen_pixmap,
				   GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
				   widget->allocation.width - 
				   widget->style->klass->xthickness - amount,
				   widget->style->klass->ythickness,
				   amount,
				   widget->allocation.height -
				   widget->style->klass->ythickness * 2);
		}
	      else
		{
		  x = widget->allocation.width - 
		    widget->style->klass->xthickness;

		  for (i = 0; i <= pbar->in_block; i++)
		    {
		      block_delta = (((i + 1) * space) / pbar->blocks) -
			((i * space) / pbar->blocks);

		      x -=  block_delta;

		      gdk_draw_rectangle (progress->offscreen_pixmap,
				  widget->style->bg_gc[GTK_STATE_PRELIGHT],
				  TRUE,
				  x,
				  widget->style->klass->ythickness,
				  block_delta,
				  widget->allocation.height - 
				  widget->style->klass->ythickness * 2);

		      gtk_draw_shadow (widget->style,
				       progress->offscreen_pixmap,
				       GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
				       x,
				       widget->style->klass->ythickness,
				       block_delta,
				       widget->allocation.height -
				       widget->style->klass->ythickness * 2);
		    }
		}
	      break;

	    case GTK_PROGRESS_BOTTOM_TO_TOP:

	      if (pbar->bar_style == GTK_PROGRESS_CONTINUOUS)
		{
		  gdk_draw_rectangle (progress->offscreen_pixmap,
				      widget->style->bg_gc[GTK_STATE_PRELIGHT],
				      TRUE,
				      widget->style->klass->xthickness,
				      widget->allocation.height - 
				      widget->style->klass->ythickness - amount,
				      widget->allocation.width - 
				      widget->style->klass->xthickness * 2,
				      amount);
		  gtk_draw_shadow (widget->style,
				   progress->offscreen_pixmap,
				   GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
				   widget->style->klass->xthickness,
				   widget->allocation.height - 
				   widget->style->klass->ythickness - amount,
				   widget->allocation.width -
				   widget->style->klass->xthickness * 2,
				   amount);
		}
	      else
		{
		  y = widget->allocation.height - 
		    widget->style->klass->ythickness;

		  for (i = 0; i <= pbar->in_block; i++)
		    {
		      block_delta = (((i + 1) * space) / pbar->blocks) -
			((i * space) / pbar->blocks);
		      
		      y -= block_delta;

		      gdk_draw_rectangle 
			(progress->offscreen_pixmap,
			 widget->style->bg_gc[GTK_STATE_PRELIGHT],
			 TRUE,
			 widget->style->klass->xthickness,
			 y,
			 widget->allocation.width - 
			 widget->style->klass->xthickness * 2,
			 block_delta);

		      gtk_draw_shadow (widget->style,
				       progress->offscreen_pixmap,
				       GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
				       widget->style->klass->xthickness,
				       y,
				       widget->allocation.width - 
				       widget->style->klass->xthickness * 2,
				       block_delta);
		    }
		}
	      break;

	    case GTK_PROGRESS_TOP_TO_BOTTOM:

	      if (pbar->bar_style == GTK_PROGRESS_CONTINUOUS)
		{
		  gdk_draw_rectangle (progress->offscreen_pixmap,
				      widget->style->bg_gc[GTK_STATE_PRELIGHT],
				      TRUE,
				      widget->style->klass->xthickness,
				      widget->style->klass->ythickness,
				      widget->allocation.width -
				      widget->style->klass->xthickness * 2,
				      amount);
		  gtk_draw_shadow (widget->style,
				   progress->offscreen_pixmap,
				   GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
				   widget->style->klass->xthickness,
				   widget->style->klass->ythickness,
				   widget->allocation.width -
				   widget->style->klass->xthickness * 2,
				   amount);
		}
	      else
		{
		  y = widget->style->klass->ythickness;

		  for (i = 0; i <= pbar->in_block; i++)
		    {

		      block_delta = (((i + 1) * space) / pbar->blocks)
			- ((i * space) / pbar->blocks);

		      gdk_draw_rectangle
			(progress->offscreen_pixmap,
			 widget->style->bg_gc[GTK_STATE_PRELIGHT],
			 TRUE,
			 widget->style->klass->xthickness,
			 y,
			 widget->allocation.width -
			 widget->style->klass->xthickness * 2,
			 block_delta);

		      gtk_draw_shadow (widget->style,
				       progress->offscreen_pixmap,
				       GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
				       widget->style->klass->xthickness,
				       y,
				       widget->allocation.width -
				       widget->style->klass->xthickness * 2,
				       block_delta);

		      y += block_delta;
		    }
		}
	      break;

	    default:
	      break;
	    }
	}

      if (progress->show_text && pbar->bar_style != GTK_PROGRESS_DISCRETE)
	{
	  gint x;
	  gint y;
	  gchar *buf;
	  GdkRectangle rect;

	  buf = gtk_progress_get_current_text (progress);

	  x = widget->style->klass->xthickness + 1 + 
	    (widget->allocation.width - 2 * widget->style->klass->xthickness -
	     3 - gdk_text_width (widget->style->font, buf, strlen (buf)))
	    * progress->x_align; 

	  y = widget->style->font->ascent + 1 +
	    (widget->allocation.height - 2 * widget->style->klass->ythickness -
	     3 - gdk_text_height (widget->style->font, buf, strlen (buf)))
	    * progress->y_align;

	  rect.x = widget->style->klass->xthickness + 1;
	  rect.y = widget->style->klass->ythickness + 1;
	  rect.width = widget->allocation.width -
	    2 * widget->style->klass->xthickness - 3;
	  rect.height = widget->allocation.height -
	    2 * widget->style->klass->ythickness - 3;

	  gdk_gc_set_clip_rectangle (widget->style->fg_gc[widget->state],
				     &rect);

	  gdk_draw_text (progress->offscreen_pixmap, widget->style->font,
			 widget->style->fg_gc[widget->state],
			 x, y, buf, strlen (buf));

	  gdk_gc_set_clip_rectangle (widget->style->fg_gc[widget->state],
				     NULL);
	  g_free (buf);
 	}
    }
}

/*******************************************************************/

void
gtk_progress_bar_update (GtkProgressBar *pbar,
			 gfloat          percentage)
{
  g_return_if_fail (pbar != NULL);
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  /***********************************************************************
   *                 Use of this function is deprecated !	         * 
   * Use gtk_progress_set_value or gtk_progress_set_percentage instead.  *
   ***********************************************************************/

  gtk_progress_set_percentage (GTK_PROGRESS (pbar), percentage);
}

void
gtk_progress_bar_set_orientation (GtkProgressBar           *pbar,
				  GtkProgressBarOrientation orientation)
{
  g_return_if_fail (pbar != NULL);
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  if (pbar->orientation != orientation)
    {
      pbar->orientation = orientation;

      if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (pbar)))
	gtk_widget_queue_resize (GTK_WIDGET (pbar));
    }
}

void
gtk_progress_bar_set_bar_style (GtkProgressBar     *pbar,
				GtkProgressBarStyle bar_style)
{
  g_return_if_fail (pbar != NULL);
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  if (pbar->bar_style != bar_style)
    {
      pbar->bar_style = bar_style;

      if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (pbar)))
	gtk_widget_queue_resize (GTK_WIDGET (pbar));
    }
}

void
gtk_progress_bar_set_number_of_blocks (GtkProgressBar *pbar,
				       guint           blocks)
{
  g_return_if_fail (pbar != NULL);
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));
  g_return_if_fail (blocks > 1);

  if (pbar->blocks != blocks)
    {
      pbar->blocks = blocks;

      if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (pbar)))
	gtk_widget_queue_resize (GTK_WIDGET (pbar));
    }
}

void
gtk_progress_bar_set_activity_step (GtkProgressBar *pbar,
                                    guint           step)
{
  g_return_if_fail (pbar != NULL);
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  if (pbar->activity_step != step)
    pbar->activity_step = step;
}
