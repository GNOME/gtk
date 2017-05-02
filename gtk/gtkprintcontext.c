/* GTK - The GIMP Toolkit
 * gtkprintcontext.c: Print Context
 * Copyright (C) 2006, Red Hat, Inc.
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

#include "config.h"
#include "gtkprintoperation-private.h"
#include "gtkalias.h"

typedef struct _GtkPrintContextClass GtkPrintContextClass;

#define GTK_IS_PRINT_CONTEXT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PRINT_CONTEXT))
#define GTK_PRINT_CONTEXT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PRINT_CONTEXT, GtkPrintContextClass))
#define GTK_PRINT_CONTEXT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PRINT_CONTEXT, GtkPrintContextClass))

#define MM_PER_INCH 25.4
#define POINTS_PER_INCH 72

struct _GtkPrintContext
{
  GObject parent_instance;

  GtkPrintOperation *op;
  cairo_t *cr;
  GtkPageSetup *page_setup;

  gdouble surface_dpi_x;
  gdouble surface_dpi_y;
  
  gdouble pixels_per_unit_x;
  gdouble pixels_per_unit_y;

  gboolean has_hard_margins;
  gdouble hard_margin_top;
  gdouble hard_margin_bottom;
  gdouble hard_margin_left;
  gdouble hard_margin_right;

};

struct _GtkPrintContextClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (GtkPrintContext, gtk_print_context, G_TYPE_OBJECT)

static void
gtk_print_context_finalize (GObject *object)
{
  GtkPrintContext *context = GTK_PRINT_CONTEXT (object);

  if (context->page_setup)
    g_object_unref (context->page_setup);

  if (context->cr)
    cairo_destroy (context->cr);
  
  G_OBJECT_CLASS (gtk_print_context_parent_class)->finalize (object);
}

static void
gtk_print_context_init (GtkPrintContext *context)
{
}

static void
gtk_print_context_class_init (GtkPrintContextClass *class)
{
  GObjectClass *gobject_class = (GObjectClass *)class;

  gobject_class->finalize = gtk_print_context_finalize;
}


GtkPrintContext *
_gtk_print_context_new (GtkPrintOperation *op)
{
  GtkPrintContext *context;

  context = g_object_new (GTK_TYPE_PRINT_CONTEXT, NULL);

  context->op = op;
  context->cr = NULL;
  context->has_hard_margins = FALSE;
  
  return context;
}

static PangoFontMap *
_gtk_print_context_get_fontmap (GtkPrintContext *context)
{
  return pango_cairo_font_map_get_default ();
}

/**
 * gtk_print_context_set_cairo_context:
 * @context: a #GtkPrintContext
 * @cr: the cairo context
 * @dpi_x: the horizontal resolution to use with @cr
 * @dpi_y: the vertical resolution to use with @cr
 *
 * Sets a new cairo context on a print context. 
 * 
 * This function is intended to be used when implementing
 * an internal print preview, it is not needed for printing,
 * since GTK+ itself creates a suitable cairo context in that
 * case.
 *
 * Since: 2.10 
 */
void
gtk_print_context_set_cairo_context (GtkPrintContext *context,
				     cairo_t         *cr,
				     double           dpi_x,
				     double           dpi_y)
{
  if (context->cr)
    cairo_destroy (context->cr);

  context->cr = cairo_reference (cr);
  context->surface_dpi_x = dpi_x;
  context->surface_dpi_y = dpi_y;

  switch (context->op->priv->unit)
    {
    default:
    case GTK_UNIT_PIXEL:
      /* Do nothing, this is the cairo default unit */
      context->pixels_per_unit_x = 1.0;
      context->pixels_per_unit_y = 1.0;
      break;
    case GTK_UNIT_POINTS:
      context->pixels_per_unit_x = dpi_x / POINTS_PER_INCH;
      context->pixels_per_unit_y = dpi_y / POINTS_PER_INCH;
      break;
    case GTK_UNIT_INCH:
      context->pixels_per_unit_x = dpi_x;
      context->pixels_per_unit_y = dpi_y;
      break;
    case GTK_UNIT_MM:
      context->pixels_per_unit_x = dpi_x / MM_PER_INCH;
      context->pixels_per_unit_y = dpi_y / MM_PER_INCH;
      break;
    }
  cairo_scale (context->cr,
	       context->pixels_per_unit_x,
	       context->pixels_per_unit_y);
}


void
_gtk_print_context_rotate_according_to_orientation (GtkPrintContext *context)
{
  cairo_t *cr = context->cr;
  cairo_matrix_t matrix;
  GtkPaperSize *paper_size;
  gdouble width, height;

  paper_size = gtk_page_setup_get_paper_size (context->page_setup);

  width = gtk_paper_size_get_width (paper_size, GTK_UNIT_INCH);
  width = width * context->surface_dpi_x / context->pixels_per_unit_x;
  height = gtk_paper_size_get_height (paper_size, GTK_UNIT_INCH);
  height = height * context->surface_dpi_y / context->pixels_per_unit_y;
  
  switch (gtk_page_setup_get_orientation (context->page_setup))
    {
    default:
    case GTK_PAGE_ORIENTATION_PORTRAIT:
      break;
    case GTK_PAGE_ORIENTATION_LANDSCAPE:
      cairo_translate (cr, 0, height);
      cairo_matrix_init (&matrix,
			 0, -1,
			 1,  0,
			 0,  0);
      cairo_transform (cr, &matrix);
      break;
    case GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT:
      cairo_translate (cr, width, height);
      cairo_matrix_init (&matrix,
			 -1,  0,
			  0, -1,
			  0,  0);
      cairo_transform (cr, &matrix);
      break;
    case GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE:
      cairo_translate (cr, width, 0);
      cairo_matrix_init (&matrix,
			  0,  1,
			 -1,  0,
			  0,  0);
      cairo_transform (cr, &matrix);
      break;
    }
}

void
_gtk_print_context_translate_into_margin (GtkPrintContext *context)
{
  GtkPrintOperationPrivate *priv;
  gdouble left, top;

  g_return_if_fail (GTK_IS_PRINT_CONTEXT (context));

  priv = context->op->priv;

  /* We do it this way to also handle GTK_UNIT_PIXELS */
  
  left = gtk_page_setup_get_left_margin (context->page_setup, GTK_UNIT_INCH);
  top = gtk_page_setup_get_top_margin (context->page_setup, GTK_UNIT_INCH);

  cairo_translate (context->cr,
		   left * context->surface_dpi_x / context->pixels_per_unit_x,
		   top * context->surface_dpi_y / context->pixels_per_unit_y);
}

void
_gtk_print_context_set_page_setup (GtkPrintContext *context,
				   GtkPageSetup    *page_setup)
{
  g_return_if_fail (GTK_IS_PRINT_CONTEXT (context));
  g_return_if_fail (page_setup == NULL ||
		    GTK_IS_PAGE_SETUP (page_setup));
  
  g_object_ref (page_setup);

  if (context->page_setup != NULL)
    g_object_unref (context->page_setup);

  context->page_setup = page_setup;
}

/**
 * gtk_print_context_get_cairo_context:
 * @context: a #GtkPrintContext
 *
 * Obtains the cairo context that is associated with the
 * #GtkPrintContext.
 *
 * Return value: (transfer none): the cairo context of @context
 *
 * Since: 2.10
 */
cairo_t *
gtk_print_context_get_cairo_context (GtkPrintContext *context)
{
  g_return_val_if_fail (GTK_IS_PRINT_CONTEXT (context), NULL);

  return context->cr;
}

/**
 * gtk_print_context_get_page_setup:
 * @context: a #GtkPrintContext
 *
 * Obtains the #GtkPageSetup that determines the page
 * dimensions of the #GtkPrintContext.
 *
 * Return value: (transfer none): the page setup of @context
 *
 * Since: 2.10
 */
GtkPageSetup *
gtk_print_context_get_page_setup (GtkPrintContext *context)
{
  g_return_val_if_fail (GTK_IS_PRINT_CONTEXT (context), NULL);

  return context->page_setup;
}

/**
 * gtk_print_context_get_width:
 * @context: a #GtkPrintContext
 *
 * Obtains the width of the #GtkPrintContext, in pixels.
 *
 * Return value: the width of @context
 *
 * Since: 2.10 
 */
gdouble
gtk_print_context_get_width (GtkPrintContext *context)
{
  GtkPrintOperationPrivate *priv;
  gdouble width;

  g_return_val_if_fail (GTK_IS_PRINT_CONTEXT (context), 0);

  priv = context->op->priv;

  if (priv->use_full_page)
    width = gtk_page_setup_get_paper_width (context->page_setup, GTK_UNIT_INCH);
  else
    width = gtk_page_setup_get_page_width (context->page_setup, GTK_UNIT_INCH);

  /* Really dpi_x? What about landscape? what does dpi_x mean in that case? */
  return width * context->surface_dpi_x / context->pixels_per_unit_x;
}

/**
 * gtk_print_context_get_height:
 * @context: a #GtkPrintContext
 * 
 * Obtains the height of the #GtkPrintContext, in pixels.
 *
 * Return value: the height of @context
 *
 * Since: 2.10
 */
gdouble
gtk_print_context_get_height (GtkPrintContext *context)
{
  GtkPrintOperationPrivate *priv;
  gdouble height;

  g_return_val_if_fail (GTK_IS_PRINT_CONTEXT (context), 0);

  priv = context->op->priv;

  if (priv->use_full_page)
    height = gtk_page_setup_get_paper_height (context->page_setup, GTK_UNIT_INCH);
  else
    height = gtk_page_setup_get_page_height (context->page_setup, GTK_UNIT_INCH);

  /* Really dpi_y? What about landscape? what does dpi_y mean in that case? */
  return height * context->surface_dpi_y / context->pixels_per_unit_y;
}

/**
 * gtk_print_context_get_dpi_x:
 * @context: a #GtkPrintContext
 * 
 * Obtains the horizontal resolution of the #GtkPrintContext,
 * in dots per inch.
 *
 * Return value: the horizontal resolution of @context
 *
 * Since: 2.10
 */
gdouble
gtk_print_context_get_dpi_x (GtkPrintContext *context)
{
  g_return_val_if_fail (GTK_IS_PRINT_CONTEXT (context), 0);

  return context->surface_dpi_x;
}

/**
 * gtk_print_context_get_dpi_y:
 * @context: a #GtkPrintContext
 * 
 * Obtains the vertical resolution of the #GtkPrintContext,
 * in dots per inch.
 *
 * Return value: the vertical resolution of @context
 *
 * Since: 2.10
 */
gdouble
gtk_print_context_get_dpi_y (GtkPrintContext *context)
{
  g_return_val_if_fail (GTK_IS_PRINT_CONTEXT (context), 0);

  return context->surface_dpi_y;
}

/**
 * gtk_print_context_get_hard_margins:
 * @context: a #GtkPrintContext
 * @top: (out): top hardware printer margin
 * @bottom: (out): bottom hardware printer margin
 * @left: (out): left hardware printer margin
 * @right: (out): right hardware printer margin
 *
 * Obtains the hardware printer margins of the #GtkPrintContext, in units.
 *
 * Return value: %TRUE if the hard margins were retrieved
 *
 * Since: 2.20
 */
gboolean
gtk_print_context_get_hard_margins (GtkPrintContext *context,
				    gdouble         *top,
				    gdouble         *bottom,
				    gdouble         *left,
				    gdouble         *right)
{
  if (context->has_hard_margins)
    {
      *top    = context->hard_margin_top / context->pixels_per_unit_y;
      *bottom = context->hard_margin_bottom / context->pixels_per_unit_y;
      *left   = context->hard_margin_left / context->pixels_per_unit_x;
      *right  = context->hard_margin_right / context->pixels_per_unit_x;
    }

  return context->has_hard_margins;
}

/**
 * gtk_print_context_set_hard_margins:
 * @context: a #GtkPrintContext
 * @top: top hardware printer margin
 * @bottom: bottom hardware printer margin
 * @left: left hardware printer margin
 * @right: right hardware printer margin
 *
 * set the hard margins in pixel coordinates
 */
void
_gtk_print_context_set_hard_margins (GtkPrintContext *context,
				     gdouble          top,
				     gdouble          bottom,
				     gdouble          left,
				     gdouble          right)
{
  context->hard_margin_top    = top;
  context->hard_margin_bottom = bottom;
  context->hard_margin_left   = left;
  context->hard_margin_right  = right;
  context->has_hard_margins   = TRUE;
}

/**
 * gtk_print_context_get_pango_fontmap:
 * @context: a #GtkPrintContext
 *
 * Returns a #PangoFontMap that is suitable for use
 * with the #GtkPrintContext.
 *
 * Return value: (transfer none): the font map of @context
 *
 * Since: 2.10
 */
PangoFontMap *
gtk_print_context_get_pango_fontmap (GtkPrintContext *context)
{
  g_return_val_if_fail (GTK_IS_PRINT_CONTEXT (context), NULL);

  return _gtk_print_context_get_fontmap (context);
}

/**
 * gtk_print_context_create_pango_context:
 * @context: a #GtkPrintContext
 *
 * Creates a new #PangoContext that can be used with the
 * #GtkPrintContext.
 *
 * Return value: (transfer full): a new Pango context for @context
 * 
 * Since: 2.10
 */
PangoContext *
gtk_print_context_create_pango_context (GtkPrintContext *context)
{
  PangoContext *pango_context;
  cairo_font_options_t *options;

  g_return_val_if_fail (GTK_IS_PRINT_CONTEXT (context), NULL);
  
  pango_context = pango_font_map_create_context (_gtk_print_context_get_fontmap (context));

  options = cairo_font_options_create ();
  cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_OFF);
  pango_cairo_context_set_font_options (pango_context, options);
  cairo_font_options_destroy (options);
  
  /* We use the unit-scaled resolution, as we still want 
   * fonts given in points to work 
   */
  pango_cairo_context_set_resolution (pango_context,
				      context->surface_dpi_y / context->pixels_per_unit_y);
  return pango_context;
}

/**
 * gtk_print_context_create_pango_layout:
 * @context: a #GtkPrintContext
 *
 * Creates a new #PangoLayout that is suitable for use
 * with the #GtkPrintContext.
 * 
 * Return value: (transfer full): a new Pango layout for @context
 *
 * Since: 2.10
 */
PangoLayout *
gtk_print_context_create_pango_layout (GtkPrintContext *context)
{
  PangoContext *pango_context;
  PangoLayout *layout;

  g_return_val_if_fail (GTK_IS_PRINT_CONTEXT (context), NULL);

  pango_context = gtk_print_context_create_pango_context (context);
  layout = pango_layout_new (pango_context);

  pango_cairo_update_context (context->cr, pango_context);
  g_object_unref (pango_context);

  return layout;
}


#define __GTK_PRINT_CONTEXT_C__
#include "gtkaliasdef.c"
