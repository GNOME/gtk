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

#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include "gdk/gdkx.h"
#include "gtkpreview.h"
#include "gtksignal.h"


#define IMAGE_SIZE            256
#define PREVIEW_CLASS(w)      GTK_PREVIEW_CLASS (GTK_OBJECT (w)->klass)
#define COLOR_COMPOSE(r,g,b)  (lookup_red[r] | lookup_green[g] | lookup_blue[b])


typedef struct _GtkPreviewProp  GtkPreviewProp;
typedef void (*GtkTransferFunc) (guchar *dest, guchar *src, gint count);

struct _GtkPreviewProp
{
  guint16 ref_count;
  guint16 nred_shades;
  guint16 ngreen_shades;
  guint16 nblue_shades;
  guint16 ngray_shades;
};


static void   gtk_preview_class_init    (GtkPreviewClass  *klass);
static void   gtk_preview_init          (GtkPreview       *preview);
static void   gtk_preview_finalize      (GtkObject        *object);
static void   gtk_preview_realize       (GtkWidget        *widget);
static gint   gtk_preview_expose        (GtkWidget        *widget,
				         GdkEventExpose   *event);
static void   gtk_preview_make_buffer   (GtkPreview       *preview);
static void   gtk_preview_get_visuals   (GtkPreviewClass  *klass);
static void   gtk_preview_get_cmaps     (GtkPreviewClass  *klass);
static void   gtk_preview_dither_init   (GtkPreviewClass  *klass);
static void   gtk_fill_lookup_array     (gulong           *array,
					 int               depth,
					 int               shift,
					 int               prec);
static void   gtk_trim_cmap             (GtkPreviewClass  *klass);
static void   gtk_create_8_bit          (GtkPreviewClass  *klass);

static void   gtk_color_8               (guchar           *src,
				         guchar           *data,
				         gint              x,
				         gint              y,
				         gulong            width);
static void   gtk_color_16              (guchar           *src,
				         guchar           *data,
				         gulong            width);
static void   gtk_color_24              (guchar           *src,
				         guchar           *data,
				         gulong            width);
static void   gtk_grayscale_8           (guchar           *src,
				         guchar           *data,
				         gint              x,
				         gint              y,
				         gulong            width);
static void   gtk_grayscale_16          (guchar           *src,
				         guchar           *data,
				         gulong            width);
static void   gtk_grayscale_24          (guchar           *src,
				         guchar           *data,
				         gulong            width);

static gint   gtk_get_preview_prop      (guint            *nred,
					 guint            *nblue,
					 guint            *ngreen,
					 guint            *ngray);
static void   gtk_set_preview_prop      (guint             nred,
					 guint             ngreen,
					 guint             nblue,
					 guint             ngray);

/* transfer functions:
 *  destination byte order/source bpp/destination bpp
 */
static void   gtk_lsbmsb_1_1            (guchar           *dest,
					 guchar           *src,
					 gint              count);
static void   gtk_lsb_2_2               (guchar           *dest,
					 guchar           *src,
					 gint              count);
static void   gtk_msb_2_2               (guchar           *dest,
					 guchar           *src,
					 gint              count);
static void   gtk_lsb_3_3               (guchar           *dest,
					 guchar           *src,
					 gint              count);
static void   gtk_msb_3_3               (guchar           *dest,
					 guchar           *src,
					 gint              count);
static void   gtk_lsb_3_4               (guchar           *dest,
					 guchar           *src,
					 gint              count);
static void   gtk_msb_3_4               (guchar           *dest,
					 guchar           *src,
					 gint              count);


static GtkWidgetClass *parent_class = NULL;
static GtkPreviewClass *preview_class = NULL;
static GtkPreviewInfo *preview_info = NULL;
static gint install_cmap = FALSE;


guint
gtk_preview_get_type (void)
{
  static guint preview_type = 0;

  if (!preview_type)
    {
      GtkTypeInfo preview_info =
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

      preview_type = gtk_type_unique (gtk_widget_get_type (), &preview_info);
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

  parent_class = gtk_type_class (gtk_widget_get_type ());
  preview_class = klass;

  object_class->finalize = gtk_preview_finalize;

  widget_class->realize = gtk_preview_realize;
  widget_class->expose_event = gtk_preview_expose;

  if (preview_info)
    klass->info = *preview_info;
  else
    {
      klass->info.visual = NULL;
      klass->info.cmap = NULL;

      klass->info.color_pixels = NULL;
      klass->info.gray_pixels = NULL;
      klass->info.reserved_pixels = NULL;

      klass->info.lookup_red = NULL;
      klass->info.lookup_green = NULL;
      klass->info.lookup_blue = NULL;

      klass->info.dither_red = NULL;
      klass->info.dither_green = NULL;
      klass->info.dither_blue = NULL;
      klass->info.dither_gray = NULL;
      klass->info.dither_matrix = NULL;

      klass->info.nred_shades = 6;
      klass->info.ngreen_shades = 6;
      klass->info.nblue_shades = 4;
      klass->info.ngray_shades = 24;
      klass->info.nreserved = 0;

      klass->info.bpp = 0;
      klass->info.cmap_alloced = FALSE;
      klass->info.gamma = 1.0;
    }

  klass->image = NULL;

  gtk_preview_get_visuals (klass);
  gtk_preview_get_cmaps (klass);
  gtk_preview_dither_init (klass);
}

void
gtk_preview_reset (void)
{
  GtkPreviewInfo *info;

  if (!preview_class || !preview_info)
    return;
  
  info = &preview_class->info;

  gtk_preview_uninit();

  if (info->color_pixels)
    {
      gdk_colors_free (info->cmap,
		       info->color_pixels,
		       info->nred_shades *
		         info->ngreen_shades *
		         info->nblue_shades, 
		       0);

      gdk_colors_free (info->cmap,
		       info->gray_pixels,
		       info->ngray_shades, 0);
      
      g_free (info->color_pixels);
      g_free (info->gray_pixels);
    }

  if (info->reserved_pixels)
    {
      gdk_colors_free (info->cmap,
		       info->reserved_pixels,
		       info->nreserved, 0);
      g_free (info->reserved_pixels);
    }

  if (info->cmap && info->cmap_alloced)
    gdk_colormap_unref (info->cmap);

  if (info->lookup_red)
    {
      g_free (info->lookup_red);
      g_free (info->lookup_green);
      g_free (info->lookup_blue);
    }

  if (info->dither_matrix)
    {
      int i, j;

      for (i= 0 ; i < 8 ; i++)
	{
	  for (j = 0; j < 8 ; j++)
	    g_free (info->dither_matrix[i][j]);
	  g_free (info->dither_matrix[i]);
	}
      g_free (info->dither_matrix);
      g_free (info->dither_red);
      g_free (info->dither_green);
      g_free (info->dither_blue);
    }

  preview_class->info = *preview_info;

  gtk_preview_get_visuals (preview_class);
  gtk_preview_get_cmaps (preview_class);
  gtk_preview_dither_init (preview_class);
}

static void
gtk_preview_init (GtkPreview *preview)
{
  GTK_WIDGET_SET_FLAGS (preview, GTK_BASIC);

  preview->buffer = NULL;
  preview->buffer_width = 0;
  preview->buffer_height = 0;
  preview->expand = FALSE;
}

void
gtk_preview_uninit (void)
{
  GtkPreviewProp *prop;
  GdkAtom property;

  /* FIXME: need to grab the server here to prevent a race condition */

  if (preview_class && !install_cmap && preview_class->info.visual &&
      (preview_class->info.visual->type != GDK_VISUAL_TRUE_COLOR) &&
      (preview_class->info.visual->type != GDK_VISUAL_DIRECT_COLOR))
    {
      property = gdk_atom_intern ("GTK_PREVIEW_INFO", FALSE);

      if (gdk_property_get (NULL, property, property,
			    0, sizeof (GtkPreviewProp), FALSE,
			    NULL, NULL, NULL, (guchar**) &prop))
	{
	  prop->ref_count = prop->ref_count - 1;
	  if (prop->ref_count == 0)
	    {
	      gdk_property_delete (NULL, property);
	    }
	  else
	    {
	      gdk_property_change (NULL, property, property, 16,
				   GDK_PROP_MODE_REPLACE,
				   (guchar*) prop, 5);
	    }
	}
    }
}

GtkWidget*
gtk_preview_new (GtkPreviewType type)
{
  GtkPreview *preview;

  preview = gtk_type_new (gtk_preview_get_type ());
  preview->type = type;

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
  GdkImage *image;
  GdkRectangle r1, r2, r3;
  GtkTransferFunc transfer_func;
  guchar *image_mem;
  guchar *src, *dest;
  gint x, xe, x2;
  gint y, ye, y2;
  guint dest_rowstride;
  guint src_bpp;
  guint dest_bpp;
  gint i;

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

  x2 = r3.x + r3.width;
  y2 = r3.y + r3.height;

  if (!preview_class->image)
    preview_class->image = gdk_image_new (GDK_IMAGE_FASTEST,
					  preview_class->info.visual,
					  IMAGE_SIZE, IMAGE_SIZE);
  image = preview_class->image;
  src_bpp = preview_class->info.bpp;

  image_mem = image->mem;
  dest_bpp = image->bpp;
  dest_rowstride = image->bpl;

  transfer_func = NULL;

  switch (dest_bpp)
    {
    case 1:
      switch (src_bpp)
	{
	case 1:
	  transfer_func = gtk_lsbmsb_1_1;
	  break;
	}
      break;
    case 2:
      switch (src_bpp)
	{
	case 2:
	  if (image->byte_order == GDK_MSB_FIRST)
	    transfer_func = gtk_msb_2_2;
	  else
	    transfer_func = gtk_lsb_2_2;
	  break;
	case 3:
	  break;
	}
      break;
    case 3:
      switch (src_bpp)
	{
	case 3:
	  if (image->byte_order == GDK_MSB_FIRST)
	    transfer_func = gtk_msb_3_3;
	  else
	    transfer_func = gtk_lsb_3_3;
	  break;
	}
      break;
    case 4:
      switch (src_bpp)
	{
	case 3:
	  if (image->byte_order == GDK_MSB_FIRST)
	    transfer_func = gtk_msb_3_4;
	  else
	    transfer_func = gtk_lsb_3_4;
	  break;
	}
      break;
    }

  if (!transfer_func)
    {
      g_warning ("unsupported byte order/src bpp/dest bpp combination: %s:%d:%d",
		 (image->byte_order == GDK_MSB_FIRST) ? "msb" : "lsb", src_bpp, dest_bpp);
      return;
    }

  for (y = r3.y; y < y2; y += IMAGE_SIZE)
    {
      for (x = r3.x; x < x2; x += IMAGE_SIZE)
	{
	  xe = x + IMAGE_SIZE;
	  if (xe > x2)
	    xe = x2;

	  ye = y + IMAGE_SIZE;
	  if (ye > y2)
	    ye = y2;

	  for (i = y; i < ye; i++)
	    {
	      src = preview->buffer + (((gulong) (i - r1.y) * (gulong) preview->buffer_width) +
				       (x - r1.x)) * (gulong) src_bpp;
	      dest = image_mem + ((gulong) (i - y) * dest_rowstride);

	      if (xe > x)
		(* transfer_func) (dest, src, xe - x);
	    }

	  gdk_draw_image (window, gc,
			  image, 0, 0,
			  destx + (r3.x - srcx) + (x - r3.x),
			  desty + (r3.y - srcy) + (y - r3.y),
			  xe - x, ye - y);
	  gdk_flush ();
	}
    }
}

void
gtk_preview_put_row (GtkPreview *preview,
		     guchar     *src,
		     guchar     *dest,
		     gint        x,
		     gint        y,
		     gint        w)
{
  g_return_if_fail (preview != NULL);
  g_return_if_fail (GTK_IS_PREVIEW (preview));
  g_return_if_fail (src != NULL);
  g_return_if_fail (dest != NULL);

  switch (preview->type)
    {
    case GTK_PREVIEW_COLOR:
      switch (preview_class->info.visual->depth)
	{
	case 8:
	  gtk_color_8 (src, dest, x, y, w);
	  break;
	case 15:
	case 16:
	  gtk_color_16 (src, dest, w);
	  break;
	case 24:
	case 32:
	  gtk_color_24 (src, dest, w);
	  break;
	}
      break;
    case GTK_PREVIEW_GRAYSCALE:
      switch (preview_class->info.visual->depth)
	{
	case 8:
	  gtk_grayscale_8 (src, dest, x, y, w);
	  break;
	case 15:
	case 16:
	  gtk_grayscale_16 (src, dest, w);
	  break;
	case 24:
	case 32:
	  gtk_grayscale_24 (src, dest, w);
	  break;
	}
      break;
    }
}

void
gtk_preview_draw_row (GtkPreview *preview,
		      guchar     *data,
		      gint        x,
		      gint        y,
		      gint        w)
{
  guchar *dest;

  g_return_if_fail (preview != NULL);
  g_return_if_fail (GTK_IS_PREVIEW (preview));
  g_return_if_fail (data != NULL);
  g_return_if_fail (preview_class->info.visual != NULL);
  
  if ((w <= 0) || (y < 0))
    return;

  g_return_if_fail (data != NULL);

  gtk_preview_make_buffer (preview);

  if (y >= preview->buffer_height)
    return;

  switch (preview->type)
    {
    case GTK_PREVIEW_COLOR:
      switch (preview_class->info.visual->depth)
	{
	case 8:
	  dest = preview->buffer + ((gulong) y * (gulong) preview->buffer_width + (gulong) x);
	  gtk_color_8 (data, dest, x, y, w);
	  break;
	case 15:
	case 16:
	  dest = preview->buffer + ((gulong) y * (gulong) preview->buffer_width + (gulong) x) * 2;
	  gtk_color_16 (data, dest, w);
	  break;
	case 24:
	case 32:
	  dest = preview->buffer + ((gulong) y * (gulong) preview->buffer_width + (gulong) x) * 3;
	  gtk_color_24 (data, dest, w);
	  break;
	}
      break;
    case GTK_PREVIEW_GRAYSCALE:
      switch (preview_class->info.visual->depth)
	{
	case 8:
	  dest = preview->buffer + ((gulong) y * (gulong) preview->buffer_width + (gulong) x);
	  gtk_grayscale_8 (data, dest, x, y, w);
	  break;
	case 15:
	case 16:
	  dest = preview->buffer + ((gulong) y * (gulong) preview->buffer_width + (gulong) x) * 2;
	  gtk_grayscale_16 (data, dest, w);
	  break;
	case 24:
	case 32:
	  dest = preview->buffer + ((gulong) y * (gulong) preview->buffer_width + (gulong) x) * 3;
	  gtk_grayscale_24 (data, dest, w);
	  break;
	}
      break;
    }
}

void
gtk_preview_set_expand (GtkPreview *preview,
			gint        expand)
{
  g_return_if_fail (preview != NULL);
  g_return_if_fail (GTK_IS_PREVIEW (preview));

  preview->expand = (expand != FALSE);
}

void
gtk_preview_set_gamma (double _gamma)
{
  if (!preview_info)
    {
      preview_info = g_new0 (GtkPreviewInfo, 1);
      preview_info->nred_shades = 6;
      preview_info->ngreen_shades = 6;
      preview_info->nblue_shades = 4;
      preview_info->ngray_shades = 24;
    }

  preview_info->gamma = _gamma;
}

void
gtk_preview_set_color_cube (guint nred_shades,
			    guint ngreen_shades,
			    guint nblue_shades,
			    guint ngray_shades)
{
  if (!preview_info)
    {
      preview_info = g_new0 (GtkPreviewInfo, 1);
      preview_info->gamma = 1.0;
    }

  preview_info->nred_shades = nred_shades;
  preview_info->ngreen_shades = ngreen_shades;
  preview_info->nblue_shades = nblue_shades;
  preview_info->ngray_shades = ngray_shades;
}

void
gtk_preview_set_install_cmap (gint _install_cmap)
{
  install_cmap = _install_cmap;
}

void
gtk_preview_set_reserved (gint nreserved)
{
  if (!preview_info)
    preview_info = g_new0 (GtkPreviewInfo, 1);

  preview_info->nreserved = nreserved;
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
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
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

static gint
gtk_preview_expose (GtkWidget      *widget,
		    GdkEventExpose *event)
{
  GtkPreview *preview;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_PREVIEW (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      preview = GTK_PREVIEW (widget);
      
      gtk_preview_put (GTK_PREVIEW (widget),
		       widget->window, widget->style->black_gc,
		       event->area.x -
		       (widget->allocation.width - preview->buffer_width)/2,
		       event->area.y -
		       (widget->allocation.height - preview->buffer_height)/2,
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

      preview->buffer = g_new0 (guchar,
				preview->buffer_width *
				preview->buffer_height *
				preview_class->info.bpp);
    }
}

static void
gtk_preview_get_visuals (GtkPreviewClass *klass)
{
  static GdkVisualType types[] =
  {
    GDK_VISUAL_TRUE_COLOR,
    GDK_VISUAL_DIRECT_COLOR,
    GDK_VISUAL_TRUE_COLOR,
    GDK_VISUAL_DIRECT_COLOR,
    GDK_VISUAL_TRUE_COLOR,
    GDK_VISUAL_DIRECT_COLOR,
    GDK_VISUAL_TRUE_COLOR,
    GDK_VISUAL_DIRECT_COLOR,
    GDK_VISUAL_PSEUDO_COLOR,
    GDK_VISUAL_STATIC_COLOR,
    GDK_VISUAL_STATIC_GRAY
  };
  static gint depths[] = { 24, 24, 32, 32, 16, 16, 15, 15, 8, 4, 1 };
  static gint nvisual_types = sizeof (types) / sizeof (types[0]);

  int i;

  g_return_if_fail (klass != NULL);

  if (!klass->info.visual)
    for (i = 0; i < nvisual_types; i++)
      if ((klass->info.visual = gdk_visual_get_best_with_both (depths[i], types[i])))
	{
	  if ((klass->info.visual->type == GDK_VISUAL_TRUE_COLOR) ||
	      (klass->info.visual->type == GDK_VISUAL_DIRECT_COLOR))
	    {
	      klass->info.lookup_red = g_new (gulong, 256);
	      klass->info.lookup_green = g_new (gulong, 256);
	      klass->info.lookup_blue = g_new (gulong, 256);

	      gtk_fill_lookup_array (klass->info.lookup_red,
				     klass->info.visual->depth,
				     klass->info.visual->red_shift,
				     8 - klass->info.visual->red_prec);
	      gtk_fill_lookup_array (klass->info.lookup_green,
				     klass->info.visual->depth,
				     klass->info.visual->green_shift,
				     8 - klass->info.visual->green_prec);
	      gtk_fill_lookup_array (klass->info.lookup_blue,
				     klass->info.visual->depth,
				     klass->info.visual->blue_shift,
				     8 - klass->info.visual->blue_prec);
	    }
	  break;
	}

  if (!klass->info.visual)
    {
      g_warning ("unable to find a suitable visual for color image display.\n");
      return;
    }

  /* If we are _not_ running with an installed cmap, we must run
   * with the system visual. Otherwise, we let GDK pick the visual,
   * and it makes some effort to pick a non-default visual, which
   * will hopefully provide minimum color flashing.
   */
  if ((klass->info.visual->depth == gdk_visual_get_system()->depth) &&
      (klass->info.visual->type == gdk_visual_get_system()->type) &&
      !install_cmap)
    {
      klass->info.visual = gdk_visual_get_system();
    }
    
  switch (klass->info.visual->depth)
    {
    case 1:
    case 4:
    case 8:
      klass->info.bpp = 1;
      break;
    case 15:
    case 16:
      klass->info.bpp = 2;
      break;
    case 24:
    case 32:
      klass->info.bpp = 3;
      break;
    }
}

static void
gtk_preview_get_cmaps (GtkPreviewClass *klass)
{
  g_return_if_fail (klass != NULL);
  g_return_if_fail (klass->info.visual != NULL);

  if ((klass->info.visual->type != GDK_VISUAL_TRUE_COLOR) &&
      (klass->info.visual->type != GDK_VISUAL_DIRECT_COLOR))
    {
      if (install_cmap)
	{
	  klass->info.cmap = gdk_colormap_new (klass->info.visual, FALSE);
	  klass->info.cmap_alloced = install_cmap;

	  gtk_trim_cmap (klass);
	  gtk_create_8_bit (klass);
	}
      else
	{
	  guint nred;
	  guint ngreen;
	  guint nblue;
	  guint ngray;
	  gint set_prop;

	  klass->info.cmap = gdk_colormap_get_system ();

	  set_prop = TRUE;
	  if (gtk_get_preview_prop (&nred, &ngreen, &nblue, &ngray))
	    {
	      set_prop = FALSE;

	      klass->info.nred_shades = nred;
	      klass->info.ngreen_shades = ngreen;
	      klass->info.nblue_shades = nblue;
	      klass->info.ngray_shades = ngray;

	      if (klass->info.nreserved)
		{
		  klass->info.reserved_pixels = g_new (gulong, klass->info.nreserved);
		  if (!gdk_colors_alloc (klass->info.cmap, 0, NULL, 0,
					 klass->info.reserved_pixels,
					 klass->info.nreserved))
		    {
		      g_free (klass->info.reserved_pixels);
		      klass->info.reserved_pixels = NULL;
		    }
		}
	    }
	  else
	    {
	      gtk_trim_cmap (klass);
	    }

	  gtk_create_8_bit (klass);

	  if (set_prop)
	    gtk_set_preview_prop (klass->info.nred_shades,
				  klass->info.ngreen_shades,
				  klass->info.nblue_shades,
				  klass->info.ngray_shades);
	}
    }
  else
    {
      if (klass->info.visual == gdk_visual_get_system ())
	klass->info.cmap = gdk_colormap_get_system ();
      else
	{
	  klass->info.cmap = gdk_colormap_new (klass->info.visual, FALSE);
	  klass->info.cmap_alloced = TRUE;
	}

      klass->info.nred_shades = 0;
      klass->info.ngreen_shades = 0;
      klass->info.nblue_shades = 0;
      klass->info.ngray_shades = 0;
    }
}

static void
gtk_preview_dither_init (GtkPreviewClass *klass)
{
  int i, j, k;
  unsigned char low_shade, high_shade;
  unsigned short index;
  long red_mult, green_mult;
  double red_matrix_width;
  double green_matrix_width;
  double blue_matrix_width;
  double gray_matrix_width;
  double red_colors_per_shade;
  double green_colors_per_shade;
  double blue_colors_per_shade;
  double gray_colors_per_shade;
  gulong *gray_pixels;
  gint shades_r, shades_g, shades_b, shades_gray;
  GtkDitherInfo *red_ordered_dither;
  GtkDitherInfo *green_ordered_dither;
  GtkDitherInfo *blue_ordered_dither;
  GtkDitherInfo *gray_ordered_dither;
  guchar ***dither_matrix;
  guchar DM[8][8] =
  {
    { 0,  32, 8,  40, 2,  34, 10, 42 },
    { 48, 16, 56, 24, 50, 18, 58, 26 },
    { 12, 44, 4,  36, 14, 46, 6,  38 },
    { 60, 28, 52, 20, 62, 30, 54, 22 },
    { 3,  35, 11, 43, 1,  33, 9,  41 },
    { 51, 19, 59, 27, 49, 17, 57, 25 },
    { 15, 47, 7,  39, 13, 45, 5,  37 },
    { 63, 31, 55, 23, 61, 29, 53, 21 }
  };

  if (!klass->info.visual || klass->info.visual->type != GDK_VISUAL_PSEUDO_COLOR)
    return;

  shades_r = klass->info.nred_shades;
  shades_g = klass->info.ngreen_shades;
  shades_b = klass->info.nblue_shades;
  shades_gray = klass->info.ngray_shades;

  red_mult = shades_g * shades_b;
  green_mult = shades_b;

  red_colors_per_shade = 255.0 / (shades_r - 1);
  red_matrix_width = red_colors_per_shade / 64;

  green_colors_per_shade = 255.0 / (shades_g - 1);
  green_matrix_width = green_colors_per_shade / 64;

  blue_colors_per_shade = 255.0 / (shades_b - 1);
  blue_matrix_width = blue_colors_per_shade / 64;

  gray_colors_per_shade = 255.0 / (shades_gray - 1);
  gray_matrix_width = gray_colors_per_shade / 64;

  /*  alloc the ordered dither arrays for accelerated dithering  */

  klass->info.dither_red = g_new (GtkDitherInfo, 256);
  klass->info.dither_green = g_new (GtkDitherInfo, 256);
  klass->info.dither_blue = g_new (GtkDitherInfo, 256);
  klass->info.dither_gray = g_new (GtkDitherInfo, 256);

  red_ordered_dither = klass->info.dither_red;
  green_ordered_dither = klass->info.dither_green;
  blue_ordered_dither = klass->info.dither_blue;
  gray_ordered_dither = klass->info.dither_gray;

  dither_matrix = g_new (guchar**, 8);
  for (i = 0; i < 8; i++)
    {
      dither_matrix[i] = g_new (guchar*, 8);
      for (j = 0; j < 8; j++)
	dither_matrix[i][j] = g_new (guchar, 65);
    }

  klass->info.dither_matrix = dither_matrix;

  /*  setup the ordered_dither_matrices  */

  for (i = 0; i < 8; i++)
    for (j = 0; j < 8; j++)
      for (k = 0; k <= 64; k++)
	dither_matrix[i][j][k] = (DM[i][j] < k) ? 1 : 0;

  /*  setup arrays containing three bytes of information for red, green, & blue  */
  /*  the arrays contain :
   *    1st byte:    low end shade value
   *    2nd byte:    high end shade value
   *    3rd & 4th bytes:    ordered dither matrix index
   */

  gray_pixels = klass->info.gray_pixels;

  for (i = 0; i < 256; i++)
    {

      /*  setup the red information  */
      {
	low_shade = (unsigned char) (i / red_colors_per_shade);
	if (low_shade == (shades_r - 1))
	  low_shade--;
	high_shade = low_shade + 1;

	index = (unsigned short)
	  (((double) i - low_shade * red_colors_per_shade) /
	   red_matrix_width);

	low_shade *= red_mult;
	high_shade *= red_mult;

	red_ordered_dither[i].s[1] = index;
	red_ordered_dither[i].c[0] = low_shade;
	red_ordered_dither[i].c[1] = high_shade;
      }


      /*  setup the green information  */
      {
	low_shade = (unsigned char) (i / green_colors_per_shade);
	if (low_shade == (shades_g - 1))
	  low_shade--;
	high_shade = low_shade + 1;

	index = (unsigned short)
	  (((double) i - low_shade * green_colors_per_shade) /
	   green_matrix_width);

	low_shade *= green_mult;
	high_shade *= green_mult;

	green_ordered_dither[i].s[1] = index;
	green_ordered_dither[i].c[0] = low_shade;
	green_ordered_dither[i].c[1] = high_shade;
      }


      /*  setup the blue information  */
      {
	low_shade = (unsigned char) (i / blue_colors_per_shade);
	if (low_shade == (shades_b - 1))
	  low_shade--;
	high_shade = low_shade + 1;

	index = (unsigned short)
	  (((double) i - low_shade * blue_colors_per_shade) /
	   blue_matrix_width);

	blue_ordered_dither[i].s[1] = index;
	blue_ordered_dither[i].c[0] = low_shade;
	blue_ordered_dither[i].c[1] = high_shade;
      }


      /*  setup the gray information */
      {
	low_shade = (unsigned char) (i / gray_colors_per_shade);
	if (low_shade == (shades_gray - 1))
	  low_shade--;
	high_shade = low_shade + 1;

	index = (unsigned short)
	  (((double) i - low_shade * gray_colors_per_shade) /
	   gray_matrix_width);

	gray_ordered_dither[i].s[1] = index;
	gray_ordered_dither[i].c[0] = gray_pixels[low_shade];
	gray_ordered_dither[i].c[1] = gray_pixels[high_shade];
      }
    }
}

static void
gtk_fill_lookup_array (gulong *array,
		       int     depth,
		       int     shift,
		       int     prec)
{
  double one_over_gamma;
  double ind;
  int val;
  int i;

  if (preview_class->info.gamma != 0.0)
    one_over_gamma = 1.0 / preview_class->info.gamma;
  else
    one_over_gamma = 1.0;

  for (i = 0; i < 256; i++)
    {
      if (one_over_gamma == 1.0)
        array[i] = ((i >> prec) << shift);
      else
        {
          ind = (double) i / 255.0;
          val = (int) (255 * pow (ind, one_over_gamma));
          array[i] = ((val >> prec) << shift);
        }
    }
}

static void
gtk_trim_cmap (GtkPreviewClass *klass)
{
  gulong pixels[256];
  guint nred;
  guint ngreen;
  guint nblue;
  guint ngray;
  guint nreserved;
  guint total;
  guint tmp;
  gint success;

  nred = klass->info.nred_shades;
  ngreen = klass->info.ngreen_shades;
  nblue = klass->info.nblue_shades;
  ngray = klass->info.ngray_shades;
  nreserved = klass->info.nreserved;

  success = FALSE;
  while (!success)
    {
      total = nred * ngreen * nblue + ngray + nreserved;

      if (total <= 256)
	{
	  if ((nred < 2) || (ngreen < 2) || (nblue < 2) || (ngray < 2))
	    success = TRUE;
	  else
	    {
	      success = gdk_colors_alloc (klass->info.cmap, 0, NULL, 0, pixels, total);
	      if (success)
		{
		  if (nreserved > 0)
		    {
		      klass->info.reserved_pixels = g_new (gulong, nreserved);
		      memcpy (klass->info.reserved_pixels, pixels, sizeof (gulong) * nreserved);
		      gdk_colors_free (klass->info.cmap, &pixels[nreserved],
				       total - nreserved, 0);
		    }
		  else
		    {
		      gdk_colors_free (klass->info.cmap, pixels, total, 0);
		    }
		}
	    }
	}

      if (!success)
	{
	  if ((nblue >= nred) && (nblue >= ngreen))
	    nblue = nblue - 1;
	  else if ((nred >= ngreen) && (nred >= nblue))
	    nred = nred - 1;
	  else
	    {
	      tmp = log ((gdouble)ngray) / log (2.0);

	      if (ngreen >= tmp)
		ngreen = ngreen - 1;
	      else
		ngray -= 1;
	    }
	}
    }
  
  if ((nred < 2) || (ngreen < 2) || (nblue < 2) || (ngray < 2))
    {
      g_message ("Unable to allocate sufficient colormap entries.");
      g_message ("Try exiting other color intensive applications.");
      return;
    }
  
  /*  If any of the shade values has changed, issue a warning  */
  if ((nred != klass->info.nred_shades) ||
      (ngreen != klass->info.ngreen_shades) ||
      (nblue != klass->info.nblue_shades) ||
      (ngray != klass->info.ngray_shades))
    {
      g_message ("Not enough colors to satisfy requested color cube.");
      g_message ("Reduced color cube shades from");
      g_message ("[%d of Red, %d of Green, %d of Blue, %d of Gray] ==> [%d of Red, %d of Green, %d of Blue, %d of Gray]\n",
		 klass->info.nred_shades, klass->info.ngreen_shades,
		 klass->info.nblue_shades, klass->info.ngray_shades,
		 nred, ngreen, nblue, ngray);
    }
  
  klass->info.nred_shades = nred;
  klass->info.ngreen_shades = ngreen;
  klass->info.nblue_shades = nblue;
  klass->info.ngray_shades = ngray;
}

static void
gtk_create_8_bit (GtkPreviewClass *klass)
{
  unsigned int r, g, b;
  unsigned int rv, gv, bv;
  unsigned int dr, dg, db, dgray;
  GdkColor color;
  gulong *pixels;
  double one_over_gamma;
  int i;

  if (!klass->info.color_pixels)
    klass->info.color_pixels = g_new (gulong, 256);

  if (!klass->info.gray_pixels)
    klass->info.gray_pixels = g_new (gulong, 256);

  if (klass->info.gamma != 0.0)
    one_over_gamma = 1.0 / klass->info.gamma;
  else
    one_over_gamma = 1.0;

  dr = klass->info.nred_shades - 1;
  dg = klass->info.ngreen_shades - 1;
  db = klass->info.nblue_shades - 1;
  dgray = klass->info.ngray_shades - 1;

  pixels = klass->info.color_pixels;

  for (r = 0, i = 0; r <= dr; r++)
    for (g = 0; g <= dg; g++)
      for (b = 0; b <= db; b++, i++)
        {
          rv = (unsigned int) ((r * klass->info.visual->colormap_size) / dr);
          gv = (unsigned int) ((g * klass->info.visual->colormap_size) / dg);
          bv = (unsigned int) ((b * klass->info.visual->colormap_size) / db);
          color.red = ((int) (255 * pow ((double) rv / 256.0, one_over_gamma))) * 257;
          color.green = ((int) (255 * pow ((double) gv / 256.0, one_over_gamma))) * 257;
          color.blue = ((int) (255 * pow ((double) bv / 256.0, one_over_gamma))) * 257;

	  if (!gdk_color_alloc (klass->info.cmap, &color))
	    {
	      g_error ("could not initialize 8-bit combined colormap");
	      return;
	    }

	  pixels[i] = color.pixel;
        }


  pixels = klass->info.gray_pixels;

  for (i = 0; i < (int) klass->info.ngray_shades; i++)
    {
      color.red = (i * klass->info.visual->colormap_size) / dgray;
      color.red = ((int) (255 * pow ((double) color.red / 256.0, one_over_gamma))) * 257;
      color.green = color.red;
      color.blue = color.red;

      if (!gdk_color_alloc (klass->info.cmap, &color))
	{
	  g_error ("could not initialize 8-bit combined colormap");
	  return;
	}

      pixels[i] = color.pixel;
    }
}


static void
gtk_color_8 (guchar *src,
	     guchar *dest,
	     gint    x,
	     gint    y,
	     gulong  width)
{
  gulong *colors;
  GtkDitherInfo *dither_red;
  GtkDitherInfo *dither_green;
  GtkDitherInfo *dither_blue;
  GtkDitherInfo r, g, b;
  guchar **dither_matrix;
  guchar *matrix;

  colors = preview_class->info.color_pixels;
  dither_red = preview_class->info.dither_red;
  dither_green = preview_class->info.dither_green;
  dither_blue = preview_class->info.dither_blue;
  dither_matrix = preview_class->info.dither_matrix[y & 0x7];

  while (width--)
    {
      r = dither_red[src[0]];
      g = dither_green[src[1]];
      b = dither_blue[src[2]];
      src += 3;

      matrix = dither_matrix[x++ & 0x7];
      *dest++ = colors[(r.c[matrix[r.s[1]]] +
			g.c[matrix[g.s[1]]] +
			b.c[matrix[b.s[1]]])];
    }
}

static void
gtk_color_16 (guchar *src,
	      guchar *dest,
	      gulong  width)
{
  gulong *lookup_red;
  gulong *lookup_green;
  gulong *lookup_blue;
  gulong val;

  lookup_red = preview_class->info.lookup_red;
  lookup_green = preview_class->info.lookup_green;
  lookup_blue = preview_class->info.lookup_blue;

  while (width--)
    {
      val = COLOR_COMPOSE (src[0], src[1], src[2]);
      dest[0] = val;
      dest[1] = val >> 8;
      dest += 2;
      src += 3;
    }
}

static void
gtk_color_24 (guchar *src,
	      guchar *dest,
	      gulong  width)
{
  gulong *lookup_red;
  gulong *lookup_green;
  gulong *lookup_blue;
  gulong val;

  lookup_red = preview_class->info.lookup_red;
  lookup_green = preview_class->info.lookup_green;
  lookup_blue = preview_class->info.lookup_blue;

  while (width--)
    {
      val = COLOR_COMPOSE (src[0], src[1], src[2]);
      dest[0] = val;
      dest[1] = val >> 8;
      dest[2] = val >> 16;
      dest += 3;
      src += 3;
    }
}

static void
gtk_grayscale_8 (guchar *src,
		 guchar *dest,
		 gint    x,
		 gint    y,
		 gulong  width)
{
  GtkDitherInfo *dither_gray;
  GtkDitherInfo gray;
  guchar **dither_matrix;
  guchar *matrix;

  dither_gray = preview_class->info.dither_gray;
  dither_matrix = preview_class->info.dither_matrix[y & 0x7];

  while (width--)
    {
      gray = dither_gray[*src++];
      matrix = dither_matrix[x++ & 0x7];
      *dest++ = gray.c[matrix[gray.s[1]]];
    }
}

static void
gtk_grayscale_16 (guchar *src,
		  guchar *dest,
		  gulong  width)
{
  gulong *lookup_red;
  gulong *lookup_green;
  gulong *lookup_blue;
  gulong val;

  lookup_red = preview_class->info.lookup_red;
  lookup_green = preview_class->info.lookup_green;
  lookup_blue = preview_class->info.lookup_blue;

  while (width--)
    {
      val = COLOR_COMPOSE (*src, *src, *src);
      dest[0] = val;
      dest[1] = val >> 8;
      dest += 2;
      src += 1;
    }
}

static void
gtk_grayscale_24 (guchar  *src,
		  guchar  *dest,
		  gulong   width)
{
  gulong *lookup_red;
  gulong *lookup_green;
  gulong *lookup_blue;
  gulong val;

  lookup_red = preview_class->info.lookup_red;
  lookup_green = preview_class->info.lookup_green;
  lookup_blue = preview_class->info.lookup_blue;

  while (width--)
    {
      val = COLOR_COMPOSE (*src, *src, *src);
      dest[0] = val;
      dest[1] = val >> 8;
      dest[2] = val >> 16;
      dest += 3;
      src += 1;
    }
}


static gint
gtk_get_preview_prop (guint *nred,
		      guint *ngreen,
		      guint *nblue,
		      guint *ngray)
{
  GtkPreviewProp *prop;
  GdkAtom property;

  /* FIXME: need to grab the server here to prevent a race condition */

  property = gdk_atom_intern ("GTK_PREVIEW_INFO", FALSE);

  if (gdk_property_get (NULL, property, property,
			0, sizeof (GtkPreviewProp), FALSE,
			NULL, NULL, NULL, (guchar**) &prop))
    {
      *nred = prop->nred_shades;
      *ngreen = prop->ngreen_shades;
      *nblue = prop->nblue_shades;
      *ngray = prop->ngray_shades;

      prop->ref_count = prop->ref_count + 1;
      gdk_property_change (NULL, property, property, 16,
			   GDK_PROP_MODE_REPLACE,
			   (guchar*) prop, 5);

      return TRUE;
    }

  return FALSE;
}

static void
gtk_set_preview_prop (guint nred,
		      guint ngreen,
		      guint nblue,
		      guint ngray)
{
  GtkPreviewProp prop;
  GdkAtom property;

  property = gdk_atom_intern ("GTK_PREVIEW_INFO", FALSE);

  prop.ref_count = 1;
  prop.nred_shades = nred;
  prop.ngreen_shades = ngreen;
  prop.nblue_shades = nblue;
  prop.ngray_shades = ngray;

  gdk_property_change (NULL, property, property, 16,
		       GDK_PROP_MODE_REPLACE,
		       (guchar*) &prop, 5);
}


static void
gtk_lsbmsb_1_1 (guchar *dest,
		guchar *src,
		gint    count)
{
  memcpy (dest, src, count);
}

static void
gtk_lsb_2_2 (guchar *dest,
	     guchar *src,
	     gint    count)
{
  memcpy (dest, src, count * 2);
}

static void
gtk_msb_2_2 (guchar *dest,
	     guchar *src,
	     gint    count)
{
  while (count--)
    {
      dest[0] = src[1];
      dest[1] = src[0];
      dest += 2;
      src += 2;
    }
}

static void
gtk_lsb_3_3 (guchar *dest,
	     guchar *src,
	     gint    count)
{
  memcpy (dest, src, count * 3);
}

static void
gtk_msb_3_3 (guchar *dest,
	     guchar *src,
	     gint    count)
{
  while (count--)
    {
      dest[0] = src[2];
      dest[1] = src[1];
      dest[2] = src[0];
      dest += 3;
      src += 3;
    }
}

static void
gtk_lsb_3_4 (guchar *dest,
	     guchar *src,
	     gint    count)
{
  while (count--)
    {
      dest[0] = src[0];
      dest[1] = src[1];
      dest[2] = src[2];
      dest += 4;
      src += 3;
    }
}

static void
gtk_msb_3_4 (guchar *dest,
	     guchar *src,
	     gint    count)
{
  while (count--)
    {
      dest[1] = src[2];
      dest[2] = src[1];
      dest[3] = src[0];
      dest += 4;
      src += 3;
    }
}
