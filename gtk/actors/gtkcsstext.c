/*
 * Copyright © 2012 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkcsstextprivate.h"

#include "gtkdebug.h"
#include "gtkintl.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtkstylecontext.h"
#include "gtktypebuiltins.h"

struct _GtkCssTextPrivate {
  PangoLayout *layout;

  gint width_chars;
  gint max_width_chars;

  guint wrap :1;
};

enum
{
  PROP_0,

  PROP_ELLIPSIZE,
  PROP_TEXT,
  PROP_TEXT_ALIGNMENT,

  PROP_LAST
};

static GParamSpec *obj_props[PROP_LAST];

G_DEFINE_TYPE (GtkCssText, _gtk_css_text, GTK_TYPE_CSS_ACTOR)

static void
gtk_css_text_finalize (GObject *object)
{
  GtkCssText *self = GTK_CSS_TEXT (object);
  GtkCssTextPrivate *priv = self->priv;

  g_object_unref (priv->layout);

  G_OBJECT_CLASS (_gtk_css_text_parent_class)->finalize (object);
}

static void
gtk_css_text_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkCssText *self = GTK_CSS_TEXT (object);

  switch (prop_id)
    {
    case PROP_ELLIPSIZE:
      _gtk_css_text_set_ellipsize (self, g_value_get_enum (value));
      break;

    case PROP_TEXT:
      _gtk_css_text_set_text (self, g_value_get_string (value));
      break;

    case PROP_TEXT_ALIGNMENT:
      _gtk_css_text_set_text_alignment (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_css_text_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GtkCssText *self = GTK_CSS_TEXT (object);

  switch (prop_id)
    {
    case PROP_ELLIPSIZE:
      g_value_set_enum (value, _gtk_css_text_get_ellipsize (self));
      break;

    case PROP_TEXT:
      g_value_set_string (value, _gtk_css_text_get_text (self));
      break;

    case PROP_TEXT_ALIGNMENT:
      g_value_set_enum (value, _gtk_css_text_get_text_alignment (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gtk_css_text_get_measuring_layout:
 * @self: the #GtkCssText
 * @existing_layout: %NULL or an existing layout already in use.
 * @width: the width to measure with in pango units, or -1 for infinite
 *
 * Gets a layout that can be used for measuring sizes. The returned
 * layout will be identical to the label's layout except for the
 * layout's width, which will be set to @width. Do not modify the returned
 * layout.
 *
 * Returns: a new reference to a pango layout
 **/
static PangoLayout *
gtk_css_text_get_measuring_layout (GtkCssText  *self,
                                   PangoLayout *existing_layout,
                                   int          width)
{
  GtkCssTextPrivate *priv = self->priv;
  PangoRectangle rect;
  PangoLayout *copy;

  if (existing_layout != NULL)
    {
      if (existing_layout != priv->layout)
        {
          pango_layout_set_width (existing_layout, width);
          return existing_layout;
        }

      g_object_unref (existing_layout);
    }

  if (pango_layout_get_width (priv->layout) == width)
    {
      g_object_ref (priv->layout);
      return priv->layout;
    }

  /* We can use the label's own layout if we're not allocated a size yet,
   * because we don't need it to be properly setup at that point.
   * This way we can make use of caching upon the label's creation.
   */
  if (_gtk_actor_get_width (GTK_ACTOR (self)) <= 1)
    {
      g_object_ref (priv->layout);
      pango_layout_set_width (priv->layout, width);
      return priv->layout;
    }

  /* oftentimes we want to measure a width that is far wider than the current width,
   * even though the layout would not change if we made it wider. In that case, we
   * can just return the current layout, because for measuring purposes, it will be
   * identical.
   */
  pango_layout_get_extents (priv->layout, NULL, &rect);
  if ((width == -1 || rect.width <= width) &&
      !pango_layout_is_wrapped (priv->layout) &&
      !pango_layout_is_ellipsized (priv->layout))
    {
      g_object_ref (priv->layout);
      return priv->layout;
    }

  copy = pango_layout_copy (priv->layout);
  pango_layout_set_width (copy, width);
  return copy;
}

static GtkSizeRequestMode
gtk_css_text_real_get_request_mode (GtkActor *actor)
{
  /* XXX: Return CONSTANT_SIZE when wrapping is disabled */
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static gint
get_char_pixels (GtkWidget   *self,
                 PangoLayout *layout)
{
  PangoContext *context;
  PangoFontMetrics *metrics;
  gint char_width, digit_width;

  context = pango_layout_get_context (layout);
  metrics = pango_context_get_metrics (context,
                                       pango_context_get_font_description (context),
                                       pango_context_get_language (context));
  char_width = pango_font_metrics_get_approximate_char_width (metrics);
  digit_width = pango_font_metrics_get_approximate_digit_width (metrics);
  pango_font_metrics_unref (metrics);

  return MAX (char_width, digit_width);;
}

static void
gtk_css_text_get_preferred_layout_size (GtkCssText     *self,
                                        PangoRectangle *smallest,
                                        PangoRectangle *widest)
{
  GtkCssTextPrivate *priv = self->priv;
  PangoLayout *layout;
  gint char_pixels;

  /* "width-chars" Hard-coded minimum width:
   *    - minimum size should be MAX (width-chars, strlen ("..."));
   *    - natural size should be MAX (width-chars, strlen (priv->text));
   *
   * "max-width-chars" User specified maximum size requisition
   *    - minimum size should be MAX (width-chars, 0)
   *    - natural size should be MIN (max-width-chars, strlen (priv->text))
   *
   *    For ellipsizing selfs; if max-width-chars is specified: either it is used as 
   *    a minimum size or the self text as a minimum size (natural size still overflows).
   *
   *    For wrapping selfs; A reasonable minimum size is useful to naturally layout
   *    interfaces automatically. In this case if no "width-chars" is specified, the minimum
   *    width will default to the wrap guess that gtk_css_text_ensure_layout() does.
   */

  /* Start off with the pixel extents of an as-wide-as-possible layout */
  layout = gtk_css_text_get_measuring_layout (self, NULL, -1);

  if (priv->width_chars > -1 || priv->max_width_chars > -1)
    char_pixels = get_char_pixels (GTK_WIDGET (self), layout);
  else
    char_pixels = 0;
      
  pango_layout_get_extents (layout, NULL, widest);
  widest->width = MAX (widest->width, char_pixels * priv->width_chars);
  widest->x = widest->y = 0;

  if (pango_layout_get_ellipsize (priv->layout) != PANGO_ELLIPSIZE_NONE || priv->wrap)
    {
      /* a layout with width 0 will be as small as humanly possible */
      layout = gtk_css_text_get_measuring_layout (self,
                                                  layout,
                                                  priv->width_chars > -1 ? char_pixels * priv->width_chars
                                                                         : 0);

      pango_layout_get_extents (layout, NULL, smallest);
      smallest->width = MAX (smallest->width, char_pixels * priv->width_chars);
      smallest->x = smallest->y = 0;

      if (priv->max_width_chars > -1 && widest->width > char_pixels * priv->max_width_chars)
        {
          layout = gtk_css_text_get_measuring_layout (self,
                                                      layout,
                                                      MAX (smallest->width, char_pixels * priv->max_width_chars));
          pango_layout_get_extents (layout, NULL, widest);
          widest->width = MAX (widest->width, char_pixels * priv->width_chars);
          widest->x = widest->y = 0;
        }
    }
  else
    {
      *smallest = *widest;
    }

  if (widest->width < smallest->width)
    *smallest = *widest;

  g_object_unref (layout);
}

static void
gtk_css_text_real_get_preferred_size (GtkActor       *actor,
                                      GtkOrientation  orientation,
                                      gfloat          for_size,
                                      gfloat         *min_size_p,
                                      gfloat         *nat_size_p)
{
  GtkCssText *self = GTK_CSS_TEXT (actor);

  if (for_size < 0)
    {
      PangoRectangle smallest_rect, widest_rect;

      gtk_css_text_get_preferred_layout_size (self, &smallest_rect, &widest_rect);
      
      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          *min_size_p = (double) smallest_rect.width / PANGO_SCALE;
          *nat_size_p = (double) widest_rect.width / PANGO_SCALE;
        }
      else
        {
          *min_size_p = (double) widest_rect.height / PANGO_SCALE;
          *nat_size_p = (double) smallest_rect.height / PANGO_SCALE;
        }
    }
  else
    {
      PangoLayout *layout;

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          /* XXX */
          return gtk_css_text_real_get_preferred_size (actor , orientation, -1, min_size_p, nat_size_p);
        }
      else
        {
          gint text_height;

          layout = gtk_css_text_get_measuring_layout (self, NULL, for_size * PANGO_SCALE);

          pango_layout_get_size (layout, NULL, &text_height);

          *min_size_p = (double) text_height / PANGO_SCALE;
          *nat_size_p = (double) text_height / PANGO_SCALE;

          g_object_unref (layout);
        }
    }
}

static void
gtk_css_text_real_allocate (GtkActor             *actor,
                            const cairo_matrix_t *position,
                            gfloat                width,
                            gfloat                height)
{
  GtkCssText *self = GTK_CSS_TEXT (actor);
  GtkCssTextPrivate *priv = self->priv;

  GTK_ACTOR_CLASS (_gtk_css_text_parent_class)->allocate (actor, position, width, height);

  pango_layout_set_width (priv->layout, width * PANGO_SCALE);
}

static void
gtk_css_text_real_draw (GtkActor *actor,
                        cairo_t  *cr)
{
  GtkCssText *self = GTK_CSS_TEXT (actor);
  GtkCssTextPrivate *priv = self->priv;
  GtkStyleContext *context;

  context = _gtk_css_actor_get_style_context (GTK_CSS_ACTOR (actor));

  GTK_ACTOR_CLASS (_gtk_css_text_parent_class)->draw (actor, cr);

  gtk_render_layout (context,
                     cr,
                     0, 0,
                     priv->layout);
}

static void
_gtk_css_text_class_init (GtkCssTextClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkActorClass *actor_class = GTK_ACTOR_CLASS (klass);

  object_class->finalize = gtk_css_text_finalize;
  object_class->set_property = gtk_css_text_set_property;
  object_class->get_property = gtk_css_text_get_property;

  actor_class->draw = gtk_css_text_real_draw;
  actor_class->get_request_mode = gtk_css_text_real_get_request_mode;
  actor_class->get_preferred_size = gtk_css_text_real_get_preferred_size;
  actor_class->allocate = gtk_css_text_real_allocate;

  /**
   * GtkCssText:ellipsize:
   *
   * The ellipsize mode to use.
   *
   * See also GtkCssText:wrap.
   */
  obj_props[PROP_ELLIPSIZE] =
    g_param_spec_enum ("ellipsize",
                       P_("Ellipsize"),
                       P_("Ellipsize mode to use"),
                       PANGO_TYPE_ELLIPSIZE_MODE,
                       PANGO_ELLIPSIZE_NONE,
                       GTK_PARAM_READWRITE);

  /**
   * GtkCssText:text:
   *
   * The text to display. 
   */
  obj_props[PROP_TEXT] =
    g_param_spec_string ("text",
                         P_("Text"),
                         P_("Text to display"),
                         NULL,
                         GTK_PARAM_READWRITE);

  /**
   * GtkCssText:text-alignment:
   *
   * The alignment of the text.
   *
   * See also GtkActor:halign for aligning the text box.
   */
  obj_props[PROP_ELLIPSIZE] =
    g_param_spec_enum ("text-alignment",
                       P_("Text alignment"),
                       P_("ow to align the rows of text"),
                       PANGO_TYPE_ALIGNMENT,
                       PANGO_ALIGN_LEFT,
                       GTK_PARAM_READWRITE);

  g_object_class_install_properties (object_class, PROP_LAST, obj_props);

  g_type_class_add_private (klass, sizeof (GtkCssTextPrivate));
}

static void
_gtk_css_text_init (GtkCssText *self)
{
  GtkCssTextPrivate *priv;
  PangoContext *context;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                            GTK_TYPE_CSS_TEXT,
                                            GtkCssTextPrivate);

  priv = self->priv;

  context = gdk_pango_context_get_for_screen (_gtk_actor_get_screen (GTK_ACTOR (self)));
  g_warning ("set proper font on context");
  pango_context_set_language (context, gtk_get_default_language ());
  priv->layout = pango_layout_new (context);
  g_object_unref (context);

  priv->max_width_chars = -1;
  priv->width_chars = -1;
}

GtkActor *
_gtk_css_text_new (void)
{
  return g_object_new (GTK_TYPE_CSS_TEXT, NULL);
}

const char *
_gtk_css_text_get_text (GtkCssText *self)
{
  g_return_val_if_fail (GTK_IS_CSS_TEXT (self), NULL);

  return pango_layout_get_text (self->priv->layout);
}

void
_gtk_css_text_set_text (GtkCssText *self,
                        const char *text)
{
  g_return_if_fail (GTK_IS_CSS_TEXT (self));

  pango_layout_set_text (self->priv->layout, text, -1);

  _gtk_actor_queue_relayout (GTK_ACTOR (self));
}

PangoEllipsizeMode
_gtk_css_text_get_ellipsize (GtkCssText *self)
{
  g_return_val_if_fail (GTK_IS_CSS_TEXT (self), PANGO_ELLIPSIZE_NONE);

  return pango_layout_get_ellipsize (self->priv->layout);
}

void
_gtk_css_text_set_ellipsize (GtkCssText         *self,
                             PangoEllipsizeMode  ellipsize)
{
  g_return_if_fail (GTK_IS_CSS_TEXT (self));

  pango_layout_set_ellipsize (self->priv->layout, ellipsize);

  _gtk_actor_queue_relayout (GTK_ACTOR (self));
}

PangoAlignment
_gtk_css_text_get_text_alignment (GtkCssText *self)
{
  g_return_val_if_fail (GTK_IS_CSS_TEXT (self), PANGO_ALIGN_LEFT);

  return pango_layout_get_alignment (self->priv->layout);
}

void
_gtk_css_text_set_text_alignment (GtkCssText     *self,
				  PangoAlignment  alignment)
{
  g_return_if_fail (GTK_IS_CSS_TEXT (self));

  pango_layout_set_alignment (self->priv->layout, alignment);

  _gtk_actor_queue_redraw (GTK_ACTOR (self));
}

