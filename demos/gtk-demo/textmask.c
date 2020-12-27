/* Pango/Text Mask
 *
 * This demo shows how to use PangoCairo to draw text with more than
 * just a single color.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#define TEXT_TYPE_MASK_DEMO (text_mask_demo_get_type ())
G_DECLARE_FINAL_TYPE (TextMaskDemo, text_mask_demo, TEXT, MASK_DEMO, GtkWidget)

struct _TextMaskDemo
{
  GtkWidget parent_instance;

  char *text;
  PangoFontDescription *desc;
  GskPath *path;
};

struct _TextMaskDemoClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (TextMaskDemo, text_mask_demo, GTK_TYPE_WIDGET)

static void
update_path (TextMaskDemo *demo)
{
  PangoLayout *layout;
  GskPathBuilder *builder;

  g_clear_pointer (&demo->path, gsk_path_unref);

  layout = gtk_widget_create_pango_layout (GTK_WIDGET (demo), demo->text);
  pango_layout_set_font_description (layout, demo->desc);

  builder = gsk_path_builder_new ();
  gsk_path_builder_add_layout (builder, layout);
  demo->path = gsk_path_builder_free_to_path (builder);

  gtk_widget_queue_draw (GTK_WIDGET (demo));
}

static void
text_mask_demo_init (TextMaskDemo *demo)
{
  demo->text = g_strdup ("No text. No fun");
  demo->desc = pango_font_description_from_string ("Cantarell 20");
  update_path (demo);
}

static void
text_mask_demo_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  TextMaskDemo *demo = TEXT_MASK_DEMO (widget);
  float width, height;
  GskColorStop stops[4];
  GskStroke *stroke;

  gdk_rgba_parse (&stops[0].color, "red");
  gdk_rgba_parse (&stops[1].color, "green");
  gdk_rgba_parse (&stops[2].color, "yellow");
  gdk_rgba_parse (&stops[3].color, "blue");

  width = gtk_widget_get_width (widget);
  height = gtk_widget_get_height (widget);

  stops[0].offset = 0;
  stops[1].offset = 0.25;
  stops[2].offset = 0.50;
  stops[3].offset = 0.75;

  gtk_snapshot_push_fill (snapshot, demo->path, GSK_FILL_RULE_WINDING);

  gtk_snapshot_append_linear_gradient (snapshot,
                                       &GRAPHENE_RECT_INIT (0, 0, width, height),
                                       &GRAPHENE_POINT_INIT (0, 0),
                                       &GRAPHENE_POINT_INIT (width, height),
                                       stops, 4);
  gtk_snapshot_pop (snapshot);

  stroke = gsk_stroke_new (1.0);
  gtk_snapshot_push_stroke (snapshot, demo->path, stroke);
  gsk_stroke_free (stroke);

  gtk_snapshot_append_color (snapshot,
                             &(GdkRGBA){ 0, 0, 0, 1 },
                             &GRAPHENE_RECT_INIT (0, 0, width, height));

  gtk_snapshot_pop (snapshot);
}

void
text_mask_demo_measure (GtkWidget      *widget,
                        GtkOrientation  orientation,
                        int             for_size,
                        int            *minimum_size,
                        int            *natural_size,
                        int            *minimum_baseline,
                        int            *natural_baseline)
{
  TextMaskDemo *demo = TEXT_MASK_DEMO (widget);
  graphene_rect_t rect;

  gsk_path_get_bounds (demo->path, &rect);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    *minimum_size = *natural_size = rect.size.width;
  else
    *minimum_size = *natural_size = rect.size.height;
}

static void
text_mask_demo_finalize (GObject *object)
{
  TextMaskDemo *demo = TEXT_MASK_DEMO (object);

  g_free (demo->text);
  pango_font_description_free (demo->desc);
  gsk_path_unref (demo->path);

  G_OBJECT_CLASS (text_mask_demo_parent_class)->finalize (object);
}

static void
text_mask_demo_class_init (TextMaskDemoClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->finalize = text_mask_demo_finalize;

  widget_class->snapshot = text_mask_demo_snapshot;
  widget_class->measure = text_mask_demo_measure;
}

static GtkWidget *
text_mask_demo_new (void)
{
  return g_object_new (text_mask_demo_get_type (), NULL);
}

static void
text_mask_demo_set_text (TextMaskDemo *demo,
                         const char   *text)
{
  g_free (demo->text);
  demo->text = g_strdup (text);

  update_path (demo);
}

static void
text_mask_demo_set_font (TextMaskDemo         *demo,
                         PangoFontDescription *desc)
{
  pango_font_description_free (demo->desc);
  demo->desc = pango_font_description_copy (desc);

  update_path (demo);
}

GtkWidget *
do_textmask (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  static GtkWidget *demo;

  if (!window)
    {
      PangoFontDescription *desc;

      window = gtk_window_new ();
      gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
      gtk_widget_set_size_request (window, 400, 240);
      gtk_window_set_title (GTK_WINDOW (window), "Text Mask");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      demo = text_mask_demo_new ();
      text_mask_demo_set_text (TEXT_MASK_DEMO (demo), "Pango power!\nPango power!\nPango power!");
      desc = pango_font_description_from_string ("Sans Bold 34");
      text_mask_demo_set_font (TEXT_MASK_DEMO (demo), desc);
      pango_font_description_free (desc);

      gtk_window_set_child (GTK_WINDOW (window), demo);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
