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

/*< private >
 * gsk_reload_font:
 * @font: a `PangoFont`
 * @scale: the scale to apply
 * @hint_metris: hint metrics to use or `CAIRO_HINT_METRICS_DEFAILT` to keep the
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
  static cairo_font_options_t *options = NULL;
  static PangoContext *context = NULL;
  cairo_scaled_font_t *sf;

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

  if (G_UNLIKELY (options == NULL))
    options = cairo_font_options_create ();

  sf = pango_cairo_font_get_scaled_font (PANGO_CAIRO_FONT (font));
  cairo_scaled_font_get_font_options (sf, options);

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

  pango_context_set_round_glyph_positions (context, hint_metrics == CAIRO_HINT_METRICS_ON);
  pango_cairo_context_set_font_options (context, options);

  last_result = pango_font_map_reload_font (pango_font_get_font_map (font), font, scale, context, NULL);

  return g_object_ref (last_result);
}

/*< private >
 * gsk_get_glyph_string_extents:
 * @glyphs: a `PangoGlyphString`
 * @font: a `PangoFont`
 * @ink_rect: (out): rectangle used to store the extents of the glyph string as drawn
 *
 * Compute the ink extents of a glyph string.
 *
 * This is like [method@Pango.GlyphString.extents], but it
 * ignores hinting of the font.
 */
void
gsk_get_glyph_string_extents (PangoGlyphString *glyphs,
                              PangoFont        *font,
                              PangoRectangle   *ink_rect)
{
  PangoFont *unhinted;

  unhinted = gsk_reload_font (font,
                              1.0,
                              CAIRO_HINT_METRICS_DEFAULT,
                              CAIRO_HINT_STYLE_NONE,
                              CAIRO_ANTIALIAS_DEFAULT);

  pango_glyph_string_extents (glyphs, unhinted, ink_rect, NULL);

  g_object_unref (unhinted);
}

/*< private >
 * gsk_font_get_hint_style:
 * @font: a `PangoFont`
 *
 * Get the hint style from the cairo font options.
 *
 * Returns: the hint style
 */
cairo_hint_style_t
gsk_font_get_hint_style (PangoFont *font)
{
  static cairo_font_options_t *options = NULL;
  cairo_scaled_font_t *sf;
  cairo_hint_style_t style;

  if (G_UNLIKELY (options == NULL))
    options = cairo_font_options_create ();

  sf = pango_cairo_font_get_scaled_font (PANGO_CAIRO_FONT (font));
  cairo_scaled_font_get_font_options (sf, options);
  style = cairo_font_options_get_hint_style (options);

  return style;
}
