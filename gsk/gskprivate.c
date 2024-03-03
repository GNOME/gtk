#include "gskresources.h"
#include "gskprivate.h"

#include <cairo.h>
#include <pango/pangocairo.h>
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
 * gsk_get_scaled_font:
 * @font: a `PangoFont`
 * @scale: the scale
 *
 * Returns a font that is just like @font, at a size that
 * is multiplied by @scale.
 *
 * Returns: (transfer full): a scaled version of @font
 */
PangoFont *
gsk_get_scaled_font (PangoFont *font,
                     float      scale)
{
  if (scale == 1.0)
    return g_object_ref (font);

#if PANGO_VERSION_CHECK (1, 52, 0)
  return pango_font_map_reload_font (pango_font_get_font_map (font), font, scale, NULL, NULL);
#else
  GHashTable *fonts;
  int key;
  PangoFont *font2;
  PangoFontDescription *desc;
  int size;
  PangoFontMap *fontmap;
  PangoContext *context;
  cairo_scaled_font_t *sf;
  cairo_font_options_t *options;

  key = (int) roundf (scale * PANGO_SCALE);

  fonts = (GHashTable *) g_object_get_data (G_OBJECT (font), "gsk-scaled-fonts");

  if (fonts)
    {
      font2 = g_hash_table_lookup (fonts, GINT_TO_POINTER (key));
      if (font2)
        return g_object_ref (font2);
    }
  else
    {
      fonts = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
      g_object_set_data_full (G_OBJECT (font), "gsk-scaled-fonts",
                              fonts, (GDestroyNotify) g_hash_table_unref);
    }

  desc = pango_font_describe (font);
  size = pango_font_description_get_size (desc);

  if (pango_font_description_get_size_is_absolute (desc))
    pango_font_description_set_absolute_size (desc, size * scale);
  else
    pango_font_description_set_size (desc, (int) roundf (size * scale));

  fontmap = pango_font_get_font_map (font);
  context = pango_font_map_create_context (fontmap);

  sf = pango_cairo_font_get_scaled_font (PANGO_CAIRO_FONT (font));
  options = cairo_font_options_create ();
  cairo_scaled_font_get_font_options (sf, options);
  pango_cairo_context_set_font_options (context, options);
  cairo_font_options_destroy (options);

  font2 = pango_font_map_load_font (fontmap, context, desc);

  pango_font_description_free (desc);
  g_object_unref (context);

  g_hash_table_insert (fonts, GINT_TO_POINTER (key), font2);

  return g_object_ref (font2);
#endif
}

/*< private >
 * gsk_get_hinted_font:
 * @font: a `PangoFont`
 * @hint_style: hint style to use
 * @antialias: antialiasing to use
 *
 * Returns a font that is just like @font, but uses the
 * given hinting options for its glyphs and metrics.
 *
 * Returns: (transfer full): the modified `PangoFont`
 */
PangoFont *
gsk_get_hinted_font (PangoFont          *font,
                     cairo_hint_style_t  hint_style,
                     cairo_antialias_t   antialias)
{
  cairo_font_options_t *options;
  cairo_scaled_font_t *sf;
  static PangoContext *context = NULL;
  PangoFontDescription *desc G_GNUC_UNUSED;

  options = cairo_font_options_create ();
  sf = pango_cairo_font_get_scaled_font (PANGO_CAIRO_FONT (font));
  cairo_scaled_font_get_font_options (sf, options);

  if (cairo_font_options_get_hint_style (options) == hint_style &&
      cairo_font_options_get_antialias (options) == antialias &&
      cairo_font_options_get_hint_metrics (options) == CAIRO_HINT_METRICS_OFF &&
      cairo_font_options_get_subpixel_order (options) == CAIRO_SUBPIXEL_ORDER_DEFAULT)
    {
      cairo_font_options_destroy (options);
      return g_object_ref (font);
    }

  cairo_font_options_set_hint_style (options, hint_style);
  cairo_font_options_set_antialias (options, antialias);
  cairo_font_options_set_hint_metrics (options, CAIRO_HINT_METRICS_OFF);
  cairo_font_options_set_subpixel_order (options, CAIRO_SUBPIXEL_ORDER_DEFAULT);

  if (!context)
    context = pango_context_new ();

  pango_cairo_context_set_font_options (context, options);
  cairo_font_options_destroy (options);

#if PANGO_VERSION_CHECK (1, 52, 0)
  return pango_font_map_reload_font (pango_font_get_font_map (font), font, 1.0, context, NULL);
#else
  desc = pango_font_describe (font);
  font = pango_font_map_load_font (pango_font_get_font_map (font), context, desc);
  pango_font_description_free (desc);

  return font;
#endif
}
