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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <math.h>
#include "gtkcontainer.h"
#include "gtkscale.h"


#define SCALE_CLASS(w)  GTK_SCALE_CLASS (GTK_OBJECT (w)->klass)

enum {
  ARG_0,
  ARG_DIGITS,
  ARG_DRAW_VALUE,
  ARG_VALUE_POS
};


static void gtk_scale_class_init      (GtkScaleClass *klass);
static void gtk_scale_init            (GtkScale      *scale);
static void gtk_scale_set_arg         (GtkObject     *object,
				       GtkArg        *arg,
				       guint          arg_id);
static void gtk_scale_get_arg         (GtkObject     *object,
				       GtkArg        *arg,
				       guint          arg_id);
static void gtk_scale_map             (GtkWidget     *widget);
static void gtk_scale_unmap           (GtkWidget     *widget);

static void gtk_scale_draw_background (GtkRange      *range);


static GtkRangeClass *parent_class = NULL;


GtkType
gtk_scale_get_type (void)
{
  static GtkType scale_type = 0;

  if (!scale_type)
    {
      static const GtkTypeInfo scale_info =
      {
	"GtkScale",
	sizeof (GtkScale),
	sizeof (GtkScaleClass),
	(GtkClassInitFunc) gtk_scale_class_init,
	(GtkObjectInitFunc) gtk_scale_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      scale_type = gtk_type_unique (GTK_TYPE_RANGE, &scale_info);
    }

  return scale_type;
}

static void
gtk_scale_class_init (GtkScaleClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkRangeClass *range_class;

  object_class = (GtkObjectClass*) class;
  range_class = (GtkRangeClass*) class;
  widget_class = (GtkWidgetClass*) class;
  
  parent_class = gtk_type_class (GTK_TYPE_RANGE);
  
  gtk_object_add_arg_type ("GtkScale::digits",
			   GTK_TYPE_INT,
			   GTK_ARG_READWRITE,
			   ARG_DIGITS);
  gtk_object_add_arg_type ("GtkScale::draw_value",
			   GTK_TYPE_BOOL,
			   GTK_ARG_READWRITE,
			   ARG_DRAW_VALUE);
  gtk_object_add_arg_type ("GtkScale::value_pos",
			   GTK_TYPE_POSITION_TYPE,
			   GTK_ARG_READWRITE,
			   ARG_VALUE_POS);

  object_class->set_arg = gtk_scale_set_arg;
  object_class->get_arg = gtk_scale_get_arg;

  widget_class->map = gtk_scale_map;
  widget_class->unmap = gtk_scale_unmap;
  
  range_class->draw_background = gtk_scale_draw_background;

  class->slider_length = 31;
  class->value_spacing = 2;
  class->draw_value = NULL;
}

static void
gtk_scale_set_arg (GtkObject      *object,
		   GtkArg         *arg,
		   guint           arg_id)
{
  GtkScale *scale;

  scale = GTK_SCALE (object);

  switch (arg_id)
    {
    case ARG_DIGITS:
      gtk_scale_set_digits (scale, GTK_VALUE_INT (*arg));
      break;
    case ARG_DRAW_VALUE:
      gtk_scale_set_draw_value (scale, GTK_VALUE_BOOL (*arg));
      break;
    case ARG_VALUE_POS:
      gtk_scale_set_value_pos (scale, GTK_VALUE_ENUM (*arg));
      break;
    default:
      break;
    }
}

static void
gtk_scale_get_arg (GtkObject      *object,
		   GtkArg         *arg,
		   guint           arg_id)
{
  GtkScale *scale;

  scale = GTK_SCALE (object);

  switch (arg_id)
    {
    case ARG_DIGITS:
      GTK_VALUE_INT (*arg) = GTK_RANGE (scale)->digits;
      break;
    case ARG_DRAW_VALUE:
      GTK_VALUE_BOOL (*arg) = scale->draw_value;
      break;
    case ARG_VALUE_POS:
      GTK_VALUE_ENUM (*arg) = scale->value_pos;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_scale_init (GtkScale *scale)
{
  GTK_WIDGET_SET_FLAGS (scale, GTK_CAN_FOCUS);
  GTK_WIDGET_SET_FLAGS (scale, GTK_NO_WINDOW);
  GTK_RANGE (scale)->digits = 1;
  scale->draw_value = TRUE;
  scale->value_pos = GTK_POS_TOP;
}

static void
gtk_scale_map (GtkWidget *widget)
{
  GtkRange *range;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SCALE (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
  range = GTK_RANGE (widget);

  gdk_window_show (range->trough);
}

static void
gtk_scale_unmap (GtkWidget *widget)
{
  GtkRange *range;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_SCALE (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);
  range = GTK_RANGE (widget);

  if (GTK_WIDGET_NO_WINDOW (widget))
     gtk_widget_queue_clear (widget);

  gdk_window_hide (range->trough);
}

void
gtk_scale_set_digits (GtkScale *scale,
		      gint      digits)
{
  g_return_if_fail (scale != NULL);
  g_return_if_fail (GTK_IS_SCALE (scale));

  digits = CLAMP (digits, -1, 16);

  if (GTK_RANGE (scale)->digits != digits)
    {
      GTK_RANGE (scale)->digits = digits;

      gtk_widget_queue_resize (GTK_WIDGET (scale));
    }
}

void
gtk_scale_set_draw_value (GtkScale *scale,
			  gboolean  draw_value)
{
  g_return_if_fail (scale != NULL);
  g_return_if_fail (GTK_IS_SCALE (scale));

  draw_value = draw_value != FALSE;

  if (scale->draw_value != draw_value)
    {
      scale->draw_value = draw_value;

      gtk_widget_queue_resize (GTK_WIDGET (scale));
    }
}

void
gtk_scale_set_value_pos (GtkScale        *scale,
			 GtkPositionType  pos)
{
  g_return_if_fail (scale != NULL);
  g_return_if_fail (GTK_IS_SCALE (scale));

  if (scale->value_pos != pos)
    {
      scale->value_pos = pos;

      if (GTK_WIDGET_VISIBLE (scale) && GTK_WIDGET_MAPPED (scale))
	gtk_widget_queue_resize (GTK_WIDGET (scale));
    }
}

gint
gtk_scale_get_value_width (GtkScale *scale)
{
  GtkRange *range;
  gchar buffer[128];
  gfloat value;
  gint temp;
  gint return_val;
  gint digits;
  gint i, j;

  g_return_val_if_fail (scale != NULL, 0);
  g_return_val_if_fail (GTK_IS_SCALE (scale), 0);

  return_val = 0;
  if (scale->draw_value)
    {
      range = GTK_RANGE (scale);

      value = ABS (range->adjustment->lower);
      if (value == 0) value = 1;
      digits = log10 (value) + 1;
      if (digits > 13)
	digits = 13;

      i = 0;
      if (range->adjustment->lower < 0)
        buffer[i++] = '-';
      for (j = 0; j < digits; j++)
        buffer[i++] = '0';
      if (GTK_RANGE (scale)->digits)
        buffer[i++] = '.';
      for (j = 0; j < GTK_RANGE (scale)->digits; j++)
        buffer[i++] = '0';
      buffer[i] = '\0';

      return_val = gdk_string_measure (GTK_WIDGET (scale)->style->font, buffer);

      value = ABS (range->adjustment->upper);
      if (value == 0) value = 1;
      digits = log10 (value) + 1;
      if (digits > 13)
        digits = 13;

      i = 0;
      if (range->adjustment->upper < 0)
        buffer[i++] = '-';
      for (j = 0; j < digits; j++)
        buffer[i++] = '0';
      if (GTK_RANGE (scale)->digits)
        buffer[i++] = '.';
      for (j = 0; j < GTK_RANGE (scale)->digits; j++)
        buffer[i++] = '0';
      buffer[i] = '\0';

      temp = gdk_string_measure (GTK_WIDGET (scale)->style->font, buffer);
      return_val = MAX (return_val, temp);
    }

  return return_val;
}

void
gtk_scale_draw_value (GtkScale *scale)
{
  g_return_if_fail (scale != NULL);
  g_return_if_fail (GTK_IS_SCALE (scale));

  if (SCALE_CLASS (scale)->draw_value)
    (* SCALE_CLASS (scale)->draw_value) (scale);
}


static void
gtk_scale_draw_background (GtkRange *range)
{
  g_return_if_fail (range != NULL);
  g_return_if_fail (GTK_IS_SCALE (range));

  gtk_scale_draw_value (GTK_SCALE (range));
}
