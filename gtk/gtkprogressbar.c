/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#if HAVE_CONFIG_H
#  include <config.h>
#  if STDC_HEADERS
#    include <string.h>
#    include <stdio.h>
#  endif
#else
#  include <stdio.h>
#endif

#include "gtkprogressbar.h"
#include "gtkintl.h"
#include "gtkalias.h"


#define MIN_HORIZONTAL_BAR_WIDTH   150
#define MIN_HORIZONTAL_BAR_HEIGHT  20
#define MIN_VERTICAL_BAR_WIDTH     22
#define MIN_VERTICAL_BAR_HEIGHT    80
#define MAX_TEXT_LENGTH            80
#define TEXT_SPACING               2

enum {
  PROP_0,

  /* Supported args */
  PROP_FRACTION,
  PROP_PULSE_STEP,
  PROP_ORIENTATION,
  PROP_TEXT,
  PROP_ELLIPSIZE,
  
  /* Deprecated args */
  PROP_ADJUSTMENT,
  PROP_BAR_STYLE,
  PROP_ACTIVITY_STEP,
  PROP_ACTIVITY_BLOCKS,
  PROP_DISCRETE_BLOCKS
};

static void gtk_progress_bar_class_init    (GtkProgressBarClass *klass);
static void gtk_progress_bar_init          (GtkProgressBar      *pbar);
static void gtk_progress_bar_set_property  (GObject             *object,
					    guint                prop_id,
					    const GValue        *value,
					    GParamSpec          *pspec);
static void gtk_progress_bar_get_property  (GObject             *object,
					    guint                prop_id,
					    GValue              *value,
					    GParamSpec          *pspec);
static void gtk_progress_bar_size_request  (GtkWidget           *widget,
					    GtkRequisition      *requisition);
static void gtk_progress_bar_real_update   (GtkProgress         *progress);
static void gtk_progress_bar_paint         (GtkProgress         *progress);
static void gtk_progress_bar_act_mode_enter (GtkProgress        *progress);

static void gtk_progress_bar_set_bar_style_internal       (GtkProgressBar *pbar,
							   GtkProgressBarStyle style);
static void gtk_progress_bar_set_discrete_blocks_internal (GtkProgressBar *pbar,
							   guint           blocks);
static void gtk_progress_bar_set_activity_step_internal   (GtkProgressBar *pbar,
							   guint           step);
static void gtk_progress_bar_set_activity_blocks_internal (GtkProgressBar *pbar,
							   guint           blocks);


GType
gtk_progress_bar_get_type (void)
{
  static GType progress_bar_type = 0;

  if (!progress_bar_type)
    {
      static const GTypeInfo progress_bar_info =
      {
	sizeof (GtkProgressBarClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_progress_bar_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkProgressBar),
	0,		/* n_preallocs */
	(GInstanceInitFunc) gtk_progress_bar_init,
      };

      progress_bar_type =
	g_type_register_static (GTK_TYPE_PROGRESS, "GtkProgressBar",
				&progress_bar_info, 0);
    }

  return progress_bar_type;
}

static void
gtk_progress_bar_class_init (GtkProgressBarClass *class)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;
  GtkProgressClass *progress_class;
  
  gobject_class = G_OBJECT_CLASS (class);
  widget_class = (GtkWidgetClass *) class;
  progress_class = (GtkProgressClass *) class;

  gobject_class->set_property = gtk_progress_bar_set_property;
  gobject_class->get_property = gtk_progress_bar_get_property;
  
  widget_class->size_request = gtk_progress_bar_size_request;

  progress_class->paint = gtk_progress_bar_paint;
  progress_class->update = gtk_progress_bar_real_update;
  progress_class->act_mode_enter = gtk_progress_bar_act_mode_enter;

  g_object_class_install_property (gobject_class,
                                   PROP_ADJUSTMENT,
                                   g_param_spec_object ("adjustment",
                                                        P_("Adjustment"),
                                                        P_("The GtkAdjustment connected to the progress bar (Deprecated)"),
                                                        GTK_TYPE_ADJUSTMENT,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_ORIENTATION,
                                   g_param_spec_enum ("orientation",
						      P_("Orientation"),
						      P_("Orientation and growth direction of the progress bar"),
						      GTK_TYPE_PROGRESS_BAR_ORIENTATION,
						      GTK_PROGRESS_LEFT_TO_RIGHT,
						      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_BAR_STYLE,
                                   g_param_spec_enum ("bar_style",
						      P_("Bar style"),
						      P_("Specifies the visual style of the bar in percentage mode (Deprecated)"),
						      GTK_TYPE_PROGRESS_BAR_STYLE,
						      GTK_PROGRESS_CONTINUOUS,
						      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVITY_STEP,
                                   g_param_spec_uint ("activity_step",
						      P_("Activity Step"),
						      P_("The increment used for each iteration in activity mode (Deprecated)"),
						      -G_MAXUINT,
						      G_MAXUINT,
						      3,
						      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVITY_BLOCKS,
                                   g_param_spec_uint ("activity_blocks",
						      P_("Activity Blocks"),
						      P_("The number of blocks which can fit in the progress bar area in activity mode (Deprecated)"),
						      2,
						      G_MAXUINT,
						      5,
						      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_DISCRETE_BLOCKS,
                                   g_param_spec_uint ("discrete_blocks",
						      P_("Discrete Blocks"),
						      P_("The number of discrete blocks in a progress bar (when shown in the discrete style)"),
						      2,
						      G_MAXUINT,
						      10,
						      G_PARAM_READWRITE));
  
  g_object_class_install_property (gobject_class,
				   PROP_FRACTION,
				   g_param_spec_double ("fraction",
							P_("Fraction"),
							P_("The fraction of total work that has been completed"),
							0.0,
							1.0,
							0.0,
							G_PARAM_READWRITE));  
  
  g_object_class_install_property (gobject_class,
				   PROP_PULSE_STEP,
				   g_param_spec_double ("pulse_step",
							P_("Pulse Step"),
							P_("The fraction of total progress to move the bouncing block when pulsed"),
							0.0,
							1.0,
							0.1,
							G_PARAM_READWRITE));  
  
  g_object_class_install_property (gobject_class,
				   PROP_TEXT,
				   g_param_spec_string ("text",
							P_("Text"),
							P_("Text to be displayed in the progress bar"),
							"%P %%",
							G_PARAM_READWRITE));

  /**
   * GtkProgressBar:ellipsize:
   *
   * The preferred place to ellipsize the string, if the progressbar does 
   * not have enough room to display the entire string, specified as a 
   * #PangoEllisizeMode. 
   *
   * Note that setting this property to a value other than 
   * %PANGO_ELLIPSIZE_NONE has the side-effect that the progressbar requests 
   * only enough space to display the ellipsis "...". Another means to set a 
   * progressbar's width is gtk_widget_set_size_request().
   *
   * Since: 2.6
   */
  g_object_class_install_property (gobject_class,
				   PROP_ELLIPSIZE,
                                   g_param_spec_enum ("ellipsize",
                                                      P_("Ellipsize"),
                                                      P_("The preferred place to ellipsize the string, if the progressbar does not have enough room to display the entire string, if at all"),
						      PANGO_TYPE_ELLIPSIZE_MODE,
						      PANGO_ELLIPSIZE_NONE,
                                                      G_PARAM_READWRITE));

}

static void
gtk_progress_bar_init (GtkProgressBar *pbar)
{
  pbar->bar_style = GTK_PROGRESS_CONTINUOUS;
  pbar->blocks = 10;
  pbar->in_block = -1;
  pbar->orientation = GTK_PROGRESS_LEFT_TO_RIGHT;
  pbar->pulse_fraction = 0.1;
  pbar->activity_pos = 0;
  pbar->activity_dir = 1;
  pbar->activity_step = 3;
  pbar->activity_blocks = 5;
  pbar->ellipsize = PANGO_ELLIPSIZE_NONE;
}

static void
gtk_progress_bar_set_property (GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
  GtkProgressBar *pbar;

  pbar = GTK_PROGRESS_BAR (object);

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      gtk_progress_set_adjustment (GTK_PROGRESS (pbar),
				   GTK_ADJUSTMENT (g_value_get_object (value)));
      break;
    case PROP_ORIENTATION:
      gtk_progress_bar_set_orientation (pbar, g_value_get_enum (value));
      break;
    case PROP_BAR_STYLE:
      gtk_progress_bar_set_bar_style_internal (pbar, g_value_get_enum (value));
      break;
    case PROP_ACTIVITY_STEP:
      gtk_progress_bar_set_activity_step_internal (pbar, g_value_get_uint (value));
      break;
    case PROP_ACTIVITY_BLOCKS:
      gtk_progress_bar_set_activity_blocks_internal (pbar, g_value_get_uint (value));
      break;
    case PROP_DISCRETE_BLOCKS:
      gtk_progress_bar_set_discrete_blocks_internal (pbar, g_value_get_uint (value));
      break;
    case PROP_FRACTION:
      gtk_progress_bar_set_fraction (pbar, g_value_get_double (value));
      break;
    case PROP_PULSE_STEP:
      gtk_progress_bar_set_pulse_step (pbar, g_value_get_double (value));
      break;
    case PROP_TEXT:
      gtk_progress_bar_set_text (pbar, g_value_get_string (value));
      break;
    case PROP_ELLIPSIZE:
      gtk_progress_bar_set_ellipsize (pbar, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_progress_bar_get_property (GObject      *object,
			       guint         prop_id,
			       GValue       *value,
			       GParamSpec   *pspec)
{
  GtkProgressBar *pbar;

  pbar = GTK_PROGRESS_BAR (object);

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      g_value_set_object (value, GTK_PROGRESS (pbar)->adjustment);
      break;
    case PROP_ORIENTATION:
      g_value_set_enum (value, pbar->orientation);
      break;
    case PROP_BAR_STYLE:
      g_value_set_enum (value, pbar->bar_style);
      break;
    case PROP_ACTIVITY_STEP:
      g_value_set_uint (value, pbar->activity_step);
      break;
    case PROP_ACTIVITY_BLOCKS:
      g_value_set_uint (value, pbar->activity_blocks);
      break;
    case PROP_DISCRETE_BLOCKS:
      g_value_set_uint (value, pbar->blocks);
      break;
    case PROP_FRACTION:
      g_value_set_double (value, gtk_progress_get_current_percentage (GTK_PROGRESS (pbar)));
      break;
    case PROP_PULSE_STEP:
      g_value_set_double (value, pbar->pulse_fraction);
      break;
    case PROP_TEXT:
      g_value_set_string (value, gtk_progress_bar_get_text (pbar));
      break;
    case PROP_ELLIPSIZE:
      g_value_set_enum (value, pbar->ellipsize);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

GtkWidget*
gtk_progress_bar_new (void)
{
  GtkWidget *pbar;

  pbar = gtk_widget_new (GTK_TYPE_PROGRESS_BAR, NULL);

  return pbar;
}

GtkWidget*
gtk_progress_bar_new_with_adjustment (GtkAdjustment *adjustment)
{
  GtkWidget *pbar;

  g_return_val_if_fail (GTK_IS_ADJUSTMENT (adjustment), NULL);

  pbar = gtk_widget_new (GTK_TYPE_PROGRESS_BAR,
			 "adjustment", adjustment,
			 NULL);

  return pbar;
}

static void
gtk_progress_bar_real_update (GtkProgress *progress)
{
  GtkProgressBar *pbar;
  GtkWidget *widget;

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
              /* Update our activity step. */
              
              pbar->activity_step = widget->allocation.width * pbar->pulse_fraction;
              
	      size = MAX (2, widget->allocation.width / pbar->activity_blocks);

	      if (pbar->activity_dir == 0)
		{
		  pbar->activity_pos += pbar->activity_step;
		  if (pbar->activity_pos + size >=
		      widget->allocation.width -
		      widget->style->xthickness)
		    {
		      pbar->activity_pos = widget->allocation.width -
			widget->style->xthickness - size;
		      pbar->activity_dir = 1;
		    }
		}
	      else
		{
		  pbar->activity_pos -= pbar->activity_step;
		  if (pbar->activity_pos <= widget->style->xthickness)
		    {
		      pbar->activity_pos = widget->style->xthickness;
		      pbar->activity_dir = 0;
		    }
		}
	    }
	  else
	    {
              /* Update our activity step. */
              
              pbar->activity_step = widget->allocation.height * pbar->pulse_fraction;
              
	      size = MAX (2, widget->allocation.height / pbar->activity_blocks);

	      if (pbar->activity_dir == 0)
		{
		  pbar->activity_pos += pbar->activity_step;
		  if (pbar->activity_pos + size >=
		      widget->allocation.height -
		      widget->style->ythickness)
		    {
		      pbar->activity_pos = widget->allocation.height -
			widget->style->ythickness - size;
		      pbar->activity_dir = 1;
		    }
		}
	      else
		{
		  pbar->activity_pos -= pbar->activity_step;
		  if (pbar->activity_pos <= widget->style->ythickness)
		    {
		      pbar->activity_pos = widget->style->ythickness;
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
			     (gdouble)pbar->blocks);
      
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
  PangoRectangle logical_rect;
  PangoLayout *layout;
  gint width, height;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (widget));
  g_return_if_fail (requisition != NULL);

  progress = GTK_PROGRESS (widget);
  pbar = GTK_PROGRESS_BAR (widget);

  width = 2 * widget->style->xthickness + 3 + 2 * TEXT_SPACING;
  height = 2 * widget->style->ythickness + 3 + 2 * TEXT_SPACING;

  if (progress->show_text && pbar->bar_style != GTK_PROGRESS_DISCRETE)
    {
      if (!progress->adjustment)
	gtk_progress_set_adjustment (progress, NULL);

      buf = gtk_progress_get_text_from_value (progress, progress->adjustment->upper);

      layout = gtk_widget_create_pango_layout (widget, buf);

      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
	  
      if (pbar->ellipsize)
	{
	  PangoContext *context;
	  PangoFontMetrics *metrics;
	  gint char_width;
	  
	  /* The minimum size for ellipsized text is ~ 3 chars */
	  context = pango_layout_get_context (layout);
	  metrics = pango_context_get_metrics (context, widget->style->font_desc, pango_context_get_language (context));
	  
	  char_width = pango_font_metrics_get_approximate_char_width (metrics);
	  pango_font_metrics_unref (metrics);
	  
	  width += PANGO_PIXELS (char_width) * 3;
	}
      else
	width += logical_rect.width;
      
      height += logical_rect.height;

      g_object_unref (layout);
      g_free (buf);
    }
  
  if (pbar->orientation == GTK_PROGRESS_LEFT_TO_RIGHT ||
      pbar->orientation == GTK_PROGRESS_RIGHT_TO_LEFT)
    {
      if (progress->show_text && pbar->bar_style != GTK_PROGRESS_DISCRETE)
	{
	  requisition->width = MAX (MIN_HORIZONTAL_BAR_WIDTH, width);
	  requisition->height = MAX (MIN_HORIZONTAL_BAR_HEIGHT, height);
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
	  requisition->width = MAX (MIN_VERTICAL_BAR_WIDTH, width);
	  requisition->height = MAX (MIN_VERTICAL_BAR_HEIGHT, height);
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
  gint size;
  GtkProgressBarOrientation orientation;

  pbar = GTK_PROGRESS_BAR (progress);
  widget = GTK_WIDGET (progress);

  orientation = pbar->orientation;
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) 
    {
      if (pbar->orientation == GTK_PROGRESS_LEFT_TO_RIGHT)
	orientation = GTK_PROGRESS_RIGHT_TO_LEFT;
      else if (pbar->orientation == GTK_PROGRESS_RIGHT_TO_LEFT)
	orientation = GTK_PROGRESS_LEFT_TO_RIGHT;
    }
  
  /* calculate start pos */

  if (orientation == GTK_PROGRESS_LEFT_TO_RIGHT ||
      orientation == GTK_PROGRESS_RIGHT_TO_LEFT)
    {
      size = MAX (2, widget->allocation.width / pbar->activity_blocks);

      if (orientation == GTK_PROGRESS_LEFT_TO_RIGHT)
	{
	  pbar->activity_pos = widget->style->xthickness;
	  pbar->activity_dir = 0;
	}
      else
	{
	  pbar->activity_pos = widget->allocation.width - 
	    widget->style->xthickness - (widget->allocation.height - 
		widget->style->ythickness * 2);
	  pbar->activity_dir = 1;
	}
    }
  else
    {
      size = MAX (2, widget->allocation.height / pbar->activity_blocks);

      if (orientation == GTK_PROGRESS_TOP_TO_BOTTOM)
	{
	  pbar->activity_pos = widget->style->ythickness;
	  pbar->activity_dir = 0;
	}
      else
	{
	  pbar->activity_pos = widget->allocation.height -
	    widget->style->ythickness - (widget->allocation.width - 
		widget->style->xthickness * 2);
	  pbar->activity_dir = 1;
	}
    }
}

static void
gtk_progress_bar_paint_activity (GtkProgressBar            *pbar,
				 GtkProgressBarOrientation  orientation)
{
  GtkWidget *widget = GTK_WIDGET (pbar);
  GtkProgress *progress = GTK_PROGRESS (pbar);
  gint x, y, w, h;

  switch (orientation)
    {
    case GTK_PROGRESS_LEFT_TO_RIGHT:
    case GTK_PROGRESS_RIGHT_TO_LEFT:
      x = pbar->activity_pos;
      y = widget->style->ythickness;
      w = MAX (2, widget->allocation.width / pbar->activity_blocks);
      h = widget->allocation.height - 2 * widget->style->ythickness;
      break;

    case GTK_PROGRESS_TOP_TO_BOTTOM:
    case GTK_PROGRESS_BOTTOM_TO_TOP:
      x = widget->style->xthickness;
      y = pbar->activity_pos;
      w = widget->allocation.width - 2 * widget->style->xthickness;
      h = MAX (2, widget->allocation.height / pbar->activity_blocks);
      break;

    default:
      return;
      break;
    }

  gtk_paint_box (widget->style,
		 progress->offscreen_pixmap,
		 GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
		 NULL, widget, "bar",
		 x, y, w, h);
}

static void
gtk_progress_bar_paint_continuous (GtkProgressBar            *pbar,
				   gint                       amount,
				   GtkProgressBarOrientation  orientation)
{
  GtkWidget *widget = GTK_WIDGET (pbar);
  gint x, y, w, h;

  if (amount <= 0)
    return;

  switch (orientation)
    {
    case GTK_PROGRESS_LEFT_TO_RIGHT:
    case GTK_PROGRESS_RIGHT_TO_LEFT:
      w = amount;
      h = widget->allocation.height - widget->style->ythickness * 2;
      y = widget->style->ythickness;
      
      x = widget->style->xthickness;
      if (orientation == GTK_PROGRESS_RIGHT_TO_LEFT)
	x = widget->allocation.width - amount - x;
      break;
      
    case GTK_PROGRESS_TOP_TO_BOTTOM:
    case GTK_PROGRESS_BOTTOM_TO_TOP:
      w = widget->allocation.width - widget->style->xthickness * 2;
      h = amount;
      x = widget->style->xthickness;
      
      y = widget->style->ythickness;
      if (orientation == GTK_PROGRESS_BOTTOM_TO_TOP)
	y = widget->allocation.height - amount - y;
      break;
      
    default:
      return;
      break;
    }
  
  gtk_paint_box (widget->style,
		 GTK_PROGRESS (pbar)->offscreen_pixmap,
		 GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
		 NULL, widget, "bar",
		 x, y, w, h);
}

static void
gtk_progress_bar_paint_discrete (GtkProgressBar            *pbar,
				 GtkProgressBarOrientation  orientation)
{
  GtkWidget *widget = GTK_WIDGET (pbar);
  gint i;

  for (i = 0; i <= pbar->in_block; i++)
    {
      gint x, y, w, h, space;

      switch (orientation)
	{
	case GTK_PROGRESS_LEFT_TO_RIGHT:
	case GTK_PROGRESS_RIGHT_TO_LEFT:
	  space = widget->allocation.width - 2 * widget->style->xthickness;
	  
	  x = widget->style->xthickness + (i * space) / pbar->blocks;
	  y = widget->style->ythickness;
	  w = widget->style->xthickness + ((i + 1) * space) / pbar->blocks - x;
	  h = widget->allocation.height - 2 * widget->style->ythickness;

	  if (orientation == GTK_PROGRESS_RIGHT_TO_LEFT)
	    x = widget->allocation.width - w - x;
	  break;
	  
	case GTK_PROGRESS_TOP_TO_BOTTOM:
	case GTK_PROGRESS_BOTTOM_TO_TOP:
	  space = widget->allocation.height - 2 * widget->style->ythickness;
	  
	  x = widget->style->xthickness;
	  y = widget->style->ythickness + (i * space) / pbar->blocks;
	  w = widget->allocation.width - 2 * widget->style->xthickness;
	  h = widget->style->ythickness + ((i + 1) * space) / pbar->blocks - y;
	  
	  if (orientation == GTK_PROGRESS_BOTTOM_TO_TOP)
	    y = widget->allocation.height - h - y;
	  break;

	default:
	  return;
	  break;
	}
      
      gtk_paint_box (widget->style,
		     GTK_PROGRESS (pbar)->offscreen_pixmap,
		     GTK_STATE_PRELIGHT, GTK_SHADOW_OUT,
		     NULL, widget, "bar",
		     x, y, w, h);
    }
}

static void
gtk_progress_bar_paint_text (GtkProgressBar            *pbar,
			     gint			amount,
			     GtkProgressBarOrientation  orientation)
{
  GtkProgress *progress = GTK_PROGRESS (pbar);
  GtkWidget *widget = GTK_WIDGET (pbar);
  
  gint x;
  gint y;
  gchar *buf;
  GdkRectangle rect;
  PangoLayout *layout;
  PangoRectangle logical_rect;
  GdkRectangle prelight_clip, normal_clip;
  
  buf = gtk_progress_get_current_text (progress);
  
  layout = gtk_widget_create_pango_layout (widget, buf);
  pango_layout_set_ellipsize (layout, pbar->ellipsize);
  if (pbar->ellipsize)
    pango_layout_set_width (layout, widget->allocation.width * PANGO_SCALE);

  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
  
  x = widget->style->xthickness + 1 +
    (widget->allocation.width - 2 * widget->style->xthickness -
     2 - logical_rect.width)
    * progress->x_align; 

  y = widget->style->ythickness + 1 +
    (widget->allocation.height - 2 * widget->style->ythickness -
     2 - logical_rect.height)
    * progress->y_align;

  rect.x = widget->style->xthickness;
  rect.y = widget->style->ythickness;
  rect.width = widget->allocation.width - 2 * widget->style->xthickness;
  rect.height = widget->allocation.height - 2 * widget->style->ythickness;

  prelight_clip = normal_clip = rect;

  switch (orientation)
    {
    case GTK_PROGRESS_LEFT_TO_RIGHT:
      prelight_clip.width = amount;
      normal_clip.x += amount;
      normal_clip.width -= amount;
      break;
      
    case GTK_PROGRESS_RIGHT_TO_LEFT:
      normal_clip.width -= amount;
      prelight_clip.x += normal_clip.width;
      prelight_clip.width -= normal_clip.width;
      break;
       
    case GTK_PROGRESS_TOP_TO_BOTTOM:
      prelight_clip.height = amount;
      normal_clip.y += amount;
      normal_clip.height -= amount;
      break;
      
    case GTK_PROGRESS_BOTTOM_TO_TOP:
      normal_clip.height -= amount;
      prelight_clip.y += normal_clip.height;
      prelight_clip.height -= normal_clip.height;
      break;
    }
  
  gtk_paint_layout (widget->style,
		    progress->offscreen_pixmap,
		    GTK_STATE_PRELIGHT,
		    FALSE,
		    &prelight_clip,
		    widget,
		    "progressbar",
		    x, y,
		    layout);
  
  gtk_paint_layout (widget->style,
		    progress->offscreen_pixmap,
		    GTK_STATE_NORMAL,
		    FALSE,
		    &normal_clip,
		    widget,
		    "progressbar",
		    x, y,
		    layout);

  g_object_unref (layout);
  g_free (buf);
}

static void
gtk_progress_bar_paint (GtkProgress *progress)
{
  GtkProgressBar *pbar;
  GtkWidget *widget;

  GtkProgressBarOrientation orientation;

  g_return_if_fail (GTK_IS_PROGRESS_BAR (progress));

  pbar = GTK_PROGRESS_BAR (progress);
  widget = GTK_WIDGET (progress);

  orientation = pbar->orientation;
  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) 
    {
      if (pbar->orientation == GTK_PROGRESS_LEFT_TO_RIGHT)
	orientation = GTK_PROGRESS_RIGHT_TO_LEFT;
      else if (pbar->orientation == GTK_PROGRESS_RIGHT_TO_LEFT)
	orientation = GTK_PROGRESS_LEFT_TO_RIGHT;
    }
 
  if (progress->offscreen_pixmap)
    {
      gtk_paint_box (widget->style,
		     progress->offscreen_pixmap,
		     GTK_STATE_NORMAL, GTK_SHADOW_IN, 
		     NULL, widget, "trough",
		     0, 0,
		     widget->allocation.width,
		     widget->allocation.height);
      
      if (progress->activity_mode)
	{
	  gtk_progress_bar_paint_activity (pbar, orientation);
	}
      else
	{
	  gint amount;
	  gint space;
	  
	  if (orientation == GTK_PROGRESS_LEFT_TO_RIGHT ||
	      orientation == GTK_PROGRESS_RIGHT_TO_LEFT)
	    space = widget->allocation.width - 2 * widget->style->xthickness;
	  else
	    space = widget->allocation.height - 2 * widget->style->ythickness;
	  
	  amount = space *
	    gtk_progress_get_current_percentage (GTK_PROGRESS (pbar));
	  
	  if (pbar->bar_style == GTK_PROGRESS_CONTINUOUS)
	    {
	      gtk_progress_bar_paint_continuous (pbar, amount, orientation);

	      if (GTK_PROGRESS (pbar)->show_text)
		gtk_progress_bar_paint_text (pbar, amount, orientation);
	    }
	  else
	    gtk_progress_bar_paint_discrete (pbar, orientation);
	}
    }
}

static void
gtk_progress_bar_set_bar_style_internal (GtkProgressBar     *pbar,
					 GtkProgressBarStyle bar_style)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  if (pbar->bar_style != bar_style)
    {
      pbar->bar_style = bar_style;

      if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (pbar)))
	gtk_widget_queue_resize (GTK_WIDGET (pbar));

      g_object_notify (G_OBJECT (pbar), "bar-style");
    }
}

static void
gtk_progress_bar_set_discrete_blocks_internal (GtkProgressBar *pbar,
					       guint           blocks)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));
  g_return_if_fail (blocks > 1);

  if (pbar->blocks != blocks)
    {
      pbar->blocks = blocks;

      if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (pbar)))
	gtk_widget_queue_resize (GTK_WIDGET (pbar));

      g_object_notify (G_OBJECT (pbar), "discrete-blocks");
    }
}

static void
gtk_progress_bar_set_activity_step_internal (GtkProgressBar *pbar,
					     guint           step)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  if (pbar->activity_step != step)
    {
      pbar->activity_step = step;
      g_object_notify (G_OBJECT (pbar), "activity-step");
    }
}

static void
gtk_progress_bar_set_activity_blocks_internal (GtkProgressBar *pbar,
					       guint           blocks)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));
  g_return_if_fail (blocks > 1);

  if (pbar->activity_blocks != blocks)
    {
      pbar->activity_blocks = blocks;
      g_object_notify (G_OBJECT (pbar), "activity-blocks");
    }
}

/*******************************************************************/

/**
 * gtk_progress_bar_set_fraction:
 * @pbar: a #GtkProgressBar
 * @fraction: fraction of the task that's been completed
 * 
 * Causes the progress bar to "fill in" the given fraction
 * of the bar. The fraction should be between 0.0 and 1.0,
 * inclusive.
 * 
 **/
void
gtk_progress_bar_set_fraction (GtkProgressBar *pbar,
                               gdouble         fraction)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  /* If we know the percentage, we don't want activity mode. */
  gtk_progress_set_activity_mode (GTK_PROGRESS (pbar), FALSE);
  
  /* We use the deprecated GtkProgress interface internally.
   * Once everything's been deprecated for a good long time,
   * we can clean up all this code.
   */
  gtk_progress_set_percentage (GTK_PROGRESS (pbar), fraction);

  g_object_notify (G_OBJECT (pbar), "fraction");
}

/**
 * gtk_progress_bar_pulse:
 * @pbar: a #GtkProgressBar
 * 
 * Indicates that some progress is made, but you don't know how much.
 * Causes the progress bar to enter "activity mode," where a block
 * bounces back and forth. Each call to gtk_progress_bar_pulse()
 * causes the block to move by a little bit (the amount of movement
 * per pulse is determined by gtk_progress_bar_set_pulse_step()).
 **/
void
gtk_progress_bar_pulse (GtkProgressBar *pbar)
{  
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  /* If we don't know the percentage, we must want activity mode. */
  gtk_progress_set_activity_mode (GTK_PROGRESS (pbar), TRUE);

  /* Sigh. */
  gtk_progress_bar_real_update (GTK_PROGRESS (pbar));
}

/**
 * gtk_progress_bar_set_text:
 * @pbar: a #GtkProgressBar
 * @text: a UTF-8 string
 * 
 * Causes the given @text to appear superimposed on the progress bar.
 **/
void
gtk_progress_bar_set_text (GtkProgressBar *pbar,
                           const gchar    *text)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));
  
  gtk_progress_set_show_text (GTK_PROGRESS (pbar), text && *text);
  gtk_progress_set_format_string (GTK_PROGRESS (pbar), text);
  
  /* We don't support formats in this interface, but turn
   * them back on for NULL, which should put us back to
   * the initial state.
   */
  GTK_PROGRESS (pbar)->use_text_format = (text == NULL);
  
  g_object_notify (G_OBJECT (pbar), "text");
}

/**
 * gtk_progress_bar_set_pulse_step:
 * @pbar: a #GtkProgressBar
 * @fraction: fraction between 0.0 and 1.0
 * 
 * Sets the fraction of total progress bar length to move the
 * bouncing block for each call to gtk_progress_bar_pulse().
 **/
void
gtk_progress_bar_set_pulse_step   (GtkProgressBar *pbar,
                                   gdouble         fraction)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));
  
  pbar->pulse_fraction = fraction;

  g_object_notify (G_OBJECT (pbar), "pulse-step");
}

void
gtk_progress_bar_update (GtkProgressBar *pbar,
			 gdouble         percentage)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  /* Use of gtk_progress_bar_update() is deprecated ! 
   * Use gtk_progress_bar_set_percentage ()
   */   

  gtk_progress_set_percentage (GTK_PROGRESS (pbar), percentage);
}

/**
 * gtk_progress_bar_set_orientation:
 * @pbar: a #GtkProgressBar
 * @orientation: orientation of the progress bar
 * 
 * Causes the progress bar to switch to a different orientation
 * (left-to-right, right-to-left, top-to-bottom, or bottom-to-top). 
 **/
void
gtk_progress_bar_set_orientation (GtkProgressBar           *pbar,
				  GtkProgressBarOrientation orientation)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  if (pbar->orientation != orientation)
    {
      pbar->orientation = orientation;

      if (GTK_WIDGET_DRAWABLE (GTK_WIDGET (pbar)))
	gtk_widget_queue_resize (GTK_WIDGET (pbar));

      g_object_notify (G_OBJECT (pbar), "orientation");
    }
}

/**
 * gtk_progress_bar_get_text:
 * @pbar: a #GtkProgressBar
 * 
 * Retrieves the text displayed superimposed on the progress bar,
 * if any, otherwise %NULL. The return value is a reference
 * to the text, not a copy of it, so will become invalid
 * if you change the text in the progress bar.
 * 
 * Return value: text, or %NULL; this string is owned by the widget
 * and should not be modified or freed.
 **/
G_CONST_RETURN gchar*
gtk_progress_bar_get_text (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), NULL);

  if (GTK_PROGRESS (pbar)->use_text_format)
    return NULL;
  else
    return GTK_PROGRESS (pbar)->format;
}

/**
 * gtk_progress_bar_get_fraction:
 * @pbar: a #GtkProgressBar
 * 
 * Returns the current fraction of the task that's been completed.
 * 
 * Return value: a fraction from 0.0 to 1.0
 **/
gdouble
gtk_progress_bar_get_fraction (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), 0);

  return gtk_progress_get_current_percentage (GTK_PROGRESS (pbar));
}

/**
 * gtk_progress_bar_get_pulse_step:
 * @pbar: a #GtkProgressBar
 * 
 * Retrieves the pulse step set with gtk_progress_bar_set_pulse_step()
 * 
 * Return value: a fraction from 0.0 to 1.0
 **/
gdouble
gtk_progress_bar_get_pulse_step (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), 0);

  return pbar->pulse_fraction;
}

/**
 * gtk_progress_bar_get_orientation:
 * @pbar: a #GtkProgressBar
 * 
 * Retrieves the current progress bar orientation.
 * 
 * Return value: orientation of the progress bar
 **/
GtkProgressBarOrientation
gtk_progress_bar_get_orientation (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), 0);

  return pbar->orientation;
}

void
gtk_progress_bar_set_bar_style (GtkProgressBar     *pbar,
				GtkProgressBarStyle bar_style)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  gtk_progress_bar_set_bar_style_internal (pbar, bar_style);
}

void
gtk_progress_bar_set_discrete_blocks (GtkProgressBar *pbar,
				      guint           blocks)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));
  g_return_if_fail (blocks > 1);

  gtk_progress_bar_set_discrete_blocks_internal (pbar, blocks);
}

void
gtk_progress_bar_set_activity_step (GtkProgressBar *pbar,
                                    guint           step)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));

  gtk_progress_bar_set_activity_step_internal (pbar, step);
}

void
gtk_progress_bar_set_activity_blocks (GtkProgressBar *pbar,
				      guint           blocks)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));
  g_return_if_fail (blocks > 1);

  gtk_progress_bar_set_activity_blocks_internal (pbar, blocks);
}

/**
 * gtk_progress_bar_set_ellipsize:
 * @pbar: a #GtkProgressBar
 * @mode: a #PangoEllipsizeMode
 *
 * Sets the mode used to ellipsize (add an ellipsis: "...") the text 
 * if there is not enough space to render the entire string.
 *
 * Since: 2.6
 **/
void
gtk_progress_bar_set_ellipsize (GtkProgressBar     *pbar,
				PangoEllipsizeMode  mode)
{
  g_return_if_fail (GTK_IS_PROGRESS_BAR (pbar));
  g_return_if_fail (mode >= PANGO_ELLIPSIZE_NONE && 
		    mode <= PANGO_ELLIPSIZE_END);
  
  if ((PangoEllipsizeMode)pbar->ellipsize != mode)
    {
      pbar->ellipsize = mode;

      g_object_notify (G_OBJECT (pbar), "ellipsize");
      gtk_widget_queue_resize (GTK_WIDGET (pbar));
    }
}

/**
 * gtk_progress_bar_get_ellipsize:
 * @pbar: a #GtkProgressBar
 *
 * Returns the ellipsizing position of the progressbar. 
 * See gtk_progress_bar_set_ellipsize().
 *
 * Return value: #PangoEllipsizeMode
 *
 * Since: 2.6
 **/
PangoEllipsizeMode 
gtk_progress_bar_get_ellipsize (GtkProgressBar *pbar)
{
  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (pbar), PANGO_ELLIPSIZE_NONE);

  return pbar->ellipsize;
}

#define __GTK_PROGRESS_BAR_C__
#include "gtkaliasdef.c"
