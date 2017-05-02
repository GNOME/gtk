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

#include "config.h"

#include <math.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#undef GDK_DISABLE_DEPRECATED
#undef GTK_DISABLE_DEPRECATED

#include "gtkpreview.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"


#define PREVIEW_CLASS(w)      GTK_PREVIEW_CLASS (GTK_OBJECT (w)->klass)

enum {
  PROP_0,
  PROP_EXPAND
};


static void   gtk_preview_set_property  (GObject          *object,
					 guint             prop_id,
					 const GValue     *value,
					 GParamSpec       *pspec);
static void   gtk_preview_get_property  (GObject          *object,
					 guint             prop_id,
					 GValue           *value,
					 GParamSpec       *pspec);
static void   gtk_preview_finalize      (GObject          *object);
static void   gtk_preview_realize       (GtkWidget        *widget);
static void   gtk_preview_size_allocate (GtkWidget        *widget,
					 GtkAllocation    *allocation);
static gint   gtk_preview_expose        (GtkWidget        *widget,
				         GdkEventExpose   *event);
static void   gtk_preview_make_buffer   (GtkPreview       *preview);
static void   gtk_fill_lookup_array     (guchar           *array);

static GtkPreviewClass *preview_class = NULL;
static gint install_cmap = FALSE;


G_DEFINE_TYPE (GtkPreview, gtk_preview, GTK_TYPE_WIDGET)

static void
gtk_preview_class_init (GtkPreviewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class;

  widget_class = (GtkWidgetClass*) klass;

  preview_class = klass;

  gobject_class->finalize = gtk_preview_finalize;

  gobject_class->set_property = gtk_preview_set_property;
  gobject_class->get_property = gtk_preview_get_property;

  widget_class->realize = gtk_preview_realize;
  widget_class->size_allocate = gtk_preview_size_allocate;
  widget_class->expose_event = gtk_preview_expose;

  klass->info.lookup = NULL;

  klass->info.gamma = 1.0;

  g_object_class_install_property (gobject_class,
                                   PROP_EXPAND,
                                   g_param_spec_boolean ("expand",
							 P_("Expand"),
							 P_("Whether the preview widget should take up the entire space it is allocated"),
							 FALSE,
							 GTK_PARAM_READWRITE));
}

static void
gtk_preview_set_property (GObject      *object,
			  guint         prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
  GtkPreview *preview = GTK_PREVIEW (object);
  
  switch (prop_id)
    {
    case PROP_EXPAND:
      gtk_preview_set_expand (preview, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_preview_get_property (GObject      *object,
			  guint         prop_id,
			  GValue       *value,
			  GParamSpec   *pspec)
{
  GtkPreview *preview;
  
  preview = GTK_PREVIEW (object);
  
  switch (prop_id)
    {
    case PROP_EXPAND:
      g_value_set_boolean (value, preview->expand);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
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
  g_return_if_fail (GTK_IS_PREVIEW (preview));

  if ((width != GTK_WIDGET (preview)->requisition.width) ||
      (height != GTK_WIDGET (preview)->requisition.height))
    {
      GTK_WIDGET (preview)->requisition.width = width;
      GTK_WIDGET (preview)->requisition.height = height;

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
  GdkRectangle r1, r2, r3;
  guchar *src;
  guint bpp;
  guint rowstride;

  g_return_if_fail (GTK_IS_PREVIEW (preview));
  g_return_if_fail (window != NULL);

  if (!preview->buffer)
    return;

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

  g_return_if_fail (GTK_IS_PREVIEW (preview));
  g_return_if_fail (data != NULL);
  
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
  g_return_if_fail (GTK_IS_PREVIEW (preview));

  expand = expand != FALSE;

  if (preview->expand != expand)
    {
      preview->expand = expand;
      gtk_widget_queue_resize (GTK_WIDGET (preview));
 
      g_object_notify (G_OBJECT (preview), "expand"); 
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
  return gdk_screen_get_rgb_visual (gdk_screen_get_default ());
}

GdkColormap*
gtk_preview_get_cmap (void)
{
  return gdk_screen_get_rgb_colormap (gdk_screen_get_default ());
}

GtkPreviewInfo*
gtk_preview_get_info (void)
{
  if (!preview_class)
    preview_class = gtk_type_class (gtk_preview_get_type ());

  return &preview_class->info;
}


static void
gtk_preview_finalize (GObject *object)
{
  GtkPreview *preview = GTK_PREVIEW (object);

  g_free (preview->buffer);

  G_OBJECT_CLASS (gtk_preview_parent_class)->finalize (object);
}

static void
gtk_preview_realize (GtkWidget *widget)
{
  GtkPreview *preview = GTK_PREVIEW (widget);
  GdkWindowAttr attributes;
  gint attributes_mask;

  gtk_widget_set_realized (widget, TRUE);

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
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void   
gtk_preview_size_allocate (GtkWidget        *widget,
			   GtkAllocation    *allocation)
{
  GtkPreview *preview = GTK_PREVIEW (widget);
  gint width, height;

  widget->allocation = *allocation;

  if (gtk_widget_get_realized (widget))
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

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      preview = GTK_PREVIEW (widget);

      gdk_drawable_get_size (widget->window, &width, &height);

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

#define __GTK_PREVIEW_C__
#include "gtkaliasdef.c"
