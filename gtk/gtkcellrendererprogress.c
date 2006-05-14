/* gtkcellrendererprogress.c
 * Copyright (C) 2002 Naba Kumar <kh_naba@users.sourceforge.net>
 * heavily modified by Jörgen Scheibengruber <mfcn@gmx.de>
 * heavily modified by Marco Pesenti Gritti <marco@gnome.org>
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
 * Modified by the GTK+ Team and others 1997-2004.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include <stdlib.h>

#include "gtkcellrendererprogress.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"

#define GTK_CELL_RENDERER_PROGRESS_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object),                        \
                                                                                     GTK_TYPE_CELL_RENDERER_PROGRESS, \
                                                                                     GtkCellRendererProgressPrivate))

enum
{
	PROP_0,
	PROP_VALUE,
	PROP_TEXT
}; 

struct _GtkCellRendererProgressPrivate
{
  gint value;
  gchar *text;
  gchar *label;
  gint min_h;
  gint min_w;
};

static void gtk_cell_renderer_progress_finalize     (GObject                 *object);
static void gtk_cell_renderer_progress_get_property (GObject                 *object,
						     guint                    param_id,
						     GValue                  *value,
						     GParamSpec              *pspec);
static void gtk_cell_renderer_progress_set_property (GObject                 *object,
						     guint                    param_id,
						     const GValue            *value,
						     GParamSpec              *pspec);
static void gtk_cell_renderer_progress_set_value    (GtkCellRendererProgress *cellprogress,
						     gint                     value);
static void gtk_cell_renderer_progress_set_text     (GtkCellRendererProgress *cellprogress,
						     const gchar             *text);
static void compute_dimensions                      (GtkCellRenderer         *cell,
						     GtkWidget               *widget,
						     const gchar             *text,
						     gint                    *width,
						     gint                    *height);
static void gtk_cell_renderer_progress_get_size     (GtkCellRenderer         *cell,
						     GtkWidget               *widget,
						     GdkRectangle            *cell_area,
						     gint                    *x_offset,
						     gint                    *y_offset,
						     gint                    *width,
						     gint                    *height);
static void gtk_cell_renderer_progress_render       (GtkCellRenderer         *cell,
						     GdkWindow               *window,
						     GtkWidget               *widget,
						     GdkRectangle            *background_area,
						     GdkRectangle            *cell_area,
						     GdkRectangle            *expose_area,
						     guint                    flags);

     
G_DEFINE_TYPE (GtkCellRendererProgress, gtk_cell_renderer_progress, GTK_TYPE_CELL_RENDERER)

static void
gtk_cell_renderer_progress_class_init (GtkCellRendererProgressClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);
  
  object_class->finalize = gtk_cell_renderer_progress_finalize;
  object_class->get_property = gtk_cell_renderer_progress_get_property;
  object_class->set_property = gtk_cell_renderer_progress_set_property;
  
  cell_class->get_size = gtk_cell_renderer_progress_get_size;
  cell_class->render = gtk_cell_renderer_progress_render;
  
  /**
   * GtkCellRendererProgress:value:
   * 
   * The "value" property determines the percentage to which the
   * progress bar will be "filled in". 
   *
   * Since: 2.6
   **/
  g_object_class_install_property (object_class,
				   PROP_VALUE,
				   g_param_spec_int ("value",
						     P_("Value"),
						     P_("Value of the progress bar"),
						     0, 100, 0,
						     GTK_PARAM_READWRITE));

  /**
   * GtkCellRendererProgress:text:
   * 
   * The "text" property determines the label which will be drawn
   * over the progress bar. Setting this property to %NULL causes the default 
   * label to be displayed. Setting this property to an empty string causes 
   * no label to be displayed.
   *
   * Since: 2.6
   **/
  g_object_class_install_property (object_class,
				   PROP_TEXT,
				   g_param_spec_string ("text",
							P_("Text"),
							P_("Text on the progress bar"),
							NULL,
							GTK_PARAM_READWRITE));

  g_type_class_add_private (object_class, 
			    sizeof (GtkCellRendererProgressPrivate));
}

static void
gtk_cell_renderer_progress_init (GtkCellRendererProgress *cellprogress)
{
  cellprogress->priv = GTK_CELL_RENDERER_PROGRESS_GET_PRIVATE (cellprogress);
  cellprogress->priv->value = 0;
  cellprogress->priv->text = NULL;
  cellprogress->priv->label = NULL;
  cellprogress->priv->min_w = -1;
  cellprogress->priv->min_h = -1;
}


/**
 * gtk_cell_renderer_progress_new:
 * 
 * Creates a new #GtkCellRendererProgress. 
 *
 * Return value: the new cell renderer
 *
 * Since: 2.6
 **/
GtkCellRenderer*
gtk_cell_renderer_progress_new (void)
{
  return g_object_new (GTK_TYPE_CELL_RENDERER_PROGRESS, NULL);
}

static void
gtk_cell_renderer_progress_finalize (GObject *object)
{
  GtkCellRendererProgress *cellprogress = GTK_CELL_RENDERER_PROGRESS (object);
  
  g_free (cellprogress->priv->text);
  g_free (cellprogress->priv->label);
  
  G_OBJECT_CLASS (gtk_cell_renderer_progress_parent_class)->finalize (object);
}

static void
gtk_cell_renderer_progress_get_property (GObject *object,
					 guint param_id,
					 GValue *value,
					 GParamSpec *pspec)
{
  GtkCellRendererProgress *cellprogress = GTK_CELL_RENDERER_PROGRESS (object);
  
  switch (param_id)
    {
    case PROP_VALUE:
      g_value_set_int (value, cellprogress->priv->value);
      break;
    case PROP_TEXT:
      g_value_set_string (value, cellprogress->priv->text);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_cell_renderer_progress_set_property (GObject *object,
					 guint param_id,
					 const GValue *value,
					 GParamSpec   *pspec)
{
  GtkCellRendererProgress *cellprogress = GTK_CELL_RENDERER_PROGRESS (object);
  
  switch (param_id)
    {
    case PROP_VALUE:
      gtk_cell_renderer_progress_set_value (cellprogress, 
					    g_value_get_int (value));
      break;
    case PROP_TEXT:
      gtk_cell_renderer_progress_set_text (cellprogress,
					   g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_cell_renderer_progress_set_value (GtkCellRendererProgress *cellprogress, 
				      gint                     value)
{
  gchar *text;
  
  cellprogress->priv->value = value;

  if (cellprogress->priv->text)
    text = g_strdup (cellprogress->priv->text);
  else
    /* do not translate the part before the | */
    text = g_strdup_printf (Q_("progress bar label|%d %%"), 
			    cellprogress->priv->value);
  
  g_free (cellprogress->priv->label);
  cellprogress->priv->label = text;
}

static void
gtk_cell_renderer_progress_set_text (GtkCellRendererProgress *cellprogress, 
				     const gchar             *text)
{
  gchar *new_text;

  new_text = g_strdup (text);
  g_free (cellprogress->priv->text);
  cellprogress->priv->text = new_text;

  /* Update the label */
  gtk_cell_renderer_progress_set_value (cellprogress, cellprogress->priv->value);
}

static void
compute_dimensions (GtkCellRenderer *cell,
		    GtkWidget       *widget, 
		    const gchar     *text, 
		    gint            *width, 
		    gint            *height)
{
  PangoRectangle logical_rect;
  PangoLayout *layout;
  
  layout = gtk_widget_create_pango_layout (widget, text);
  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
  
  if (width)
    *width = logical_rect.width + cell->xpad * 2 + widget->style->xthickness * 2;
  
  if (height)
    *height = logical_rect.height + cell->ypad * 2 + widget->style->ythickness * 2;
  
  g_object_unref (layout);
}

static void
gtk_cell_renderer_progress_get_size (GtkCellRenderer *cell,
				     GtkWidget       *widget,
				     GdkRectangle    *cell_area,
				     gint            *x_offset,
				     gint            *y_offset,
				     gint            *width,
				     gint            *height)
{
  GtkCellRendererProgress *cellprogress = GTK_CELL_RENDERER_PROGRESS (cell);
  gint w, h;
  gchar *text;

  if (cellprogress->priv->min_w < 0)
    {
      text = g_strdup_printf (Q_("progress bar label|%d %%"), 100);
      compute_dimensions (cell, widget, text,
			  &cellprogress->priv->min_w,
			  &cellprogress->priv->min_h);
      g_free (text);
    }
  
  compute_dimensions (cell, widget, cellprogress->priv->label, &w, &h);
  
  if (width)
    *width = MAX (cellprogress->priv->min_w, w);
  
  if (height)
    *height = MIN (cellprogress->priv->min_h, h);

  /* FIXME: at the moment cell_area is only set when we are requesting
   * the size for drawing the focus rectangle. We now just return
   * the last size we used for drawing the progress bar, which will
   * work for now. Not a really nice solution though.
   */
  if (cell_area)
    {
      if (width)
        *width = cell_area->width;
      if (height)
        *height = cell_area->height;
    }
}

static void
gtk_cell_renderer_progress_render (GtkCellRenderer *cell,
				   GdkWindow       *window,
				   GtkWidget       *widget,
				   GdkRectangle    *background_area,
				   GdkRectangle    *cell_area,
				   GdkRectangle    *expose_area,
				   guint            flags)
{
  GtkCellRendererProgress *cellprogress = GTK_CELL_RENDERER_PROGRESS (cell);
  PangoLayout *layout;
  PangoRectangle logical_rect;
  gint x, y, w, h, perc_w, pos;
  GdkRectangle clip;
  gboolean is_rtl;
  cairo_t *cr;

  is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  
  cr = gdk_cairo_create (window);
  
  x = cell_area->x + cell->xpad;
  y = cell_area->y + cell->ypad;
  
  w = cell_area->width - cell->xpad * 2;
  h = cell_area->height - cell->ypad * 2;

  cairo_rectangle (cr, x, y, w, h);
  gdk_cairo_set_source_color (cr, &widget->style->fg[GTK_STATE_NORMAL]);
  cairo_fill (cr);
  
  x += widget->style->xthickness;
  y += widget->style->ythickness;
  w -= widget->style->xthickness * 2;
  h -= widget->style->ythickness * 2;
  
  cairo_rectangle (cr, x, y, w, h);
  gdk_cairo_set_source_color (cr, &widget->style->bg[GTK_STATE_NORMAL]);
  cairo_fill (cr);
  
  perc_w = w * MAX (0, cellprogress->priv->value) / 100;
  
  cairo_rectangle (cr, is_rtl ? (x + w - perc_w) : x, y, perc_w, h);
  gdk_cairo_set_source_color (cr, &widget->style->bg[GTK_STATE_SELECTED]);
  cairo_fill (cr);
  
  layout = gtk_widget_create_pango_layout (widget, cellprogress->priv->label);
  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);
  
  pos = (w - logical_rect.width)/2;
  
  clip.x = x;
  clip.y = y;
  clip.width = is_rtl ? w - perc_w : perc_w;
  clip.height = h; 

  gtk_paint_layout (widget->style, window, 
		    is_rtl ? GTK_STATE_NORMAL : GTK_STATE_SELECTED,
		    FALSE, &clip, widget, "progressbar",
		    x + pos, y + (h - logical_rect.height)/2,
		    layout);

  clip.x = clip.x + clip.width;
  clip.width = w - clip.width;

  gtk_paint_layout (widget->style, window, 
		    is_rtl ?  GTK_STATE_SELECTED : GTK_STATE_NORMAL,
		    FALSE, &clip, widget, "progressbar",
		    x + pos, y + (h - logical_rect.height)/2,
		    layout);
  
  g_object_unref (layout);
  cairo_destroy (cr);
}

#define __GTK_CELL_RENDERER_PROGRESS_C__
#include "gtkaliasdef.c"
