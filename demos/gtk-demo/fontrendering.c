/* Pango/Font rendering
 *
 * Demonstrates variations in font rendering.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;
static GtkWidget *font_button = NULL;
static GtkWidget *entry = NULL;
static GtkWidget *image = NULL;
static GtkWidget *hinting = NULL;
static GtkWidget *hint_metrics = NULL;
static GtkWidget *up_button = NULL;
static GtkWidget *down_button = NULL;

static PangoContext *context;

static int scale = 1;

static GdkPixbuf *pb;

static void
on_destroy (gpointer data)
{
  window = NULL;
}

static void
update_image (void)
{
  const char *text;
  PangoFontDescription *desc;
  PangoLayout *layout;
  PangoRectangle ink, logical;
  cairo_surface_t *surface;
  cairo_t *cr;
  GdkPixbuf *pixbuf;
  GdkPixbuf *pixbuf2;
  const char *hint;
  cairo_font_options_t *fopt;
  cairo_hint_style_t hintstyle;
  cairo_hint_metrics_t hintmetrics;

  if (!context)
    context = gtk_widget_create_pango_context (image);

  text = gtk_entry_get_text (GTK_ENTRY (entry));
  desc = gtk_font_chooser_get_font_desc (GTK_FONT_CHOOSER (font_button));  

  fopt = cairo_font_options_copy (pango_cairo_context_get_font_options (context));

  hint = gtk_combo_box_get_active_id (GTK_COMBO_BOX (hinting));
  if (strcmp (hint, "none") == 0)
    hintstyle = CAIRO_HINT_STYLE_NONE;
  else if (strcmp (hint, "slight") == 0)
    hintstyle = CAIRO_HINT_STYLE_SLIGHT;
  else if (strcmp (hint, "medium") == 0)
    hintstyle = CAIRO_HINT_STYLE_MEDIUM;
  else if (strcmp (hint, "full") == 0)
    hintstyle = CAIRO_HINT_STYLE_FULL;
  else
    hintstyle = CAIRO_HINT_STYLE_DEFAULT;
  cairo_font_options_set_hint_style (fopt, hintstyle);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (hint_metrics)))
    hintmetrics = CAIRO_HINT_METRICS_ON;
  else
    hintmetrics = CAIRO_HINT_METRICS_OFF;
  cairo_font_options_set_hint_metrics (fopt, hintmetrics);

  pango_cairo_context_set_font_options (context, fopt);
  cairo_font_options_destroy (fopt);

  layout = pango_layout_new (context);
  pango_layout_set_text (layout, text, -1);
  pango_layout_set_font_description (layout, desc);
  pango_layout_get_extents (layout, &ink, &logical);

  pango_extents_to_pixels (&logical, NULL);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, logical.width, logical.height);
  cr = cairo_create (surface);
  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_paint (cr);

  cairo_set_source_rgb (cr, 0, 0, 0);
  pango_cairo_show_layout (cr, layout);

  cairo_destroy (cr);

  pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, logical.width, logical.height);
  pixbuf2 = gdk_pixbuf_scale_simple (pixbuf, logical.width * scale, logical.height * scale, GDK_INTERP_NEAREST);

  g_set_object (&pb, pixbuf2);

  gtk_widget_set_size_request (image, gdk_pixbuf_get_width (pb), gdk_pixbuf_get_height (pb));

  g_object_unref (pixbuf);
  g_object_unref (pixbuf2);

  cairo_surface_destroy (surface);
  pango_font_description_free (desc);

  
}

static void
update_buttons (void)
{
  gtk_widget_set_sensitive (up_button, scale < 32);
  gtk_widget_set_sensitive (down_button, scale > 1);
}

static void
scale_up (void)
{
  scale += 1;
  update_buttons ();
  update_image ();
}

static void
scale_down (void)
{
  scale -= 1;
  update_buttons ();
  update_image ();
}

static gboolean
draw (GtkWidget *widget, cairo_t *cr, gpointer data)
{
  guint width, height;
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (widget);

  width = gtk_widget_get_allocated_width (widget);
  height = gtk_widget_get_allocated_height (widget);

  gtk_render_background (context, cr, 0, 0, width, height);

  if (pb)
    {
      gdk_cairo_set_source_pixbuf (cr, pb, 0, 0);
      cairo_paint (cr);
    } 

 return FALSE;
}

GtkWidget *
do_fontrendering (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkBuilder *builder;

      builder = gtk_builder_new_from_resource ("/fontrendering/fontrendering.ui");
      gtk_builder_connect_signals (builder, NULL);
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      gtk_window_set_screen (GTK_WINDOW (window),
                              gtk_widget_get_screen (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (on_destroy), NULL);
      g_object_set_data_full (G_OBJECT (window), "builder", builder, g_object_unref);
      font_button = GTK_WIDGET (gtk_builder_get_object (builder, "font_button"));
      up_button = GTK_WIDGET (gtk_builder_get_object (builder, "up_button"));
      down_button = GTK_WIDGET (gtk_builder_get_object (builder, "down_button"));
      entry = GTK_WIDGET (gtk_builder_get_object (builder, "entry"));
      image = GTK_WIDGET (gtk_builder_get_object (builder, "image"));
      hinting = GTK_WIDGET (gtk_builder_get_object (builder, "hinting"));
      hint_metrics = GTK_WIDGET (gtk_builder_get_object (builder, "hint_metrics"));

      g_signal_connect (up_button, "clicked", G_CALLBACK (scale_up), NULL);
      g_signal_connect (down_button, "clicked", G_CALLBACK (scale_down), NULL);
      g_signal_connect (entry, "notify::text", G_CALLBACK (update_image), NULL);
      g_signal_connect (font_button, "notify::font-desc", G_CALLBACK (update_image), NULL);
      g_signal_connect (hinting, "notify::active", G_CALLBACK (update_image), NULL);
      g_signal_connect (hint_metrics, "notify::active", G_CALLBACK (update_image), NULL);

      g_signal_connect (image, "draw", G_CALLBACK (draw), NULL);

      update_image ();
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    {
      gtk_widget_destroy (window);
    }

  return window;
}
