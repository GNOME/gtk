#include "config.h"

#include "glyphpaintable.h"
#include <hb-gobject.h>
#include <hb-cairo.h>
#include <hb-ot.h>

struct _GlyphPaintable
{
  GObject parent_instance;
  hb_face_t *face;
  hb_font_t *font;
  hb_codepoint_t glyph;
  unsigned int palette_index;
  unsigned int subpixel_bits;
  char *variations;
  char *custom_colors;
  GdkRGBA color;
};

struct _GlyphPaintableClass
{
  GObjectClass parent_class;
};

enum {
  PROP_FACE = 1,
  PROP_GLYPH,
  PROP_VARIATIONS,
  PROP_COLOR,
  PROP_PALETTE_INDEX,
  PROP_CUSTOM_COLORS,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static void
glyph_paintable_snapshot (GdkPaintable *paintable,
                          GdkSnapshot  *snapshot,
                          double        width,
                          double        height)
{
  GlyphPaintable *self = GLYPH_PAINTABLE (paintable);
  cairo_t *cr;
  unsigned int upem;
  cairo_font_face_t *cairo_face;
  cairo_matrix_t font_matrix, ctm;
  cairo_font_options_t *font_options;
  cairo_scaled_font_t *scaled_font;
  cairo_glyph_t glyph;
  hb_glyph_extents_t extents;
  double draw_scale;

  if (self->face == NULL)
    return;

  hb_font_get_glyph_extents (self->font, self->glyph, &extents);

  upem = hb_face_get_upem (self->face);

  cairo_face = hb_cairo_font_face_create_for_font (self->font);
  hb_cairo_font_face_set_scale_factor (cairo_face, 1 << self->subpixel_bits);

  cairo_matrix_init_identity (&ctm);
  cairo_matrix_init_scale (&font_matrix, (double)upem, (double)upem);

  font_options = cairo_font_options_create ();
  cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_NONE);
  cairo_font_options_set_hint_metrics (font_options, CAIRO_HINT_METRICS_OFF);
#ifdef CAIRO_COLOR_PALETTE_DEFAULT
  cairo_font_options_set_color_palette (font_options, self->palette_index);
#endif
#ifdef HAVE_CAIRO_FONT_OPTIONS_SET_CUSTOM_PALETTE_COLOR
  if (self->custom_colors)
    {
      char **entries = g_strsplit (self->custom_colors, ",", -1);

      for (int i = 0; entries[i]; i++)
        {
          unsigned int r, g, b, a;

          if (sscanf (entries[i], "%2x%2x%2x%2x", &r, &g, &b, &a) == 4)
            cairo_font_options_set_custom_palette_color (font_options, i, r / 255., g / 255., b / 255. , a / 255.);
        }

       g_strfreev (entries);
    }
#endif

  scaled_font = cairo_scaled_font_create (cairo_face, &font_matrix, &ctm, font_options);

  cr = gtk_snapshot_append_cairo (GTK_SNAPSHOT (snapshot),
                                  &GRAPHENE_RECT_INIT (0, 0, width, height));

  cairo_set_scaled_font (cr, scaled_font);

  draw_scale = width / (double) (extents.width / (1 << self->subpixel_bits));

  cairo_scale (cr, draw_scale, draw_scale);

  cairo_set_source_rgba (cr, 0, 0, 0, 0);
  cairo_paint (cr);

  gdk_cairo_set_source_rgba (cr, &self->color);

  glyph.index = self->glyph;
  glyph.x = - extents.x_bearing / (1 << self->subpixel_bits);
  glyph.y = extents.y_bearing / (1 << self->subpixel_bits);

  cairo_show_glyphs (cr, &glyph, 1);

  cairo_scaled_font_destroy (scaled_font);
  cairo_font_options_destroy (font_options);
  cairo_font_face_destroy (cairo_face);

  cairo_destroy (cr);
}

static int
glyph_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GlyphPaintable *self = GLYPH_PAINTABLE (paintable);
  hb_glyph_extents_t extents;

  if (!self->font)
    return 0;

  if (hb_font_get_glyph_extents (self->font, self->glyph, &extents))
    return extents.width / (1 << self->subpixel_bits);

  return 0;
}

static int
glyph_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GlyphPaintable *self = GLYPH_PAINTABLE (paintable);
  hb_glyph_extents_t extents;

  if (!self->font)
    return 0;

  if (hb_font_get_glyph_extents (self->font, self->glyph, &extents))
    return (-extents.height) / (1 << self->subpixel_bits);

  return 0;
}

static void
glyph_paintable_init_interface (GdkPaintableInterface *iface)
{
  iface->snapshot = glyph_paintable_snapshot;
  iface->get_intrinsic_width = glyph_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = glyph_paintable_get_intrinsic_height;
}

G_DEFINE_TYPE_WITH_CODE (GlyphPaintable, glyph_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                glyph_paintable_init_interface))

static void
glyph_paintable_init (GlyphPaintable *self)
{
  self->subpixel_bits = 6;
  self->color = (GdkRGBA) { 0, 0, 0, 1 };
}

static void
glyph_paintable_dispose (GObject *object)
{
  GlyphPaintable *self = GLYPH_PAINTABLE (object);

  g_clear_pointer (&self->face, hb_face_destroy);
  g_clear_pointer (&self->font, hb_font_destroy);
  g_free (self->variations);
  g_free (self->custom_colors);

  G_OBJECT_CLASS (glyph_paintable_parent_class)->dispose (object);
}

static void
glyph_paintable_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GlyphPaintable *self = GLYPH_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_FACE:
      glyph_paintable_set_face (self, (hb_face_t *)g_value_get_boxed (value));
      break;

    case PROP_GLYPH:
      glyph_paintable_set_glyph (self, g_value_get_uint (value));
      break;

    case PROP_VARIATIONS:
      glyph_paintable_set_variations (self, g_value_get_string (value));
      break;

    case PROP_COLOR:
      glyph_paintable_set_color (self, g_value_get_boxed (value));
      break;

    case PROP_PALETTE_INDEX:
      glyph_paintable_set_palette_index (self, g_value_get_uint (value));
      break;

    case PROP_CUSTOM_COLORS:
      glyph_paintable_set_custom_colors (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
glyph_paintable_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GlyphPaintable *self = GLYPH_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_FACE:
      g_value_set_boxed (value, self->face);
      break;

    case PROP_GLYPH:
      g_value_set_uint (value, self->glyph);
      break;

    case PROP_VARIATIONS:
      g_value_set_string (value, self->variations);
      break;

    case PROP_COLOR:
      g_value_set_boxed (value, &self->color);
      break;

    case PROP_PALETTE_INDEX:
      g_value_set_uint (value, self->palette_index);
      break;

    case PROP_CUSTOM_COLORS:
      g_value_set_string (value, self->custom_colors);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
glyph_paintable_class_init (GlyphPaintableClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = glyph_paintable_dispose;
  object_class->set_property = glyph_paintable_set_property;
  object_class->get_property = glyph_paintable_get_property;

  properties[PROP_FACE] = g_param_spec_boxed ("face", NULL, NULL,
                                              HB_GOBJECT_TYPE_FACE,
                                              G_PARAM_READWRITE);

  properties[PROP_GLYPH] = g_param_spec_uint ("glyph", NULL, NULL,
                                              0, G_MAXUINT, 0,
                                              G_PARAM_READWRITE);

  properties[PROP_VARIATIONS] = g_param_spec_string ("variations", NULL, NULL,
                                                     NULL,
                                                     G_PARAM_READWRITE);

  properties[PROP_COLOR] = g_param_spec_boxed ("color", NULL, NULL,
                                               GDK_TYPE_RGBA,
                                               G_PARAM_READWRITE);

  properties[PROP_PALETTE_INDEX] = g_param_spec_uint ("palette-index", NULL, NULL,
                                                      0, G_MAXUINT, 0,
                                                      G_PARAM_READWRITE);

  properties[PROP_CUSTOM_COLORS] = g_param_spec_string ("custom-colors", NULL, NULL,
                                                        NULL,
                                                        G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

GdkPaintable *
glyph_paintable_new (hb_face_t      *face,
                     hb_codepoint_t  glyph)
{
  return g_object_new (GLYPH_TYPE_PAINTABLE,
                       "face", face,
                       "glyph", glyph,
                       NULL);
}

static void
parse_variations (const char     *str,
                  hb_variation_t *variations,
                  unsigned int    length,
                  unsigned int   *n_variations)
{
  const char *p;

  *n_variations = 0;

  p = str;
  while (p && *p && *n_variations < length)
    {
      const char *end = strchr (p, ',');
      if (hb_variation_from_string (p, end ? end - p: -1, &variations[*n_variations]))
        (*n_variations)++;
      p = end ? end + 1 : NULL;
    }
}

static unsigned int
count_variations (const char *string)
{
  unsigned int n;
  const char *p;

  n = 1;
  p = string;
  while ((p = strchr (p + 1, ',')) != NULL)
    n++;

  return n;
}

static void
update_font (GlyphPaintable *self)
{
  unsigned int upem;
  int scale;

  g_clear_pointer (&self->font, hb_font_destroy);

  self->font = hb_font_create (self->face);

  upem = hb_face_get_upem (self->face);
  scale = (int) scalbnf ((double)upem, self->subpixel_bits);

  hb_font_set_scale (self->font, scale, scale);

  if (self->variations)
    {
      unsigned int n_vars;
      hb_variation_t *vars;

      n_vars = count_variations (self->variations);
      vars = g_newa (hb_variation_t, n_vars);
      parse_variations (self->variations, vars, n_vars, &n_vars);
      hb_font_set_variations (self->font, vars, n_vars);
    }
}

void
glyph_paintable_set_face (GlyphPaintable *self,
                          hb_face_t      *face)
{
  g_return_if_fail (GLYPH_IS_PAINTABLE (self));

  g_clear_pointer (&self->face, hb_face_destroy);

  if (face)
    self->face = hb_face_reference (face);

  update_font (self);

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACE]);
}

hb_face_t *
glyph_paintable_get_face (GlyphPaintable *self)
{
  g_return_val_if_fail (GLYPH_IS_PAINTABLE (self), NULL);

  return self->face;
}

void
glyph_paintable_set_glyph (GlyphPaintable *self,
                           hb_codepoint_t  glyph)
{
  g_return_if_fail (GLYPH_IS_PAINTABLE (self));

  self->glyph = glyph;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GLYPH]);
}

hb_codepoint_t
glyph_paintable_get_glyph (GlyphPaintable *self)
{
  g_return_val_if_fail (GLYPH_IS_PAINTABLE (self), 0);

  return self->glyph;
}

void
glyph_paintable_set_palette_index (GlyphPaintable *self,
                                   unsigned int    palette_index)
{
  g_return_if_fail (GLYPH_IS_PAINTABLE (self));

  self->palette_index = palette_index;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PALETTE_INDEX]);
}

unsigned int
glyph_paintable_get_palette_index (GlyphPaintable *self)
{
  g_return_val_if_fail (GLYPH_IS_PAINTABLE (self), 0);

  return self->palette_index;
}

void
glyph_paintable_set_variations (GlyphPaintable *self,
                                const char     *variations)
{
  g_return_if_fail (GLYPH_IS_PAINTABLE (self));

  g_free (self->variations);
  self->variations = g_strdup (variations);

  update_font (self);

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VARIATIONS]);
}

const char *
glyph_paintable_get_variations (GlyphPaintable *self)
{
  g_return_val_if_fail (GLYPH_IS_PAINTABLE (self), NULL);

  return self->variations;
}

void
glyph_paintable_set_custom_colors (GlyphPaintable *self,
                                   const char     *custom_colors)
{
  g_return_if_fail (GLYPH_IS_PAINTABLE (self));

  g_free (self->custom_colors);
  self->custom_colors = g_strdup (custom_colors);

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CUSTOM_COLORS]);
}

const char *
glyph_paintable_get_custom_colors (GlyphPaintable *self)
{
  g_return_val_if_fail (GLYPH_IS_PAINTABLE (self), NULL);

  return self->custom_colors;
}

void
glyph_paintable_set_color (GlyphPaintable *self,
                           const GdkRGBA  *color)
{
  g_return_if_fail (GLYPH_IS_PAINTABLE (self));

  if (gdk_rgba_equal (&self->color, color))
    return;

  self->color = *color;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLOR]);
}

const GdkRGBA *
glyph_paintable_get_color (GlyphPaintable *self)
{
  g_return_val_if_fail (GLYPH_IS_PAINTABLE (self), NULL);

  return &self->color;
}
