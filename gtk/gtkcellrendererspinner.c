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
#include "deprecated/gtkiconfactory.h"
#include "gtkicontheme.h"
#include "gtkintl.h"
#include "gtksettings.h"
#include "gtktypebuiltins.h"

#undef GDK_DEPRECATED
#undef GDK_DEPRECATED_FOR
#define GDK_DEPRECATED
#define GDK_DEPRECATED_FOR(f)

#include "deprecated/gtkstyle.h"


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

struct _GtkCellRendererSpinnerPrivate
{
  gboolean active;
  guint pulse;
  GtkIconSize icon_size, old_icon_size;
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
static void gtk_cell_renderer_spinner_render       (GtkCellRenderer      *cell,
                                                    cairo_t              *cr,
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
  cell_class->render = gtk_cell_renderer_spinner_render;

  /* GtkCellRendererSpinner:active:
   *
   * Whether the spinner is active (ie. shown) in the cell
   *
   * Since: 2.20
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
   *
   * Since: 2.20
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
   *
   * Since: 2.20
   */
  g_object_class_install_property (object_class,
                                   PROP_SIZE,
                                   g_param_spec_enum ("size",
                                                      P_("Size"),
                                                      P_("The GtkIconSize value that specifies the size of the rendered spinner"),
                                                      GTK_TYPE_ICON_SIZE, GTK_ICON_SIZE_MENU,
                                                      G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

}

static void
gtk_cell_renderer_spinner_init (GtkCellRendererSpinner *cell)
{
  cell->priv = gtk_cell_renderer_spinner_get_instance_private (cell);
  cell->priv->pulse = 0;
  cell->priv->old_icon_size = GTK_ICON_SIZE_INVALID;
  cell->priv->icon_size = GTK_ICON_SIZE_MENU;
}

/**
 * gtk_cell_renderer_spinner_new:
 *
 * Returns a new cell renderer which will show a spinner to indicate
 * activity.
 *
 * Returns: a new #GtkCellRenderer
 *
 * Since: 2.20
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
  GtkCellRendererSpinnerPrivate *priv = cell->priv;

  if (priv->old_icon_size == priv->icon_size)
    return;

  if (!gtk_icon_size_lookup (priv->icon_size, &priv->size, NULL))
    {
      g_warning ("Invalid icon size %u\n", priv->icon_size);
      priv->size = 24;
    }
}

static void
gtk_cell_renderer_spinner_get_property (GObject    *object,
                                        guint       param_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GtkCellRendererSpinner *cell = GTK_CELL_RENDERER_SPINNER (object);
  GtkCellRendererSpinnerPrivate *priv = cell->priv;

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
  GtkCellRendererSpinnerPrivate *priv = cell->priv;

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
            priv->old_icon_size = priv->icon_size;
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
  GtkCellRendererSpinnerPrivate *priv = cell->priv;
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
gtk_cell_renderer_spinner_render (GtkCellRenderer      *cellr,
                                  cairo_t              *cr,
                                  GtkWidget            *widget,
                                  const GdkRectangle   *background_area,
                                  const GdkRectangle   *cell_area,
                                  GtkCellRendererState  flags)
{
  GtkCellRendererSpinner *cell = GTK_CELL_RENDERER_SPINNER (cellr);
  GtkCellRendererSpinnerPrivate *priv = cell->priv;
  GtkStateType state;
  GdkRectangle pix_rect;
  GdkRectangle draw_rect;
  gint xpad, ypad;

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

  state = GTK_STATE_NORMAL;
  if ((gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_INSENSITIVE) ||
      !gtk_cell_renderer_get_sensitive (cellr))
    {
      state = GTK_STATE_INSENSITIVE;
    }
  else
    {
      if ((flags & GTK_CELL_RENDERER_SELECTED) != 0)
        {
          if (gtk_widget_has_focus (widget))
            state = GTK_STATE_SELECTED;
          else
            state = GTK_STATE_ACTIVE;
        }
      else
        state = GTK_STATE_PRELIGHT;
    }

  cairo_save (cr);

  gdk_cairo_rectangle (cr, cell_area);
  cairo_clip (cr);

  gtk_paint_spinner (gtk_widget_get_style (widget),
                           cr,
                           state,
                           widget,
                           "cell",
                           priv->pulse,
                           draw_rect.x, draw_rect.y,
                           draw_rect.width, draw_rect.height);

  cairo_restore (cr);
}
