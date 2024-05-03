#include "config.h"

#include "gskresources.h"
#include "gskprivate.h"

#include <cairo.h>
#include <pango/pangocairo.h>
#ifdef HAVE_PANGOFT
#include <pango/pangoft2.h>
#endif
#include <math.h>

static gpointer
register_resources (gpointer data)
{
  _gsk_register_resource ();
  return NULL;
}

void
gsk_ensure_resources (void)
{
  static GOnce register_resources_once = G_ONCE_INIT;

  g_once (&register_resources_once, register_resources, NULL);
}

static cairo_font_options_t *font_options = NULL;

static inline cairo_font_options_t *
pango_font_get_cairo_font_options (PangoFont *font)
{
  cairo_scaled_font_t *sf;

  if (G_UNLIKELY (font_options == NULL))
    font_options = cairo_font_options_create ();

  sf = pango_cairo_font_get_scaled_font (PANGO_CAIRO_FONT (font));
  cairo_scaled_font_get_font_options (sf, font_options);

  return font_options;
}

/*< private >
 * gsk_reload_font:
 * @font: a `PangoFont`
 * @scale: the scale to apply
 * @hint_metris: hint metrics to use or `CAIRO_HINT_METRICS_DEFAULT` to keep the
 *   hint metrics of the font unchanged
 * @hint_style: hint style to use or `CAIRO_HINT_STYLE_DEFAULT` to keep the
 *   hint style of @font unchanged
 * @antialias: antialiasing to use, or `CAIRO_ANTIALIAS_DEFAULT` to keep the
 *   antialias option of @font unchanged
 *
 * Returns a font that is just like @font, but uses the
 * given scale and hinting options for its glyphs and metrics.
 *
 * Returns: (transfer full): the modified `PangoFont`
 */
PangoFont *
gsk_reload_font (PangoFont            *font,
                 float                 scale,
                 cairo_hint_metrics_t  hint_metrics,
                 cairo_hint_style_t    hint_style,
                 cairo_antialias_t     antialias)
{
  static PangoContext *context = NULL;
  cairo_font_options_t *options;

  /* These requests often come in sequentially so keep the result
   * around and re-use it if everything matches.
   */
  static PangoFont *last_font;
  static float last_scale;
  static cairo_hint_metrics_t last_hint_metrics;
  static cairo_hint_style_t last_hint_style;
  static cairo_antialias_t last_antialias;
  static PangoFont *last_result;

  if (last_result != NULL &&
      last_font == font &&
      last_scale == scale &&
      last_hint_metrics == hint_metrics &&
      last_hint_style == hint_style &&
      last_antialias == antialias)
    return g_object_ref (last_result);

  last_scale = scale;
  last_hint_metrics = hint_metrics;
  last_hint_style = hint_style;
  last_antialias = antialias;

  g_set_object (&last_font, font);
  g_clear_object (&last_result);

  options = pango_font_get_cairo_font_options (font);

  if (hint_metrics == CAIRO_HINT_METRICS_DEFAULT)
    hint_metrics = cairo_font_options_get_hint_metrics (options);

  if (hint_style == CAIRO_HINT_STYLE_DEFAULT)
    hint_style = cairo_font_options_get_hint_style (options);

  if (antialias == CAIRO_ANTIALIAS_DEFAULT)
    antialias = cairo_font_options_get_antialias (options);

  if (1.0 == scale &&
      cairo_font_options_get_hint_metrics (options) == hint_metrics &&
      cairo_font_options_get_hint_style (options) == hint_style &&
      cairo_font_options_get_antialias (options) == antialias &&
      cairo_font_options_get_subpixel_order (options) == CAIRO_SUBPIXEL_ORDER_DEFAULT)
    {
      last_result = g_object_ref (font);
      return g_object_ref (font);
    }

  cairo_font_options_set_hint_metrics (options, hint_metrics);
  cairo_font_options_set_hint_style (options, hint_style);
  cairo_font_options_set_antialias (options, antialias);
  cairo_font_options_set_subpixel_order (options, CAIRO_SUBPIXEL_ORDER_DEFAULT);

  if (G_UNLIKELY (context == NULL))
    context = pango_context_new ();

  pango_cairo_context_set_font_options (context, options);

  last_result = pango_font_map_reload_font (pango_font_get_font_map (font), font, scale, context, NULL);

  return g_object_ref (last_result);
}

static inline double
gsk_font_get_size (PangoFont *font)
{
  PangoFontDescription *desc;
  double size;

  desc = pango_font_describe_with_absolute_size (font);

  size = pango_font_description_get_size (desc);

  pango_font_description_free (desc);

  return size / PANGO_SCALE;
}

/*< private >
 * gsk_font_get_rendering:
 * @font: a `PangoFont`
 * @scale: the scale to use
 * @hint_metrics: (out): return location for metrics hinting
 * @hint_style: (out): return location for hint style
 * @antialias: (out): return location for antialiasing
 *
 * Determines the font options to use for rendering with
 * the font at the given scale.
 */
void
gsk_font_get_rendering (PangoFont            *font,
                        float                 scale,
                        cairo_hint_metrics_t *hint_metrics,
                        cairo_hint_style_t   *hint_style,
                        cairo_antialias_t    *antialias)
{
  const cairo_font_options_t *options;

  options = pango_font_get_cairo_font_options (font);

  /* Keep this in sync with gtkwidget.c:update_pango_context */
  if (cairo_font_options_get_antialias (options) == CAIRO_ANTIALIAS_GOOD)
    {
      double font_size;

      font_size = gsk_font_get_size (font) * scale;

      *antialias = CAIRO_ANTIALIAS_GRAY;
      *hint_metrics = CAIRO_HINT_METRICS_OFF;

      /* 31 pixels is equivalent to an 11 pt font at 200 dpi */
      if (font_size > 31)
        *hint_style = CAIRO_HINT_STYLE_NONE;
      else
        *hint_style = CAIRO_HINT_STYLE_SLIGHT;
    }
  else
    {
      *antialias = cairo_font_options_get_antialias (options);
      *hint_metrics = cairo_font_options_get_hint_metrics (options);
      *hint_style = cairo_font_options_get_hint_style (options);
    }

  /* The combination of hint-style != none and hint-metrics == off
   * leads to broken rendering with some fonts.
   */
  if (*hint_style != CAIRO_HINT_STYLE_NONE)
    *hint_metrics = CAIRO_HINT_METRICS_ON;
}

/*< private >
 * gsk_font_get_extents:
 * @font: a `PangoFont`
 * @glyphs: a `PangoGlyphString`
 * @ink_rect: (out): rectangle used to store the extents of the glyph
 *   string as drawn
 *
 * Compute the ink extents of a glyph string.
 *
 * This is like [method@Pango.GlyphString.extents], but it provides
 * unhinted extents.
 */
void
gsk_font_get_extents (PangoFont        *font,
                      PangoGlyphString *glyphs,
                      PangoRectangle   *ink_rect)
{
  PangoFont *unhinted;

  unhinted = gsk_reload_font (font, 1.0, CAIRO_HINT_METRICS_OFF,
                                         CAIRO_HINT_STYLE_NONE,
                                         CAIRO_ANTIALIAS_GRAY);

  pango_glyph_string_extents (glyphs, unhinted, ink_rect, NULL);

  /* Hack: Without this, cff fonts like Fira get clipped */
  ink_rect->x -= 1024;
  ink_rect->y -= 1024;
  ink_rect->width += 2048;
  ink_rect->height += 2048;

  g_object_unref (unhinted);
}
