#include "glyphview.h"

#include <gtk/gtk.h>
#include <hb-ot.h>

struct _GlyphView
{
  GtkWidget parent;

  Pango2Font *font;
  GQuark palette;
  int palette_index;
  hb_codepoint_t glyph;
  int n_layers;
  int layer;
};

struct _GlyphViewClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE(GlyphView, glyph_view, GTK_TYPE_WIDGET);

static void
click_cb (GtkGestureClick *gesture,
          int              n_press,
          double           x,
          double           y,
          GlyphView       *self)
{
  if (self->n_layers == 0)
    return;

  self->layer++;

  if (self->layer == self->n_layers)
    self->layer = -1;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
glyph_view_init (GlyphView *self)
{
  GtkGesture *click;

  self->n_layers = 0;
  self->layer = -1;

  click = gtk_gesture_click_new ();
  g_signal_connect (click, "pressed", G_CALLBACK (click_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (click));
}

static void
glyph_view_dispose (GObject *object)
{
  GlyphView *self = GLYPH_VIEW (object);

  g_clear_object (&self->font);

  G_OBJECT_CLASS (glyph_view_parent_class)->dispose (object);
}

static void
glyph_view_snapshot (GtkWidget   *widget,
                     GtkSnapshot *snapshot)
{
  GlyphView *self = GLYPH_VIEW (widget);
  Pango2GlyphString *glyphs;
  Pango2Rectangle ink, logical;
  cairo_t *cr;
  int width, height;
  char name[80];

  pango2_font_get_glyph_extents (self->font, self->glyph, &ink, &logical);

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  cr = gtk_snapshot_append_cairo (snapshot, &GRAPHENE_RECT_INIT (0, 0, width, height));

  glyphs = pango2_glyph_string_new ();
  pango2_glyph_string_set_size (glyphs, 1);

  glyphs->glyphs[0].geometry.width = ink.width;
  glyphs->glyphs[0].geometry.x_offset = ink.x;
  glyphs->glyphs[0].geometry.y_offset = -ink.y;

  if (self->layer == -1)
    {
      glyphs->glyphs[0].glyph = self->glyph;
      cairo_set_source_rgb (cr, 0, 0, 0);
    }
  else
    {
      hb_face_t *face = hb_font_get_face (pango2_font_get_hb_font (self->font));
      hb_ot_color_layer_t *layers;
      unsigned int count;

      layers = g_newa (hb_ot_color_layer_t, self->n_layers);
      count = self->n_layers;
      hb_ot_color_glyph_get_layers (face,
                                    self->glyph,
                                    0,
                                    &count,
                                    layers);
      glyphs->glyphs[0].glyph = layers[self->layer].glyph;
      if (layers[self->layer].color_index == 0xffff)
        {
          cairo_set_source_rgb (cr, 0, 0, 0);
        }
      else
        {
          hb_color_t *colors;
          unsigned int n_colors;
          hb_color_t color;

          n_colors = hb_ot_color_palette_get_colors (face,
                                                     self->palette_index,
                                                     0,
                                                     NULL,
                                                     NULL);
          colors = g_newa (hb_color_t, n_colors);
          hb_ot_color_palette_get_colors (face,
                                          self->palette_index,
                                          0,
                                          &n_colors,
                                          colors);
          color = colors[layers[self->layer].color_index];
          cairo_set_source_rgba (cr, hb_color_get_red (color)/255.,
                                     hb_color_get_green (color)/255.,
                                     hb_color_get_blue (color)/255.,
                                     hb_color_get_alpha (color)/255.);
        }
    }

  cairo_move_to (cr, (width - logical.width/1024.)/2, (height - logical.height/1024.)/2);
  pango2_cairo_show_color_glyph_string (cr, self->font, self->palette, glyphs);

    {
      Pango2Layout *layout;
      Pango2FontDescription *desc;
      char gid[20];
      hb_font_t *hb_font;
      hb_face_t *hb_face;

      g_snprintf (gid, sizeof (gid), "%d", self->glyph);
      layout = gtk_widget_create_pango_layout (widget, gid);
      desc = pango2_font_description_from_string ("Cantarell 8");
      pango2_layout_set_font_description (layout, desc);
      pango2_font_description_free (desc);

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (5, 5));
      gtk_snapshot_append_layout (snapshot, layout, &(GdkRGBA){ 0.,0.,0.,0.7});
      gtk_snapshot_restore (snapshot);

      hb_font = pango2_font_get_hb_font (self->font);
      hb_face = hb_font_get_face (hb_font);

      if (hb_ot_layout_has_glyph_classes (hb_face))
        {
          hb_ot_layout_glyph_class_t class;
          const char *class_names[] = {
            NULL, "Base", "Ligature", "Mark", "Component"
          };
          int w, h;

          class = hb_ot_layout_get_glyph_class (hb_face, self->glyph);

          if (class != HB_OT_LAYOUT_GLYPH_CLASS_UNCLASSIFIED)
            {
              pango2_layout_set_text (layout, class_names[class], -1);
              pango2_lines_get_size (pango2_layout_get_lines (layout), &w, &h);

              gtk_snapshot_save (snapshot);
              gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (width - w/1024. - 5, 5));
              gtk_snapshot_append_layout (snapshot, layout, &(GdkRGBA){ 0.,0.,0.,.7});
              gtk_snapshot_restore (snapshot);
            }
        }

      if (hb_font_get_glyph_name (hb_font, self->glyph, name, sizeof (name)))
        {
          int w, h;

          pango2_layout_set_text (layout, name, -1);
          pango2_lines_get_size (pango2_layout_get_lines (layout), &w, &h);

          gtk_snapshot_save (snapshot);
          gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (5, height - h/1024. - 5));
          gtk_snapshot_append_layout (snapshot, layout, &(GdkRGBA){ 0.,0.,0.,.7});
          gtk_snapshot_restore (snapshot);
        }

      if (self->n_layers > 0)
        {
          char buf[128];
          int w, h;

          if (self->layer == -1)
            g_snprintf (buf, sizeof (buf), "%d Layers", self->n_layers);
          else
            g_snprintf (buf, sizeof (buf), "Layer %d", self->layer);

          pango2_layout_set_text (layout, buf, -1);
          pango2_lines_get_size (pango2_layout_get_lines (layout), &w, &h);

          gtk_snapshot_save (snapshot);
          gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (width - w/1024. - 5, height - h/1024. - 5));
          gtk_snapshot_append_layout (snapshot, layout, &(GdkRGBA){ 0.,0.,0.,.7});
          gtk_snapshot_restore (snapshot);
        }

      g_object_unref (layout);
    }
}

static void
glyph_view_measure (GtkWidget      *widget,
                    GtkOrientation  orientation,
                    int             for_size,
                    int            *minimum,
                    int            *natural,
                    int            *minimum_baseline,
                    int            *natural_baseline)
{
  GlyphView *self = GLYPH_VIEW (widget);
  Pango2Rectangle ink, logical;

  pango2_font_get_glyph_extents (self->font, self->glyph, &ink, &logical);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    *minimum = *natural = 2 * logical.width / PANGO2_SCALE;
  else
    *minimum = *natural = 2 * logical.height / PANGO2_SCALE;
}

static void
glyph_view_class_init (GlyphViewClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = glyph_view_dispose;
  widget_class->snapshot = glyph_view_snapshot;
  widget_class->measure = glyph_view_measure;

  gtk_widget_class_set_css_name (widget_class, "glyphview");
}

GlyphView *
glyph_view_new (void)
{
  return g_object_new (GLYPH_VIEW_TYPE, NULL);
}

void
glyph_view_set_font (GlyphView *self,
                     Pango2Font *font)
{
  if (g_set_object (&self->font, font))
    gtk_widget_queue_resize (GTK_WIDGET (self));
}

static unsigned int
find_palette_index_by_flag (hb_face_t                   *hbface,
                            hb_ot_color_palette_flags_t  flag)
{
  unsigned int n_palettes;

  n_palettes = hb_ot_color_palette_get_count (hbface);
  for (unsigned int i = 0; i < n_palettes; i++)
    {
      if (hb_ot_color_palette_get_flags (hbface, i) & flag)
        return i;
    }

  return 0;
}

void
glyph_view_set_palette (GlyphView *self,
                        GQuark     palette)
{
  if (self->palette == palette)
    return;

  self->palette = palette;

  if (palette == g_quark_from_string (PANGO2_COLOR_PALETTE_DEFAULT))
    self->palette_index = 0;
  else if (palette == g_quark_from_string (PANGO2_COLOR_PALETTE_LIGHT))
    self->palette_index = find_palette_index_by_flag (hb_font_get_face (pango2_font_get_hb_font (self->font)), HB_OT_COLOR_PALETTE_FLAG_USABLE_WITH_LIGHT_BACKGROUND);
  else if (palette == g_quark_from_string (PANGO2_COLOR_PALETTE_DARK))
    self->palette_index = find_palette_index_by_flag (hb_font_get_face (pango2_font_get_hb_font (self->font)), HB_OT_COLOR_PALETTE_FLAG_USABLE_WITH_DARK_BACKGROUND);
  else
    {
      const char *str = g_quark_to_string (palette);
      char *endp;
      if (g_str_has_prefix (str, "palette"))
        self->palette_index = g_ascii_strtoll (str + strlen ("palette"), &endp, 10);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
}

void
glyph_view_set_glyph (GlyphView      *self,
                      hb_codepoint_t  glyph)
{
  if (self->glyph == glyph)
    return;

  self->glyph = glyph;
  self->n_layers = hb_ot_color_glyph_get_layers (hb_font_get_face (pango2_font_get_hb_font (self->font)), glyph, 0, NULL, NULL);
  self->layer = -1;
  gtk_widget_queue_resize (GTK_WIDGET (self));
}
