/* Paintable/Glyph
 *
 * This demo shows how to wrap a font in a GdkPaintable to display
 * a single glyph that can be scaled by resizing the window.
 *
 * The demo also has controls for font variations and colors.
 */

#include <gtk/gtk.h>
#include <hb-ot.h>

#include "glyphpaintable.h"
#include "fontvariations.h"
#include "fontcolors.h"
#include "glyphpicker.h"
#include "colorpicker.h"


static GtkWidget *window;
static GtkWidget *glyph_picker;
static GtkWidget *font_name;
static GtkWidget *font_variations;
static GtkWidget *font_colors;

static void
update_font_name (GtkWidget *label,
                  hb_face_t *face)
{
  unsigned int len;
  char *name;

  len = hb_ot_name_get_utf8 (face, HB_OT_NAME_ID_FONT_FAMILY, HB_LANGUAGE_INVALID, NULL, NULL);
  len++;
  name = alloca (len);

  hb_ot_name_get_utf8 (face, HB_OT_NAME_ID_FONT_FAMILY, HB_LANGUAGE_INVALID, &len, name);

  gtk_label_set_label (GTK_LABEL (label), name);
}

static void
set_font_from_path (GdkPaintable *paintable,
                    const char   *path)
{
  hb_blob_t *blob;
  hb_face_t *face;

  blob = hb_blob_create_from_file (path);
  face = hb_face_create (blob, 0);
  hb_blob_destroy (blob);

  glyph_paintable_set_face (GLYPH_PAINTABLE (paintable), face);

  update_font_name (font_name, face);

  hb_face_destroy (face);
}

static void
open_response_cb (GObject *source,
                  GAsyncResult *result,
                  void *data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  GdkPaintable *paintable = data;
  GFile *file;

  file = gtk_file_dialog_open_finish (dialog, result, NULL);
  if (file)
    {
      set_font_from_path (paintable, g_file_peek_path (file));
      g_object_unref (file);
    }
}

static void
show_file_open (GtkWidget    *button,
                GdkPaintable *paintable)
{
  GtkFileFilter *filter;
  GtkFileDialog *dialog;
  GListStore *filters;
  GFile *folder;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, "Open Font");

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "Font Files");
  gtk_file_filter_add_suffix (filter, "ttf");
  gtk_file_filter_add_suffix (filter, "otf");
  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, filter);
  g_object_unref (filter);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
  g_object_unref (filters);
  folder = g_file_new_for_path ("/usr/share/fonts");
  gtk_file_dialog_set_initial_folder (dialog, folder);
  g_object_unref (folder);

  gtk_file_dialog_open (dialog,
                        GTK_WINDOW (gtk_widget_get_root (button)),
                        NULL,
                        open_response_cb, paintable);
}

static void
reset (GSimpleAction *action,
       GVariant      *parameter,
       gpointer       data)
{
  g_action_activate (font_variations_get_reset_action (FONT_VARIATIONS (font_variations)), NULL);
  g_action_activate (font_colors_get_reset_action (FONT_COLORS (font_colors)), NULL);
}

static void
update_reset (GSimpleAction *action,
              GParamSpec    *pspec,
              GSimpleAction *reset_action)
{
  gboolean enabled;

  enabled = g_action_get_enabled (font_variations_get_reset_action (FONT_VARIATIONS (font_variations))) ||
            g_action_get_enabled (font_colors_get_reset_action (FONT_COLORS (font_colors)));

  g_simple_action_set_enabled (G_SIMPLE_ACTION (reset_action), enabled);
}

static void
create_reset_action (void)
{
  GSimpleAction *reset_action;
  GSimpleActionGroup *group;

  reset_action = g_simple_action_new ("reset", NULL);
  g_signal_connect (reset_action, "activate", G_CALLBACK (reset), NULL);
  g_signal_connect (font_variations_get_reset_action (FONT_VARIATIONS (font_variations)),
                    "notify::enabled", G_CALLBACK (update_reset), reset_action);
  g_signal_connect (font_colors_get_reset_action (FONT_COLORS (font_colors)),
                    "notify::enabled", G_CALLBACK (update_reset), reset_action);

  update_reset (NULL, NULL, reset_action);

  group = g_simple_action_group_new ();
  g_simple_action_group_insert (group, G_ACTION (reset_action));
  gtk_widget_insert_action_group (window, "win", G_ACTION_GROUP (group));
  g_object_unref (group);

  g_object_unref (reset_action);
}

static void
clear_provider (gpointer data)
{
  GtkStyleProvider *provider = data;

  gtk_style_context_remove_provider_for_display (gdk_display_get_default (), provider);
}

static void
background_changed (GtkWidget *color_picker,
                    GParamSpec *pspec,
                    GtkWidget *box)
{
  const GdkRGBA *bg;
  char *css;
  GtkCssProvider *provider;

  provider = GTK_CSS_PROVIDER (g_object_get_data (G_OBJECT (box), "bg-provider"));
  if (!provider)
    {
      provider = gtk_css_provider_new ();
      g_object_set_data_full (G_OBJECT (box), "bg-provider", provider, clear_provider);
      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (provider),
                                                  800);
    }

  g_object_get (color_picker, "background", &bg, NULL);

  css = g_strdup_printf (".picture-parent-box { background-color: rgba(%f,%f,%f,%f); }",
                         255 * bg->red, 255 * bg->green, 255 * bg->blue, 255 * bg->alpha);

  gtk_css_provider_load_from_data (provider, css, -1);

  g_free (css);

  gtk_widget_queue_draw (box);
}

GtkWidget *
do_paintable_glyph (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkBuilderScope *scope;
      GtkBuilder *builder;
      GdkPaintable *paintable;
      GtkCssProvider *provider;

      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_resource (provider, "/paintable_glyph/paintable_glyph.css");
      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (provider),
                                                  800);
      g_object_unref (provider);

      g_type_ensure (GLYPH_TYPE_PAINTABLE);
      g_type_ensure (FONT_VARIATIONS_TYPE);
      g_type_ensure (FONT_COLORS_TYPE);
      g_type_ensure (GLYPH_PICKER_TYPE);
      g_type_ensure (COLOR_PICKER_TYPE);

      scope = gtk_builder_cscope_new ();
      gtk_builder_cscope_add_callback (scope, show_file_open);
      gtk_builder_cscope_add_callback (scope, background_changed);

      builder = gtk_builder_new ();
      gtk_builder_set_scope (builder, scope);
      gtk_builder_add_from_resource (builder, "/paintable_glyph/paintable_glyph.ui", NULL);

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
      gtk_window_set_display (GTK_WINDOW (window), gtk_widget_get_display (do_widget));

      glyph_picker = GTK_WIDGET (gtk_builder_get_object (builder, "glyph_picker"));
      g_assert (glyph_picker);
      font_name = GTK_WIDGET (gtk_builder_get_object (builder, "font_name"));
      g_assert (font_name);
      font_variations = GTK_WIDGET (gtk_builder_get_object (builder, "font_variations"));
      g_assert (font_variations);
      font_colors = GTK_WIDGET (gtk_builder_get_object (builder, "font_colors"));
      g_assert (font_colors);
      paintable = GDK_PAINTABLE (gtk_builder_get_object (builder, "paintable"));
      g_assert (paintable);

      create_reset_action ();

      set_font_from_path (paintable, "/usr/share/fonts/abattis-cantarell-vf-fonts/Cantarell-VF.otf");

      g_object_unref (builder);
      g_object_unref (scope);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
