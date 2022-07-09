#include "fontexplorerapp.h"
#include "fontexplorerwin.h"
#include "fontview.h"
#include "fontcontrols.h"
#include "samplechooser.h"
#include "fontcolors.h"
#include "fontfeatures.h"
#include "fontvariations.h"

#include <gtk/gtk.h>
#include <string.h>


struct _FontExplorerWindow
{
  GtkApplicationWindow parent;

  GtkFontButton *fontbutton;
  FontControls *controls;
  FontFeatures *features;
  FontVariations *variations;
  FontColors *colors;
  FontView *view;
};

struct _FontExplorerWindowClass
{
  GtkApplicationWindowClass parent_class;
};

G_DEFINE_TYPE(FontExplorerWindow, font_explorer_window, GTK_TYPE_APPLICATION_WINDOW);

static void
reset (GSimpleAction      *action,
       GVariant           *parameter,
       FontExplorerWindow *win)
{
  g_action_activate (font_controls_get_reset_action (win->controls), NULL);
  g_action_activate (font_features_get_reset_action (win->features), NULL);
  g_action_activate (font_variations_get_reset_action (win->variations), NULL);
  g_action_activate (font_colors_get_reset_action (win->colors), NULL);
}

static void
update_reset (GSimpleAction      *action,
              GParamSpec         *pspec,
              FontExplorerWindow *win)
{
  gboolean enabled;
  GAction *reset_action;

  enabled = g_action_get_enabled (font_controls_get_reset_action (win->controls)) ||
            g_action_get_enabled (font_features_get_reset_action (win->features)) ||
            g_action_get_enabled (font_variations_get_reset_action (win->variations)) ||
            g_action_get_enabled (font_colors_get_reset_action (win->colors));

  reset_action = g_action_map_lookup_action (G_ACTION_MAP (win), "reset");

  g_simple_action_set_enabled (G_SIMPLE_ACTION (reset_action), enabled);
}

static void
font_explorer_window_init (FontExplorerWindow *win)
{
  GSimpleAction *reset_action;

  gtk_widget_init_template (GTK_WIDGET (win));

  reset_action = g_simple_action_new ("reset", NULL);
  g_signal_connect (reset_action, "activate", G_CALLBACK (reset), win);
  g_signal_connect (font_controls_get_reset_action (win->controls),
                    "notify::enabled", G_CALLBACK (update_reset), win);
  g_signal_connect (font_variations_get_reset_action (win->variations),
                    "notify::enabled", G_CALLBACK (update_reset), win);
  g_signal_connect (font_colors_get_reset_action (win->colors),
                    "notify::enabled", G_CALLBACK (update_reset), win);
  g_signal_connect (font_features_get_reset_action (win->features),
                    "notify::enabled", G_CALLBACK (update_reset), win);

  g_action_map_add_action (G_ACTION_MAP (win), G_ACTION (reset_action));
  update_reset (NULL, NULL, win);
}

static void
font_explorer_window_dispose (GObject *object)
{
  gtk_widget_clear_template (GTK_WIDGET (object), FONT_EXPLORER_WINDOW_TYPE);

  G_OBJECT_CLASS (font_explorer_window_parent_class)->dispose (object);
}

static void
font_explorer_window_finalize (GObject *object)
{
//  FontExplorerWindow *win = FONT_EXPLORER_WINDOW (object);

  G_OBJECT_CLASS (font_explorer_window_parent_class)->finalize (object);
}

static void
font_explorer_window_class_init (FontExplorerWindowClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  g_type_ensure (FONT_VIEW_TYPE);
  g_type_ensure (FONT_CONTROLS_TYPE);
  g_type_ensure (SAMPLE_CHOOSER_TYPE);
  g_type_ensure (FONT_VARIATIONS_TYPE);
  g_type_ensure (FONT_COLORS_TYPE);
  g_type_ensure (FONT_FEATURES_TYPE);

  object_class->dispose = font_explorer_window_dispose;
  object_class->finalize = font_explorer_window_finalize;

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/fontexplorer/fontexplorerwin.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontExplorerWindow, fontbutton);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontExplorerWindow, controls);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontExplorerWindow, variations);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontExplorerWindow, colors);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontExplorerWindow, features);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), FontExplorerWindow, view);

}

FontExplorerWindow *
font_explorer_window_new (FontExplorerApp *app)
{
  return g_object_new (FONT_EXPLORER_WINDOW_TYPE, "application", app, NULL);
}

void
font_explorer_window_load (FontExplorerWindow *win,
                           GFile              *file)
{
  const char *path;
  Pango2FontMap *map;
  Pango2HbFace *face;
  Pango2FontDescription *desc;
  char *title;

  path = g_file_peek_path (file);

  face = pango2_hb_face_new_from_file (path, 0, -2, NULL, NULL);
  desc = pango2_font_face_describe (PANGO2_FONT_FACE (face));

  map = pango2_font_map_new ();
  pango2_font_map_add_face (map, PANGO2_FONT_FACE (face));
  pango2_font_map_set_fallback (map, pango2_font_map_get_default ());

  font_features_set_font_map (win->features, map);
  font_variations_set_font_map (win->variations, map);
  font_colors_set_font_map (win->colors, map);
  font_view_set_font_map (win->view, map);

  g_object_unref (map);

  gtk_font_chooser_set_font_desc (GTK_FONT_CHOOSER (win->fontbutton), desc);

  gtk_widget_hide (GTK_WIDGET (win->fontbutton));

  title = g_strdup_printf ("%s — %s",
                           pango2_font_description_get_family (desc),
                           path);
  gtk_window_set_title (GTK_WINDOW (win), title);
  g_free (title);

  pango2_font_description_free (desc);
}
