/* Paintable/Glyph
 *
 * This demo shows how to wrap a font in a GdkPaintable to display
 * a single glyph that can be scaled by resizing the window.
 *
 * The demo also has controls for font variations and colors.
 */

#include <gtk/gtk.h>
#include <hb-ot.h>

#include "fontvariations.h"
#include "fontcolors.h"
#include "fontpicker.h"
#include "glyphpicker.h"
#include "colorpicker.h"
#include "glyphmodel.h"

static GtkWidget *window;
static GtkWidget *color_picker;
static GtkWidget *font_variations;
static GtkWidget *font_colors;
static GtkWidget *toggle;

static void
reset (GSimpleAction *action,
       GVariant      *parameter,
       gpointer       data)
{
  g_action_activate (font_variations_get_reset_action (FONT_VARIATIONS (font_variations)), NULL);
  g_action_activate (font_colors_get_reset_action (FONT_COLORS (font_colors)), NULL);
  g_action_activate (color_picker_get_reset_action (COLOR_PICKER (color_picker)), NULL);
}

static void
update_reset (GSimpleAction *action,
              GParamSpec    *pspec,
              GSimpleAction *reset_action)
{
  gboolean enabled;

  enabled = g_action_get_enabled (font_variations_get_reset_action (FONT_VARIATIONS (font_variations))) ||
            g_action_get_enabled (font_colors_get_reset_action (FONT_COLORS (font_colors))) ||
            g_action_get_enabled (color_picker_get_reset_action (COLOR_PICKER (color_picker)));

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
  g_signal_connect (color_picker_get_reset_action (COLOR_PICKER (color_picker)),
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
color_changed (GtkWidget *picker,
               GParamSpec *pspec,
               gpointer data)
{
  const GdkRGBA *fg, *bg;
  char *css;
  GtkCssProvider *provider;

  provider = GTK_CSS_PROVIDER (g_object_get_data (G_OBJECT (picker), "bg-provider"));
  if (!provider)
    {
      provider = gtk_css_provider_new ();
      g_object_set_data_full (G_OBJECT (picker), "bg-provider", provider, clear_provider);
      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (provider),
                                                  800);
    }

  g_object_get (picker, "foreground", &fg, "background", &bg, NULL);

  css = g_strdup_printf (".picture-parent-box { "
                         "  color: rgba(%f,%f,%f,%f); "
                         "  background-color: rgba(%f,%f,%f,%f); "
                         "} ",
                         255 * fg->red, 255 * fg->green, 255 * fg->blue, 255 * fg->alpha,
                         255 * bg->red, 255 * bg->green, 255 * bg->blue, 255 * bg->alpha);

  gtk_css_provider_load_from_data (provider, css, -1);

  g_free (css);

  if (data)
    gtk_widget_queue_draw (GTK_WIDGET (data));
}

static void
setup_grid_item (GtkSignalListItemFactory *factory,
                 GObject                  *listitem)
{
  gtk_list_item_set_child (GTK_LIST_ITEM (listitem), gtk_image_new ());
}

static void
bind_grid_item (GtkSignalListItemFactory *factory,
                GObject                  *listitem)
{
  GtkWidget *image;
  GdkPaintable *paintable;

  image = gtk_list_item_get_child (GTK_LIST_ITEM (listitem));
  paintable = gtk_list_item_get_item (GTK_LIST_ITEM (listitem));
  gtk_image_set_from_paintable (GTK_IMAGE (image), paintable);
}

static void
grid_toggled (GtkToggleButton *grid_toggle,
              GParamSpec *pspec,
              GtkStack *stack)
{
  if (gtk_toggle_button_get_active (grid_toggle))
    {
      gtk_stack_set_visible_child_name (stack, "grid");
      gtk_widget_set_visible (toggle, FALSE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), FALSE);
    }
  else
    {
      gtk_stack_set_visible_child_name (stack, "glyph");
      gtk_widget_set_visible (toggle, TRUE);
    }
}

GtkWidget *
do_paintable_glyph (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkBuilderScope *scope;
      GtkBuilder *builder;
      GtkCssProvider *provider;
      GtkWidget *font_picker;

      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_resource (provider, "/paintable_glyph/paintable_glyph.css");
      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (provider),
                                                  800);
      g_object_unref (provider);

      g_type_ensure (FONT_VARIATIONS_TYPE);
      g_type_ensure (FONT_COLORS_TYPE);
      g_type_ensure (FONT_PICKER_TYPE);
      g_type_ensure (GLYPH_PICKER_TYPE);
      g_type_ensure (COLOR_PICKER_TYPE);
      g_type_ensure (GLYPH_MODEL_TYPE);

      scope = gtk_builder_cscope_new ();
      gtk_builder_cscope_add_callback (scope, color_changed);
      gtk_builder_cscope_add_callback (scope, setup_grid_item);
      gtk_builder_cscope_add_callback (scope, bind_grid_item);
      gtk_builder_cscope_add_callback (scope, grid_toggled);

      builder = gtk_builder_new ();
      gtk_builder_set_scope (builder, scope);
      gtk_builder_add_from_resource (builder, "/paintable_glyph/paintable_glyph.ui", NULL);

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
      gtk_window_set_display (GTK_WINDOW (window), gtk_widget_get_display (do_widget));

      font_picker = GTK_WIDGET (gtk_builder_get_object (builder, "font_picker"));
      color_picker = GTK_WIDGET (gtk_builder_get_object (builder, "color_picker"));
      font_variations = GTK_WIDGET (gtk_builder_get_object (builder, "font_variations"));
      font_colors = GTK_WIDGET (gtk_builder_get_object (builder, "font_colors"));
      toggle = GTK_WIDGET (gtk_builder_get_object (builder, "toggle"));

      create_reset_action ();

      font_picker_set_from_file (FONT_PICKER (font_picker), "/usr/share/fonts/abattis-cantarell-vf-fonts/Cantarell-VF.otf");

      color_changed (color_picker, NULL, NULL);

      g_object_unref (builder);
      g_object_unref (scope);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
