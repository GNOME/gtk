/* Pango2/Rotated Text
 *
 * This demo shows how to use Pango2Cairo to draw rotated and transformed
 * text. The right pane shows a rotated GtkLabel widget.
 *
 * In both cases, a custom Pango2Cairo shape renderer is installed to draw
 * a red heart using cairo drawing operations instead of the Unicode heart
 * character.
 */

#include <gtk/gtk.h>
#include <string.h>

#define HEART "♥"
const char text[] = "I ♥ GTK";

static gboolean
glyph_cb (Pango2UserFace  *face,
          hb_codepoint_t  unicode,
          hb_codepoint_t *glyph,
          gpointer        data)
{
  if (unicode == 0x2665)
    {
      *glyph = 0x2665;
      return TRUE;
    }

  return FALSE;
}

static gboolean
glyph_info_cb (Pango2UserFace      *face,
               int                 size,
               hb_codepoint_t      glyph,
               hb_glyph_extents_t *extents,
               hb_position_t      *h_advance,
               hb_position_t      *v_advance,
               gboolean           *is_color,
               gpointer            user_data)
{
  if (glyph == 0x2665)
    {
      extents->x_bearing = 0;
      extents->y_bearing = size;
      extents->width = size;
      extents->height = - size;

      *h_advance = size;
      *v_advance = size;
      *is_color = TRUE;

      return TRUE;
    }

  return FALSE;
}

static gboolean
font_info_cb (Pango2UserFace     *face,
              int                size,
              hb_font_extents_t *extents,
              gpointer           user_data)
{
  extents->ascender = size;
  extents->descender = 0;
  extents->line_gap = 0;

  return TRUE;
}

static gboolean
render_cb (Pango2UserFace  *face,
           int             size,
           hb_codepoint_t  glyph,
           gpointer        user_data,
           const char     *backend_id,
           gpointer        backend_data)
{
  cairo_t *cr;

  if (strcmp (backend_id, "cairo") != 0)
    {
      g_warning ("Unsupported Pango2Renderer backend %s", backend_id);
      return FALSE;
    }

  cr = backend_data;

  if (glyph == 0x2665)
    {
      cairo_set_source_rgb (cr, 1., 0., 0.);

      cairo_move_to (cr, .5, .0);
      cairo_line_to (cr, .9, -.4);
      cairo_curve_to (cr, 1.1, -.8, .5, -.9, .5, -.5);
      cairo_curve_to (cr, .5, -.9, -.1, -.8, .1, -.4);
      cairo_close_path (cr);
      cairo_fill (cr);

      return TRUE;
    }

  return FALSE;
}

static void
setup_fontmap (void)
{
  Pango2FontMap *fontmap = pango2_font_map_get_default ();
  Pango2FontDescription *desc;
  Pango2UserFace *face;

  desc = pango2_font_description_new ();
  pango2_font_description_set_family (desc, "Bullets");

  /* Create our fancy user font, "Bullets Black" */
  face = pango2_user_face_new (font_info_cb,
                              glyph_cb,
                              glyph_info_cb,
                              NULL,
                              render_cb,
                              NULL, NULL, "Black", desc);

  /* And add it to the default fontmap */
  pango2_font_map_add_face (fontmap, PANGO2_FONT_FACE (face));

  pango2_font_description_free (desc);
}

static void
rotated_text_draw (GtkDrawingArea *da,
                   cairo_t        *cr,
                   int             width,
                   int             height,
                   gpointer        data)
{
#define RADIUS 150
#define N_WORDS 5
#define FONT "Bullets 18"

  Pango2Context *context;
  Pango2Layout *layout;
  Pango2FontDescription *desc;
  cairo_pattern_t *pattern;
  double device_radius;
  int i;

  /* Create a cairo context and set up a transformation matrix so that the user
   * space coordinates for the centered square where we draw are [-RADIUS, RADIUS],
   * [-RADIUS, RADIUS].
   * We first center, then change the scale. */
  device_radius = MIN (width, height) / 2.;
  cairo_translate (cr,
                   device_radius + (width - 2 * device_radius) / 2,
                   device_radius + (height - 2 * device_radius) / 2);
  cairo_scale (cr, device_radius / RADIUS, device_radius / RADIUS);

  /* Create and a subtle gradient source and use it. */
  pattern = cairo_pattern_create_linear (-RADIUS, -RADIUS, RADIUS, RADIUS);
  cairo_pattern_add_color_stop_rgb (pattern, 0., .5, .0, .0);
  cairo_pattern_add_color_stop_rgb (pattern, 1., .0, .0, .5);
  cairo_set_source (cr, pattern);

  /* Create a Pango2Context and set up our shape renderer */
  context = gtk_widget_create_pango_context (GTK_WIDGET (da));

  /* Create a Pango2Layout, set the text, font, and attributes */
  layout = pango2_layout_new (context);
  pango2_layout_set_text (layout, text, -1);
  desc = pango2_font_description_from_string (FONT);
  pango2_layout_set_font_description (layout, desc);

  /* Draw the layout N_WORDS times in a circle */
  for (i = 0; i < N_WORDS; i++)
    {
      Pango2Rectangle ext;

      /* Inform Pango2 to re-layout the text with the new transformation matrix */
      pango2_cairo_update_layout (cr, layout);

      pango2_lines_get_extents (pango2_layout_get_lines (layout), NULL, &ext);
      pango2_extents_to_pixels (&ext, NULL);
      cairo_move_to (cr, - ext.width / 2, - RADIUS * .9);
      pango2_cairo_show_layout (cr, layout);

      /* Rotate for the next turn */
      cairo_rotate (cr, G_PI*2 / N_WORDS);
    }

  /* free the objects we created */
  pango2_font_description_free (desc);
  g_object_unref (layout);
  g_object_unref (context);
  cairo_pattern_destroy (pattern);
}

GtkWidget *
do_rotated_text (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *box;
      GtkWidget *drawing_area;
      GtkWidget *label;
      Pango2Attribute *attr;
      Pango2AttrList *attrs;

      setup_fontmap ();

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Rotated Text");
      gtk_window_set_default_size (GTK_WINDOW (window), 4 * RADIUS, 2 * RADIUS);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_set_homogeneous (GTK_BOX (box), TRUE);
      gtk_window_set_child (GTK_WINDOW (window), box);

      /* Add a drawing area */
      drawing_area = gtk_drawing_area_new ();
      gtk_box_append (GTK_BOX (box), drawing_area);
      gtk_widget_add_css_class (drawing_area, "view");

      gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (drawing_area),
                                      rotated_text_draw,
                                      NULL, NULL);

      /* And a label */
      label = gtk_label_new (text);
      attrs = pango2_attr_list_new ();
      attr = pango2_attr_font_desc_new (pango2_font_description_from_string (FONT));
      pango2_attr_list_insert (attrs, attr);
      gtk_label_set_attributes (GTK_LABEL (label), attrs);
      pango2_attr_list_unref (attrs);
      gtk_box_append (GTK_BOX (box), label);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
