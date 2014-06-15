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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * Modified by the GTK+ Team and others 1997-2007.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"
#include <stdlib.h>

#include "gtkcellrendererprogress.h"
#include "gtkintl.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtkrender.h"


/**
 * SECTION:gtkcellrendererprogress
 * @Short_description: Renders numbers as progress bars
 * @Title: GtkCellRendererProgress
 *
 * #GtkCellRendererProgress renders a numeric value as a progress par in a cell.
 * Additionally, it can display a text on top of the progress bar.
 *
 * The #GtkCellRendererProgress cell renderer was added in GTK+ 2.6.
 */


enum
{
  PROP_0,
  PROP_VALUE,
  PROP_TEXT,
  PROP_PULSE,
  PROP_TEXT_XALIGN,
  PROP_TEXT_YALIGN,
  PROP_ORIENTATION,
  PROP_INVERTED
};

struct _GtkCellRendererProgressPrivate
{
  gint value;
  gchar *text;
  gchar *label;
  gint min_h;
  gint min_w;
  gint pulse;
  gint offset;
  gfloat text_xalign;
  gfloat text_yalign;
  GtkOrientation orientation;
  gboolean inverted;
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
static void gtk_cell_renderer_progress_set_pulse    (GtkCellRendererProgress *cellprogress,
						     gint                     pulse);
static void compute_dimensions                      (GtkCellRenderer         *cell,
						     GtkWidget               *widget,
						     const gchar             *text,
						     gint                    *width,
						     gint                    *height);
static void gtk_cell_renderer_progress_get_size     (GtkCellRenderer         *cell,
						     GtkWidget               *widget,
						     const GdkRectangle      *cell_area,
						     gint                    *x_offset,
						     gint                    *y_offset,
						     gint                    *width,
						     gint                    *height);
static void gtk_cell_renderer_progress_render       (GtkCellRenderer         *cell,
						     cairo_t                 *cr,
						     GtkWidget               *widget,
						     const GdkRectangle      *background_area,
						     const GdkRectangle      *cell_area,
				                     GtkCellRendererState    flags);

     
G_DEFINE_TYPE_WITH_CODE (GtkCellRendererProgress, gtk_cell_renderer_progress, GTK_TYPE_CELL_RENDERER,
                         G_ADD_PRIVATE (GtkCellRendererProgress)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

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
						     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

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

  /**
   * GtkCellRendererProgress:pulse:
   * 
   * Setting this to a non-negative value causes the cell renderer to
   * enter "activity mode", where a block bounces back and forth to 
   * indicate that some progress is made, without specifying exactly how
   * much.
   *
   * Each increment of the property causes the block to move by a little 
   * bit.
   *
   * To indicate that the activity has not started yet, set the property
   * to zero. To indicate completion, set the property to %G_MAXINT.
   *
   * Since: 2.12
   */
  g_object_class_install_property (object_class,
                                   PROP_PULSE,
                                   g_param_spec_int ("pulse",
                                                     P_("Pulse"),
                                                     P_("Set this to positive values to indicate that some progress is made, but you don't know how much."),
                                                     -1, G_MAXINT, -1,
                                                     GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCellRendererProgress:text-xalign:
   *
   * The "text-xalign" property controls the horizontal alignment of the
   * text in the progress bar.  Valid values range from 0 (left) to 1
   * (right).  Reserved for RTL layouts.
   *
   * Since: 2.12
   */
  g_object_class_install_property (object_class,
                                   PROP_TEXT_XALIGN,
                                   g_param_spec_float ("text-xalign",
                                                       P_("Text x alignment"),
                                                       P_("The horizontal text alignment, from 0 (left) to 1 (right). Reversed for RTL layouts."),
                                                       0.0, 1.0, 0.5,
                                                       GTK_PARAM_READWRITE));

  /**
   * GtkCellRendererProgress:text-yalign:
   *
   * The "text-yalign" property controls the vertical alignment of the
   * text in the progress bar.  Valid values range from 0 (top) to 1
   * (bottom).
   *
   * Since: 2.12
   */
  g_object_class_install_property (object_class,
                                   PROP_TEXT_YALIGN,
                                   g_param_spec_float ("text-yalign",
                                                       P_("Text y alignment"),
                                                       P_("The vertical text alignment, from 0 (top) to 1 (bottom)."),
                                                       0.0, 1.0, 0.5,
                                                       GTK_PARAM_READWRITE));

  g_object_class_override_property (object_class,
                                    PROP_ORIENTATION,
                                    "orientation");

  g_object_class_install_property (object_class,
                                   PROP_INVERTED,
                                   g_param_spec_boolean ("inverted",
                                                         P_("Inverted"),
                                                         P_("Invert the direction in which the progress bar grows"),
                                                         FALSE,
                                                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
}

static void
gtk_cell_renderer_progress_init (GtkCellRendererProgress *cellprogress)
{
  GtkCellRendererProgressPrivate *priv;

  cellprogress->priv = gtk_cell_renderer_progress_get_instance_private (cellprogress);
  priv = cellprogress->priv;

  priv->value = 0;
  priv->text = NULL;
  priv->label = NULL;
  priv->min_w = -1;
  priv->min_h = -1;
  priv->pulse = -1;
  priv->offset = 0;

  priv->text_xalign = 0.5;
  priv->text_yalign = 0.5;

  priv->orientation = GTK_ORIENTATION_HORIZONTAL,
  priv->inverted = FALSE;
}


/**
 * gtk_cell_renderer_progress_new:
 * 
 * Creates a new #GtkCellRendererProgress. 
 *
 * Returns: the new cell renderer
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
  GtkCellRendererProgressPrivate *priv = cellprogress->priv;
  
  g_free (priv->text);
  g_free (priv->label);
  
  G_OBJECT_CLASS (gtk_cell_renderer_progress_parent_class)->finalize (object);
}

static void
gtk_cell_renderer_progress_get_property (GObject    *object,
					 guint       param_id,
					 GValue     *value,
					 GParamSpec *pspec)
{
  GtkCellRendererProgress *cellprogress = GTK_CELL_RENDERER_PROGRESS (object);
  GtkCellRendererProgressPrivate *priv = cellprogress->priv;
  
  switch (param_id)
    {
    case PROP_VALUE:
      g_value_set_int (value, priv->value);
      break;
    case PROP_TEXT:
      g_value_set_string (value, priv->text);
      break;
    case PROP_PULSE:
      g_value_set_int (value, priv->pulse);
      break;
    case PROP_TEXT_XALIGN:
      g_value_set_float (value, priv->text_xalign);
      break;
    case PROP_TEXT_YALIGN:
      g_value_set_float (value, priv->text_yalign);
      break;
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;
    case PROP_INVERTED:
      g_value_set_boolean (value, priv->inverted);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_cell_renderer_progress_set_property (GObject      *object,
					 guint         param_id,
					 const GValue *value,
					 GParamSpec   *pspec)
{
  GtkCellRendererProgress *cellprogress = GTK_CELL_RENDERER_PROGRESS (object);
  GtkCellRendererProgressPrivate *priv = cellprogress->priv;
  
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
    case PROP_PULSE:
      gtk_cell_renderer_progress_set_pulse (cellprogress, 
					    g_value_get_int (value));
      break;
    case PROP_TEXT_XALIGN:
      priv->text_xalign = g_value_get_float (value);
      break;
    case PROP_TEXT_YALIGN:
      priv->text_yalign = g_value_get_float (value);
      break;
    case PROP_ORIENTATION:
      if (priv->orientation != g_value_get_enum (value))
        {
          priv->orientation = g_value_get_enum (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    case PROP_INVERTED:
      if (priv->inverted != g_value_get_boolean (value))
        {
          priv->inverted = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
recompute_label (GtkCellRendererProgress *cellprogress)
{
  GtkCellRendererProgressPrivate *priv = cellprogress->priv;
  gchar *label;

  if (priv->text)
    label = g_strdup (priv->text);
  else if (priv->pulse < 0)
    label = g_strdup_printf (C_("progress bar label", "%d %%"), priv->value);
  else
    label = NULL;
 
  g_free (priv->label);
  priv->label = label;
}

static void
gtk_cell_renderer_progress_set_value (GtkCellRendererProgress *cellprogress, 
				      gint                     value)
{
  if (cellprogress->priv->value != value)
    {
      cellprogress->priv->value = value;
      recompute_label (cellprogress);
      g_object_notify (G_OBJECT (cellprogress), "value");
    }
}

static void
gtk_cell_renderer_progress_set_text (GtkCellRendererProgress *cellprogress, 
				     const gchar             *text)
{
  gchar *new_text;

  new_text = g_strdup (text);
  g_free (cellprogress->priv->text);
  cellprogress->priv->text = new_text;
  recompute_label (cellprogress);
  g_object_notify (G_OBJECT (cellprogress), "text");
}

static void
gtk_cell_renderer_progress_set_pulse (GtkCellRendererProgress *cellprogress, 
				      gint                     pulse)
{
  GtkCellRendererProgressPrivate *priv = cellprogress->priv;

  if (pulse != priv->pulse)
    {
      if (pulse <= 0)
        priv->offset = 0;
      else
        priv->offset = pulse;
      g_object_notify (G_OBJECT (cellprogress), "pulse");
    }

  priv->pulse = pulse;
  recompute_label (cellprogress);
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
  gint xpad, ypad;
  
  layout = gtk_widget_create_pango_layout (widget, text);
  pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

  gtk_cell_renderer_get_padding (cell, &xpad, &ypad);
  
  if (width)
    *width = logical_rect.width + xpad * 2;
  
  if (height)
    *height = logical_rect.height + ypad * 2;

  g_object_unref (layout);
}

static void
gtk_cell_renderer_progress_get_size (GtkCellRenderer    *cell,
				     GtkWidget          *widget,
				     const GdkRectangle *cell_area,
				     gint               *x_offset,
				     gint               *y_offset,
				     gint               *width,
				     gint               *height)
{
  GtkCellRendererProgress *cellprogress = GTK_CELL_RENDERER_PROGRESS (cell);
  GtkCellRendererProgressPrivate *priv = cellprogress->priv;
  gint w, h;
  gchar *text;

  if (priv->min_w < 0)
    {
      text = g_strdup_printf (C_("progress bar label", "%d %%"), 100);
      compute_dimensions (cell, widget, text,
			  &priv->min_w,
			  &priv->min_h);
      g_free (text);
    }
  
  compute_dimensions (cell, widget, priv->label, &w, &h);
  
  if (width)
    *width = MAX (priv->min_w, w);
  
  if (height)
    *height = MIN (priv->min_h, h);

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

  if (x_offset) *x_offset = 0;
  if (y_offset) *y_offset = 0;
}

static inline gint
get_bar_size (gint pulse,
	      gint value,
	      gint full_size)
{
  gint bar_size;

  if (pulse < 0)
    bar_size = full_size * MAX (0, value) / 100;
  else if (pulse == 0)
    bar_size = 0;
  else if (pulse == G_MAXINT)
    bar_size = full_size;
  else
    bar_size = MAX (2, full_size / 5);

  return bar_size;
}

static inline gint
get_bar_position (gint     start,
		  gint     full_size,
		  gint     bar_size,
		  gint     pulse,
		  gint     offset,
		  gboolean is_rtl)
{
  gint position;

  if (pulse < 0 || pulse == 0 || pulse == G_MAXINT)
    {
      position = is_rtl ? (start + full_size - bar_size) : start;
    }
  else
    {
      position = (is_rtl ? offset + 12 : offset) % 24;
      if (position > 12)
	position = 24 - position;
      position = start + full_size * position / 15;
    }

  return position;
}

static void
gtk_cell_renderer_progress_render (GtkCellRenderer      *cell,
                                   cairo_t              *cr,
				   GtkWidget            *widget,
				   const GdkRectangle   *background_area,
				   const GdkRectangle   *cell_area,
				   GtkCellRendererState  flags)
{
  GtkCellRendererProgress *cellprogress = GTK_CELL_RENDERER_PROGRESS (cell);
  GtkCellRendererProgressPrivate *priv= cellprogress->priv;
  GtkStyleContext *context;
  GtkBorder padding;
  PangoLayout *layout;
  PangoRectangle logical_rect;
  gint x, y, w, h, x_pos, y_pos, bar_position, bar_size, start, full_size;
  gint xpad, ypad;
  GdkRectangle clip;
  gboolean is_rtl;

  context = gtk_widget_get_style_context (widget);
  is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

  gtk_cell_renderer_get_padding (cell, &xpad, &ypad);
  x = cell_area->x + xpad;
  y = cell_area->y + ypad;
  w = cell_area->width - xpad * 2;
  h = cell_area->height - ypad * 2;

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_TROUGH);

  gtk_render_background (context, cr, x, y, w, h);
  gtk_render_frame (context, cr, x, y, w, h);

  gtk_style_context_get_padding (context, GTK_STATE_FLAG_NORMAL, &padding);

  x += padding.left;
  y += padding.top;
  w -= padding.left + padding.right;
  h -= padding.top + padding.bottom;

  gtk_style_context_restore (context);

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      clip.y = y;
      clip.height = h;

      start = x;
      full_size = w;

      bar_size = get_bar_size (priv->pulse, priv->value, full_size);

      if (!priv->inverted)
	bar_position = get_bar_position (start, full_size, bar_size,
					 priv->pulse, priv->offset, is_rtl);
      else
	bar_position = get_bar_position (start, full_size, bar_size,
					 priv->pulse, priv->offset, !is_rtl);

      clip.width = bar_size;
      clip.x = bar_position;
    }
  else
    {
      clip.x = x;
      clip.width = w;

      start = y;
      full_size = h;

      bar_size = get_bar_size (priv->pulse, priv->value, full_size);

      if (priv->inverted)
	bar_position = get_bar_position (start, full_size, bar_size,
					 priv->pulse, priv->offset, TRUE);
      else
	bar_position = get_bar_position (start, full_size, bar_size,
					 priv->pulse, priv->offset, FALSE);

      clip.height = bar_size;
      clip.y = bar_position;
    }

  if (bar_size > 0)
    {
      gtk_style_context_save (context);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_PROGRESSBAR);

      gtk_render_background (context, cr, clip.x, clip.y, clip.width, clip.height);
      gtk_render_frame (context, cr, clip.x, clip.y, clip.width, clip.height);

      gtk_style_context_restore (context);
    }

  if (priv->label)
    {
      gfloat text_xalign;

      layout = gtk_widget_create_pango_layout (widget, priv->label);
      pango_layout_get_pixel_extents (layout, NULL, &logical_rect);

      if (gtk_widget_get_direction (widget) != GTK_TEXT_DIR_LTR)
	text_xalign = 1.0 - priv->text_xalign;
      else
	text_xalign = priv->text_xalign;

      x_pos = x + padding.left + text_xalign *
	(w - padding.left - padding.right - logical_rect.width);

      y_pos = y + padding.top + priv->text_yalign *
	(h - padding.top - padding.bottom - logical_rect.height);

      cairo_save (cr);
      gdk_cairo_rectangle (cr, &clip);
      cairo_clip (cr);

      gtk_style_context_save (context);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_PROGRESSBAR);

      gtk_render_layout (context, cr,
                         x_pos, y_pos,
                         layout);

      gtk_style_context_restore (context);
      cairo_restore (cr);

      gtk_style_context_save (context);
      gtk_style_context_add_class (context, GTK_STYLE_CLASS_TROUGH);

      if (bar_position > start)
        {
	  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
	      clip.x = x;
	      clip.width = bar_position - x;
	    }
	  else
	    {
	      clip.y = y;
	      clip.height = bar_position - y;
	    }

          cairo_save (cr);
          gdk_cairo_rectangle (cr, &clip);
          cairo_clip (cr);

          gtk_render_layout (context, cr,
                             x_pos, y_pos,
                             layout);

          cairo_restore (cr);
        }

      if (bar_position + bar_size < start + full_size)
        {
	  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
	      clip.x = bar_position + bar_size;
	      clip.width = x + w - (bar_position + bar_size);
	    }
	  else
	    {
	      clip.y = bar_position + bar_size;
	      clip.height = y + h - (bar_position + bar_size);
	    }

          cairo_save (cr);
          gdk_cairo_rectangle (cr, &clip);
          cairo_clip (cr);

          gtk_render_layout (context, cr,
                             x_pos, y_pos,
                             layout);

          cairo_restore (cr);
        }

      gtk_style_context_restore (context);
      g_object_unref (layout);
    }
}
