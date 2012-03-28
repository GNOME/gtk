/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkshadowprivate.h"

#include "gtkstylecontextprivate.h"
#include "gtkthemingengineprivate.h"
#include "gtkpango.h"

typedef struct _GtkShadowElement GtkShadowElement;

struct _GtkShadowElement {
  gint16 hoffset;
  gint16 voffset;
  gint16 radius;
  gint16 spread;

  gboolean inset;

  GdkRGBA color;
  GtkSymbolicColor *symbolic_color;
};

static void
shadow_element_print (GtkShadowElement *element,
                      GString          *str)
{
  gchar *color_str;

  if (element->inset)
    g_string_append (str, "inset ");

  g_string_append_printf (str, "%d %d ",
                          (gint) element->hoffset,
                          (gint) element->voffset);

  if (element->radius != 0)
    g_string_append_printf (str, "%d ", (gint) element->radius);

  if (element->spread != 0)
    g_string_append_printf (str, "%d ", (gint) element->spread);

  if (element->symbolic_color != NULL)
    color_str = gtk_symbolic_color_to_string (element->symbolic_color);
  else
    color_str = gdk_rgba_to_string (&element->color);

  g_string_append (str, color_str);
  g_free (color_str);
}

static void
shadow_element_free (GtkShadowElement *element)
{
  if (element->symbolic_color != NULL)
    gtk_symbolic_color_unref (element->symbolic_color);

  g_slice_free (GtkShadowElement, element);
}

static GtkShadowElement *
shadow_element_new (gdouble hoffset,
                    gdouble voffset,
                    gdouble radius,
                    gdouble spread,
                    gboolean inset,
                    GdkRGBA *color,
                    GtkSymbolicColor *symbolic_color)
{
  GtkShadowElement *retval;

  retval = g_slice_new0 (GtkShadowElement);

  retval->hoffset = hoffset;
  retval->voffset = voffset;
  retval->radius = radius;
  retval->spread = spread;
  retval->inset = inset;

  if (symbolic_color != NULL)
    retval->symbolic_color = gtk_symbolic_color_ref (symbolic_color);

  if (color != NULL)
    retval->color = *color;

  return retval;
}                  

/****************
 * GtkShadow *
 ****************/

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GList *elements;
};

static void
gtk_css_value_shadow_free (GtkCssValue *shadow)
{
  g_list_free_full (shadow->elements,
                    (GDestroyNotify) shadow_element_free);
  g_slice_free (GtkShadow, shadow);
}

static gboolean
gtk_css_value_shadow_equal (const GtkCssValue *shadow1,
                            const GtkCssValue *shadow2)
{
  /* FIXME */
  return shadow1 == shadow2;
}

static void
gtk_css_value_shadow_print (const GtkCssValue *shadow,
                            GString           *string)
{
  gint length;
  GList *l;

  length = g_list_length (shadow->elements);

  if (length == 0)
    {
      g_string_append (string, "none");
      return;
    }

  shadow_element_print (shadow->elements->data, string);

  if (length == 1)
    return;

  for (l = g_list_next (shadow->elements); l != NULL; l = l->next)
    {
      g_string_append (string, ", ");
      shadow_element_print (l->data, string);
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_SHADOW = {
  gtk_css_value_shadow_free,
  gtk_css_value_shadow_equal,
  gtk_css_value_shadow_print
};

static GtkCssValue none_singleton = { &GTK_CSS_VALUE_SHADOW, 1, NULL };

GtkShadow *
_gtk_shadow_new_none (void)
{
  return _gtk_css_value_ref (&none_singleton);
}

GtkShadow *
_gtk_shadow_parse (GtkCssParser *parser)
{
  gboolean have_inset, have_color, have_lengths;
  gdouble hoffset, voffset, blur, spread;
  GtkSymbolicColor *color;
  GtkShadowElement *element;
  GtkShadow *shadow;
  guint i;

  if (_gtk_css_parser_try (parser, "none", TRUE))
    return _gtk_shadow_new_none ();

  shadow = _gtk_css_value_new (GtkShadow, &GTK_CSS_VALUE_SHADOW);

  do
    {
      have_inset = have_lengths = have_color = FALSE;

      for (i = 0; i < 3; i++)
        {
          if (!have_inset && 
              _gtk_css_parser_try (parser, "inset", TRUE))
            {
              have_inset = TRUE;
              continue;
            }
            
          if (!have_lengths &&
              _gtk_css_parser_try_double (parser, &hoffset))
            {
              have_lengths = TRUE;

              if (!_gtk_css_parser_try_double (parser, &voffset))
                {
                  _gtk_css_parser_error (parser, "Horizontal and vertical offsets are required");
                  _gtk_css_value_unref (shadow);
                  return NULL;
                }

              if (!_gtk_css_parser_try_double (parser, &blur))
                blur = 0;

              if (!_gtk_css_parser_try_double (parser, &spread))
                spread = 0;

              continue;
            }

          if (!have_color)
            {
              have_color = TRUE;

              /* XXX: the color is optional and UA-defined if it's missing,
               * but it doesn't really make sense for us...
               */
              color = _gtk_css_parser_read_symbolic_color (parser);

              if (color == NULL)
                {
                  _gtk_css_value_unref (shadow);
                  return NULL;
                }
            }
        }

      if (!have_color || !have_lengths)
        {
          _gtk_css_parser_error (parser, "Must specify at least color and offsets");
          _gtk_css_value_unref (shadow);
          return NULL;
        }

      element = shadow_element_new (hoffset, voffset,
                                    blur, spread, have_inset,
                                    NULL, color);

      shadow->elements = g_list_append (shadow->elements, element);

      gtk_symbolic_color_unref (color);

    }
  while (_gtk_css_parser_try (parser, ",", TRUE));

  return shadow;
}

GtkShadow *
_gtk_shadow_resolve (GtkShadow       *shadow,
                     GtkStyleContext *context)
{
  GtkShadow *resolved_shadow;
  GtkShadowElement *element, *resolved_element;
  GdkRGBA color;
  GList *l;

  resolved_shadow = _gtk_css_value_new (GtkShadow, &GTK_CSS_VALUE_SHADOW);

  for (l = shadow->elements; l != NULL; l = l->next)
    {
      element = l->data;

      if (!_gtk_style_context_resolve_color (context,
                                             element->symbolic_color,
                                             &color))
        {
          _gtk_css_value_unref (resolved_shadow);
          return NULL;
        }

      resolved_element =
        shadow_element_new (element->hoffset, element->voffset,
                            element->radius, element->spread, element->inset,
                            &color, NULL);

      resolved_shadow->elements =
        g_list_append (resolved_shadow->elements, resolved_element);
    }

  return resolved_shadow;
}

void
_gtk_text_shadow_paint_layout (GtkShadow       *shadow,
                               cairo_t         *cr,
                               PangoLayout     *layout)
{
  GList *l;
  GtkShadowElement *element;

  if (!cairo_has_current_point (cr))
    cairo_move_to (cr, 0, 0);

  /* render shadows starting from the last one,
   * and the others on top.
   */
  for (l = g_list_last (shadow->elements); l != NULL; l = l->prev)
    {
      element = l->data;

      cairo_save (cr);

      cairo_rel_move_to (cr, element->hoffset, element->voffset);
      gdk_cairo_set_source_rgba (cr, &element->color);
      _gtk_pango_fill_layout (cr, layout);

      cairo_rel_move_to (cr, -element->hoffset, -element->voffset);
      cairo_restore (cr);
  }
}

void
_gtk_icon_shadow_paint (GtkShadow *shadow,
			cairo_t *cr)
{
  GList *l;
  GtkShadowElement *element;
  cairo_pattern_t *pattern;

  for (l = g_list_last (shadow->elements); l != NULL; l = l->prev)
    {
      element = l->data;

      cairo_save (cr);
      pattern = cairo_pattern_reference (cairo_get_source (cr));
      gdk_cairo_set_source_rgba (cr, &element->color);

      cairo_translate (cr, element->hoffset, element->voffset);
      cairo_mask (cr, pattern);

      cairo_restore (cr);
      cairo_pattern_destroy (pattern);
    }
}

void
_gtk_icon_shadow_paint_spinner (GtkShadow *shadow,
                                cairo_t   *cr,
                                gdouble    radius,
                                gdouble    progress)
{
  GtkShadowElement *element;
  GList *l;

  for (l = g_list_last (shadow->elements); l != NULL; l = l->prev)
    {
      element = l->data;

      cairo_save (cr);

      cairo_translate (cr, element->hoffset, element->voffset);
      _gtk_theming_engine_paint_spinner (cr,
                                         radius, progress,
                                         &element->color);

      cairo_restore (cr);
    }
}

void
_gtk_box_shadow_render (GtkShadow           *shadow,
                        cairo_t             *cr,
                        const GtkRoundedBox *padding_box)
{
  GtkShadowElement *element;
  GtkRoundedBox box;
  GList *l;

  cairo_save (cr);
  cairo_set_fill_rule (cr, CAIRO_FILL_RULE_EVEN_ODD);

  _gtk_rounded_box_path (padding_box, cr);
  cairo_clip (cr);

  /* render shadows starting from the last one,
   * and the others on top.
   */
  for (l = g_list_last (shadow->elements); l != NULL; l = l->prev)
    {
      element = l->data;

      if (!element->inset)
        continue;

      box = *padding_box;
      _gtk_rounded_box_move (&box, element->hoffset, element->voffset);
      _gtk_rounded_box_shrink (&box,
                               element->spread, element->spread,
                               element->spread, element->spread);

      _gtk_rounded_box_path (&box, cr);
      _gtk_rounded_box_clip_path (padding_box, cr);

      gdk_cairo_set_source_rgba (cr, &element->color);
      cairo_fill (cr);
  }

  cairo_restore (cr);
}
