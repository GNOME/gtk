#include "config.h"

#include "gtkglyphpaintable.h"
#include <hb-gobject.h>
#include <hb-cairo.h>
#include <hb-ot.h>

#define SUBPIXEL_BITS 6

struct _GtkGlyphPaintable
{
  GObject parent_instance;
  hb_face_t *face;
  hb_font_t *font;
  hb_codepoint_t glyph;
  unsigned int palette_index;
  char *variations;
  GdkRGBA *custom_palette;
  unsigned int num_palette_entries;
  char *custom_colors;
  GdkRGBA color;
  unsigned int uses_foreground : 1;
  unsigned int uses_palette    : 1;
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
  GdkRGBA *custom_palette = NULL;

  if (self->face == NULL)
    return;

  if (n_colors > 0)
    foreground_color = colors[0];
  else
    foreground_color = self->color;

  if (self->custom_palette && colors)
    {
      num_colors = self->num_palette_entries;
      custom_palette = g_newa (GdkRGBA, num_colors);

      memcpy (custom_palette, self->custom_palette, sizeof (GdkRGBA) * num_colors);
      memcpy (custom_palette, &colors[1], sizeof (GdkRGBA) * MIN (n_colors - 1, num_colors));
    }
  else if (self->custom_palette)
    {
      num_colors = self->num_palette_entries;
      custom_palette = self->custom_palette;
    }
  else if (n_colors > 1)
    {
      num_colors = n_colors - 1;
      custom_palette = (GdkRGBA *)&colors[1];
    }

  node = gsk_glyph_node_new (&GRAPHENE_RECT_INIT (0, 0, width, height),
                             self->font,
                             self->glyph,
                             self->palette_index,
                             &foreground_color,
                             num_colors,
                             custom_palette);

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
    return extents.width / (1 << SUBPIXEL_BITS);

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
    return (-extents.height) / (1 << SUBPIXEL_BITS);

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
  g_free (self->custom_palette);

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
paint_color (hb_paint_funcs_t *funcs,
             void *paint_data,
             hb_bool_t use_foreground,
             hb_color_t color,
             void *user_data)
{
  GtkGlyphPaintable *self = GTK_GLYPH_PAINTABLE (paint_data);

  if (use_foreground)
    self->uses_foreground = TRUE;
}

static void
handle_color_line (GtkGlyphPaintable *self,
                   hb_color_line_t   *color_line)
{
  unsigned int len;
  hb_color_stop_t *stops;

  len = hb_color_line_get_color_stops (color_line, 0, NULL, NULL);
  stops = g_newa (hb_color_stop_t, len);
  hb_color_line_get_color_stops (color_line, 0, &len, stops);
  for (unsigned int i = 0; i < len; i++)
    {
      if (stops[i].is_foreground)
        {
          self->uses_foreground = TRUE;
          break;
        }
    }
}

static void
linear_gradient (hb_paint_funcs_t *funcs,
                 void *paint_data,
                 hb_color_line_t *color_line,
                 float x0, float y0,
                 float x1, float y1,
                 float x2, float y2,
                 void *user_data)
{
  GtkGlyphPaintable *self = GTK_GLYPH_PAINTABLE (paint_data);

  handle_color_line (self, color_line);
}

static void
radial_gradient (hb_paint_funcs_t *funcs,
                 void *paint_data,
                 hb_color_line_t *color_line,
                 float x0, float y0, float r0,
                 float x1, float y1, float r1,
                 void *user_data)
{
  GtkGlyphPaintable *self = GTK_GLYPH_PAINTABLE (paint_data);

  handle_color_line (self, color_line);
}

static void
sweep_gradient (hb_paint_funcs_t *funcs,
                void *paint_data,
                hb_color_line_t *color_line,
                float x0, float y0,
                float start_angle, float end_angle,
                void *user_data)
{
  GtkGlyphPaintable *self = GTK_GLYPH_PAINTABLE (paint_data);

  handle_color_line (self, color_line);
}

static hb_bool_t
custom_palette_color (hb_paint_funcs_t *funcs,
                      void *paint_data,
                      unsigned int color_index,
                      hb_color_t *color,
                      void *user_data)
{
  GtkGlyphPaintable *self = GTK_GLYPH_PAINTABLE (paint_data);

  self->uses_palette = TRUE;

  return FALSE;
}

static hb_paint_funcs_t *
get_classify_funcs (void)
{
  static hb_paint_funcs_t *funcs;

  if (funcs)
    return funcs;

  funcs = hb_paint_funcs_create ();

  hb_paint_funcs_set_color_func (funcs, paint_color, NULL, NULL);
  hb_paint_funcs_set_linear_gradient_func (funcs, linear_gradient, NULL, NULL);
  hb_paint_funcs_set_radial_gradient_func (funcs, radial_gradient, NULL, NULL);
  hb_paint_funcs_set_sweep_gradient_func (funcs, sweep_gradient, NULL, NULL);
  hb_paint_funcs_set_custom_palette_color_func (funcs, custom_palette_color, NULL, NULL);

  hb_paint_funcs_make_immutable (funcs);

  return funcs;
}

static void
classify_glyph (GtkGlyphPaintable *self)
{
  self->uses_foreground = FALSE;
  self->uses_palette = FALSE;

  if (self->font)
    hb_font_paint_glyph (self->font, self->glyph,
                         get_classify_funcs (), self,
                         0, HB_COLOR (0, 0, 0, 255));
}

static void
update_font (GtkGlyphPaintable *self)
{
  unsigned int upem;
  int scale;

  g_clear_pointer (&self->font, hb_font_destroy);

  if (!self->face)
    return;

  self->font = hb_font_create (self->face);

  upem = hb_face_get_upem (self->face);
  scale = (int) scalbnf ((double)upem, SUBPIXEL_BITS);

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

  if (!self->font)
    return;

  self->glyph = 1;

  if (hb_font_get_glyph_from_name (self->font, "icon0", -1, &glyph) ||
      hb_font_get_glyph_from_name (self->font, "A", -1, &glyph))
    self->glyph = glyph;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_GLYPH]);
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
  classify_glyph (self);

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

  classify_glyph (self);

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

static void
update_custom_palette (GtkGlyphPaintable *self)
{
  unsigned int len;
  hb_color_t *colors;
  char **entries;

  if (self->custom_colors == NULL)
    {
      g_clear_pointer (&self->custom_palette, g_free);
      self->num_palette_entries = 0;
      return;
    }

  len = hb_ot_color_palette_get_colors (self->face, self->palette_index, 0, NULL, NULL);

  if (self->num_palette_entries != len)
    {
      self->custom_palette = g_realloc (self->custom_palette, sizeof (GdkRGBA) * len);
      self->num_palette_entries = len;
    }

  colors = g_newa (hb_color_t, len);
  hb_ot_color_palette_get_colors (self->face, self->palette_index, 0, &len, colors);
  for (unsigned int i = 0; i < len; i++)
    {
      self->custom_palette[i].red = hb_color_get_red (colors[i]) / 255.;
      self->custom_palette[i].green = hb_color_get_green (colors[i]) / 255.;
      self->custom_palette[i].blue = hb_color_get_blue (colors[i]) / 255.;
      self->custom_palette[i].alpha = hb_color_get_alpha (colors[i]) / 255.;
    }

  entries = g_strsplit (self->custom_colors, ",", -1);
  for (int i = 0; entries[i] && i < len; i++)
    {
      unsigned int r, g, b, a;

      if (sscanf (entries[i], "%2x%2x%2x%2x", &r, &g, &b, &a) == 4)
        {
          self->custom_palette[i].red = r / 255.;
          self->custom_palette[i].green = g / 255.;
          self->custom_palette[i].blue = b / 255.;
          self->custom_palette[i].alpha = a / 255.;
        }
    }

  g_strfreev (entries);
}

void
gtk_glyph_paintable_set_palette_index (GtkGlyphPaintable *self,
                                       unsigned int    palette_index)
{
  g_return_if_fail (GTK_IS_GLYPH_PAINTABLE (self));

  self->palette_index = palette_index;

  update_custom_palette (self);

  if (self->uses_palette)
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

  update_custom_palette (self);

  if (self->uses_palette)
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

  if (self->uses_foreground)
    gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLOR]);
}

const GdkRGBA *
gtk_glyph_paintable_get_color (GtkGlyphPaintable *self)
{
  g_return_val_if_fail (GTK_IS_GLYPH_PAINTABLE (self), NULL);

  return &self->color;
}
