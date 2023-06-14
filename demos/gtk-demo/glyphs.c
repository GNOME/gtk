/* Path/Glyphs
 *
 * This demo shows boolean operation on paths with the example
 * of glyphs from a font.
 */

#include <gtk/gtk.h>

#include "glyph-demo.h"
#include "glyph-chooser.h"

static char *
newlineify (char *s)
{
  if (*s == '\0')
    return s;

  for (char *p = s + 1; *p; p++)
    {
      if (strchr ("XZzMmLlHhVvCcSsQqTtOoAaa", *p))
        p[-1] = '\n';
    }

  return s;
}

static gboolean
path_to_text (GBinding     *binding,
              const GValue *from,
              GValue       *to,
              gpointer      user_data)
{
  GskPath *path;
  char *text;

  path = g_value_get_boxed (from);
  if (path)
    text = newlineify (gsk_path_to_string (path));
  else
    text = g_strdup ("");

  g_value_take_string (to, text);

  return TRUE;
}

static void
apply_short_path (GlyphDemo *demo)
{
  g_object_set (demo, "outline-step", 10000, NULL);
}

static void
short_path_step (GlyphDemo *demo)
{
  guint step;

  g_object_get (demo, "outline-step", &step, NULL);
  g_object_set (demo, "outline-step", step + 1, NULL);
}

static void
reset_short_path (GlyphDemo *demo)
{
  g_object_set (demo, "outline-step", 0, NULL);
}

GtkWidget *
do_glyphs (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkBuilderScope *scope;
      GtkBuilder *builder;
      GtkCssProvider *style;
      GlyphDemo *demo;
      GtkTextBuffer *buffer;

      g_type_ensure (glyph_demo_get_type ());
      g_type_ensure (glyph_chooser_get_type ());

      style = gtk_css_provider_new ();
      gtk_css_provider_load_from_resource (style, "/glyphs/glyphs.css");
      gtk_style_context_add_provider_for_display (gdk_display_get_default (), GTK_STYLE_PROVIDER (style), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      g_object_unref (style);

      scope = gtk_builder_cscope_new ();
      gtk_builder_cscope_add_callback (scope, apply_short_path);
      gtk_builder_cscope_add_callback (scope, short_path_step);
      gtk_builder_cscope_add_callback (scope, reset_short_path);

      builder = gtk_builder_new ();
      gtk_builder_set_scope (builder, scope);
      gtk_builder_add_from_resource (builder, "/glyphs/glyphs.ui", NULL);

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));

      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);

      demo = GLYPH_DEMO (gtk_builder_get_object (builder, "demo"));
      buffer = GTK_TEXT_BUFFER (gtk_builder_get_object (builder, "buffer"));
      g_object_bind_property_full (demo, "path",
                                   buffer, "text",
                                   G_BINDING_SYNC_CREATE,
                                   path_to_text, NULL, NULL, NULL);

      g_object_unref (builder);
      g_object_unref (scope);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
