#include "config.h"

#include "gtkglyphpaintable.h"
#include <hb-gobject.h>
#include <hb-cairo.h>
#include <hb-ot.h>

struct _GtkGlyphPaintable
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

struct _GtkGlyphPaintableClass
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
gtk_glyph_paintable_snapshot_symbolic (GtkSymbolicPaintable  *paintable,
                                       GdkSnapshot           *snapshot,
                                       double                 width,
                                       double                 height,
                                       const GdkRGBA         *colors,
                                       gsize                  n_colors)
{
  GtkGlyphPaintable *self = GTK_GLYPH_PAINTABLE (paintable);
  GskRenderNode *node;
  GdkRGBA foreground_color;
  unsigned int num_colors = 0;
  GdkRGBA *_colors = NULL;

  if (self->face == NULL)
    return;

  if (n_colors > 0)
    foreground_color = colors[0];
  else
    foreground_color = self->color;

  if (self->custom_colors)
    {
      char **entries = g_strsplit (self->custom_colors, ",", -1);

      num_colors = g_strv_length (entries);
      if (n_colors > 1)
        num_colors = MAX (n_colors - 1, num_colors);

      _colors = g_new0 (GdkRGBA, num_colors);

      for (int i = 0; entries[i]; i++)
        {
          unsigned int r, g, b, a;

          if (sscanf (entries[i], "%2x%2x%2x%2x", &r, &g, &b, &a) == 4)
            {
              _colors[i].red = r / 255.;
              _colors[i].green = g / 255.;
              _colors[i].blue = b / 255.;
              _colors[i].alpha = a / 255.;
            }
        }

       g_strfreev (entries);
    }

  if (n_colors > 1 && !colors)
    colors = g_new0 (GdkRGBA, n_colors - 1);

  for (int i = 0; i + 1 < MIN (4, n_colors); i++)
    {
      _colors[i].red = colors[i + 1].red;
      _colors[i].green = colors[i + 1].green;
      _colors[i].blue = colors[i + 1].blue;
      _colors[i].alpha = colors[i + 1].alpha;
    }

  node = gsk_glyph_node_new (&GRAPHENE_RECT_INIT (0, 0, width, height),
                             self->font,
                             self->glyph,
                             self->palette_index,
                             &foreground_color,
                             num_colors,
                             _colors);

  gtk_snapshot_append_node (snapshot, node);

  gsk_render_node_unref (node);
}

static void
gtk_glyph_paintable_snapshot (GdkPaintable *paintable,
                              GdkSnapshot  *snapshot,
                              double        width,
                              double        height)
{
  gtk_glyph_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (paintable),
                                         snapshot, width, height, NULL, 0);
}

static int
gtk_glyph_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkGlyphPaintable *self = GTK_GLYPH_PAINTABLE (paintable);
  hb_glyph_extents_t extents;

  if (!self->font)
    return 0;

  if (hb_font_get_glyph_extents (self->font, self->glyph, &extents))
    return extents.width / (1 << self->subpixel_bits);

  return 0;
}

static int
gtk_glyph_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkGlyphPaintable *self = GTK_GLYPH_PAINTABLE (paintable);
  hb_glyph_extents_t extents;

  if (!self->font)
    return 0;

  if (hb_font_get_glyph_extents (self->font, self->glyph, &extents))
    return (-extents.height) / (1 << self->subpixel_bits);

  return 0;
}

static void
gtk_glyph_paintable_init_interface (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_glyph_paintable_snapshot;
  iface->get_intrinsic_width = gtk_glyph_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_glyph_paintable_get_intrinsic_height;
}

static void
glyph_symbolic_paintable_init_interface (GtkSymbolicPaintableInterface *iface)
{
  iface->snapshot_symbolic = gtk_glyph_paintable_snapshot_symbolic;
}

G_DEFINE_TYPE_WITH_CODE (GtkGlyphPaintable, gtk_glyph_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_glyph_paintable_init_interface)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SYMBOLIC_PAINTABLE,
                                                glyph_symbolic_paintable_init_interface))

static void
gtk_glyph_paintable_init (GtkGlyphPaintable *self)
{
  self->subpixel_bits = 6;
  self->color = (GdkRGBA) { 0, 0, 0, 1 };
}

static void
gtk_glyph_paintable_dispose (GObject *object)
{
  GtkGlyphPaintable *self = GTK_GLYPH_PAINTABLE (object);

  g_clear_pointer (&self->face, hb_face_destroy);
  g_clear_pointer (&self->font, hb_font_destroy);
  g_free (self->variations);
  g_free (self->custom_colors);

  G_OBJECT_CLASS (gtk_glyph_paintable_parent_class)->dispose (object);
}

static void
gtk_glyph_paintable_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkGlyphPaintable *self = GTK_GLYPH_PAINTABLE (object);

  switch (prop_id)
    {
    case PROP_FACE:
      gtk_glyph_paintable_set_face (self, (hb_face_t *)g_value_get_boxed (value));
      break;

    case PROP_GLYPH:
      gtk_glyph_paintable_set_glyph (self, g_value_get_uint (value));
      break;

    case PROP_VARIATIONS:
      gtk_glyph_paintable_set_variations (self, g_value_get_string (value));
      break;

    case PROP_COLOR:
      gtk_glyph_paintable_set_color (self, g_value_get_boxed (value));
      break;

    case PROP_PALETTE_INDEX:
      gtk_glyph_paintable_set_palette_index (self, g_value_get_uint (value));
      break;

    case PROP_CUSTOM_COLORS:
      gtk_glyph_paintable_set_custom_colors (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_glyph_paintable_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkGlyphPaintable *self = GTK_GLYPH_PAINTABLE (object);

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
gtk_glyph_paintable_class_init (GtkGlyphPaintableClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = gtk_glyph_paintable_dispose;
  object_class->set_property = gtk_glyph_paintable_set_property;
  object_class->get_property = gtk_glyph_paintable_get_property;

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
gtk_glyph_paintable_new (hb_face_t *face)
{
  return g_object_new (GTK_TYPE_GLYPH_PAINTABLE,
                       "face", face,
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
update_font (GtkGlyphPaintable *self)
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

static void
guess_default_glyph (GtkGlyphPaintable *self)
{
  hb_codepoint_t glyph;

  if (hb_font_get_glyph_from_name (self->font, "icon0", -1, &glyph) ||
      hb_font_get_glyph_from_name (self->font, "A", -1, &glyph))
    {
      self->glyph = glyph;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GLYPH]);
    }
}

void
gtk_glyph_paintable_set_face (GtkGlyphPaintable *self,
                          hb_face_t      *face)
{
  g_return_if_fail (GTK_IS_GLYPH_PAINTABLE (self));

  g_clear_pointer (&self->face, hb_face_destroy);

  if (face)
    self->face = hb_face_reference (face);

  update_font (self);
  guess_default_glyph (self);

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACE]);
}

hb_face_t *
gtk_glyph_paintable_get_face (GtkGlyphPaintable *self)
{
  g_return_val_if_fail (GTK_IS_GLYPH_PAINTABLE (self), NULL);

  return self->face;
}

void
gtk_glyph_paintable_set_glyph (GtkGlyphPaintable *self,
                               hb_codepoint_t  glyph)
{
  g_return_if_fail (GTK_IS_GLYPH_PAINTABLE (self));

  self->glyph = glyph;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GLYPH]);
}

hb_codepoint_t
gtk_glyph_paintable_get_glyph (GtkGlyphPaintable *self)
{
  g_return_val_if_fail (GTK_IS_GLYPH_PAINTABLE (self), 0);

  return self->glyph;
}

void
gtk_glyph_paintable_set_palette_index (GtkGlyphPaintable *self,
                                       unsigned int    palette_index)
{
  g_return_if_fail (GTK_IS_GLYPH_PAINTABLE (self));

  self->palette_index = palette_index;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PALETTE_INDEX]);
}

unsigned int
gtk_glyph_paintable_get_palette_index (GtkGlyphPaintable *self)
{
  g_return_val_if_fail (GTK_IS_GLYPH_PAINTABLE (self), 0);

  return self->palette_index;
}

void
gtk_glyph_paintable_set_variations (GtkGlyphPaintable *self,
                                    const char     *variations)
{
  g_return_if_fail (GTK_IS_GLYPH_PAINTABLE (self));

  g_free (self->variations);
  self->variations = g_strdup (variations);

  update_font (self);

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VARIATIONS]);
}

const char *
gtk_glyph_paintable_get_variations (GtkGlyphPaintable *self)
{
  g_return_val_if_fail (GTK_IS_GLYPH_PAINTABLE (self), NULL);

  return self->variations;
}

void
gtk_glyph_paintable_set_custom_colors (GtkGlyphPaintable *self,
                                       const char     *custom_colors)
{
  g_return_if_fail (GTK_IS_GLYPH_PAINTABLE (self));

  g_free (self->custom_colors);
  self->custom_colors = g_strdup (custom_colors);

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CUSTOM_COLORS]);
}

const char *
gtk_glyph_paintable_get_custom_colors (GtkGlyphPaintable *self)
{
  g_return_val_if_fail (GTK_IS_GLYPH_PAINTABLE (self), NULL);

  return self->custom_colors;
}

void
gtk_glyph_paintable_set_color (GtkGlyphPaintable *self,
                               const GdkRGBA  *color)
{
  g_return_if_fail (GTK_IS_GLYPH_PAINTABLE (self));

  if (gdk_rgba_equal (&self->color, color))
    return;

  self->color = *color;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLOR]);
}

const GdkRGBA *
gtk_glyph_paintable_get_color (GtkGlyphPaintable *self)
{
  g_return_val_if_fail (GTK_IS_GLYPH_PAINTABLE (self), NULL);

  return &self->color;
}
