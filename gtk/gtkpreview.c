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
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include "gdk/gdkx.h"
#include "gdk/gdkrgb.h"
#include "gtkpreview.h"
#include "gtksignal.h"


#define PREVIEW_CLASS(w)      GTK_PREVIEW_CLASS (GTK_OBJECT (w)->klass)

enum {
  ARG_0,
  ARG_EXPAND
};


static void   gtk_preview_class_init    (GtkPreviewClass  *klass);
static void   gtk_preview_init          (GtkPreview       *preview);
static void   gtk_preview_set_arg	(GtkObject        *object,
					 GtkArg           *arg,
					 guint             arg_id);
static void   gtk_preview_get_arg	(GtkObject        *object,
					 GtkArg           *arg,
					 guint             arg_id);
static void   gtk_preview_finalize      (GtkObject        *object);
static void   gtk_preview_realize       (GtkWidget        *widget);
static void   gtk_preview_size_allocate (GtkWidget        *widget,
					 GtkAllocation    *allocation);
static gint   gtk_preview_expose        (GtkWidget        *widget,
				         GdkEventExpose   *event);
static void   gtk_preview_make_buffer   (GtkPreview       *preview);
static void   gtk_fill_lookup_array     (guchar           *array);

static GtkWidgetClass *parent_class = NULL;
static GtkPreviewClass *preview_class = NULL;
static gint install_cmap = FALSE;


GtkType
gtk_preview_get_type (void)
{
  static GtkType preview_type = 0;

  if (!preview_type)
    {
      static const GtkTypeInfo preview_info =
      {
        "GtkPreview",
        sizeof (GtkPreview),
        sizeof (GtkPreviewClass),
        (GtkClassInitFunc) gtk_preview_class_init,
        (GtkObjectInitFunc) gtk_preview_init,
	/* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      preview_type = gtk_type_unique (GTK_TYPE_WIDGET, &preview_info);
    }

  return preview_type;
}

static void
gtk_preview_class_init (GtkPreviewClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) klass;
  widget_class = (GtkWidgetClass*) klass;

  parent_class = gtk_type_class (GTK_TYPE_WIDGET);
  preview_class = klass;

  object_class->set_arg = gtk_preview_set_arg;
  object_class->get_arg = gtk_preview_get_arg;
  object_class->finalize = gtk_preview_finalize;

  widget_class->realize = gtk_preview_realize;
  widget_class->size_allocate = gtk_preview_size_allocate;
  widget_class->expose_event = gtk_preview_expose;

  klass->info.visual = NULL;
  klass->info.cmap = NULL;

  klass->info.lookup = NULL;

  klass->info.gamma = 1.0;

  gdk_rgb_init ();
  klass->info.cmap = gdk_rgb_get_cmap ();
  klass->info.visual = gdk_rgb_get_visual ();

  gtk_object_add_arg_type ("GtkPreview::expand",
			   GTK_TYPE_BOOL,
			   GTK_ARG_READWRITE,
			   ARG_EXPAND);
}

static void
gtk_preview_set_arg (GtkObject *object,
		     GtkArg    *arg,
		     guint      arg_id)
{
  GtkPreview *preview = GTK_PREVIEW (object);
  
  switch (arg_id)
    {
    case ARG_EXPAND:
      gtk_preview_set_expand (preview, GTK_VALUE_BOOL (*arg));
      break;
    }
}

static void
gtk_preview_get_arg (GtkObject *object,
		     GtkArg    *arg,
		     guint      arg_id)
{
  GtkPreview *preview;
  
  preview = GTK_PREVIEW (object);
  
  switch (arg_id)
    {
    case ARG_EXPAND:
      GTK_VALUE_BOOL (*arg) = preview->expand;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

void
gtk_preview_reset (void)
{
  /* unimplemented */
}

static void
gtk_preview_init (GtkPreview *preview)
{
  preview->buffer = NULL;
  preview->buffer_width = 0;
  preview->buffer_height = 0;
  preview->expand = FALSE;
}

void
gtk_preview_uninit (void)
{

  /* unimplemented */
}

GtkWidget*
gtk_preview_new (GtkPreviewType type)
{
  GtkPreview *preview;

  preview = gtk_type_new (gtk_preview_get_type ());
  preview->type = type;

  if (type == GTK_PREVIEW_COLOR)
    preview->bpp = 3;
  else
    preview->bpp = 1;

  preview->dither = GDK_RGB_DITHER_NORMAL;

  return GTK_WIDGET (preview);
}

void
gtk_preview_size (GtkPreview *preview,
		  gint        width,
		  gint        height)
{
  g_return_if_fail (preview != NULL);
  g_return_if_fail (GTK_IS_PREVIEW (preview));

  if ((width != GTK_WIDGET (preview)->requisition.width) ||
      (height != GTK_WIDGET (preview)->requisition.height))
    {
      GTK_WIDGET (preview)->requisition.width = width;
      GTK_WIDGET (preview)->requisition.height = height;

      if (preview->buffer)
	g_free (preview->buffer);
      preview->buffer = NULL;
    }
}

void
gtk_preview_put (GtkPreview   *preview,
		 GdkWindow    *window,
		 GdkGC        *gc,
		 gint          srcx,
		 gint          srcy,
		 gint          destx,
		 gint          desty,
		 gint          width,
		 gint          height)
{
  GtkWidget *widget;
  GdkRectangle r1, r2, r3;
  guchar *src;
  guint bpp;
  guint rowstride;

  g_return_if_fail (preview != NULL);
  g_return_if_fail (GTK_IS_PREVIEW (preview));
  g_return_if_fail (window != NULL);

  if (!preview->buffer)
    return;

  widget = GTK_WIDGET (preview);

  r1.x = 0;
  r1.y = 0;
  r1.width = preview->buffer_width;
  r1.height = preview->buffer_height;

  r2.x = srcx;
  r2.y = srcy;
  r2.width = width;
  r2.height = height;

  if (!gdk_rectangle_intersect (&r1, &r2, &r3))
    return;

  bpp = preview->bpp;
  rowstride = preview->rowstride;

  src = preview->buffer + r3.y * rowstride + r3.x * bpp;

  if (preview->type == GTK_PREVIEW_COLOR)
    gdk_draw_rgb_image (window,
			gc,
			destx + (r3.x - srcx),
			desty + (r3.y - srcy),
			r3.width,
			r3.height,
			preview->dither,
			src,
			rowstride);
  else
    gdk_draw_gray_image (window,
			 gc,
			 destx + (r3.x - srcx),
			 desty + (r3.y - srcy),
			 r3.width,
			 r3.height,
			 preview->dither,
			 src,
			 rowstride);
			
}

void
gtk_preview_draw_row (GtkPreview *preview,
		      guchar     *data,
		      gint        x,
		      gint        y,
		      gint        w)
{
  guint bpp;
  guint rowstride;

  g_return_if_fail (preview != NULL);
  g_return_if_fail (GTK_IS_PREVIEW (preview));
  g_return_if_fail (data != NULL);
  g_return_if_fail (preview_class->info.visual != NULL);
  
  bpp = (preview->type == GTK_PREVIEW_COLOR ? 3 : 1);
  rowstride = (preview->buffer_width * bpp + 3) & -4;

  if ((w <= 0) || (y < 0))
    return;

  g_return_if_fail (data != NULL);

  gtk_preview_make_buffer (preview);

  if (x + w > preview->buffer_width)
    return;

  if (y + 1 > preview->buffer_height)
    return;

  if (preview_class->info.gamma == 1.0)
    memcpy (preview->buffer + y * rowstride + x * bpp, data, w * bpp);
  else
    {
      guint i, size;
      guchar *src, *dst;
      guchar *lookup;

      if (preview_class->info.lookup != NULL)
	lookup = preview_class->info.lookup;
      else
	{
	  preview_class->info.lookup = g_new (guchar, 256);
	  gtk_fill_lookup_array (preview_class->info.lookup);
	  lookup = preview_class->info.lookup;
	}
      size = w * bpp;
      src = data;
      dst = preview->buffer + y * rowstride + x * bpp;
      for (i = 0; i < size; i++)
	*dst++ = lookup[*src++];
    }
}

void
gtk_preview_set_expand (GtkPreview *preview,
			gboolean    expand)
{
  g_return_if_fail (preview != NULL);
  g_return_if_fail (GTK_IS_PREVIEW (preview));

  expand = expand != FALSE;

  if (preview->expand != expand)
    {
      preview->expand = expand;
      gtk_widget_queue_resize (GTK_WIDGET (preview));
    }
}

void
gtk_preview_set_gamma (double _gamma)
{
  if (!preview_class)
    preview_class = gtk_type_class (gtk_preview_get_type ());

  if (preview_class->info.gamma != _gamma)
    {
      preview_class->info.gamma = _gamma;
      if (preview_class->info.lookup != NULL)
	{
	  g_free (preview_class->info.lookup);
	  preview_class->info.lookup = NULL;
	}
    }
}

void
gtk_preview_set_color_cube (guint nred_shades,
			    guint ngreen_shades,
			    guint nblue_shades,
			    guint ngray_shades)
{
  /* unimplemented */
}

void
gtk_preview_set_install_cmap (gint _install_cmap)
{
  /* effectively unimplemented */
  install_cmap = _install_cmap;
}

void
gtk_preview_set_reserved (gint nreserved)
{

  /* unimplemented */
}

void
gtk_preview_set_dither (GtkPreview      *preview,
			GdkRgbDither     dither)
{
  preview->dither = dither;
}

GdkVisual*
gtk_preview_get_visual (void)
{
  if (!preview_class)
    preview_class = gtk_type_class (gtk_preview_get_type ());

  return preview_class->info.visual;
}

GdkColormap*
gtk_preview_get_cmap (void)
{
  if (!preview_class)
    preview_class = gtk_type_class (gtk_preview_get_type ());

  return preview_class->info.cmap;
}

GtkPreviewInfo*
gtk_preview_get_info (void)
{
  if (!preview_class)
    preview_class = gtk_type_class (gtk_preview_get_type ());

  return &preview_class->info;
}


static void
gtk_preview_finalize (GtkObject *object)
{
  GtkPreview *preview;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_PREVIEW (object));

  preview = GTK_PREVIEW (object);
  if (preview->buffer)
    g_free (preview->buffer);
  preview->type = (GtkPreviewType) -1;

  (* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
gtk_preview_realize (GtkWidget *widget)
{
  GtkPreview *preview;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PREVIEW (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  preview = GTK_PREVIEW (widget);

  attributes.window_type = GDK_WINDOW_CHILD;

  if (preview->expand)
    {
      attributes.width = widget->allocation.width;
      attributes.height = widget->allocation.height;
    }
  else
    {
      attributes.width = MIN (widget->requisition.width, widget->allocation.width);
      attributes.height = MIN (widget->requisition.height, widget->allocation.height);
    }

  attributes.x = widget->allocation.x + (widget->allocation.width - attributes.width) / 2;
  attributes.y = widget->allocation.y + (widget->allocation.height - attributes.height) / 2;;

  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = preview_class->info.visual;
  attributes.colormap = preview_class->info.cmap;
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void   
gtk_preview_size_allocate (GtkWidget        *widget,
			   GtkAllocation    *allocation)
{
  GtkPreview *preview;
  gint width, height;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_PREVIEW (widget));

  preview = GTK_PREVIEW (widget);
  widget->allocation = *allocation;

  if (GTK_WIDGET_REALIZED (widget))
    {
      if (preview->expand)
	{
	  width = widget->allocation.width;
	  height = widget->allocation.height;
	}
      else
	{
	  width = MIN (widget->allocation.width, widget->requisition.width);
	  height = MIN (widget->allocation.height, widget->requisition.height);
	}

      gdk_window_move_resize (widget->window,
			      widget->allocation.x + (widget->allocation.width - width) / 2,
			      widget->allocation.y + (widget->allocation.height - height) / 2,
			      width, height);
    }
}

static gint
gtk_preview_expose (GtkWidget      *widget,
		    GdkEventExpose *event)
{
  GtkPreview *preview;
  gint width, height;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_PREVIEW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      preview = GTK_PREVIEW (widget);
      
      gdk_window_get_size (widget->window, &width, &height);

      gtk_preview_put (GTK_PREVIEW (widget),
		       widget->window, widget->style->black_gc,
		       event->area.x - (width - preview->buffer_width)/2,
		       event->area.y - (height - preview->buffer_height)/2,
		       event->area.x, event->area.y,
		       event->area.width, event->area.height);
    }
  
  return FALSE;
}

static void
gtk_preview_make_buffer (GtkPreview *preview)
{
  GtkWidget *widget;
  gint width;
  gint height;

  g_return_if_fail (preview != NULL);
  g_return_if_fail (GTK_IS_PREVIEW (preview));

  widget = GTK_WIDGET (preview);

  if (preview->expand &&
      (widget->allocation.width != 0) &&
      (widget->allocation.height != 0))
    {
      width = widget->allocation.width;
      height = widget->allocation.height;
    }
  else
    {
      width = widget->requisition.width;
      height = widget->requisition.height;
    }

  if (!preview->buffer ||
      (preview->buffer_width != width) ||
      (preview->buffer_height != height))
    {
      if (preview->buffer)
	g_free (preview->buffer);

      preview->buffer_width = width;
      preview->buffer_height = height;

      preview->rowstride = (preview->buffer_width * preview->bpp + 3) & -4;
      preview->buffer = g_new0 (guchar,
				preview->buffer_height *
				preview->rowstride);
    }
}

/* This is used for implementing gamma. */
static void
gtk_fill_lookup_array (guchar *array)
{
  double one_over_gamma;
  double ind;
  int val;
  int i;

  one_over_gamma = 1.0 / preview_class->info.gamma;

  for (i = 0; i < 256; i++)
    {
      ind = (double) i / 255.0;
      val = (int) (255 * pow (ind, one_over_gamma));
      array[i] = val;
    }
}
