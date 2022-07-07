#include "glyphview.h"
#include <gtk/gtk.h>
#include <hb-ot.h>

struct _GlyphView
{
  GtkWidget parent;

  Pango2Font *font;
  hb_codepoint_t glyph;
};

struct _GlyphViewClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE(GlyphView, glyph_view, GTK_TYPE_WIDGET);

static void
glyph_view_init (GlyphView *self)
{
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

  glyphs = pango2_glyph_string_new ();
  pango2_glyph_string_set_size (glyphs, 1);
  glyphs->glyphs[0].glyph = self->glyph;
  glyphs->glyphs[0].geometry.width = ink.width;
  glyphs->glyphs[0].geometry.x_offset = ink.x;
  glyphs->glyphs[0].geometry.y_offset = -ink.y;

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  cr = gtk_snapshot_append_cairo (snapshot, &GRAPHENE_RECT_INIT (0, 0, width, height));

  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_move_to (cr, (width - logical.width/1024.)/2, (height - logical.height/1024.)/2);
  pango2_cairo_show_glyph_string (cr, self->font, glyphs);

  if (hb_font_get_glyph_name (pango2_font_get_hb_font (self->font), self->glyph, name, sizeof (name)))
    {
      Pango2Layout *layout;
      Pango2FontDescription *desc;
      int w, h;
      char gid[20];

      g_snprintf (gid, sizeof (gid), "%d", self->glyph);
      layout = gtk_widget_create_pango_layout (widget, gid);
      desc = pango2_font_description_from_string ("Cantarell 8");
      pango2_layout_set_font_description (layout, desc);
      pango2_font_description_free (desc);

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (5, 5));
      gtk_snapshot_append_layout (snapshot, layout, &(GdkRGBA){ 0.,0.,0.,0.7});
      gtk_snapshot_restore (snapshot);
      g_object_unref (layout);

      layout = gtk_widget_create_pango_layout (widget, name);
      desc = pango2_font_description_from_string ("Cantarell 8");
      pango2_layout_set_font_description (layout, desc);
      pango2_font_description_free (desc);

      pango2_lines_get_size (pango2_layout_get_lines (layout), &w, &h);

      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (5, height - h/1024. - 5));
      gtk_snapshot_append_layout (snapshot, layout, &(GdkRGBA){ 0.,0.,0.,1.});
      gtk_snapshot_restore (snapshot);
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

void
glyph_view_set_glyph (GlyphView      *self,
                      hb_codepoint_t  glyph)
{
  if (self->glyph == glyph)
    return;

  self->glyph = glyph;

  gtk_widget_queue_resize (GTK_WIDGET (self));
}
