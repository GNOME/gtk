/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2009 Matthias Clasen <mclasen@redhat.com>
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2009 Bastien Nocera <hadess@hadess.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.         See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 2007.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkcellrendererspinner.h"
#include "gtkiconhelperprivate.h"
#include "gtkintl.h"
#include "gtksettings.h"
#include "gtksnapshot.h"
#include "gtktypebuiltins.h"
#include "gtkstylecontextprivate.h"
#include "gtkcssnumbervalueprivate.h"

#include <math.h>

/**
 * SECTION:gtkcellrendererspinner
 * @Short_description: Renders a spinning animation in a cell
 * @Title: GtkCellRendererSpinner
 * @See_also: #GtkSpinner, #GtkCellRendererProgress
 *
 * GtkCellRendererSpinner renders a spinning animation in a cell, very
 * similar to #GtkSpinner. It can often be used as an alternative
 * to a #GtkCellRendererProgress for displaying indefinite activity,
 * instead of actual progress.
 *
 * To start the animation in a cell, set the #GtkCellRendererSpinner:active
 * property to %TRUE and increment the #GtkCellRendererSpinner:pulse property
 * at regular intervals. The usual way to set the cell renderer properties
 * for each cell is to bind them to columns in your tree model using e.g.
 * gtk_tree_view_column_add_attribute().
 */


enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_PULSE,
  PROP_SIZE
};

typedef struct _GtkCellRendererSpinnerClass   GtkCellRendererSpinnerClass;
typedef struct _GtkCellRendererSpinnerPrivate GtkCellRendererSpinnerPrivate;

struct _GtkCellRendererSpinner
{
  GtkCellRenderer                parent;
};

struct _GtkCellRendererSpinnerClass
{
  GtkCellRendererClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};

struct _GtkCellRendererSpinnerPrivate
{
  gboolean active;
  guint pulse;
  GtkIconSize icon_size;
  gint size;
};


static void gtk_cell_renderer_spinner_get_property (GObject         *object,
                                                    guint            param_id,
                                                    GValue          *value,
                                                    GParamSpec      *pspec);
static void gtk_cell_renderer_spinner_set_property (GObject         *object,
                                                    guint            param_id,
                                                    const GValue    *value,
                                                    GParamSpec      *pspec);
static void gtk_cell_renderer_spinner_get_size     (GtkCellRenderer *cell,
                                                    GtkWidget          *widget,
                                                    const GdkRectangle *cell_area,
                                                    gint               *x_offset,
                                                    gint               *y_offset,
                                                    gint               *width,
                                                    gint               *height);
static void gtk_cell_renderer_spinner_snapshot     (GtkCellRenderer      *cell,
                                                    GtkSnapshot          *snapshot,
                                                    GtkWidget            *widget,
                                                    const GdkRectangle   *background_area,
                                                    const GdkRectangle   *cell_area,
                                                    GtkCellRendererState  flags);

G_DEFINE_TYPE_WITH_PRIVATE (GtkCellRendererSpinner, gtk_cell_renderer_spinner, GTK_TYPE_CELL_RENDERER)

static void
gtk_cell_renderer_spinner_class_init (GtkCellRendererSpinnerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);

  object_class->get_property = gtk_cell_renderer_spinner_get_property;
  object_class->set_property = gtk_cell_renderer_spinner_set_property;

  cell_class->get_size = gtk_cell_renderer_spinner_get_size;
  cell_class->snapshot = gtk_cell_renderer_spinner_snapshot;

  /* GtkCellRendererSpinner:active:
   *
   * Whether the spinner is active (ie. shown) in the cell
   */
  g_object_class_install_property (object_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         P_("Active"),
                                                         P_("Whether the spinner is active (ie. shown) in the cell"),
                                                         FALSE,
                                                         G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCellRendererSpinner:pulse:
   *
   * Pulse of the spinner. Increment this value to draw the next frame of the
   * spinner animation. Usually, you would update this value in a timeout.
   *
   * By default, the #GtkSpinner widget draws one full cycle of the animation,
   * consisting of 12 frames, in 750 milliseconds.
   */
  g_object_class_install_property (object_class,
                                   PROP_PULSE,
                                   g_param_spec_uint ("pulse",
                                                      P_("Pulse"),
                                                      P_("Pulse of the spinner"),
                                                      0, G_MAXUINT, 0,
                                                      G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCellRendererSpinner:size:
   *
   * The #GtkIconSize value that specifies the size of the rendered spinner.
   */
  g_object_class_install_property (object_class,
                                   PROP_SIZE,
                                   g_param_spec_enum ("size",
                                                      P_("Size"),
                                                      P_("The GtkIconSize value that specifies the size of the rendered spinner"),
                                                      GTK_TYPE_ICON_SIZE, GTK_ICON_SIZE_INHERIT,
                                                      G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

}

static void
gtk_cell_renderer_spinner_init (GtkCellRendererSpinner *cell)
{
  GtkCellRendererSpinnerPrivate *priv = gtk_cell_renderer_spinner_get_instance_private (cell);
  priv->pulse = 0;
  priv->icon_size = GTK_ICON_SIZE_INHERIT;
}

/**
 * gtk_cell_renderer_spinner_new:
 *
 * Returns a new cell renderer which will show a spinner to indicate
 * activity.
 *
 * Returns: a new #GtkCellRenderer
 */
GtkCellRenderer *
gtk_cell_renderer_spinner_new (void)
{
  return g_object_new (GTK_TYPE_CELL_RENDERER_SPINNER, NULL);
}

static void
gtk_cell_renderer_spinner_update_size (GtkCellRendererSpinner *cell,
                                       GtkWidget              *widget)
{
  GtkCellRendererSpinnerPrivate *priv = gtk_cell_renderer_spinner_get_instance_private (cell);
  GtkStyleContext *context;
  GtkCssNode *node;
  GtkCssStyle *style;

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_save (context);

  gtk_style_context_add_class (context, "spinner");
  node = gtk_style_context_get_node (context);
  gtk_icon_size_set_style_classes (node, priv->icon_size);
  style = gtk_css_node_get_style (node);
  priv->size = _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_SIZE), 100);

  gtk_style_context_restore (context);
}

static void
gtk_cell_renderer_spinner_get_property (GObject    *object,
                                        guint       param_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GtkCellRendererSpinner *cell = GTK_CELL_RENDERER_SPINNER (object);
  GtkCellRendererSpinnerPrivate *priv = gtk_cell_renderer_spinner_get_instance_private (cell);

  switch (param_id)
    {
      case PROP_ACTIVE:
        g_value_set_boolean (value, priv->active);
        break;
      case PROP_PULSE:
        g_value_set_uint (value, priv->pulse);
        break;
      case PROP_SIZE:
        g_value_set_enum (value, priv->icon_size);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_cell_renderer_spinner_set_property (GObject      *object,
                                        guint         param_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GtkCellRendererSpinner *cell = GTK_CELL_RENDERER_SPINNER (object);
  GtkCellRendererSpinnerPrivate *priv = gtk_cell_renderer_spinner_get_instance_private (cell);

  switch (param_id)
    {
      case PROP_ACTIVE:
        if (priv->active != g_value_get_boolean (value))
          {
            priv->active = g_value_get_boolean (value);
            g_object_notify (object, "active");
          }
        break;
      case PROP_PULSE:
        if (priv->pulse != g_value_get_uint (value))
          {
            priv->pulse = g_value_get_uint (value);
            g_object_notify (object, "pulse");
          }
        break;
      case PROP_SIZE:
        if (priv->icon_size != g_value_get_enum (value))
          {
            priv->icon_size = g_value_get_enum (value);
            g_object_notify (object, "size");
          }
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
    }
}

static void
gtk_cell_renderer_spinner_get_size (GtkCellRenderer    *cellr,
                                    GtkWidget          *widget,
                                    const GdkRectangle *cell_area,
                                    gint               *x_offset,
                                    gint               *y_offset,
                                    gint               *width,
                                    gint               *height)
{
  GtkCellRendererSpinner *cell = GTK_CELL_RENDERER_SPINNER (cellr);
  GtkCellRendererSpinnerPrivate *priv = gtk_cell_renderer_spinner_get_instance_private (cell);
  gdouble align;
  gint w, h;
  gint xpad, ypad;
  gfloat xalign, yalign;
  gboolean rtl;

  rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;

  gtk_cell_renderer_spinner_update_size (cell, widget);

  g_object_get (cellr,
                "xpad", &xpad,
                "ypad", &ypad,
                "xalign", &xalign,
                "yalign", &yalign,
                NULL);
  w = h = priv->size;

  if (cell_area)
    {
      if (x_offset)
        {
          align = rtl ? 1.0 - xalign : xalign;
          *x_offset = align * (cell_area->width - w);
          *x_offset = MAX (*x_offset, 0);
        }
      if (y_offset)
        {
          align = rtl ? 1.0 - yalign : yalign;
          *y_offset = align * (cell_area->height - h);
          *y_offset = MAX (*y_offset, 0);
        }
    }
  else
    {
      if (x_offset)
        *x_offset = 0;
      if (y_offset)
        *y_offset = 0;
    }

  if (width)
    *width = w;
  if (height)
    *height = h;
}

static void
gtk_paint_spinner (GtkStyleContext *context,
                   cairo_t         *cr,
                   guint            step,
                   gint             x,
                   gint             y,
                   gint             width,
                   gint             height)
{
  GdkRGBA color;
  guint num_steps;
  gdouble dx, dy;
  gdouble radius;
  gdouble half;
  gint i;
  guint real_step;

  num_steps = 12;
  real_step = step % num_steps;

  /* set a clip region for the expose event */
  cairo_rectangle (cr, x, y, width, height);
  cairo_clip (cr);

  cairo_translate (cr, x, y);

  /* draw clip region */
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  gtk_style_context_get_color (context, &color);
  dx = width / 2;
  dy = height / 2;
  radius = MIN (width / 2, height / 2);
  half = num_steps / 2;

  for (i = 0; i < num_steps; i++)
    {
      gint inset = 0.7 * radius;

      /* transparency is a function of time and intial value */
      gdouble t = (gdouble) ((i + num_steps - real_step)
                             % num_steps) / num_steps;

      cairo_save (cr);

      cairo_set_source_rgba (cr,
                             color.red / 65535.,
                             color.green / 65535.,
                             color.blue / 65535.,
                             color.alpha * t);

      cairo_set_line_width (cr, 2.0);
      cairo_move_to (cr,
                     dx + (radius - inset) * cos (i * G_PI / half),
                     dy + (radius - inset) * sin (i * G_PI / half));
      cairo_line_to (cr,
                     dx + radius * cos (i * G_PI / half),
                     dy + radius * sin (i * G_PI / half));
      cairo_stroke (cr);

      cairo_restore (cr);
    }
}

static void
gtk_cell_renderer_spinner_snapshot (GtkCellRenderer      *cellr,
                                    GtkSnapshot          *snapshot,
                                    GtkWidget            *widget,
                                    const GdkRectangle   *background_area,
                                    const GdkRectangle   *cell_area,
                                    GtkCellRendererState  flags)
{
  GtkCellRendererSpinner *cell = GTK_CELL_RENDERER_SPINNER (cellr);
  GtkCellRendererSpinnerPrivate *priv = gtk_cell_renderer_spinner_get_instance_private (cell);
  GdkRectangle pix_rect;
  GdkRectangle draw_rect;
  gint xpad, ypad;
  cairo_t *cr;

  if (!priv->active)
    return;

  gtk_cell_renderer_spinner_get_size (cellr, widget, (GdkRectangle *) cell_area,
                                      &pix_rect.x, &pix_rect.y,
                                      &pix_rect.width, &pix_rect.height);

  g_object_get (cellr,
                "xpad", &xpad,
                "ypad", &ypad,
                NULL);
  pix_rect.x += cell_area->x + xpad;
  pix_rect.y += cell_area->y + ypad;
  pix_rect.width -= xpad * 2;
  pix_rect.height -= ypad * 2;

  if (!gdk_rectangle_intersect (cell_area, &pix_rect, &draw_rect))
    return;

  cr = gtk_snapshot_append_cairo (snapshot,
                                  &GRAPHENE_RECT_INIT (
                                      cell_area->x, cell_area->y,
                                      cell_area->width, cell_area->height
                                  ));

  gtk_paint_spinner (gtk_widget_get_style_context (widget),
                     cr,
                     priv->pulse,
                     draw_rect.x, draw_rect.y,
                     draw_rect.width, draw_rect.height);

  cairo_destroy (cr);
}
