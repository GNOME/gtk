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

#include "gtkcssshadowvalueprivate.h"

#include "gtkcsscolorvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtksnapshotprivate.h"
#include "gtkpangoprivate.h"
#include "gtkcssnodeprivate.h"

#include "gsk/gskcairoblurprivate.h"
#include "gsk/gskroundedrectprivate.h"
#include "gdk/gdkrgbaprivate.h"

#include <math.h>

typedef struct {
  guint inset :1;

  GtkCssValue *hoffset;
  GtkCssValue *voffset;
  GtkCssValue *radius;
  GtkCssValue *spread;
  GtkCssValue *color;
} ShadowValue;

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  guint is_filter : 1; /* values stored in radius are std_dev, for drop-shadow */
  guint n_shadows;
  ShadowValue shadows[1];
};

static GtkCssValue *    gtk_css_shadow_value_new     (ShadowValue *shadows,
                                                      guint        n_shadows,
                                                      gboolean     is_filter);

static void
shadow_value_for_transition (ShadowValue *result,
                             gboolean     inset)
{
  result->inset = inset;
  result->hoffset = gtk_css_number_value_new (0, GTK_CSS_PX);
  result->voffset = gtk_css_number_value_new (0, GTK_CSS_PX);
  result->radius = gtk_css_number_value_new (0, GTK_CSS_PX);
  result->spread = gtk_css_number_value_new (0, GTK_CSS_PX);
  result->color = gtk_css_color_value_new_transparent ();
}

static void
shadow_value_unref (const ShadowValue *shadow)
{
  gtk_css_value_unref (shadow->hoffset);
  gtk_css_value_unref (shadow->voffset);
  gtk_css_value_unref (shadow->spread);
  gtk_css_value_unref (shadow->radius);
  gtk_css_value_unref (shadow->color);
}

static gboolean
shadow_value_transition (const ShadowValue *start,
                         const ShadowValue *end,
                         guint              property_id,
                         double             progress,
                         ShadowValue       *result)

{
  if (start->inset != end->inset)
    return FALSE;

  result->inset = start->inset;
  result->hoffset = gtk_css_value_transition (start->hoffset, end->hoffset, property_id, progress);
  result->voffset = gtk_css_value_transition (start->voffset, end->voffset, property_id, progress);
  result->radius = gtk_css_value_transition (start->radius, end->radius, property_id, progress);
  result->spread = gtk_css_value_transition (start->spread, end->spread, property_id, progress);
  result->color = gtk_css_value_transition (start->color, end->color, property_id, progress);

  return TRUE;
}

static void
gtk_css_value_shadow_free (GtkCssValue *value)
{
  guint i;

  for (i = 0; i < value->n_shadows; i ++)
    {
      const ShadowValue *shadow = &value->shadows[i];

      shadow_value_unref (shadow);
    }

  g_free (value);
}

static GtkCssValue *
gtk_css_value_shadow_compute (GtkCssValue          *value,
                              guint                 property_id,
                              GtkCssComputeContext *context)
{
  guint i;
  ShadowValue *shadows;

  shadows = g_alloca (sizeof (ShadowValue) * value->n_shadows);

  for (i = 0; i < value->n_shadows; i++)
    {
      const ShadowValue *shadow = &value->shadows[i];

      shadows[i].hoffset = gtk_css_value_compute (shadow->hoffset, property_id, context);
      shadows[i].voffset = gtk_css_value_compute (shadow->voffset, property_id, context);
      shadows[i].radius = gtk_css_value_compute (shadow->radius, property_id, context);
      shadows[i].spread = gtk_css_value_compute (shadow->spread, property_id, context),
      shadows[i].color = gtk_css_value_compute (shadow->color, property_id, context);
      shadows[i].inset = shadow->inset;
    }

  return gtk_css_shadow_value_new (shadows, value->n_shadows, value->is_filter);
}

static GtkCssValue *
gtk_css_value_shadow_resolve (GtkCssValue          *value,
                              GtkCssComputeContext *context,
                              GtkCssValue          *current_color)
{
  guint i;
  ShadowValue *shadows;

  if (!gtk_css_value_contains_current_color (value))
    return gtk_css_value_ref (value);

  shadows = g_alloca (sizeof (ShadowValue) * value->n_shadows);

  for (i = 0; i < value->n_shadows; i++)
    {
      const ShadowValue *shadow = &value->shadows[i];

      shadows[i].hoffset = gtk_css_value_ref (shadow->hoffset);
      shadows[i].voffset = gtk_css_value_ref (shadow->voffset);
      shadows[i].radius = gtk_css_value_ref (shadow->radius);
      shadows[i].spread = gtk_css_value_ref (shadow->spread);
      shadows[i].color = gtk_css_value_resolve (shadow->color, context, current_color);
      shadows[i].inset = shadow->inset;
    }

  return gtk_css_shadow_value_new (shadows, value->n_shadows, value->is_filter);
}

static gboolean
gtk_css_value_shadow_equal (const GtkCssValue *value1,
                            const GtkCssValue *value2)
{
  guint i;

  if (value1->n_shadows != value2->n_shadows)
    return FALSE;

  for (i = 0; i < value1->n_shadows; i++)
    {
      const ShadowValue *shadow1 = &value1->shadows[i];
      const ShadowValue *shadow2 = &value2->shadows[i];

      if (shadow1->inset != shadow2->inset ||
          !gtk_css_value_equal (shadow1->hoffset, shadow2->hoffset) ||
          !gtk_css_value_equal (shadow1->voffset, shadow2->voffset) ||
          !gtk_css_value_equal (shadow1->radius, shadow2->radius) ||
          !gtk_css_value_equal (shadow1->spread, shadow2->spread) ||
          !gtk_css_value_equal (shadow1->color, shadow2->color))
        return FALSE;
    }

  return TRUE;
}

static GtkCssValue *
gtk_css_value_shadow_transition (GtkCssValue *start,
                                 GtkCssValue *end,
                                 guint        property_id,
                                 double       progress)
{

  guint i, len;
  ShadowValue *shadows;

  if (start->n_shadows > end->n_shadows)
    len = start->n_shadows;
  else
    len = end->n_shadows;

  shadows = g_newa (ShadowValue, len);

  for (i = 0; i < MIN (start->n_shadows, end->n_shadows); i++)
    {
      if (!shadow_value_transition (&start->shadows[i], &end->shadows[i],
                                    property_id, progress,
                                    &shadows[i]))
        {
          while (i--)
            shadow_value_unref (&shadows[i]);

          return NULL;
        }
    }

  if (start->n_shadows > end->n_shadows)
    {
      for (; i < len; i++)
        {
          ShadowValue fill;

          shadow_value_for_transition (&fill, start->shadows[i].inset);
          if (!shadow_value_transition (&start->shadows[i], &fill, property_id, progress, &shadows[i]))
            {
              while (i--)
                shadow_value_unref (&shadows[i]);
              shadow_value_unref (&fill);
              return NULL;
            }
          shadow_value_unref (&fill);
        }
    }
  else
    {
      for (; i < len; i++)
        {
          ShadowValue fill;

          shadow_value_for_transition (&fill, end->shadows[i].inset);
          if (!shadow_value_transition (&fill, &end->shadows[i], property_id, progress, &shadows[i]))
            {
              while (i--)
                shadow_value_unref (&shadows[i]);
              shadow_value_unref (&fill);
              return NULL;
            }
          shadow_value_unref (&fill);
        }
    }

  return gtk_css_shadow_value_new (shadows, len, start->is_filter);
}

static void
gtk_css_value_shadow_print (const GtkCssValue *value,
                            GString           *string)
{
  guint i;

  if (value->n_shadows == 0)
    {
      g_string_append (string, "none");
      return;
    }

  for (i = 0; i < value->n_shadows; i ++)
    {
      const ShadowValue *shadow = &value->shadows[i];

      if (i > 0)
        g_string_append (string, ", ");

      gtk_css_value_print (shadow->hoffset, string);
      g_string_append_c (string, ' ');
      gtk_css_value_print (shadow->voffset, string);
      g_string_append_c (string, ' ');
      if (gtk_css_number_value_get (shadow->radius, 100) != 0 ||
          gtk_css_number_value_get (shadow->spread, 100) != 0)
        {
          gtk_css_value_print (shadow->radius, string);
          g_string_append_c (string, ' ');
        }

      if (gtk_css_number_value_get (shadow->spread, 100) != 0)
        {
          gtk_css_value_print (shadow->spread, string);
          g_string_append_c (string, ' ');
        }

      gtk_css_value_print (shadow->color, string);

      if (shadow->inset)
        g_string_append (string, " inset");
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_SHADOW = {
  "GtkCssShadowValue",
  gtk_css_value_shadow_free,
  gtk_css_value_shadow_compute,
  gtk_css_value_shadow_resolve,
  gtk_css_value_shadow_equal,
  gtk_css_value_shadow_transition,
  NULL,
  NULL,
  gtk_css_value_shadow_print,
};

static GtkCssValue shadow_none_singleton = { &GTK_CSS_VALUE_SHADOW, 1, 1, 0, 0, 0, 0 };

GtkCssValue *
gtk_css_shadow_value_new_none (void)
{
  return gtk_css_value_ref (&shadow_none_singleton);
}

static GtkCssValue *
gtk_css_shadow_value_new (ShadowValue *shadows,
                          guint        n_shadows,
                          gboolean     is_filter)
{
  GtkCssValue *retval;
  guint i;

  if (n_shadows == 0)
    return gtk_css_shadow_value_new_none ();

  retval = gtk_css_value_alloc (&GTK_CSS_VALUE_SHADOW, sizeof (GtkCssValue) + sizeof (ShadowValue) * (n_shadows - 1));
  retval->n_shadows = n_shadows;
  retval->is_filter = is_filter;

  memcpy (retval->shadows, shadows, sizeof (ShadowValue) * n_shadows);

  retval->is_computed = TRUE;
  for (i = 0; i < n_shadows; i++)
    {
      const ShadowValue *shadow = &retval->shadows[i];

      if (retval->is_computed &&
          (!gtk_css_value_is_computed (shadow->hoffset) ||
           !gtk_css_value_is_computed (shadow->voffset) ||
           !gtk_css_value_is_computed (shadow->spread) ||
           !gtk_css_value_is_computed (shadow->radius) ||
           !gtk_css_value_is_computed (shadow->color)))
        {
          retval->is_computed = FALSE;
        }

      if (gtk_css_value_contains_current_color (shadow->color))
        retval->contains_current_color = TRUE;
    }

  return retval;
}

GtkCssValue *
gtk_css_shadow_value_new_filter (const GtkCssValue *other)
{
  ShadowValue value;

  value.inset = FALSE;
  value.hoffset = gtk_css_number_value_new (0, GTK_CSS_PX);
  value.voffset = gtk_css_number_value_new (0, GTK_CSS_PX);
  value.radius = gtk_css_number_value_new (0, GTK_CSS_PX);
  value.spread = gtk_css_number_value_new (0, GTK_CSS_PX);
  value.color = gtk_css_value_ref (other->shadows[0].color);

  return gtk_css_shadow_value_new (&value, 1, TRUE);
}

enum {
  HOFFSET,
  VOFFSET,
  RADIUS,
  SPREAD,
  N_VALUES
};

static gboolean
has_inset (GtkCssParser *parser,
           gpointer      option_data,
           gpointer      box_shadow_mode)
{
  return box_shadow_mode && gtk_css_parser_has_ident (parser, "inset");
}

static gboolean
parse_inset (GtkCssParser *parser,
             gpointer      option_data,
             gpointer      box_shadow_mode)
{
  gboolean *inset = option_data;

  if (!gtk_css_parser_try_ident (parser, "inset"))
    {
      g_assert_not_reached ();
      return FALSE;
    }

  *inset = TRUE;

  return TRUE;
}

static gboolean
parse_lengths (GtkCssParser *parser,
               gpointer      option_data,
               gpointer      box_shadow_mode)
{
  GtkCssValue **values = option_data;

  values[HOFFSET] = gtk_css_number_value_parse (parser,
                                                GTK_CSS_PARSE_LENGTH);
  if (values[HOFFSET] == NULL)
    return FALSE;

  values[VOFFSET] = gtk_css_number_value_parse (parser,
                                                GTK_CSS_PARSE_LENGTH);
  if (values[VOFFSET] == NULL)
    return FALSE;

  if (gtk_css_number_value_can_parse (parser))
    {
      values[RADIUS] = gtk_css_number_value_parse (parser,
                                                   GTK_CSS_PARSE_LENGTH
                                                   | GTK_CSS_POSITIVE_ONLY);
      if (values[RADIUS] == NULL)
        return FALSE;
    }
  else
    values[RADIUS] = gtk_css_number_value_new (0.0, GTK_CSS_PX);

  if (box_shadow_mode && gtk_css_number_value_can_parse (parser))
    {
      values[SPREAD] = gtk_css_number_value_parse (parser,
                                                   GTK_CSS_PARSE_LENGTH);
      if (values[SPREAD] == NULL)
        return FALSE;
    }
  else
    values[SPREAD] = gtk_css_number_value_new (0.0, GTK_CSS_PX);

  return TRUE;
}

static gboolean
parse_color (GtkCssParser *parser,
             gpointer      option_data,
             gpointer      box_shadow_mode)
{
  GtkCssValue **color = option_data;
  
  *color = gtk_css_color_value_parse (parser);
  if (*color == NULL)
    return FALSE;

  return TRUE;
}

static gboolean
gtk_css_shadow_value_parse_one (GtkCssParser *parser,
                                gboolean      box_shadow_mode,
                                ShadowValue  *result)
{
  GtkCssValue *values[N_VALUES] = { NULL, };
  GtkCssValue *color = NULL;
  gboolean inset = FALSE;
  GtkCssParseOption options[] =
    {
      { (void *) gtk_css_number_value_can_parse, parse_lengths, values },
      { has_inset, parse_inset, &inset },
      { (void *) gtk_css_color_value_can_parse, parse_color, &color },
    };
  guint i;

  if (!gtk_css_parser_consume_any (parser, options, G_N_ELEMENTS (options), GUINT_TO_POINTER (box_shadow_mode)))
    goto fail;

  if (values[0] == NULL)
    {
      gtk_css_parser_error_syntax (parser, "Expected shadow value to contain a length");
      goto fail;
    }

  if (color == NULL)
    color = gtk_css_color_value_new_current_color ();

  result->hoffset = values[HOFFSET];
  result->voffset = values[VOFFSET];
  result->spread = values[SPREAD];
  result->radius = values[RADIUS];
  result->color = color;
  result->inset = inset;

  return TRUE;

fail:
  for (i = 0; i < N_VALUES; i++)
    {
      g_clear_pointer (&values[i], gtk_css_value_unref);
    }
  g_clear_pointer (&color, gtk_css_value_unref);

  return FALSE;
}

#define MAX_SHADOWS 64
GtkCssValue *
gtk_css_shadow_value_parse (GtkCssParser *parser,
                            gboolean      box_shadow_mode)
{
  ShadowValue shadows[MAX_SHADOWS];
  int n_shadows = 0;
  int i;

  if (gtk_css_parser_try_ident (parser, "none"))
    return gtk_css_shadow_value_new_none ();

  do {
    if (n_shadows == MAX_SHADOWS)
      {
        gtk_css_parser_error_syntax (parser, "Not more than %d shadows supported", MAX_SHADOWS);
        goto fail;
      }
    if (gtk_css_shadow_value_parse_one (parser, box_shadow_mode, &shadows[n_shadows]))
      n_shadows++;

  } while (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA));

  return gtk_css_shadow_value_new (shadows, n_shadows, FALSE);

fail:
  for (i = 0; i < n_shadows; i++)
    {
      const ShadowValue *shadow = &shadows[i];

      shadow_value_unref (shadow);
    }
  return NULL;
}

GtkCssValue *
gtk_css_shadow_value_parse_filter (GtkCssParser *parser)
{
  ShadowValue shadow;

  if (gtk_css_shadow_value_parse_one (parser, FALSE, &shadow))
    return gtk_css_shadow_value_new (&shadow, 1, TRUE);
  else
    return NULL;
}

void
gtk_css_shadow_value_get_extents (const GtkCssValue *value,
                                  GtkBorder         *border)
{
  guint i;

  memset (border, 0, sizeof (GtkBorder));

  for (i = 0; i < value->n_shadows; i ++)
    {
      const ShadowValue *shadow = &value->shadows[i];
      double hoffset, voffset, spread, radius, clip_radius;

      spread = gtk_css_number_value_get (shadow->spread, 0);
      radius = gtk_css_number_value_get (shadow->radius, 0);
      if (!value->is_filter)
        radius = radius / 2.0;
      clip_radius = gsk_cairo_blur_compute_pixels (radius);
      hoffset = gtk_css_number_value_get (shadow->hoffset, 0);
      voffset = gtk_css_number_value_get (shadow->voffset, 0);

      border->top    = MAX (border->top, ceil (clip_radius + spread - voffset));
      border->right  = MAX (border->right, ceil (clip_radius + spread + hoffset));
      border->bottom = MAX (border->bottom, ceil (clip_radius + spread + voffset));
      border->left   = MAX (border->left, ceil (clip_radius + spread - hoffset));
    }
}

void
gtk_css_shadow_value_snapshot_outset (const GtkCssValue    *value,
                                      GtkSnapshot          *snapshot,
                                      const GskRoundedRect *border_box)
{
  guint i;
  double dx, dy, spread, radius;
  GdkColor color;

  g_return_if_fail (value->class == &GTK_CSS_VALUE_SHADOW);

  for (i = 0; i < value->n_shadows; i ++)
    {
      const ShadowValue *shadow = &value->shadows[i];

      if (shadow->inset)
        continue;

      gtk_css_color_to_color (gtk_css_color_value_get_color (shadow->color), &color);

      /* We don't need to draw invisible shadows */
      if (gdk_color_is_clear (&color))
        {
          gdk_color_finish (&color);
          continue;
        }

      dx = gtk_css_number_value_get (shadow->hoffset, 0);
      dy = gtk_css_number_value_get (shadow->voffset, 0);
      spread = gtk_css_number_value_get (shadow->spread, 0);
      radius = gtk_css_number_value_get (shadow->radius, 0);
      if (value->is_filter)
        radius = 2 * radius;

      gtk_snapshot_append_outset_shadow2 (snapshot, border_box,
                                          &color,
                                          &GRAPHENE_POINT_INIT (dx, dy),
                                          spread, radius);
      gdk_color_finish (&color);
    }
}

void
gtk_css_shadow_value_snapshot_inset (const GtkCssValue    *value,
                                     GtkSnapshot          *snapshot,
                                     const GskRoundedRect *padding_box)
{
  guint i;
  double dx, dy, spread, radius;
  GdkColor color;

  g_return_if_fail (value->class == &GTK_CSS_VALUE_SHADOW);

  for (i = 0; i < value->n_shadows; i ++)
    {
      const ShadowValue *shadow = &value->shadows[i];

      if (!shadow->inset)
        continue;

      gtk_css_color_to_color (gtk_css_color_value_get_color (shadow->color), &color);

      /* We don't need to draw invisible shadows */
      if (gdk_color_is_clear (&color))
        {
          gdk_color_finish (&color);
          continue;
        }

      dx = gtk_css_number_value_get (shadow->hoffset, 0);
      dy = gtk_css_number_value_get (shadow->voffset, 0);
      spread = gtk_css_number_value_get (shadow->spread, 0);
      radius = gtk_css_number_value_get (shadow->radius, 0);
      if (value->is_filter)
        radius = 2 * radius;

      /* These are trivial to do with a color node */
      if (spread == 0 && radius == 0 &&
          gsk_rounded_rect_is_rectilinear (padding_box))
        {
          const graphene_rect_t *padding_bounds = &padding_box->bounds;

          if (dx > 0)
            {
              const float y = dy > 0 ? dy : 0;

              gtk_snapshot_append_color2 (snapshot, &color,
                                          &GRAPHENE_RECT_INIT (
                                            padding_bounds->origin.x,
                                            padding_bounds->origin.y + y,
                                            dx,
                                            padding_bounds->size.height - ABS (dy)
                                          ));
            }
          else if (dx < 0)
            {
              const float y = dy > 0 ? dy : 0;

              gtk_snapshot_append_color2 (snapshot, &color,
                                          &GRAPHENE_RECT_INIT (
                                            padding_bounds->origin.x + padding_bounds->size.width + dx,
                                            padding_bounds->origin.y + y,
                                            - dx,
                                            padding_bounds->size.height - ABS (dy)
                                          ));
            }

          if (dy > 0)
            {
              gtk_snapshot_append_color2 (snapshot, &color,
                                          &GRAPHENE_RECT_INIT (
                                            padding_bounds->origin.x,
                                            padding_bounds->origin.y,
                                            padding_bounds->size.width,
                                            dy
                                          ));
            }
          else if (dy < 0)
            {
              gtk_snapshot_append_color2 (snapshot, &color,
                                          &GRAPHENE_RECT_INIT (
                                            padding_bounds->origin.x,
                                            padding_bounds->origin.y + padding_bounds->size.height + dy,
                                            padding_bounds->size.width,
                                            - dy
                                          ));
            }
        }
      else
        {
          gtk_snapshot_append_inset_shadow2 (snapshot,
                                             padding_box,
                                             &color,
                                             &GRAPHENE_POINT_INIT (dx, dy),
                                             spread, radius);
        }

      gdk_color_finish (&color);
    }
}

gboolean
gtk_css_shadow_value_is_clear (const GtkCssValue *value)
{
  guint i;

  if (!value)
    return TRUE;

  for (i = 0; i < value->n_shadows; i++)
    {
      const ShadowValue *shadow = &value->shadows[i];

      if (!gdk_rgba_is_clear (gtk_css_color_value_get_rgba (shadow->color)))
        return FALSE;
    }

  return TRUE;
}

gboolean
gtk_css_shadow_value_is_none (const GtkCssValue *shadow)
{
  return !shadow || shadow->n_shadows == 0;
}

gboolean
gtk_css_shadow_value_push_snapshot (const GtkCssValue *value,
                                    GtkSnapshot       *snapshot)
{
  GskShadow2 *shadows;
  guint i;

  if (gtk_css_shadow_value_is_clear (value))
    return FALSE;

  shadows = g_newa (GskShadow2, value->n_shadows);

  for (i = 0; i < value->n_shadows; i++)
    {
      const ShadowValue *shadow = &value->shadows[i];

      gtk_css_color_to_color (gtk_css_color_value_get_color (shadow->color), &shadows[i].color);
      graphene_point_init (&shadows[i].offset,
                           gtk_css_number_value_get (shadow->hoffset, 0),
                           gtk_css_number_value_get (shadow->voffset, 0));
      shadows[i].radius = gtk_css_number_value_get (shadow->radius, 0);
      if (value->is_filter)
        shadows[i].radius *= 2;
    }

  gtk_snapshot_push_shadow2 (snapshot, shadows, value->n_shadows);

  for (i = 0; i < value->n_shadows; i++)
     gdk_color_finish (&shadows[i].color);

  return TRUE;
}

void
gtk_css_shadow_value_pop_snapshot (const GtkCssValue *value,
                                   GtkSnapshot       *snapshot)
{
  if (!gtk_css_shadow_value_is_clear (value))
    gtk_snapshot_pop (snapshot);
}
